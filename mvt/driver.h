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

#ifndef DRIVER_H
#define DRIVER_H

#include <mvt/mvt.h>

MVT_BEGIN_DECLS

typedef unsigned int mvt_color_t;
typedef struct _mvt_driver_vt mvt_driver_vt_t;
typedef struct _mvt_driver mvt_driver_t;
typedef struct _mvt_screen_vt mvt_screen_vt_t;
typedef struct _mvt_attribute mvt_attribute_t;

#define MVT_DEFAULT_COLOR 256
uint32_t mvt_color_value(mvt_color_t color);

enum {
    MVT_KEYPAD_SPACE = 0x100,
    MVT_KEYPAD_TAB,
    MVT_KEYPAD_ENTER,
    MVT_KEYPAD_PF1,
    MVT_KEYPAD_PF2,
    MVT_KEYPAD_PF3,
    MVT_KEYPAD_PF4,
    MVT_KEYPAD_HOME,
    MVT_KEYPAD_LEFT,
    MVT_KEYPAD_UP,
    MVT_KEYPAD_RIGHT,
    MVT_KEYPAD_DOWN,
    MVT_KEYPAD_PRIOR,
    MVT_KEYPAD_PAGEUP,
    MVT_KEYPAD_NEXT,
    MVT_KEYPAD_PAGEDOWN,
    MVT_KEYPAD_END,
    MVT_KEYPAD_BEGIN,
    MVT_KEYPAD_INSERT,
    MVT_KEYPAD_EQUAL,
    MVT_KEYPAD_MULTIPLY,
    MVT_KEYPAD_ADD,
    MVT_KEYPAD_SEPARATOR,
    MVT_KEYPAD_SUBTRACT,
    MVT_KEYPAD_DECIMAL,
    MVT_KEYPAD_DIVIDE,
    MVT_KEYPAD_0,
    MVT_KEYPAD_1,
    MVT_KEYPAD_2,
    MVT_KEYPAD_3,
    MVT_KEYPAD_4,
    MVT_KEYPAD_5,
    MVT_KEYPAD_6,
    MVT_KEYPAD_7,
    MVT_KEYPAD_8,
    MVT_KEYPAD_9,
    MVT_KEYPAD_F1,
    MVT_KEYPAD_F2,
    MVT_KEYPAD_F3,
    MVT_KEYPAD_F4,
    MVT_KEYPAD_F5,
    MVT_KEYPAD_F6,
    MVT_KEYPAD_F7,
    MVT_KEYPAD_F8,
    MVT_KEYPAD_F9,
    MVT_KEYPAD_F10,
    MVT_KEYPAD_F11,
    MVT_KEYPAD_F12,
    MVT_KEYPAD_F13,
    MVT_KEYPAD_F14,
    MVT_KEYPAD_F15,
    MVT_KEYPAD_F16,
    MVT_KEYPAD_F17,
    MVT_KEYPAD_F18,
    MVT_KEYPAD_F19,
    MVT_KEYPAD_F20
};

typedef enum _mvt_cursor_t {
    MVT_CURSOR_CURRENT,
    MVT_CURSOR_SELECTION_START,
    MVT_CURSOR_SELECTION_END
} mvt_cursor_t;

struct _mvt_attribute {
    mvt_color_t foreground_color : 9;
    mvt_color_t background_color : 9;
    unsigned int wide : 1;
    unsigned int no_char : 1;
    unsigned int bright : 1;
    unsigned int dim : 1;
    unsigned int underscore : 1;
    unsigned int blink : 1;
    unsigned int reverse : 1;
    unsigned int hidden : 1;
};

/* mvt_screen_vt_t */
struct _mvt_screen_vt {
    void *(*begin)(mvt_screen_t *screen);
    void (*end)(mvt_screen_t *screen, void *gc);
    void (*draw_text)(mvt_screen_t *screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t count);
    void (*clear_rect)(mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color);
    void (*scroll)(mvt_screen_t *screen, int y1, int y2, int count);
    void (*move_cursor)(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y);
    void (*beep)(mvt_screen_t *screen);
    void (*get_size)(mvt_screen_t *screen, int *width, int *height);
    int (*resize)(mvt_screen_t *screen, int width, int height);
    void (*set_title)(mvt_screen_t *screen, const mvt_char_t *ws);
    void (*set_scroll_info)(mvt_screen_t *screen, int scroll_position, int virtual_height);
    void (*set_mode)(mvt_screen_t *screen, int mode, int value);
};

struct _mvt_screen {
    const mvt_screen_vt_t *vt;
    void *driver_data;
    void *user_data;
};

void mvt_screen_set_driver_data(mvt_screen_t *screen, void *data);
void *mvt_screen_get_driver_data(const mvt_screen_t *screen);
void mvt_screen_dispatch_paint(const mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2);
void mvt_screen_dispatch_repaint(const mvt_screen_t *screen);
void mvt_screen_dispatch_close(mvt_screen_t *screen);
void mvt_screen_dispatch_keydown(mvt_screen_t *screen, int meta, int code);
void mvt_screen_dispatch_mousebutton(mvt_screen_t *screen, int down, int button, uint32_t mod, int x, int y, int align);
void mvt_screen_dispatch_mousemove(mvt_screen_t *screen, int x, int y, int align);
void mvt_screen_dispatch_resize(mvt_screen_t *screen);
void mvt_screen_dispatch_paste(mvt_screen_t *screen, const mvt_char_t *ws, size_t count);

void mvt_terminal_set_driver_data(mvt_terminal_t *terminal, void *data);
void *mvt_terminal_get_driver_data(const mvt_terminal_t *terminal);

struct _mvt_driver_vt {
    int (*init)(int *argc, char ***argv, mvt_event_func_t event_func);
    void (*main)();
    void (*main_quit)(void);
    void (*exit)(void);
    mvt_screen_t *(*open_screen)(char **args);
    void (*close_screen)(mvt_screen_t *screen);
    mvt_terminal_t *(*open_terminal)(char **args);
    void (*close_terminal)(mvt_terminal_t *terminal);
    int (*set_screen_attribute)(mvt_screen_t *screen, const char *name, const char *value);
    int (*set_terminal_attribute)(mvt_terminal_t *terminal, const char *name, const char *value);
    void (*suspend)(mvt_terminal_t *terminal);
    void (*resume)(mvt_terminal_t *terminal);
    void (*shutdown)(mvt_terminal_t *terminal);
};

struct _mvt_driver {
    const mvt_driver_vt_t *vt;
    const char *name;
};

const mvt_driver_t *mvt_get_driver(void);

mvt_terminal_t *mvt_worker_open_terminal(char **args);
void mvt_worker_close_terminal(mvt_terminal_t *terminal);
int mvt_worker_set_terminal_attribute(mvt_terminal_t *terminal, const char *name, const char *value);
void mvt_worker_suspend(mvt_terminal_t *terminal);
void mvt_worker_resume(mvt_terminal_t *terminal);
void mvt_worker_shutdown(mvt_terminal_t *terminal);
void mvt_notify_request(void);
void mvt_handle_request(void);
void mvt_notify_resize(mvt_terminal_t *terminal);
void mvt_worker_init(mvt_event_func_t event_func);
void mvt_worker_exit(void);

MVT_END_DECLS

#endif
