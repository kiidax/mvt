/* Multi-purpose Virtual Terminal
 * Copyright (C) 2005-2010,2012 Katsuya Iida
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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif
#ifdef HAVE_SDL
#include <SDL.h>
#endif
#ifdef HAVE_ICONV
#include <iconv.h>
#include <errno.h>
#endif
#ifdef HAVE_LIBICONV
#include <iconv.h>
#include <errno.h>
#endif
#ifdef HAVE_WIN32
#include <windows.h>
#endif
#include <assert.h>
#include <mvt/mvt.h>
#include "misc.h"
#include "debug.h"
#include "driver.h"
#include "private.h"

#define PTHREAD_BYTEORDER PTHREAD_LIL_ENDIAN
#define PTHREAD_LIL_ENDIAN 0
#define PTHREAD_BIG_ENDIAN 1

#define MVT_READ_BUFFER_SIZE 4096
#define MVT_WRITE_BUFFER_SIZE 4096
#define MVT_MAX_SESSIONS 3

typedef struct _mvt_worker_request mvt_worker_request_t;
typedef struct _mvt_worker mvt_worker_t;

typedef enum {
    MVT_WORKER_WRITE,
    MVT_WORKER_READ,
    MVT_WORKER_CLOSE
} mvt_worker_request_type_t;

struct _mvt_worker_request {
    mvt_worker_request_type_t type;
    mvt_worker_request_t *next;
    mvt_worker_t *worker;
    mvt_char_t *ws;
    size_t count;
    size_t result;
    int resized;
};

/* This object is accessed by threads */
struct _mvt_worker {
    mvt_terminal_t *terminal;
    mvt_session_t *session_list[MVT_MAX_SESSIONS];
    int last_session;
#ifdef HAVE_PTHREAD
    pthread_t input_thread;
    pthread_t output_thread;
    pthread_cond_t write_cond;
    pthread_cond_t read_cond;
#endif
#ifdef HAVE_SDL
    SDL_Thread *input_thread;
    SDL_Thread *output_thread;
    SDL_cond *write_cond;
    SDL_cond *read_cond;
#endif
#ifdef HAVE_WIN32_THREAD
    HANDLE input_tread;
    HANDLE output_thread;
    HANDLE write_cond;
    HANDLE read_cond;
#endif
    unsigned int resized : 1;
    unsigned int active : 1;
    mvt_worker_request_t *pending_read_message;
};

#ifdef HAVE_PTHREAD
static pthread_mutex_t global_mutex;
#endif
#ifdef HAVE_SDL
static SDL_mutex *global_mutex;
#endif
#ifdef HAVE_WIN32_THREAD
static HANDLE global_mutex;
#endif
static mvt_worker_t *shutdown_terminal;
static mvt_worker_request_t *message_queue;
static mvt_worker_request_t **message_last;
static mvt_event_func_t global_event_func = NULL;

static void mvt_worker_response_read(mvt_worker_request_t *message);
static void mvt_worker_response_write(mvt_worker_request_t *message);
static void mvt_worker_response_close(mvt_worker_request_t *message);
static size_t mvt_worker_read(mvt_worker_t *worker, mvt_char_t *ws, size_t count, int *resized);
static size_t mvt_worker_write(mvt_worker_t *worker, const void *ws, size_t count);
static void mvt_worker_close(mvt_worker_t *worker);

#ifdef HAVE_PTHREAD
#define mvt_mutex_lock(mutex) pthread_mutex_lock(mutex)
#define mvt_mutex_unlock(mutex) pthread_mutex_unlock(mutex)
#define mvt_cond_signal(cond) pthread_cond_signal(cond)
#define mvt_cond_wait(cond, mutex) pthread_cond_wait(cond, mutex)
#endif
#ifdef HAVE_SDL
#define mvt_mutex_lock(mutex) SDL_mutexP(*(mutex))
#define mvt_mutex_unlock(mutex) SDL_mutexV(*(mutex))
#define mvt_cond_signal(cond) SDL_CondSignal(*(cond))
#define mvt_cond_wait(cond, mutex) SDL_CondWait(*(cond), *(mutex))
#endif

