/* Multi-purpose Virtual Terminal
 * Copyright (C) 2010-2011 Katsuya Iida
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
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <mvt/mvt.h>
#include <mvt/mvt_lua.h>
#ifdef WIN32
#include <winsock2.h>
#endif

#define LUA_MVTLIBNAME "mvt"

typedef struct _lmvt_screen_t lmvt_screen_t;
typedef struct _lmvt_terminal_t lmvt_terminal_t;

struct _lmvt_screen_t {
    mvt_screen_t *screen;
    int event_func;
    int state;
    lua_State *L;
};

struct _lmvt_terminal_t {
    mvt_terminal_t *terminal;
    int event_func;
    lua_State *L;
};

static lua_State *lmvt_L;

static int lmvt_event(void *target, int type, int param1, int param2)
{
    mvt_screen_t *screen;
    lmvt_screen_t *lscreen;
    mvt_terminal_t *terminal;
    lmvt_terminal_t *lterminal;
    lua_State *L = lmvt_L;
    const char *error_msg;
    switch (type) {
    case MVT_EVENT_TYPE_KEY:
        screen = (mvt_screen_t *)target;
        lscreen = (lmvt_screen_t *)mvt_screen_get_user_data(screen);
        switch (lscreen->state) {
        case 0:
            if (param1 == 12) { /* ^L */
                lscreen->state = 1;
                return 0;
            }
            break;
        case 1:
            lscreen->state = 0;
            lua_rawgeti(L, LUA_REGISTRYINDEX, lscreen->event_func);
            lua_pushinteger(L, type);
            lua_pushinteger(L, param1);
            if (lua_pcall(L, 2, 1, 0)) {
                error_msg = lua_tostring(L, -1);
                fprintf(stderr, "%s\n", error_msg);
            }
            if (lua_toboolean(L, -1))
                return 0;
            break;
        }
        break;
    case MVT_EVENT_TYPE_DATA:
    case MVT_EVENT_TYPE_CLOSE:
        terminal = (mvt_terminal_t *)target;
        lterminal = (lmvt_terminal_t *)mvt_terminal_get_user_data(terminal);
        lua_rawgeti(L, LUA_REGISTRYINDEX, lterminal->event_func);
        lua_pushinteger(L, type);
        if (lua_pcall(L, 1, 0, 0)) {
            error_msg = lua_tostring(L, -1);
            fprintf(stderr, "%s\n", error_msg);
        }
        break;
    default:
        break;
    }
    return -1;
}

static int lmvt_open_screen(lua_State *L)
{
    lmvt_screen_t *lscreen;
    const char *spec;
    int ref;
    spec = luaL_checkstring(L, 1);
    lua_pushvalue (L, 2);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lscreen = (lmvt_screen_t *)lua_newuserdata(L, sizeof (lmvt_screen_t));
    luaL_getmetatable(L, "mvt.screen");
    lua_setmetatable(L, -2);
    lscreen->screen = mvt_open_screen(spec);
    if (!lscreen->screen) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        luaL_error(L, "cannot open screen");
    }
    mvt_screen_set_user_data(lscreen->screen, lscreen);
    lscreen->state = 0;
    lscreen->event_func = ref;
    return 1;
}

static int lmvt_screen_close(lua_State *L)
{
    lmvt_screen_t *lscreen = luaL_checkudata(L, 1, "mvt.screen");
    mvt_screen_t *screen = lscreen->screen;
    if (!screen)
        return 1;
    mvt_close_screen(screen);
    lscreen->screen = NULL;
    return 1;
}

static int lmvt_screen_set_attribute(lua_State *L)
{
    lmvt_screen_t *lscreen = luaL_checkudata(L, 1, "mvt.screen");
    mvt_screen_t *screen = lscreen->screen;
    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);
    if (!screen)
        return 1;
    if (mvt_set_screen_attribute(screen, name, value) == -1)
        return 0;
    return 1;
}

static int lmvt_screen_gc(lua_State *L)
{
    lmvt_screen_t *lscreen = luaL_checkudata(L, 1, "mvt.screen");
    mvt_screen_t *screen = lscreen->screen;
    if (!screen)
        return 1;
    mvt_close_screen(screen);
    return 1;
}

