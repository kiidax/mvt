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

#ifndef MVT_PLUGIN_H
#define MVT_PLUGIN_H

#include <mvt/mvt.h>

typedef struct _mvt_session_vt mvt_session_vt_t;
typedef struct _mvt_session_plugin mvt_session_plugin_t;

/* mvt_session_vt_t */
struct _mvt_session_vt {
    void (*close) (mvt_session_t *session);
    int (*connect) (mvt_session_t *session);
    int (*read) (mvt_session_t *session, void *buf, size_t count, size_t *countread);
    int (*write) (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten);
    void (*shutdown) (mvt_session_t *session);
    void (*resize) (mvt_session_t *session, int width, int height);
};

struct _mvt_session {
    const mvt_session_vt_t *vt;
};

struct _mvt_session_plugin {
	mvt_session_t *(*open)(char **args, mvt_session_t *source, int width, int height);
};

int mvt_register_session_plugin(const char *proto, const mvt_session_plugin_t *plugin);

#endif
