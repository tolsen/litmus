/* 
   litmus: WebDAV server test suite
   Copyright (C) 2001-2002, Joe Orton <joe@manyfish.co.uk>
                                                                     
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <fcntl.h>

#include <ne_request.h>
#include <ne_string.h>

#include "common.h"

static char *src, *dest, *coll, *ncoll;

static int copy_ok = 0;

static int copy_init(void)
{
    src = ne_concat(i_path, "copysrc", NULL);
    dest = ne_concat(i_path, "copydest", NULL);
    coll = ne_concat(i_path, "copycoll/", NULL);
    ncoll = ne_concat(i_path, "copycoll", NULL);

    CALL(upload_foo("copysrc"));
    ONNREQ("could not create collection", ne_mkcol(i_session, coll));

    copy_ok = 1;
    return OK;
}

static int copy_simple(void)
{
    PRECOND(copy_ok);

    /* Now copy it once */
    ONNREQ("simple resource COPY", 
	   ne_copy(i_session, 0, NE_DEPTH_INFINITE, src, dest));

    if (STATUS(201)) {
	t_warning("COPY to new resource didn't give 201");
    }

    return OK;
}

static int copy_overwrite(void)
{
    PRECOND(copy_ok);

    /* Do it again with Overwrite: F to check that fails. */
    ONN("COPY on existing resource with Overwrite: F",
	ne_copy(i_session, 0, NE_DEPTH_INFINITE, src, dest) != NE_ERROR);

    if (STATUS(412)) {
	t_warning("COPY-on-existing fails with 412");
    }
    
    ONV(ne_copy(i_session, 1, NE_DEPTH_INFINITE, src, dest),
	("COPY-on-existing with 'Overwrite: T': %s", ne_get_error(i_session)));

    /* tricky one this, I didn't think it should work, but the spec
     * makes it look like it should. */
    ONV(ne_copy(i_session, 1, NE_DEPTH_INFINITE, src, coll),
	("COPY overwrites collection: %s", ne_get_error(i_session)));
    
    if (STATUS(204)) {
	t_warning("COPY to existing resource didn't give 204");
    }

    return OK;
}

static int copy_nodestcoll(void)
{
    char *nodest = ne_concat(i_path, "nonesuch/foo", NULL);
    int ret;

    PRECOND(copy_ok);

    ret = ne_copy(i_session, 0, NE_DEPTH_ZERO, src, nodest);
    
    ONV(ret == NE_OK, 
        ("COPY into non-existant collection '%snonesuch' succeeded", i_path));

    if (STATUS(409)) {
        t_warning("COPY to non-existant collection '%snonesuch' gave '%s' not 409",
                  i_path, ne_get_error(i_session));
    }

    ne_free(nodest);
    return OK;
}

static int copy_cleanup(void)
{
    ne_delete(i_session, src);
    ne_delete(i_session, dest);
    ne_delete(i_session, ncoll);
    ne_delete(i_session, coll);
    return OK;
}

