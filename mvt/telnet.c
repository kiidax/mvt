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

#include <mvt/mvt.h>
#include "private.h"
/* #define MVT_DEBUG */
#include "debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MVT_COMMAND_SE   (240)
#define MVT_COMMAND_SB   (250)
#define MVT_COMMAND_WILL (251)
#define MVT_COMMAND_WONT (252)
#define MVT_COMMAND_DO   (253)
#define MVT_COMMAND_DONT (254)
#define MVT_COMMAND_IAC  (255)

#define MVT_OPTION_ECHO                (1)
#define MVT_OPTION_SUPPRESS_GO_AHEAD   (3)
#define MVT_OPTION_STATUS              (5)
#define MVT_OPTION_TIMING_MARK         (6)
#define MVT_OPTION_TERMINAL_TYPE       (24)
#define MVT_OPTION_NAWS                (31)
#define MVT_OPTION_TERMINAL_SPEED      (32)
#define MVT_OPTION_TOGGLE_FLOW_CONTROL (33)
#define MVT_OPTION_LINEMODE            (34)
#define MVT_OPTION_NEW_ENVIRON         (39)

enum _mvt_telnet_state_t {
    MVT_TELNET_STATE_NORMAL,
    MVT_TELNET_STATE_IAC,
    MVT_TELNET_STATE_NEGOTIATE,
    MVT_TELNET_STATE_SB,
    MVT_TELNET_STATE_SE,
    MVT_TELNET_STATE_SB_COMMAND,
    MVT_TELNET_STATE_SB_IAC
};

struct _mvt_telnet {
    mvt_session_t parent;
    mvt_session_t *source;
    uint8_t state;
    uint8_t tmp;
    int sub_command_length;
    uint8_t options_do[256 / 8];
    uint8_t options_will[256 / 8];
    const char *terminal_type;
    const char *username;
    const char *x_display_location;
	int width;
	int height;
};

static void mvt_telnet_process_iac(mvt_telnet_t *telnet, uint8_t c);
static void mvt_telnet_negotiate(mvt_telnet_t *telnet, uint8_t command, uint8_t c);
static void mvt_telnet_reply_negotiate(mvt_telnet_t *telnet, uint8_t command, uint8_t c);
#ifdef ENABLE_DEBUG
static const char *mvt_telnet_command_name(uint8_t c);
#endif
static void mvt_telnet_sub_command(mvt_telnet_t *telnet, int option, const uint8_t *p, size_t len);

static void mvt_telnet_notify_agreed_naws(mvt_telnet_t *telnet);
static void mvt_telnet_sub_command_terminal_type(mvt_telnet_t *telnet, const uint8_t *p, size_t len);
static void mvt_telnet_sub_command_new_environ(mvt_telnet_t *telnet, const uint8_t *p, size_t len);
#define mvt_telnet_set_flags(telnet, value) ((telnet)->flags |= (value))
#define mvt_telnet_unset_flags(telnet, value) ((telnet)->flags &= ~(value))
#define mvt_telnet_check_flags(telnet, value) ((telnet)->flags & (value))

#define mvt_telnet_get_option_will(telnet, index) mvt_get_index_flag(telnet->options_will, sizeof telnet->options_will, index)
#define mvt_telnet_set_option_will(telnet, index, value) mvt_set_index_flag(telnet->options_will, sizeof telnet->options_will, index, value)
#define mvt_telnet_get_option_do(telnet, index) mvt_get_index_flag(telnet->options_do, sizeof telnet->options_do, index)
#define mvt_telnet_set_option_do(telnet, index, value) mvt_set_index_flag(telnet->options_do, sizeof telnet->options_do, index, value)
static int mvt_get_index_flag(void *flags, int size, int index);
static void mvt_set_index_flag(void *flags, int size, int index, int value);
static void mvt_telnet_send_naws(mvt_telnet_t *telnet);

