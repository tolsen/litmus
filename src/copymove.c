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

const char *test_contents = ""
"This is\n"
"a test file.\n"
"for litmus copymove\n"
"testing.\n";

static int copy_content_check(void)
{
  char *fn, tmp[] = "/tmp/litmus2-XXXXXX", *src, *dest;
  int fd, res;
   
  fn = create_temp(test_contents);
  ONN("could not create temporary file", fn == NULL);    
  
  src = ne_concat(i_path, "resource", NULL);
  dest = ne_concat(i_path, "copyresource", NULL);
  
  fd = open(fn, O_RDONLY | O_BINARY);
  ONV(ne_put(i_session, src, fd),
      ("PUT of `%s' failed: %s", src, ne_get_error(i_session)));
  close(fd);
  
  if (STATUS(201)) {
    t_warning("PUT of new resource gave %d, should be 201",
	      GETSTATUS);
  }
  
  
  ONNREQ("simple resource COPY", 
	 ne_copy(i_session, 0, NE_DEPTH_INFINITE, src, dest));
  
  
  if (STATUS(201)) {
    t_warning("COPY to new resource didn't give 201");
  }
  
  
  fd = mkstemp(tmp);
  BINARYMODE(fd);
  ONV(ne_get(i_session, dest, fd),
      ("GET of `%s' failed: %s", dest, ne_get_error(i_session)));
  close(fd);
  
  if (STATUS(200)) {
    t_warning("GET of new resource gave %d, should be 200",
	      GETSTATUS);
  }
  
  res = compare_contents(tmp, test_contents);
  if (res != OK) {
    char cmd[1024];
    
    ne_snprintf(cmd, 1024, "diff -u %s %s", fn, tmp);
    system(cmd);
    ONN("PUT/GET byte comparison", res);
  }
  
  /* Clean up. */
  unlink(fn);
  unlink(tmp);
  
  ONV(ne_delete(i_session, src),
      ("DELETE on normal resource failed: %s", ne_get_error(i_session)));
  
  ONV(ne_delete(i_session, dest),
      ("DELETE on normal resource failed: %s", ne_get_error(i_session)));
    
  return OK;
}

static int copy_coll_depth(void)
{
    char *csrc, *cdest, *csubsrc, *csubdest;
	
	csrc = ne_concat(i_path, "csrc/", NULL);
	csubsrc = ne_concat(i_path, "csrc/subsrc", NULL);
	cdest = ne_concat(i_path, "cdest/", NULL);
	csubdest = ne_concat(i_path, "cdest/subsrc", NULL);
    
	ONMREQ("MKCOL", csrc, ne_mkcol(i_session, csrc));
    ONMREQ("MKCOL", csubsrc, ne_mkcol(i_session, csubsrc));
    
	ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, csrc, cdest),
	("collection COPY `%s' to `%s': %s", csrc, cdest,
	 ne_get_error(i_session)));

	if (STATUS(201)) {
	t_warning("COPY to new collection gave %d, should be 201",
		  GETSTATUS);
	}
    
	ONV(ne_delete(i_session, csubdest),
	("COPY destination missing coll %s? %s", csubdest,
	 ne_get_error(i_session)));	 
	
	
	ONV(ne_copy(i_session, 1, NE_DEPTH_ZERO, csrc, cdest),
	("collection COPY `%s' to `%s': %s", csrc, cdest,
	 ne_get_error(i_session)));
	
	if (STATUS(204)) {
	t_warning("COPY to new collection gave %d, should be 204",
		  GETSTATUS);
	}

	//TODO: This should fail but currently we need to add this in the code
	//ONN("Delete-on-sub-destination-collection should fail",
	//ne_delete(i_session, csubdest) != NE_ERROR);
    
    ONV(ne_delete(i_session, cdest),
	("COPY destination missing coll %s? %s", cdest,
	 ne_get_error(i_session)));	 
	
	ONV(ne_delete(i_session, csubsrc),
	("COPY destination missing sub-coll %s? %s", csubsrc,
	 ne_get_error(i_session)));	 
    ONV(ne_delete(i_session, csrc),
	("COPY destination missing sub-coll %s? %s", csrc,
	 ne_get_error(i_session)));	 

	
	return OK;
}


