AC_INIT([mvt], 0.2.0)
AC_CONFIG_SRCDIR(mvt/telnet.c)
AC_CONFIG_HEADER(mvt/config.h)
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_OBJC

targetos=`uname -s`
case $targetos in
CYGWIN*) CFLAGS="$CFLAGS -D_GNU_SOURCES" ;;
esac

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([floor])
AC_CHECK_FUNCS([atexit gethostbyname memmove memset setenv socket])

# Checks for iconv library
AC_CHECK_FUNC(iconv_open, [
  AC_DEFINE(HAVE_ICONV)
  found_iconv=yes])
if test x$found_iconv != xyes ; then
  AC_CHECK_LIB(iconv, libiconv_open, [
    AC_DEFINE(HAVE_LIBICONV)
    LIBS="$LIBS -liconv"
    found_iconv=yes
])
fi
if test x$found_iconv != xyes ; then
  AC_CHECK_LIB(iconv, iconv_open, [
    AC_DEFINE(HAVE_ICONV)
    LIBS="$LIBS -liconv"
    found_iconv=yes
])
fi
AH_TEMPLATE([HAVE_ICONV], [])
AH_TEMPLATE([HAVE_LIBICONV], [])

AC_ARG_WITH([win32],
  [AS_HELP_STRING([--with-win32],[use Windows GDI])])
if test x$with_win32 == xyes ; then
  AC_DEFINE([HAVE_WIN32], [1])
  CFLAGS="$CFLAGS -D_WIN32_WINNT=0x0500"
  LIBS="$LIBS -lgdi32"
fi
AM_CONDITIONAL([HAVE_WIN32], [test x$with_win32 == xyes])
AH_TEMPLATE([HAVE_WIN32], [])

AC_ARG_WITH([sdl],
  [AS_HELP_STRING([--with-sdl],[use SDL])])
if test x$with_sdl == xyes ; then
  AC_DEFINE([HAVE_SDL], [1])
  case $targetos in
  CYGWIN*) ;;
  *)
    AM_PATH_SDL(1.2.0,:,)
    CFLAGS="$CFLAGS $SDL_CFLAGS"
    LIBS="$LIBS $SDL_LIBS"
    AC_CHECK_LIB([SDL_ttf], [TTF_OpenFont])
  esac
fi
AM_CONDITIONAL([HAVE_SDL], [test x$with_sdl == xyes])
AH_TEMPLATE([HAVE_SDL], [])

AC_ARG_WITH([cocoa],
  [AS_HELP_STRING([--with-cocoa],[use Cocoa])])
if test x$with_cocoa == xyes ; then
  AC_DEFINE([HAVE_COCOA])
  LIBS="$LIBS -framework Cocoa"
fi
AM_CONDITIONAL([HAVE_COCOA], [test x$with_cocoa == xyes])
AH_TEMPLATE([HAVE_COCOA], [])

if test x$with_cocoa == xyes ; then
  AC_DEFINE([HAVE_PTHREAD])
  with_pthread=yes
fi
if test x$with_win32 == xyes ; then
  AC_DEFINE([HAVE_PTHREAD])
  with_pthread=yes
fi
AM_CONDITIONAL([HAVE_PTHREAD], [test x$with_pthread == xyes])
AH_TEMPLATE([HAVE_PTHREAD], [])

AC_ARG_WITH([lua],
  [AS_HELP_STRING([--without-lua],[do not use Lua])])
if test x$with_lua != xno ; then
  AC_DEFINE([HAVE_LUA])
  AC_SUBST(LUA_CFLAGS)
  AC_SUBST(LUA_LIBS)
  CFLAGS="$CFLAGS $LUA_CFLAGS"
  LIBS="$LIBS $LUA_LIBS"
fi
AM_CONDITIONAL(HAVE_LUA, [test x$with_lua != xno])
AH_TEMPLATE([HAVE_LUA], [])

AC_ARG_ENABLE(pty,
  [AS_HELP_STRING([--disable-pty],[disable pseudo terminal support])])
if test x$enable_pty != xno ; then
  AC_DEFINE([ENABLE_PTY])
fi
AM_CONDITIONAL(ENABLE_PTY, [test x$enable_pty != xno])
AH_TEMPLATE([ENABLE_PTY], [])

AC_ARG_ENABLE(telnet,
  [AS_HELP_STRING([--disable-telnet],[disable telnet support])])
if test x$enable_telnet != xno ; then
  AC_DEFINE([ENABLE_TELNET])
fi
AM_CONDITIONAL(ENABLE_TELNET, [test x$enable_telnet != xno])
AH_TEMPLATE([ENABLE_TELNET], [])

AC_ARG_ENABLE(debug,
  [AS_HELP_STRING([--enable-debug],[turn on debuging])])
if test x$enable_debug = xyes ; then
  AC_DEFINE([ENABLE_DEBUG])
fi
AM_CONDITIONAL(ENABLE_DEBUG, [test x$enable_debug = xyes])
AH_TEMPLATE([ENABLE_DEBUG], [])

AC_OUTPUT([Makefile mvt/Makefile])