static int mvt_telnet_negotiate_echo(mvt_telnet_t *telnet, uint8_t command, int option);
static int mvt_telnet_negotiate_terminal_type(mvt_telnet_t *telnet, uint8_t command, int option);
static int mvt_telnet_negotiate_new_environ(mvt_telnet_t *telnet, uint8_t command, int option);
static size_t mvt_telnet_write0(mvt_telnet_t *session, const void *buf, size_t len);
static void mvt_telnet_close(mvt_session_t *session);
static int mvt_telnet_connect(mvt_session_t *session);
static int mvt_telnet_read (mvt_session_t *session, void *buf, size_t count, size_t *countread);
static int mvt_telnet_write (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten);
static void mvt_telnet_shutdown(mvt_session_t *session);
static void mvt_telnet_resize(mvt_session_t *session, int columns, int rows);

typedef struct _mvt_telnet_negotiate_t mvt_telnet_negotiate_t;
struct _mvt_telnet_negotiate_t
{
  int option;
  int (*negotiate)(mvt_telnet_t *telnet, uint8_t command, int option);
  void (*notify_agreed)(mvt_telnet_t *telnet);
  void (*sub_command)(mvt_telnet_t *telnet, const uint8_t *p, size_t len);
};

static const mvt_session_vt_t mvt_telnet_vt = {
    mvt_telnet_close,
    mvt_telnet_connect,
    mvt_telnet_read,
    mvt_telnet_write,
    mvt_telnet_shutdown,
    mvt_telnet_resize
};

static const mvt_telnet_negotiate_t negotiate_list[] =
  {
    {
      MVT_OPTION_ECHO,
      mvt_telnet_negotiate_echo,
      NULL,
      NULL
    },
    {
      MVT_OPTION_NAWS,
      NULL,
      mvt_telnet_notify_agreed_naws,
      NULL
    },
    {
      MVT_OPTION_TERMINAL_TYPE,
      mvt_telnet_negotiate_terminal_type,
      NULL,
      mvt_telnet_sub_command_terminal_type
    },
    {
      MVT_OPTION_SUPPRESS_GO_AHEAD,
      NULL,
      NULL,
      NULL
    },
    {
      MVT_OPTION_NEW_ENVIRON,
      mvt_telnet_negotiate_new_environ,
      NULL,
      mvt_telnet_sub_command_new_environ,
    },
    { -1,
      NULL,
      NULL,
      NULL
    }
  };

mvt_session_t *
mvt_telnet_open (char **args, mvt_session_t *source, int width, int height)
{
    mvt_telnet_t *telnet = malloc(sizeof (mvt_telnet_t));
    MVT_DEBUG_PRINT1("mvt_telnet_open\n");
    if (telnet == NULL)
        return NULL;
    memset(telnet, 0, sizeof *telnet);
    telnet->parent.vt = &mvt_telnet_vt;
    telnet->source = source;
	telnet->width = width;
	telnet->height = height;
	while (*args) {
		const char *name, *value;
		name = *args++;
		value = *args++;
		if (value == NULL) value = "";
		if (strcmp(name, "terminal_type") == 0) {
			telnet->terminal_type = strdup(value);
		} else if (strcmp(name, "x_display_location") == 0) {
			telnet->x_display_location = strdup(value);
		} else if (strcmp(name, "username") == 0) {
			telnet->username = strdup(value);
		}
	}
    return &telnet->parent;
}

static void
mvt_telnet_close(mvt_session_t *session)
{
    mvt_telnet_t *telnet = (mvt_telnet_t *)session;
    mvt_session_close(telnet->source);
}

static int
mvt_telnet_connect (mvt_session_t *session)
{
    mvt_telnet_t *telnet = (mvt_telnet_t *)session;
    MVT_DEBUG_PRINT1("mvt_telnet_connect()\n");
    return mvt_session_connect(telnet->source);
}