static int copy_coll(void)
{
    int n;
    char *csrc, *cdest, *rsrc, *rdest, *subsrc, *subdest, *cdest2;
    char res[512];

    csrc = ne_concat(i_path, "ccsrc/", NULL);
    cdest = ne_concat(i_path, "ccdest/", NULL);
    cdest2 = ne_concat(i_path, "ccdest2/", NULL);
    rsrc = ne_concat(i_path, "ccsrc/foo", NULL);
    rdest = ne_concat(i_path, "ccdest/foo", NULL); 
    subsrc = ne_concat(i_path, "ccsrc/subcoll/", NULL);
    subdest = ne_concat(i_path, "ccdest/subcoll/", NULL);

    /* Set up the ccsrc collection. */
    ONMREQ("MKCOL", csrc, ne_mkcol(i_session, csrc));
    for (n = 0; n < 10; n++) {
	sprintf(res, "ccsrc/foo.%d", n);
	CALL(upload_foo(res));
    }
    ONMREQ("MKCOL", subsrc, ne_mkcol(i_session, subsrc));
    
    /* Clean up to make some fresh copies. */
    ne_delete(i_session, cdest);
    ne_delete(i_session, cdest2);

    /* Now copy the collection a couple of times */
    ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, csrc, cdest),
	("collection COPY `%s' to `%s': %s", csrc, cdest,
	 ne_get_error(i_session)));
    ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, csrc, cdest2),
	("collection COPY `%s' to `%s': %s", csrc, cdest,
	 ne_get_error(i_session)));

    ONN("COPY-on-existing-coll should fail",
	ne_copy(i_session, 0, NE_DEPTH_INFINITE, cdest, cdest2) != NE_ERROR);
    
    ONV(ne_copy(i_session, 1, NE_DEPTH_INFINITE, cdest2, cdest),
	("COPY-on-existing-coll with overwrite: %s", ne_get_error(i_session)));

    /* Remove the source, to be paranoid. */
    if (ne_delete(i_session, csrc)) {
	t_warning("Could not delete csrc");
    }

    /* Now delete things out of the destination collection to check if
     * they are there. */
    for (n = 0; n < 10; n++) {
	sprintf(res, "%s%s.%d", i_path, "ccdest/foo", n);
	ONV(ne_delete(i_session, res),
	    ("COPY destination coll missing %s? %s", res,
	     ne_get_error(i_session)));
    }

    ONV(ne_delete(i_session, subdest),
	("COPY destination missing sub-coll %s? %s", subdest,
	 ne_get_error(i_session)));	 

    /* Now nuke the whole of the second copy. */
    ONV(ne_delete(i_session, cdest2),
	("COPY destination %s missing? %s", cdest2, ne_get_error(i_session)));

    if (ne_delete(i_session, cdest)) {
	t_warning("Could not clean up cdest");
    }

    return OK;
}

static int copy_shallow(void)
{
    char *csrc, *cdest, *res;

    csrc = ne_concat(i_path, "ccsrc/", NULL);
    cdest = ne_concat(i_path, "ccdest/", NULL);

    /* Set up the ccsrc collection with one member */
    ONMREQ("MKCOL", csrc, ne_mkcol(i_session, csrc));
    CALL(upload_foo("ccsrc/foo"));

    /* Clean up to make some fresh copies. */
    ne_delete(i_session, cdest);

    /* Now copy with Depth 0 */
    ONV(ne_copy(i_session, 0, NE_DEPTH_ZERO, csrc, cdest),
	("collection COPY `%s' to `%s': %s", csrc, cdest,
	 ne_get_error(i_session)));

    /* Remove the source, to be paranoid. */
    if (ne_delete(i_session, csrc)) {
	t_warning("Could not delete csrc");
    }

    /* Now make sure the child resource hasn't been copied along with
     * the collection. */
    res = ne_concat(i_path, "foo", NULL);
    ne_delete(i_session, res);
    ONV(STATUS(404), 
        ("DELETE on `%s' should fail with 404: got %d", res, GETSTATUS));
    ne_free(res);

    if (ne_delete(i_session, cdest)) {
	t_warning("Could not clean up cdest");
    }

    return OK;
}

static int move(void)
{
    char *src2;

    src = ne_concat(i_path, "move", NULL);
    src2 = ne_concat(i_path, "move2", NULL);
    dest = ne_concat(i_path, "movedest", NULL);
    coll = ne_concat(i_path, "movecoll/", NULL);
    ncoll = ne_concat(i_path, "movecoll", NULL);

    /* Upload it twice. */
    CALL(upload_foo("move"));
    CALL(upload_foo("move2"));
    ONMREQ("MKCOL", coll, ne_mkcol(i_session, coll));

    /* Now move it */
    ONM2REQ("MOVE", src, dest, ne_move(i_session, 0, src, dest));

    if (STATUS(201)) {
	t_warning("MOVE to new resource didn't give 201");
    }

    /* Try a move with Overwrite: F to check that fails. */
    ONM2REQ("MOVE on existing resource with Overwrite: F succeeded",
	    src2, dest, 
	    ne_move(i_session, 0, src2, dest) != NE_ERROR);

    if (STATUS(412)) {
	t_warning("MOVE-on-existing should fail with 412");
    }

    ONM2REQ("MOVE onto existing resource with Overwrite: T",
	    src2, dest,
	    ne_move(i_session, 1, src2, dest));

    ONM2REQ("MOVE overwrites collection", coll, dest,
	    ne_move(i_session, 1, dest, coll));
    
    if (STATUS(204)) {
	t_warning("MOVE to existing collection resource didn't give 204");
    }
		  
    if (ne_delete(i_session, ncoll)) {
	t_warning("Could not clean up `%s'", ncoll);
    }

    return OK;
}

