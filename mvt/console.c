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
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <mvt/mvt.h>
#include "private.h"
#include "misc.h"
#include "debug.h"
#include "driver.h"

/*! \addtogroup Console
 * @{
 **/

/**
 * @typedef mvt_console_t
 * Console
 **/

/**
 * Get the offset in the buffer
 * @param console a console
 * @param virtual_y virtual Y position 
 */
#define mvt_console_offset(console, virtual_y)                  \
    ((((virtual_y) + (console)->offset)                         \
      % (console)->virtual_height) * (console)->width)

#define mvt_console_get_char_pointer(console, offset) ((char *)NULL)
#define mvt_console_get_color_pair_pointer(console, offset) ((char *)NULL)
#define mvt_console_get_charset_pointer(console, offset) ((char *)NULL)
#define mvt_console_get_attribute_pointer(console, offset) ((char *)NULL)

static size_t mvt_console_write0(mvt_console_t *console, const mvt_char_t *ws, size_t len);
static void mvt_console_erase_line0(mvt_console_t *console, int startx, int endx, int cy);
static void mvt_console_clear_buffer(mvt_console_t *consle, size_t offset, size_t length);
static int mvt_console_resize0(mvt_console_t *console, int width, int height, int virtual_height);
static void mvt_console_copy_buffer(mvt_console_t *console, size_t dst_offset, size_t src_offset, size_t count);
static void mvt_console_scroll(mvt_console_t *console, int start, int end, int count);
static void mvt_console_erase_display0(mvt_console_t *console, int start, int end);
static int mvt_console_adjust_to_char(const mvt_console_t *console, int x, int y, int *rx);
static void mvt_console_adjust_point_to_char (const mvt_console_t *console, int end, int x, int y, int align, int *rx, int *ry);
static void mvt_console_update(const mvt_console_t *console, int x1, int y1, int x2, int y2);
static void mvt_console_clear_selection(mvt_console_t *console);

int mvt_console_init(mvt_console_t *console, int width, int height, int save_height)
{
    memset(console, 0, sizeof (*console));
    console->attribute.foreground_color = MVT_DEFAULT_COLOR;
    console->attribute.background_color = MVT_DEFAULT_COLOR;
    console->cursor_x = 0;
    console->cursor_y = 0;
    console->show_cursor = TRUE;
    console->save_height = save_height;
    console->scroll_y1 = -1;
    console->scroll_y2 = -1;
    console->selection_x1 = -1;
    console->selection_y1 = -1;
    console->selection_x2 = -1;
    console->selection_y2 = -1;
    if (mvt_console_resize0(console, width, height, height + save_height) == -1) {
        free(console->title);
        return -1;
    }
    return 0;
}

void mvt_console_destroy(mvt_console_t *console)
{
    free(console->text_buffer);
    free(console->attribute_buffer);
    if (console->input_buffer) free(console->input_buffer);
    if (console->title) free(console->title);
    memset(console, 0, sizeof *console);
}

int mvt_console_set_screen(mvt_console_t *console, mvt_screen_t *screen)
{
    int width, height;
    console->screen = screen;
    if (!console->screen)
        return 0;
    mvt_screen_get_size(console->screen, &width, &height);
    assert(width > 0);
    assert(height > 0);
    if (console->width != width || console->height != height)
        mvt_console_resize0(console, width, height, height + console->save_height);
    mvt_screen_move_cursor(console->screen, MVT_CURSOR_CURRENT, console->cursor_x, console->cursor_y);
    mvt_screen_set_scroll_info(console->screen, console->top, console->top + console->height);
    mvt_screen_set_title(console->screen, console->title);
    return 0;
}

mvt_screen_t *mvt_console_get_screen(const mvt_console_t *console)
{
    return console->screen;
}

int mvt_console_resize(mvt_console_t *console)
{
    int width, height;
    int old_cursor_x, old_cursor_y;
    if (!console->screen)
        return 0;
    mvt_screen_get_size(console->screen, &width, &height);
    assert(width > 0);
    assert(height > 0);
    if (console->width == width && console->height == height)
        return 0;
    old_cursor_x = console->cursor_x;
    if (old_cursor_x >= width)
        old_cursor_x = width - 1;
    old_cursor_y = console->cursor_y - console->top;
    if (old_cursor_y >= height)
        old_cursor_y = height - 1;
    if (mvt_console_resize0(console, width, height, height + console->save_height) == -1)
        return -1;
    mvt_screen_move_cursor(console->screen, MVT_CURSOR_CURRENT, old_cursor_x, old_cursor_y + console->top);
    mvt_screen_set_scroll_info(console->screen, console->top, console->top + console->height);
    return 0;
}