static int copy_coll(void)
{
    int n;
    char *csrc, *cdest, *rsrc, *rdest, *subsrc, *subdest, *cdest2, *forbiddendest;
    char res[512];

    csrc = ne_concat(i_path, "ccsrc/", NULL);
    cdest = ne_concat(i_path, "ccdest/", NULL);
    cdest2 = ne_concat(i_path, "ccdest2/", NULL);
    rsrc = ne_concat(i_path, "ccsrc/foo", NULL);
    rdest = ne_concat(i_path, "ccdest/foo", NULL); 
    subsrc = ne_concat(i_path, "ccsrc/subcoll/", NULL);
    subdest = ne_concat(i_path, "ccdest/subcoll/", NULL);
	forbiddendest = ne_concat(i_path, "../../cdest/", NULL);

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

	if (STATUS(201)) {
	t_warning("COPY to new collection gave %d, should be 201",
		  GETSTATUS);
	}
	
    ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, csrc, cdest2),
	("collection COPY `%s' to `%s': %s", csrc, cdest,
	 ne_get_error(i_session)));

    ONN("COPY-on-existing-coll should fail",
	ne_copy(i_session, 0, NE_DEPTH_INFINITE, cdest, cdest2) != NE_ERROR);
   
	if (STATUS(412)) {
	t_warning("COPY on existing collection gave %d, should be 412",
		  GETSTATUS);
	}
	
	ONN("COPY-to-self should fail",
	ne_copy(i_session, 1, NE_DEPTH_INFINITE, cdest, cdest) != NE_ERROR);
	
	/* Maybe this should be 403, currently discussing in TODO*/
	if (STATUS(403)) {
	    t_warning("Copy to self gave %d, should be 403",GETSTATUS);
	}
	
	ONN("COPY-on-root-collection should fail",
	ne_copy(i_session, 1, NE_DEPTH_INFINITE, cdest, forbiddendest) != NE_ERROR);
   
	if (STATUS(403)) {
	t_warning("COPY to root collection gave %d, should be 403",
		  GETSTATUS);
	}
	
    /*
	 * ONV(ne_copy(i_session, 1, NE_DEPTH_INFINITE, cdest2, cdest),
	("COPY-on-existing-coll with overwrite: %s", ne_get_error(i_session)));

	if (STATUS(204)) {
	t_warning("COPY on existing collection gave %d, should be 204",
		  GETSTATUS);
	}
    */
	
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

