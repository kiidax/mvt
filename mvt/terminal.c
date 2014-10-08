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
#include <mvt/mvt.h>
#include "private.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MVT_TERMINAL_MAX_TITLE_LENGTH 1024

#define MVT_ANSIMODE_KAM 2
#define MVT_ANSIMODE_CRM 3
#define MVT_ANSIMODE_IRM 4
#define MVT_ANSIMODE_HEM 10
#define MVT_ANSIMODE_SRM 12
#define MVT_ANSIMODE_LNM 20

#define MVT_DECMODE_DECCKM   1 
#define MVT_DECMODE_DECANM   2 
#define MVT_DECMODE_DECCOLM  3
#define MVT_DECMODE_DECSCLM  4
#define MVT_DECMODE_DECSCNM  5
#define MVT_DECMODE_DECOM    6
#define MVT_DECMODE_DECAWM   7
#define MVT_DECMODE_DECARM   8
#define MVT_DECMODE_DECINLM  9
#define MVT_DECMODE_DECPFF  18
#define MVT_DECMODE_DECPEX  19
#define MVT_DECMODE_DECTCEM 25
#define MVT_DECMODE_DECRLM  34
#define MVT_DECMODE_DECHEBM 35
#define MVT_DECMODE_DECHEM  36
#define MVT_DECMODE_DECNRCM 42
#define MVT_DECMODE_DECNKM  66
#define MVT_DECMODE_DECKBUM 68
#define MVT_DECMODE_VT200MOUSE 1000

typedef enum _mvt_terminal_state {
    MVT_TERMINAL_STATE_NORMAL,
    MVT_TERMINAL_STATE_ESC,
    MVT_TERMINAL_STATE_CSI,
    MVT_TERMINAL_STATE_OSC,
    MVT_TERMINAL_STATE_OSC_TEXT
} mvt_terminal_state_t;

/* mvt_terminal_t */

#define MVT_TERMINAL_MAX_PARAMS 8

#define MVT_TERMINAL_FLAG_ECHO       (1 << 0)
#define MVT_TERMINAL_FLAG_META       (1 << 1)
#define MVT_TERMINAL_FLAG_APPNUMPAD  (1 << 2)
#define MVT_TERMINAL_FLAG_NORMCURSOR (1 << 3)
#define MVT_TERMINAL_FLAG_INSERTMODE (1 << 4)
#define MVT_TERMINAL_FLAG_VT200MOUSE (1 << 5)

struct _mvt_terminal {
    mvt_console_t console;
    int flags;
    mvt_terminal_state_t state;
    unsigned int private : 1;
    unsigned int num_params : 3;
    unsigned int mouse_capture : 1;
    short params[MVT_TERMINAL_MAX_PARAMS];
    void *driver_data;
    void *user_data;
    int mouse_x;
    int mouse_y;
    int mouse_align;
};

#define MVT_IS_CONTROL(wc) ((wc) < 0x20)
static int mvt_terminal_init(mvt_terminal_t *terminal, int width, int height, int save_height);
static void mvt_terminal_destroy(mvt_terminal_t *terminal);
static void mvt_terminal_write_control(mvt_terminal_t *terminal, mvt_char_t wc);
static size_t mvt_terminal_write_text(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t len);
static void mvt_terminal_write_esc(mvt_terminal_t *terminal, mvt_char_t wc);
static void mvt_terminal_write_csi(mvt_terminal_t *terminal, mvt_char_t wc);
static void mvt_terminal_write_csi0(mvt_terminal_t *terminal, mvt_char_t wc);
static void mvt_terminal_write_csi1(mvt_terminal_t *terminal, mvt_char_t wc);
static void mvt_terminal_write_csi_sgr(mvt_terminal_t *terminal);

static int mvt_terminal_get_param(const mvt_terminal_t *terminal, int index, int default_value);
static void mvt_terminal_write_osc(mvt_terminal_t *terminal, mvt_char_t wc);
static size_t mvt_terminal_write_osc_text(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t count);

static int mvt_terminal_init(mvt_terminal_t *terminal, int width, int height, int save_height)
{
    memset(terminal, 0, sizeof (mvt_terminal_t));
    terminal->flags = MVT_TERMINAL_FLAG_META;
    if (mvt_console_init(&terminal->console, width, height, save_height) == -1)
        return -1;
    return 0;
}