void
mvt_console_begin (mvt_console_t *console)
{
    int x, y;
    int char_width;

    assert(!console->gc);

    console->gc = mvt_screen_begin(console->screen);
    
    if (console->show_cursor) {
        x = console->cursor_x;
        y = console->cursor_y;
        char_width = mvt_console_adjust_to_char(console, x, y, &x);
        if (!console->screen)
            return;
        mvt_screen_move_cursor(console->screen, MVT_CURSOR_CURRENT, -1, -1);
        mvt_console_update(console, x, y, x + char_width - 1, y);
    }
}

void
mvt_console_end (mvt_console_t *console)
{
    int x, y;
    int char_width;
    if (console->show_cursor) {
        x = console->cursor_x;
        y = console->cursor_y;
        char_width = mvt_console_adjust_to_char(console, x, y, &x);
        if (!console->screen)
            return;
        mvt_screen_move_cursor(console->screen, MVT_CURSOR_CURRENT, x, y);
        mvt_console_update(console, x, y, x + char_width - 1, y);
    }

    if (console->gc) {
        mvt_screen_end(console->screen, console->gc);
        console->gc = NULL;
    }
}

/**
 * Move the cursor. The position won't be adjusted. The cursor can be on
 * the right half of a zenkaku character. If x and y are -1, the position
 * won't be changed.
 *
 * @param console a console
 * @param x physical X position
 * @param y physical Y position
 */
void
mvt_console_move_cursor (mvt_console_t *console, int x, int y)
{
    MVT_DEBUG_PRINT3("mvt_console_move_cursor(%d,%d)\n", x, y);
    if (x >= 0) console->cursor_x = x;
    if (y >= 0) console->cursor_y = y + console->top;
    if (console->cursor_x < 0)
        console->cursor_x = 0;
    if (console->cursor_x >= console->width)
        console->cursor_x = console->width - 1;
    if (console->cursor_y < console->top)
        console->cursor_y = console->top;
    if (console->cursor_y >= console->top + console->height)
        console->cursor_y = console->top + console->height - 1;
}

void
mvt_console_save_cursor (mvt_console_t *console)
{
    console->save_cursor_x = console->cursor_x;
    console->save_cursor_y = console->cursor_y - console->top;
}

void
mvt_console_restore_cursor (mvt_console_t *console)
{
    console->cursor_x = console->save_cursor_x;
    console->cursor_y = console->save_cursor_y + console->top;
    if (console->cursor_x >= console->width)
        console->cursor_x = console->width - 1;
    if (console->cursor_y >= console->top + console->height)
        console->cursor_y = console->top + console->height - 1;
}

void
mvt_console_write (mvt_console_t *console, const mvt_char_t *ws, size_t count)
{
    while (count > 0) {
        size_t n;
        /* trying to write over the selection, erase the selection */
        if (console->cursor_y >= console->selection_y1 &&
            console->cursor_y <= console->selection_y2)
            mvt_console_clear_selection(console);
        n = mvt_console_write0(console, ws, count);
        ws += n;
        assert(count >= n);
        count -= n;
        if (count > 0) {
            MVT_DEBUG_PRINT1("mvt_console_write: wrapped\n");
            mvt_console_carriage_return(console);
            mvt_console_line_feed(console);
        }
    }
}

static size_t mvt_console_write0(mvt_console_t *console, const mvt_char_t *ws, size_t count)
{
    const mvt_char_t *p = ws;
    mvt_char_t wc, *text;
    mvt_attribute_t *attribute;
    int new_x, char_width, offset;

    offset = mvt_console_offset(console, console->cursor_y);
    text = &console->text_buffer[offset + console->cursor_x];
    attribute = &console->attribute_buffer[offset + console->cursor_x];
    new_x = console->cursor_x;
    while (count--) {
        wc = *p;
        if (mvt_wcwidth(wc) == 2) {
            char_width = 2;
        } else {
            char_width = 1;
        }
        if (new_x + char_width > console->width) {
            /* the character doesn't fit in the line */
            break;
        }
        *text++ = wc;
        *attribute = console->attribute;
        if (char_width > 1)
            attribute->wide = TRUE;
        attribute++;
        if (char_width > 1) {
            *text++ = '\0';
            *attribute = console->attribute;
            attribute->no_char = TRUE;
            attribute++;
        }
        p++;
        new_x += char_width;
    }

    if (console->screen && new_x > console->cursor_x) {
        if (console->gc) {
            mvt_console_paint(console, console->gc, console->cursor_x, console->cursor_y, new_x - 1, console->cursor_y);
        }
    }

    console->cursor_x = new_x;

    return p - ws;
}

