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
#define UNICODE
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <assert.h>
#include <mvt/mvt.h>
#include "misc.h"
#include "debug.h"
#include "driver.h"
#define IDI_MVTC 100

#define MVT_WIN32_SCREEN_ALWAYSBRIGHT (1<<0)
#define MVT_WIN32_SCREEN_PSEUDOBOLD   (1<<1)

#define GETMVTWINDOW(hwnd) ((mvt_win32_screen_t *)(LONG_PTR)GetWindowLongPtr((hwnd), 0))
#define WM_MVT_REQUEST (WM_USER+0)

typedef struct _mvt_win32_screen mvt_win32_screen_t;

struct _mvt_win32_screen {
    mvt_screen_t parent;
    HWND hwnd;
    HFONT hfont;

    char *font_name;
    int font_size;
    int cell_width, cell_height;
    int font_baseline;

    uint32_t foreground_color;
    uint32_t background_color;
    uint32_t selection_color;

    int width, height, virtual_height;
    int cursor_x, cursor_y;
    int scroll_position;
    int selection_x1, selection_y1;
    int selection_x2, selection_y2;

    unsigned int flags;
};

static HWND message_hwnd;
static mvt_event_func_t global_event_func = NULL;
static int loop;

/* utilities */
static COLORREF mvt_color_value_to_win32(unsigned int color_value);
static int mvt_vk_from_win32(UINT nChar, BOOL bOverride);

/* mvt_win32_screen_t */

static void *mvt_win32_screen_begin(mvt_screen_t *screen);
static void mvt_win32_screen_end(mvt_screen_t *screen, void *gc);
static void mvt_win32_screen_draw_text(mvt_screen_t *screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t count);
static void mvt_win32_screen_clear_rect(mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color);
static void mvt_win32_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y);
static void mvt_win32_screen_scroll(mvt_screen_t *screen, int y1, int y2, int count);
static void mvt_win32_screen_beep(mvt_screen_t *screen);
static void mvt_win32_screen_get_size(mvt_screen_t *screen, int *width, int *height);
static int mvt_win32_screen_resize(mvt_screen_t *screen, int width, int height);
static void mvt_win32_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws);
static void mvt_win32_screen_set_scroll_info(mvt_screen_t *screen, int scroll_position, int virtual_height);
static void mvt_win32_screen_set_mode(mvt_screen_t *screen, int mode, int value);
static int mvt_win32_set_screen_attribute0(mvt_win32_screen_t *win32_screen, const char *name, const char *value);

static const mvt_screen_vt_t win32_screen_vt = {
    mvt_win32_screen_begin,
    mvt_win32_screen_end,
    mvt_win32_screen_draw_text,
    mvt_win32_screen_clear_rect,
    mvt_win32_screen_scroll,
    mvt_win32_screen_move_cursor,
    mvt_win32_screen_beep,
    mvt_win32_screen_get_size,
    mvt_win32_screen_resize,
    mvt_win32_screen_set_title,
    mvt_win32_screen_set_scroll_info,
    mvt_win32_screen_set_mode
};

int mvt_win32_screen_init(mvt_win32_screen_t *win32_screen)
{
    memset(win32_screen, 0, sizeof *win32_screen);
    win32_screen->parent.vt = &win32_screen_vt;
    win32_screen->foreground_color = 0xffffff;
    win32_screen->background_color = 0;
    win32_screen->selection_color = 0xff0000;
    win32_screen->scroll_position = 0;
    win32_screen->flags = MVT_WIN32_SCREEN_PSEUDOBOLD;
    win32_screen->selection_x1 = -1;
    win32_screen->selection_y1 = -1;
    win32_screen->selection_x2 = -1;
    win32_screen->selection_y2 = -1;
    return 0;
}

void
mvt_win32_screen_destroy(mvt_win32_screen_t *win32_screen)
{
    DeleteObject(win32_screen->hfont);
    free(win32_screen->font_name);
    memset(win32_screen, 0, sizeof *win32_screen);
}

static void *mvt_win32_screen_begin(mvt_screen_t *screen)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    HDC hdc = GetDC(win32_screen->hwnd);
    return (void *)hdc;
}

static void mvt_win32_screen_end(mvt_screen_t *screen, void *gc)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    HDC hdc = (HDC)gc;
    ReleaseDC(win32_screen->hwnd, hdc);
}

