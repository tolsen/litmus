/* 
   litmus: DAV server test suite
   Copyright (C) 2001-2006, Joe Orton <joe@manyfish.co.uk>
                                                                     
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

#include <stdlib.h>

#include <ne_request.h>
#include <ne_props.h>
#include <ne_uri.h>

#include "common.h"

#define NS "http://example.com/neon/litmus/"

#define NSPACE(x) ((x) ? (x) : "")

static const ne_propname props[] = {
    { "DAV:", "getcontentlength" },
    { "DAV:", "getlastmodified" },
    { "DAV:", "displayname" },
    { "DAV:", "resourcetype" },
    { NS, "foo" },
    { NS, "bar" },
    { NULL }
};

#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

#define XML_DECL "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" 

static const struct ne_xml_idmap map[] = {
    { "DAV:", "resourcetype", ELM_resourcetype },
    { "DAV:", "collection", ELM_collection }
};

struct private {
    int collection;
};

struct results {
    ne_propfind_handler *ph;
    int result;
};

#ifdef HAVE_NEON_026PLUS
static void d0_results(void *userdata, const ne_uri *uri,
		       const ne_prop_result_set *rset)
#else
static void d0_results(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
#endif
{
    struct results *r = userdata;
    const char *path;
#ifndef HAVE_NEON_026PLUS
    const char *scheme;
    size_t slen;

    scheme = ne_get_scheme(ne_get_session(ne_propfind_get_request(r->ph)));
    slen = strlen(scheme);

    if (strncmp(uri, scheme, slen) == 0 &&
        strncmp(uri+slen, "://", 3) == 0) {
	/* Absolute URI */
	path = strchr(uri+slen+3, '/');
	if (path == NULL) {
	    NE_DEBUG(NE_DBG_HTTP, "Invalid URI???");
	    return;
	}
    }
    else {
        path = uri;
    }
#else
    path = uri->path;
#endif

    if (ne_path_compare(path, i_path)) {
	t_warning("response href for wrong resource");
    } else {
	struct private *priv = ne_propset_private(rset);
	if (!priv->collection) {
	    r->result = FAIL;
	    t_context("Base collection did not define {DAV:}collection property");
	    return;
	} else {
	    r->result = 0;
	}
    }
}

#ifdef HAVE_NEON_026PLUS
static void *create_private(void *userdata, const ne_uri *uri)
{
    return ne_calloc(sizeof(struct private));
}

static void destroy_private(void *userdata, void *private)
{
    ne_free(private);
}

#else
static void *create_private(void *userdata, const char *uri)
{
    return ne_calloc(sizeof(struct private));
}
#endif

static int startelm(void *ud, int parent, const char *nspace,
                    const char *name, const char **atts)
{
    int state = ne_xml_mapid(map, NE_XML_MAPLEN(map), nspace, name);
    struct private *p = ne_propfind_current_private(ud);

    if (parent == NE_207_STATE_PROP && state == ELM_resourcetype)
        return ELM_resourcetype;
    else if (parent == ELM_resourcetype && state == ELM_collection)
        p->collection = 1;

    return NE_XML_DECLINE;
}

/* Depth 0 PROPFIND on root collection. */
static int propfind_d0(void)
{
    int ret;
    struct results r = {0};

    r.ph = ne_propfind_create(i_session, i_path, NE_DEPTH_ZERO);
    
    ne_propfind_set_private(r.ph, create_private, 
#ifdef HAVE_NEON_026PLUS
                            destroy_private,
#endif
                            NULL);

    r.result = FAIL;
    t_context("No responses returned");

    ne_xml_push_handler(ne_propfind_get_parser(r.ph), startelm,
                        NULL, NULL, r.ph);

    ret = ne_propfind_named(r.ph, props, d0_results, &r);

    if (r.result) {
	return r.result;
    }

    return OK;
}

static int do_invalid_pfind(const char *body, const char *failmsg)
{
    ne_request *req = ne_request_create(i_session, "PROPFIND", i_path);

    ne_set_request_body_buffer(req, body, strlen(body));

    ne_add_depth_header(req, NE_DEPTH_ZERO);

    ONV(ne_request_dispatch(req),
	("PROPFIND with %s failed: %s", failmsg, ne_get_error(i_session)));
    
    if (STATUS(400)) {
	t_context("PROPFIND with %s got %d response not 400", 
		  failmsg, GETSTATUS);
	return FAIL;
    }

    ne_request_destroy(req);

    return OK;
}

