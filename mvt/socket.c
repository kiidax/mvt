/* Multi-purpose Virtual Terminal
 * Copyright (C) 2012 Katsuya Iida
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <mvt/mvt.h>
#include "private.h"
#include "debug.h"

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct _mvt_socket mvt_socket_t;

struct _mvt_socket {
    mvt_session_t parent;
#ifdef WIN32
    SOCKET sock;
#else
	int sock;
#endif
    const char *hostname;
    int port;
};

mvt_session_t *mvt_socket_open(char **args, mvt_session_t *source, int width, int height);
static void mvt_socket_close(mvt_session_t *session);
static int mvt_socket_connect(mvt_session_t *session);
static int mvt_socket_read (mvt_session_t *session, void *buf, size_t count, size_t *countread);
static int mvt_socket_write (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten);
static void mvt_socket_shutdown (mvt_session_t *session);
static void mvt_socket_resize (mvt_session_t *session, int width, int height);

static const mvt_session_vt_t mvt_socket_vt = {
    mvt_socket_close,
    mvt_socket_connect,
    mvt_socket_read,
    mvt_socket_write,
    mvt_socket_shutdown,
    mvt_socket_resize
};

mvt_session_t *
mvt_socket_open (char **args, mvt_session_t *source, int width, int height)
{
    mvt_socket_t *sock = malloc(sizeof (mvt_socket_t));
    MVT_DEBUG_PRINT1("mvt_socket_open\n");
    if (sock == NULL)
        return NULL;
    memset(sock, 0, sizeof *sock);
    sock->parent.vt = &mvt_socket_vt;
    sock->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (
#ifdef WIN32
    sock->sock == INVALID_SOCKET
#else
    sock->sock == -1
#endif
    ) {
        free(sock);
        return NULL;
    }
	sock->port = -1;
	while (*args) {
		const char *name, *value;
		name = *args++;
		value = *args++;
		if (value == NULL) value = "";
		if (strcmp(name, "hostname") == 0) {
            sock->hostname = strdup(value);
		} else if (strcmp(name, "port") == 0) {
			sock->port = atoi(value);
		}
	}
    if (sock->port < 0) {
        free(sock);
        return NULL;
    }
    return &sock->parent;
}

static void
mvt_socket_close(mvt_session_t *session)
{
    mvt_socket_t *socket = (mvt_socket_t *)session;
#ifdef WIN32
    closesocket(socket->sock);
    socket->sock = INVALID_SOCKET;
#else
    close(socket->sock);
    socket->sock = -1;
#endif
}

static int
mvt_socket_connect (mvt_session_t *session)
{
    mvt_socket_t *socket = (mvt_socket_t *)session;
    struct hostent* hostent;
    struct sockaddr_in addr;

    MVT_DEBUG_PRINT3("mvt_socket_connect(%s,%d)\n", socket->hostname, socket->port);

    hostent = gethostbyname(socket->hostname);
    if (hostent == NULL) return -1;
    
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((short)socket->port);
    memcpy(&addr.sin_addr, hostent->h_addr_list[0], hostent->h_length);
    if (connect(socket->sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
#ifdef WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#endif
        return -1;
    }

    return 1;
}

static int
mvt_socket_read (mvt_session_t *session, void *buf, size_t count, size_t *countread)
{
    mvt_socket_t *socket = (mvt_socket_t *)session;
    int ret;

    ret = recv(socket->sock, buf, count, 0);
    MVT_DEBUG_PRINT2("mvt_socket_notify_socket_read: ret=%d\n", ret);
    if (ret == -1) {
#ifdef WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
        if (errno == EAGAIN) return 0;
#endif
        MVT_DEBUG_PRINT2("mvt_socket_notify_socket_read: error=%d\n", ret);
        return -1;
    }
    if (ret == 0) {
        /* End of stream */
        return -1;
    }
    *countread = ret;
    return 0;
}

static int
mvt_socket_write (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten)
{
    mvt_socket_t *socket = (mvt_socket_t *)session;
    int ret;

    ret = send(socket->sock, buf, count, 0);
    if (ret < 0)
        return -1;
    if (ret == 0)
        return -1;
    *countwritten = ret;
    return 0;
}

static void
mvt_socket_shutdown (mvt_session_t *session)
{
}

static void
mvt_socket_resize (mvt_session_t *session, int width, int height)
{
}