static void mvt_terminal_destroy(mvt_terminal_t *terminal)
{
    mvt_console_destroy(&terminal->console);
}

mvt_terminal_t *mvt_terminal_new(int width, int height, int save_height)
{
    mvt_terminal_t *terminal = malloc(sizeof (mvt_terminal_t));
    if (terminal == NULL)
        return NULL;
    if (mvt_terminal_init(terminal, width, height, save_height) == -1) {
        free(terminal);
        return NULL;
    }
    return terminal;
}

void mvt_terminal_delete(mvt_terminal_t *terminal)
{
    mvt_terminal_destroy(terminal);
    free(terminal);
}

void mvt_terminal_set_user_data(mvt_terminal_t *terminal, void *data)
{
    terminal->user_data = data;
}

void *mvt_terminal_get_user_data(const mvt_terminal_t *terminal)
{
    return terminal->user_data;
}

void mvt_terminal_set_driver_data(mvt_terminal_t *terminal, void *data)
{
    terminal->driver_data = data;
}

void *mvt_terminal_get_driver_data(const mvt_terminal_t *terminal)
{
    return terminal->driver_data;
}

int mvt_terminal_set_screen(mvt_terminal_t *terminal, mvt_screen_t *screen)
{
    return mvt_console_set_screen(&terminal->console, screen);
}

mvt_screen_t *mvt_terminal_get_screen(const mvt_terminal_t *terminal)
{
    return mvt_console_get_screen(&terminal->console);
}

void mvt_terminal_paint(const mvt_terminal_t *terminal, void *gc, int x1, int y1, int x2, int y2)
{
    mvt_console_paint(&terminal->console, gc, x1, y1, x2, y2);
}

void mvt_terminal_repaint(const mvt_terminal_t *terminal)
{
    mvt_console_repaint(&terminal->console);
}

size_t mvt_terminal_write(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t len)
{
    const mvt_char_t *p = ws;
    const mvt_char_t *pl = ws + len;
    int ret;
    mvt_char_t wc;
    
    mvt_console_begin(&terminal->console);

    while (p < pl) {
        wc = *p;
        switch (terminal->state) {
        case MVT_TERMINAL_STATE_NORMAL:
            if (MVT_IS_CONTROL(wc)) {
                mvt_terminal_write_control(terminal, wc);
                p++;
            } else {
                ret = mvt_terminal_write_text(terminal, p, pl - p);
                p += ret;
            }
            break;
        case MVT_TERMINAL_STATE_ESC:
            mvt_terminal_write_esc(terminal, wc);
            p++;
            break;
        case MVT_TERMINAL_STATE_CSI:
            mvt_terminal_write_csi(terminal, wc);
            p++;
            break;
        case MVT_TERMINAL_STATE_OSC:
            mvt_terminal_write_osc(terminal, wc);
            p++;
            break;
        case MVT_TERMINAL_STATE_OSC_TEXT:
            ret = mvt_terminal_write_osc_text(terminal, p, pl - p);
            p += ret;
            break;
        }
    }
    mvt_console_end(&terminal->console);
    return pl - ws;
}

static void mvt_terminal_write_control(mvt_terminal_t *terminal, mvt_char_t wc)
{
    switch (wc) {
        /* C0 controls */
    case 0: /* NULL */
        break;
    case 7: /* BELL */
        mvt_console_beep(&terminal->console);
        break;
    case 8: /* BACKSPACE */
        mvt_console_move_cursor_relative(&terminal->console, -1, 0);
        break;
    case 9: /* CHARACTER TABULATION */
        mvt_console_forward_tabstops(&terminal->console, 1);
        break;
    case 10: /* LINE FEED */
    case 11: /* VERTICAL TAB */
    case 12: /* FORM FEED */
        mvt_console_line_feed(&terminal->console);
        break;
    case 13: /* CARRIAGE RETURN */
        mvt_console_carriage_return(&terminal->console);
        break;
    case 27: /* ESCAPE */
        terminal->state = MVT_TERMINAL_STATE_ESC;
        break;
    default:
        MVT_DEBUG_PRINT1("Unknown control character\n");
        break;
    }
}

static size_t mvt_terminal_write_text(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t len)
{
    const mvt_char_t *p = ws;
    const mvt_char_t *pl = ws + len;
    
    while (p < pl && !MVT_IS_CONTROL(*p)) p++;
    if (terminal->flags & MVT_TERMINAL_FLAG_INSERTMODE)
        /* @todo support multi width */
        mvt_console_insert_chars(&terminal->console, len);
    mvt_console_write(&terminal->console, ws, p - ws);
    
    return p - ws;
}