void
mvt_console_move_cursor_relative (mvt_console_t *console, int dx, int dy)
{
    console->cursor_x += dx;
    console->cursor_y += dy;
    if (console->cursor_x < 0)
        console->cursor_x = 0;
    if (console->cursor_x >= console->width)
        console->cursor_x = console->width - 1;
    if (console->cursor_y < console->top)
        console->cursor_y = console->top;
    if (console->cursor_y >= console->top + console->height)
        console->cursor_y = console->top + console->height - 1;
}

void mvt_console_forward_tabstops(mvt_console_t *console, int n)
{
    console->cursor_x = (console->cursor_x + 8 * n) & ~7;
    if (console->cursor_x >= console->width)
        console->cursor_x = console->width - 1;
}

void mvt_console_carriage_return(mvt_console_t *console)
{
    console->cursor_x = 0;
}

void
mvt_console_line_feed (mvt_console_t *console)
{
    size_t offset;

    /* MVT_DEBUG_PRINT2("mvt_console_line_feed: height=%d,top=%d,offset=%d,virtual_height=%d\n", console->height, console->top, console->top, console->virtual_height); */

    if (console->cursor_y == console->scroll_y2) {
        mvt_console_scroll(console, console->scroll_y1, console->scroll_y2, -1);
        return;
    }

    if (console->cursor_y < console->top + console->height - 1) {
        console->cursor_y++;
        return;
    }
    
    if (console->scroll_y1 != -1)
        return;

    if (console->top + console->height < console->virtual_height) {
        console->top++;
        console->cursor_y++;
        mvt_screen_set_scroll_info(console->screen, console->top, console->top + console->height);
        return;
    }

    console->offset++;
    if (console->offset >= console->virtual_height)
        console->offset = 0;
    offset = mvt_console_offset(console, console->cursor_y);
    mvt_console_clear_buffer(console, offset, console->width);
    if (mvt_console_has_selection(console)) {
        if (console->selection_y1 == 0) {
            console->selection_x1 = -1;
            console->selection_x2 = -1;
            console->selection_y1 = -1;
            console->selection_y2 = -1;
        } else {
            console->selection_y1--;
            console->selection_y2--;
        }
    }

    if (!console->screen) return;
    mvt_screen_scroll(console->screen, -1, -1, -1);
    if (!console->gc) return;
}

void mvt_console_reverse_index(mvt_console_t *console)
{
    if (console->scroll_y1 != -1 && console->cursor_y > console->scroll_y1) {
        console->cursor_y--;
        return;
    }
    mvt_console_scroll(console, console->scroll_y1, console->scroll_y2, 1);
}

void
mvt_console_erase_display (mvt_console_t *console, int mode)
{
    switch (mode) {
    case 0:
        mvt_console_erase_line0(console, console->cursor_x, console->width - 1, console->cursor_y);
        mvt_console_erase_display0(console, console->cursor_y + 1, console->top + console->height - 1);
        break;
    case 1:
        mvt_console_erase_display0(console, console->top, console->cursor_y);
        mvt_console_erase_line0(console, 0, console->cursor_x, console->cursor_y);
        break;
    case 2:
        mvt_console_erase_display0(console, console->top, console->top + console->height - 1);
        break;
    }
}

void
mvt_console_erase_line (mvt_console_t *console, int mode)
{
    switch (mode) {
    case 0:
        mvt_console_erase_line0(console, console->cursor_x, console->width - 1, console->cursor_y);
        break;
    case 1:
        mvt_console_erase_line0(console, 0, console->cursor_x, console->cursor_y);
        break;
    case 2:
        mvt_console_erase_line0(console, 0, console->width - 1, console->cursor_y);
        break;
    }
}

void
mvt_console_erase_chars (mvt_console_t *console, int n)
{
    int startx = console->cursor_x;
    int endx = console->cursor_x + n - 1;
    if (startx > endx)
        endx = startx;
    mvt_console_erase_line0(console, startx, endx, console->cursor_y);
}

static void
mvt_console_update (const mvt_console_t *console, int x1, int y1, int x2, int y2)
{
    if (!console->gc) return;
    mvt_console_paint(console, console->gc, x1, y1, x2, y2);
}

/*! 
 * Paint the specified area.
 * @param console console
 * @param gc graphics context
 * @param x1 virtual left-most position
 * @param y1 virtual top-most position
 * @param x2 virtual right-most position
 * @param y2 virtual bottom-most position
 **/
