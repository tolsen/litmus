/* 
   litmus: DAV server test suite
   Modified by Sharad Maloo (maloo@limewire.co.in)
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

#include <ne_request.h>
#include <ne_props.h>
#include <ne_uri.h>

#include "common.h"

#define NS "http://webdav.org/neon/litmus/"

#define NSPACE(x) ((x) ? (x) : "")

static const ne_propname props[] = {
    { "DAV:", "owner" },
    { "DAV:", "current-user-privilege-set" },
    { "DAV:", "supported-privilege-set" },
    { "DAV:", "acl" },
//    { NS, "foo" },
    { NULL }
};

#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

#define XML_DECL "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" 

#define NP (4)

static ne_proppatch_operation pops[NP + 1];
static ne_propname propnames[NP + 1];
static char *values[NP + 1];
static int numprops = NP, removedprops = 5;

static char *src, *res, *collX, *collY;
char *acl_uri,*test_uri;
char *base_uri;
static int acl_ok = 0;

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

// Status to check whether the request was meant to pass or fail. 
// 1 means it should pass.
// 0 means it should fail.
static int do_my_patch(const char *failmsg, const char *body, char *header,char *uri,int status)
{
    ne_request *req = ne_request_create(i_session, header, uri);
    ne_set_request_body_buffer(req, body, strlen(body));
    ONNREQ(failmsg, ne_request_dispatch(req));
    if(status)
    	ONV(ne_get_status(req)->klass != 2, ("%s", failmsg));
    else 
    	ONV(ne_get_status(req)->klass != 4, ("%s", failmsg));
    ne_request_destroy(req);
    return OK;
}

static int report_returns_wellformed(const char *msg, const char *body, char *request)
{
    ne_xml_parser *p = ne_xml_create();
    ne_request *req = ne_request_create(i_session, request, acl_uri);
    ne_add_depth_header(req, NE_DEPTH_ZERO);

    ne_set_request_body_buffer(req, body, strlen(body));

    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, p);
    ONMREQ(request, acl_uri, ne_request_dispatch(req));
    ONV(ne_get_status(req)->code != 207, ("%s", msg));
    ONV(ne_get_status(req)->klass == 4, ("%s", msg));
    ONV(ne_xml_failed(p), ("%s response %s was not well-formed: %s",
			   request,msg, ne_xml_get_error(p)));

    ne_xml_destroy(p);
    ne_request_destroy(req);
    return OK;
}

static int acl_init(void)
{
	char temp[1024];
	base_uri=i_path;
	// Create base collection called acl-test
	base_uri=ne_concat(i_path,"acl-test/",NULL);
	ONNREQ("could not create collection", ne_mkcol(i_session, base_uri));
    	test_uri=ne_concat(i_path,"acl-test/temp/",NULL);
	ONNREQ("could not create collection", ne_mkcol(i_session, test_uri));
    	test_uri=ne_concat(i_path,"acl-test/temp/test",NULL);
	sprintf(temp,"acl-test/resource");
	res=ne_concat(i_path,temp,NULL);
	CALL(upload_foo(temp));
	acl_uri= ne_concat(i_path,temp, NULL);
 	//CALL(do_my_patch2("ACL Specs Not Allowed.","ACL"));		
	acl_ok=1;
        return OK;
}

static int acl_form_query(char *body, char *err_msg)
{
	PRECOND(acl_ok);
	char temp[1024],err[1024];
    	sprintf(temp,XML_DECL "<propfind xmlns='DAV:'><prop>"
			"<%s xmlns='DAV:'/></prop></propfind>",body);
    	sprintf(err,"ACL - Retrieving %s element. %s",body,err_msg);
	return report_returns_wellformed(err,temp,"PROPFIND");
	// FIX IT. Complete XML Parsing. 	
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
	value = ne_propset_value(rset, &props[n]);
	status = ne_propset_status(rset, &props[n]);

	    if (value == NULL) {
		if (status == NULL) {
		    t_warning("Property %d omitted from results with no status",
			      n);
		} else if (status->code != 404) {
		    t_warning("Status for missing property %d was not 404", n);
		}
	    } else { 
		    if (status->klass == 4){
			r->result = FAIL;
			t_context("Property `{%s}%s' returned a %d status",
				  NSPACE(props[n].nspace), props[n].name, status->klass);
		    }
		   else if (status->code != 200)
			t_context("Property %s did not return 200. It returned %d",props[n],status->code);			   
	    }								
    } 
}

static int aclget(void)
{
    struct results r = {0};

    PRECOND(acl_ok);

    r.result = 1;
    t_context("No responses returned");

    ONMREQ("PROPFIND", acl_uri,
	   ne_simple_propfind(i_session, acl_uri, NE_DEPTH_ZERO,
			      props, pg_results, &r));

    if (r.result) {
	return r.result;
    }	

    return OK;
}


static int acl_owner(void)
{
    PRECOND(acl_ok);
    return acl_form_query("owner","Error in DAV:owner");
}

static int acl_set_owner(void)
{
    PRECOND(acl_ok);
    char temp[512];
    
    sprintf(temp,XML_DECL "<propertyupdate xmlns='DAV:'>""<set><prop>"
                "<owner xmlns='DAV:'><href xmlns='DAV:'>/limesport/users/foo</href>"
		"</owner></prop></set></propertyupdate>");
    return report_returns_wellformed("ACL - Propset on owner element",temp,"PROPPATCH");
    // FIX IT. Complete XML Parsing. 	
}

static int acl_privilege_set(void)
{
    PRECOND(acl_ok);
    return acl_form_query("supported-privilege-set","Propfind of DAV:supported-privilege-set");
}

static int acl_user_privilege_set(void)
{
    PRECOND(acl_ok);
    return acl_form_query("current-user-privilege-set ","Propfind of DAV:current-user-privilege-set ");
}

static int acl_set(void)
{
    PRECOND(acl_ok);
    return acl_form_query("acl","Finding a resource's ACL");
}

static int acl_restrictions(void)
{
    PRECOND(acl_ok);
    return acl_form_query("acl-restrictions","Error in DAV:acl-restrictions");
}

static int acl_inherited_acl_set(void)
{
    PRECOND(acl_ok);
    return acl_form_query("inherited-acl-set","Error in DAV:inherited-acl-set");
}

static int acl_pcpl_collectn_set(void)
{
    PRECOND(acl_ok);
    return acl_form_query("principal-collection-set","Error in DAV:principal-collection-set");
}

static int acl_pcpl_prop_report(void)
{
    PRECOND(acl_ok);
    char temp[512];
    sprintf(temp, XML_DECL "<D:acl-principal-prop-set xmlns:D='DAV:'>"
		    	"<D:prop><D:creationdate/>"
			"<D:displayname/>"
			"</D:prop></D:acl-principal-prop-set>");
    return report_returns_wellformed("Error in REPORT acl-principal-prop-set",temp,"REPORT"); 
}

static int acl_pcpl_match_report(void)
{
    PRECOND(acl_ok);
    char temp[512];
    sprintf(temp, XML_DECL "<D:principal-match xmlns:D='DAV:'>"
		    	"<D:principal-property><D:owner/>"
			"</D:principal-property></D:principal-match>");
    return report_returns_wellformed("Error in REPORT acl-principal-prop-set",temp,"REPORT"); 
}

static int acl_set_privileges(void)
{
    PRECOND(acl_ok);
    char temp[1024];
    sprintf(temp, XML_DECL "<D:acl xmlns:D='DAV:'><D:ace>" 
	      	   "<D:principal>"
		   //"<D:property><D:owner/></D:property>"
		   "<D:href>/users/limestone</D:href>"
		   "</D:principal>"
		   "<D:grant>"
		   "<D:privilege><D:write/></D:privilege>"
		   "</D:grant>"
		   "</D:ace>"
		   "</D:acl>");

		  /* " <D:ace> "
		   " <D:principal> "
		   " <D:property><D:owner/></D:property>  "
		   " </D:principal> "
		   " <D:grant> "
		   " <D:privilege><D:write-acl/></D:privilege>  "
		   " </D:grant> "
		   " </D:ace> "
		   " <D:ace> "
		   " <D:principal><D:all/></D:principal> "
		   " <D:grant> "
		   " <D:privilege><D:read/></D:privilege> "
		   " </D:grant> "
		   " </D:ace>"
		  "  </D:acl>");*/ 
	return do_my_patch("ACL - Trying to set ACEs",temp,"ACL",acl_uri,1);
}