static int copy_med_on_coll(void)
{
    int n;
    char *csrc,*cdest, *csubdest;
    char res[512];
    
    sprintf(res, "foofile", n);
    CALL(upload_foo(res));
    
    csrc = ne_concat(i_path, "foofile", NULL);
    cdest = ne_concat(i_path, "dest", NULL);
    csubdest = ne_concat(i_path, "dest/dest1", NULL);
    
    ONMREQ("MKCOL", cdest, ne_mkcol(i_session, cdest));
    //ONMREQ("MKCOL", subsrc, ne_mkcol(i_session, csubdest));
    
    ONV(ne_copy(i_session, 1, NE_DEPTH_INFINITE, csrc, cdest),
	("collection COPY `%s' to `%s': %s", csrc, cdest,
	 ne_get_error(i_session)));
    
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

    /* TODO: Maybe status message should be 403 here */
	if (STATUS(412)) {
	t_warning("MOVE-on-existing should fail with 412 gave %d", GETSTATUS);
    }

    /*ONM2REQ("MOVE onto existing resource with Overwrite: T",
	    src2, dest,
	    ne_move(i_session, 1, src2, dest));

    ONM2REQ("MOVE overwrites collection", coll, dest,
	    ne_move(i_session, 1, dest, coll));
    
    if (STATUS(204)) {
	t_warning("MOVE to existing collection resource didn't give 204");
    }
	*/
	
    if (ne_delete(i_session, dest)) {
	t_warning("Could not clean up `%s'", dest);
    }
	
    if (ne_delete(i_session, src2)) {
	t_warning("Could not clean up `%s'", src2);
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
    char *rsrc, *rdest, *subsrc, *subdest, *forbiddendest;
    char res[512];
	
    msrc = ne_concat(i_path, "mvsrc/", NULL);
    mdest = ne_concat(i_path, "mvdest/", NULL);
    mdest2 = ne_concat(i_path, "mvdest2/", NULL);
    rsrc = ne_concat(i_path, "mvsrc/foo", NULL);
    rdest = ne_concat(i_path, "mvdest/foo", NULL); 
    subsrc = ne_concat(i_path, "mvsrc/subcoll/", NULL);
    subdest = ne_concat(i_path, "mvdest/subcoll/", NULL);
    mnoncoll = ne_concat(i_path, "mvnoncoll", NULL);
	forbiddendest = ne_concat(i_path, "../../cdest/", NULL);

    /* Set up the mvsrc collection. */
    ONMREQ("MKCOL", msrc, ne_mkcol(i_session, msrc));
    for (n = 0; n < 10; n++) {
	sprintf(res, "mvsrc/foo.%d", n);
	CALL(upload_foo(res));
    }
    CALL(upload_foo("mvnoncoll"));

    ONV(ne_mkcol(i_session, subsrc),
	("MKCOL of `%s' failed: %s\n", subsrc, SERR));
	
    if (STATUS(201)) {
      t_warning("MKCOL of new collection gave %d, should be 201",
		GETSTATUS);
    }
    
    /* Now make a copy of the collection */
    ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, msrc, mdest2),
	("collection COPY `%s' to `%s', depth infinity: %s",
	 msrc, mdest2, SERR));

	if (STATUS(201)) {
	t_warning("COPY to new collection gave %d, should be 201",
		  GETSTATUS);
	}
	
	ONV(ne_move(i_session, 0, msrc, mdest),
	("collection MOVE `%s' to `%s': %s", msrc, mdest, SERR));

	if (STATUS(201)) {
	t_warning("Move to new collection gave %d, should be 201",
		  GETSTATUS);
	}
	
    
	ONN("MOVE-on-existing-coll should fail",
	ne_move(i_session, 0, mdest, mdest2) != NE_ERROR);
	
	if (STATUS(412)) {
	t_warning("Move to existing collection with overwrite=F gave %d, should be 412",
		  GETSTATUS);
	}
    
	ONN("MOVE-to-self should fail",
	ne_move(i_session, 1, mdest, mdest) != NE_ERROR);
	
	/* Maybe this should be 412, currently discussing in TODO*/
	if (STATUS(403)) {
	t_warning("Move to self gave %d, should be 403",
		  GETSTATUS);
	}
	
	ONN("MOVE-in-root-directory should fail",
	ne_move(i_session, 1, mdest, forbiddendest) != NE_ERROR);
	
	if (STATUS(403)) {
	t_warning("Move to root collection gave %d, should be 403",
		  GETSTATUS);
	}
    
    /*
	 * ONN("MOVE-on-existing-coll with overwrite",
	ne_move(i_session, 1, mdest2, mdest));

	if (STATUS(204)) {
	t_warning("Move to existing collection with overwrite=T gave %d, should be 204",
		  GETSTATUS);
	}
	*/

	
    /* Take another copy. */
	/*
    ONV(ne_copy(i_session, 0, NE_DEPTH_INFINITE, mdest, mdest2),
	("collection COPY `%s' to `%s', depth infinity: %s",
	 mdest, mdest2, ne_get_error(i_session)));
*/
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
    ne_delete(i_session, mnoncoll);
	
    ONV(ne_move(i_session, 1, mdest2, mnoncoll),
	("MOVE collection `%s' over non-collection `%s' with overwrite: %s",
	 mdest2, mnoncoll, ne_get_error(i_session)));

    return OK;
}

static int move_cleanup(void)
{
    ne_delete(i_session, mdest);
    ne_delete(i_session, mnoncoll);
    return OK;
}


