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

/* Several tests here are based on or copied from code contributed by
 * Chris Sharp <csharp@apple.com> */

#include "config.h"

#include <stdlib.h>

#include <ne_props.h>
#include <ne_uri.h>
#include <ne_locks.h>
//for setting the acl entries of a new user.
#include <ne_acl.h> 
#include <ne_auth.h>

#include "common.h"

#define NS "http://webdav.org/neon/litmus/"

#define NSPACE(x) ((x) ? (x) : "")

static const ne_propname props[] = {
    { "DAV:", "owner" },
    { "DAV:", "lockdiscovery" },
    { "DAV:", "supportedlock" },
//    { NS, "foo" },
    { NULL }
};

#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

#define XML_DECL "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" 

#define NP (4)

static char *res, *res2, *res3, *coll;
static ne_lock_store *store;
static struct ne_lock reslock, *gotlock = NULL;

static const struct ne_xml_idmap map[] = {
    { "DAV:", "resourcetype", ELM_resourcetype },
    { "DAV:", "collection", ELM_collection }
};

static ne_proppatch_operation pops[NP + 1];
static ne_propname propnames[NP + 1];
static char *values[NP + 1];
static int numprops = NP, removedprops = 5;

struct private {
    int collection;
};

struct results {
    ne_propfind_handler *ph;
    int result;
};

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

static int report_returns_wellformed(const char *uri, const char *msg, const char *body, char *request)
{
    ne_xml_parser *p = ne_xml_create();
    ne_request *req = ne_request_create(i_session, request, uri);
    ne_add_depth_header(req, NE_DEPTH_ZERO);

    ne_set_request_body_buffer(req, body, strlen(body));

    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, p);
    ONMREQ(request, uri, ne_request_dispatch(req));
    ONV(ne_get_status(req)->code != 207, ("%s", msg));
    ONV(ne_get_status(req)->klass == 4, ("%s", msg));
    ONV(ne_xml_failed(p), ("%s response %s was not well-formed: %s",
			   request,msg, ne_xml_get_error(p)));

    ne_xml_destroy(p);
    ne_request_destroy(req);
    return OK;
}

static int form_query(char *body, char *err_msg)
{
	PRECOND(gotlock);
	char temp[1024],err[1024];
    	sprintf(temp,XML_DECL "<propfind xmlns='DAV:'><prop>"
			"<%s xmlns='DAV:'/></prop></propfind>",body);
    	sprintf(err,"Locks - Retrieving %s element. %s",body,err_msg);
	return report_returns_wellformed(res,err,temp,"PROPFIND");
	// FIX IT. Complete XML Parsing. 	
}


static int precond(void)
{
    if (!i_class2) {
	t_context("locking tests skipped,\n"
		  "server does not claim Class 2 compliance");
	return SKIPREST;
    }
    
    return OK;
}

static int init_locks(void)
{
    store = ne_lockstore_create();    
    ne_lockstore_register(store, i_session);
    return OK;
}

static int put(void)
{
    res = ne_concat(i_path, "lockme", NULL);
    res2 = ne_concat(i_path, "notlocked", NULL);
    res3 = ne_concat(i_path,"not-existing",NULL);

    CALL(upload_foo("lockme"));
    CALL(upload_foo("notlocked"));    

    return OK;
}

/* Get a lock, store pointer in global 'getlock'. */
static int getlock(enum ne_lock_scope scope, int depth)
{
    
    memset(&reslock, 0, sizeof(reslock));

    ne_fill_server_uri(i_session, &reslock.uri);
    reslock.uri.path = res;
    reslock.depth = depth;
    reslock.scope = scope;
    reslock.type = ne_locktype_write;
    reslock.timeout = 3600;
    reslock.owner = ne_strdup("litmus test suite");

    /* leave gotlock as NULL if the LOCK fails. */
    gotlock = NULL;

    ONMREQ("LOCK", res, ne_lock(i_session, &reslock));
    
    /* Take a copy of the lock. */
    gotlock = ne_lock_copy(&reslock);

    ne_lockstore_add(store, gotlock);

    return OK;
}

static int custom_getlock(enum ne_lock_scope scope, int depth)
{
    //memset(&reslock, 0, sizeof(reslock));
    ne_fill_server_uri(i_session, &reslock.uri);
    reslock.uri.path = res;
    reslock.depth = depth;
    reslock.scope = scope;
    reslock.type = ne_locktype_write;
    reslock.timeout = NE_TIMEOUT_CLOSE_TO_INFINITE;
    reslock.owner = ne_strdup("litmus test suite");
    /* leave gotlock as NULL if the LOCK fails. */
    gotlock = NULL;
    ONMREQ("LOCK", res, ne_lock(i_session, &reslock));
    /* Take a copy of the lock. */
    gotlock = ne_lock_copy(&reslock);
    ne_lockstore_add(store, gotlock);
    return OK;
}