void
mvt_console_paint (const mvt_console_t *console, void *gc, int x1, int y1, int x2, int y2)
{
    assert(x1 >= 0);
    assert(y1 >= 0);
    assert(x2 >= x1);
    assert(y2 >= y1);
    assert(x2 < console->width);
    assert(y2 < console->top + console->height);
    
    while (y1 <= y2) {
        int offset = mvt_console_offset(console, y1);
        mvt_char_t *text = &console->text_buffer[offset + x1];
        mvt_attribute_t *attribute = &console->attribute_buffer[offset + x1];
        mvt_screen_draw_text(console->screen, gc, x1, y1, text, attribute, x2 - x1 + 1);
        y1++;
    }
}

void mvt_console_repaint(const mvt_console_t *console)
{
    void *gc;
    if (!console->screen) return;
    gc = mvt_screen_begin(console->screen);
    if (gc == NULL) return;
    mvt_console_paint(console, gc, 0, 0, console->width - 1, console->height - 1);
    mvt_screen_end(console->screen, gc);
    mvt_screen_set_scroll_info(console->screen, console->top, console->top + console->height);
}

/**
 * Erase lines
 * @param y virtual Y position
 **/
static void
mvt_console_erase_display0 (mvt_console_t *console, int y1, int y2)
{
    int width = console->width;
    int y;
    if (y2 < y1)
        return;
    for (y = y1; y <= y2; y++) {
        int offset = mvt_console_offset(console, y);
        mvt_console_clear_buffer(console, offset, width);
    }
    if (!console->screen) return;
    if (!console->gc) return;
    mvt_screen_clear_rect(console->screen, console->gc, 0, y1, width - 1, y2,
                          console->attribute.background_color);
}

/**
 * Erase line
 * @param y virtual Y position
 **/
static void
mvt_console_erase_line0 (mvt_console_t *console, int x1, int x2, int y)
{
    int offset = mvt_console_offset(console, y);
    int char_width;
    (void)mvt_console_adjust_to_char(console, x1, y, &x1);
    char_width = mvt_console_adjust_to_char(console, x2, y, &x2);
    x2 += char_width - 1;
    mvt_console_clear_buffer(console, offset + x1, x2 - x1 + 1);
    if (!console->screen) return;
    if (!console->gc) return;
    mvt_screen_clear_rect(console->screen, console->gc, x1, y, x2, y,
                          console->attribute.background_color);
}

void
mvt_console_delete_lines (mvt_console_t *console, int count)
{
  int start = console->cursor_y > console->scroll_y1 ? console->cursor_y : console->scroll_y1;
  int end = console->scroll_y2 == -1 ? console->virtual_height - 1 : console->scroll_y2;
  mvt_console_scroll(console, start, end, -count);
}

/**
 * Move chars
 * @param y virtual Y position
 **/
static void
mvt_console_move_chars (mvt_console_t *console, int x1, int x2, int y, int count)
{
    int offset;
    offset = mvt_console_offset(console, y);
    if (count > 0) {
        if (x2 - x1 - count + 1 > 0) {
            mvt_console_copy_buffer(console, offset + x1 + count, offset + x1,
                                    x2 - x1 - count + 1);
            mvt_console_clear_buffer(console, offset + x1, count);
        } else {
            mvt_console_clear_buffer(console, offset + x1, x2 - x1 + 1);
        }
    } else {
        if (x2 - x1 + count + 1 > 0) {
            mvt_console_copy_buffer(console, offset + x1, offset + x1 - count,
                                    x2 - x1 + count + 1);
            mvt_console_clear_buffer(console, offset + x2 + count + 1, -count);
        } else {
            mvt_console_clear_buffer(console, offset + x1, x2 - x1 + 1);
        }
    }
    if (!console->screen) return;
    if (!console->gc) return;
    offset = mvt_console_offset(console, y);
    if (count > 0) {
        if (x2 - x1 - count + 1 > 0) {
            mvt_console_paint(console, console->gc, x1 + count, y, x2, y);
            mvt_screen_clear_rect(console->screen, console->gc, x1, y, x1 + count - 1, y,
                                  console->attribute.background_color);
        } else {
            mvt_screen_clear_rect(console->screen, console->gc, x1, y, x2, y,
                                  console->attribute.background_color);
        }
    } else {
        if (x2 - x1 + count + 1 > 0) {
            mvt_console_paint(console, console->gc, x1, y, x2 + count, y);
            mvt_screen_clear_rect(console->screen, console->gc, x2 + count + 1, y, x2, y,
                                  console->attribute.background_color);
        } else {
            mvt_screen_clear_rect(console->screen, console->gc, x1, y, x2, y,
                                  console->attribute.background_color);
        }
    }
}

