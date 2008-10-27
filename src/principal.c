/* 
   litmus: DAV server test suite
   Copyright (C) 2001-2004, Joe Orton <joe@manyfish.co.uk>

   PRINCIPAL functionality testing.
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
char *base_uri;
char *del_uri;
char *uri;
static int principal_ok = 0;

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

static int put_user()
{
    base_uri=i_path;
    char temp[512];
    sprintf(temp,XML_DECL "<lb:user xmlns:D='DAV:' xmlns:lb='http://limebits.com/ns/1.0/'>"
		 "<D:displayname>Sharad - Test for put user</D:displayname>"
		 "<lb:name>sharad</lb:name><lb:password>text</lb:password>"
		 "<lb:email >sharad@gmail.com</lb:email>"
                 "</lb:user>");
    CALL(do_my_patch("PUT of user with name sharad","/users/sharad",temp,"PUT",1,0));
    return OK;
}

static int principalcleanup(void)
{
  ne_delete(i_session, base_uri);
  return OK;
}

ne_test tests[] = 
  {
    INIT_TESTS,
    T(put_user),
    FINISH_TESTS
};