static int lock_on_no_file(void)
{
    char *tmp;
    res = ne_concat(i_path, "locknullfile", NULL);
    tmp = ne_concat(i_path, "whocares", NULL);
		
    getlock(ne_lockscope_exclusive, NE_DEPTH_ZERO);
	
	if (STATUS(201)) 
		t_warning("Lock Null returned %d not 201", GETSTATUS);

    /*FIXME: After Lock Null is created, Do Unlock it to maintain integrity of tests
     * ONNREQ2("unlock of second shared lock",ne_unlock(i_session, &gotlock)); 
     * */
    
   
    /* Copy of nulllock resource */
    ONN("COPY null locked resource should ",
	ne_copy(i_session, 1, NE_DEPTH_ZERO, res, tmp) == NE_ERROR);
     
    /* Delete of nulllockresource */
    ONN("DELETE of locknull resource by owner", 
	ne_delete(i_session, tmp) == NE_ERROR);
    free(tmp);

    /* Move of nulllockresource */
    tmp = ne_concat(i_path, "who-cares", NULL);
    ONN("MOVE of null-locked resource", 
	ne_move(i_session, 0, res, tmp) == NE_ERROR);
    ONN("DELETE of locknull resource by owner after a MOVE with overwrite (F)", 
	ne_delete(i_session, tmp) == NE_ERROR);
   
    /* Delete the locktoken from store */ 	
    ne_lockstore_remove(store, gotlock);
    getlock(ne_lockscope_exclusive, NE_DEPTH_ZERO);
	if (STATUS(201)) 
		t_warning("Lock Null returned %d not 201", GETSTATUS);

     /* Lot of code duplication, but want to test each case individually.
      * Locknull resource. How it behaves when it is copied 
      * moved (with overwrite T/F)
      * PUT request on locknullresource should succeed. 
      */
     

    /* MOVE of null-locked resource with overwrite=T */
    ONN("MOVE of null-locked resource with overwrite=T (1)", 
	ne_move(i_session, 1, res, tmp) == NE_ERROR);
    ne_lockstore_remove(store, gotlock);
    getlock(ne_lockscope_exclusive, NE_DEPTH_ZERO);
	if (STATUS(201)) 
		t_warning("Lock Null returned %d not 201", GETSTATUS);
    ONN("MOVE of null-locked resource with overwrite=T (2)", 
	ne_move(i_session, 1, res, tmp) == NE_ERROR);
	
    ne_lockstore_remove(store, gotlock);
    getlock(ne_lockscope_shared, NE_DEPTH_ZERO);
	if (STATUS(201)) 
		t_warning("Lock Null returned %d not 201", GETSTATUS);
    
    ONN("COPY on null-locked resource with overwrite=T", 
	ne_copy(i_session, 1, NE_DEPTH_ZERO, tmp, res) == NE_ERROR);

   ONN("DELETE of locknull resource by owner after a MOVE (T) ", 
	ne_delete(i_session, tmp) == NE_ERROR);
    free(tmp);

    /* Put on nulllockresource */
    ONV(ne_put(i_session,res, i_foo_fd),
	 ("PUT on locknullfile resource failed: %s", ne_get_error(i_session)));

  

	return OK;
}

static int unlock_on_no_file(void)
{
	/* FIXME: After Lock Null is created, Do Unlock it to maintain integrity of tests */
	ONNREQ2("unlock of second shared lock",ne_unlock(i_session, gotlock));
	//CALL(ne_unlock(i_session, gotlock));
    /* Remove lock from session. */
    ne_lockstore_remove(store, gotlock);
    /* for safety sake. */
    gotlock = NULL;
    /* Delete the resource. Not required after this */
    ne_delete(i_session,res);
	return OK;
}


static int lock_excl(void)
{
    return getlock(ne_lockscope_exclusive, NE_DEPTH_ZERO);
}

static int lock_fail(char *uri,enum ne_lock_scope scope, int depth)
{
    PRECOND(gotlock);
    struct ne_lock dummy;

    memcpy(&dummy, &reslock, sizeof(reslock));
    dummy.scope = scope;
    dummy.token = ne_strdup("opaquelocktoken:foobar");
    dummy.uri.path = uri;
    dummy.depth = depth;
    dummy.owner = ne_strdup("Fail lock");
    
    /* leave gotlock as NULL if the LOCK fails. */
    ONN("Conflicting LOCK should fail", ne_lock(i_session2, &dummy) != NE_ERROR);
    
    //if (STATUS2(423))
	//  t_warning("LOCK failed with %d not 423", GETSTATUS2);

    return OK;
}

