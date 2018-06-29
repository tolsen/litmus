/* 
   litmus: WebDAV server test suite
   Copyright (C) 2001-2007, Joe Orton <joe@manyfish.co.uk>
                                                                     
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

/* BINARYMODE() enables binary file I/O on cygwin. */
#ifdef __CYGWIN__
#define BINARYMODE(fd) do { setmode(fd, O_BINARY); } while (0)
#else
#define BINARYMODE(fd) if (0)
#endif

static char *create_temp(const char *contents)
{
    char tmp[256] = "/tmp/litmus-XXXXXX";
    int fd;
    size_t len = strlen(contents);
    
    fd = mkstemp(tmp);
    BINARYMODE(fd);
    if (write(fd, contents, len) != (ssize_t)len) {
        close(fd);
        return NULL;
    }        
    close(fd);

    return ne_strdup(tmp);
}

static int compare_contents(const char *fn, const char *contents)
{
    int fd = open(fn, O_RDONLY | O_BINARY), ret;
    char buffer[BUFSIZ];
    ne_buffer *b = ne_buffer_create();
    ssize_t bytes;

    while ((bytes = read(fd, buffer, BUFSIZ)) > 0) {
	ne_buffer_append(b, buffer, bytes);
    }

    close(fd);

#define SvsS "%" NE_FMT_SIZE_T " vs %" NE_FMT_SIZE_T
    if (strlen(b->data) != strlen(contents)) {
	t_warning("length mismatch: " SvsS, strlen(b->data), strlen(contents));
    }
    if (strlen(b->data) != ne_buffer_size(b)) {
	t_warning("buffer problem: " SvsS, 
		  strlen(b->data), ne_buffer_size(b));
    }
#undef SvsS

    ret = memcmp(b->data, contents, ne_buffer_size(b));
    ne_buffer_destroy(b);

    return ret;
}

static const char *test_contents = ""
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

static int put_no_parent(void)
{
    char *uri = ne_concat(i_path, "409me/noparent.txt", NULL);
    ONN("MKCOL with missing intermediate succeeds",
	ne_mkcol(i_session, uri) != NE_ERROR);
    
    if (STATUS(409)) {
	t_warning("MKCOL with missing intermediate gave %d, should be 409",
		  GETSTATUS);
    }

    ne_free(uri);

    return OK;
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
	t_warning("DELETE on null resource gave %d, should be 404 (RFC2518:S3)",
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

static int mkcol(void)
{
    char *uri;

    uri = ne_concat(i_path, "coll/", NULL);
    
    ONV(ne_mkcol(i_session, uri),
	("MKCOL %s: %s", uri, ne_get_error(i_session)));
    
    coll_uri = uri; /* for subsequent tests. */

    return OK;
}

static int mkcol_again(void)
{
    PRECOND(coll_uri);

    ONN("MKCOL on existing collection should fail (RFC2518:8.3.1)",
	ne_mkcol(i_session, coll_uri) != NE_ERROR);

    if (STATUS(405)) {
	t_warning("MKCOL on existing collection gave %d, should be 405 (RFC2518:8.3.2)",
		  GETSTATUS);
    }
    
    return OK;
}

static int delete_coll(void)
{
    PRECOND(coll_uri);
    
    ONV(ne_delete(i_session, coll_uri),
	("DELETE on collection `%s': %s", coll_uri, 
	 ne_get_error(i_session)));

    return OK;
}

static int mkcol_no_parent(void)
{
    char *uri;

    uri = ne_concat(i_path, "409me/noparent/", NULL);

    ONN("MKCOL with missing intermediate should fail (RFC2518:8.3.1)",
	ne_mkcol(i_session, uri) != NE_ERROR);
    
    if (STATUS(409)) {
	t_warning("MKCOL with missing intermediate gave %d, should be 409 (RFC2518:8.3.1)",
		  GETSTATUS);
    }

    free(uri);

    return OK;
}

static int mkcol_with_body(void)
{
    char *uri;
    ne_request *req;
    static const char body[] = "afafafaf";

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

ne_test tests[] = {
    INIT_TESTS,

    /* Basic tests. */
    T(options),
    T(put_get),
    T(put_get_utf8_segment),
    T(put_no_parent),
    T(mkcol_over_plain),
    T(delete),
    T(delete_null),
    T(delete_fragment),
    T(mkcol),
    T(mkcol_again),
    T(delete_coll),
    T(mkcol_no_parent),
    T(mkcol_with_body),

    FINISH_TESTS
};
   
