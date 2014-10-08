/* Multi-purpose Virtual Terminal
 * Copyright (C) 2005-2011,2012 Katsuya Iida
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
#include <wchar.h>
#include <assert.h>
#include <d2d1.h>
#include <dwrite.h>

#include <mvt/mvt.h>
#include <mvt/mvt_d2d.h>

#include "misc.h"
#include "debug.h"
#include "driver.h"

#define MVT_D2D_SCREEN_ALWAYSBRIGHT (1<<0)
#define MVT_D2D_SCREEN_PSEUDOBOLD   (1<<1)

typedef struct _mvt_d2d_screen mvt_d2d_screen_t;

struct _mvt_d2d_screen {
    mvt_screen_t parent;

    char *font_name;
    int font_size;

    uint32_t foreground_color;
    uint32_t background_color;
    uint32_t selection_color;
    bool background_transparent;

    int width, height, virtual_height;
    int cursor_x, cursor_y;
    int scroll_position;
    int scroll_position_modified;
    int line_modified;
    int selection_x1, selection_y1;
    int selection_x2, selection_y2;
    unsigned int flags;

    void *user_data;
};

/* mvt_d2d_screen_t */

static void *mvt_d2d_screen_begin(mvt_screen_t *screen);
static void mvt_d2d_screen_end(mvt_screen_t *screen, void *gc);
static void mvt_d2d_screen_draw_text(mvt_screen_t *screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t count);
static void mvt_d2d_screen_clear_rect(mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color);
static void mvt_d2d_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y);
static void mvt_d2d_screen_scroll(mvt_screen_t *screen, int y1, int y2, int count);
static void mvt_d2d_screen_beep(mvt_screen_t *screen);
static void mvt_d2d_screen_get_size(mvt_screen_t *screen, int *width, int *height);
static int mvt_d2d_screen_resize(mvt_screen_t *screen, int width, int height);
static void mvt_d2d_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws);
static void mvt_d2d_screen_set_scroll_info(mvt_screen_t *screen, int scroll_position, int virtual_height);
static void mvt_d2d_screen_set_mode(mvt_screen_t *screen, int mode, int value);
static int mvt_win32_set_screen_attribute0(mvt_d2d_screen_t *d2d_screen, const char *name, const char *value);

static const mvt_screen_vt_t d2d_screen_vt = {
    mvt_d2d_screen_begin,
    mvt_d2d_screen_end,
    mvt_d2d_screen_draw_text,
    mvt_d2d_screen_clear_rect,
    mvt_d2d_screen_scroll,
    mvt_d2d_screen_move_cursor,
    mvt_d2d_screen_beep,
    mvt_d2d_screen_get_size,
    mvt_d2d_screen_resize,
    mvt_d2d_screen_set_title,
    mvt_d2d_screen_set_scroll_info,
    mvt_d2d_screen_set_mode
};

int mvt_d2d_screen_init(mvt_d2d_screen_t *d2d_screen)
{
    memset(d2d_screen, 0, sizeof *d2d_screen);
    d2d_screen->parent.vt = &d2d_screen_vt;
    d2d_screen->foreground_color = 0xffffffff;
    d2d_screen->background_color = 0;
    d2d_screen->selection_color = 0xffff0000;
    d2d_screen->background_transparent = true;
    d2d_screen->scroll_position = 0;
    d2d_screen->flags = MVT_D2D_SCREEN_PSEUDOBOLD;
    d2d_screen->selection_x1 = -1;
    d2d_screen->selection_y1 = -1;
    d2d_screen->selection_x2 = -1;
    d2d_screen->selection_y2 = -1;
    return 0;
}

void
mvt_d2d_screen_destroy(mvt_d2d_screen_t *d2d_screen)
{
    free(d2d_screen->font_name);
    memset(d2d_screen, 0, sizeof *d2d_screen);
}

static void *mvt_d2d_screen_begin(mvt_screen_t *screen)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    return (void *)1;
}

static void mvt_d2d_screen_end(mvt_screen_t *screen, void *gc)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
}

