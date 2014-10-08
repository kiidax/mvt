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
#include "debug.h"
#include "misc.h"
#include "driver.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const int mvt_major_version = MVT_MAJOR_VERSION;
const int mvt_minor_version = MVT_MINOR_VERSION;
const int mvt_micro_version = MVT_MICRO_VERSION;

/* debug */

static const uint32_t mvt_color_value_table[16] = {
    0x000000, 0xcd0000, 0x00cd00, 0xcdcd00,
    0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
    0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00,
    0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff
};
static const uint8_t mvt_color_6step_value_table[6] = {
    0, 95, 135, 175, 215, 255
};

uint32_t mvt_color_value(mvt_color_t color)
{
    if (color < 16)
        return mvt_color_value_table[color];
    if (color < 232) {
        uint32_t a, v;
        uint32_t c = (uint32_t)(color - 16);
        a = c/36; c = c%36;
        v = mvt_color_6step_value_table[a] << 16;
        a = c/6; c = c%6;
        v = mvt_color_6step_value_table[a] << 8;
        v = mvt_color_6step_value_table[c];
        return v;
    } else {
        uint32_t c = 10 * (color - 232) + 8;
        uint32_t v = (c<<16) + (c<<8) + c;
        return v;
    }
}

static const char vktochar_table[] =
  {
    0, /* MVT_KEYPAD_SPACE */
    '\t', /* MVT_KEYPAD_TAB */
    '\r', /* MVT_KEYPAD_ENTER */
    0, /* MVT_KEYPAD_PF1 */
    0, /* MVT_KEYPAD_PF2 */
    0, /* MVT_KEYPAD_PF3 */
    0, /* MVT_KEYPAD_PF4 */
    0, /* MVT_KEYPAD_HOME */
    0, /* MVT_KEYPAD_LEFT */
    0, /* MVT_KEYPAD_UP */
    0, /* MVT_KEYPAD_RIGHT */
    0, /* MVT_KEYPAD_DOWN */
    0, /* MVT_KEYPAD_PRIOR */
    0, /* MVT_KEYPAD_PAGEUP */
    0, /* MVT_KEYPAD_NEXT */
    0, /* MVT_KEYPAD_PAGEDOWN */
    0, /* MVT_KEYPAD_END */
    0, /* MVT_KEYPAD_BEGIN */
    0, /* MVT_KEYPAD_INSERT */
    0, /* MVT_KEYPAD_EQUAL */
    '*', /* MVT_KEYPAD_MULTIPLY */
    '+', /* MVT_KEYPAD_ADD */
    ',', /* MVT_KEYPAD_SEPARATOR */
    '-', /* MVT_KEYPAD_SUBTRACT */
    '.', /* MVT_KEYPAD_DECIMAL */
    '/', /* MVT_KEYPAD_DIVIDE */
    '0', /* MVT_KEYPAD_0 */
    '1', /* MVT_KEYPAD_1 */
    '2', /* MVT_KEYPAD_2 */
    '3', /* MVT_KEYPAD_3 */
    '4', /* MVT_KEYPAD_4 */
    '5', /* MVT_KEYPAD_5 */
    '6', /* MVT_KEYPAD_6 */
    '7', /* MVT_KEYPAD_7 */
    '8', /* MVT_KEYPAD_8 */
    '9', /* MVT_KEYPAD_9 */
    0, /* MVT_KEYPAD_F1 */
    0, /* MVT_KEYPAD_F2 */
    0, /* MVT_KEYPAD_F3 */
    0, /* MVT_KEYPAD_F4 */
    0, /* MVT_KEYPAD_F5 */
    0, /* MVT_KEYPAD_F6 */
    0, /* MVT_KEYPAD_F7 */
    0, /* MVT_KEYPAD_F8 */
    0, /* MVT_KEYPAD_F9 */
    0, /* MVT_KEYPAD_F10 */
    0, /* MVT_KEYPAD_F11 */
    0, /* MVT_KEYPAD_F12 */
    0, /* MVT_KEYPAD_F13 */
    0, /* MVT_KEYPAD_F14 */
    0, /* MVT_KEYPAD_F15 */
    0, /* MVT_KEYPAD_F16 */
    0, /* MVT_KEYPAD_F17 */
    0, /* MVT_KEYPAD_F18 */
    0, /* MVT_KEYPAD_F19 */
    0  /* MVT_KEYPAD_F20 */
  };

