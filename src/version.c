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
    { "DAV:", "version-history" },
    { "DAV:", "version-name" },
    { "DAV:", "creator-displayname" },
    { "DAV:", "successor-set" },
//    { NS, "foo" },
    { NULL }
};

static const ne_propname prop_history[] = {
    { "DAV:", "version-history" },
    { NULL }
};


#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

#define XML_DECL "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" 

#define NP (5)

//static ne_proppatch_operation pops[NP + 1];
static ne_propname propnames[NP + 1];
//static char *values[NP + 1];
static int numprops = NP;
//static int removedprops = 5;


static char *src, *res; 
//static char *collX, *collY;
char *version_uri,*version_c_uri;  // URI for versioned-resource and versioned-collection.
char *base_uri;
static int version_ok = 0;

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

static int do_my_patch(const char *failmsg, char *header,char *uri)
{
    ne_request *req = ne_request_create(i_session, header, uri);
    
    ONNREQ(failmsg, ne_request_dispatch(req));
    
    switch(header[0])
    {
	    case 'V':
		    {
			if (STATUS(200))
			{
				t_warning("Version-Control did not return 200");
			}
		    	break;
		    }
	    case 'U':
		    {
			if (STATUS(200))
			{
				t_warning("Uncheckout did not return 200");
			}
		    	break;
		    }
	    case 'C':
		    switch(header[5])
		    {
		    	case 'O':
				{
					if(STATUS(200))
					{
						t_warning("CHECKOUT did not return 200");
					}
				   	break;
			    	}
			case 'I':
				{
				       if(STATUS(201))
				       {
						t_warning("CHECKIN did not return 201");
				       }
				       break;
				}
		    }
    }
    
    ONV(ne_get_status(req)->klass != 2, ("%s", failmsg));
    ne_request_destroy(req);
    return OK;
}


static int do_my_patch_fail(const char *failmsg, const char *body, char *header)
{
    ne_request *req = ne_request_create(i_session, header, version_uri);

    //ne_set_request_body_buffer(req, body, strlen(body));
    ONNREQ(failmsg, ne_request_dispatch(req));
    ONV(ne_get_status(req)->klass != 4, ("%s", failmsg));
    ne_request_destroy(req);
    return OK;
}

static int report_returns_wellformed(const char *msg, char *hdr, const char *body,char *uri)
{
    ne_xml_parser *p = ne_xml_create();
    ne_request *req = ne_request_create(i_session, hdr, uri);

    ne_set_request_body_buffer(req, body, strlen(body));

    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, p);
    ONMREQ(hdr, uri, ne_request_dispatch(req));
    ONV(ne_get_status(req)->code != 207, ("%s Error",msg));
    
    ONV(ne_xml_failed(p), ("%s response %s was not well-formed: %s",
                           hdr,msg, ne_xml_get_error(p)));

    ne_xml_destroy(p);
    ne_request_destroy(req);
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
			t_context("Property %s did not return 200. It returned %d",value,status->code);			   
	    } 
    }
}


static int version_init(void)
{
	char temp[512];
	int n=1;
	base_uri=i_path;
	res=ne_concat(i_path,"resource",NULL);
        
	// Create base collection called version-test
	base_uri=ne_concat(i_path,"version-test/",NULL);
    	ONNREQ("could not create collection", ne_mkcol(i_session, base_uri));
	
	sprintf(temp,"version-test/resource%d",n);
	CALL(upload_foo(temp));
	version_uri= ne_concat(i_path,temp, NULL); // URI for version controlled resource. 
 	CALL(do_my_patch("VERSIONING Specs Not Allowed.","VERSION-CONTROL",version_uri));		
	version_ok=1;
        return OK;
}

static int version_collection(void)
{
	PRECOND(version_ok);
	char temp[1024];
	sprintf(temp,"version-test/");
	version_c_uri=ne_concat(i_path,temp,NULL); // URI for version controlled collection. 
 	CALL(do_my_patch("VERSION Controlled Collections Not Allowed.","VERSION-CONTROL",version_c_uri));		
	sprintf(temp,"version-test/resource2");
	CALL(upload_foo(temp)); // Creating a non-versioned resource in a VCC. 
 	//CALL(do_my_patch("Creation of a non-versioned resource in a VCC.","VERSION-CONTROL",version_uri));		
	return OK;
}


static int version_checkin(void)
{
    PRECOND(version_ok);
    CALL(do_my_patch("Checkin of versioned-resource not allowed.","CHECKIN",version_uri));		
    return OK;
}

// A version checkin request that should fail.
static int version_checkin_f(void)
{
    PRECOND(version_ok);
    CALL(do_my_patch_fail("Checkin of resource allowed, when it should not have been",NULL,"CHECKIN"));		
    return OK;
}

static int version_checkout(void)
{
    PRECOND(version_ok);
    CALL(do_my_patch("Checkout of versioned-resource not allowed.","CHECKOUT",version_uri));		
    return OK;
}

// FIX IT. Duplication of code. Fix it with one single function. 
static int version_c_checkout(void)
{
    PRECOND(version_ok);
    CALL(do_my_patch("Checkout of versioned-collection not allowed.","CHECKOUT",version_c_uri));		
    return OK;
}

static int version_c_checkin(void)
{
    PRECOND(version_ok);
    CALL(do_my_patch("Checkin of versioned-collection not allowed.","CHECKIN",version_c_uri));		
    return OK;
}

// A version checkout request that should fail.
static int version_checkout_f(void)
{
    PRECOND(version_ok);
    CALL(do_my_patch_fail("Checkout of versioned-resource allowed, when it should not have been",NULL,"CHECKOUT"));		
    return OK;
}