static int
mvt_telnet_read (mvt_session_t *session, void *buf, size_t len, size_t *countread)
{
    mvt_telnet_t *telnet = (mvt_telnet_t *)session;
    uint8_t *q = buf;
    const uint8_t *p;
    const uint8_t *lp;
    size_t n;

    if (mvt_session_read(telnet->source, buf, len, &n) < 0) {
        MVT_DEBUG_PRINT1("mvt_telnet_notify_socket_read\n");
        return -1;
    }
    p = buf;
    lp = (uint8_t *)buf + n;
    while (p < lp) {
        switch (telnet->state) {
        case MVT_TELNET_STATE_NORMAL:
            if (*p == MVT_COMMAND_IAC) { /* IAC */
                /* MVT_DEBUG_PRINT3("mvt_telnet_notify_socket_read: IAC\n"); */
                telnet->state = MVT_TELNET_STATE_IAC;
                p++;
            } else {
                size_t i = 0;
                MVT_DEBUG_PRINT1("mvt_telnet_notify_socket_read: normal\n");
                while (p + i < lp && p[i] != MVT_COMMAND_IAC) i++;
                memmove(q, p, i);
                q += i;
                p += i;
            }
            break;
        case MVT_TELNET_STATE_IAC:
            mvt_telnet_process_iac(telnet, *p++);
            break;
        case MVT_TELNET_STATE_NEGOTIATE:
            mvt_telnet_negotiate(telnet, telnet->tmp, *p++);
            break;
        case MVT_TELNET_STATE_SB:
            MVT_DEBUG_PRINT1("mvt_telnet_notify_socket_read: in SB\n");
            telnet->tmp = *p++;
            telnet->sub_command_length = 0;
            telnet->state = MVT_TELNET_STATE_SB_COMMAND;
            break;
        case MVT_TELNET_STATE_SB_COMMAND:
            if (*p == MVT_COMMAND_IAC) { /* IAC */
                MVT_DEBUG_PRINT1("mvt_telnet_notify_socket_read: IAC in SB\n");
                telnet->state = MVT_TELNET_STATE_SB_IAC;
                p++;
            } else {
                size_t i = 0;
                while (p + i < lp && p[i] != MVT_COMMAND_IAC) i++;
                telnet->sub_command_length += i;
                mvt_telnet_sub_command(telnet, telnet->tmp, p, i);
                p += i;
            }
            break;
        case MVT_TELNET_STATE_SB_IAC:
            switch (*p & 0xff) {
            case 240: /* SE */
                MVT_DEBUG_PRINT1("mvt_telnet_notify_socket_read: SE\n");
                telnet->state = MVT_TELNET_STATE_NORMAL;
                mvt_telnet_sub_command(telnet, telnet->tmp, NULL, -1);
                break;
            }
            p++;
            break;
        default:
            telnet->state = MVT_TELNET_STATE_NORMAL;
            p++;
            MVT_DEBUG_PRINT1("mvt_telnet_notify_socket_read: else\n");
            break;
        }
    }
    *countread = q - (uint8_t *)buf;
    return 0;
}

static void
mvt_telnet_process_iac(mvt_telnet_t *telnet, uint8_t c)
{
  if (((c - 251) & 0xff) > 4) /* suppress negotiation */
    MVT_DEBUG_PRINT2("mvt_telnet_process_iac: %s\n", mvt_telnet_command_name(c));

  switch (c & 0xff)
    {
    case 240: /* SE */
      telnet->state = MVT_TELNET_STATE_SE;
      break;
    case 241: /* NOP */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 242: /* Data Mark */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 243: /* Break */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 244: /* Interrupt Process */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 245: /* Abort output */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 246: /* Are You There */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 247: /* Erase character */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 248: /* Erase Line */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case 249: /* Go ahead */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    case MVT_COMMAND_SB: /* SB */
      telnet->state = MVT_TELNET_STATE_SB;
      break;
    case 251: /* WILL */
    case 252: /* WON'T */
    case 253: /* DO */
    case 254: /* DON'T */
      telnet->tmp = c;
      telnet->state = MVT_TELNET_STATE_NEGOTIATE;
      break;
    case MVT_COMMAND_IAC: /* IAC */
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    default:
      telnet->state = MVT_TELNET_STATE_NORMAL;
      break;
    }
}

