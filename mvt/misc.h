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

#ifndef MISC_H
#define MISC_H

MVT_BEGIN_DECLS

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

mvt_char_t mvt_vktochar(int code);
size_t mvt_vktoappseq(int code, int normcursor, mvt_char_t *ws, size_t count);
size_t mvt_strlen(const mvt_char_t *s);
mvt_char_t *mvt_strcpy(mvt_char_t *d, const mvt_char_t *s);
int mvt_wcwidth(mvt_char_t wc);
int mvt_parse_param(const char *s, int state, char ***args, char **buf);
uint32_t mvt_atocolor(const char *s);

MVT_END_DECLS

#endif