static int mvt_worker_send_request(mvt_worker_request_t *message)
{
    mvt_mutex_lock(&global_mutex);
    if (message->worker == shutdown_terminal) {
        mvt_mutex_unlock(&global_mutex);
        return -1;
    }
    *message_last = message;
    message_last = &message->next;
    message->next = NULL;
    if (message_queue == message)
        mvt_notify_request();
    switch (message->type) {
    case MVT_WORKER_WRITE:
    case MVT_WORKER_CLOSE:
        mvt_cond_wait(&message->worker->write_cond, &global_mutex);
        break;
    case MVT_WORKER_READ:
        mvt_cond_wait(&message->worker->read_cond, &global_mutex);
        break;
    }
    mvt_mutex_unlock(&global_mutex);
    return 0;
}

static mvt_worker_request_t *mvt_worker_get_request(mvt_worker_t *worker)
{
    mvt_worker_request_t **message, *result_message;
    mvt_mutex_lock(&global_mutex);
    message = &message_queue;
    if (worker) {
        while (*message) {
            if ((*message)->worker == worker)
                break;
            message = &(*message)->next;
        }
    }
    result_message = *message;
    if (*message) {
        *message = (*message)->next;
        if (!(*message))
            message_last = message;
    }
    mvt_mutex_unlock(&global_mutex);
    return result_message;
}

static void mvt_worker_data_ready(mvt_terminal_t *terminal)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    mvt_worker_request_t *message;
    if (!mvt_terminal_read_ready(worker->terminal))
        return;
    if (!worker->active) {
        (*global_event_func)(worker->terminal, MVT_EVENT_TYPE_DATA, 0, 0);
        return;
    }
    message = worker->pending_read_message;
    if (message) {
        worker->pending_read_message = NULL;
        mvt_worker_response_read(message);
    }
}

static void mvt_worker_response_read(mvt_worker_request_t *message)
{
    mvt_worker_t *worker;

    MVT_DEBUG_PRINT1("mvt_worker_response_read\n");
    worker = message->worker;
    /* ensure that worker_output() waits */
    if (worker->resized) {
        worker->resized = FALSE;
        message->result = 0;
        message->resized = TRUE;
        mvt_cond_signal(&worker->read_cond);
        return;
    }
    message->result = mvt_terminal_read(worker->terminal,
                                        message->ws,
                                        message->count);
    message->resized = FALSE;
    if (message->result > 0) {
        mvt_cond_signal(&worker->read_cond);
        return;
    }
    /* No data available here. Process the message when data are
     * ready. */
    worker->pending_read_message = message;
}

static void mvt_worker_response_write(mvt_worker_request_t *message)
{
    mvt_worker_t *worker;
    MVT_DEBUG_PRINT1("mvt_worker_response_write\n");
    worker = message->worker;
    /* ensure that worker_input() waits */
    message->result = mvt_terminal_write(worker->terminal,
                                         message->ws,
                                         message->count);
    mvt_cond_signal(&worker->write_cond);
}

static void mvt_worker_response_close(mvt_worker_request_t *message)
{
    mvt_worker_t *worker = message->worker;
    mvt_cond_signal(&worker->write_cond);
    mvt_shutdown(worker->terminal);
    (void)(*global_event_func)(worker->terminal, MVT_EVENT_TYPE_CLOSE, 0, 0);
}

void mvt_handle_request(void)
{
    mvt_worker_request_t *message;
    for (;;) {
        message = mvt_worker_get_request(NULL);
        if (!message)
            break;
        switch (message->type) {
        case MVT_WORKER_READ:
            mvt_worker_response_read(message);
            break;
        case MVT_WORKER_WRITE:
            mvt_worker_response_write(message);
            break;
        case MVT_WORKER_CLOSE:
            mvt_worker_response_close(message);
            break;
        }
    }
}

mvt_terminal_t *mvt_worker_open_terminal(char **args)
{
    mvt_worker_t *worker = malloc(sizeof (mvt_worker_t));
    int width, height, save_lines;
    char **p;
    const char *name, *value;
    if (worker == NULL)
        return NULL;
    memset(worker, 0, sizeof *worker);
    width = 80;
    height = 24;
    save_lines = 64;
    p = args;
    while (*p) {
        name = *p++;
        if (!*p) {
            free(worker);
            return NULL;
        }
        value = *p++;
        if (strcmp(name, "width") == 0)
            width = atoi(value);
        else if (strcmp(name, "height") == 0)
            height = atoi(value);
        else if (strcmp(name, "save-lines") == 0)
            save_lines = atoi(value);
    }
    worker->active = FALSE;
    worker->last_session = -1;
    worker->terminal = mvt_terminal_new(width, height, save_lines);
    mvt_terminal_set_driver_data(worker->terminal, worker);
#ifdef HAVE_PTHREAD
    pthread_cond_init(&worker->read_cond, NULL);
    pthread_cond_init(&worker->write_cond, NULL);
#endif
#ifdef HAVE_SDL
    worker->read_cond = SDL_CreateCond();
    worker->write_cond = SDL_CreateCond();
#endif
    return worker->terminal;
}