static void
mvt_terminal_write_esc (mvt_terminal_t *terminal, mvt_char_t wc)
{
    switch (wc) {
    case '7': /* DECSC */
        mvt_console_save_cursor(&terminal->console);
        break;
    case '8': /* DECRC */
        mvt_console_restore_cursor(&terminal->console);
        break;
    case '=':
        terminal->flags |= MVT_TERMINAL_FLAG_APPNUMPAD;
        break;
    case '>':
        terminal->flags &= ~MVT_TERMINAL_FLAG_APPNUMPAD;
        break;
    case '[':
        terminal->state = MVT_TERMINAL_STATE_CSI;
        memset(terminal->params, 0, sizeof terminal->params);
        terminal->private = FALSE;
        terminal->num_params = 0;
        return;
    case ']':
        terminal->state = MVT_TERMINAL_STATE_OSC;
        memset(terminal->params, 0, sizeof terminal->params);
        terminal->private = FALSE;
        terminal->num_params = 0;
        return;
    case 'D': /* INDEX (IND) */
        mvt_console_line_feed(&terminal->console);
        break;
    case 'E': /* NEXT LINE (NEL) */
        mvt_console_carriage_return(&terminal->console);
        mvt_console_line_feed(&terminal->console);
        break;
    case 'M': /* REVERSE INDEX (RI) */
        mvt_console_reverse_index(&terminal->console);
        break;
    case 'N': /* SS2 */
    case 'O': /* SS3 */
        break;
    case 'c':
        mvt_console_full_reset(&terminal->console);
        break;
    case '\\': /* STRING TERMINATOR (ST) */
    case '^': /* PRIVACY MESSAGE (PM) */
    case '_': /* APPLICATION PROGRAM COMMAND (APC) */
    case 'H': /* TAB SET (HTS) */
    case 'P': /* DEVICE CONTROL STRING (DCS) */
    case 'V': /* START OF GUARDED AREA (SPA) */
    case 'W': /* END OF GUARDED AREA (SPA) */
    case 'X': /* START OF STRING (SOS) */
    case 'Z': /* RETURN TERMINAL ID (DECID) */
    default:
        MVT_DEBUG_PRINT2("mvt_terminal_write_esc: not supported: ESC %c\n", wc);
        break;
    }
    terminal->state = MVT_TERMINAL_STATE_NORMAL;
    return;
}

static void mvt_terminal_write_csi(mvt_terminal_t *terminal, mvt_char_t wc)
{
    if (terminal->num_params == 0 && terminal->params[0] == 0) {
        if (wc == '?') {
            terminal->private = TRUE;
            return;
        }
    }

    if (wc >= '0' && wc <= '9') {
        if (terminal->num_params < MVT_TERMINAL_MAX_PARAMS) {
            int n = terminal->params[(int)terminal->num_params];
            n = n * 10 + (wc - '0');
            terminal->params[(int)terminal->num_params] = n;
        }
        return;
    }

    if (wc == ';') {
        if (terminal->num_params < MVT_TERMINAL_MAX_PARAMS)
            terminal->num_params++;
        return;
    }

    if (wc == '\033') {
        MVT_DEBUG_PRINT1("going back to ESC");
        terminal->state = MVT_TERMINAL_STATE_ESC;
        return;
    }

    if (!((wc >= '@' && wc <= 'Z') || (wc >= 'a' && wc <= 'z'))) {
        MVT_DEBUG_PRINT1("Non alphabet after CSI");
        terminal->state = MVT_TERMINAL_STATE_NORMAL;
        return;
    }

    terminal->num_params++;

#ifdef ENABLE_DEBUG
    {
        int i;
        MVT_DEBUG_PRINT1("mvt_terminal_write_csi: CSI");
        if (terminal->private)
            MVT_DEBUG_PRINT1(" ?");
        for (i = 0; i < terminal->num_params; i++)
            MVT_DEBUG_PRINT2(" %d", terminal->params[i]);
        MVT_DEBUG_PRINT2(" %c\n", wc);
    }
#endif
    
    if (terminal->private)
        mvt_terminal_write_csi1(terminal, wc);
    else
        mvt_terminal_write_csi0(terminal, wc);

    terminal->state = MVT_TERMINAL_STATE_NORMAL;
    return;
}