static void
mvt_d2d_screen_draw_text (mvt_screen_t *screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t count)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    mvt_color_t color;
    unsigned int color_value;
    UINT32 win32_foreground_color, win32_background_color;
    float background_alpha;
    WCHAR wbuf[100];
    int cbstring;
    float dx[100];
    int start_x;
    float px;
    int on_cursor, start_on_cursor;
    const mvt_attribute_t *start_attribute;
    D2D1_RECT_F rect;
    mvt_d2d_gc_line_t gc_line;

    assert(x >= 0 && x + (int)count <= d2d_screen->width);
    assert(y >= 0 && y < d2d_screen->virtual_height);

    if (mvt_d2d_begin_draw_line(d2d_screen->user_data, x, count, y, &gc_line) < 0)
        return;
    if (!gc_line.render_target)
    {
        mvt_d2d_end_draw_line(d2d_screen->user_data, &gc_line);
        return;
    }
    gc_line.render_target->Clear(D2D1::ColorF(0x000000, 0.0));
    while (count) {
        start_attribute = attribute;
        cbstring = 0;
        start_x = x;
        start_on_cursor = x == d2d_screen->cursor_x && y == d2d_screen->cursor_y;
        do {
            if (attribute->no_char) {
                ws++;
                attribute++;
                x++;
                if (cbstring)
                    dx[cbstring-1] = 2*gc_line.cell_width;
                continue;
            }
            if (cbstring + 2 >= 100)
                break;
            if (attribute->foreground_color != start_attribute->foreground_color ||
                attribute->background_color != start_attribute->background_color ||
                attribute->reverse != start_attribute->reverse)
                break;
            if (cbstring) {
                if (x == d2d_screen->selection_x1 && y == d2d_screen->selection_y1)
                    break;
                if (x == d2d_screen->selection_x2 + 1 && y == d2d_screen->selection_y2)
                    break;
            }
            on_cursor = x == d2d_screen->cursor_x && y == d2d_screen->cursor_y;
            if (on_cursor != start_on_cursor)
                break;
            if (*ws < 0x20) {
                wbuf[cbstring] = L' ';
            } else {
                wbuf[cbstring] = *ws;
            }
            dx[cbstring] = gc_line.cell_width;
            attribute++;
            ws++;
            cbstring++;
            x++;
        } while (--count);

        color = start_attribute->foreground_color;
        color_value = color == MVT_DEFAULT_COLOR ?
            d2d_screen->foreground_color : mvt_color_value(color);
        win32_foreground_color = color_value;
        color = start_attribute->background_color;
        color_value = color == MVT_DEFAULT_COLOR ?
            d2d_screen->background_color : mvt_color_value(color);
        win32_background_color = color_value;
        background_alpha = (color == MVT_DEFAULT_COLOR && d2d_screen->background_transparent) ? 0.0f : 1.0f;

        if ((y > d2d_screen->selection_y1 ||
             (y == d2d_screen->selection_y1 && start_x >= d2d_screen->selection_x1)) &&
            (y < d2d_screen->selection_y2 ||
             (y == d2d_screen->selection_y2 && start_x <= d2d_screen->selection_x2))) {
            UINT32 swap_color;
            swap_color = win32_background_color;
            win32_background_color = win32_foreground_color;
            win32_foreground_color = swap_color;
            background_alpha = 1.0f;
        }
            
        if (start_on_cursor || start_attribute->reverse) {
            UINT32 swap_color;
            swap_color = win32_background_color;
            win32_background_color = win32_foreground_color;
            win32_foreground_color = swap_color;
            background_alpha = 1.0f;
        }

        px = start_x * gc_line.cell_width;

        rect = D2D1::RectF(
            px,
            0,
            x * gc_line.cell_width,
            30);
        ID2D1SolidColorBrush *brush;
        gc_line.render_target->CreateSolidColorBrush(
			D2D1::ColorF(win32_background_color, background_alpha),
			&brush);
        gc_line.render_target->FillRectangle(&rect, brush);
        brush->Release();
        gc_line.render_target->CreateSolidColorBrush(
			D2D1::ColorF(win32_foreground_color, 1.0f),
			&brush);
        IDWriteTextLayout *textLayout;
        gc_line.dwrite_factory->CreateGdiCompatibleTextLayout(wbuf, cbstring, gc_line.dwrite_text_format, x * gc_line.cell_width - px, 30, 1.0, NULL, true, &textLayout);
        gc_line.render_target->DrawTextLayout(D2D1::Point2F(px, 0.0f), textLayout, brush);
        textLayout->Release();
#if 0
        gc_line.render_target->DrawText(wbuf, cbstring, d2d_screen->text_format, &rect, brush);
#endif
        brush->Release();
    }
    mvt_d2d_end_draw_line(d2d_screen->user_data, &gc_line);
}