// Trying to set grant and deny in a single ACE request. Should fail. 
static int acl_privileges_f(void)
{
    PRECOND(acl_ok);
    char temp[1024];
    sprintf(temp, XML_DECL "<D:acl xmlns:D='DAV:'>"
		   "<D:ace>" 
	      	   "<D:principal>"
		   "<D:href>/users/test1</D:href>"
		   "</D:principal>"
		   "<D:grant>"
		   "<D:privilege><D:all/></D:privilege>"
		   "</D:grant>"
	      	   "<D:principal>"
		   "<D:href>/users/test1</D:href>"
		   "</D:principal>"
		   "<D:deny>"
		   "<D:privilege><D:write/></D:privilege>"
		   "</D:deny>"
		   "</D:ace>"
		   "</D:acl>"); 
    return do_my_patch("ACL - Trying to set ACEs",temp,"ACL",acl_uri,0);
}
		   
static int acl_privileges_test(void)
{
    PRECOND(acl_ok);
    char temp[1024];
    sprintf(temp, XML_DECL "<D:acl xmlns:D='DAV:'>"
		   "<D:ace>" 
	      	   "<D:principal>"
		   "<D:href>/users/test1</D:href>"
		   "</D:principal>"
		   "<D:grant>"
		   "<D:privilege><D:all/></D:privilege>"
		   "</D:grant>"
		   "</D:ace>"
		   "<D:ace>" 
	      	   "<D:principal>"
		   "<D:href>/users/test1</D:href>"
		   "</D:principal>"
		   "<D:deny>"
		   "<D:privilege><D:write/></D:privilege>"
		   "</D:deny>"
		   "</D:ace>"
		   "</D:acl>");
    return do_my_patch("ACL - Trying to set ACEs",temp,"ACL",test_uri,1);
}

// FIX IT - Write tests for moving contents when ACL restrictions apply. 

static int acl_copy_simple(void)
{
    PRECOND(acl_ok);

    /* Now copy it once */
    //ONNREQ("Resource COPY that should fail. We dont have perms", 
	   ne_copy(i_session, 1, NE_DEPTH_INFINITE, acl_uri, test_uri);
    //);

    if (STATUS(201)) {
	t_warning("Resource COPY that should have failed. We dont have perms");
    }

    return OK;
}

static int aclcleanup(void)
{
    ne_delete(i_session, base_uri);
    return OK;
}

ne_test tests[] = 
{
    INIT_TESTS,
    T(acl_init),
    T(acl_owner),
    T(acl_set_owner),
    T(acl_owner),
    T(acl_copy_simple),
    T(acl_privilege_set),
    T(acl_user_privilege_set),
    T(acl_set),
    T(acl_restrictions),
    T(acl_inherited_acl_set),
    T(acl_pcpl_collectn_set),
    T(acl_pcpl_prop_report),
    T(acl_pcpl_match_report),
    T(aclget),	
    T(acl_set_privileges),
    T(acl_privileges_test),
    T(acl_privileges_f),
    T(aclcleanup),
    FINISH_TESTS
};