static char *mdest, *msrc, *mdest2, *mnoncoll;

#define SERR (ne_get_error(i_session))

static int move_coll(void)
{
    int n;
    char *rsrc, *rdest, *subsrc, *subdest;
    char res[512];

    msrc = ne_concat(i_path, "mvsrc/", NULL);
    mdest = ne_concat(i_path, "mvdest/", NULL);
    mdest2 = ne_concat(i_path, "mvdest2/", NULL);
    rsrc = ne_concat(i_path, "mvsrc/foo", NULL);
    rdest = ne_concat(i_path, "mvdest/foo", NULL); 
    subsrc = ne_concat(i_path, "mvsrc/subcoll/", NULL);
    subdest = ne_concat(i_path, "mvdest/subcoll/", NULL);
    mnoncoll = ne_concat(i_path, "mvnoncoll", NULL);

    /* Set up the mvsrc collection. */
    ONMREQ("MKCOL", msrc, ne_mkcol(i_session, msrc));
    for (n = 0; n < 10; n++) {
	sprintf(res, "mvsrc/foo.%d", n);
	CALL(upload_foo(res));
    }
    CALL(upload_foo("mvnoncoll"));

    ONV(ne_mkcol(i_session, subsrc),
	("MKCOL of `%s' failed: %s\n", subsrc, SERR));
    
    /* Now make a copy of the collection */
    ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, msrc, mdest2),
	("collection COPY `%s' to `%s', depth infinity: %s",
	 msrc, mdest2, SERR));

    ONV(ne_move(i_session, 0, msrc, mdest),
	("collection MOVE `%s' to `%s': %s", msrc, mdest, SERR));

    ONN("MOVE-on-existing-coll should fail",
	ne_move(i_session, 0, mdest, mdest2) != NE_ERROR);
    
    ONN("MOVE-on-existing-coll with overwrite",
	ne_move(i_session, 1, mdest2, mdest));

    /* Take another copy. */
    ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, mdest, mdest2),
	("collection COPY `%s' to `%s', depth infinity: %s",
	 mdest, mdest2, ne_get_error(i_session)));

    /* Now delete things out of the destination collection to check if
     * they are there. */
    for (n = 0; n < 10; n++) {
	sprintf(res, "%s%s.%d", i_path, "mvdest/foo", n);
	ONV(ne_delete(i_session, res),
	    ("DELETE from copied collection failed for `%s': %s",
	     res, SERR));
    }

    ONV(ne_delete(i_session, subdest),
	("DELETE from copied collection failed for `%s': %s",
	 subdest, SERR));

    /* And move the spare collection over a non-coll. */
    ONV(ne_move(i_session, 1, mdest2, mnoncoll),
	("MOVE collection `%s' over non-collection `%s' with overwrite: %s",
	 mdest2, mnoncoll, ne_get_error(i_session)));

    return OK;
}

static int move_cleanup(void)
{
    ne_delete(i_session, mdest);
    ne_delete(i_session, mdest2);
    ne_delete(i_session, mnoncoll);
    return OK;
}

ne_test tests[] = {
    INIT_TESTS,

    /*** Copy/move tests. ***/
    T(copy_init), T(copy_simple), T(copy_overwrite), 
    T(copy_nodestcoll), 
    T(copy_cleanup), 

    T(copy_coll), T(copy_shallow),

    T(move), T(move_coll), T(move_cleanup),

    FINISH_TESTS
};

    
