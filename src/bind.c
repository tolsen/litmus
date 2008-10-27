/* 
   litmus: DAV server test suite
   Copyright (C) 2001-2004, Joe Orton <joe@manyfish.co.uk>

   BIND functionality testing.
   Copyright (C) 2001-2006, Sharad Maloo <maloo@rootshell.be>   
   
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

#include <string.h>
#include <unistd.h>
#include <ne_request.h>
#include <ne_props.h>
#include <ne_uri.h>

#include "common.h"

#define XML_DECL "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" 

static char *src, *res, *collX, *collY;
char *bind_uri;
char *base_uri;
char *del_uri;
char *uri;
static int bind_ok = 0;

static int bind_init(void)
{
	char temp[512];
	int n=1;
	base_uri=i_path;
	res=ne_concat(i_path,"resource",NULL);
        
	// Create base collection called bind-test
	base_uri=ne_concat(i_path,"bind-test/",NULL);
    	ONNREQ("could not create collection", ne_mkcol(i_session, base_uri));
	
	sprintf(temp,"bind-test/collX/resource%d",n);
	collX=ne_concat(base_uri,"collX/",NULL);
        collY=ne_concat(base_uri,"collY/",NULL);
	//CALL(upload_foo("resource"));
	//CALL(upload_foo("resource2"));
        ONNREQ("could not create collection",ne_mkcol(i_session,collX));
        ONNREQ("could not create collection",ne_mkcol(i_session,collY));
	CALL(upload_foo(temp));
	bind_uri = ne_concat(i_path, "bind-test/collY/", NULL);
	bind_ok=1;
        return OK;
}


/*
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

*/

/*
static int propfind_invalid2(void)
{
    return do_invalid_pfind(
	"<D:propfind xmlns:D=\"DAV:\">"
	"<D:prop><bar:foo xmlns:bar=\"\"/>"
	"</D:prop></D:propfind>",
	"invalid namespace declaration in body (see FAQ)");
}
*/


/* Sending msg and then checking the response. 
*/ 
static int do_my_patch(const char *failmsg, char * uri, const char *body, char *header, int overwrite, int shouldfail)
{
    ne_request *req = ne_request_create(i_session, header, uri);
		
	if(overwrite==0){
	ne_add_request_header(req, "Overwrite", overwrite?"T":"F");
	}
	
    ne_set_request_body_buffer(req, body, strlen(body));
    
    ONNREQ(failmsg, ne_request_dispatch(req));
	if(shouldfail)
    ONV(ne_get_status(req)->klass == 2, ("%s", failmsg));
	else
    ONV(ne_get_status(req)->klass != 2, ("%s", failmsg));
		
    ne_request_destroy(req);
    
    /*
    ONNREQ("simple bind resource",ne_bind(i_session,res,coll));
    if(STATUS(201)){
         t_warning("Binding to resource didnt give 201");
    }*/
    return OK;
}

// SM 

static int bind()
{
    char temp[512];
    // <!ELEMENT bind (segment, href)>
    // reponse - <!ELEMENT bind-response ANY>
    PRECOND(bind_ok);
    /* Now create the binding */
    sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
		 "<segment>res2</segment>"
		 "<href>%s://%s%scollX/</href>"
		 "</bind>",
		 ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",1,0));

    return OK;
}

static int bind_circular()
{
    char temp[512];
    // <!ELEMENT bind (segment, href)>
    // reponse - <!ELEMENT bind-response ANY>
    PRECOND(bind_ok);
    /* Now create the binding */
    sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
		 "<segment>circular</segment>"
		 "<href>%s://%s%s</href>"
		 "</bind>",
		 ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
    CALL(do_my_patch("Rebind of resource with href defining uri",bind_uri,temp,"BIND",1,0));

    return OK;
}

static int unbind()
{
    // <!ELEMENT unbind (segment)>
    //
    // response - <!ELEMENT unbind-response ANY>
    char temp[512];
    PRECOND(bind_ok);
    sprintf(temp,XML_DECL "<unbind xmlns='DAV:'>"
		 "<segment>res2</segment>"
		 "</unbind>");
    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"UNBIND",1,0));
    return OK;
}

static int rebind()
{
    // <!ELEMENT rebind (segment,href)>
    //
    // response - <!ELEMENT rebind-response ANY>
    char temp[512];
    PRECOND(bind_ok);
    sprintf(temp,XML_DECL "<rebind xmlns='DAV:'>"
		 "<segment>res2</segment>"
		 "<href>%s://%s%scollX/resource1</href>"
		 "</rebind>",
		 ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"REBIND",1,0));
    return OK;
}

static int bindcleanup1(void)
{
	uri=ne_concat(i_path,"bind-test/collX/resource1",NULL);
    ne_delete(i_session, uri);
	
	uri=ne_concat(i_path,"bind-test/collY/circular",NULL);
    ne_delete(i_session, uri);

    ne_delete(i_session, base_uri);
    return OK;
}


