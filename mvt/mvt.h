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

#ifndef MVT_H
#define MVT_H

#include <stdio.h>
#ifdef __GNUC__
#include <stdint.h>
#else
#ifdef WIN32
typedef unsigned __int32 uint32_t;
#elif WINAPI_FAMILY == WINAPI_FAMILY_APP
#include <stdint.h>
#else
#error "Don't know how to define uint32_t"
#endif
#endif

#ifdef  __cplusplus
# define MVT_BEGIN_DECLS  extern "C" {
# define MVT_END_DECLS    }
#else
# define MVT_BEGIN_DECLS
# define MVT_END_DECLS
#endif

MVT_BEGIN_DECLS

#define MVT_MAJOR_VERSION 0
#define MVT_MINOR_VERSION 2
#define MVT_MICRO_VERSION 0

typedef struct _mvt_screen mvt_screen_t;
typedef struct _mvt_session mvt_session_t;
typedef struct _mvt_terminal mvt_terminal_t;
typedef uint32_t mvt_char_t;

extern const int mvt_major_version;
extern const int mvt_minor_version;
extern const int mvt_micro_version;

/* These value for modifiers are the same as that of xterm's mouse
   tracking mode */
#define MVT_MOD_SHIFT      4
#define MVT_MOD_META       8
#define MVT_MOD_CONTROL   16

/* screen */
void mvt_screen_set_user_data(mvt_screen_t *screen, void *data);
void *mvt_screen_get_user_data(const mvt_screen_t *screen);

/* terminal */
mvt_terminal_t *mvt_terminal_new(int width, int height, int save_height);
void mvt_terminal_delete(mvt_terminal_t *terminal);
void mvt_terminal_set_user_data(mvt_terminal_t *terminal, void *data);
void *mvt_terminal_get_user_data(const mvt_terminal_t *terminal);
int mvt_terminal_set_screen(mvt_terminal_t *terminal, mvt_screen_t *screen);
mvt_screen_t *mvt_terminal_get_screen(const mvt_terminal_t *terminal);
int mvt_terminal_set_title(mvt_terminal_t *terminal, const mvt_char_t *ws);
void mvt_terminal_paint(const mvt_terminal_t *terminal, void *gc, int x1, int y1, int x2, int y2);
size_t mvt_terminal_read(mvt_terminal_t *terminal, mvt_char_t *ws, size_t count);
size_t mvt_terminal_write(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t count);
int mvt_terminal_append_input(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t count);
int mvt_terminal_read_ready(const mvt_terminal_t *terminal);
#define mvt_terminal_set_echo(terminal, value) (mvt_set_flag(&(terminal)->flags, MVT_TERMINAL_FLAG_ECHO, value))
#define mvt_terminal_get_echo(terminal) ((terminal)->flags | MVT_TERMINAL_FLAG_ECHO)
void mvt_terminal_get_size(const mvt_terminal_t *terminal, int *width, int *height);
int mvt_terminal_resize(mvt_terminal_t *terminal);
int mvt_terminal_paste(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t count);
int mvt_terminal_copy_selection(const mvt_terminal_t *terminal, mvt_char_t *buf, size_t count, int nl);
void mvt_terminal_repaint(const mvt_terminal_t *terminal);
int mvt_terminal_keydown(mvt_terminal_t *terminal, int meta, int code);
int mvt_terminal_mousebutton(mvt_terminal_t *terminal, int down, int button, uint32_t mod, int x, int y, int align);
int mvt_terminal_mousemove(mvt_terminal_t *terminal, int x, int y, int align);

/* mvt_session_t */
void mvt_session_close(mvt_session_t *session);
int mvt_session_connect(mvt_session_t *session);
int mvt_session_read(mvt_session_t *session, void *buf, size_t count, size_t *countread);
int mvt_session_write(mvt_session_t *session, const void *buf, size_t count, size_t *countwrite);
void mvt_session_shutdown(mvt_session_t *session);
void mvt_session_resize(mvt_session_t *session, int width, int height);

/* mvt_session_t */
mvt_session_t *mvt_socket_open(char **args, mvt_session_t *source, int width, int height);
mvt_session_t *mvt_telnet_open(char **args, mvt_session_t *source, int width, int height);
mvt_session_t *mvt_pty_open(char **args, mvt_session_t *source, int width, int height);
mvt_session_t *mvt_pipe_open (void);
void *mvt_pipe_lock_in(mvt_session_t *session, size_t count);
void mvt_pipe_unlock_in(mvt_session_t *session, size_t count);
void *mvt_pipe_lock_out(mvt_session_t *session, size_t *count);
void mvt_pipe_unlock_out(mvt_session_t *session, size_t count);

/* mvt_iconv_t */
#define MVT_E2BIG -1
#define MVT_EINVAL -2
#define MVT_EILSEQ -3
typedef void *mvt_iconv_t;
mvt_iconv_t mvt_iconv_open(int utf8_to_ucs4);
int mvt_iconv(mvt_iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
void mvt_iconv_close(mvt_iconv_t cd);

/* mvt_driver_t */
#define MVT_EVENT_TYPE_KEY 1
#define MVT_EVENT_TYPE_RESIZE 2
#define MVT_EVENT_TYPE_DATA 3
#define MVT_EVENT_TYPE_CLOSE 4
typedef int (*mvt_event_func_t)(void *data, int type, int param1, int param2);

int mvt_init(int *argc, char ***argv, mvt_event_func_t event_func);
void mvt_main(void);
void mvt_main_quit(void);
void mvt_exit(void);
int mvt_register_default_plugins(void);
mvt_screen_t *mvt_open_screen(const char *spec);
void mvt_close_screen(mvt_screen_t *screen);
mvt_terminal_t *mvt_open_terminal(const char *spec);
void mvt_close_terminal(mvt_terminal_t *terminal);
int mvt_set_screen_attribute(mvt_screen_t *screen, const char *name, const char *value);
int mvt_set_terminal_attribute(mvt_terminal_t *terminal, const char *name, const char *value);
int mvt_attach(mvt_terminal_t *terminal, mvt_screen_t *screen);
int mvt_open(mvt_terminal_t *terminal, const char *spec);
int mvt_connect(mvt_terminal_t *terminal);
void mvt_suspend(mvt_terminal_t *terminal);
void mvt_resume(mvt_terminal_t *terminal);
void mvt_shutdown(mvt_terminal_t *terminal);

MVT_END_DECLS

#endif