static void
mvt_d2d_screen_clear_rect (mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    UINT32 d2d1_color_value;
    FLOAT background_alpha;
    mvt_d2d_gc_line_t gc_line;

    assert(x1 >= 0 && x2 >= x1 && d2d_screen->width > x2);
    assert(y1 >= 0 && y2 >= y1 && d2d_screen->virtual_height > y2);

    for (int i = y1; i <= y2; i++) {
        if (mvt_d2d_begin_draw_line(d2d_screen->user_data, x1, x2 - x1 + 1, i, &gc_line) < 0)
            return;
        if (!gc_line.render_target)
        {
            mvt_d2d_end_draw_line(d2d_screen->user_data, &gc_line);
            return;
        }
        gc_line.render_target->Clear(D2D1::ColorF(0x000000, 0.0));

        D2D1_RECT_F rect = D2D1::RectF(
            x1 * gc_line.cell_width,
            0,
            (x2 + 1) * gc_line.cell_width,
            30);
        d2d1_color_value = background_color == MVT_DEFAULT_COLOR ? d2d_screen->background_color : mvt_color_value(background_color);
        background_alpha = (background_color == MVT_DEFAULT_COLOR && d2d_screen->background_transparent) ? 0.0f : 1.0f;
        ID2D1SolidColorBrush *brush;
        gc_line.render_target->CreateSolidColorBrush(
			D2D1::ColorF(d2d1_color_value, background_alpha),
			&brush);
        brush->Release();
        mvt_d2d_end_draw_line(d2d_screen->user_data, &gc_line);
    }
}

static void
mvt_d2d_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    switch (cursor) {
    case MVT_CURSOR_CURRENT:
        d2d_screen->cursor_x = x;
        d2d_screen->cursor_y = y;
        break;
    case MVT_CURSOR_SELECTION_START:
        d2d_screen->selection_x1 = x;
        d2d_screen->selection_y1 = y;
        break;
    case MVT_CURSOR_SELECTION_END:
        d2d_screen->selection_x2 = x;
        d2d_screen->selection_y2 = y;
        break;
    }
}

static void
mvt_d2d_screen_scroll (mvt_screen_t *screen, int y1, int y2, int count)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    mvt_d2d_screen_scroll(d2d_screen->user_data, y1, y2, count);
    d2d_screen->line_modified = 1;
}

static void mvt_d2d_screen_beep(mvt_screen_t *screen)
{
    MVT_DEBUG_PRINT1("mvt_d2d_screen_beep\n");
    //MessageBeep(-1);
}

static void mvt_d2d_screen_get_size(mvt_screen_t *screen, int *width, int *height)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    *width = d2d_screen->width;
    *height = d2d_screen->height;
}

static int mvt_d2d_screen_resize(mvt_screen_t *screen, int width, int height)
{
    /* The store app doesn't support resize */
    return 0;
}

static void mvt_d2d_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    int i;
    TCHAR tbuf[256];
    if (ws) {
        for (i = 0; i < 256; i++) {
            mvt_char_t wc = *ws++;
            tbuf[i] = wc;
            if (wc == '\0')
                break;
        }
    } else {
        memcpy(tbuf, TEXT("(no title)"), sizeof (TCHAR) * 4);
    }
    mvt_d2d_screen_set_title(d2d_screen->user_data, tbuf);
}

static void
mvt_d2d_screen_set_scroll_info (mvt_screen_t *screen, int scroll_position, int virtual_height)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    d2d_screen->virtual_height = virtual_height;
    d2d_screen->scroll_position = scroll_position;
    d2d_screen->scroll_position_modified = 1;
}

static void mvt_d2d_screen_set_mode(mvt_screen_t *screen, int mode, int value)
{
}

static int mvt_win32_update_screen(mvt_d2d_screen_t *d2d_screen)
{
    return 0;
}