static void
mvt_win32_screen_draw_text (mvt_screen_t *screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t count)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    HDC hdc = (HDC)gc;
    mvt_color_t color;
    unsigned int color_value;
    COLORREF win32_foreground_color, win32_background_color;
    WCHAR wbuf[100];
    int cbstring;
    INT dx[100];
    RECT rc;
    HFONT hfontOld;
    int start_x;
    int px, py;
    int on_cursor, start_on_cursor;
    const mvt_attribute_t *start_attribute;

    assert(x >= 0 && x + count <= win32_screen->width);
    assert(y >= 0 && y < win32_screen->virtual_height);

    hfontOld = SelectObject(hdc, win32_screen->hfont);
    if (y < win32_screen->scroll_position || y >= win32_screen->scroll_position + win32_screen->height)
        return;
    while (count) {
        start_attribute = attribute;
        cbstring = 0;
        start_x = x;
        start_on_cursor = x == win32_screen->cursor_x && y == win32_screen->cursor_y;
        do {
            if (attribute->no_char) {
                ws++;
                attribute++;
                x++;
                if (cbstring)
                    dx[cbstring-1] = 2*win32_screen->cell_width;
                continue;
            }
            if (cbstring + 2 >= 100)
                break;
            if (attribute->foreground_color != start_attribute->foreground_color ||
                attribute->background_color != start_attribute->background_color ||
                attribute->reverse != start_attribute->reverse)
                break;
            if (cbstring) {
                if (x == win32_screen->selection_x1 && y == win32_screen->selection_y1)
                    break;
                if (x == win32_screen->selection_x2 + 1 && y == win32_screen->selection_y2)
                    break;
            }
            on_cursor = x == win32_screen->cursor_x && y == win32_screen->cursor_y;
            if (on_cursor != start_on_cursor)
                break;
            if (*ws < 0x20) {
                wbuf[cbstring] = L' ';
            } else {
                wbuf[cbstring] = *ws;
            }
            dx[cbstring] = win32_screen->cell_width;
            attribute++;
            ws++;
            cbstring++;
            x++;
        } while (--count);

        color = start_attribute->foreground_color;
        color_value = color == MVT_DEFAULT_COLOR ?
            win32_screen->foreground_color : mvt_color_value(color);
        win32_foreground_color = mvt_color_value_to_win32(color_value);
        color = start_attribute->background_color;
        color_value = color == MVT_DEFAULT_COLOR ?
            win32_screen->background_color : mvt_color_value(color);
        win32_background_color = mvt_color_value_to_win32(color_value);

        if ((y > win32_screen->selection_y1 ||
             (y == win32_screen->selection_y1 && start_x >= win32_screen->selection_x1)) &&
            (y < win32_screen->selection_y2 ||
             (y == win32_screen->selection_y2 && start_x <= win32_screen->selection_x2))) {
            COLORREF swap_color;
            swap_color = win32_background_color;
            win32_background_color = win32_foreground_color;
            win32_foreground_color = swap_color;
        }
            
        if (start_on_cursor || start_attribute->reverse) {
            COLORREF swap_color;
            swap_color = win32_background_color;
            win32_background_color = win32_foreground_color;
            win32_foreground_color = swap_color;
        }

        SetTextColor(hdc, win32_foreground_color);
        SetBkColor(hdc, win32_background_color);
        px = start_x * win32_screen->cell_width;
        py = (y - win32_screen->scroll_position) * win32_screen->cell_height;
        rc.left = px;
        rc.right = x * win32_screen->cell_width;
        rc.top = py;
        rc.bottom = py + win32_screen->cell_height;
        ExtTextOutW(hdc, px, py, ETO_OPAQUE | ETO_CLIPPED, &rc, wbuf, cbstring, dx);
    }
    SelectObject(hdc, hfontOld);
}

static void
mvt_win32_screen_clear_rect (mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    uint32_t color_value;
    RECT rect;
    HDC hdc = (HDC)gc;
    HBRUSH hbrush;
    assert(x1 >= 0 && x2 >= x1 && win32_screen->width > x2);
    assert(y1 >= 0 && y2 >= y1 && win32_screen->virtual_height > y2);
	if (y1 < win32_screen->scroll_position)
		y1 = win32_screen->scroll_position;
	if (y2 >= win32_screen->scroll_position + win32_screen->virtual_height)
		y2 = win32_screen->scroll_position + win32_screen->virtual_height - 1;
	if (y1 > y2)
		return;
    rect.left = x1 * win32_screen->cell_width;
    rect.top = (y1 - win32_screen->scroll_position) * win32_screen->cell_height;
    rect.right = (x2 + 1) * win32_screen->cell_width;
    rect.bottom = (y2 - win32_screen->scroll_position + 1) * win32_screen->cell_height;
    if (background_color == MVT_DEFAULT_COLOR)
        color_value = win32_screen->background_color;
    else
        color_value = mvt_color_value(background_color);
    hbrush = CreateSolidBrush(mvt_color_value_to_win32(color_value));
    FillRect(hdc, &rect, hbrush);
    DeleteObject(hbrush);
}

