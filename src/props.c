/* 
   litmus: DAV server test suite
   Copyright (C) 2001-2004, Joe Orton <joe@manyfish.co.uk>
                                                                     
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
#include <unistd.h>

#include <ne_request.h>
#include <ne_props.h>
#include <ne_uri.h>

#include "common.h"

#define NS "http://webdav.org/neon/litmus/"

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

#define NLP (10)
static int numliveprops=NLP;
static const ne_propname live_props[NLP+1] = {
	{ "DAV:", "creationdate"},
	{ "DAV:", "displayname"},
	{ "DAV:", "getcontentlanguage"},
	{ "DAV:", "getcontentlength"},
	{ "DAV:", "getcontenttype"},
	{ "DAV:", "getetag"},
	{ "DAV:", "getlastmodified"},
	{ "DAV:", "lockdiscovery"},
	{ "DAV:", "resourcetype"},
	{ "DAV:", "supportedlock"},
	{ NULL }
};

static void d0_results(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
{
    struct results *r = userdata;
    const char *scheme;
    size_t slen;

    scheme = ne_get_scheme(ne_get_session(ne_propfind_get_request(r->ph)));
    slen = strlen(scheme);

    if (strncmp(uri, scheme, slen) == 0 &&
        strncmp(uri+slen, "://", 3) == 0) {
	/* Absolute URI */
	uri = strchr(uri+slen+3, '/');
	if (uri == NULL) {
	    NE_DEBUG(NE_DBG_HTTP, "Invalid URI???");
	    return;
	}
    }

    if (ne_path_compare(uri, i_path)) {
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

static void *create_private(void *userdata, const char *uri)
{
    return ne_calloc(sizeof(struct private));
}

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

    r.ph = ne_propfind_create(i_session, i_path, NE_DEPTH_ZERO,"PROPFIND");
    
    ne_propfind_set_private(r.ph, create_private, NULL);

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
    //if you decide to add another res here make sure to change the d1_results function accordingly
    prop_uri = ne_concat(i_path, "prop", NULL);

    ne_delete(i_session, prop_uri);

    CALL(upload_foo("prop"));

    prop_ok = 1;
    
    return OK;
}

