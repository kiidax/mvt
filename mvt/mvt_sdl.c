/* Multi-purpose Virtual Terminal
 * Copyright (C) 2005-2011 Katsuya Iida
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
#include <SDL.h>
#include <SDL_ttf.h>
#include <assert.h>
#include <mvt/mvt.h>
#include "misc.h"
#include "debug.h"
#include "driver.h"

#define MVT_SCROLLBAR_WIDTH 8
#define MVT_SDL_SCREEN_ALWAYSBRIGHT (1<<0)
#define MVT_SDL_SCREEN_PSEUDOBOLD   (1<<1)

#define MVT_SDL_REQUEST (SDL_USEREVENT+0)

typedef struct _mvt_sdl_screen mvt_sdl_screen_t;

struct _mvt_sdl_screen {
    mvt_screen_t parent;
    SDL_Surface *surface;

    int font_size;
    int font_width, font_height;
    TTF_Font *font;

    uint32_t foreground_color;
    uint32_t background_color;
    uint32_t selection_color;
    uint32_t scroll_foreground_color;
    uint32_t scroll_background_color;
    int invalid_left, invalid_top;
    int invalid_right, invalid_bottom;
    int width, height;
    int cursor_x, cursor_y;
    int selection_start_x, selection_start_y;
    int selection_end_x, selection_end_y;

  unsigned int flags;
};

/* On SDL, there's only one screen */
static mvt_sdl_screen_t *global_sdl_screen = NULL;
static int loop;

/* utilities */
static void mvt_color_value_to_sdl(SDL_Color *sdl_color, unsigned int color_value);

/* mvt_sdl_screen_t */

static void *mvt_sdl_screen_begin(mvt_screen_t *screen);
static void mvt_sdl_screen_end(mvt_screen_t *screen, void *gc);
static void mvt_sdl_screen_draw_text(mvt_screen_t *screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t len);
static void mvt_sdl_screen_clear_rect(mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color);
static void mvt_sdl_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y);
static void mvt_sdl_screen_scroll(mvt_screen_t *screen, int y1, int y2, int count);
static void mvt_sdl_screen_beep(mvt_screen_t *screen);
static void mvt_sdl_screen_get_size(mvt_screen_t *screen, int *width, int *height);
static int mvt_sdl_screen_resize(mvt_screen_t *screen, int width, int height);
static void mvt_sdl_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws);
static void mvt_sdl_screen_set_scroll_info(mvt_screen_t *screen, int top, int height, int virtual_height);
static void mvt_sdl_screen_set_mode(mvt_screen_t *screen, int mode, int value);
static int mvt_sdl_set_screen_attribute0(mvt_sdl_screen_t *sdl_screen, const char *name, const char *value);

static const mvt_screen_vt_t sdl_screen_vt = {
    mvt_sdl_screen_begin,
    mvt_sdl_screen_end,
    mvt_sdl_screen_draw_text,
    mvt_sdl_screen_clear_rect,
    mvt_sdl_screen_scroll,
    mvt_sdl_screen_move_cursor,
    mvt_sdl_screen_beep,
    mvt_sdl_screen_get_size,
    mvt_sdl_screen_resize,
    mvt_sdl_screen_set_title,
    mvt_sdl_screen_set_scroll_info,
    mvt_sdl_screen_set_mode
};

int mvt_sdl_screen_init(mvt_sdl_screen_t *sdl_screen)
{
    memset(sdl_screen, 0, sizeof *sdl_screen);
    sdl_screen->parent.vt = &sdl_screen_vt;
    sdl_screen->foreground_color = 0xffffff;
    sdl_screen->background_color = 0;
    sdl_screen->selection_color = 0xff0000;
    sdl_screen->scroll_foreground_color = 0xffffff;
    sdl_screen->scroll_background_color = 0;
    sdl_screen->flags = MVT_SDL_SCREEN_PSEUDOBOLD;
    sdl_screen->selection_start_x = -1;
    sdl_screen->selection_start_y = -1;
    sdl_screen->selection_end_x = -1;
    sdl_screen->selection_end_y = -1;
    return 0;
}

