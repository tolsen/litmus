/* 
   Litmus tests for large file support
   Copyright (C) 2004, 2005, Joe Orton <joe@manyfish.co.uk>

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

#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "ne_request.h"

#include "tests.h"
#include "common.h"

#ifndef INT64_C
#define INT64_C(x) x ## LL
#endif

#define BLOCKSIZE (INT64_C(8192))
#define NUMBLOCKS (262152)
#define TOTALSIZE (BLOCKSIZE * NUMBLOCKS)

static char block[BLOCKSIZE], *path;

static int init_largefile(void)
{
    int n;

#ifndef NE_LFS
    if (sizeof(off_t) == 4) {
        t_context("32-bit off_t and no LFS support detected "
                  "=> cannot run tests!");
        return SKIPREST;
    }
#endif    

    for (n = 0; n < BLOCKSIZE; n++)
        block[n] = n % 256;

    /* upload a random file to prep auth if necessary. */
    CALL(upload_foo("random.txt"));

    path = ne_concat(i_path, "large.txt", NULL);

    /* don't log a message for each body block! */
    ne_debug_init(ne_debug_stream, ne_debug_mask & ~(NE_DBG_HTTPBODY|NE_DBG_HTTP));

    return OK;
}

static ssize_t provider(void *userdata, char *buffer, size_t buflen)
{
    int *count = userdata;

    if (buflen == 0) {
        *count = 0;
        return 0;
    }

    assert(buflen == BLOCKSIZE);

    if ((*count)++ < NUMBLOCKS) {
        memcpy(buffer, block, BLOCKSIZE);
        return buflen;
    }        
    else
        return 0;    
}

static int large_put(void)
{
    ne_request *req = ne_request_create(i_session, "PUT", path);
    int count, ret;
   
#ifdef NE_LFS
    ne_set_request_body_provider64(req, TOTALSIZE, provider, &count);
#else
    ne_set_request_body_provider(req, TOTALSIZE, provider, &count);
#endif
    
    ret = ne_request_dispatch(req);

    ONNREQ("large PUT request", ret || ne_get_status(req)->klass != 2);

    ne_request_destroy(req);

    return OK;
}

static int large_get(void)
{
    ne_request *req = ne_request_create(i_session, "GET", path);
    char buffer[BLOCKSIZE], origin[BLOCKSIZE * 2];
    long long progress = 0;
    ssize_t offset = 0;
    ssize_t bytes;

    memcpy(origin, block, BLOCKSIZE);
    memcpy(origin + BLOCKSIZE, block, BLOCKSIZE);

    ONNREQ("begin large GET request", ne_begin_request(req));

    ONNREQ("failed GET request", ne_get_status(req)->klass != 2);

    while ((bytes = ne_read_response_block(req, buffer, BLOCKSIZE)) > 0) {
        ONV(memcmp(origin + offset, buffer, bytes),
            ("byte mismatch at %" NE_FMT_LONG_LONG, progress));
        offset = (offset + bytes) % BLOCKSIZE;
        progress += bytes;
    }

    ONNREQ("failed reading GET response", bytes < 0);

    ONNREQ("end large GET request", ne_end_request(req));

    ne_request_destroy(req);
    return OK;
}

ne_test tests[] = {
    INIT_TESTS,
    T(init_largefile),

    T(large_put),    
    T(large_get),

    FINISH_TESTS
};