static void
mvt_win32_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    switch (cursor) {
    case MVT_CURSOR_CURRENT:
        win32_screen->cursor_x = x;
        win32_screen->cursor_y = y;
        break;
    case MVT_CURSOR_SELECTION_START:
        win32_screen->selection_x1 = x;
        win32_screen->selection_y1 = y;
        break;
    case MVT_CURSOR_SELECTION_END:
        win32_screen->selection_x2 = x;
        win32_screen->selection_y2 = y;
        break;
    }
}

static void
mvt_win32_screen_scroll (mvt_screen_t *screen, int y1, int y2, int count)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    HWND hwnd = win32_screen->hwnd;
    RECT rect;
    RECT rcUpdate;

    if (y1 == -1)
        y1 = 0;
    if (y2 == -1)
        y2 = win32_screen->virtual_height - 1;

    if (y1 < win32_screen->scroll_position)
        y1 = win32_screen->scroll_position;
    if (y2 >= win32_screen->scroll_position + win32_screen->height)
        y2 = win32_screen->scroll_position + win32_screen->height;

    if (y1 > y2)
        return;

    y1 -= win32_screen->scroll_position;
    y2 -= win32_screen->scroll_position;

    if (y1 == 0 && y2 == win32_screen->height - 1) {
        ScrollWindowEx(hwnd, 0, count * win32_screen->cell_height, NULL, NULL, NULL, &rcUpdate, 0);
    } else {
        GetClientRect(hwnd, &rect);
        rect.top = y1 * win32_screen->cell_height;
        rect.bottom = (y2 + 1) * win32_screen->cell_height;
        ScrollWindowEx(hwnd, 0, count * win32_screen->cell_height, &rect, &rect, NULL, &rcUpdate, 0);
    }
    InvalidateRect(hwnd, &rcUpdate, FALSE);
}

static void mvt_win32_screen_beep(mvt_screen_t *screen)
{
    MVT_DEBUG_PRINT1("mvt_win32_screen_beep\n");
    MessageBeep(-1);
}

static void mvt_win32_screen_get_size(mvt_screen_t *screen, int *width, int *height)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    *width = win32_screen->width;
    *height = win32_screen->height;
}

static int mvt_win32_screen_resize(mvt_screen_t *screen, int width, int height)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    HWND hwnd = win32_screen->hwnd;
    RECT rect;
    int top, left;
    int cx, cy;
    GetWindowRect(hwnd, &rect);
    left = rect.left;
    top = rect.top;
    cx = win32_screen->cell_width * width
        + GetSystemMetrics(SM_CXVSCROLL);
    cy = win32_screen->cell_height * height;
    SetRect(&rect, 0, 0, cx, cy);
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_THICKFRAME,
                       FALSE, WS_EX_CLIENTEDGE);
    MoveWindow(hwnd, left, top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    win32_screen->width = width;
    win32_screen->height = height;
    return 0;
}

static void mvt_win32_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    HWND hwnd = win32_screen->hwnd;
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
        memcpy(tbuf, TEXT("mvt"), sizeof (TCHAR) * 4);
    }
    SetWindowText(hwnd, tbuf);
}

static void
mvt_win32_screen_set_scroll_info (mvt_screen_t *screen, int scroll_position, int virtual_height)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    HWND hwnd = win32_screen->hwnd;
    SCROLLINFO si;
    int count;
    RECT rcUpdate;
  
    si.cbSize = sizeof si;
    si.fMask = SIF_POS | SIF_DISABLENOSCROLL;
    si.nPos = scroll_position;
#if 1
    si.fMask  = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
    si.nMin   = 0;
    si.nMax   = virtual_height - 1;
    si.nPage  = win32_screen->height;
#endif
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    win32_screen->virtual_height = virtual_height;
    count = win32_screen->scroll_position - scroll_position;
    win32_screen->scroll_position = scroll_position;
    if (count != 0) {
        ScrollWindowEx(hwnd, 0, count * win32_screen->cell_height, NULL, NULL, NULL, &rcUpdate, 0);
        InvalidateRect(hwnd, &rcUpdate, FALSE);
    }
}

static void mvt_win32_screen_set_mode(mvt_screen_t *screen, int mode, int value)
{
}

static void
mvt_win32_screen_convert_size (const mvt_win32_screen_t *win32_screen, int px, int py, int *width, int *height)
{
    int cell_width = win32_screen->cell_width;
    int cell_height = win32_screen->cell_height;
    if (width != NULL) *width = px / cell_width;
    if (height != NULL) *height = py / cell_height;
}

static void
mvt_win32_screen_convert_point (const mvt_win32_screen_t *win32_screen, int px, int py, int *x, int *y, int *align)
{
    int cell_width = win32_screen->cell_width;
    int cell_height = win32_screen->cell_height;
    if (x != NULL) *x = (px >= 0) ? px / cell_width : (px - cell_width + 1) / cell_width;
    if (align != NULL) *align = ((px * 2 / cell_width) & 1) ? 1 : -1;
    if (y != NULL) *y = ((py >= 0) ? py / cell_height : (py - cell_height + 1) / cell_height) + win32_screen->scroll_position;
}