void
mvt_sdl_screen_destroy(mvt_sdl_screen_t *sdl_screen)
{
    memset(sdl_screen, 0, sizeof *sdl_screen);
}

static void *mvt_sdl_screen_begin(mvt_screen_t *screen)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    if (SDL_MUSTLOCK(sdl_screen->surface)) {
        if (SDL_LockSurface(sdl_screen->surface) < 0)
            return NULL;
    }
    sdl_screen->invalid_left = sdl_screen->width;
    sdl_screen->invalid_top = sdl_screen->height;
    sdl_screen->invalid_right = -1;
    sdl_screen->invalid_bottom = -1;
  
    return (void *)sdl_screen->surface->pixels;
}

static void mvt_sdl_screen_end(mvt_screen_t *screen, void *gc)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    SDL_Rect rect;
    if (SDL_MUSTLOCK(sdl_screen->surface)) {
        SDL_UnlockSurface(sdl_screen->surface);
    }
    if (sdl_screen->invalid_left <= sdl_screen->invalid_right
        && sdl_screen->invalid_top <= sdl_screen->invalid_bottom) {
        rect.x = sdl_screen->invalid_left * sdl_screen->font_width;
        rect.y = sdl_screen->invalid_top * sdl_screen->font_height;
        rect.w = (sdl_screen->invalid_right + 1) * sdl_screen->font_width - rect.x;
        rect.h = (sdl_screen->invalid_bottom + 1) * sdl_screen->font_height - rect.y;
        assert(rect.x >= 0 && rect.y >= 0);
        assert(rect.x + rect.w <= sdl_screen->width * sdl_screen->font_width);
        assert(rect.y + rect.h <= sdl_screen->height * sdl_screen->font_height);
        SDL_UpdateRect(sdl_screen->surface, rect.x, rect.y, rect.w, rect.h);
    }
}

static void mvt_sdl_screen_draw_text(mvt_screen_t* screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t len)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    SDL_Surface *text_surface;
    mvt_color_t color;
    unsigned int color_value;
    SDL_Color sdl_foreground_color, sdl_background_color;
    Uint16 wbuf[2];
    SDL_Rect rect;
    int px, py;
    px = x * sdl_screen->font_width;
    py = y * sdl_screen->font_height;
    assert(x >= 0 && x + len <= sdl_screen->width);
    assert(y >= 0 && y < sdl_screen->height);
    if (sdl_screen->invalid_left > (int)x)
        sdl_screen->invalid_left = (int)x;
    if (sdl_screen->invalid_right < (int)(x + len - 1))
        sdl_screen->invalid_right = (int)(x + len - 1);
    if (sdl_screen->invalid_top > y)
        sdl_screen->invalid_top = (int)y;
    if (sdl_screen->invalid_bottom < (int)y)
        sdl_screen->invalid_bottom = (int)y;
    while (len--) {
        if (attribute->no_char) {
            ws++;
            attribute++;
            px += sdl_screen->font_width;
            x++;
            continue;
        }

        color = attribute->foreground_color;
        color_value = color == MVT_DEFAULT_COLOR ?
            sdl_screen->foreground_color : mvt_color_value(color);
        mvt_color_value_to_sdl(&sdl_foreground_color, color_value);
        color = attribute->background_color;
        color_value = color == MVT_DEFAULT_COLOR ?
            sdl_screen->background_color : mvt_color_value(color);
        mvt_color_value_to_sdl(&sdl_background_color, color_value);

        if ((y > sdl_screen->selection_start_y ||
             (y == sdl_screen->selection_start_y && x >= sdl_screen->selection_start_x)) &&
            (y < sdl_screen->selection_end_y ||
             (y == sdl_screen->selection_end_y && x <= sdl_screen->selection_end_x))) {
            SDL_Color swap_color;
            swap_color = sdl_background_color;
            sdl_background_color = sdl_foreground_color;
            sdl_foreground_color = swap_color;
        }
            
        if ((x == sdl_screen->cursor_x && y == sdl_screen->cursor_y) || attribute->reverse) {
            SDL_Color swap_color;
            swap_color = sdl_background_color;
            sdl_background_color = sdl_foreground_color;
            sdl_foreground_color = swap_color;
        }

        if (*ws < 0x20)
            wbuf[0] = ' ';
        else
            wbuf[0] = *ws;
        ws++;
        attribute++;
        wbuf[1] = 0;
        
        text_surface = TTF_RenderUNICODE_Shaded(sdl_screen->font, wbuf,
                                                sdl_foreground_color,
                                                sdl_background_color);
        if (text_surface) {
            rect.x = px;
            rect.y = py;
            rect.w = text_surface->w;
            rect.h = text_surface->h;
            SDL_BlitSurface(text_surface, NULL, sdl_screen->surface, &rect);
            SDL_FreeSurface(text_surface);
        }
        x++;
        px += sdl_screen->font_width;
    }
}