void mvt_worker_close_terminal(mvt_terminal_t *terminal)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    mvt_screen_t *screen = mvt_terminal_get_screen(terminal);
    if (screen != NULL)
        mvt_screen_set_driver_data(screen, NULL);
    mvt_shutdown(terminal);
    mvt_terminal_delete(terminal);
#ifdef HAVE_PTHREAD
    pthread_cond_destroy(&worker->read_cond);
    pthread_cond_destroy(&worker->write_cond);
#endif
#ifdef HAVE_SDL
    SDL_DestroyCond(worker->read_cond);
    SDL_DestroyCond(worker->write_cond);
#endif
    free(worker);
}

static size_t mvt_worker_read(mvt_worker_t *worker, mvt_char_t *ws, size_t count, int *resized)
{
    mvt_worker_request_t message;
    size_t result;
    message.type = MVT_WORKER_READ;
    message.worker = worker;
    message.ws = ws;
    message.count = count;
    if (mvt_worker_send_request(&message) == -1) {
        *resized = FALSE;
        return 0;
    }
    result = message.result;
    *resized = message.resized;
    return result;
}

static size_t mvt_worker_write(mvt_worker_t *worker, const void *ws, size_t count)
{
    mvt_worker_request_t message;
    size_t result;
    message.type = MVT_WORKER_WRITE;
    message.worker = worker;
    message.ws = (mvt_char_t *)ws;
    message.count = count;
    if (mvt_worker_send_request(&message) == -1)
        return 0;
    result = message.result;
    return result;
}

static void mvt_worker_close(mvt_worker_t *worker)
{
    mvt_worker_request_t message;
    message.type = MVT_WORKER_CLOSE;
    message.worker = worker;
    message.ws = NULL;
    message.count = 0;
    (void)mvt_worker_send_request(&message);
}

int mvt_worker_set_terminal_attribute(mvt_terminal_t *terminal, const char *name, const char *value)
{
    return 0;
}

void mvt_screen_dispatch_resize(mvt_screen_t *screen)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    mvt_terminal_resize(worker->terminal);
    mvt_mutex_lock(&global_mutex);
    worker->resized = TRUE;
    mvt_mutex_unlock(&global_mutex);
    mvt_worker_data_ready(worker->terminal);
    mvt_terminal_repaint(worker->terminal);
}

static int worker_input(void *data)
{
    mvt_terminal_t *terminal = (mvt_terminal_t *)data;
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    char buf[MVT_READ_BUFFER_SIZE];
    char wbuf[MVT_READ_BUFFER_SIZE];
    char *s, *ws;
    size_t count, wcount, n;
    iconv_t cd;
    int need_read;
#if PTHREAD_BYTEORDER == PTHREAD_LIL_ENDIAN
    cd = iconv_open("UCS-4LE", "UTF-8");
#endif
#if PTHREAD_BYTEORDER == PTHREAD_BIG_ENDIAN
    cd = iconv_open("UCS-4BE", "UTF-8");
#endif
    if (cd == (iconv_t)-1)
        return -1;
    s = buf;
    count = 0;
    ws = wbuf;
    wcount = MVT_READ_BUFFER_SIZE;
    need_read = TRUE;
    for (;;) {
        if (need_read) {
            if (mvt_session_read(worker->session_list[worker->last_session], s, MVT_READ_BUFFER_SIZE - count, &n) < 0)
                break;
            s = buf;
            count += n;
        }
        for (;;) {
            n = iconv(cd, &s, &count, (char **)&ws, &wcount);
            if (n != (size_t)-1) {
                need_read = TRUE;
                assert(count == 0);
                s = buf;
                count = 0;
                break;
            } else if (errno == E2BIG) {
                need_read = FALSE;
                break;
            } else if (errno == EINVAL) {
                assert(count > 0);
                need_read = TRUE;
                memmove(buf, s, count);
                s = buf + count;
                break;
            }
            assert(count > 0);
            assert(errno == EILSEQ);
            /* invalid sequence */
            s++;
            count--;
        }
        if (ws > wbuf) {
#if 0
            {
                size_t i, count;
                count = ws - wbuf;
                for (i = 0; i < count; i++) {
                    if (i % 8 == 0) {
                        printf("%04x:", (int)i);
                    }
                    printf(" %02x", (int)wbuf[i]);
                    if (i % 8 == 7) {
                        printf("\n");
                    }
                }
            }
#endif
            mvt_worker_write(worker, (mvt_char_t *)wbuf, (ws - wbuf) / sizeof (mvt_char_t));
            /* We don't have to copy wbuf as iconv converts one
             * character at a time */
            ws = wbuf;
            wcount = MVT_READ_BUFFER_SIZE;
        }
    }
    iconv_close(cd);
    mvt_worker_close(worker);
    return 0;
}