static HINSTANCE hInst;

static int mvt_win32_update_screen(mvt_win32_screen_t *win32_screen)
{
    int cx, cy;
    HDC hdc;
    HFONT hfont, hfontOld;
    TEXTMETRIC metric;
    RECT rect;
    HWND hwnd;
	LPCSTR szFontName;

	szFontName = win32_screen->font_name ? win32_screen->font_name : "FixedSys";

    hfont = CreateFontA(
        -win32_screen->font_size,
        0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH, szFontName);
    if (hfont == NULL)
        return -1;
    hwnd = GetDesktopWindow();
    hdc = GetDC(hwnd);
    hfontOld = SelectObject(hdc, hfont);
    GetTextMetrics(hdc, &metric);
    SelectObject(hdc, hfontOld);
    ReleaseDC(hwnd, hdc);
    if (win32_screen->hfont != NULL)
        DeleteObject(win32_screen->hfont);
    win32_screen->hfont = hfont;
    win32_screen->cell_width = metric.tmAveCharWidth;
    win32_screen->cell_height = metric.tmHeight;
    win32_screen->font_baseline = metric.tmAscent;

    cx = win32_screen->cell_width * win32_screen->width
        + GetSystemMetrics(SM_CXVSCROLL);
    cy = win32_screen->cell_height * win32_screen->height;
    SetRect(&rect, 0, 0, cx, cy);
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_THICKFRAME,
                       FALSE, WS_EX_CLIENTEDGE);
    
    if (win32_screen->hwnd == NULL) {
        hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("MVT"),
                              TEXT("mvt"),
                              WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_THICKFRAME,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rect.right - rect.left, rect.bottom - rect.top,
                              NULL, NULL, hInst, NULL);
        if (hwnd == NULL)
            return -1;
        SetWindowLongPtr(hwnd, 0, PtrToLong(win32_screen));
        message_hwnd = hwnd;
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        win32_screen->hwnd = hwnd;
    } else {
        RECT win_rect;

        hwnd = win32_screen->hwnd;
        GetWindowRect(hwnd, &win_rect);
        win_rect.right = win_rect.left + rect.right - rect.left;
        win_rect.bottom = win_rect.top + rect.bottom - rect.top;
        MoveWindow(hwnd, win_rect.left, win_rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    }

    return 0;
}

static mvt_screen_t *mvt_win32_open_screen(char **args)
{
    mvt_win32_screen_t *win32_screen;
    const char *name, *value;
    char **p;

    win32_screen = malloc(sizeof (mvt_win32_screen_t));
    if (mvt_win32_screen_init(win32_screen) == -1) {
        free(win32_screen);
        return NULL;
    }
    win32_screen->width = 80;
    win32_screen->height = 24;
    win32_screen->font_size = 12;
    p = args;
    while (*p) {
        name = *p++;
        if (!*p) {
            mvt_win32_screen_destroy(win32_screen);
            free(win32_screen);
            return NULL;
        }
        value = *p++;
        if (mvt_win32_set_screen_attribute0(win32_screen, name, value) == -1) {
            free(win32_screen);
            return NULL;
        }
    }
    if (mvt_win32_update_screen(win32_screen) == -1) {
        free(win32_screen);
        return NULL;
    }
    return (mvt_screen_t *)win32_screen;
}

static void mvt_win32_close_screen(mvt_screen_t *screen)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    mvt_screen_dispatch_close(screen);
    mvt_win32_screen_destroy(win32_screen);
    free(screen);
}

static int
mvt_win32_set_screen_attribute0 (mvt_win32_screen_t *win32_screen, const char *name, const char *value)
{
    if (strcmp(name, "width") == 0)
        win32_screen->width = atoi(value);
    else if (strcmp(name, "height") == 0)
        win32_screen->height = atoi(value);
    else if (strcmp(name, "font-name") == 0) {
        char *new_value = strdup(value);
        if (new_value == NULL)
            return -1;
        win32_screen->font_name = new_value;
    } else if (strcmp(name, "font-size") == 0) {
        win32_screen->font_size = atoi(value);
    } else if (strcmp(name, "foreground-color") == 0)
        win32_screen->foreground_color = mvt_atocolor(value);
    else if (strcmp(name, "background-color") == 0)
        win32_screen->background_color = mvt_atocolor(value);
    return 0;
}

static int mvt_win32_set_screen_attribute(mvt_screen_t *screen, const char *name, const char *value)
{
    mvt_win32_screen_t *win32_screen = (mvt_win32_screen_t *)screen;
    if (mvt_win32_set_screen_attribute0(win32_screen, name, value) == -1)
        return -1;
    if (mvt_win32_update_screen(win32_screen) == -1)
        return -1;
    return 0;
}