/*just to check whether all the members are returned when doing
Depth:1 on root. After adding one member that is.*/
//TODO: implement this in a better way.
static void d1_results(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
{
	struct results *r = userdata;
	if (!ne_path_compare(uri, prop_uri)) {
		r->result=0; //only if it finds propuri	will this be set to succeed.
	}



}

static int propfind_d1(void)
{
    struct results r = {0};

    r.result = 1; 
    t_context("PROPFIND did not return the newly added resource %s", prop_uri);

    ONMREQ("PROPFIND", prop_uri,
	   ne_simple_propfind(i_session, i_path, NE_DEPTH_ONE,
			      NULL, d1_results, &r));
    if (r.result) {
	return r.result;
    }

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

static void pg_results(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
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

char *creationdate=NULL;
static void pg_results_gcd(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
{
	creationdate = ne_strdup(ne_propset_value(rset, &live_props[0]));
	const ne_status *status = ne_propset_status(rset, &live_props[0]);
	if(status->code != 200)
		t_warning("Property %s returns %d and not 200",live_props[0].name,status->code);
	if(creationdate==NULL)
		t_warning("No value for Property %s ",live_props[0].name);

}

static int propget_creationdate(const char *uri)
{
	//const char *body="<D:prop><D:creationdate/></D:prop>"
	struct results r = {0};
	ONMREQ("PROPFIND", uri,
		ne_simple_propfind(i_session,uri,NE_DEPTH_ZERO,live_props ,pg_results_gcd, &r));
	return OK;
}

static int propmove(void)
{
    char *dest;
    char *src_creationdate;
    PRECOND(prop_ok);

    dest = ne_concat(i_path, "prop2", NULL);
    
    ne_delete(i_session, dest);
    
    //get the creation date of the source.
    propget_creationdate(prop_uri);
    if(creationdate !=NULL)
	src_creationdate = ne_strdup(creationdate);

    ONM2REQ("MOVE", prop_uri, dest,
	    ne_move(i_session, 0, prop_uri, dest));

   propget_creationdate(dest);
   if(strcmp(creationdate, src_creationdate)!=0)
	t_warning("Move from %s to %s should not have changed the creation date",prop_uri,dest);

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



/*Test whether all dead props are copied on a dest res that already
exists by the same bind name*/
static int propcopy(void)
{
    char *dest, *coll;
    PRECOND(prop_ok);

    //using the same name as in propinit, to enable to copy on same bind name.
    dest = ne_concat(i_path, "copycoll/prop", NULL);
    coll = ne_concat(i_path, "copycoll/", NULL);
    //delete if it was already there. fresh start.
    ne_delete(i_session, dest);
   //create a collection
    ONNREQ("could not create collection", ne_mkcol(i_session, coll));

    //upload the file on the detination
    CALL(upload_foo("copycoll/prop"));

    ONM2REQ("dead properties copy on COPY of a resource", prop_uri, dest,
	    ne_copy(i_session, 1, NE_DEPTH_INFINITE, prop_uri, dest));

    ne_delete(i_session, dest);
    ne_delete(i_session, coll);

    return OK;
}

/*Test whether all dead props are copied on an unmapped url.*/
static int propcopy_unmapped(void)
{
    char *dest;
    char *src_creationdate=NULL;
    PRECOND(prop_ok);

    dest = ne_concat(i_path, "copydest", NULL);

    //delete if it was already there. fresh start.
    ne_delete(i_session, dest);
    //get the creation date of the source.
    propget_creationdate(prop_uri);
    if(creationdate !=NULL)
	src_creationdate=ne_strdup(creationdate);

   //let's  sleep zzzzzzzzz
   sleep(2);

    ONM2REQ("dead properties copy on COPY", prop_uri, dest,
	    ne_copy(i_session, 0, NE_DEPTH_INFINITE, prop_uri, dest));


   //check for DAV:creationdate since it was an unmapped url
   propget_creationdate(dest);
   if(strcmp(creationdate, src_creationdate)==0)
	t_warning("Copy from %s to unmapped url %s should have changed the creation date %s %s",prop_uri,dest);

    ne_delete(i_session, dest);


    return OK;
}

/* Test whether the response to a PROPFIND request with given body is
 * well-formed XML. */
static int propfind_returns_wellformed(const char *msg, const char *body, char *request)
{
    ne_xml_parser *p = ne_xml_create();
    ne_request *req = ne_request_create(i_session, request, prop_uri);

    ne_set_request_body_buffer(req, body, strlen(body));

    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, p);
    ONMREQ(request, prop_uri, ne_request_dispatch(req));
    
    ONV(ne_get_status(req)->code != 207, ("%s", msg));
    ONV(ne_get_status(req)->klass == 4, ("%s", msg));
    ONV(ne_xml_failed(p), ("%s response %s was not well-formed: %s",
                           request, msg, ne_xml_get_error(p)));

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

    //setting content type as xml..don't ask.
    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
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



static int numlpinc=2;
static int incliveprops=0;
static const ne_propname live_props_inc[2] = {
	{ "DAV:", "acl"},
	{ "DAV:", "resource-id"}
};
/*
internal function to compare returned and stored propnames
*/
static void propcmp(void *userdata,const ne_propname *pname,
			  const ne_prop_result_set *rset)
{
	const char *value = ne_propset_value(rset, pname);
	if (value == NULL)
	   t_warning("Server did not return the property: %s", pname->name);
	
}

/* chking for response from allprop
*all live and dead properties should be returned
*/
static void pg_results_allprop(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
{
	int i;
	for(i=0;i<numprops;i++)
	{
		propcmp(userdata, &propnames[i], rset);
	}
	for(i=0;i<numliveprops;i++)
	{
		propcmp(userdata, &live_props[i], rset);
	}
	if(incliveprops)
	{
		for(i=0;i<numlpinc;i++)
		{
			propcmp(userdata, &live_props_inc[i], rset);
		}
	}
}
/*propfind with an empty body should be treated as allprop request.*/
static int propfind_empty(void)
{
	struct results r = {0};

	r.ph = ne_propfind_create(i_session,prop_uri, NE_DEPTH_ZERO,"PROPFIND");
	ONMREQ("PROPFIND", prop_uri,
		ne_propfind(r.ph,NULL,pg_results_allprop, &r, ne_propfind_method));
	ne_propfind_destroy(r.ph);
        return OK;
}

static int propfind_allprop_include(void)
{
    char body[512];
    //if you change this change in live_props_inc as well.
    sprintf(body,  "<D:allprop />"
                   "<D:include>"
                   "<D:acl />"
                   "<D:resource-id />"
                   "</D:include>");

	struct results r = {0};
	incliveprops=1;
	r.ph = ne_propfind_create(i_session,prop_uri, NE_DEPTH_ZERO,"PROPFIND");
	ONMREQ("PROPFIND", prop_uri,
		ne_propfind(r.ph,body,pg_results_allprop, &r, ne_propfind_method));
	ne_propfind_destroy(r.ph);
	incliveprops=0;
        return OK;
}

static int propfind_propname(void)
{
	struct results r = {0};
	//I don't know if it should return all the property names or only defined by the spec,
	//I am presuming it should be like allprop for names.
	incliveprops=1;
	r.ph = ne_propfind_create(i_session,prop_uri, NE_DEPTH_ZERO,"PROPFIND");
	ONMREQ("PROPFIND", prop_uri,
		ne_propfind(r.ph,"<D:propname/>",pg_results_allprop, &r, ne_propfind_method));
incliveprops=0;
	ne_propfind_destroy(r.ph);
	return OK;
}


/* PROPPATCH of property with mixed content */
static int property_mixed(void)
{
    PRECOND(prop_ok);
    char body[1024];
    
    sprintf(body,XML_DECL "<D:propertyupdate xmlns:D='DAV:'><D:set><D:prop xml:lang=\"en\">"
                "<x:author xmlns:x='http://example.com/ns'>"
                "<x:name>Sharad</x:name>"
                "<x:uri type='email'  "
                "added='2007-01-26'>mailto:sharad@example.com</x:uri>"
                "<x:uri type='web' " 
                " added='2005-11-27'>http://www.example.com</x:uri>"
                "<x:notes xmlns:h='http://www.w3.org/1999/xhtml'>"
                "Sharad has been working way <h:em>too</h:em> long on the"  
                "long-awaited revision of...."
                " </x:notes>"
                "</x:author>"
		 "</D:prop></D:set></D:propertyupdate>");
	CALL(do_patch("PROPPATCH of dead property with mixed contents",body));
	return OK;
    //return propfind_returns_wellformed("Dead Property Update - XML test",body,"PROPPATCH");
    //no need to do xml parsing here as we just need to check valid xml which do_patch will do anyway.	
}

#define ELM_author (NE_PROPS_STATE_TOP + 1)
#define ELM_name (NE_PROPS_STATE_TOP + 2)
#define ELM_uri (NE_PROPS_STATE_TOP + 3)
#define ELM_notes (NE_PROPS_STATE_TOP + 4)
#define ELM_em (NE_PROPS_STATE_TOP + 5)

//id map of xml defined for dead property author
static const struct ne_xml_idmap map_author[] = {
    { "http://example.com/ns", "author", ELM_author },
    { "http://example.com/ns", "name", ELM_name },
    { "http://example.com/ns", "uri", ELM_uri },
    { "http://example.com/ns", "notes", ELM_notes },
    { "http://www.w3.org/1999/xhtml", "em", ELM_em }
};

/*Callback to handle start element of dead property author*/
static int start_elm_author(void *userdata, int parent, const char *nspace,
                    const char *name, const char **atts)
{
	struct results *r = userdata;
	int state = ne_xml_mapid(map_author, NE_XML_MAPLEN(map_author), nspace, name);

	//I am ignoring xml:lang ns because it is too hard to handle.
	//TODO: chk if the the prefix is same, if not raise a warning.
	//find a better way to handle complex xml property, this is a very bad way.
	if (parent == NE_207_STATE_PROP && state == ELM_author)
	{
		//nothing to chk if adding any att add the line as below, do not include <>
		//att = ne_xml_get_attr(r->ph->parser,atts,"http://example.com/ns","<attsname>");
		return ELM_author;
	}
	else if (parent == ELM_author && state == ELM_name)
        {
		//no attr to chk
		return ELM_name;
	}
	else if (parent == ELM_author && state == ELM_uri)
        {
		//check att type and added. no need to chk anything else.
		if(ne_xml_get_attr(ne_propfind_get_parser(r->ph),atts,NULL,"type") != NULL &&
		   ne_xml_get_attr(ne_propfind_get_parser(r->ph),atts,NULL,"added") != NULL)
		return ELM_uri;
		else
		{
			r->result=FAIL;
			t_context("did not preserve attributes in xml element");
			return NE_XML_ABORT;
		}
	}
	else if (parent == ELM_author && state == ELM_notes)
        {
		//TODO: check for mixed content and further tags
		return ELM_notes;
	}
	else if (parent == ELM_notes && state == ELM_em)
        {
		//check for ns
		//TODO: any kind of mix data is not parsed for xml elem of a property????
		return ELM_em;
	}
	else
	{
		r->result = FAIL;
		return NE_XML_ABORT;
	}

	return NE_XML_DECLINE;
}

/*Callback to handle char data of dead property author*/
//static int chardata_elm(void *userdata,int state, const ne_xml_char *data, int len)
//{
//	//FIX IT, to chk for the value, ns of the xml elements.
//return 0;
//}

static int propfind_mixed()
{	
const ne_propname prop[] = {
    { "http://example.com/ns", "author" },
    { NULL }
};
    int ret;
    struct results r = {0};

    r.ph = ne_propfind_create(i_session, prop_uri, NE_DEPTH_ZERO,"PROPFIND");

    r.result = OK;
    t_context("property value not as expected");

    ne_xml_push_handler(ne_propfind_get_parser(r.ph), start_elm_author,
                        NULL, NULL, r.ph);

    ret = ne_propfind_named(r.ph, prop, NULL, &r);

       return r.result;
}

static int numlp_unprotect=2;
//this array has dependency on two functions
//pg_results_liveunprop and pgresults_invalid_sem
static const ne_propname live_prop_unprotect[2] = {
	{ "DAV:", "getcontentlanguage"},
	{ "DAV:", "displayname"}
};

static void pgresults_invalid_sem(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
{
	const ne_status *status;

	if (STATUS(207)) //not 207
		t_warning("PROPPATCH of property got %d response, not 207",
		  	 GETSTATUS);
    	else //207
	{
		//I am expecting a conflict here as the values passed are not correct according to 
		//[RFC2616]
		status = ne_propset_status(rset, &live_prop_unprotect[0]);
		if (status->code != 409)
		t_warning("The status for setting property %s was %d and not 409 as expected",
			live_prop_unprotect[0].name, status->code);
	}
}

static int proppatch_invalid_semantics(void)
{
	//if you make changes here change the live_prop_unprotect as well
    char * body =   "<D:set><D:prop>"
        	"<D:getcontentlanguage>sdkjhfkjsdh</D:getcontentlanguage>"
		"</D:prop></D:set>";

    struct results r = {0};
    r.ph = ne_propfind_create(i_session,prop_uri, NE_DEPTH_ZERO,"PROPPATCH");

   ONMREQ("PROPPATCH", prop_uri,
	ne_propfind(r.ph,body,pgresults_invalid_sem, &r,ne_proppatch_method));

   ne_propfind_destroy(r.ph);

    return OK;
}

static void pg_results_liveunprop(void *userdata, const char *uri,
		       const ne_prop_result_set *rset)
{
	int i;
	const ne_status *status;

	if (STATUS(207)) 
		t_warning("PROPPATCH of live unprotected property got %d response, not 207", GETSTATUS);
    	else //207
	{
		for (i=0;i<numlp_unprotect;i++)
		{
			status = ne_propset_status(rset, &live_prop_unprotect[i]);
			if (status->code != 200)
			t_warning("The status for setting property %s was %d and not 200 as expected",
				live_prop_unprotect[i].name, status->code);
		}
	}
}

static int proppatch_liveunprotect(void)
{
//if you make changes here change the live_prop_unprotect as well
    char * body = "<D:set><D:prop>"
        	  "<D:getcontentlanguage>en-US</D:getcontentlanguage>"
		  "<D:displayname>propchanged</D:displayname>"
 		  "</D:prop></D:set>";
    struct results r = {0};
    r.ph = ne_propfind_create(i_session,prop_uri, NE_DEPTH_ZERO,"PROPPATCH");

   ONMREQ("PROPPATCH", prop_uri,
	ne_propfind(r.ph,body,pg_results_liveunprop, &r,ne_proppatch_method));

   ne_propfind_destroy(r.ph);

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
                  "<t:valnspace xmlns:t='" NS "'><foo xmlns='bar'/></t:valnspace>"
                  "</prop></set></propertyupdate>"));

    return OK;
}

static int propwformed(void)
{
    char body[500];

    ne_snprintf(body, sizeof body, XML_DECL "<propfind xmlns='DAV:'><prop>"
                "<%s xmlns='%s'/></prop></propfind>",
                propnames[0].name, propnames[0].nspace);

    return propfind_returns_wellformed("for property in namespace", body,"PROPFIND");
}

static int propextended(void)
{
    return propfind_returns_wellformed("with extended <propfind> element",
                                       XML_DECL "<propfind xmlns=\"DAV:\"><foobar/>"
                                       "<allprop/></propfind>","PROPFIND");
}

static const char *manyns[10] = {
    "alpha", "beta", "gamma", "delta", "epsilon", 
    "zeta", "eta", "theta", "iota", "kappa"
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
    T(propfind_d1),
    T(proppatch_invalid_semantics),
    T(propset), T(propget),
    T(propfind_empty),
    T(propfind_allprop_include),
    T(propfind_propname),
    T(proppatch_liveunprotect),
    T(propextended),

    T(propcopy), T(propget),
    T(propcopy_unmapped), T(propget),
    T(propmove), T(propget),
    T(propdeletes), T(propget),
    T(propreplace), T(propget),
    T(propnullns), T(propget),
    T(prophighunicode), T(propget),
    T(propvalnspace), T(propwformed),

    T(propinit),

    T(propmanyns), T(propget),
    T(property_mixed),
    T(propfind_mixed),
    T(propcleanup),

    FINISH_TESTS
};