static void mvt_sdl_screen_clear_rect(mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    SDL_Rect rect;
    unsigned int color_value;
    Uint32 sdl_color;
    assert(x1 >= 0 && x2 >= x1 && sdl_screen->width > x2);
    assert(y1 >= 0 && y2 >= y2 && sdl_screen->height > y2);
    if (sdl_screen->invalid_left > x1) sdl_screen->invalid_left = x1;
    if (sdl_screen->invalid_top > y1) sdl_screen->invalid_top = y1;
    if (sdl_screen->invalid_right < x2) sdl_screen->invalid_right = x2;
    if (sdl_screen->invalid_bottom < y2) sdl_screen->invalid_bottom = y2;
    rect.x = x1 * sdl_screen->font_width;
    rect.y = y1 * sdl_screen->font_height;
    rect.w = (x2 + 1) * sdl_screen->font_width - rect.x;
    rect.h = (y2 + 1) * sdl_screen->font_height - rect.y;
    if (background_color == MVT_DEFAULT_COLOR)
        color_value = sdl_screen->background_color;
    else
        color_value = mvt_color_value(background_color);
    sdl_color = SDL_MapRGB(sdl_screen->surface->format,
                           (color_value >> 16) & 255,
                           (color_value >> 8 ) & 255,
                           (color_value      ) & 255);
    SDL_FillRect(sdl_screen->surface, &rect, sdl_color);
}

static void mvt_sdl_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    switch (cursor) {
    case MVT_CURSOR_CURRENT:
        sdl_screen->cursor_x = x;
        sdl_screen->cursor_y = y;
        break;
    case MVT_CURSOR_SELECTION_START:
        sdl_screen->selection_start_x = x;
        sdl_screen->selection_start_y = y;
        break;
    case MVT_CURSOR_SELECTION_END:
        sdl_screen->selection_end_x = x;
        sdl_screen->selection_end_y = y;
        break;
    }
}

static void mvt_sdl_screen_scroll(mvt_screen_t *screen, int y1, int y2, int count)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    SDL_Surface *surface;
    Uint8 *bufp;
    size_t bytes_per_row;

    if (y1 == -1)
        y1 = 0;
    if (y2 == -1)
        y2 = sdl_screen->height - 1;

    assert(y1 >= 0 && y2 >= 0);
    assert(count != 0 && count <= y2 - y1 + 1 && count >= -(y2 - y1 + 1));

    surface = sdl_screen->surface;
    bufp = mvt_sdl_screen_begin(screen);
    if (bufp == NULL)
        return;
    bytes_per_row = surface->pitch * sdl_screen->font_height;
    if (count > 0) {
        memmove(bufp + (y1 + count) * bytes_per_row,
                bufp + y1 * bytes_per_row,
                (y2 - y1 + 1 - count) * bytes_per_row);
    } else {
        memmove(bufp + y1 * bytes_per_row,
                bufp + (y1 - count) * bytes_per_row,
                (y2 - y1 + 1 + count) * bytes_per_row);
    }
    if (sdl_screen->invalid_left > 0)
        sdl_screen->invalid_left = 0;
    if (sdl_screen->invalid_top > y1)
        sdl_screen->invalid_top = y1;
    if (sdl_screen->invalid_right < sdl_screen->width - 1)
        sdl_screen->invalid_right = sdl_screen->width - 1;
    if (sdl_screen->invalid_bottom < y2)
        sdl_screen->invalid_bottom = y2;
    mvt_sdl_screen_end(screen, bufp);
}