void mvt_notify_request(void)
{
    PostMessage(message_hwnd, WM_MVT_REQUEST, 0, 0);
}

static void
mvt_win32_screen_scroll_to (mvt_win32_screen_t *win32_screen, int new_pos)
{
    HWND hwnd = win32_screen->hwnd;
    RECT rcUpdate;
    int count;

	if (new_pos < 0)
		new_pos = 0;
	if (new_pos + win32_screen->height >= win32_screen->virtual_height)
		new_pos = win32_screen->virtual_height - win32_screen->height;

	count = new_pos - win32_screen->scroll_position;
    if (count >= 1 && count < win32_screen->height) {
        ScrollWindowEx(hwnd, 0, -count * win32_screen->cell_height, NULL, NULL, NULL, &rcUpdate, 0);
    } else if (count <= -1 && count > -win32_screen->height) {
        ScrollWindowEx(hwnd, 0, -count * win32_screen->cell_height, NULL, NULL, NULL, &rcUpdate, 0);
    } else if (count != 0) {
        GetClientRect(hwnd, &rcUpdate);
    }
    win32_screen->scroll_position = new_pos;
    InvalidateRect(hwnd, &rcUpdate, FALSE);
}

#define mvt_win32_screen_scroll_by(win32_screen, delta) mvt_win32_screen_scroll_to((win32_screen), (win32_screen)->scroll_position + (delta))

static int OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    mvt_win32_screen_t a, *win32_screen = &a;
    HFONT hfont, hfontOld;
    HDC hdc;
    TEXTMETRIC metric;
    
    hfont = GetStockObject(SYSTEM_FIXED_FONT);
    hdc = GetDC(hwnd);
    hfontOld = SelectObject(hdc, hfont);
    GetTextMetrics(hdc, &metric);
    SelectObject(hdc, hfontOld);
    ReleaseDC(hwnd, hdc);
    win32_screen->hfont = hfont;
    win32_screen->cell_width = metric.tmAveCharWidth;
    win32_screen->cell_height = metric.tmHeight;
    win32_screen->font_baseline = metric.tmAscent;

    return 0;
}

static void OnSize(HWND hwnd, UINT nType, int cx, int cy)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    int width, height;

    if (nType == SIZE_MINIMIZED)
        return;

    mvt_win32_screen_convert_size(win32_screen, cx, cy, &width, &height);

    if (width < 16) height = 16;
    if (height < 2) height = 2;

    if (win32_screen->cell_width * width != cx ||
        win32_screen->cell_height * height != cy) {
        RECT rect;
        int top, left;
        GetWindowRect(hwnd, &rect);
        left = rect.left;
        top = rect.top;
        cx = win32_screen->cell_width * width
            + GetSystemMetrics(SM_CXVSCROLL);
        cy = win32_screen->cell_height * height;
        SetRect(&rect, 0, 0, cx, cy);
        AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_THICKFRAME,
                           FALSE, WS_EX_CLIENTEDGE);
        MoveWindow(hwnd, left, top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
        return;
    }
    win32_screen->width = width;
    win32_screen->height = height;
    mvt_screen_dispatch_resize((mvt_screen_t *)win32_screen);
}

static void
OnPaint(HWND hwnd)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    mvt_screen_dispatch_paint((mvt_screen_t *)win32_screen, (void *)hdc, 0, win32_screen->scroll_position, win32_screen->width - 1, win32_screen->scroll_position + win32_screen->height - 1);
    EndPaint(hwnd, &ps);
}

static void OnPaste(HWND hwnd)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    HANDLE hText;
    BYTE *lpText;

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
        return;
    if (!OpenClipboard(hwnd))
        return;
    hText = GetClipboardData(CF_UNICODETEXT);
    if (hText != NULL) {
        lpText = GlobalLock(hText);
        if (lpText != NULL) {
            size_t count = wcslen((wchar_t *)lpText);
            WCHAR *p;
            mvt_char_t *ws, *wp;
            ws = malloc(count * sizeof (mvt_char_t));
            p = (WCHAR *)lpText;
            wp = ws;
            while (*p != '\0') *wp++ = *p++;
            mvt_screen_dispatch_paste((mvt_screen_t *)win32_screen, ws, count);
            GlobalUnlock(hText);
        }
    }
    CloseClipboard();
}

static void
OnLButtonDown (HWND hwnd, UINT nFlags, short px, short py)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    int button = 0;
    int x, y, align;
    SetCapture(hwnd);
    mvt_win32_screen_convert_point(win32_screen, px, py, &x, &y, &align);
    mvt_screen_dispatch_mousebutton((mvt_screen_t *)win32_screen, TRUE, button, 0, x, y, align);
}

static void
OnLButtonUp (HWND hwnd, UINT nFlags, short px, short py)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    int button = 0;
    int x, y, align;
    mvt_win32_screen_convert_point(win32_screen, px, py, &x, &y, &align);
    mvt_screen_dispatch_mousebutton((mvt_screen_t *)win32_screen, FALSE, button, 0, x, y, align);
    ReleaseCapture();
}