static int deletemultiplebinds()
{
	uri=ne_concat(i_path,"bind-test/collY/res2/resource1",NULL);

	char  tmp[] = "/tmp/litmus2-XXXXXX";
	
	int fd = mkstemp(tmp);
	BINARYMODE(fd);
	ONV(ne_get(i_session, uri, fd),
	    ("GET of `%s' failed: %s", uri, ne_get_error(i_session)));
	close(fd);
	
	if (STATUS(200)) {
	  t_warning("GET of new resource gave %d, should be 200",
		        GETSTATUS);
	}
				  
	
	del_uri=ne_concat(i_path,"bind-test/collY/res2",NULL);
	ONV(ne_delete(i_session, del_uri),("DELETE on normal resource failed: %s", ne_get_error(i_session)));
	
	ONN("DELETE nonexistent resource succeeded",
	    ne_delete(i_session, uri) != NE_ERROR);
	
	//	if (STATUS(404)) {
	//		    t_warning("DELETE on null resource gave %d, should be 404",
	//					          GETSTATUS);
	//	}
	
	return OK;	
}

static int movemultiplebinds()
{
	char* src=ne_concat(i_path,"bind-test/collY/res2",NULL);
	char* dest=ne_concat(i_path,"bind-test/collX/resself",NULL);
	
	ONM2REQ("MOVE", src, dest, ne_move(i_session, 0, src, dest));
	
	if (STATUS(201)) {
		    t_warning("MOVE to new resource didn't give 201");
	}
	uri=ne_concat(i_path,"bind-test/collX/resself/resself/resource1",NULL);

	char  tmp[] = "/tmp/litmus2-XXXXXX";
	
	int fd = mkstemp(tmp);
	BINARYMODE(fd);
	ONV(ne_get(i_session, uri, fd),
	    ("GET of `%s' failed: %s", uri, ne_get_error(i_session)));
	close(fd);

	//make a new bind resource from CollX to CollY
	char temp[512];
	bind_uri = ne_concat(i_path, "bind-test/collX/", NULL);
	sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
	 	           "<segment>resource</segment>"
		           "<href>%s://%s%scollY/</href>"
		           "</bind>",
		           ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
 	
	CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",1,0));
	
	int n=2;
	//upload files to CollY/
	sprintf(temp,"bind-test/collY/resource%d",n);
	CALL(upload_foo(temp));
	n++;
	sprintf(temp,"bind-test/collY/resource%d",n);
	CALL(upload_foo(temp));

	//delete file from CollY through by using the multiple binds in CollX
	//using selfbind in CollX and bind in CollX to CollY
	del_uri=ne_concat(i_path,"bind-test/collX/resself/resource/resource2",NULL);
	ONV(ne_delete(i_session, del_uri),("DELETE on normal resource failed: %s", ne_get_error(i_session)));

	//create a new collection inside CollX
	char* coll = ne_concat(base_uri,"collX/newcollection",NULL);
    ONNREQ("could not create collection",ne_mkcol(i_session,coll));

	//upload file in the new collection
	n++;
	sprintf(temp,"bind-test/collX/newcollection/resource%d",n);
	CALL(upload_foo(temp));

	//create a new bind for the newcollection
	bind_uri = ne_concat(i_path, "bind-test/", NULL);
	sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
				          "<segment>bind2newcollection</segment>"
						  "<href>%s://%s%scollX/newcollection/</href>"
				          "</bind>",
				          ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",1,0));

	//create another bind for collection X
	sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
				          "<segment>bind2collX</segment>"
						  "<href>%s://%s%scollX/</href>"
				          "</bind>",
				          ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);

    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",1,0));
			
	
	//move newcollection over collX, Child Over Parent	
	src=ne_concat(i_path,"bind-test/collX",NULL);
    dest=ne_concat(i_path,"bind-test/collX/newcollection",NULL);
	ONM2REQ("MOVE", src, dest, ne_move(i_session, 1, src, dest));

	if (STATUS(204)) {
		t_warning("MOVE to existing resource didn't give 204");
	}

	
	bind_uri = ne_concat(i_path, "bind-test/", NULL);
	sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
	 	           "<segment>binding1</segment>"
		           "<href>%s://%s%sbind2newcollection/</href>"
		           "</bind>",
		           ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
	CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",1,0));
	
	bind_uri = ne_concat(i_path, "bind-test/", NULL);
	sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
	 	           "<segment>binding2</segment>"
		           "<href>%s://%s%sbind2newcollection/resource4</href>"
		           "</bind>",
		           ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
	CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",1,0));
	
	//move parent over child
	src=ne_concat(i_path,"bind-test/bind2newcollection",NULL);
    dest=ne_concat(i_path,"bind-test/bind2newcollection/resource4",NULL);
	ONM2REQ("MOVE", src, dest, ne_move(i_session, 1, src, dest));

	if (STATUS(204)) {
		t_warning("MOVE to existing resource didn't give 204");
	}
				
	sprintf(temp,"bind-test/collX");
	CALL(upload_foo(temp));
	
	//remove the self loops so that cleanup can clean things
	coll=ne_concat(i_path,"bind-test/bind2collX/newcollection",NULL);
	ne_delete(i_session, coll);
	coll=ne_concat(i_path,"bind-test/bind2collX/resself",NULL);
	ne_delete(i_session, coll);
	coll=ne_concat(i_path,"bind-test/binding1/resource4",NULL);
	ne_delete(i_session, coll);

	
    return OK;
	
    
}


