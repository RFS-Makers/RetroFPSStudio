-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.window == nil then
    rfs.window = {}
    rfs.window.renderw = 1
    rfs.window.renderh = 1
    rfs.window.uiscaler = 1
end

rfs.window.enable_fullscreen = _graphics_setfullscreen
rfs.window.force_software = _window_no3dacc
rfs.window.set_rel_mouse = _window_setrelmouse
rfs.window.quit = _window_quit
rfs.window.get_size = _window_getsize