static int lock_excl_fail(void)
{
    return lock_fail(res,ne_lockscope_exclusive, NE_DEPTH_ZERO);
}

static int lock_shared(void)
{
    return getlock(ne_lockscope_shared, NE_DEPTH_ZERO);
}

// Infinite depth lock on a resource. 

static int lock_infinite(void)
{
    return getlock(ne_lockscope_shared, NE_DEPTH_INFINITE);
}

static int lock_invalid_depth(void)
{
    return getlock(ne_lockscope_shared, -1);
}

static int notowner_modify(void)
{
    char *tmp;
    ne_propname pname = { "http://webdav.org/neon/litmus/", "random" };
    ne_proppatch_operation pops[] = { 
	{ NULL, ne_propset, "foobar" },
	{ NULL }
    };

    PRECOND(gotlock);

    pops[0].name = &pname;

    ONN("DELETE of locked resource should fail", 
    	ne_delete(i_session2, res) != NE_ERROR);

    if (STATUS2(423)) 
    	t_warning("DELETE failed with %d not 423", GETSTATUS2);

    tmp = ne_concat(i_path, "who-cares", NULL);
    ONN("MOVE of locked resource should fail", 
	ne_move(i_session2, 1, res, tmp) != NE_ERROR);
    free(tmp);
    
    if (STATUS2(423))
    	t_warning("MOVE failed with %d not 423", GETSTATUS2);
    
    ONN("COPY onto locked resource should fail",
	    ne_copy(i_session2, 1, NE_DEPTH_ZERO, res2, res) != NE_ERROR);

    if (STATUS2(423))
    	t_warning("COPY failed with %d not 423", GETSTATUS2);

    ONN("PROPPATCH of locked resource should fail",
	    ne_proppatch(i_session2, res, pops) != NE_ERROR);
    
    if (STATUS2(423))
    	t_warning("PROPPATCH failed with %d not 423", GETSTATUS2);

    //("PUT on locked resource should fail",
	    CALL(ne_put(i_session2, res, i_foo_fd));

    if (STATUS2(423))
    	t_warning("PUT failed with %d not 423", GETSTATUS2);

    return OK;    
}

static int notowner_lock(void)
{
    struct ne_lock dummy;

    PRECOND(gotlock);

    memcpy(&dummy, &reslock, sizeof(reslock));
    dummy.token = ne_strdup("opaquelocktoken:foobar");
    dummy.scope = ne_lockscope_exclusive;
    dummy.owner = ne_strdup("notowner lock");

    ONN("UNLOCK with bogus lock token",
	ne_unlock(i_session2, &dummy) != NE_ERROR);

    /* 2518 doesn't really say what status code that UNLOCK should
     * fail with. mod_dav gives a 400 as the locktoken is bogus.  */
    
    ONN("LOCK on locked resource",
	ne_lock(i_session2, &dummy) != NE_ERROR);
    
    if (dummy.token)  
        ne_free(dummy.token);

    if (STATUS2(423))
	t_warning("LOCK failed with %d not 423", GETSTATUS2);

    return OK;
}

//TODO: currently hardcoding the user.
//will have to change after decision is taken on how user would be created.
const char *i_newuser="test2";
static int newauth(void *ud, const char *realm, int attempt,
		char *username, char *password)
{
	//hardcoding for now, read it from the newuser file in the limestone folder.
    strcpy(username, i_newuser);
    strcpy(password, "qwerty");
    return attempt;
}

static int init_newsession(ne_session *sess)
{
/*
//in order to do this I will have to declare this global as well.
    if (proxy_hostname) {
	ne_session_proxy(sess, proxy_hostname, proxy_port);
    }
*/
    	ne_set_useragent(sess, "litmus/" PACKAGE_VERSION);
	ne_set_server_auth(sess, newauth, NULL);

/*
//TODO:once scheme is globally declared, uncomment this.
    if (use_secure) {
	if (!ne_has_support(NE_FEATURE_SSL)) {
	    t_context("No SSL support, reconfigure using --with-ssl");
	    return FAILHARD;
	} else {
	    ne_ssl_set_verify(sess, ignore_verify, NULL);
	}
    }
*/
    return OK;
}