static int lmvt_open_terminal(lua_State *L)
{
    lmvt_terminal_t *lterminal;
    const char *spec;
    int ref;
    spec = luaL_checkstring(L, 1);
    lua_pushvalue (L, 2);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lterminal = (lmvt_terminal_t *)lua_newuserdata(L, sizeof (lmvt_terminal_t));
    luaL_getmetatable(L, "mvt.terminal");
    lua_setmetatable(L, -2);
    lterminal->terminal = mvt_open_terminal(spec);
    if (!lterminal->terminal) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        luaL_error(L, "cannot open terminal");
    }
    mvt_terminal_set_user_data(lterminal->terminal, lterminal);
    lterminal->event_func = ref;
    return 1;
}

static int lmvt_terminal_close(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    if (!terminal)
        return 1;
    mvt_close_terminal(terminal);
    lterminal->terminal = NULL;
    return 1;
}

static int lmvt_terminal_set_attribute(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);
    if (!terminal)
        return 1;
    if (mvt_set_terminal_attribute(terminal, name, value) == -1)
        return 0;
    return 1;
}

static int lmvt_terminal_attach_screen(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    lmvt_screen_t *lscreen = luaL_checkudata(L, 2, "mvt.screen");
    mvt_terminal_t *terminal = lterminal->terminal;
    if (!terminal)
        return 1;
    if (mvt_attach(terminal, lscreen->screen) == -1)
        luaL_error(L, "cannot attach");
    return 1;
}

static size_t lmvt_mbstowcs(mvt_char_t *ws, size_t wcount, const char *s, size_t count)
{
    mvt_char_t *wbuf = ws;
    mvt_char_t wc;
    int c;
    while (count && wcount) {
        c = *s++;
        if (!(c & 0x80)) {
            *ws++ = c;
            count--;
        } else if (!(c & 0x40)) {
            /* invalid */
            count--;
        } else if (!(c & 0x20)) {
            if (count < 2)
                break;
            wc = c & 0x1f;
            wc = (wc << 6) + ((*s++) & 0x3f);
            count -= 2;
            *ws++ = wc;
        } else if (!(c & 0x10)) {
            if (count < 3)
                break;
            wc = c & 0x0f;
            wc = (wc << 6) + ((*s++) & 0x3f);
            wc = (wc << 6) + ((*s++) & 0x3f);
            count -= 3;
            *ws++ = wc;
        } else {
            if (count < 4)
                break;
            wc = c & 0x07;
            wc = (wc << 6) + ((*s++) & 0x3f);
            wc = (wc << 6) + ((*s++) & 0x3f);
            wc = (wc << 6) + ((*s++) & 0x3f);
            count -= 3;
            *ws++ = wc;
        }
        wcount--;
    }
    return ws - wbuf;
}

static int lmvt_terminal_read(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    mvt_char_t *ws, wbuf[4096];
    char *s, *buf;
    size_t count;
    if (!terminal)
        return 1;
    count = mvt_terminal_read(terminal, wbuf, 4096);
    if (count == 0) {
        lua_pushstring(L, "");
        return 1;
    }
    buf = malloc(count * sizeof (char));
    ws = wbuf;
    s = buf;
    while (count--) *s++ = *ws++;
    lua_pushlstring(L, buf, s - buf);
    free(buf);
    return 1;
}

static int lmvt_terminal_write(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    mvt_char_t *ws;
    size_t len;
    const char *s = luaL_checklstring(L, 2, &len);
    if (!terminal)
        return 1;
    ws = malloc(len * sizeof (mvt_char_t));
    if (!ws) return 0;
    len = lmvt_mbstowcs(ws, len, s, len);
    mvt_terminal_write(terminal, ws, len);
    free(ws);
    return 1;
}

static int lmvt_terminal_append_input(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    mvt_char_t *ws;
    size_t len;
    const char *s = luaL_checklstring(L, 2, &len);
    if (!terminal)
        return 1;
    ws = malloc(len * sizeof (mvt_char_t));
    if (!ws) return 0;
    len = lmvt_mbstowcs(ws, len, s, len);
    if (mvt_terminal_append_input(terminal, ws, len) == -1) {
        free(ws);
        return 0;
    }
    free(ws);
    return 1;
}

static int lmvt_terminal_suspend(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    if (!terminal)
        return 1;
    mvt_suspend(terminal);
    return 1;
}

static int lmvt_terminal_resume(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    if (!terminal)
        return 1;
    mvt_resume(terminal);
    return 1;
}

