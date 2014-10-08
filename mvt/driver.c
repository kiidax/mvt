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

#ifdef ENABLE_DEBUG
static FILE *debug_trace_fp;

static void mvt_debug_init(FILE *fp)
{
    debug_trace_fp = fp;
}

static void mvt_debug_exit(void)
{
}

void mvt_debug_printf(const char *s, ...)
{
    if (debug_trace_fp != NULL) {
        va_list ap;
        va_start(ap, s);
        vfprintf(debug_trace_fp, s, ap);
        fflush(debug_trace_fp);
        va_end(ap);
    }
}
#endif

static const mvt_driver_t *mvt_current_driver;

int mvt_init(int *argc, char ***argv, mvt_event_func_t event_func)
{
#ifdef ENABLE_DEBUG
    mvt_debug_init(stderr);
#endif
    mvt_current_driver = mvt_get_driver();
    MVT_DEBUG_PRINT2("mvt_init: driver `%s' selected.\n", mvt_current_driver->name);
    return (*mvt_current_driver->vt->init)(argc, argv, event_func);
}

void mvt_main(void)
{
    (*mvt_current_driver->vt->main)();
}

void mvt_main_quit(void)
{
    (*mvt_current_driver->vt->main_quit)();
}

void mvt_exit(void)
{
    (*mvt_current_driver->vt->exit)();
#ifdef ENABLE_DEBUG
    mvt_debug_exit();
#endif
}

mvt_screen_t *mvt_open_screen(const char *spec)
{
    mvt_screen_t *screen;
    char **args, *buf;
    if (mvt_parse_param(spec, 1, &args, &buf) == -1)
        return NULL;
#ifdef ENABLE_DEBUG
    do {
        char **p = args;
        while (*p) {
            printf("%s=%s\n", *p, *(p+1));
            p += 2;
        }
    } while (FALSE);
#endif
    screen = (*mvt_current_driver->vt->open_screen)(args);
    free(args);
    free(buf);
    return screen;
}

void mvt_close_screen(mvt_screen_t *screen)
{
    (*mvt_current_driver->vt->close_screen)(screen);
}

mvt_terminal_t *mvt_open_terminal(const char *spec)
{
    mvt_terminal_t *terminal;
    char **args, *buf;
    if (mvt_parse_param(spec, 1, &args, &buf) == -1)
        return NULL;
#ifdef ENABLE_DEBUG
    do {
        char **p = args;
        while (*p) {
            printf("%s=%s\n", *p, *(p+1));
            p += 2;
        }
    } while (FALSE);
#endif
    terminal = (*mvt_current_driver->vt->open_terminal)(args);
    free(args);
    free(buf);
    return terminal;
}

void mvt_close_terminal(mvt_terminal_t *terminal)
{
    (*mvt_current_driver->vt->close_terminal)(terminal);
}

int mvt_set_screen_attribute(mvt_screen_t *screen, const char *name, const char *value)
{
    return (*mvt_current_driver->vt->set_screen_attribute)(screen, name, value);
}

int mvt_set_terminal_attribute(mvt_terminal_t *terminal, const char *name, const char *value)
{
    return (*mvt_current_driver->vt->set_terminal_attribute)(terminal, name, value);
}

void mvt_shutdown(mvt_terminal_t *terminal)
{
    (*mvt_current_driver->vt->shutdown)(terminal);
}

void mvt_suspend(mvt_terminal_t *terminal)
{
    (*mvt_current_driver->vt->suspend)(terminal);
}

void mvt_resume(mvt_terminal_t *terminal)
{
    (*mvt_current_driver->vt->resume)(terminal);
}
