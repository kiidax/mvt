-- -*- mode: lua; coding: utf-8; -*-
-- vi: sw=3 ts=8

-- Multi-purpose Virtual Terminal
-- Copyright (C) 2010-2011 Katsuya Iida
--
-- This library is free software; you can redistribute it and/or
-- modify it under the terms of the GNU Lesser General Public
-- License as published by the Free Software Foundation; either
-- version 2 of the License, or (at your option) any later version.
--
-- This library is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
-- Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public
-- License along with this library; if not, write to the
-- Free Software Foundation, Inc., 59 Temple Place - Suite 330,
-- Boston, MA 02111-1307, USA.

local M = {}

-- global variables

M.default_screen_spec = ""
M.default_terminal_spec_list = {}
M.default_session = ""
M.font_size_table = { 8, 10, 12, 14, 16, 20, 24 }
M.screen_key_bind_table = {}
M.terminal_key_bind_table = {}
local first_terminal = nil
M.event_key = 1
M.event_resize = 2
M.event_data = 3
M.event_close = 4

-- Screen class

local Screen = {}
Screen.__index = Screen

function Screen:close ()
   return self.screen:close()
end

function Screen:set_attribute (name, value)
   return self.screen:set_attribute(name, value)
end

function Screen:open_new_terminal_and_connect ()
   local terminal = M.open_terminal(M.default_terminal_spec)
   local i
   for i=1,#M.default_session_spec_list do
      terminal:open(M.default_session_spec_list[i])
   end
   terminal:connect()
   terminal:attach_screen(self)
end

function Screen:switch_next ()
   self.current_terminal.next:attach_screen(self)
end

function Screen:increase_font_size ()
   if (self.font_size_index < #M.font_size_table) then
      self.font_size_index = self.font_size_index + 1
      local font_size = M.font_size_table[self.font_size_index]
      self:set_attribute("font-size", font_size)
   end
end

function Screen:decrease_font_size ()
   if (self.font_size_index > 1) then
      self.font_size_index = self.font_size_index - 1
      local font_size = M.font_size_table[self.font_size_index]
      self:set_attribute("font-size", font_size)
   end
end

function Screen:onkey_default (code)
   func = M.screen_key_bind_table[code]
   if func then
      func(self)
      return true
   end
   func = M.terminal_key_bind_table[code]
   if func then
      func(self.current_terminal)
      return true
   end
   return true
end

-- Terminal class

local Terminal = {}
Terminal.__index = Terminal

function Terminal:set_attribute (name, value)
   return self.terminal:set_attribute(name, value)
end

function Terminal:write (s)
   return self.terminal:write(s)
end

function Terminal:read ()
   return self.terminal:read()
end

function Terminal:open (spec)
   return self.terminal:open(spec)
end

function Terminal:connect_raw ()
   return self.terminal:connect()
end

function Terminal:suspend (spec)
   return self.terminal:suspend(spec)
end

function Terminal:resume (spec)
   return self.terminal:resume(spec)
end

function Terminal:attach_screen (screen)
   if screen.current_terminal then
      screen.current_terminal.attached_screen = nil
   end
   self.attached_screen = screen
   screen.current_terminal = self
   return self.terminal:attach_screen(screen.screen)
end

function Terminal:append_input_buffer (s)
   return self.terminal:append_input_buffer(s)
end

function Terminal:close ()
   return self.terminal:close()
end

function Terminal:connect ()
   local co
   local line = ""
   local function readline ()
      return coroutine.yield()
   end
   self.ondata = function ()
      local s = self:read()
      local i
      for i=1,s:len() do
	 c = s:sub(i, i)
	 if c == '\r' or c == '\n' then
	    self:write("\r\n")
	    coroutine.resume(co, line)
	    line = ""
	 elseif c == '\b' then
	    if line:len() > 0 then
	       line = line:sub(1,line:len()-1)
	       self:write("\b\027[K")
	    end
	 else
	    line = line .. c
	    self:write(c)
	 end
      end
   end
   self.onclose = function ()
		     M.quit()
		  end
   co = coroutine.create(function ()
	while true do
	   local s = readline()
	   if s == "" then
	      self:resume()
	   else
	      if s:sub(1,1) == '=' then
		 s = "return " .. s:sub(2)
	      end
	      local status, result = pcall(loadstring(s, "=escape"))
	      self:write("" .. _G.tostring(result) .. "\r\n")
	      self:write(">>> ")
	   end
	end
     end)
   self:connect_raw()
   coroutine.resume(co)
end

function Terminal:suspend_to_lua ()
   self:suspend()
   self:write("\r\n>>> ")
end

function Terminal:input_escape ()
   self:append_input_buffer("\012")
end

-- static functions

function M.open_screen (spec)
   local screen = {}
   setmetatable(screen, Screen)
   local function screen_func (type, code)
      return screen:onkey(code)
   end
   screen.screen = mvt.open_screen(spec, screen_func)
   screen.onkey = Screen.onkey_default
   screen.font_size_index = 3
   return screen
end

function M.open_terminal (spec)
   local terminal = {}
   setmetatable(terminal, Terminal)
   local function terminal_func (type)
      if type == M.event_data then
	 return terminal:ondata()
      elseif type == M.event_close then
	 return terminal:onclose()
      end
   end
   terminal.terminal = mvt.open_terminal(spec, terminal_func)
   if first_terminal == nil then
      first_terminal = terminal
      terminal.next = terminal
      terminal.previous = terminal
   else
      terminal.next = first_terminal
      terminal.previous = first_terminal.previous
      first_terminal.previous = terminal
      terminal.previous.next = terminal
   end
   return terminal
end

function M.quit ()
   mvt.quit()
end

function M.start_default_ui ()
   local screen = M.open_screen(M.default_screen_spec)
   screen:open_new_terminal_and_connect()
   local version = mvt.major_version
   version = version .. "." .. mvt.minor_version
   version = version .. "." .. mvt.micro_version
   screen.current_terminal:write("Welcome to mvt " .. version .. "\r\n")
   return screen
end

-- key binding

M.screen_key_bind_table[32] = Screen.switch_next -- SPC
M.screen_key_bind_table[43] = Screen.increase_font_size -- +
M.screen_key_bind_table[45] = Screen.decrease_font_size -- -
M.screen_key_bind_table[99] = Screen.open_new_terminal_and_connect -- c

M.terminal_key_bind_table[58] = Terminal.suspend_to_lua -- :
M.terminal_key_bind_table[107] = Terminal.close -- k
M.terminal_key_bind_table[108] = Terminal.input_escape -- l

return M
