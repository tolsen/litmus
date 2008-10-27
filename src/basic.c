/* 
   litmus: WebDAV server test suite
   Copyright (C) 2001-2005, Joe Orton <joe@manyfish.co.uk>
                                                                     
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

#if 0
static struct {
    const char *name;
    int found;
} methods[] = {
#define M(x) { #x, 0 }
    M(PROPFIND), M(HEAD), M(GET), M(OPTIONS), M(DELETE), 
    M(PROPPATCH), M(COPY), M(MOVE), M(LOCK), M(UNLOCK),
    { NULL, 0 }
};

static void allow_hdr(void *userdata, const char *value)
{
    char *str = ne_strdup(value), *pnt = str;

    do {
	char *tok = ne_token(&pnt, ',', NULL);
	int n;
	
	for (n = 0; methods[n].name != NULL; n++) {
	    if (strcmp(methods[n].name, tok) == 0) {
		methods[n].found = 1;
		break;
	    }
	}	
	
    } while (pnt != NULL);    

    free(str);
}

static int allowed(const char *method)
{
    int n;

    for (n = 0; methods[n].name != NULL; n++) {
	if (strcmp(methods[n].name, method) == 0) {
	    return methods[n].found;
	    break;
	}
    }	

    return -1;
}

/* pull in from ne_basic.c. */
extern void dav_hdr_handler(void *userdata, const char *value);

static int adv_options(void)
{
    ne_request *req = ne_request_create(i_session, "OPTIONS", "/dav/");

    ne_add_response_header_handler(req, "Allow", allow_hdr, NULL);
    ne_add_response_header_handler(req, "DAV", dav_hdr_handler, &caps);

    ONREQ(ne_request_dispatch(req) || ne_get_status(req)->code != 200);

    ne_request_destroy(req);

    return OK;
}

#endif

const char *test_contents = ""
"This is\n"
"a test file.\n"
"for litmus\n"
"testing.\n";

static char *pg_uri = NULL;

static int do_put_get(const char *segment)
{
    char *fn, tmp[] = "/tmp/litmus2-XXXXXX", *uri;
    int fd, res;
    
    fn = create_temp(test_contents);
    ONN("could not create temporary file", fn == NULL);        

    uri = ne_concat(i_path, segment, NULL);

    fd = open(fn, O_RDONLY | O_BINARY);
    ONV(ne_put(i_session, uri, fd),
	("PUT of `%s' failed: %s", uri, ne_get_error(i_session)));
    close(fd);
    
    if (STATUS(201)) {
	t_warning("PUT of new resource gave %d, should be 201",
		  GETSTATUS);
    }

    fd = mkstemp(tmp);
    BINARYMODE(fd);
    ONV(ne_get(i_session, uri, fd),
	("GET of `%s' failed: %s", uri, ne_get_error(i_session)));
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

    /* so delete() isn't skipped. */
    pg_uri = uri;

    return OK;
}

static int put_get(void)
{
    return do_put_get("res");
}

static int put_get_utf8_segment(void)
{
    return do_put_get("res-%e2%82%ac");
}

static int mkcol_over_plain(void)
{
    PRECOND(pg_uri);

    ONV(ne_mkcol(i_session, pg_uri) != NE_ERROR,
	("MKCOL on plain resource `%s' succeeded!", pg_uri));
    
    return OK;
}

static int delete(void)
{
    PRECOND(pg_uri); /* skip if put_get failed. */

    ONV(ne_delete(i_session, pg_uri),
	("DELETE on normal resource failed: %s", ne_get_error(i_session)));

    return OK;
}

static int delete_null(void)
{
    char *uri;

    uri = ne_concat(i_path, "404me", NULL);
    ONN("DELETE nonexistent resource succeeded",
	ne_delete(i_session, uri) != NE_ERROR);

    if (STATUS(404)) {
	t_warning("DELETE on null resource gave %d, should be 404",
		  GETSTATUS);
    }

    return OK;
}

static int delete_fragment(void)
{
    char *uri = ne_concat(i_path, "frag/", NULL);
    char *frag = ne_concat(i_path, "frag/#ment", NULL);
    
    ONN("could not create collection", ne_mkcol(i_session, uri) != NE_OK);

    if (ne_delete(i_session, frag) == NE_OK) {
        t_warning("DELETE removed collection resource with Request-URI including fragment; unsafe");
    } else {
        ONMREQ("DELETE", uri, ne_delete(i_session, uri));
    }               

    return OK;
}

static char *coll_uri = NULL;

static int mkcol_forbidden(void)
{
    char *uri;

    uri = ne_concat(i_path, "../../coll/", NULL);
    
    ONN("MKCOL on root collection with user test1 succeeds",
	ne_mkcol(i_session, uri) != NE_ERROR);
    
	if (STATUS(403)) {
	t_warning("MKCOL of new collection gave %d, should be 403",
		  GETSTATUS);
	}

	return OK;
}

static int mkcol(void)
{
    char *uri;

    uri = ne_concat(i_path, "coll/", NULL);
    
    ONV(ne_mkcol(i_session, uri),
	("MKCOL %s: %s", uri, ne_get_error(i_session)));
	
	if (STATUS(201)) {
	t_warning("MKCOL of new collection gave %d, should be 201",
		  GETSTATUS);
	}
    
    coll_uri = uri; /* for subsequent tests. */

    return OK;
}