static int worker_output(void *data)
{
    mvt_terminal_t *terminal = (mvt_terminal_t *)data;
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    char buf[MVT_WRITE_BUFFER_SIZE];
    char wbuf[MVT_WRITE_BUFFER_SIZE];
    char *s, *ws;
    size_t count, wcount, n;
    iconv_t cd;
    int need_read;
    int resized;
    int width, height;

#if PTHREAD_BYTEORDER == PTHREAD_LIL_ENDIAN
    cd = iconv_open("UTF-8", "UCS-4LE");
#endif
#if PTHREAD_BYTEORDER == PTHREAD_BIG_ENDIAN
    cd = iconv_open("UTF-8", "UCS-4BE");
#endif
    s = buf;
    count = MVT_READ_BUFFER_SIZE;
    ws = wbuf;
    wcount = 0;
    need_read = TRUE;
    if (cd == (iconv_t)-1)
        return -1;
    for (;;) {
        if (need_read) {
            n = mvt_worker_read(worker, (mvt_char_t *)ws, (MVT_WRITE_BUFFER_SIZE - wcount) / sizeof (mvt_char_t), &resized);
            if (n == 0) {
                if (resized) {
                    mvt_terminal_get_size(worker->terminal, &width, &height);
                    mvt_session_resize(worker->session_list[worker->last_session], width, height);
                    continue;
                } else {
                    break;
                }
            }
            ws = wbuf;
            wcount += n * sizeof (mvt_char_t);
        }
        for (;;) {
            n = iconv(cd, &ws, &wcount, &s, &count);
            if (n != (size_t)-1) {
                need_read = TRUE;
                assert(wcount == 0);
                ws = wbuf;
                wcount = 0;
                break;
            } else if (errno == E2BIG) {
                need_read = FALSE;
                break;
            } else if (errno == EINVAL) {
                assert(wcount > 0);
                need_read = TRUE;
                memmove(wbuf, ws, wcount);
                ws = wbuf + wcount;
                break;
            }
            assert(wcount > 0);
            ws++;
            wcount--;
        }
        if (s > buf) {
            const char *p = buf;
            while (p < s) {
                if (mvt_session_write(worker->session_list[worker->last_session], p, s - p, &n) < 0)
                    return 0;
                p += n;
            }
            s = buf;
            count = MVT_READ_BUFFER_SIZE;
        }
    }
    iconv_close(cd);
    return 0;
}

#ifdef HAVE_PTHREAD
static void *pthread_worker_input(void *data)
{
    worker_input(data);
    return NULL;
}

static void *pthread_worker_output(void *data)
{
    worker_output(data);
    return NULL;
}
#endif

int
mvt_open (mvt_terminal_t *terminal, const char *spec)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
	int width, height;
    mvt_session_t *source;

    mvt_terminal_get_size(terminal, &width, &height);
    if (worker->last_session == -1)
        source = NULL;
    else
        source = worker->session_list[worker->last_session];
    if (worker->last_session + 1 == MVT_MAX_SESSIONS)
        return -1;
    worker->session_list[worker->last_session + 1] = mvt_session_open(spec, source, width, height);
    if (worker->session_list[worker->last_session + 1] == NULL)
        return -1;
    worker->last_session++;

    return 0;
}

int
mvt_connect (mvt_terminal_t *terminal)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);

    if (worker->last_session == -1)
        return -1;
    mvt_session_connect(worker->session_list[worker->last_session]);
    worker->active = TRUE;
#ifdef HAVE_SDL
    assert(!worker->input_thread);
    assert(!worker->output_thread);
    worker->input_thread = SDL_CreateThread(worker_input, terminal);
    worker->output_thread = SDL_CreateThread(worker_output, terminal);