static void mvt_terminal_write_sm(mvt_terminal_t *terminal, int value)
{
    size_t i;
    for (i = 0; i < terminal->num_params; i++) {
        switch (terminal->params[i]) {
        case MVT_ANSIMODE_KAM:
        case MVT_ANSIMODE_CRM:
            MVT_DEBUG_PRINT2("mvt_terminal_write_csi: not supported ANSI mode %d.\n", terminal->params[i]);
            break;
        case MVT_ANSIMODE_IRM:
            if (value)
                terminal->flags |= MVT_TERMINAL_FLAG_INSERTMODE;
            else
                terminal->flags &= ~MVT_TERMINAL_FLAG_INSERTMODE;
            break;
        case MVT_ANSIMODE_HEM:
        case MVT_ANSIMODE_SRM:
        case MVT_ANSIMODE_LNM:
            MVT_DEBUG_PRINT2("mvt_terminal_write_csi: not supported ANSI mode %d.\n", terminal->params[i]);
            break;
        default:
            MVT_DEBUG_PRINT2("mvt_terminal_write_csi: not supported ANSI mode %d.\n", terminal->params[i]);
            break;
        }
    }
}

static void mvt_terminal_write_csi0(mvt_terminal_t *terminal, mvt_char_t wc)
{
    int n;

    switch (wc) {
    case '@':
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_insert_chars(&terminal->console, n);
        break;
    case 'A': /* CUrsor Up */
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_move_cursor_relative(&terminal->console, 0, -n);
        break;
    case 'B':
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_move_cursor_relative(&terminal->console, 0, n);
        break;
    case 'C':
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_move_cursor_relative(&terminal->console, n, 0);
        break;
    case 'D':
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_move_cursor_relative(&terminal->console, -n, 0);
        break;
    case 'G':
        n = mvt_terminal_get_param(terminal, 0, 1) - 1;
        mvt_console_move_cursor(&terminal->console, n, -1);
        break;
    case 'H': /* CUrsor Position */
        mvt_console_move_cursor(&terminal->console,
                                mvt_terminal_get_param(terminal, 1, 1) - 1,
                                mvt_terminal_get_param(terminal, 0, 1) - 1);
        break;
    case 'J': /* Erase in Display */
        mvt_console_erase_display(&terminal->console, terminal->params[0]);
        break;
    case 'K': /* Erase in Line */
        mvt_console_erase_line(&terminal->console, terminal->params[0]);
        break;
    case 'L':
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_insert_lines(&terminal->console, n);
        break;
    case 'M':
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_delete_lines(&terminal->console, n);
        break;
    case 'P':
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_delete_chars(&terminal->console, n);
        break;
    case 'X': /* Erase CHaracters */
        n = mvt_terminal_get_param(terminal, 0, 1);
        mvt_console_erase_chars(&terminal->console, n);
        break;
    case 'd':
        n = mvt_terminal_get_param(terminal, 0, 1) - 1;
        mvt_console_move_cursor(&terminal->console, -1, n);
        break;
    case 'h': case 'l':
        mvt_terminal_write_sm(terminal, wc == 'h');
        break;
    case 'm':
        mvt_terminal_write_csi_sgr(terminal);
        break;
    case 'r':
        mvt_console_set_scroll_region(&terminal->console,
                                      mvt_terminal_get_param(terminal, 0, 0) - 1,
                                      mvt_terminal_get_param(terminal, 1, 0) - 1);
        break;
    default:
        MVT_DEBUG_PRINT2("mvt_terminal_write_csi: not supported CSI %c\n", wc);
        break;
    }
}

static void mvt_terminal_write_decset(mvt_terminal_t *terminal, int value)
{
    size_t i;
    for (i = 0; i < terminal->num_params; i++) {
        switch (terminal->params[i]) {
        case 0: /* error (ignored) */
            break;
        case MVT_DECMODE_DECCKM:
            mvt_set_flag(&terminal->flags, MVT_TERMINAL_FLAG_NORMCURSOR, value);
            break;
        case MVT_DECMODE_DECANM:
            MVT_DEBUG_PRINT1("mvt_terminal_write_csi1: ignored DECANM\n");
            break;
        case MVT_DECMODE_DECTCEM: /* show/hide cursor */
            mvt_console_set_show_cursor(&terminal->console, value);
            break;
        case MVT_DECMODE_VT200MOUSE:
            mvt_set_flag(&terminal->flags, MVT_TERMINAL_FLAG_VT200MOUSE, value);
            break;
        default:
            MVT_DEBUG_PRINT2("mvt_terminal_write_csi1: not supported DEC private mode %d.\n", terminal->params[i]);
            break;
        }
    }
}