static int copymultiplebinds(void)
{
  
  char *src, *dest;
  char temp[512];

  //copy bind-test to bind-test3 with depth 0
  src=ne_concat(i_path,"bind-test",NULL);
  dest=ne_concat(i_path,"bind-test3",NULL);
  ONV(ne_copy(i_session, 1, NE_DEPTH_ZERO, src, dest),
      ("COPY-on-existing with 'Overwrite: T': %s", ne_get_error(i_session)));
  
  //copy with depth 0 should not have copied collX directory
  src = ne_concat(i_path,"bind-test3/collX",NULL);
  ONN("DELETE nonexistent resource succeeded",
      ne_delete(i_session, src) != NE_ERROR);

  //delete bind-test3
  src=ne_concat(i_path,"bind-test3",NULL);
  ONN("Delete on existing resource failed",
      ne_delete(i_session, src) == NE_ERROR);
  

  //copy bind-test to bind-test2
  src=ne_concat(i_path,"bind-test",NULL);
  dest=ne_concat(i_path,"bind-test2",NULL);
  
  //create a collection of dest name
  ONNREQ("could not create collection",ne_mkcol(i_session,dest));

  //copy src to destination with overwrite, it should succeed
  ONV(ne_copy(i_session, 1, NE_DEPTH_INFINITE, src, dest),
      ("COPY-on-existing with 'Overwrite: T': %s", ne_get_error(i_session)));
  
  //copy from src to destination with Overwrite=F should fail
  ONN("COPY on existing resource with Overwrite: F",
      ne_copy(i_session, 0, NE_DEPTH_INFINITE, src, dest) != NE_ERROR);
 
  //delete bind-test
  ne_delete(i_session, base_uri);
  
  //delete to check
  src=ne_concat(i_path,"bind-test2/collY/res2",NULL);
  ONN("Delete on existing resource failed",
      ne_delete(i_session, src) == NE_ERROR);
  
  //upload a file
  sprintf(temp,"bind-test2/collX/bind-test2copy");
  CALL(upload_foo(temp));
  
  //copying parent over child, over the file we just uploaded
  src=ne_concat(i_path,"bind-test2",NULL);
  dest=ne_concat(i_path,"bind-test2/collX/bind-test2copy",NULL);
  ONV(ne_copy(i_session, 1, NE_DEPTH_INFINITE, src, dest),
      ("COPY-on-existing with 'Overwrite: T': %s", ne_get_error(i_session)));
  
  //delete to check
  src=ne_concat(i_path,"bind-test2/collX/bind-test2copy/collX/bind-test2copy",NULL);
  ONN("Delete on existing resource failed",
      ne_delete(i_session, src) == NE_ERROR);
  
  //delete bind-test2
  src=ne_concat(i_path,"bind-test2",NULL);
  ONN("Delete on existing resource failed",
      ne_delete(i_session, src) == NE_ERROR);
  

  return OK;
}


static int bindcleanup(void)
{
  ne_delete(i_session, base_uri);
  return OK;
}

/* call bind with overwrite F should fail */
static int bind_with_overwrite_f()
{
    char temp[512];
    // <!ELEMENT bind (segment, href)>
    // reponse - <!ELEMENT bind-response ANY>
	bind_uri = ne_concat(i_path, "bind-test/", NULL);

    /* Now create the binding */
    sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
		 "<segment>collX</segment>"
		 "<href>%s://%s%scollX/resource1</href>"
		 "</bind>",
		 ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",0,1));
	return OK;
}


/* call bind with overwrite */
static int bind_with_overwrite_destination()
{
    char temp[512];
    // <!ELEMENT bind (segment, href)>
    // reponse - <!ELEMENT bind-response ANY>
    bind_uri = ne_concat(i_path, "bind-test/", NULL);
    
    /* Now create the binding */
    sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
		 "<segment>collX_bind</segment>"
		 "<href>%s://%s%scollX</href>"
            "</bind>",
            ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",0,0));
    
        
    /* Now create the binding */
    sprintf(temp,XML_DECL "<bind xmlns='DAV:'>"
            "<segment>collX</segment>"
            "<href>%s://%s%scollX/resource1</href>"
            "</bind>",
            ne_get_scheme(i_session),ne_get_server_hostport(i_session),base_uri);
    CALL(do_my_patch("Bind of resource with href defining uri",bind_uri,temp,"BIND",1,0));
    return OK;
    
}





ne_test tests[] = 
  {
    INIT_TESTS,
    T(bind_init),
    
    T(bind), 
    T(bind_circular),
    T(unbind),T(rebind),
    T(bindcleanup1),
    
    T(bind_init),
    T(bind),
    T(deletemultiplebinds),
    T(bind),
    T(movemultiplebinds),
    T(bindcleanup),
   
    T(bind_init),
    T(bind),
    T(copymultiplebinds),
  
    T(bind_init),
    T(bind_with_overwrite_f),
    T(bind_with_overwrite_destination),
    T(bindcleanup),
    FINISH_TESTS
};