static int setacl(const char *failmsg,int status)
{
    char body[1024];
    sprintf(body, "<D:acl xmlns:D='DAV:'>"
		   "<D:ace>" 
	      	   "<D:principal>"
		   "<D:href>/users/test2</D:href>"
		   "</D:principal>"
		   "<D:grant>"
		   "<D:privilege><D:all/></D:privilege>"
		   "</D:grant>"
		   "</D:ace>"
		   "</D:acl>");
    ne_request *req = ne_request_create(i_session, "ACL", i_path);
    ne_set_request_body_buffer(req, body, strlen(body));
    ONNREQ(failmsg, ne_request_dispatch(req));
    if(status)
    	ONV(ne_get_status(req)->klass != 2, ("%s", failmsg));
    else 
    	ONV(ne_get_status(req)->klass != 4, ("%s", failmsg));
    ne_request_destroy(req);
    return OK;
}
static int newowner_modify(int istoken)
{

	//create a new ne_session with an entierly new user.
	//grant him locking, unlocking , move copy etc privileges on the resource.
	//try to move/copy/delete the already "exclusively locked resource", which shud obviously fail.
	char *tmp;
	ne_session *tmp_session;
	int status;
	//ne_acl_entry entries[]={ {ne_acl_href,ne_acl_grant, "http://localhost:8080/users/test2",0,0,0,0,0} };
	ne_propname pname = { "http://webdav.org/neon/litmus/", "random" };
	ne_proppatch_operation pops[] = { 
		{ NULL, ne_propset, "foobar" },
		{ NULL }
	};
	
	PRECOND(gotlock);
	
	pops[0].name = &pname;
	
	//I know I am hardcoding the scheme
	//TODO: will have to change common.h to globally declare the scheme as well.
	tmp_session = ne_session_create("http", i_hostname, i_port);
	CALL(init_newsession(tmp_session));

	//also registering the tokens with this session	
	if(istoken)
	  ne_lockstore_register(store, tmp_session);
	//now setting the acl
	//const char *user_href=ne_concat("http://",i_hostname,":",i_port,"/
	//ne_acl_set(i_session,i_path,entries,1);
	setacl("Trying to set acl for new user",1);
	//try to move copy and delete a resource using this new session.

    	ONN("DELETE of locked resource by different user should fail", 
    		ne_delete(tmp_session, res) != NE_ERROR);
	status=atoi(ne_get_error(tmp_session));
    	if (status!=423) 
    		t_warning("DELETE failed with %d not 423", status);

	tmp = ne_concat(i_path, "who-cares", NULL);
	ONN("MOVE of locked resource by different user should fail", 
		ne_move(tmp_session, 1, res, tmp) != NE_ERROR);
	free(tmp);
	status=atoi(ne_get_error(tmp_session));
	if (status!=423)
		t_warning("MOVE failed with %d not 423", status );
	
	ONN("COPY onto locked resource by different user should fail",
		ne_copy(tmp_session, 1, NE_DEPTH_ZERO, res2, res) != NE_ERROR);
	status=atoi(ne_get_error(tmp_session));
	if (status!=423)
		t_warning("COPY failed with %d not 423", status);
	
	ONN("PROPPATCH of locked resource by different user should fail",
		ne_proppatch(tmp_session, res, pops) != NE_ERROR);
	status=atoi(ne_get_error(tmp_session));
	if (status!=423)
		t_warning("PROPPATCH failed with %d not 423", status);
	
	//ONN("PUT on locked resource  by different user should fail",
		CALL(ne_put(tmp_session, res, i_foo_fd));
	status=atoi(ne_get_error(tmp_session));
	if (status!=423)
		t_warning("PUT failed with %d not 423", status);
	
//destroy the tmp session
	ne_session_destroy(tmp_session);
	return OK; 

}

static int newowner_modify_notoken(void)
{
	return newowner_modify(0);
}

static int newowner_modify_correcttoken(void)
{
	return newowner_modify(1);
}

/* take out another shared lock on the resource. */
static int double_sharedlock(void)
{
    struct ne_lock dummy;

    PRECOND(gotlock);

    memcpy(&dummy, &reslock, sizeof(reslock));
    dummy.token = NULL;
    dummy.owner = ne_strdup("litmus: notowner_sharedlock");
    dummy.scope = ne_lockscope_shared;

    ONNREQ2("shared LOCK on locked resource", 
	    ne_lock(i_session2, &dummy));
    
    ONNREQ2("unlock of second shared lock",
	    ne_unlock(i_session2, &dummy));

    return OK;
}