mvt_screen_t *mvt_d2d_open_screen(char **args, void *user_data)
{
    mvt_d2d_screen_t *d2d_screen;
    const char *name, *value;
    char **p;

    d2d_screen = (mvt_d2d_screen_t *)malloc(sizeof (mvt_d2d_screen_t));
    if (!d2d_screen)
        return NULL;
    if (mvt_d2d_screen_init(d2d_screen) == -1) {
        free(d2d_screen);
        return NULL;
    }
    d2d_screen->user_data = user_data;
    d2d_screen->width = 80;
    d2d_screen->height = 24;
    d2d_screen->font_size = 12;
    p = args;
    while (*p) {
        name = *p++;
        if (!*p) {
            mvt_d2d_screen_destroy(d2d_screen);
            free(d2d_screen);
            return NULL;
        }
        value = *p++;
        if (mvt_win32_set_screen_attribute0(d2d_screen, name, value) == -1) {
            free(d2d_screen);
            return NULL;
        }
    }
    if (mvt_win32_update_screen(d2d_screen) == -1) {
        free(d2d_screen);
        return NULL;
    }
    return (mvt_screen_t *)d2d_screen;
}

void mvt_win32_close_screen(mvt_screen_t *screen)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    //mvt_screen_dispatch_close(screen);
    mvt_d2d_screen_destroy(d2d_screen);
    free(screen);
}

static int
mvt_win32_set_screen_attribute0 (mvt_d2d_screen_t *d2d_screen, const char *name, const char *value)
{
    if (strcmp(name, "width") == 0)
        d2d_screen->width = atoi(value);
    else if (strcmp(name, "height") == 0)
        d2d_screen->height = atoi(value);
    else if (strcmp(name, "font-name") == 0) {
        char *new_value = strdup(value);
        if (new_value == NULL)
            return -1;
        d2d_screen->font_name = new_value;
    } else if (strcmp(name, "font-size") == 0) {
        d2d_screen->font_size = atoi(value);
    } else if (strcmp(name, "foreground-color") == 0)
        d2d_screen->foreground_color = mvt_atocolor(value);
    else if (strcmp(name, "background-color") == 0)
        d2d_screen->background_color = mvt_atocolor(value);
    return 0;
}

int
mvt_set_screen_attribute (mvt_screen_t *screen, const char *name, const char *value)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    if (mvt_win32_set_screen_attribute0(d2d_screen, name, value) == -1)
        return -1;
    if (mvt_win32_update_screen(d2d_screen) == -1)
        return -1;
    return 0;
}

static void
mvt_d2d_screen_scroll_to (mvt_d2d_screen_t *d2d_screen, int new_pos)
{
#if 0
    HWND hwnd = d2d_screen->hwnd;
    RECT rcUpdate;
    int count;

	if (new_pos < 0)
		new_pos = 0;
	if (new_pos + d2d_screen->height >= d2d_screen->virtual_height)
		new_pos = d2d_screen->virtual_height - d2d_screen->height;

	count = new_pos - d2d_screen->scroll_position;
    if (count >= 1 && count < d2d_screen->height) {
        ScrollWindowEx(hwnd, 0, -count * d2d_screen->font_height, NULL, NULL, NULL, &rcUpdate, 0);
    } else if (count <= -1 && count > -d2d_screen->height) {
        ScrollWindowEx(hwnd, 0, -count * d2d_screen->font_height, NULL, NULL, NULL, &rcUpdate, 0);
    } else if (count != 0) {
        GetClientRect(hwnd, &rcUpdate);
    }
    d2d_screen->scroll_position = new_pos;
    InvalidateRect(hwnd, &rcUpdate, FALSE);
#endif
}

#define mvt_d2d_screen_scroll_by(d2d_screen, delta) mvt_d2d_screen_scroll_to((d2d_screen), (d2d_screen)->scroll_position + (delta))

void
mvt_d2d_screen_notify_resize(mvt_screen_t *screen, int width, int height)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    d2d_screen->width = width;
    d2d_screen->height = height;
}

int
mvt_d2d_screen_get_scroll_info(mvt_screen_t *screen, int *scroll_position, int *virtual_height)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    int result = d2d_screen->scroll_position_modified;
    *scroll_position = d2d_screen->scroll_position;
    *virtual_height = d2d_screen->virtual_height;
    d2d_screen->scroll_position_modified = 0;
    return result;
}

int
mvt_d2d_screen_get_line_info(mvt_screen_t *screen)
{
    mvt_d2d_screen_t *d2d_screen = (mvt_d2d_screen_t *)screen;
    int result = d2d_screen->line_modified;
    d2d_screen->line_modified = 0;
    return result;
}