static void mvt_sdl_screen_beep(mvt_screen_t *screen)
{
    MVT_DEBUG_PRINT1("mvt_sdl_screen_beep\n");
}

static void mvt_sdl_screen_get_size(mvt_screen_t *screen, int *width, int *height)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    *width = sdl_screen->width;
    *height = sdl_screen->height;
}

static int mvt_sdl_screen_resize(mvt_screen_t *screen, int width, int height)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    SDL_Surface *surface;
    int pixel_width, pixel_height;
    Uint32 flags;

    pixel_width = width * sdl_screen->font_width + MVT_SCROLLBAR_WIDTH;
    pixel_height = height * sdl_screen->font_height;
    flags = SDL_ANYFORMAT | SDL_SWSURFACE | SDL_HWSURFACE | SDL_RESIZABLE;
    surface = SDL_SetVideoMode(pixel_width, pixel_height, 0, flags);
    if (surface == NULL) {
        MVT_DEBUG_PRINT2("Unable to set screen mode: %s\n", SDL_GetError());
        return -1;
    }
    sdl_screen->surface = surface;
    sdl_screen->width = width;
    sdl_screen->height = height;
    return 0;
}

static void mvt_sdl_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws)
{
    int i;
    char buf[256];
    if (ws) {
        for (i = 0; i < 256; i++) {
            mvt_char_t wc = *ws++;
            buf[i] = wc;
            if (wc == '\0')
                break;
        }
    } else {
        strcpy(buf, "mvt");
    }
    SDL_WM_SetCaption(buf, "mvt");
}

static void mvt_sdl_screen_set_scroll_info(mvt_screen_t *screen, int top, int height, int total_height)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    SDL_Rect rect, inner_rect;
    unsigned int color_value;
    Uint32 sdl_color;
    rect.x = sdl_screen->width * sdl_screen->font_width;
    rect.y = 0;
    rect.w = MVT_SCROLLBAR_WIDTH;
    rect.h = sdl_screen->height * sdl_screen->font_height;
    color_value = sdl_screen->scroll_background_color;
    sdl_color = SDL_MapRGB(sdl_screen->surface->format,
                           (color_value >> 16) & 0xff,
                           (color_value >> 8 ) & 0xff,
                           (color_value      ) & 0xff);
    SDL_FillRect(sdl_screen->surface, &rect, sdl_color);
    inner_rect = rect;
    inner_rect.y = top * rect.h / total_height;
    inner_rect.h = (top + height) * rect.h / total_height - inner_rect.y;
    color_value = sdl_screen->scroll_foreground_color;
    sdl_color = SDL_MapRGB(sdl_screen->surface->format,
                           (color_value >> 16) & 0xff,
                           (color_value >> 8 ) & 0xff,
                           (color_value      ) & 0xff);
    SDL_FillRect(sdl_screen->surface, &inner_rect, sdl_color);
    SDL_UpdateRect(sdl_screen->surface,
                   rect.x, rect.y, rect.w, rect.h);
}

static void mvt_sdl_screen_set_mode(mvt_screen_t *screen, int mode, int value)
{
}