static int propfind_invalid(void)
{
    return do_invalid_pfind("<foo>", "non-well-formed XML request body");
}    

/* Julian Reschke's bug regarding invalid namespace declarations.
 * http://dav.lyra.org/pipermail/dav-dev/2001-August/002593.html
 * (mod_dav regression test).  */
static int propfind_invalid2(void)
{
    return do_invalid_pfind(
	"<D:propfind xmlns:D=\"DAV:\">"
	"<D:prop><bar:foo xmlns:bar=\"\"/>"
	"</D:prop></D:propfind>",
	"invalid namespace declaration in body (see FAQ)");
}


static int prop_ok = 0;
char *prop_uri;

static int propinit(void)
{
    prop_uri = ne_concat(i_path, "prop", NULL);
    
    ne_delete(i_session, prop_uri);

    CALL(upload_foo("prop"));

    prop_ok = 1;
    
    return OK;
}

#define NP (10)

static ne_proppatch_operation pops[NP + 1];
static ne_propname propnames[NP + 1];
static char *values[NP + 1];

static int numprops = NP, removedprops = 5;

#define PS_VALUE "value goes here"

static int propset(void)
{
    int n;
    char tmp[100];

    for (n = 0; n < numprops; n++) {
	sprintf(tmp, "prop%d", n);
	propnames[n].nspace = NS;
	propnames[n].name = ne_strdup(tmp);
	pops[n].name = &propnames[n];
	pops[n].type = ne_propset;
	sprintf(tmp, "value%d", n);
	values[n] = ne_strdup(tmp);
	pops[n].value = values[n];
    }

    memset(&pops[n], 0, sizeof(pops[n]));
    memset(&propnames[n], 0, sizeof(propnames[n]));	   
    values[n] = NULL;

    PRECOND(prop_ok);

    prop_ok = 0; /* if the PROPPATCH fails, no point in testing further. */
    ONMREQ("PROPPATCH", prop_uri, ne_proppatch(i_session, prop_uri, pops));
    prop_ok = 1;

    return OK;
}

#ifdef HAVE_NEON_026PLUS
static void pg_results(void *userdata, const ne_uri *uri,
		       const ne_prop_result_set *rset)
