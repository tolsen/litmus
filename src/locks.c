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

#include "common.h"

static char *res, *res2, *coll;
static ne_lock_store *store;

static struct ne_lock reslock, *gotlock = NULL;

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

static int lock_excl(void)
{
    return getlock(ne_lockscope_exclusive, NE_DEPTH_ZERO);
}

static int lock_shared(void)
{
    return getlock(ne_lockscope_shared, NE_DEPTH_ZERO);
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

    tmp = ne_concat(i_path, "whocares", NULL);
    ONN("MOVE of locked resource should fail", 
	ne_move(i_session2, 0, res, tmp) != NE_ERROR);
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

    ONN("PUT on locked resource should fail",
	ne_put(i_session2, res, i_foo_fd) != NE_ERROR);

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
    PRECOND(gotlock);

    ONV(ne_put(i_session, res, i_foo_fd),
	("PUT on locked resource failed: %s", ne_get_error(i_session)));

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
	    ne_copy(i_session2, 1, NE_DEPTH_ZERO, res, dest));
    
    ONNREQ2("LOCK discovery failed",
	    ne_lock_discover(i_session2, dest, count_discover, &count));
    
    ONV(count != 0,
	("found %d locks on copied resource", count));

    ONNREQ2("could not delete copy of locked resource",
	    ne_delete(i_session2, dest));

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
    ne_free(res);
    res = coll = ne_concat(i_path, "lockcoll/", NULL);
    ONV(ne_mkcol(i_session, res),
        ("MKCOL %s: %s", res, ne_get_error(i_session)));
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

ne_test tests[] = {
    INIT_TESTS,

    /* check server is class 2. */
    T(options), T(precond),

    T(init_locks),

    /* upload, and exclusive lock a resource. */
    T(put), T(lock_excl),
  
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

    T(notowner_modify), T(notowner_lock), T(owner_modify),

    /* take out a second shared lock */
    T(double_sharedlock),

    /* make sure the main lock is still intact. */
    T(notowner_modify), T(notowner_lock),
    /* finally, unlock the poor abused resource. */
    T(unlock),
    
    /* collection locking */
    T(prep_collection),
    T(lock_collection),
    T(owner_modify), T(notowner_modify),
    T(refresh), 
    T(indirect_refresh),
    T(unlock),

    FINISH_TESTS
};