static void mvt_terminal_write_csi1(mvt_terminal_t *terminal, mvt_char_t wc)
{
    switch (wc) {
    case 'h': case 'l':
        mvt_terminal_write_decset(terminal, wc == 'h');
        break;
    default:
        MVT_DEBUG_PRINT2("mvt_terminal_write_csi1: not supported CSI1 %c\n", wc);
        break;
    }
}

static void mvt_terminal_write_csi_sgr(mvt_terminal_t *terminal)
{
    size_t i;
    mvt_attribute_t attribute;
    mvt_console_get_attribute(&terminal->console, &attribute);
    for (i = 0; i < terminal->num_params; i++) {
        int code = terminal->params[i];
        if (code >= 0 && code <= 28) {
            switch (code) {
            case 0:
                memset(&attribute, 0, sizeof (mvt_attribute_t));
                attribute.foreground_color = MVT_DEFAULT_COLOR;
                attribute.background_color = MVT_DEFAULT_COLOR;
                break;
            case 1:
                attribute.bright = 1;
                attribute.dim = 0;
                break;
            case 2:
                attribute.bright = 0;
                attribute.dim = 1;
                break;
            case 4:
                attribute.underscore = 1;
                break;
            case 5:
                attribute.blink = 1;
                break;
            case 7:
                attribute.reverse = 1;
                break;
            case 8:
                attribute.hidden = 1;
                break;
            case 22:
                attribute.bright = 0;
                attribute.dim = 0;
                break;
            case 24:
                attribute.underscore = 0;
                break;
            case 25:
                attribute.blink = 0;
                break;
            case 27:
                attribute.reverse = 0;
                break;
            case 28:
                attribute.hidden = 0;
                break;
            }
        } else if (code >= 30 && code <= 37) {
            attribute.foreground_color = (unsigned int)(code - 30);
        } else if (code == 39) {
            attribute.foreground_color = MVT_DEFAULT_COLOR;
        } else if (code >= 40 && code <= 47) {
            attribute.background_color = (unsigned int)(code - 40);
        } else if (code == 49) {
            attribute.background_color = MVT_DEFAULT_COLOR;
        } else {
            MVT_DEBUG_PRINT2("mvt_terminal_write_csi_sgr: unsupported attribute %d\n", code);
        }
    }
    mvt_console_set_attribute(&terminal->console, &attribute);
}

static void mvt_terminal_write_osc(mvt_terminal_t *terminal, mvt_char_t wc)
{
    if (wc >= '0' && wc <= '9') {
        terminal->params[0] = terminal->params[0] * 10 + (wc - '0');
    } else if (wc == ';') {
        terminal->state = MVT_TERMINAL_STATE_OSC_TEXT;
    } else {
        terminal->state = MVT_TERMINAL_STATE_NORMAL;
    }
    return;
}

static size_t mvt_terminal_write_osc_text(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t count)
{
    mvt_char_t title[MVT_TERMINAL_MAX_TITLE_LENGTH];
    const mvt_char_t *p = ws;
    int i;
    for (i = 0; i < MVT_TERMINAL_MAX_TITLE_LENGTH && p < ws + count; i++) {
        if (*p == '\007' || *p == 0x9c) {
            switch (terminal->params[0]) {
            case 0: case 1: case 2: case 3:
                title[i++] = '\0';
                mvt_console_set_title(&terminal->console, title);
                terminal->state = MVT_TERMINAL_STATE_NORMAL;
                break;
            }
            return i;
        }
        title[i] = *p++;
    }
    return i;
}

size_t mvt_terminal_read(mvt_terminal_t *terminal, mvt_char_t *ws, size_t count)
{
    return mvt_console_read_input(&terminal->console, ws, count);
}

int mvt_terminal_read_ready(const mvt_terminal_t *terminal)
{
    return mvt_console_has_input(&terminal->console);
}

static int mvt_terminal_get_param(const mvt_terminal_t *terminal, int index, int default_value)
{
    int value = terminal->params[index];
    if (value == 0) value = default_value;
    return value;
}