#else
static void pg_results(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
#endif
{
    struct results *r = userdata;
    const char *value;
    const ne_status *status;
    int n;

    r->result = 0;

    for (n = 0; n < numprops; n++) {
	value = ne_propset_value(rset, &propnames[n]);
	status = ne_propset_status(rset, &propnames[n]);

	if (values[n] == NULL) {
	    /* We should have received a 404 for this property. */

	    if (value == NULL) {
		if (status == NULL) {
		    t_warning("Property %d omitted from results with no status",
			      n);
		} else if (status->code != 404) {
		    t_warning("Status for missing property %d was not 404", n);
		}
	    } else {
		r->result = FAIL;
		t_context("Deleted property `{%s}%s' was still present",
			  NSPACE(propnames[n].nspace), propnames[n].name);
	    }
	} else {
	    /* We should have a value for this property. */
	    if (value == NULL) {
		t_context("No value given for property {%s}%s", 
			  NSPACE(propnames[n].nspace), propnames[n].name);
		r->result = FAIL;
	    } else if (strcmp(value, values[n])) {
		t_context("Property {%s}%s had value %s, expected %s",
			  NSPACE(propnames[n].nspace), propnames[n].name, 
			  value, values[n]);
		/* Check the value matches. */
		r->result = FAIL;
	    }
	}
    }
    
}

static int propget(void)
{
    struct results r = {0};

    PRECOND(prop_ok);

    r.result = 1;
    t_context("No responses returned");

    ONMREQ("PROPFIND", prop_uri,
	   ne_simple_propfind(i_session, prop_uri, NE_DEPTH_ZERO,
			      propnames, pg_results, &r));

    if (r.result) {
	return r.result;
    }	

    return OK;
}

static int propmove(void)
{
    char *dest;

    PRECOND(prop_ok);

    dest = ne_concat(i_path, "prop2", NULL);
    
    ne_delete(i_session, dest);

    ONM2REQ("MOVE", prop_uri, dest,
	    ne_move(i_session, 0, prop_uri, dest));

    free(prop_uri);
    prop_uri = dest;

    return OK;
}

static int propdeletes(void)
{
    int n;

    PRECOND(prop_ok);
    
    for (n = 0; n < removedprops; n++) {
	pops[n].type = ne_propremove;
	values[n] = NULL;
    }

    ONMREQ("PROPPATCH", prop_uri,
	   ne_proppatch(i_session, prop_uri, pops));

    return OK;
}

static int propreplace(void)
{
    int n;

    PRECOND(prop_ok);

    for (n = removedprops; n < numprops; n++) {
	char tmp[100];
	sprintf(tmp, "newvalue%d", n);
	pops[n].type = ne_propset;
	pops[n].value = values[n] = ne_strdup(tmp);
    }

    ONMREQ("PROPPATCH", prop_uri,
	   ne_proppatch(i_session, prop_uri, pops));

    return OK;
}

/* Test whether the response to a PROPFIND request with given body is
 * well-formed XML. */
static int propfind_returns_wellformed(const char *msg, const char *body)
{
    ne_xml_parser *p = ne_xml_create();
    ne_request *req = ne_request_create(i_session, "PROPFIND", prop_uri);

    ne_set_request_body_buffer(req, body, strlen(body));

    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, p);
    ONMREQ("PROPFIND", prop_uri, ne_request_dispatch(req));
    
    ONV(ne_xml_failed(p), ("PROPFIND response %s was not well-formed: %s",
                           msg, ne_xml_get_error(p)));

    ne_xml_destroy(p);
    ne_request_destroy(req);
    return OK;
}

/* Run a PROPPATCH request with given body; do an XML parse on the
 * response to make sure its well-formed.  Return failure with given
 * 'msg' if the request fails. */
static int do_patch(const char *failmsg, const char *body)
{
    ne_request *req = ne_request_create(i_session, "PROPPATCH", prop_uri);

    ne_set_request_body_buffer(req, body, strlen(body));
    
    ONNREQ(failmsg, ne_request_dispatch(req));
    ONV(ne_get_status(req)->klass != 2, ("%s", failmsg));

    ne_request_destroy(req);
    return OK;
}

/* Regression test for mod_dav 1.0.2, another found by Julian Reschke, 
 * see FAQ for details. */
static int propnullns(void)
{
    PRECOND(prop_ok);

    numprops = 1;
    removedprops = 0;
    
    propnames[0].nspace = NULL;
    propnames[0].name = "nonamespace";
    pops[0].value = values[0] = "randomvalue";
    pops[0].type = ne_propset;

    pops[1].name = NULL;
    propnames[1].name = NULL;

    CALL(do_patch("PROPPATCH of property with null namespace (see FAQ)",
                  XML_DECL
		  "<propertyupdate xmlns=\"DAV:\"><set><prop>"
		  "<nonamespace xmlns=\"\">randomvalue</nonamespace>" 
		  "</prop></set></propertyupdate>"));

    return OK;
}

/* Test ability to parse and persist Unicode characters above UXFFFF. */
static int prophighunicode(void)
{
    PRECOND(prop_ok);

    numprops = 1;
    removedprops = 0;

    propnames[0].nspace = NS;
    propnames[0].name = "high-unicode";
    pops[0].value = values[0] = "\xf0\x90\x80\x80";
    pops[0].type = ne_propset;

    pops[1].name = NULL;
    propnames[1].name = NULL;

    CALL(do_patch("PROPPATCH of property with high unicode value",
		  XML_DECL "<propertyupdate xmlns='DAV:'><set><prop>"
		  "<high-unicode xmlns='" NS "'>&#65536;</high-unicode>"
		  "</prop></set></propertyupdate>"));

    return OK;
}