static int owner_modify(void)
{
    char *tmp;
    ne_propname pnames[] = { { "http://webdav.org/neon/litmus/", "random" } };
    ne_proppatch_operation pops[] = { 
	{ pnames, ne_propset, "foobar" },
	{ NULL }
    };
    PRECOND(gotlock);

    ONV(ne_put(i_session, res, i_foo_fd),
	("PUT on locked resource failed: %s", ne_get_error(i_session)));

    tmp = ne_concat(i_path, "whocares", NULL);
    ONN("COPY of locked resource", 
	ne_copy(i_session, 1, NE_DEPTH_ZERO, res, tmp) == NE_ERROR);
    
   if (STATUS(201))
	t_warning("COPY failed with %d not 201", GETSTATUS);

    ONN("DELETE of locked resource by owner", 
	ne_delete(i_session, tmp) == NE_ERROR);

    if (STATUS(204)) 
	t_warning("DELETE of %s failed with %d not 200", tmp, GETSTATUS);
    free(tmp);
    
    ONN("PROPPATCH of locked resource",
    	ne_proppatch(i_session, res, pops) == NE_ERROR);
    
    if (STATUS(207))
	t_warning("PROPPATCH failed with %d", GETSTATUS);

    return OK;
}

/* ne_lock_discover which counts number of calls. */
static void count_discover(void *userdata, const struct ne_lock *lock,
			   const char *uri, const ne_status *status)
{
    if (lock) {
	int *count = userdata;
	*count += 1;
    }
}

/* check that locks don't follow copies. */
static int copy(void)
{
    char *dest;
    int count = 0;
    
    PRECOND(gotlock);

    dest = ne_concat(res, "-copydest", NULL);

    ne_delete(i_session2, dest);

    ONNREQ2("could not COPY locked resource",
	    ne_copy(i_session, 1, NE_DEPTH_ZERO, res, dest));
    
    ONNREQ2("LOCK discovery failed",
	    ne_lock_discover(i_session, dest, count_discover, &count));
    
    ONV(count != 0,
	("found %d locks on copied resource", count));

    ONNREQ2("could not delete copy of locked resource",
	    ne_delete(i_session, dest));

    free(dest);

    return OK;
}

/* Compare locks, expected EXP, actual ACT. */
static int compare_locks(const struct ne_lock *exp, const struct ne_lock *act)
{
    ONCMP(exp->token, act->token, "compare discovered lock", "token");
    ONCMP(exp->owner, act->owner, "compare discovered lock", "owner");
    return OK;
}

/* check that the lock returned has correct URI, token */
static void verify_discover(void *userdata, const struct ne_lock *lock,
			    const char *uri, const ne_status *status)
{
    int *ret = userdata;

    if (*ret == 1) {
	/* already failed. */
	return;
    }
 
    if (lock) {
        *ret = compare_locks(gotlock, lock);
    } else {
	*ret = 1;
	t_context("failed: %d %s\n", status->code, status->reason_phrase);
    }

}

static int discover(void)
{
    int ret = 0;
    
    PRECOND(gotlock);

    ONNREQ("lock discovery failed",
	   ne_lock_discover(i_session, res, verify_discover, &ret));

    /* check for failure from the callback. */
    if (ret)
	return FAIL;

    return OK;    
}

static int refresh(void)
{
    PRECOND(gotlock);

    ONMREQ("LOCK refresh", gotlock->uri.path,
           ne_lock_refresh(i_session, gotlock));
    
    return OK;
}

static int unlock(void)
{
    PRECOND(gotlock);

    ONMREQ("UNLOCK", gotlock->uri.path, ne_unlock(i_session, gotlock));
    /* Remove lock from session. */
    ne_lockstore_remove(store, gotlock);
    /* for safety sake. */
    gotlock = NULL;
    return OK;
}

/* Unlocking a URL which was never locked. Should fail */
static int unlock_fail(void)
{
    PRECOND(gotlock);
    /* Normal UNLOCKING */
    ONMREQ("UNLOCK", gotlock->uri.path, ne_unlock(i_session, gotlock));
    /* Now the lock does not exist. It should fail */
    ONN("UNLOCKING now should fail.", 
    	ne_unlock(i_session, gotlock) != NE_ERROR);
    if(STATUS(409))
        t_warning("Unlock resulted in %d and did not result in 409",GETSTATUS);
    /* Remove lock from session. */
    ne_lockstore_remove(store, gotlock);
    /* for safety sake. */
    gotlock = NULL;
    return OK;
}


/* Perform a conditional PUT request with given If: header value,
 * placing response status-code in *code and class in *klass.  Fails
 * if requests cannot be dispatched. */