void mvt_terminal_get_size(const mvt_terminal_t *terminal, int *width, int *height)
{
    mvt_console_get_size(&terminal->console, width, height);
}

int mvt_terminal_resize(mvt_terminal_t *terminal)
{
    return mvt_console_resize(&terminal->console);
}

int mvt_terminal_append_input(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t count)
{
    return mvt_console_append_input(&terminal->console, ws, count);
}

int mvt_terminal_paste(mvt_terminal_t *terminal, const mvt_char_t *ws, size_t count)
{
    return mvt_terminal_append_input(terminal, ws, count);
}

int mvt_terminal_copy_selection(const mvt_terminal_t *terminal, mvt_char_t *ws, size_t len, int nl)
{
  return mvt_console_copy_selection(&terminal->console, ws, len, nl);
}

int mvt_terminal_keydown(mvt_terminal_t *terminal, int meta, int code)
{
    mvt_char_t wbuf[5];
    size_t count = 0;
    /* meta is also allowed for characters which has 7th bit set */
    if (code > 0x100) {
        mvt_char_t wc = 0;
        MVT_DEBUG_PRINT2("mvt_terminal_key: %d\n", code);
        if (!(terminal->flags & MVT_TERMINAL_FLAG_APPNUMPAD))
            wc = mvt_vktochar(code);
        if (wc == 0) {
            count = mvt_vktoappseq(code, terminal->flags & MVT_TERMINAL_FLAG_NORMCURSOR, wbuf, 5);
        } else {
            wbuf[0] = wc;
            count = 1;
        }
    } else if (code >= 0 && code < 0x100) {
        if (meta) {
            if (terminal->flags & MVT_TERMINAL_FLAG_META) {
                wbuf[0] = '\033';
                wbuf[1] = code;
                count = 2;
            } else {
                wbuf[0] = code | 0x80;
                count = 1;
            }
        } else {
            wbuf[0] = code;
            count = 1;
        }
    }
    if (count > 0) {
        mvt_console_append_input(&terminal->console, wbuf, count);
        if (terminal->flags & MVT_TERMINAL_FLAG_ECHO) {
            MVT_DEBUG_PRINT2("mvt_terminal_key: echo %c\n", code);
            mvt_terminal_write(terminal, wbuf, count);
        }
    }
    return 1;
}

int
mvt_terminal_mousebutton(mvt_terminal_t *terminal, int down, int button, uint32_t mod, int x, int y, int align)
{
    mvt_char_t wbuf[6];
    if (terminal->flags & MVT_TERMINAL_FLAG_VT200MOUSE) {
        if (button == 0)
            return 0;
        wbuf[0] = '\033';
        wbuf[1] = '[';
        wbuf[2] = 'M';
        wbuf[3] = (down ? button - 1 : 3) + 32;
        wbuf[4] = (x + 1) + 32;
        wbuf[5] = (y + 1) + 32;
        mvt_console_append_input(&terminal->console, wbuf, 6);
    } else if (down) {
        terminal->mouse_capture = TRUE;
        terminal->mouse_x = x;
        terminal->mouse_y = y;
        terminal->mouse_align = align;
    } else {
        terminal->mouse_capture = FALSE;
    }
    return 1;
}

int
mvt_terminal_mousemove (mvt_terminal_t *terminal, int x, int y, int align)
{
    int x1, y1, x2, y2, align1, align2;
    if (!terminal->mouse_capture)
        return 0;

    x1 = terminal->mouse_x;
    y1 = terminal->mouse_y;
    align1 = terminal->mouse_align;
    x2 = x;
    y2 = y;
    align2 = align;
    /* swap x1,y1 and x2,y2 if x1,y1 is after x2,y2. */
    if (y1 > y2 || (y1 == y2 && (x1 > x2 || (x1 == x2 && align1 > align2)))) { 
        int tmp;
        tmp = y1;
        y1 = y2;
        y2 = tmp;
        tmp = x1;
        x1 = x2;
        x2 = tmp;
        tmp = align1;
        align1 = align2;
        align2 = tmp;
    }
    if (align2 == 0)
        x2++;

    mvt_console_begin(&terminal->console);
    mvt_console_set_selection(&terminal->console, x1, y1, align1, x2, y2, align2);
    mvt_console_end(&terminal->console);
    return 1;
}
