/* Multi-purpose Virtual Terminal
 * Copyright (C) 2005-2010 Katsuya Iida
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <mvt/mvt.h>
#include "private.h"
#include "debug.h"

/* pty */

typedef struct _mvt_pty mvt_pty_t;
struct _mvt_pty {
    mvt_session_t parent;
    const char *terminal_type;
    pid_t pid;
    int fd;
};

static void mvt_pty_close(mvt_session_t *session);
static int mvt_pty_connect(mvt_session_t *session);
static int mvt_pty_read(mvt_session_t *session, void *buf, size_t count, size_t *countread);
static int mvt_pty_write(mvt_session_t *session, const void *buf, size_t count, size_t *countwritten);
static void mvt_pty_shutdown(mvt_session_t *session);
static void mvt_pty_resize(mvt_session_t *session, int columns, int rows);

static const mvt_session_vt_t mvt_pty_vt = {
    mvt_pty_close,
    mvt_pty_connect,
    mvt_pty_read,
    mvt_pty_write,
    mvt_pty_shutdown,
    mvt_pty_resize
};

mvt_session_t *
mvt_pty_open (char **args, mvt_session_t *source, int width, int height)
{
    mvt_pty_t *pty;

    MVT_DEBUG_PRINT1("mvt_pty_open\n");
    
    pty = malloc(sizeof (mvt_pty_t));
    if (pty == NULL)
        return NULL;
    memset(pty, 0, sizeof *pty);
    pty->parent.vt = &mvt_pty_vt;
    pty->terminal_type = "xterm";
	return (mvt_session_t *)pty;
}

static void
mvt_pty_close (mvt_session_t *session)
{
    mvt_pty_t *pty = (mvt_pty_t *)session;
    if (pty->pid != 0)
        mvt_pty_shutdown(session);
    close(pty->fd);
    free(pty);
}

static int
mvt_pty_connect (mvt_session_t *session)
{
    mvt_pty_t *pty = (mvt_pty_t *)session;
    int ptm, pts, pid, fd, f;
    char *argv[3];
    char *shell;

    MVT_DEBUG_PRINT1("mvt_pty_connect\n");
    
#if 0
    ptm = open("/dev/ptmx", O_RDWR | O_NONBLOCK);
#endif
    ptm = open("/dev/ptmx", O_RDWR);
    if (ptm < 0) {
        return -1; 
    }
    pid = fork();
    if (pid < 0) {
        close(ptm);
		return -1;
    }
    if (pid == 0) {
        if (grantpt(ptm) < 0)
            exit(1);
        if (unlockpt(ptm) < 0)
            exit(1);
        pts = open(ptsname(ptm), O_RDWR);
        if (pts < 0)
            exit(1);
        close(ptm);
        for (fd = 0; fd < 3; fd++) {
            close(fd);
            dup(pts);
            fcntl(fd, F_SETFD, 0);
	}
        pid = setsid();
        tcsetpgrp(0, pid);
        setenv("TERM", pty->terminal_type, TRUE);
        unsetenv("LINES");
        unsetenv("COLUMNS");
        unsetenv("TERMCAP");
        shell = getenv("SHELL");
        if (shell == NULL) shell = "/bin/sh";
        argv[0] = shell;
        argv[1] = NULL;
        execv (shell, argv);
        exit (1);
    }
    f = fcntl(ptm, F_GETFD);
    fcntl(ptm, F_SETFD, f | FD_CLOEXEC);
    pty->fd = ptm;
    pty->pid = pid;
    return 1;
}

static int
mvt_pty_read(mvt_session_t *session, void *buf, size_t count, size_t *countread)
{
    mvt_pty_t *pty = (mvt_pty_t *)session;
    ssize_t n;
    n = read(pty->fd, buf, count);
    if (n <= 0)
        return -1;
	*countread = n;
    return 0;
}

static int
mvt_pty_write(mvt_session_t *session, const void *buf, size_t count, size_t *countwritten)
{
    mvt_pty_t *pty = (mvt_pty_t *)session;
    ssize_t n;
    n = write(pty->fd, buf, count);
    if (n <= 0)
        return -1;
	*countwritten = n;
    return 0;
}

static void mvt_pty_shutdown(mvt_session_t *session)
{
    int status;
    mvt_pty_t *pty = (mvt_pty_t *)session;
    kill(pty->pid, SIGKILL);
    waitpid(pty->pid, &status, 0);
    MVT_DEBUG_PRINT2("The child process %d was killed.\n", pty->pid);
    pty->pid = 0;
}

static void mvt_pty_resize(mvt_session_t *session, int width, int height)
{
    mvt_pty_t *pty = (mvt_pty_t *)session;
    struct winsize ws;
    MVT_DEBUG_PRINT3("mvt_pty_resize: %d,%d\n", width, height);
    ws.ws_col = (unsigned short)width;
    ws.ws_row = (unsigned short)height;
    ws.ws_xpixel = ws.ws_ypixel = 0;
    ioctl(pty->fd, TIOCSWINSZ, &ws);
}