static int conditional_put(const char *ifhdr, int *klass, int *code)
{
    ne_request *req;
    
    req = ne_request_create(i_session, "PUT", res);
    ne_set_request_body_fd(req, i_foo_fd, 0, i_foo_len);

    ne_print_request_header(req, "If", "%s", ifhdr);
    
    ONMREQ("PUT", res, ne_request_dispatch(req));

    if (code) *code = ne_get_status(req)->code;
    if (klass) *klass = ne_get_status(req)->klass;
    
    ne_request_destroy(req);
    return OK;
}

/*** A series of conditional PUTs suggested by Julian Reschke. */

/* a PUT conditional on lock and etag should succeed */
static int cond_put(void)
{
    char *etag = get_etag(res);
    char hdr[200];
    int klass;

    PRECOND(etag && gotlock);
    
    ne_snprintf(hdr, sizeof hdr, "(<%s> [%s])", gotlock->token, etag);
    
    CALL(conditional_put(hdr, &klass, NULL));

    ONV(klass != 2, 
        ("PUT conditional on lock and etag failed: %s",
         ne_get_error(i_session)));

    return OK;
}

/* PUT conditional on bogus lock-token and valid etag, should fail. */
static int fail_cond_put(void)
{
    int klass, code;
    char *etag = get_etag(res);
    char hdr[200];

    PRECOND(etag && gotlock);
    
    ne_snprintf(hdr, sizeof hdr, "(<DAV:no-lock> [%s])", etag);
    
    CALL(conditional_put(hdr, &klass, &code));

    ONV(klass == 2,
        ("conditional PUT with invalid lock-token should fail: %s",
         ne_get_error(i_session)));

    ONN("conditional PUT with invalid lock-token code got 400", code == 400);

    if (code != 412) 
	t_warning("PUT failed with %d not 412", code);

    return OK;
}

/* PUT conditional on bogus lock-token and valid etag, should fail. */
static int fail_cond_put_unlocked(void)
{
    int klass, code;

    CALL(conditional_put("(<DAV:no-lock>)", &klass, &code));

    ONV(klass == 2,
        ("conditional PUT with invalid lock-token should fail: %s",
         ne_get_error(i_session)));

    ONN("conditional PUT with invalid lock-token code got 400", code == 400);

    if (code != 412) 
	t_warning("PUT failed with %d not 412", code);

    return OK;
}


/* PUT conditional on real lock-token and not(bogus lock-token),
 * should succeed. */
static int cond_put_with_not(void)
{
    int klass, code;
    char hdr[200];

    PRECOND(gotlock);

    ne_snprintf(hdr, sizeof hdr, "(<%s>) (Not <DAV:no-lock>)", 
                gotlock->token);
    
    CALL(conditional_put(hdr, &klass, &code));

    ONV(klass != 2,
        ("PUT with conditional (Not <DAV:no-lock>) failed: %s",
         ne_get_error(i_session)));

    return OK;
}

/* PUT conditional on corruption of real lock-token and not(bogus
 * lock-token) , should fail. */
static int cond_put_corrupt_token(void)
{
    int class, code;
    char hdr[200];

    PRECOND(gotlock);

    ne_snprintf(hdr, sizeof hdr, "(<%sx>) (Not <DAV:no-lock>)", 
                gotlock->token);
    
    CALL(conditional_put(hdr, &class, &code));

    ONV(class == 2,
        ("conditional PUT with invalid lock-token should fail: %s",
         ne_get_error(i_session)));

    if (code != 423)
	t_warning("PUT failed with %d not 423", code);

    return OK;
}

/* PUT with a conditional (lock-token and etag) (Not bogus-token and etag) */
static int complex_cond_put(void)
{
    int klass, code;
    char hdr[200];
    char *etag = get_etag(res);

    PRECOND(gotlock && etag != NULL);

    ne_snprintf(hdr, sizeof hdr, "(<%s> [%s]) (Not <DAV:no-lock> [%s])", 
                gotlock->token, etag, etag);
    
    CALL(conditional_put(hdr, &klass, &code));

    ONV(klass != 2,
        ("PUT with complex conditional failed: %s",
         ne_get_error(i_session)));

    return OK;
}

/* PUT with a conditional (lock-token and not-the-etag) (Not
 * bogus-token and etag) */
static int fail_complex_cond_put(void)
{
    int klass, code;
    char hdr[200];
    char *etag = get_etag(res), *pnt;

    PRECOND(gotlock && etag != NULL);

    /* Corrupt the etag string: change the third character from the end. */
    pnt = etag + strlen(etag) - 3;
    PRECOND(pnt > etag);
    (*pnt)++;

    ne_snprintf(hdr, sizeof hdr, "(<%s> [%s]) (Not <DAV:no-lock> [%s])", 
                gotlock->token, etag, etag);
    
    CALL(conditional_put(hdr, &klass, &code));

    ONV(code != 412,
        ("PUT with complex bogus conditional should fail with 412: %s",
         ne_get_error(i_session)));

    return OK;
}
    

