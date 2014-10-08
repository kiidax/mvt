-- -*- mode: lua; coding: utf-8; -*-
-- vi: sw=3 ts=8

local mvtui = require "mvtui"

local spec = "foreground-color=#ccc"
spec = spec .. ",background-color=#004"
spec = spec .. ",scroll-foreground-color=#f88"
spec = spec .. ",scroll-background-color=#888"
spec = spec .. ",width=80"
spec = spec .. ",height=34"
mvtui.default_screen_spec = spec
mvtui.default_terminal_spec_list = {}
mvtui.default_session_spec = nil
screen = mvtui.start_default_ui()

screen.theme_normal = function (this)
   this:set_attribute("foreground-color", "#000")
   this:set_attribute("background-color", "#fff")
end

