/* Multi-purpose Virtual Terminal
 * Copyright (C) 2013 Katsuya Iida
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

int mvt_wcwidth(mvt_char_t wc)
{
    if (wc < 0x1100) {
        return 1;
    } else if (wc < 0x1160) {
        return 2;
    } else if (wc < 0x2329) {
        return 1;
    } else if (wc < 0x232b) {
        return 2;
    } else if (wc < 0x2e80) {
        return 1;
    } else if (wc < 0x2e9a) {
        return 2;
    } else if (wc < 0x2e9b) {
        return 1;
    } else if (wc < 0x2ef4) {
        return 2;
    } else if (wc < 0x2f00) {
        return 1;
    } else if (wc < 0x2fd6) {
        return 2;
    } else if (wc < 0x2ff0) {
        return 1;
    } else if (wc < 0x2ffc) {
        return 2;
    } else if (wc < 0x3000) {
        return 1;
    } else if (wc < 0x303f) {
        return 2;
    } else if (wc < 0x3041) {
        return 1;
    } else if (wc < 0x3097) {
        return 2;
    } else if (wc < 0x3099) {
        return 1;
    } else if (wc < 0x3100) {
        return 2;
    } else if (wc < 0x3105) {
        return 1;
    } else if (wc < 0x312e) {
        return 2;
    } else if (wc < 0x3131) {
        return 1;
    } else if (wc < 0x318f) {
        return 2;
    } else if (wc < 0x3190) {
        return 1;
    } else if (wc < 0x31bb) {
        return 2;
    } else if (wc < 0x31c0) {
        return 1;
    } else if (wc < 0x31e4) {
        return 2;
    } else if (wc < 0x31f0) {
        return 1;
    } else if (wc < 0x321f) {
        return 2;
    } else if (wc < 0x3220) {
        return 1;
    } else if (wc < 0x3248) {
        return 2;
    } else if (wc < 0x3250) {
        return 1;
    } else if (wc < 0x32ff) {
        return 2;
    } else if (wc < 0x3300) {
        return 1;
    } else if (wc < 0x4dc0) {
        return 2;
    } else if (wc < 0x4e00) {
        return 1;
    } else if (wc < 0xa48d) {
        return 2;
    } else if (wc < 0xa490) {
        return 1;
    } else if (wc < 0xa4c7) {
        return 2;
    } else if (wc < 0xa960) {
        return 1;
    } else if (wc < 0xa97d) {
        return 2;
    } else if (wc < 0xf900) {
        return 1;
    } else if (wc < 0xfb00) {
        return 2;
    } else if (wc < 0xfe10) {
        return 1;
    } else if (wc < 0xfe1a) {
        return 2;
    } else if (wc < 0xfe30) {
        return 1;
    } else if (wc < 0xfe53) {
        return 2;
    } else if (wc < 0xfe54) {
        return 1;
    } else if (wc < 0xfe67) {
        return 2;
    } else if (wc < 0xfe68) {
        return 1;
    } else if (wc < 0xfe6c) {
        return 2;
    } else if (wc < 0xff01) {
        return 1;
    } else if (wc < 0xff61) {
        return 2;
    } else if (wc < 0xffe0) {
        return 1;
    } else if (wc < 0xffe7) {
        return 2;
    } else {
        return 1;
    }
}
