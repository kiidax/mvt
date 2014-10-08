-- -*- mode: lua; coding: utf-8; -*-
-- vi: sw=3 ts=8

local mvtui = require "mvtui"

mvtui.default_screen_spec = ""
mvtui.default_terminal_spec = ""
screen = mvtui.start_default_ui()
local terminal = screen.current_terminal;
terminal:write("あいう")