static void
mvt_telnet_negotiate(mvt_telnet_t *telnet, uint8_t command, uint8_t option)
{
  const mvt_telnet_negotiate_t *negotiate;
  int i;
  int ok = 0;

  for (i = 0; negotiate_list[i].option != -1; i++)
    {
      if (negotiate_list[i].option == option)
        break;
    }
  negotiate = &negotiate_list[i];

  if (command == MVT_COMMAND_WILL || command == MVT_COMMAND_DO)
    {
      if (negotiate->option != -1)
        {
          if (negotiate->negotiate == NULL)
            ok = 1;
          else
            ok = negotiate->negotiate(telnet, command, option);
        }
    }

  MVT_DEBUG_PRINT4("mvt_telnet_negotiate(%s,%d): %d\n", mvt_telnet_command_name(command), option, ok);

  switch (command)
    {
    case MVT_COMMAND_WILL:
      mvt_telnet_reply_negotiate(telnet, ok ? MVT_COMMAND_DO : MVT_COMMAND_DONT, option);
      mvt_telnet_set_option_will(telnet, option, ok);
      break;
    case MVT_COMMAND_WONT:
      mvt_telnet_reply_negotiate(telnet, MVT_COMMAND_DONT, option);
      mvt_telnet_set_option_will(telnet, option, 0);
      break;
    case MVT_COMMAND_DO:
      mvt_telnet_reply_negotiate(telnet, ok ? MVT_COMMAND_WILL : MVT_COMMAND_WONT, option);
      mvt_telnet_set_option_do(telnet, option, ok);
      if (ok && negotiate_list[i].notify_agreed != NULL)
        negotiate_list[i].notify_agreed(telnet);
      break;
    case MVT_COMMAND_DONT:
      mvt_telnet_reply_negotiate(telnet, MVT_COMMAND_WONT, option);
      mvt_telnet_set_option_do(telnet, option, 0);
      break;
    }

  telnet->state = MVT_TELNET_STATE_NORMAL;
}

static void
mvt_telnet_reply_negotiate(mvt_telnet_t *telnet, uint8_t command, uint8_t c)
{
  uint8_t buf[3];
  size_t len;

  MVT_DEBUG_PRINT3("mvt_telnet_reply_negotiate(%s,%d)\n", mvt_telnet_command_name(command), c);

  buf[0] = MVT_COMMAND_IAC; /* IAC */
  buf[1] = command;
  buf[2] = c;
  len = mvt_telnet_write0(telnet, buf, 3);
  if (len == -1)
    {
      MVT_DEBUG_PRINT1("error\n");
    }
}

static int
mvt_telnet_negotiate_echo(mvt_telnet_t *telnet, uint8_t command, int option)
{
  switch (command)
    {
    case MVT_COMMAND_WILL:
      return 1;
    case MVT_COMMAND_DO:
      return 0;
    }
  return 0;
}

static void
mvt_telnet_sub_command(mvt_telnet_t *telnet, int option, const uint8_t *p, size_t len)
{
  int i;
  for (i = 0; negotiate_list[i].option != -1; i++)
    {
      if (negotiate_list[i].option == option)
        {
          if (negotiate_list[i].sub_command != NULL)
            negotiate_list[i].sub_command(telnet, p, len);
          break;
        }
    }
}

static int
mvt_telnet_negotiate_terminal_type(mvt_telnet_t *telnet, uint8_t command, int option)
{
  switch (command)
    {
    case MVT_COMMAND_WILL:
      return FALSE;
    case MVT_COMMAND_DO:
      return telnet->terminal_type != NULL;
    }
  return FALSE;
}

static void
mvt_telnet_sub_command_terminal_type(mvt_telnet_t *telnet, const uint8_t *p, size_t len)
{
  if (len == -1)
    {
      uint8_t buf[4];
      const char *type;
      buf[0] = MVT_COMMAND_IAC; /* IAC */
      buf[1] = MVT_COMMAND_SB; /* SB */
      buf[2] = 24; /* TERMINAL-TYPE */
      buf[3] = 0; /* IS */
      MVT_DEBUG_PRINT1("mvt_telnet_state_reply_terminal_type\n");
      mvt_telnet_write0(telnet, buf, 4);
      type = telnet->terminal_type ? telnet->terminal_type : "vt100";
      mvt_telnet_write0(telnet, type, strlen(type));
      buf[1] = MVT_COMMAND_SE; /* SE */
      mvt_telnet_write0(telnet, buf, 2);
    }
  else
    {
      if (telnet->sub_command_length == 0)
        {
          if (*p != 1)
            MVT_DEBUG_PRINT1("mvt_telnet_state_process_terminal_type: to be ignored\n");
        }
    }
}

static int
mvt_telnet_negotiate_new_environ(mvt_telnet_t *telnet, uint8_t command, int option)
{
  switch (command)
    {
    case MVT_COMMAND_WILL:
      return FALSE;
    case MVT_COMMAND_DO:
      return telnet->username != NULL;
    }
  return FALSE;
}

