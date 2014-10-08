bin_PROGRAMS = mvt
mvt_SOURCES = session.c console.c misc.c terminal.c \
	worker.c driver.c socket.c \
	debug.h driver.h misc.h mvt.h mvt_lua.h mvt_plugin.h \
	private.h $(platform_SOURCES) $(mvt_DATA)
mvt_DATA = mvtui.lua default.lua
mvtdir = $(datadir)/mvt
platform_SOURCES =
platform_LIBS =
EXTRA_DIST = mvt.ico

AM_CPPFLAGS = -I$(top_srcdir) -DMVTDATADIR='"$(mvtdir)"'

if HAVE_SDL
platform_SOURCES += mvt_sdl.c
endif

if HAVE_WIN32
platform_SOURCES += mvt_win32.c mvt_res.rc
endif

if HAVE_COCOA
platform_SOURCES += mvt_cocoa.m
endif

if HAVE_LUA
platform_SOURCES += mvt_lua.c
endif

if ENABLE_PTY
platform_SOURCES += pty.c
endif

if ENABLE_TELNET
platform_SOURCES += telnet.c
endif

.rc.o:
	windres -o $@ $<