static const char vktoappchar_table[] =
  {
    ' ', /* MVT_KEYPAD_SPACE */
    'I', /* MVT_KEYPAD_TAB */
    'M', /* MVT_KEYPAD_ENTER */
    'P', /* MVT_KEYPAD_PF1 */
    'Q', /* MVT_KEYPAD_PF2 */
    'R', /* MVT_KEYPAD_PF3 */
    'S', /* MVT_KEYPAD_PF4 */
    1, /* MVT_KEYPAD_HOME */
    'D', /* MVT_KEYPAD_LEFT */
    'A', /* MVT_KEYPAD_UP */
    'C', /* MVT_KEYPAD_RIGHT */
    'B', /* MVT_KEYPAD_DOWN */
    5, /* MVT_KEYPAD_PRIOR */
    5, /* MVT_KEYPAD_PAGEUP */
    6, /* MVT_KEYPAD_NEXT */
    6, /* MVT_KEYPAD_PAGEDOWN */
    4, /* MVT_KEYPAD_END */
    'E', /* MVT_KEYPAD_BEGIN */
    2, /* MVT_KEYPAD_INSERT */
    'X', /* MVT_KEYPAD_EQUAL */
    'j', /* MVT_KEYPAD_MULTIPLY */
    'k', /* MVT_KEYPAD_ADD */
    'l', /* MVT_KEYPAD_SEPARATOR */
    'm', /* MVT_KEYPAD_SUBTRACT */
    0, /* MVT_KEYPAD_DECIMAL */
    'o', /* MVT_KEYPAD_DIVIDE */
    2, /* MVT_KEYPAD_0 */
    4, /* MVT_KEYPAD_1 */
    'B', /* MVT_KEYPAD_2 */
    6, /* MVT_KEYPAD_3 */
    'D', /* MVT_KEYPAD_4 */
    'E', /* MVT_KEYPAD_5 */
    'C', /* MVT_KEYPAD_6 */
    1, /* MVT_KEYPAD_7 */
    'A', /* MVT_KEYPAD_8 */
    5, /* MVT_KEYPAD_9 */
    11, /* MVT_KEYPAD_F1 */
    12, /* MVT_KEYPAD_F2 */
    13, /* MVT_KEYPAD_F3 */
    14, /* MVT_KEYPAD_F4 */
    15, /* MVT_KEYPAD_F5 */
    17, /* MVT_KEYPAD_F6 */
    18, /* MVT_KEYPAD_F7 */
    19, /* MVT_KEYPAD_F8 */
    20, /* MVT_KEYPAD_F9 */
    21, /* MVT_KEYPAD_F10 */
    23, /* MVT_KEYPAD_F11 */
    24, /* MVT_KEYPAD_F12 */
    25, /* MVT_KEYPAD_F13 */
    26, /* MVT_KEYPAD_F14 */
    28, /* MVT_KEYPAD_F15 */
    29, /* MVT_KEYPAD_F16 */
    31, /* MVT_KEYPAD_F17 */
    32, /* MVT_KEYPAD_F18 */
    33, /* MVT_KEYPAD_F19 */
    34  /* MVT_KEYPAD_F20 */
  };

mvt_char_t mvt_vktochar(int code)
{
    if (code >= MVT_KEYPAD_SPACE && code <= MVT_KEYPAD_F20)
        return vktochar_table[code - MVT_KEYPAD_SPACE];
    return 0;
}

size_t mvt_vktoappseq(int code, int normcursor, mvt_char_t *buf, size_t len)
{
  int is_ss3;
  char last_char;
  MVT_DEBUG_PRINT2("mvt_vktoappseq(%d)\n", code);
  /* todo if len < 5 */
  if (len < 5) return 0;
  if (!(code >= MVT_KEYPAD_SPACE && code <= MVT_KEYPAD_F20))
    return 0;
  last_char = vktoappchar_table[code - MVT_KEYPAD_SPACE];
  if (last_char == 0) return 0;
  is_ss3 = ((code >= MVT_KEYPAD_SPACE && code <= MVT_KEYPAD_PF4)
            || (code >= MVT_KEYPAD_MULTIPLY && code <= MVT_KEYPAD_DIVIDE)
            || (normcursor && code >= MVT_KEYPAD_LEFT && code <= MVT_KEYPAD_DOWN));

  buf[0] = '\033';
  buf[1] = is_ss3 ? 'O' : '[';
  if (last_char > 0 && last_char <= 9)
    {
      buf[2] = '0' + last_char;
      buf[3] = '~';
      return 4;
    }
  else if (last_char > 10 && last_char < ' ')
    {
      buf[2] = '0' + last_char % 10;
      buf[3] = '0' + last_char / 10;
      buf[4] = '~';
      return 5;
    }
  else
    {
      buf[2] = last_char;
      return 3;
    }
}

