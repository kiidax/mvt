-- -*- mode: lua; coding: utf-8; -*-
-- vi: sw=3 ts=8

local mvtui = require "mvtui"

mvtui.default_screen_spec = ""
mvtui.default_terminal_spec = ""
screen = mvtui.start_default_ui()
local terminal = screen.current_terminal;
local a09 = "0123456789"
local i
for i = 1, 8 do
    terminal:write(a09)
end
terminal:write("\027[3K")
terminal:write("\027[2K")
terminal:write("\027[1K")
terminal:write("\027[0K")