static void mvt_sdl_screen_convert_point(mvt_sdl_screen_t *sdl_screen, int x, int y, int *cx, int *cy)
{
    int font_width = sdl_screen->font_width;
    int font_height = sdl_screen->font_height;
    if (cx != NULL) *cx = x / font_width;
    if (cy != NULL) *cy = y / font_height;
}

static int mvt_sdl_update_screen(mvt_sdl_screen_t *sdl_screen)
{
    const char *font_filename;
    int font_width, font_height;
    TTF_Font *font;

    font_filename = getenv("MVT_FONTPATH");
    if (!font_filename) font_filename = "./font.ttf";
    font = TTF_OpenFont(font_filename , sdl_screen->font_size);
    if (!font) {
        MVT_DEBUG_PRINT1("Unable to open TrueType font\n");
        return -1;
    }
    if (sdl_screen->font)
        TTF_CloseFont(sdl_screen->font);
    sdl_screen->font = font;
    if (TTF_SizeText(sdl_screen->font, "0", &font_width, &font_height) != 0)
        return -1;
    sdl_screen->font_width = font_width;
    sdl_screen->font_height = TTF_FontHeight(sdl_screen->font);
    if (mvt_sdl_screen_resize((mvt_screen_t *)sdl_screen, sdl_screen->width,
                              sdl_screen->height) == -1)
        return -1;
    return 0;
}

static mvt_screen_t *mvt_sdl_open_screen(char **args)
{
    mvt_sdl_screen_t *sdl_screen;
    const char *name, *value;
    char **p;

    if (global_sdl_screen) {
        MVT_DEBUG_PRINT1("Only one screen can be opened on SDL\n");
        return NULL;
    }
    sdl_screen = malloc(sizeof (mvt_sdl_screen_t));
    if (mvt_sdl_screen_init(sdl_screen) == -1) {
        free(sdl_screen);
	return NULL;
    }
    sdl_screen->width = 80;
    sdl_screen->height = 24;
    sdl_screen->font_size = 12;
    p = args;
    while (*p) {
        name = *p++;
        if (!*p) {
            mvt_sdl_screen_destroy(sdl_screen);
            free(sdl_screen);
            return NULL;
        }
        value = *p++;
        if (mvt_sdl_set_screen_attribute0(sdl_screen, name, value) == -1) {
            free(sdl_screen);
            return NULL;
        }
    }
    if (mvt_sdl_update_screen(sdl_screen) == -1) {
        free(sdl_screen);
        return NULL;
    }
    global_sdl_screen = sdl_screen;
    return (mvt_screen_t *)sdl_screen;
}

static void mvt_sdl_close_screen(mvt_screen_t *screen)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    mvt_screen_dispatch_close(screen);
    mvt_sdl_screen_destroy(sdl_screen);
    TTF_CloseFont(sdl_screen->font);
    free(screen);
    global_sdl_screen = NULL;
}

static int mvt_sdl_set_screen_attribute0(mvt_sdl_screen_t *sdl_screen, const char *name, const char *value)
{
    if (strcmp(name, "width") == 0)
        sdl_screen->width = atoi(value);
    else if (strcmp(name, "height") == 0)
        sdl_screen->height = atoi(value);
    else if (strcmp(name, "font-size") == 0) {
        sdl_screen->font_size = atoi(value);
    } else if (strcmp(name, "foreground-color") == 0)
        sdl_screen->foreground_color = mvt_atocolor(value);
    else if (strcmp(name, "background-color") == 0)
        sdl_screen->background_color = mvt_atocolor(value);
    else if (strcmp(name, "scroll-foreground-color") == 0)
        sdl_screen->scroll_foreground_color = mvt_atocolor(value);
    else if (strcmp(name, "scroll-background-color") == 0)
        sdl_screen->scroll_background_color = mvt_atocolor(value);
    return 0;
}