#endif
#ifdef HAVE_PTHREAD
    if (pthread_create(&worker->input_thread, NULL, pthread_worker_input, terminal) != 0)
        return -1;
    if (pthread_create(&worker->output_thread, NULL, pthread_worker_output, terminal) != 0)
        return -1;
#endif
    return 0;
}

int mvt_attach(mvt_terminal_t *terminal, mvt_screen_t *screen)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (worker) {
        if (mvt_terminal_set_screen(worker->terminal, NULL) == -1)
            return -1;
    }
    worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    mvt_screen_set_driver_data(screen, worker);
    if (!terminal)
        return 0;
    if (mvt_terminal_set_screen(terminal, screen) == -1)
        return -1;
    mvt_screen_dispatch_resize(screen);
    return 0;
}

void mvt_worker_suspend(mvt_terminal_t *terminal)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    mvt_mutex_lock(&global_mutex);
    if (worker->last_session != -1)
        worker->active = FALSE;
    mvt_mutex_unlock(&global_mutex);
}

void mvt_worker_resume(mvt_terminal_t *terminal)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    mvt_mutex_lock(&global_mutex);
    if (worker->last_session != -1)
        worker->active = TRUE;
    mvt_mutex_unlock(&global_mutex);
}

void mvt_worker_shutdown(mvt_terminal_t *terminal)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_terminal_get_driver_data(terminal);
    mvt_worker_request_t *message;
    int i;
#ifdef HAVE_PTHREAD
    void *status;
#endif
#ifdef HAVE_SDL
    int status;
#endif
    MVT_DEBUG_PRINT1("mvt_worker_shutdown\n");
    if (worker->last_session == -1)
        return;
    mvt_mutex_lock(&global_mutex);
    shutdown_terminal = worker;
    mvt_mutex_unlock(&global_mutex);
    mvt_session_shutdown(worker->session_list[worker->last_session]);
    message = worker->pending_read_message;
    if (message) {
        message->result = 0;
        message->resized = FALSE;
        mvt_cond_signal(&worker->read_cond);
    }
    for (;;) {
        message = mvt_worker_get_request(worker);
        if (!message)
            break;
        switch (message->type) {
        case MVT_WORKER_READ:
            message->result = 0;
            message->resized = FALSE;
            mvt_cond_signal(&worker->read_cond);
            break;
        case MVT_WORKER_WRITE:
            message->result = 0;
            mvt_cond_signal(&worker->write_cond);
            break;
        case MVT_WORKER_CLOSE:
            break;
        }
    }
#ifdef HAVE_PTHREAD
    pthread_join(worker->input_thread, &status);
    pthread_join(worker->output_thread, &status);
#endif
#ifdef HAVE_SDL
    SDL_WaitThread(worker->input_thread, &status);
    SDL_WaitThread(worker->output_thread, &status);
#endif

    for (i = worker->last_session; i >= 0; i--)
        mvt_session_close(worker->session_list[i]);
    worker->active = FALSE;
    worker->last_session = -1;
    mvt_mutex_lock(&global_mutex);
    shutdown_terminal = NULL;
    mvt_mutex_unlock(&global_mutex);
}

void mvt_worker_init(mvt_event_func_t event_func)
{
#ifdef HAVE_PTHREAD
    pthread_mutex_init(&global_mutex, NULL);
#endif
#ifdef HAVE_SDL
    global_mutex = SDL_CreateMutex();
#endif
    message_queue = NULL;
    message_last = &message_queue;
    global_event_func = event_func;
    shutdown_terminal = NULL;
}

void mvt_worker_exit(void)
{
#ifdef HAVE_PTHREAD
    pthread_mutex_destroy(&global_mutex);
#endif
#ifdef HAVE_SDL
    SDL_DestroyMutex(global_mutex);
#endif
}

/*! \addtogroup Screen
 * @{
 */

/**
 * @typedef mvt_screen_t
 * Screen
 **/

void mvt_screen_dispatch_repaint(const mvt_screen_t *screen)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    mvt_terminal_repaint(worker->terminal);
}

/*! 
 * Paint the specified area.
 * @param screen screen
 * @param gc graphics context
 * @param x1 virtual left-most position
 * @param y1 virtual top-most position
 * @param x2 virtual right-most position
 * @param y2 virtual bottom-most position
 **/