static void
mvt_telnet_sub_command_new_environ(mvt_telnet_t *telnet, const uint8_t *p, size_t len)
{
  if (len == -1)
    {
      uint8_t buf[10];
      const char *username;
      buf[0] = MVT_COMMAND_IAC; /* IAC */
      buf[1] = MVT_COMMAND_SB; /* SB */
      buf[2] = MVT_OPTION_NEW_ENVIRON;
      buf[3] = 0; /* IS */
      buf[4] = 0; /* VAR */
      buf[5] = 'U';
      buf[6] = 'S';
      buf[7] = 'E';
      buf[8] = 'R';
      buf[9] = 1; /* VALUE */
      MVT_DEBUG_PRINT1("mvt_telnet_state_reply_terminal_type\n");
      mvt_telnet_write0(telnet, buf, 10);
      username = telnet->username;
      mvt_telnet_write0(telnet, username, strlen(username));
      buf[1] = MVT_COMMAND_SE; /* SE */
      mvt_telnet_write0(telnet, buf, 2);
    }
  else
    {
      if (telnet->sub_command_length == 0)
        {
          if (*p != 1)
            MVT_DEBUG_PRINT1("mvt_telnet_state_process_terminal_type: to be ignored\n");
        }
    }
}

#ifdef ENABLE_DEBUG
static const char *
mvt_telnet_command_name(uint8_t c)
{
  const static char *command_name[] =
    {
      "SE", "NOP", "DM", "BREAK", "IP", "AO", "AYT", "EC", "EL", "GA",
      "SB", "WILL", "WONT", "DO", "DONT", "IAC"
    };

  if (c >= 240 /* && c <= MVT_COMMAND_IAC */)
    return command_name[c - 240];
  else
    return "invalid";
}
#endif

static void
mvt_telnet_notify_agreed_naws(mvt_telnet_t *telnet)
{
  mvt_telnet_send_naws(telnet);
}

static void
mvt_telnet_send_naws(mvt_telnet_t *telnet)
{
  uint8_t buf[9];
  int columns = telnet->width;
  int rows = telnet->height;
  buf[0] = MVT_COMMAND_IAC;
  buf[1] = MVT_COMMAND_SB;
  buf[2] = MVT_OPTION_NAWS;
  buf[3] = columns >> 8;
  buf[4] = columns & 0xff;
  buf[5] = rows >> 8;
  buf[6] = rows & 0xff;
  buf[7] = MVT_COMMAND_IAC;
  buf[8] = MVT_COMMAND_SE;
  mvt_telnet_write0(telnet, buf, 9);
}

static int
mvt_get_index_flag(void *flags, int size, int index)
{
  unsigned char c;
  if (index > size * 8) return 0;
  c = ((unsigned char *)flags)[index >> 3];
  return c & (1 << (index & 7)) ? 1 : 0;
}

static void
mvt_set_index_flag(void *flags, int size, int index, int value)
{
  unsigned char *p;
  if (index > size * 8) return;
  p = &((unsigned char *)flags)[index >> 3];
  if (value)
    *p |= 1 << (index & 7);
  else
    *p &= ~(1 << (index & 7));
}

static size_t
mvt_telnet_write0(mvt_telnet_t *telnet, const void *buf, size_t count)
{
    size_t n;
    if (mvt_session_write(telnet->source, buf, count, &n) < 0) {
        return (size_t)-1;
    }
    return n;
}

static int
mvt_telnet_write (mvt_session_t *session, const void *buf, size_t count, size_t *countwritten)
{
    size_t ret;
    mvt_telnet_t *telnet = (mvt_telnet_t *)session;
    ret = mvt_telnet_write0(telnet, buf, count);
    if (ret < 0)
        return -1;
    if (ret == 0)
        return -1;
    *countwritten = ret;
    return 0;
}

static void mvt_telnet_shutdown(mvt_session_t *session)
{
}

static void mvt_telnet_resize(mvt_session_t *session, int columns, int rows)
{
    mvt_telnet_t *telnet = (mvt_telnet_t *)session;
    telnet->width = columns;
    telnet->height = rows;
    if (mvt_telnet_get_option_do(telnet, MVT_OPTION_NAWS))
        mvt_telnet_send_naws(telnet);
}