static int mvt_sdl_set_screen_attribute(mvt_screen_t *screen, const char *name, const char *value)
{
    mvt_sdl_screen_t *sdl_screen = (mvt_sdl_screen_t *)screen;
    if (mvt_sdl_set_screen_attribute0(sdl_screen, name, value) == -1)
        return -1;
    if (mvt_sdl_update_screen(sdl_screen) == -1)
        return -1;
    mvt_screen_dispatch_repaint(screen);
    return 0;
}

void mvt_notify_request(void)
{
    SDL_Event event;
    event.type = SDL_USEREVENT;
    event.user.code = MVT_SDL_REQUEST;
    event.user.data1 = NULL;
    event.user.data2 = NULL;
    SDL_PushEvent(&event);
}

static void mvt_sdl_event_request(mvt_sdl_screen_t *sdl_screen, SDL_Event *event)
{
    mvt_handle_request();
}

int mvt_sdl_sdkkey_to_mvtkey(SDLKey key)
{
    if (key >= SDLK_KP0 && key <= SDLK_KP9)
        return (int)(key - SDLK_KP0) + MVT_KEYPAD_0;
    if (key >= SDLK_F1 && key <= SDLK_F4)
        return (int)(key - SDLK_F1) + MVT_KEYPAD_PF1;
    if (key >= SDLK_F1 && key <= SDLK_F15)
        return (int)(key - SDLK_F1) + MVT_KEYPAD_F1;
    switch (key) {
    case SDLK_SPACE:
        return MVT_KEYPAD_SPACE;
    case SDLK_TAB:
        return MVT_KEYPAD_TAB;
    case SDLK_KP_ENTER:
        return MVT_KEYPAD_ENTER;
    case SDLK_HOME:
        return MVT_KEYPAD_HOME;
    case SDLK_LEFT:
        return MVT_KEYPAD_LEFT;
    case SDLK_UP:
        return MVT_KEYPAD_UP;
    case SDLK_RIGHT:
        return MVT_KEYPAD_RIGHT;
    case SDLK_DOWN:
        return MVT_KEYPAD_DOWN;
    case SDLK_PAGEUP:
        return MVT_KEYPAD_PAGEUP;
    case SDLK_PAGEDOWN:
        return MVT_KEYPAD_PAGEDOWN;
    case SDLK_END:
        return MVT_KEYPAD_END;
    case SDLK_INSERT:
        return MVT_KEYPAD_INSERT;
    case SDLK_KP_EQUALS:
        return MVT_KEYPAD_EQUAL;
    case SDLK_KP_MULTIPLY:
        return MVT_KEYPAD_MULTIPLY;
    case SDLK_KP_PLUS:
        return MVT_KEYPAD_ADD;
    case SDLK_KP_PERIOD:
        return MVT_KEYPAD_SEPARATOR;
    case SDLK_KP_MINUS:
        return MVT_KEYPAD_SUBTRACT;
    case SDLK_KP_DIVIDE:
        return MVT_KEYPAD_DIVIDE;
    default:
        return -1;
    }
}

static void mvt_sdl_event_keydown(mvt_sdl_screen_t *sdl_screen, SDL_Event *event)
{
    SDLMod mod;
    int meta = FALSE;
    int code = -1;

    mod = SDL_GetModState();
    code = mvt_sdl_sdkkey_to_mvtkey(event->key.keysym.sym);
    if (code == MVT_KEYPAD_SPACE) {
        if (mod & KMOD_LCTRL || mod & KMOD_RCTRL)
            code = '\0';
        else
            code = ' ';
    } else if (code == -1 && event->key.keysym.unicode != 0) {
        code = event->key.keysym.unicode;
        if (mod & KMOD_LALT || mod & KMOD_RALT)
            meta = TRUE;
    }
    if (code == -1)
        return;
    mvt_screen_dispatch_keydown((mvt_screen_t *)sdl_screen, meta, code);
}

