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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "common.h"

#define EOL "\r\n"

#define ONS(str, x) do {					\
    if ((x) < 0) {						\
	t_context("%s: %s", (str), ne_sock_error(sock));	\
	return FAIL;						\
    }								\
} while (0)

static int expect100(void)
{
    ne_socket *sock = ne_sock_create();
    char req[BUFSIZ], buf[BUFSIZ];
    ne_status status = {0};
    const ne_inet_addr *ia;
    int success = 0;

    if (strcmp(ne_get_scheme(i_session), "https") == 0) {
        t_context("skipping for SSL server");
        return SKIP;
    }        

    for (ia = ne_addr_first(i_address); ia && !success; 
	 ia = ne_addr_next(i_address))
	success = ne_sock_connect(sock, ia, i_port) == 0;

    ONN("could not connect to server", !success);
    
    sprintf(req, 
	    "PUT %sexpect100 HTTP/1.1" EOL
	    "Host: %s" EOL
	    "Content-Length: 100" EOL
	    "Expect: 100-continue" EOL EOL,
	    i_path, ne_get_server_hostport(i_session));

    NE_DEBUG(NE_DBG_SOCKET, "Request:\n%s", req);

    ONS("sending request", ne_sock_fullwrite(sock, req, strlen(req)));

    switch (ne_sock_block(sock, 30)) {
    case NE_SOCK_TIMEOUT: 
	ONN("timeout waiting for interim response", FAIL);
	break;
    case 0:
	/* no error. */
	break;
    default:
	ONN("error reading from socket", FAIL);
	break;
    }

    ONS("reading status line", ne_sock_readline(sock, buf, BUFSIZ));

    NE_DEBUG(NE_DBG_HTTP, "[status] %s", buf);

    ONN("parse status line", ne_parse_statusline(buf, &status));

    if (status.code == 100) {
	char rbuf[100] = {0};
	
	ONN("write request body", ne_sock_fullwrite(sock, rbuf, 100));
    }

    ne_sock_close(sock);

    return OK;
}

ne_test tests[] = {
    INIT_TESTS,

    T(expect100),

    FINISH_TESTS
};