static int version_uncheckout(void)
{
    PRECOND(version_ok);
    CALL(do_my_patch("Uncheckout of versioned-resource not allowed.","UNCHECKOUT",version_uri));		
    return OK;
}

static int version_report(char *uri)
{
    struct results r = {0};
    PRECOND(version_ok);
    char temp[512];
    r.result = 1;
    t_context("No responses returned");
    sprintf(temp,XML_DECL "<version-tree xmlns='DAV:'>"
		 "<prop>"
		 "<version-name/>"
		 "<creator-displayname/>"
		 "<successor-set/>"
		 "</prop>"
		 "</version-tree>");
    return report_returns_wellformed("Version-tree REPORT element","REPORT",temp,version_c_uri);
    
    // FIX IT. Extensive testing for versioning. Returning (no response read)
    // Parse XML response and check individual elements.   
    //ONMREQ("REPORT", version_uri,
    //		ne_simple_report(i_session, version_uri, NE_DEPTH_ZERO,
    //		props, pg_results, &r));
    //if (r.result) {
    //	return r.result;
    //  }	
    //return OK;
}

// Fix IT. Add test for locate version by history. 
// http://webdav.org/specs/rfc3253.html#rfc.section.5.4.1

static int version_report_detail(void)
{
    PRECOND(version_ok);
    char temp[512];
    sprintf(temp,XML_DECL "<expand-property xmlns='DAV:'>"
		 "<property name='version-history'>"
		 	"<property name='version-set'>"
				 "<property name='creator-displayname'/>"
				 "<property name='activity-set'/>"
		 	"</property>"
		 "</property>"
		 "</expand-property>");
    return report_returns_wellformed("Detailed Report - Version-tree REPORT element","REPORT",temp,version_c_uri);
    //CALL(do_my_patch("Expand Report of versioned resource failed",temp,"REPORT"));
    //return OK;
}

// Complete test to get version history and delete it. 
// Test it by deleting the VCC also. 

static int version_history_resource(void)
{
    /*
    PRECOND(version_ok);
    char temp[512];
    sprintf(temp,XML_DECL "<D:propfind xmlns:D='DAV:'><D:prop>"
		 "<D:version-history/>"
	 	 "</D:prop>"
		"</D:propfind>");
    return report_returns_wellformed("Report - To find version-history resource","PROPFIND",temp,version_c_uri);
    */
    
    struct results r = {0};

    PRECOND(version_ok);

    r.result = 1;
    t_context("No responses returned");

    ONMREQ("PROPFIND", version_c_uri,
	   ne_simple_propfind(i_session, version_c_uri, NE_DEPTH_ZERO,
			      prop_history, pg_results, &r));
		
    if (r.result) {
	return r.result;
    }	

    return OK;


}

static int version_c_bind_set(void)
{
    PRECOND(version_ok);
    char temp[512];
    sprintf(temp,XML_DECL "<version-controlled-binding-set xmlns='DAV:'>"
		 "</version-controlled-binding-set>");
    return report_returns_wellformed("Detailed Report - VCC element","REPORT",temp,version_c_uri);
}

// Version supported property set. 
static int v_spptd_prpty_set(void)
{
    PRECOND(version_ok);
    char temp[512];
    sprintf(temp,XML_DECL "<propfind xmlns='DAV:'><prop>"
		    "<supported-live-property-set xmlns='DAV:'/>"
		     "</prop></propfind>");
    return report_returns_wellformed("Supported Property Set","PROPFIND",temp,version_c_uri);
}

static int version_auto(char *method)
{
    PRECOND(version_ok);
    char temp[512];
    sprintf(temp,XML_DECL 
		   "<D:propertyupdate xmlns:D='DAV:'><D:set><D:prop>"
		   "<D:auto-version><D:%s/></D:auto-version>"
		   "</D:prop></D:set></D:propertyupdate>",method);
    return report_returns_wellformed("Proppatch auto-version","PROPPATCH",temp,version_uri);
}

static int version_auto_tests(void)
{
    PRECOND(version_ok);
    int ret;
    
    ret=version_checkout();
    ret=version_auto("checkout-checkin");
    ret=version_checkin();
    
    return ret; 
}

static int version_c_report(void)
{
    PRECOND(version_ok);
    return version_report(version_uri);
    //return version_report(version_c_uri); // This test currently results in seg-fault. FIX IT. 
}

// Copy of versioned collection allowed.
static int copy_simple(void)
{
    PRECOND(version_ok);
    //char *dst=ne_concat(base_uri,"copy",NULL);
    char *dst=ne_concat(base_uri,"copy/",NULL);    // Resulted in infinite loop. FIX IT (modification to copy method reqd.)
    
    ONNREQ("simple resource COPY", 
	   ne_copy(i_session, 0, NE_DEPTH_INFINITE, version_c_uri,dst ));

    if (STATUS(201)) {
	t_warning("COPY of versioned collection did not give 201");
    }
    return OK;
}

static int version_delete(void)
{
	ne_delete(i_session,version_c_uri);
	return OK;
}

static int versioncleanup(void)
{
    ne_delete(i_session, base_uri);
    return OK;
}

ne_test tests[] = 
{
    INIT_TESTS,
    T(version_init),
    T(version_checkin_f),
    T(version_checkout),	
    T(version_checkin),
    T(version_checkout),	
    T(version_checkout_f),	
    T(version_uncheckout),	
    T(version_collection),
    T(version_c_checkout),	
    T(version_c_checkin),
    T(version_c_report),
    T(version_report_detail),
    T(copy_simple),
    //T(version_c_bind_set),
    //T(version_history_resource),
    T(v_spptd_prpty_set),
    T(version_c_checkout),	
    T(version_auto_tests),
    T(version_delete),
    T(versioncleanup),
    FINISH_TESTS
};