static void mvt_sdl_event_mousebutton(mvt_sdl_screen_t *sdl_screen, SDL_Event *event)
{
    int cx, cy, button, down;
    mvt_sdl_screen_convert_point(sdl_screen, event->button.x,
                                 event->button.y, &cx, &cy);
    button = event->button.button;
    down = event->type == SDL_MOUSEBUTTONDOWN;
    mvt_screen_dispatch_mousebutton((mvt_screen_t *)sdl_screen, down, button, 0, cx, cy);
}

static void mvt_sdl_event_mousemotion(mvt_sdl_screen_t *sdl_screen, SDL_Event *event)
{
    int cx, cy;
    mvt_sdl_screen_convert_point(sdl_screen, event->button.x,
                                 event->button.y, &cx, &cy);
    mvt_screen_dispatch_mousemove((mvt_screen_t *)sdl_screen, cx, cy);
}

static void mvt_sdl_event_videoresize(mvt_sdl_screen_t *sdl_screen, SDL_Event *event)
{
    int width, height;
    mvt_sdl_screen_convert_point(sdl_screen,
                                 event->resize.w - MVT_SCROLLBAR_WIDTH,
                                 event->resize.h,
                                 &width, &height);
    if (mvt_sdl_screen_resize((mvt_screen_t *)sdl_screen, width, height) == -1)
        return;
    mvt_screen_dispatch_resize((mvt_screen_t *)sdl_screen);
}

static int mvt_sdl_init(int *argc, char ***argv, mvt_event_func_t event_func)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() == -1) {
        SDL_Quit();
        fprintf(stderr, "Unable to init SDL_ttf: %s\n", TTF_GetError());
        return -1;
    }

    SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    mvt_worker_init(event_func);
    return 0;
}

static void mvt_sdl_main(void)
{
    SDL_Event event;
    mvt_sdl_screen_t *sdl_screen;

    loop = TRUE;
    while (loop) {
        SDL_WaitEvent(&event);
        sdl_screen = global_sdl_screen;
        switch (event.type) {
        case SDL_USEREVENT:
            switch (event.user.code) {
            case MVT_SDL_REQUEST:
                mvt_sdl_event_request(sdl_screen, &event);
                break;
            }
            break;
        case SDL_KEYDOWN:
            mvt_sdl_event_keydown(sdl_screen, &event);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            mvt_sdl_event_mousebutton(sdl_screen, &event);
            break;
        case SDL_MOUSEMOTION:
            mvt_sdl_event_mousemotion(sdl_screen, &event);
            break;
        case SDL_VIDEORESIZE:
            mvt_sdl_event_videoresize(sdl_screen, &event);
            break;
        case SDL_QUIT:
            return;
        }
    }
}

void mvt_sdl_main_quit(void)
{
    loop = FALSE;
}

void mvt_sdl_exit(void)
{
    mvt_worker_exit();
    TTF_Quit();
    SDL_Quit();
}

static const mvt_driver_vt_t mvt_sdl_driver_vt = {
    mvt_sdl_init,
    mvt_sdl_main,
    mvt_sdl_main_quit,
    mvt_sdl_exit,
    mvt_sdl_open_screen,
    mvt_sdl_close_screen,
    mvt_worker_open_terminal,
    mvt_worker_close_terminal,
    mvt_sdl_set_screen_attribute,
    mvt_worker_set_terminal_attribute,
    mvt_worker_connect,
    mvt_worker_suspend,
    mvt_worker_resume,
    mvt_worker_shutdown
};

static const mvt_driver_t mvt_sdl_driver = {
    &mvt_sdl_driver_vt, "sdl"
};

const mvt_driver_t *mvt_get_driver(void)
{
    return &mvt_sdl_driver;
}

static void mvt_color_value_to_sdl(SDL_Color *sdl_color, unsigned int color_value)
{
    sdl_color->r = (color_value >> 16) & 0xff;
    sdl_color->g = (color_value >> 8) & 0xff;
    sdl_color->b = color_value & 0xff;
}