static int prep_collection(void)
{
    if (gotlock) {
        ne_lock_destroy(gotlock);
        gotlock = NULL;
    }
  
    ne_delete(i_session,res); 
    ne_free(res);
    res = coll = ne_concat(i_path, "lockcoll/", NULL);
    ONV(ne_mkcol(i_session, res),
        ("MKCOL %s: %s", res, ne_get_error(i_session)));
    return OK;
}

/* Try to create conflicting locks within collection */
static int conflicting_locks(void)
{
    CALL(upload_foo("lockcoll/conflict.txt"));
    
    /* Test for Multi-resource lock request */
    /*
    res=ne_strdup(coll);
    CALL(custom_getlock(ne_lockscope_exclusive,NE_DEPTH_INFINITE));   
    if(STATUS(200))
        t_warning("Multi-resource lock request failed with %d",GETSTATUS);

    
    if (gotlock) {
         ne_lock_destroy(gotlock);
        gotlock = NULL;
    }
    */
    /* end multi-resource test */
    
    res=ne_concat(coll,"conflict.txt",NULL);
    
    /* Lock child resource with shared-lock and collection by exclusive-lock */
    CALL(getlock(ne_lockscope_shared,NE_DEPTH_ZERO));
    CALL(lock_fail(coll,ne_lockscope_exclusive,NE_DEPTH_INFINITE));
    CALL(unlock());   
 
    /* Lock child resource with exclusive-lock and collection by exclusive-lock */
    CALL(getlock(ne_lockscope_exclusive,NE_DEPTH_ZERO));
    CALL(lock_fail(coll,ne_lockscope_exclusive,NE_DEPTH_INFINITE));
    CALL(unlock());   
    
    /* Lock child resource with exclusive-lock and collection by shared-lock */
    CALL(getlock(ne_lockscope_exclusive,NE_DEPTH_ZERO));
    CALL(lock_fail(coll,ne_lockscope_shared,NE_DEPTH_INFINITE));
    CALL(unlock());   
    
    if (gotlock) {
        ne_lock_destroy(gotlock);
        gotlock = NULL;
    }
    ne_free(res);
    res=ne_strdup(coll);
    
    return OK;
}

static int lock_collection(void)
{
    CALL(getlock(ne_lockscope_exclusive, NE_DEPTH_INFINITE));
    /* change res to point to a normal resource for subsequent
     * {not_,}owner_modify tests */
    res = ne_concat(coll, "lockme.txt", NULL);
    return upload_foo("lockcoll/lockme.txt");
}

/* indirectly refresh the the collection lock */
static int indirect_refresh(void)
{
    struct ne_lock *indirect;

    PRECOND(gotlock);

    indirect = ne_lock_copy(gotlock);
    ne_free(indirect->uri.path);
    indirect->uri.path = ne_strdup(res);

    ONV(ne_lock_refresh(i_session, indirect),
        ("indirect refresh LOCK on %s via %s: %s",
         coll, res, ne_get_error(i_session)));

    ne_lock_destroy(indirect);

    return OK;    
}

static int lockcleanup(void)
{
    ne_delete(i_session, res2);
    ne_delete(i_session2, coll);
    ne_delete(i_session2, i_path);
    return OK;
}