static void OnRButtonDown(HWND hwnd, UINT nFlags, short px, short py)
{
    OnPaste(hwnd);
}

static void OnMouseMove(HWND hwnd, UINT nFlags, short px, short py)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    int x, y, align;
    if (hwnd != GetCapture()) return;
    mvt_win32_screen_convert_point(win32_screen, px, py, &x, &y, &align);
    mvt_screen_dispatch_mousemove((mvt_screen_t *)win32_screen, x, y, align);
}

static void
OnVScroll (HWND hwnd, UINT nSBCode, UINT nPos)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    SCROLLINFO si;

    switch (nSBCode) {
	case SB_LINEUP:
        mvt_win32_screen_scroll_by(win32_screen, -1);
		break;
	case SB_LINEDOWN:
        mvt_win32_screen_scroll_by(win32_screen, 1);
		break;
    case SB_THUMBTRACK:
        memset(&si, 0, sizeof (si));
        si.cbSize = sizeof si;
        si.fMask = SIF_TRACKPOS;
        if (!GetScrollInfo(hwnd, SB_VERT, &si))
            return;
        mvt_win32_screen_scroll_to(win32_screen, si.nTrackPos);
		return; /* Do not update scrollbar */
    case SB_THUMBPOSITION:
        memset(&si, 0, sizeof (si));
        si.cbSize = sizeof si;
        si.fMask = SIF_TRACKPOS;
        if (!GetScrollInfo(hwnd, SB_VERT, &si))
            return;
        mvt_win32_screen_scroll_to(win32_screen, nPos);
		break;
	}

	memset(&si, 0, sizeof (si));
	si.cbSize = sizeof si;
	si.nPos = win32_screen->scroll_position;
	si.fMask = SIF_POS;
	if (!SetScrollInfo(hwnd, SB_VERT, &si, TRUE))
		return;
}

static BOOL OnMouseWheel(HWND hwnd, UINT nFlags, short zDelta, short px, short py)
{
    int nCount;
    UINT nSBCode;
	int i;

    nSBCode = zDelta > 0 ? SB_LINEUP : SB_LINEDOWN;
    nCount = zDelta > 0 ? zDelta : -zDelta;
    for (i = 0; i < nCount; i += WHEEL_DELTA) {
		OnVScroll(hwnd, nSBCode, 0);
    }
    return TRUE;
}

static void OnChar(HWND hwnd, UINT nChar, UINT nRepCnt, UINT nFlags)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    int meta = FALSE;
    int code = nChar;
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        if (code == ' ')
            code = '\0'; /* Ctrl+Space */
    }
    if (GetKeyState(VK_MENU) & 0x8000) {
        meta = TRUE;
    }
    MVT_DEBUG_PRINT3("OnChar: %d %d\n", nChar, nFlags);
    mvt_screen_dispatch_keydown((mvt_screen_t *)win32_screen, meta, code);
}

static void OnKeyDown(HWND hwnd, UINT nChar, UINT nRepCnt, UINT nFlags)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    int meta = FALSE;
    int code;
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        switch (nChar) {
        case VK_NEXT:
            OnVScroll(hwnd, SB_PAGEDOWN, 0);
            return;
        case VK_PRIOR:
            OnVScroll(hwnd, SB_PAGEUP, 0);
            return;
        }
    }
    MVT_DEBUG_PRINT3("OnKeyDown: %d %d\n", nChar, nFlags);
    code = mvt_vk_from_win32(nChar, !(nFlags & (1 << 8)));
    if (code == -1)
        return;
    mvt_screen_dispatch_keydown((mvt_screen_t *)win32_screen, meta, code);
}