void
mvt_console_insert_chars(mvt_console_t *console, int count)
{
    mvt_console_move_chars(console, console->cursor_x, console->width - 1, console->cursor_y, count);
    /* clear selection */
    if (console->cursor_y + console->top >= console->selection_y1
        && console->cursor_y + console->top <= console->selection_y2)
        mvt_console_clear_selection(console);
}

void mvt_console_delete_chars(mvt_console_t *console, int count)
{
    mvt_console_move_chars(console, console->cursor_x, console->width - 1, console->cursor_y, -count);
    /* clear selection */
    if (console->cursor_y + console->top >= console->selection_y1
        && console->cursor_y + console->top <= console->selection_y2)
        mvt_console_clear_selection(console);
}

/**
 * scroll lines
 * @param y1 virtual top-most position
 * @param y2 virtual bottom-most position
 * @param count amount to scroll
 **/
static void
mvt_console_scroll (mvt_console_t *console, int y1, int y2, int count)
{
    int scroll_height, clear_height;
    int width = console->width;
    int i, j;
    void *gc;

    if (count == 0)
        return;

    if (y1 == -1) y1 = console->top;
    if (y2 == -1) y2 = console->virtual_height - 1;

    scroll_height = y2 - y1 + 1;
    if (count > 0)
        scroll_height -= count;
    else
        scroll_height += count;

    for (i = 0; i < scroll_height; i++) {
        int src_offset, dst_offset;
        if (count > 0) {
            src_offset = mvt_console_offset(console, y2 - i - count);
            dst_offset = mvt_console_offset(console, y2 - i);
        } else {
            src_offset = mvt_console_offset(console, y1 + i - count);
            dst_offset = mvt_console_offset(console, y1 + i);
        }
        mvt_console_copy_buffer(console, dst_offset, src_offset, width);
    }

    clear_height = count > 0 ? count : -count;

    j = count > 0 ? y1 : y2 - clear_height + 1;
    for (i = 0; i < clear_height; i++) {
        int offset = mvt_console_offset(console, i + j);
        mvt_console_clear_buffer(console, offset, width);
    }

    if (!console->screen) return;
    if (scroll_height > 0) {
        if (y1 == 0 && y2 == console->height)
            mvt_screen_scroll(console->screen, -1, -1, count);
        else
            mvt_screen_scroll(console->screen, y1, y2, count);
    }
    gc = mvt_screen_begin(console->screen);
    if (gc == NULL) return;
    if (clear_height > 0)
        mvt_screen_clear_rect(console->screen, gc, 0, j,
                              width - 1, j + clear_height - 1,
                              console->attribute.background_color);
    mvt_screen_end(console->screen, gc);
}

static void mvt_console_clear_buffer(mvt_console_t *console, size_t offset, size_t count)
{
    mvt_char_t *ws;
    mvt_attribute_t *attribute;
    ws = &console->text_buffer[offset];
    memset(ws, 0, sizeof (mvt_char_t) * count);
    attribute = &console->attribute_buffer[offset];
    while (count--)
        *attribute++ = console->attribute;
}

static void mvt_console_copy_buffer(mvt_console_t *console, size_t dst_offset, size_t src_offset, size_t count)
{
    memmove(&console->text_buffer[dst_offset],
            &console->text_buffer[src_offset],
            count * sizeof (mvt_char_t));
    memmove(&console->attribute_buffer[dst_offset],
            &console->attribute_buffer[src_offset],
            count * sizeof (mvt_attribute_t));
}

int mvt_console_set_save_height(mvt_console_t *console, int save_height)
{
    assert(save_height >= 0);
    if (mvt_console_resize0(console, console->width, console->height, console->height + save_height) == -1)
        return -1;
    console->save_height = save_height;
    return 0;
}