static int unmap_lockroot(void)
{
    char *collX, *collY, *tmp; 
    collX=ne_concat(coll,"collX/",NULL);
    collY=ne_concat(coll,"collY/",NULL);
    ONV(ne_mkcol(i_session, collX),
        ("MKCOL - 1 %s: %s", collX, ne_get_error(i_session)));
    ONV(ne_mkcol(i_session, collY),
        ("MKCOL - 2 %s: %s", collY, ne_get_error(i_session)));
  
    /* Tests for Depth 0 locks on collection and checks if adding resources to its(collection's) children is allowed */ 
    res=ne_strdup(coll);
    CALL(getlock(ne_lockscope_exclusive,NE_DEPTH_ZERO));
    CALL(upload_foo("lockcoll/collX/conflict.txt"));        
    if(STATUS(201))
        t_warning("This upload should have worked. Resulted in %d",GETSTATUS);
    
    CALL(unlock());
    tmp=ne_concat(coll,"collX/conflict.txt",NULL);
    ne_delete(i_session,tmp);
    /* end depth 0 test */ 
    
    /* Tests for moving content into a infinite depth locked collection */
    CALL(getlock(ne_lockscope_exclusive, NE_DEPTH_INFINITE));
    /* This upload does not have locktoken should fail */
    CALL(upload_foo2("lockcoll/collX/conflict.txt"));
    if(STATUS2(423))
        t_warning("This upload should have failed with 423. Resulted in %d", GETSTATUS2);
   
     /* This upload has locktoken should pass. */
    CALL(upload_foo("lockcoll/collX/conflict.txt"));
    if(STATUS(201))
        t_warning("This upload should have passed. Resulted in %d", GETSTATUS);
    
    CALL(unlock());
    ne_delete(i_session,tmp); 
    /* end test */
 
    res=ne_concat(collX,"conflict.txt",NULL);
    CALL(upload_foo("lockcoll/collX/conflict.txt"));
    
    CALL(getlock(ne_lockscope_exclusive,NE_DEPTH_ZERO));
    /* i_session2 does not have the lock-token. */
    /* This move should fail. Trying to destroy a lock root without lock-token */
    ONN("Unmapping a lockroot that should fail.", 
    	ne_move(i_session2, 1, collY, collX) != NE_ERROR);
    
    /* This move should pass. */
    ONN("Unmapping a lockroot that should have succeeded", 
    	ne_move(i_session, 1, collY, collX) == NE_ERROR);
    
    collY=ne_strdup(res);
    res=ne_concat(collX,"conflict.txt",NULL); 
    ONN("Unmapped lockroot should not exist", discover() != NE_ERROR);
    ne_delete(i_session2,collX);
    res=ne_strdup(collY);
    return OK;
}

static int lockdiscovery(void)
{
    PRECOND(gotlock);
    return form_query("lockdiscovery","Propfind of DAV:lockdiscovery");
}

static int supportedlock(void)
{
    PRECOND(gotlock);
    return form_query("supportedlock","Propfind of DAV:supportedlock");
}

ne_test tests[] = {
    INIT_TESTS,

    /* check server is class 2. */
    T(options), T(precond),

    T(init_locks),
    T(lock_on_no_file),
    T(double_sharedlock),
    T(supportedlock), 
    T(unlock_on_no_file),

    /* upload, and exclusive lock a resource. */
    T(put), T(lock_excl),
    /* conflicting exclusive lock on a resource should fail */
    T(lock_excl_fail),
    T(lockdiscovery), 
    /* check lock discovery and refresh */
    T(discover), T(refresh),
  
    T(notowner_modify), T(notowner_lock),
    T(owner_modify),

    /* After modifying the resource, check it is still locked (this
     * catches a mod_dav regression when the atomic PUT code is
     * enabled). */
    T(notowner_modify), T(notowner_lock),

    /* make sure locks don't follow a COPY */
    T(copy),

    /* Julian's conditional PUTs. */
    T(cond_put),
    T(fail_cond_put),
    T(cond_put_with_not),
    T(cond_put_corrupt_token),
    T(complex_cond_put),
    T(fail_complex_cond_put),

    T(unlock),

    T(fail_cond_put_unlocked),

    /* now try it all again with a shared lock. */
    T(lock_shared),
    T(lock_excl_fail),

    T(notowner_modify), T(notowner_lock), T(owner_modify),

    /* take out a second shared lock */
    T(double_sharedlock),
    /* Conflicting lock should fail */ 
    T(lock_excl_fail),

    /* make sure the main lock is still intact. */
    T(notowner_modify), T(notowner_lock),
    /* finally, unlock the poor abused resource. */
    
    /* conditional PUTs. */
    T(cond_put),
    T(fail_cond_put),
    T(cond_put_with_not),
    T(cond_put_corrupt_token),
    T(complex_cond_put),
    T(fail_complex_cond_put),
    T(unlock),
    
    // Depth infinite lock on a leaf resource. 
    T(lock_infinite),
    T(lockdiscovery), 
    T(supportedlock), 
    T(notowner_modify), T(notowner_lock),
    T(discover), T(refresh),
    T(unlock_fail),
    
    T(lock_invalid_depth),
    T(unlock),
 	
    /* collection locking */
    T(prep_collection),
    T(conflicting_locks),
    T(lock_collection),
    T(supportedlock), 
    /* conflicting exclusive lock on a coll should fail */
    //T(lock_excl_fail),
    T(owner_modify), T(notowner_modify),
    /* trying to move/copy/delete with different authenticated user shud fail.*/
    T(newowner_modify_notoken),
    T(newowner_modify_correcttoken),
    T(refresh), 
    T(indirect_refresh),
    T(unlock),
    T(unmap_lockroot),
    T(lockcleanup),
    FINISH_TESTS
};
