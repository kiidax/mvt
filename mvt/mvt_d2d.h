/* Multi-purpose Virtual Terminal
 * Copyright (C) 2012 Katsuya Iida
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

#ifndef MVT_D2D_H
#define MVT_D2D_H

MVT_BEGIN_DECLS

typedef struct _mvt_d2d_gc_line mvt_d2d_gc_line_t;

struct _mvt_d2d_gc_line {
    ID2D1RenderTarget *render_target;
    IDWriteTextFormat *dwrite_text_format;
    IDWriteFactory *dwrite_factory;
    float cell_width;
};

mvt_screen_t *mvt_d2d_open_screen(char **args, void *user_data);
void mvt_d2d_close_screen(mvt_screen_t *screen);
int mvt_d2d_begin_draw_line(void *user_data, int x, int width, int y, mvt_d2d_gc_line_t *gc_line);
void mvt_d2d_end_draw_line(void *user_data, mvt_d2d_gc_line_t *gc_line);
void mvt_d2d_screen_scroll(void *user_data, int y1, int y2, int count);
void mvt_d2d_screen_set_title(void *user_data, LPCTSTR sz);
void mvt_d2d_screen_notify_resize(mvt_screen_t *screen, int columns, int rows);
int mvt_d2d_screen_get_line_info(mvt_screen_t *screen);
int mvt_d2d_screen_get_scroll_info(mvt_screen_t *screen, int *scroll_position, int *virtual_height);

MVT_END_DECLS

#endif