static int move_content_check(void)
{
  char *fn, tmp[] = "/tmp/litmus2-XXXXXX", *src, *dest;
  int fd, res;
   
  fn = create_temp(test_contents);
  ONN("could not create temporary file", fn == NULL);    
  
  src = ne_concat(i_path, "resource", NULL);
  dest = ne_concat(i_path, "copyresource", NULL);
  
  fd = open(fn, O_RDONLY | O_BINARY);
  ONV(ne_put(i_session, src, fd),
      ("PUT of `%s' failed: %s", src, ne_get_error(i_session)));
  close(fd);
  
  if (STATUS(201)) {
    t_warning("PUT of new resource gave %d, should be 201",
	      GETSTATUS);
  }
  
  
  ONNREQ("simple resource MOVE", 
	 ne_move(i_session, 0, src, dest));
  
  
  if (STATUS(201)) {
    t_warning("MOVE to new resource didn't give 201");
  }
  
  
  fd = mkstemp(tmp);
  BINARYMODE(fd);
  ONV(ne_get(i_session, dest, fd),
      ("GET of `%s' failed: %s", dest, ne_get_error(i_session)));
  close(fd);
  
  if (STATUS(200)) {
    t_warning("GET of new resource gave %d, should be 200",
	      GETSTATUS);
  }
  
  res = compare_contents(tmp, test_contents);
  if (res != OK) {
    char cmd[1024];
    
    ne_snprintf(cmd, 1024, "diff -u %s %s", fn, tmp);
    system(cmd);
    ONN("PUT/GET byte comparison", res);
  }
  
  /* Clean up. */
  unlink(fn);
  unlink(tmp);
  
  ONV(ne_delete(i_session, dest),
      ("DELETE on normal resource failed: %s", ne_get_error(i_session)));
    
  return OK;
}

static int move_collection_check(void)
{
    char *subsrc, *subdest;
    
    src = ne_concat(i_path, "move", NULL);
    subsrc = ne_concat(i_path, "move/move2", NULL);
    dest = ne_concat(i_path, "movedest", NULL);
    subdest = ne_concat(i_path, "movedest/move2", NULL);
    
    ONMREQ("MKCOL", src, ne_mkcol(i_session, src));
    ONMREQ("MKCOL", subsrc, ne_mkcol(i_session, subsrc));
    
    /* Now move it */
    ONM2REQ("MOVE", src, dest, ne_move(i_session, 1, src, dest));
    
    ONV(ne_delete(i_session, subdest),
        ("DELETE on normal resource failed: %s", ne_get_error(i_session)));
    
    ONV(ne_delete(i_session, dest),
        ("DELETE on normal resource failed: %s", ne_get_error(i_session)));
    
    int ret = ne_delete(i_session, src);
    
    ONV(ret == NE_OK, 
        ("DELETE into non-existant collection '%snonesuch' succeeded", i_path));
    
    return OK;
}

static int depth_zero_copy(void)
{
    char *subsrc, *subdest;
    src=ne_concat(i_path, "copy-a", NULL);
    subsrc = ne_concat(i_path, "copy-a/copy1", NULL);
    dest = ne_concat(i_path, "copy-b", NULL);
    subdest = ne_concat(i_path, "copy-b/copy2", NULL);
    
    ONMREQ("MKCOL", src, ne_mkcol(i_session, src));
    ONMREQ("MKCOL", subsrc, ne_mkcol(i_session, subsrc));
    ONMREQ("MKCOL", dest, ne_mkcol(i_session, dest));
    ONMREQ("MKCOL", subdest, ne_mkcol(i_session, subdest));
    
    /* Now make a copy (OVERWRITE='T' and Depth=0) of the collection */
    ONV(ne_copy(i_session, 1, NE_DEPTH_ZERO, src, dest),
    	("collection COPY `%s' to `%s', depth infinity: %s", src, dest, SERR));
    
    ONV(ne_delete(i_session, subdest),
        ("After COPY with depth Zero. DELETE on normal resource failed: %s", ne_get_error(i_session)));
       
    subsrc=ne_concat(i_path,"copy-b/copy1", NULL);
    //int ret= ne_delete(i_session,subsrc);
    ONN("Delete should have failed", ne_delete(i_session, subsrc) == NE_OK);
    ONV(ne_delete(i_session, dest),
        ("After COPY with depth Zero. DELETE on normal resource failed: %s", ne_get_error(i_session)));
    ONV(ne_delete(i_session, src),
        ("After COPY with depth Zero. DELETE on normal resource failed: %s", ne_get_error(i_session)));
    return OK;
    
}
    
    



ne_test tests[] = {
    INIT_TESTS,

    /*** Copy/move tests. ***/
    T(copy_init), T(copy_simple), T(copy_overwrite), 
    T(copy_nodestcoll), 
    T(copy_cleanup), 
    T(copy_content_check), 
    T(copy_coll_depth),	

    T(copy_coll), 
    T(depth_zero_copy), 
    T(copy_med_on_coll),
    T(move), 
    T(move_coll), 
    T(move_cleanup),
    T(move_content_check),
    T(move_collection_check),
    FINISH_TESTS
};

    