void mvt_screen_dispatch_paint(const mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    mvt_terminal_paint(worker->terminal, gc, x1, y1, x2, y2);
}

void mvt_screen_dispatch_close(mvt_screen_t *screen)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    (void)mvt_terminal_set_screen(worker->terminal, NULL);
}

void mvt_screen_dispatch_keydown(mvt_screen_t *screen, int meta, int code)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if ((*global_event_func)(screen, MVT_EVENT_TYPE_KEY, code, 0) == -1) {
        worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
        if (!worker)
            return;
        if (mvt_terminal_keydown(worker->terminal, meta, code) == -1)
            return;
        mvt_worker_data_ready(worker->terminal);
        return;
    }
    MVT_DEBUG_PRINT2("mvt_screen_dispatch_keydown: %d\n", code);
    worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    mvt_worker_data_ready(worker->terminal);
}

void mvt_screen_dispatch_mousebutton(mvt_screen_t *screen, int down, int button, uint32_t mod, int x, int y, int align)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    if (mvt_terminal_mousebutton(worker->terminal, down, button, mod, x, y, align) == -1)
        return;
    mvt_worker_data_ready(worker->terminal);
}

void mvt_screen_dispatch_paste(mvt_screen_t *screen, const mvt_char_t *ws, size_t count)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    if (mvt_terminal_paste(worker->terminal, ws, count) == -1)
        return;
    mvt_worker_data_ready(worker->terminal);
}

void mvt_screen_dispatch_mousemove(mvt_screen_t *screen, int x, int y, int align)
{
    mvt_worker_t *worker = (mvt_worker_t *)mvt_screen_get_driver_data(screen);
    if (!worker)
        return;
    if (mvt_terminal_mousemove(worker->terminal, x, y, align) == -1)
        return;
    mvt_worker_data_ready(worker->terminal);
}

typedef struct _mvt_session_plugin_list mvt_session_plugin_list_t;
struct _mvt_session_plugin_list {
	mvt_session_plugin_t plugin;
	char *proto;
	mvt_session_plugin_list_t *next;
};

static mvt_session_plugin_list_t *first_plugin;

static const char *default_plugin_proto_list[] = {
    "socket",
#ifdef ENABLE_PTY
    "pty",
#endif
#ifdef ENABLE_TELNET
	"telnet",
#endif
    NULL
};

static const mvt_session_plugin_t default_plugin_list[] = {
    { mvt_socket_open },
#ifdef ENABLE_PTY
    { mvt_pty_open },
#endif
#ifdef ENABLE_TELNET
	{ mvt_telnet_open },
#endif
};

int
mvt_register_default_plugins (void)
{
    const mvt_session_plugin_t *plugin = default_plugin_list;
    const char **proto = default_plugin_proto_list;

    first_plugin = NULL;
    while (*proto != NULL) {
        if (mvt_register_session_plugin(*proto, plugin) == -1)
            return -1;
        proto++;
        plugin++;
    }
        
    return 0;
}

int
mvt_register_session_plugin (const char *proto, const mvt_session_plugin_t *plugin)
{
	mvt_session_plugin_list_t *list = malloc(sizeof (mvt_session_plugin_list_t));
	if (list == NULL)
		return -1;
	memset(list, 0, sizeof (mvt_session_plugin_list_t));
	list->proto = strdup(proto);
	if (list->proto == NULL) {
		free(list);
		return -1;
	}
	memcpy(&list->plugin, plugin, sizeof (mvt_session_plugin_t));
	list->next = first_plugin;
	first_plugin = list;
	return 0;
}

mvt_session_t *
mvt_session_open (const char *spec, mvt_session_t *source, int width, int height)
{
	mvt_session_plugin_list_t *list = first_plugin;
    mvt_session_t *session = NULL;
    char **args, *buf;
    const char *proto;
    if (mvt_parse_param(spec, 0, &args, &buf) == -1)
        return NULL;
#ifdef ENABLE_DEBUG
	do {
		char **p = args;
        printf("proto: %s\n", *p++);
		while (*p) {
			printf("\t%s=%s\n", *p, *(p+1));
			p += 2;
		}
    } while (FALSE);
#endif
	proto = *args;
	while (list != NULL) {
		if (strcmp(proto, list->proto) == 0) {
			session = (*list->plugin.open)(args + 1, source, width, height);
			break;
		}
		list = list->next;
	}
    free(args);
    free(buf);

    return session;
}

/** @} */