static int mvt_console_resize0(mvt_console_t *console, int width, int height, int virtual_height)
{
    mvt_char_t *new_text_buffer, *new_text, *old_text;
    mvt_attribute_t *new_attribute_buffer, *new_attribute, *old_attribute;
    size_t size;
    int offset, new_top;
    int y, copy_start, copy_width, copy_height, new_cursor_y;

    size = width * virtual_height;
    new_text_buffer = malloc(size * sizeof (mvt_char_t));
    if (!new_text_buffer)
        return -1;
    new_attribute_buffer = malloc(size * sizeof (mvt_attribute_t));
    if (!new_attribute_buffer) {
        free(new_text_buffer);
        return -1;
    }
    new_text = new_text_buffer;
    new_attribute = new_attribute_buffer;
    while (size--) {
        *new_text++ = '\0';
        *new_attribute++ = console->attribute;
    }

    /* If there's an old buffer, copy from it */
    if (console->text_buffer) {
        new_top = console->top;
        new_cursor_y = console->cursor_y;
        copy_start = 0;
        copy_width = console->width;
        copy_height = console->top + console->height;
        if (copy_width > width)
            copy_width = width;
        if (height > console->height) {
            assert(height - console->height > 0);
            new_top -= height - console->height;
            if (new_top < 0) {
                new_top = 0;
            }
        }
        if (new_cursor_y >= new_top + height) {
            copy_height = console->cursor_y + 1;
            new_top = copy_height - height;
        }
        if (copy_height > virtual_height) {
            assert(copy_height - virtual_height > 0);
            copy_start = copy_height - virtual_height;
            new_top = virtual_height - height;
			new_cursor_y -= copy_height - virtual_height;
            copy_height = virtual_height;
        }
        for (y = 0; y < copy_height; y++) {
            new_text = &new_text_buffer[y * width];
            new_attribute = &new_attribute_buffer[y * width];
            offset = mvt_console_offset(console, y + copy_start);
            old_text = &console->text_buffer[offset];
            old_attribute = &console->attribute_buffer[offset];
            memcpy(new_text, old_text, copy_width * sizeof (mvt_char_t));
            memcpy(new_attribute, old_attribute, copy_width * sizeof (mvt_attribute_t));
        }
        free(console->text_buffer);
        free(console->attribute_buffer);
        if (console->cursor_x > width)
            console->cursor_x = width - 1;
    } else {
        new_top = 0;
        console->cursor_x = 0;
        new_cursor_y = 0;
    }
    /* xterm resets scroll region and Emacs depends on it. */
    console->scroll_y1 = -1;
    console->scroll_y2 = -1;
    console->cursor_y = new_cursor_y;
    console->text_buffer = new_text_buffer;
    console->attribute_buffer = new_attribute_buffer;
    console->top = new_top;
    console->offset = 0;
    console->width = width;
    console->height = height;
    console->virtual_height = virtual_height;

    MVT_DEBUG_PRINT3("mvt_console_resize0: top=%d,cy=%d\n", console->top, console->cursor_y);

    return 0;
}

void mvt_console_get_size(const mvt_console_t *console, int *width, int *height)
{
    if (width != NULL) *width = console->width;
    if (height != NULL) *height = console->height;
}

/**
 * scroll lines
 * @param console a console
 * @param y1 physical top-most position
 * @param y2 physical bottom-most position
 * @param count amount to scroll
 **/
void
mvt_console_set_scroll_region (mvt_console_t *console, int y1, int y2)
{
    MVT_DEBUG_PRINT3("mvt_console_set_scroll_region(%d,%d)\n", y1, y2);
    if (y1 == -1) {
        console->scroll_y1 = -1;
        console->scroll_y2 = -1;
    } else {
        if (y1 > y2 || y1 > console->virtual_height - 1 || y2 > console->virtual_height - 1)
            return;
        console->scroll_y1 = console->top + y1;
        console->scroll_y2 = console->top + y2;
    }
    console->cursor_x = 0;
    console->cursor_y = console->top;
}

void mvt_console_full_reset(mvt_console_t *console)
{
    MVT_DEBUG_PRINT1("mvt_console_full_reset\n");
    mvt_console_set_scroll_region(console, -1, -1);
    memset(&console->attribute, 0, sizeof (console->attribute));
    console->attribute.foreground_color = MVT_DEFAULT_COLOR;
    console->attribute.background_color = MVT_DEFAULT_COLOR;
    mvt_console_clear_buffer(console, 0, console->width * console->virtual_height);
    console->top = 0;
    console->cursor_x = 0;
    console->cursor_y = 0;

    if (!console->screen) return;
    mvt_screen_move_cursor(console->screen, MVT_CURSOR_CURRENT, 0, 0);
    mvt_screen_set_scroll_info(console->screen, console->top, console->top + console->height);
    if (!console->gc) return;
    mvt_screen_clear_rect(console->screen, console->gc, 0, console->top, console->width - 1, console->top + console->height - 1, console->attribute.background_color);
}

int mvt_console_append_input(mvt_console_t *console, const mvt_char_t *ws, size_t count)
{
    mvt_char_t *new_buf;
    size_t new_count;
    if (console->input_buffer) {
        new_count = console->input_buffer_length
            - console->input_buffer_index + count;
        new_buf = malloc(new_count * sizeof (mvt_char_t));
        if (new_buf == NULL)
            return -1;
        memcpy(new_buf, console->input_buffer + console->input_buffer_index,
               (console->input_buffer_length - console->input_buffer_index) * sizeof (mvt_char_t));
        free(console->input_buffer);
        memcpy(new_buf + console->input_buffer_index, ws, count * sizeof (mvt_char_t));
    } else {
        new_buf = malloc(count * sizeof (mvt_char_t));
        if (new_buf == NULL)
            return -1;
        memcpy(new_buf, ws, count * sizeof (mvt_char_t));
    }
    console->input_buffer = new_buf;
    console->input_buffer_index = 0;
    console->input_buffer_length += count;
    return 0;
}