/* Test whether PROPPATCH is processed in document order (1/2). */
static int propremoveset(void)
{
    PRECOND(prop_ok);

    numprops = 1;
    removedprops = 0;

    propnames[0].nspace = NS;
    propnames[0].name = "removeset";
    values[0] = "y";
 
    CALL(do_patch("PROPPATCH remove then set",
		  XML_DECL "<propertyupdate xmlns='DAV:'>"
      "<remove><prop><removeset xmlns='" NS "'/></prop></remove>"
      "<set><prop><removeset xmlns='" NS "'>x</removeset></prop></set>"
      "<set><prop><removeset xmlns='" NS "'>y</removeset></prop></set>"
      "</propertyupdate>"));

    return OK;
}

/* Test whether PROPPATCH is processed in document order (2/2). */
static int propsetremove(void)
{
    PRECOND(prop_ok);

    numprops = 1;
    removedprops = 0;

    propnames[0].nspace = NS;
    propnames[0].name = "removeset";
    values[0] = NULL;
 
    CALL(do_patch("PROPPATCH remove then set",
		  XML_DECL "<propertyupdate xmlns='DAV:'>"
      "<set><prop><removeset xmlns='" NS "'>x</removeset></prop></set>"
      "<remove><prop><removeset xmlns='" NS "'/></prop></remove>"
      "</propertyupdate>"));

    return OK;
}

/* regression test for Apache bug #15728. */
static int propvalnspace(void)
{
    PRECOND(prop_ok);

    numprops = 1;
    removedprops = 0;

    propnames[0].nspace = NS;
    propnames[0].name = "valnspace";
    pops[0].value = values[0] = "<foo></foo>";
    pops[0].type = ne_propset;

    pops[1].name = NULL;
    propnames[1].name = NULL;

    CALL(do_patch("PROPPATCH of property with value defining namespace",
                  XML_DECL "<propertyupdate xmlns='DAV:'><set><prop>"
                  "<t:valnspace xmlns:t='" NS "'><foo xmlns='http://bar'/></t:valnspace>"
                  "</prop></set></propertyupdate>"));

    return OK;
}

static int propwformed(void)
{
    char body[500];

    ne_snprintf(body, sizeof body, XML_DECL "<propfind xmlns='DAV:'><prop>"
                "<%s xmlns='%s'/></prop></propfind>",
                propnames[0].name, propnames[0].nspace);

    return propfind_returns_wellformed("for property in namespace", body);
}

static int propextended(void)
{
    return propfind_returns_wellformed("with extended <propfind> element",
                                       XML_DECL "<propfind xmlns=\"DAV:\"><foobar/>"
                                       "<allprop/></propfind>");
}

static const char *manyns[10] = {
    "http://example.com/alpha", "http://example.com/beta", 
    "http://example.com/gamma", "http://example.com/delta", 
    "http://example.com/epsilon", "http://example.com/zeta", 
    "http://example.com/eta", "http://example.com/theta", 
    "http://example.com/iota", "http://example.com/kappa"
};    

static int propmanyns(void)
{
    int n;

    numprops = 10;

    for (n = 0; n < numprops; n++) {
	propnames[n].nspace = manyns[n];
	propnames[n].name = "somename";
	pops[n].name = &propnames[n];
	pops[n].value = values[n] = "manynsvalue";
	pops[n].type = ne_propset;
    }

    ONMREQ("PROPPATCH", prop_uri,
	   ne_proppatch(i_session, prop_uri, pops));
    
    return OK;
}

static int propcleanup(void)
{
    ne_delete(i_session, prop_uri);
    return OK;
}

ne_test tests[] = 
{
    INIT_TESTS,

    T(propfind_invalid), T(propfind_invalid2),
    T(propfind_d0),
    T(propinit),
    T(propset), T(propget),
    T(propextended),

    T(propmove), T(propget),
    T(propdeletes), T(propget),
    T(propreplace), T(propget),
    T(propnullns), T(propget),
    T(prophighunicode), T(propget),
    T(propremoveset), T(propget),
    T(propsetremove), T(propget),
    T(propvalnspace), T(propwformed),
    
    T(propinit),

    T(propmanyns), T(propget),
    T(propcleanup),

    FINISH_TESTS
};