static void OnSysKeyDown(HWND hwnd, UINT nChar, UINT nRepCnt, UINT nFlags)
{
    mvt_win32_screen_t *win32_screen = GETMVTWINDOW(hwnd);
    int meta = FALSE;
    int code = -1;
    if (nFlags & (1 << 13)) {/* Alt key */
        meta = TRUE;
        if (nChar >= 'A' && nChar <= 'Z') {
            code = (nChar + ('a' - 'A'));
        } else {
            switch (nChar)
            {
            case VK_OEM_PLUS:
                code = '+';
                break;
            case VK_OEM_COMMA:
                code = ',';
                break;
            case VK_OEM_MINUS:
                code = '-';
                break;
            case VK_OEM_PERIOD:
                code = '.';
                break;
            case VK_OEM_1:
                code = ':';
                break;
            case VK_OEM_2:
                code = '/';
                break;
            case VK_OEM_3:
                code = '`';
                break;
            case VK_OEM_4:
                code = '[';
                break;
            case VK_OEM_5:
                code = '\\';
                break;
            case VK_OEM_6:
                code = ']';
                break;
            case VK_OEM_7:
                code = '\'';
                break;
            }
        }
    }
    if (code == -1)
        return;
    MVT_DEBUG_PRINT3("OnSysKeyDown: %d %d\n", nChar, nFlags);
    mvt_screen_dispatch_keydown((mvt_screen_t *)win32_screen, meta, code);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        return OnCreate(hwnd, (LPCREATESTRUCT)lparam);
    case WM_PAINT:
        OnPaint(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        OnSize(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_MOUSEWHEEL:
        OnMouseWheel(hwnd, LOWORD(wparam), HIWORD(wparam), LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_LBUTTONDOWN:
        OnLButtonDown(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_RBUTTONDOWN:
        OnRButtonDown(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_LBUTTONUP:
        OnLButtonUp(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_VSCROLL:
        OnVScroll(hwnd, LOWORD(wparam), HIWORD(wparam));
        return 0;
    case WM_CHAR:
        OnChar(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        break;
    case WM_KEYDOWN:
        OnKeyDown(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_KEYUP:
        return 0;
    case WM_MVT_REQUEST:
        mvt_handle_request();
        break;
    case WM_SYSKEYDOWN:
        OnSysKeyDown(hwnd, (UINT)wparam, LOWORD(lparam), HIWORD(lparam));
        return 0;
    default:
        return DefWindowProc(hwnd, message, wparam, lparam);
    }
    return 0;
}

static int mvt_win32_init(int *argc, char ***argv, mvt_event_func_t event_func)
{
    LPCTSTR lpszClassName = TEXT("MVT");
    WNDCLASS wndclass;

    hInst = GetModuleHandle(NULL);

    ZeroMemory(&wndclass, sizeof wndclass);
    wndclass.style = CS_VREDRAW | CS_HREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = sizeof (LONG);
    wndclass.hInstance = hInst;
    wndclass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MVTC));
    wndclass.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wndclass.hbrBackground = GetStockObject(NULL_BRUSH);
    wndclass.lpszMenuName  = NULL; //MAKEINTRESOURCE(IDR_MVTC);
    wndclass.lpszClassName = lpszClassName;
    if (!RegisterClass(&wndclass))
        return -1;

    global_event_func = event_func;
    mvt_worker_init(global_event_func);
    return 0;
}

static void
MyTranslateMessage(const MSG *lpMsg)
{
  if (lpMsg->message == WM_SYSKEYDOWN)
    {
      return;
    }
  if (lpMsg->message == WM_KEYDOWN)
    {
      switch (lpMsg->wParam)
        {
        case VK_NUMPAD0:
        case VK_NUMPAD1:
        case VK_NUMPAD2:
        case VK_NUMPAD3:
        case VK_NUMPAD4:
        case VK_NUMPAD5:
        case VK_NUMPAD6:
        case VK_NUMPAD7:
        case VK_NUMPAD8:
        case VK_NUMPAD9:
        case VK_MULTIPLY:
        case VK_ADD:
        case VK_SEPARATOR:
        case VK_SUBTRACT:
        case VK_DECIMAL:
        case VK_DIVIDE:
          /* These keys are translated by mvt_w32_screen_t */
          MVT_DEBUG_PRINT1("VK\n");
          return;
        }
    }
  TranslateMessage(lpMsg);
}

static void mvt_win32_main(void)
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        MyTranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void mvt_win32_main_quit(void)
{
    loop = FALSE;
}

void mvt_win32_exit(void)
{
    mvt_worker_exit();
}

static const mvt_driver_vt_t mvt_win32_driver_vt = {
    mvt_win32_init,
    mvt_win32_main,
    mvt_win32_main_quit,
    mvt_win32_exit,
    mvt_win32_open_screen,
    mvt_win32_close_screen,
    mvt_worker_open_terminal,
    mvt_worker_close_terminal,
    mvt_win32_set_screen_attribute,
    mvt_worker_set_terminal_attribute,
    mvt_worker_suspend,
    mvt_worker_resume,
    mvt_worker_shutdown
};

static const mvt_driver_t mvt_win32_driver = {
    &mvt_win32_driver_vt, "win32"
};

const mvt_driver_t *mvt_get_driver(void)
{
    return &mvt_win32_driver;
}

/* utilities */

static COLORREF mvt_color_value_to_win32(uint32_t color_value)
{
    return RGB((color_value >> 16) & 0xff, (color_value >> 8) & 0xff, color_value & 0xff);
}

static const BYTE w32vktovk_table[] =
  {
    0, /* MVT_KEYPAD_SPACE */
    0, /* MVT_KEYPAD_TAB */
    VK_EXECUTE, /* MVT_KEYPAD_ENTER */
    VK_F1, /* MVT_KEYPAD_PF1 */
    VK_F2, /* MVT_KEYPAD_PF2 */
    VK_F3, /* MVT_KEYPAD_PF3 */
    VK_F4, /* MVT_KEYPAD_PF4 */
    VK_HOME, /* MVT_KEYPAD_HOME */
    VK_LEFT, /* MVT_KEYPAD_LEFT */
    VK_UP, /* MVT_KEYPAD_UP */
    VK_RIGHT, /* MVT_KEYPAD_RIGHT */
    VK_DOWN, /* MVT_KEYPAD_DOWN */
    VK_PRIOR, /* MVT_KEYPAD_PRIOR */
    0, /* MVT_KEYPAD_PAGEUP */
    VK_NEXT, /* MVT_KEYPAD_NEXT */
    0, /* MVT_KEYPAD_PAGEDOWN */
    VK_END, /* MVT_KEYPAD_END */
    0, /* MVT_KEYPAD_BEGIN */
    VK_INSERT, /* MVT_KEYPAD_INSERT */
    0, /* MVT_KEYPAD_EQUAL */
    VK_MULTIPLY, /* MVT_KEYPAD_MULTIPLY */
    VK_ADD, /* MVT_KEYPAD_ADD */
    VK_SEPARATOR, /* MVT_KEYPAD_SEPARATOR */
    VK_SUBTRACT, /* MVT_KEYPAD_SUBTRACT */
    VK_DECIMAL, /* MVT_KEYPAD_DECIMAL */
    VK_DIVIDE, /* MVT_KEYPAD_DIVIDE */
    VK_NUMPAD0, /* MVT_KEYPAD_0 */
    VK_NUMPAD1, /* MVT_KEYPAD_1 */
    VK_NUMPAD2, /* MVT_KEYPAD_2 */
    VK_NUMPAD3, /* MVT_KEYPAD_3 */
    VK_NUMPAD4, /* MVT_KEYPAD_4 */
    VK_NUMPAD5, /* MVT_KEYPAD_5 */
    VK_NUMPAD6, /* MVT_KEYPAD_6 */
    VK_NUMPAD7, /* MVT_KEYPAD_7 */
    VK_NUMPAD8, /* MVT_KEYPAD_8 */
    VK_NUMPAD9, /* MVT_KEYPAD_9 */
    VK_F1, /* MVT_KEYPAD_F1 */
    VK_F2, /* MVT_KEYPAD_F2 */
    VK_F3, /* MVT_KEYPAD_F3 */
    VK_F4, /* MVT_KEYPAD_F4 */
    VK_F5, /* MVT_KEYPAD_F5 */
    VK_F6, /* MVT_KEYPAD_F6 */
    VK_F7, /* MVT_KEYPAD_F7 */
    VK_F8, /* MVT_KEYPAD_F8 */
    VK_F9, /* MVT_KEYPAD_F9 */
    VK_F10, /* MVT_KEYPAD_F10 */
    VK_F11, /* MVT_KEYPAD_F11 */
    VK_F12, /* MVT_KEYPAD_F12 */
    VK_F13, /* MVT_KEYPAD_F13 */
    VK_F14, /* MVT_KEYPAD_F14 */
    VK_F15, /* MVT_KEYPAD_F15 */
    VK_F16, /* MVT_KEYPAD_F16 */
    VK_F17, /* MVT_KEYPAD_F17 */
    VK_F18, /* MVT_KEYPAD_F18 */
    VK_F19, /* MVT_KEYPAD_F19 */
    VK_F20 /* MVT_KEYPAD_F20 */
  };

static UINT mvt_override_numlock(UINT nChar)
{
  switch (nChar)
    {
    case VK_INSERT:
      nChar = VK_NUMPAD0;
      break;
    case VK_END:
      nChar = VK_NUMPAD1;
      break;
    case VK_DOWN:
      nChar = VK_NUMPAD2;
      break;
    case VK_NEXT:
      nChar = VK_NUMPAD3;
      break;
    case VK_LEFT:
      nChar = VK_NUMPAD4;
      break;
    case VK_CLEAR:
      nChar = VK_NUMPAD5;
      break;
    case VK_RIGHT:
      nChar = VK_NUMPAD6;
      break;
    case VK_HOME:
      nChar = VK_NUMPAD7;
      break;
    case VK_UP:
      nChar = VK_NUMPAD8;
      break;
    case VK_PRIOR:
      nChar = VK_NUMPAD9;
      break;
    }
  return nChar;
}

static int mvt_vk_from_win32(UINT nChar, BOOL bOverride)
{
  int code;

  if (bOverride) nChar = mvt_override_numlock(nChar);

  if (nChar == VK_DELETE)
    return '\177';
  for (code = MVT_KEYPAD_SPACE; code <= MVT_KEYPAD_F20; code++)
    {
      if (w32vktovk_table[code - MVT_KEYPAD_SPACE] == nChar)
    return code;
    }
  return -1;
}

