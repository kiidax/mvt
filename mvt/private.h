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

#ifndef PRIVATE_H
#define PRIVATE_H

#include <mvt/mvt.h>
#include <mvt/mvt_plugin.h>
#include "driver.h"
#include "misc.h"
#include "debug.h"

typedef struct _mvt_telnet mvt_telnet_t;
typedef struct _mvt_console mvt_console_t;

#define mvt_screen_begin(screen) ((*(screen)->vt->begin)((screen)))
#define mvt_screen_end(screen, gc) ((*(screen)->vt->end)((screen), (gc)))
#define mvt_screen_draw_text(screen, gc, x, y, ws, attribute, len) ((*(screen)->vt->draw_text)((screen), (gc), (x), (y), (ws), (attribute), (len)))
#define mvt_screen_clear_rect(screen, gc, x1, y1, x2, y2, bc) ((*(screen)->vt->clear_rect)((screen), (gc), (x1), (y1), (x2), (y2), (bc)))
#define mvt_screen_scroll(screen, y1, y2, count) ((*(screen)->vt->scroll)((screen), (y1), (y2), (count)))
#define mvt_screen_move_cursor(screen, type, x, y) ((*(screen)->vt->move_cursor)((screen), (type), (x), (y)))
#define mvt_screen_beep(screen) ((*(screen)->vt->beep)((screen)))
#define mvt_screen_resize(screen, width, height) ((*(screen)->vt->resize)((screen), (width), (height)))
#define mvt_screen_get_size(screen, width, height) ((*(screen)->vt->get_size)((screen), (width), (height)))
#define mvt_screen_set_title(screen, s) ((*(screen)->vt->set_title)((screen), (s)))
#define mvt_screen_get_key(screen, code) ((*(screen)->vt->get_key)((screen), (code)))
#define mvt_screen_set_scroll_info(screen, scroll_position, virtual_height) ((*(screen)->vt->set_scroll_info)((screen), (scroll_position), (virtual_height)))
#define mvt_screen_set_mode(screen, mode, value) ((*(screen)->vt->set_mode)((screen), (mode), (value)))

/*! \addtogroup Console
 * @{
 */

/**
 * a console
 */
struct _mvt_console {
    mvt_screen_t *screen;
    
    int offset;
    mvt_char_t *text_buffer;
    mvt_attribute_t *attribute_buffer;
    int width;
    int height;
    int virtual_height;
    int save_height;
    
    int top;

    int cursor_x; /** a virtual X position of the cursor */
    int cursor_y; /** a virtual Y position of the cursor */
    int save_cursor_x; /** a physical X position of the saved cursor */
    int save_cursor_y; /** a physical Y position of the saved cursor */
    int show_cursor;
    mvt_attribute_t attribute;

    mvt_char_t *title;

    mvt_char_t *input_buffer;
    size_t input_buffer_index;
    size_t input_buffer_length;

    int scroll_y1;
    int scroll_y2;

    void *gc;

    int selection_x1;
    int selection_y1;
    int selection_x2;
    int selection_y2;
};

int mvt_console_init(mvt_console_t *console, int width, int height, int save_line);
void mvt_console_destroy(mvt_console_t *console);
int mvt_console_set_screen(mvt_console_t *console, mvt_screen_t *screen);
mvt_screen_t *mvt_console_get_screen(const mvt_console_t *console);
void mvt_console_begin(mvt_console_t *console);
void mvt_console_end(mvt_console_t *console);
void mvt_console_write(mvt_console_t *console, const mvt_char_t *ws, size_t count);
void mvt_console_repaint(const mvt_console_t *console);
void mvt_console_paint(const mvt_console_t *console, void *gc, int x1, int y1, int x2, int y2);
#define mvt_console_get_attribute(console, _attribute)   \
    (*(_attribute) = (console)->attribute)
#define mvt_console_set_attribute(console, _attribute)   \
    ((console)->attribute = *(_attribute))

void mvt_console_move_cursor(mvt_console_t *console, int x, int y);
void mvt_console_move_cursor_relative(mvt_console_t *console, int dx, int dy);
void mvt_console_save_cursor(mvt_console_t *console);
void mvt_console_restore_cursor(mvt_console_t *console);
#define mvt_console_set_show_cursor(console, value) ((console)->show_cursor = (value))
void mvt_console_carriage_return(mvt_console_t *console);
void mvt_console_line_feed(mvt_console_t *console);
void mvt_console_forward_tabstops(mvt_console_t *console, int n);
void mvt_console_erase_display(mvt_console_t *console, int mode);
void mvt_console_erase_line(mvt_console_t *console, int mode);
void mvt_console_erase_chars(mvt_console_t *console, int n);
void mvt_console_beep(mvt_console_t *console);
int mvt_console_get_key(mvt_console_t *console, int *code);
int mvt_console_resize(mvt_console_t *console);
int mvt_console_set_save_height(mvt_console_t *console, int save_height);
void mvt_console_reverse_index(mvt_console_t *console);
void mvt_console_get_size(const mvt_console_t *console, int *width, int *height);
void mvt_console_set_numeric_keypad_mode(mvt_console_t *console, int mode);
#define mvt_console_insert_lines(console, count) mvt_console_delete_lines(console, -count)
void mvt_console_insert_chars(mvt_console_t *console, int count);
void mvt_console_delete_lines(mvt_console_t *console, int count);
void mvt_console_delete_chars(mvt_console_t *console, int count);
void mvt_console_set_scroll_region(mvt_console_t *console, int y1, int y2);
void mvt_console_full_reset(mvt_console_t *console);
#define mvt_console_get_virtual_height(console) ((console)->virtual_height)
#define mvt_console_get_height(console) ((console)->height)
#define mvt_console_get_top(console) ((console)->top)
int mvt_console_append_input(mvt_console_t *console, const mvt_char_t *ws, size_t count);
size_t mvt_console_read_input(mvt_console_t *console, mvt_char_t *ws, size_t count);
#define mvt_console_has_input(console) ((console)->input_buffer != NULL)
int mvt_console_copy_selection(const mvt_console_t *console, mvt_char_t *buf, size_t count, int nl);
void mvt_console_set_selection(mvt_console_t *console, int x1, int y1, int align1, int x2, int y2, int align2);
void mvt_console_get_selection(const mvt_console_t *console, int *start_vx, int *start_vy, int *end_vx, int *end_vy);
#define mvt_console_has_selection(console) ((console)->selection_y1 != -1)
int mvt_console_set_title(mvt_console_t *console, const mvt_char_t *ws);

/** @} */

/* misc */

mvt_session_t *mvt_session_open(const char *spec, mvt_session_t *session, int width, int height);
void mvt_set_flag(int *flags, int flag, int value);

#endif