static int lmvt_terminal_open(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    const char *spec = luaL_checkstring(L, 2);
    if (!terminal)
        return 1;
    if (mvt_open(terminal, spec) == -1)
        luaL_error(L, "cannot open");
    return 1;
}

static int lmvt_terminal_connect(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    if (!terminal)
        return 1;
    if (mvt_connect(terminal) == -1)
        luaL_error(L, "cannot connect");
    return 1;
}

static int lmvt_terminal_shutdown(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    if (!terminal)
        return 1;
    mvt_shutdown(terminal);
    return 1;
}

static int lmvt_terminal_gc(lua_State *L)
{
    lmvt_terminal_t *lterminal = luaL_checkudata(L, 1, "mvt.terminal");
    mvt_terminal_t *terminal = lterminal->terminal;
    if (!terminal)
        return 1;
    mvt_close_terminal(terminal);
    return 1;
}

static int lmvt_quit(lua_State *L)
{
    mvt_main_quit();
    return 1;
}

static const luaL_Reg lmvt_f[] = {
    { "open_screen", lmvt_open_screen },
    { "open_terminal", lmvt_open_terminal },
    { "quit", lmvt_quit },
    { NULL, NULL }
};

static const luaL_Reg lmvt_screen_m[] = {
    { "close", lmvt_screen_close },
    { "set_attribute", lmvt_screen_set_attribute },
    { "__gc", lmvt_screen_gc },
    { NULL, NULL }
};

static const luaL_Reg lmvt_terminal_m[] = {
    { "close", lmvt_terminal_close },
    { "set_attribute", lmvt_terminal_set_attribute },
    { "attach_screen", lmvt_terminal_attach_screen },
    { "open", lmvt_terminal_open },
    { "connect", lmvt_terminal_connect },
    { "suspend", lmvt_terminal_suspend },
    { "resume", lmvt_terminal_resume },
    { "shutdown", lmvt_terminal_shutdown },
    { "read", lmvt_terminal_read },
    { "write", lmvt_terminal_write },
    { "append_input_buffer", lmvt_terminal_append_input },
    { "__gc", lmvt_terminal_gc },
    { NULL, NULL }
};

int luaopen_mvt(lua_State *L)
{
    luaL_newmetatable(L, "mvt.screen");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, lmvt_screen_m, 0);
    lua_pop(L, 1);

    luaL_newmetatable(L, "mvt.terminal");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, lmvt_terminal_m, 0);
    lua_pop(L, 1);

    luaL_newlib(L, lmvt_f);
    lua_pushstring(L, "major_version");
    lua_pushnumber(L, (double)mvt_major_version);
    lua_settable(L, -3);
    lua_pushstring(L, "minor_version");
    lua_pushnumber(L, (double)mvt_minor_version);
    lua_settable(L, -3);
    lua_pushstring(L, "micro_version");
    lua_pushnumber(L, (double)mvt_micro_version);
    lua_settable(L, -3);

    return 1;
}

int
main(int argc, char *argv[])
{
    lua_State *L;
    const char *error_msg;
    const char *filename;
#ifdef WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    if (mvt_init(&argc, &argv, lmvt_event) == -1) {
        fprintf(stderr, "cannot initialize mvt\n");
        return EXIT_FAILURE;
    }
    if (mvt_register_default_plugins() == -1)
        return EXIT_FAILURE;
    atexit(mvt_exit);
    L = luaL_newstate();
    if (L == NULL) {
        fprintf(stderr, "cannot create state: not enough memory\n");
        return EXIT_FAILURE;
    }
    lmvt_L = L;
    luaL_openlibs(L);

    /* open MVT library */
    luaL_requiref(L, LUA_MVTLIBNAME, luaopen_mvt, 1);
    lua_pop(L, 1);

    if (argc >= 3) {
        fprintf(stderr, "usage: mvt [initfile]\n");
        return EXIT_FAILURE;
    } else if (argc == 2) {
        filename = argv[1];
    } else if (argc == 1) {
        filename = MVTDATADIR "/default.lua";
    }
    if (luaL_loadfile(L, filename) || lua_pcall(L, 0, 0, 0)) {
        error_msg = lua_tostring(L, -1);
        fprintf(stderr, "%s\n", error_msg);
        lua_close(L);
        return EXIT_FAILURE;
    }
    mvt_main();
    lua_close(lmvt_L);
#ifdef WIN32
    WSACleanup();
#endif
    return EXIT_SUCCESS;
}
