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

#ifndef DEBUG_H
#define DEBUG_H

#ifdef ENABLE_DEBUG
#include <assert.h>
#define MVT_DEBUG_PRINT1(x1) mvt_debug_printf(x1)
#define MVT_DEBUG_PRINT2(x1,x2) mvt_debug_printf(x1,x2)
#define MVT_DEBUG_PRINT3(x1,x2,x3) mvt_debug_printf(x1,x2,x3)
#define MVT_DEBUG_PRINT4(x1,x2,x3,x4) mvt_debug_printf(x1,x2,x3,x4)
#define MVT_DEBUG_PRINT5(x1,x2,x3,x4,x5) mvt_debug_printf(x1,x2,x3,x4,x5)
#else
#define MVT_DEBUG_PRINT1(x1) ((void)0)
#define MVT_DEBUG_PRINT2(x1,x2) ((void)0)
#define MVT_DEBUG_PRINT3(x1,x2,x3) ((void)0)
#define MVT_DEBUG_PRINT4(x1,x2,x3,x4) ((void)0)
#define MVT_DEBUG_PRINT5(x1,x2,x3,x4,x5) ((void)0)
#endif

#ifdef ENABLE_DEBUG
void mvt_debug_printf(const char *s, ...);
#endif

#endif