size_t mvt_console_read_input(mvt_console_t *console, mvt_char_t *ws, size_t count)
{
    size_t rest;
    if (console->input_buffer == NULL) return 0;
    rest = console->input_buffer_length - console->input_buffer_index;
    if (count > rest) count = rest;
    memcpy(ws, console->input_buffer + console->input_buffer_index, count * sizeof (mvt_char_t));
    console->input_buffer_index += count;
    if (console->input_buffer_index == console->input_buffer_length) {
        free(console->input_buffer);
        console->input_buffer = NULL;
        console->input_buffer_index = 0;
        console->input_buffer_length = 0;
    }
    return count;
}

int mvt_console_copy_selection(const mvt_console_t *console, mvt_char_t *buf, size_t len, int nl)
{
#if 0
  int vx;
  int vy;
  int i;
  int tlen = 0;
  char *dst = buf;

  if (!mvt_console_has_selection(console))
    return 0;

  vx = console->selection_x1;
  vy = console->selection_y1;

  while (vy <= console->selection_y2)
    {
      int offset = mvt_console_virtual_offset(console, vy);
      int end_vx = console->width - 1;
      if (vy == console->selection_y2)
        end_vx = console->selection_x2;
      end_vx = vx + mvt_console_find_last_char(console, offset + vx, offset + end_vx) - 1;

      while (vx <= end_vx)
        {
          const uint8_t *p = mvt_console_get_char_pointer(console, offset + vx);
          uint8_t attribute = *mvt_console_get_attribute_pointer(console, offset + vx);
          size_t len;
          if (!(attribute & MVT_ATTR_CHARSTART))
            {
              vx++;
              continue;
            }
          len = mvt_console_find_chunk(console, offset + vx, offset + end_vx);
          vx += len;

          for (i = 0; i < len; i++)
            {
              if (dst != NULL)
                {
                  if (*p < ' ')
                    *dst++ = ' ';
                  else
                    *dst++ = *p;
                  p++;
                }
              tlen++;
            }
        }
      if (vx != console->width && vy != console->selection_y2)
        {
          if (dst != NULL)
            {
              if (nl) *dst++ = '\r';
              *dst++ = '\n';
            }
          if (nl) tlen++;
          tlen++;
        }
      vx = 0;
      vy++;
    }
  return tlen;
#else
  return 0;
#endif
}

static void
mvt_console_clear_selection(mvt_console_t *console)
{
    mvt_console_set_selection(console, -1, -1, -1, -1, -1, -1);
}

/**
 * Set selection. x2,y2 is not selected.
 * @param console a console
 * @param x1 virtual X position of start
 * @param y1 virtual Y position of start
 * @param x2 virtual X position of end
 * @param y2 virtual Y position of end
 **/
