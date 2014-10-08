/* Multi-purpose Virtual Terminal
 * Copyright (C) 2005-2010,2012 Katsuya Iida
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

/*! \addtogroup Session
 * @{
 **/

void
mvt_session_close (mvt_session_t *session)
{
    MVT_DEBUG_PRINT1("mvt_session_close\n");
    (*session->vt->close)(session);
}

int
mvt_session_connect (mvt_session_t *session)
{
    MVT_DEBUG_PRINT1("mvt_session_connect\n");
    return (*session->vt->connect)(session);
}

/**
 * Read data from session. countread can be zero when read succeeded.
 * @param session a session
 * @param buf buffer
 * @param count count
 * @param countread countread
 * @retval 0 success
 * @retval -1 fail or end of stream
 **/
int
mvt_session_read (mvt_session_t *session, void *buf, size_t count, size_t *countread)
{
    return (*session->vt->read)(session, buf, count, countread);
}

int
mvt_session_write (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten)
{
    return (*session->vt->write)(session, buf, count, countwritten);
}

void
mvt_session_shutdown (mvt_session_t *session)
{
    MVT_DEBUG_PRINT1("mvt_session_shutdown\n");
    (*session->vt->shutdown)(session);
}

void
mvt_session_resize (mvt_session_t *session, int width, int height)
{
    (*session->vt->resize)(session, width, height);
}

/**
 * @}
 **/