void
mvt_set_flag(int *flags, int flag, int value)
{
  if (value)
    *flags |= flag;
  else
    *flags &= ~flag;
}

size_t mvt_strlen(const mvt_char_t *s)
{
    const mvt_char_t *t = s;
    while (*t) t++;
    return t - s;
}

mvt_char_t *mvt_strcpy(mvt_char_t *d, const mvt_char_t *s)
{
    mvt_char_t *od = d;
    while (*s) *d++ = *s++;
    *d = *s;
    return od;
}

void mvt_screen_set_driver_data(mvt_screen_t *screen, void *data)
{
    screen->driver_data = data;
}

void *mvt_screen_get_driver_data(const mvt_screen_t *screen)
{
    return screen->driver_data;
}

void mvt_screen_set_user_data(mvt_screen_t *screen, void *data)
{
    screen->user_data = data;
}

void *mvt_screen_get_user_data(const mvt_screen_t *screen)
{
    return screen->user_data;
}

int mvt_ctoi(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'Z' + 10;
    return -1;
}

uint32_t mvt_atocolor(const char *s)
{
    size_t len = strlen(s);
    if (len == 4) {
        if (*s != '#') return 0;
        return mvt_ctoi(s[1]) * 0x110000
            + mvt_ctoi(s[2]) * 0x001100
            + mvt_ctoi(s[3]) * 0x000011;
    } else if (len == 7) {
        if (*s != '#') return 0;
        return mvt_ctoi(s[1]) * 0x100000
            + mvt_ctoi(s[2]) * 0x010000
            + mvt_ctoi(s[3]) * 0x001000
            + mvt_ctoi(s[4]) * 0x000100
            + mvt_ctoi(s[5]) * 0x000010
            + mvt_ctoi(s[6]) * 0x000001;
    }
    return 0;
}

int mvt_parse_param(const char *s, int state, char ***pargs, char **pbuf)
{
    /* state 0:proto 1:name 2:value */
    size_t i, len, iargs, argslen;
    char *buf = NULL, **argsbuf = NULL;
    void *p;
    int quote, escape;
    len = 1024;
    buf = malloc(len);
    if (!buf) goto error;
    argslen = 16;
    argsbuf = malloc(sizeof (char *) * argslen);
    if (!argsbuf) goto error;
    i = 0;
    iargs = 0;
    escape = 0;
    quote = '\0';
    if (*s == '\0' && state == 1)
        goto accept;
    for (;;) {
        if (iargs >= argslen) {
            argslen += 16;
            p = realloc(argsbuf, sizeof (char *) * argslen);
            if (!p) goto error;
            argsbuf = p;
        }
        argsbuf[iargs++] = &buf[i];
        for (;;) {
            if (i >= len) {
                len += 1024;
                p = realloc(buf, len);
                if (!p) goto error;
                buf = p;
            }
            if (*s == '\0') {
                if (escape || quote != '\0') goto error;
                if (state == 1) goto error;
                buf[i++] = '\0';
                goto accept;
            }
            if (escape) {
                buf[i++] = *s++;
                continue;
            }
            if (*s == quote) {
                s++;
                quote = '\0';
                continue;
            }
            if (*s == ':' && state == 0) {
                s++;
                buf[i++] = '\0';
                state = 1;
                break;
            }
            if (*s == ',' && state == 2) {
                s++;
                buf[i++] = '\0';
                state = 1;
                break;
            }
            if (*s == '=' && state == 1) {
                s++;
                buf[i++] = '\0';
                state = 2;
                break;
            }
            if ((*s == '\"' || *s == '\'') && state == 2) {
                quote = *s;
                s++;
                continue;
            }
            if (*s == '\\') {
                s++;
                escape = 1;
                continue;
            }
            buf[i++] = *s++;
        }
    }
  accept:
    if (iargs >= argslen) {
        argslen += 16;
        p = realloc(argsbuf, sizeof (char *) * argslen);
        if (!p) goto error;
        argsbuf = p;
    }
    argsbuf[iargs] = NULL;
    *pargs = argsbuf;
    *pbuf = buf;
    return 0;
  error:
    if (argsbuf) free(argsbuf);
    if (buf) free(buf);
    return -1;
}