void
mvt_console_set_selection (mvt_console_t *console, int x1, int y1, int align1, int x2, int y2, int align2)
{
    int old_y1;
    int old_y2;
    int had_sel;
    int update_y1;
    int update_y2;

    MVT_DEBUG_PRINT1("mvt_console_set_selection\n");

    if (x1 < 0) { x1 = 0; if (align1 > 0) align1 = -1; }
    if (x1 >= console->width) { x1 = console->width - 1; if (align1 < 0) align1 = 1; }
    if (y1 < 0) { y1 = 0; x1 = 0; if (align1 > 0) align1 = -1; }
    if (y1 >= console->virtual_height) { x1 = console->width - 1; y1 = console->virtual_height - 1; if (align1 < 0) align1 = 1; }
    if (x2 < 0) { x2 = 0; if (align2 > 0) align2 = -1; }
    if (x2 >= console->width) { x2 = console->width - 1; if (align2 < 0) align2 = 1; }
    if (y2 < 0) { y2 = 0; x2 = 0; if (align2 > 0) align2 = -1; }
    if (y2 >= console->top + console->height) { x2 = console->width - 1; y2 = console->top + console->height - 1; if (align2 < 0) align2 = 1; }

    if (y1 != -1) {
        /* adjust the points to the start of the char */
        mvt_console_adjust_point_to_char(console, FALSE, x1, y1, align1, &x1, &y1);
    }
    if (y2 != -1) {
        /* adjust the points to the end of the char */
        mvt_console_adjust_point_to_char(console, TRUE, x2, y2, align2, &x2, &y2);
    }

    assert(x1 >= 0);
    assert(x2 >= 0);

    if (y1 > y2 || (y1 == y2 && x1 >= x2)) {
        x1 = -1;
        y1 = -1;
        x2 = -1;
        y2 = -1;
    }
  
    had_sel = mvt_console_has_selection(console);
    old_y1 = console->selection_y1;
    old_y2 = console->selection_y2;
    console->selection_x1 = x1;
    console->selection_y1 = y1;
    console->selection_x2 = x2;
    console->selection_y2 = y2;

    if (had_sel) {
        if (mvt_console_has_selection(console)) {
            update_y1 = old_y1;
            update_y2 = old_y2;
            if (update_y1 > y1)
                update_y1 = y1;
            if (update_y2 < y2)
                update_y2 = y2;
        } else {
            update_y1 = old_y1;
            update_y2 = old_y2;
        }
    } else {
        if (mvt_console_has_selection(console)) {
            update_y1 = y1;
            update_y2 = y2;
        } else {
            return;
        }
    }

    mvt_screen_move_cursor(console->screen, MVT_CURSOR_SELECTION_START, x1, y1);
    mvt_screen_move_cursor(console->screen, MVT_CURSOR_SELECTION_END, x2, y2);
    mvt_console_update(console, 0, update_y1, console->width - 1, update_y2);
}

void
mvt_console_get_selection (const mvt_console_t *console, int *x1, int *y1, int *x2, int *y2)
{
  *x1 = console->selection_x1;
  *y1 = console->selection_y1;
  *x2 = console->selection_x2;
  *y2 = console->selection_y2;
}

/**
 * @param console a console
 * @param x virtual X position
 * @param y virtual Y position
 * @param char_width width of character at the point
 * @return virtual X position of the start of the character
 */
static int
mvt_console_adjust_to_char (const mvt_console_t *console, int x, int y, int *rx)
{
    size_t offset = mvt_console_offset(console, y);
    int char_width;

    assert(rx != NULL);

    /* cursor is at the last columns */
    if (x == console->width) x--;

    /* cursor is at the right half of a zenkaku character */
    if (console->attribute_buffer[x + offset].no_char) {
        assert(x > 0);
        if (x > 0) x--;
    }

    if (console->attribute_buffer[x + offset].wide)
        char_width = 2;
    else
        char_width = 1;

    assert(x >= 0 && x < console->width);
    *rx = x;

    return char_width;
}

static void
mvt_console_adjust_point_to_char (const mvt_console_t *console, int end, int x, int y, int align, int *rx, int *ry)
{
    mvt_char_t *text;
    mvt_attribute_t *attribute;
    int offset;

    assert(rx != NULL && ry != NULL);

    offset = mvt_console_offset(console, y);
    text = &console->text_buffer[offset];
    attribute = &console->attribute_buffer[offset];
    if (align != 0) {
        if (attribute[x].no_char) {
            x++;
            while (x < console->width) {
                if (!attribute[x].no_char) {
                    break;
                }
                x++;
            }
        } else if (!attribute[x].wide) {
            if (align > 0)
                x++;
            if (x > 0 && text[x - 1] == '\0' && !attribute[x - 1].no_char) {
                int t = x;
                /* check if this NIL character is beyond the end of line */
                while (t < console->width) {
                    if (text[t] != '\0') {
                        t = x;
                        break;
                    }
                    t++;
                }
                x = t;
            }
        }
        if (end) {
            if (x > 0) {
                x--;
            } else {
                if (y > 0) {
                    x = console->width - 1;
                    y--;
                }
            }
        } else {
            if (x >= console->width) {
                if (y < console->top + console->height - 1) {
                    x = 0;
                    y++;
                }
            }
        }
    } else {
        assert(FALSE); /* TODO */
    }

    *rx = x;
    *ry = y;
}

int mvt_console_set_title(mvt_console_t *console, const mvt_char_t *ws)
{
    mvt_char_t *new_title = malloc((mvt_strlen(ws) + 1) * sizeof (mvt_char_t));
    if (!new_title)
        return -1;
    if (console->title)
        free(console->title);
    mvt_strcpy(new_title, ws);
    if (console->screen != NULL)
        mvt_screen_set_title(console->screen, new_title);
    console->title = new_title;
    return 0;
}

void mvt_console_beep(mvt_console_t *console)
{
    if (console->screen)
        mvt_screen_beep(console->screen);
}

/*! * @} */