static int mkcol_again(void)
{
    PRECOND(coll_uri);

    ONN("MKCOL on existing collection succeeds",
	ne_mkcol(i_session, coll_uri) != NE_ERROR);

    if (STATUS(405)) {
	t_warning("MKCOL on existing collection gave %d, should be 405",
		  GETSTATUS);
    }
    
    return OK;
}

static int mkcol_percent_encoded(void)
{
    char *uri;
    uri = ne_concat(i_path, "coll%20A/", NULL);
    
    ONV(ne_mkcol(i_session, uri),
	    ("MKCOL %s: %s", uri, ne_get_error(i_session)));
	
	if (STATUS(201)) {
    	t_warning("MKCOL of new collection gave %d, should be 201",GETSTATUS);
	}
    
    uri=ne_concat(i_path,"coll A/",NULL); 
    
    ONN("MKCOL on existing collection succeeds",
	    ne_mkcol(i_session, uri) != NE_ERROR);
   return OK;
}

static int delete_coll(void)
{
    PRECOND(coll_uri);
    
    ONV(ne_delete(i_session, coll_uri),
	("DELETE on collection `%s': %s", coll_uri, 
	 ne_get_error(i_session)));

	if (STATUS(204)) {
	t_warning("DELETE of resource gave %d, should be 204",
		  GETSTATUS);
	}

    return OK;
}

static int mkcol_no_parent(void)
{
    char *uri;

    uri = ne_concat(i_path, "409me/noparent/", NULL);

    ONN("MKCOL with missing intermediate succeeds",
	ne_mkcol(i_session, uri) != NE_ERROR);
    
    if (STATUS(409)) {
	t_warning("MKCOL with missing intermediate gave %d, should be 409",
		  GETSTATUS);
    }

    free(uri);

    return OK;
}

static int mkcol_with_body(void)
{
    char *uri;
    ne_request *req;
    static const char body[] = "foo-bar-blah";

    uri = ne_concat(i_path, "mkcolbody", NULL);

    req = ne_request_create(i_session, "MKCOL", uri);

    /* Presume nobody will register this MIME type. */
    ne_add_request_header(req, "Content-Type", "xzy-foo/bar-512");
    
    ne_set_request_body_buffer(req, body, strlen(body));

    ONV(ne_request_dispatch(req),
	("MKCOL on `%s' with (invalid) body: %s", uri,
	 ne_get_error(i_session)));
    
    ONN("MKCOL with weird body must fail (RFC2518:8.3.1)",
	ne_get_status(req)->klass == 2);
    
    ONN("MKCOL with weird body must fail with 415 (RFC2518:8.3.1)",
	ne_get_status(req)->code != 415);
    
    ne_request_destroy(req);

    return OK;
}

static int check_last_modified(void)
{
    const char *uri, *res, *last_modified1, *last_modified2;
    uri = ne_concat(i_path, "mycoll/", NULL);

    ONV(ne_mkcol(i_session, uri),
        ("MKCOL %s: %s", uri, ne_get_error(i_session)));
    last_modified1=get_lastmodified(uri);
    
    sleep(2);

    res=ne_concat(uri,"a",NULL);
    ONV(ne_mkcol(i_session, res),
        ("MKCOL %s: %s", res, ne_get_error(i_session)));
      
    last_modified2=get_lastmodified(uri);
    
    if(strcmp(last_modified1,last_modified2)==0)
        t_warning("Collection changed. Last modified should have changed. %s , %s",last_modified1, last_modified2);
    
    return OK;
}

/*Changed content to chk the ETags*/
const char *test_contents1 = ""
"This is\n"
"a test file.\n"
"for litmus\n"
"testing of etags.\n";

const char *resETag;

/*put a resource identified by the uri returns the etag of the resource.*/
static int do_put_return_ETag(const char *segment, const char *contents)
{
    const char *fn, *uri;
    int fd;
    fn = create_temp(contents);

    ONN("could not create temporary file", fn==NULL);

    uri = ne_concat(i_path, segment, NULL);
    fd = open(fn, O_RDONLY | O_BINARY);
    ONV(ne_put(i_session, uri, fd),
	("PUT of `%s' failed: %s", uri, ne_get_error(i_session)));
    close(fd);

    if (STATUS(201)) {
	if(STATUS(204)){
	t_warning("PUT of new resource gave %d, should be 201 or 204",
		  GETSTATUS);
	}
    }
   resETag = get_etag(uri);
   unlink (fn);
    return OK;

}


static int chk_ETag(void)
{
    const char *etag, *etag1;
    do_put_return_ETag("resETag", "test_contents");
    etag=resETag;
    do_put_return_ETag("resETag", "test_contents1");
    etag1=resETag;
    if (strcmp(etag,etag1) ==0)
	t_warning("resource changed. Etag should have changed. %s , %s",etag, etag1);

    return OK;
}

ne_test tests[] = {
    INIT_TESTS,

    /* Basic tests. */
    T(options),
    T(put_get),
    T(put_get_utf8_segment),
    T(mkcol_over_plain),
    T(delete),
    T(delete_null),
    T(delete_fragment),
    T(mkcol),
    T(mkcol_percent_encoded),
    T(mkcol_again),
    T(delete_coll),
    T(mkcol_no_parent),
    T(mkcol_with_body),
    T(mkcol_forbidden),
    T(chk_ETag),

    FINISH_TESTS
};
   
