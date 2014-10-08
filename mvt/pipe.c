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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MVT_PIPE_DEFAULT_DATA_SIZE 1024

typedef struct _mvt_pipe mvt_pipe_t;

struct _mvt_pipe {
    mvt_session_t parent;
    uint8_t *in_data;
    size_t in_length;
    size_t in_offset;
    uint8_t *out_data;
    size_t out_length;
    size_t out_offset;
};

static void mvt_pipe_close(mvt_session_t *session);
static int mvt_pipe_connect(mvt_session_t *session);
static int mvt_pipe_read (mvt_session_t *session, void *buf, size_t count, size_t *countread);
static int mvt_pipe_write (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten);
static void mvt_pipe_shutdown (mvt_session_t *session);
static void mvt_pipe_resize (mvt_session_t *session, int width, int height);

static const mvt_session_vt_t mvt_pipe_vt = {
    mvt_pipe_close,
    mvt_pipe_connect,
    mvt_pipe_read,
    mvt_pipe_write,
    mvt_pipe_shutdown,
    mvt_pipe_resize
};

mvt_session_t *
mvt_pipe_open (void)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)malloc(sizeof (mvt_pipe_t));
    MVT_DEBUG_PRINT1("mvt_pipe_open\n");
    if (pipe == NULL)
        return NULL;
    pipe->parent.vt = &mvt_pipe_vt;
    pipe->in_data = (uint8_t *)malloc(MVT_PIPE_DEFAULT_DATA_SIZE);
    if (pipe->in_data == NULL) {
        free(pipe);
        return NULL;
    }
    pipe->in_offset = 0;
    pipe->in_length = MVT_PIPE_DEFAULT_DATA_SIZE;
    pipe->out_data = (uint8_t *)malloc(MVT_PIPE_DEFAULT_DATA_SIZE);
    if (pipe->in_data == NULL) {
        free(pipe->in_data);
        free(pipe);
        return NULL;
    }
    pipe->out_offset = 0;
    pipe->out_length = MVT_PIPE_DEFAULT_DATA_SIZE;
    return &pipe->parent;
}

static void
mvt_pipe_close(mvt_session_t *session)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;

    free(pipe->in_data);
    free(pipe->out_data);
    free(pipe);
}

static int
mvt_pipe_connect (mvt_session_t *session)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;

    MVT_DEBUG_PRINT1("mvt_pipe_connect()\n");

    return 1;
}

static int
mvt_pipe_read (mvt_session_t *session, void *buf, size_t count, size_t *countread)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;
    size_t n;

    n = pipe->in_offset;
    if (n > count)
        n = count;
    if (n > 0) {
        memcpy(buf, pipe->in_data, n);
        if (n < pipe->in_offset)
            memmove(pipe->in_data, pipe->in_data + n, pipe->in_offset - n);
        pipe->in_offset -= n;
    }
    *countread = n;

    return 0;
}

static int
mvt_pipe_write (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;
    size_t n;

    n = pipe->out_length - pipe->out_offset;
    if (n > count)
        n = count;
    if (n > 0) {
        memcpy(pipe->out_data + pipe->out_offset, buf, n);
        pipe->out_offset += n;
    }
    *countwritten = n;
    return 0;
}

static void
mvt_pipe_shutdown (mvt_session_t *session)
{
}

static void
mvt_pipe_resize (mvt_session_t *session, int width, int height)
{
}

void *
mvt_pipe_lock_in (mvt_session_t *session, size_t count)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;

    if (pipe->in_offset + count > pipe->in_length) {
        uint8_t *data = (uint8_t *)realloc(pipe->in_data, pipe->in_offset + count);
        if (data == NULL)
            return NULL;
        pipe->in_data = data;
        pipe->in_length = pipe->in_offset + count;
    }
    return pipe->in_data + pipe->in_offset;
}

void
mvt_pipe_unlock_in (mvt_session_t *session, size_t count)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;

    if (pipe->in_offset + count > pipe->in_length)
        return;

    pipe->in_offset += count;
}

void *
mvt_pipe_lock_out (mvt_session_t *session, size_t *count)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;

    *count = pipe->out_offset;

    return pipe->out_data;
}

void
mvt_pipe_unlock_out (mvt_session_t *session, size_t count)
{
    mvt_pipe_t *pipe = (mvt_pipe_t *)session;

    assert(count <= pipe->out_offset);

    if (count > pipe->out_offset)
        return;

    if (count < pipe->out_offset) {
        memmove(pipe->out_data, pipe->out_data + count, pipe->out_offset - count);
    }
    pipe->out_offset -= count;
}

