-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.roomcam == nil then
    rfs.roomcam = {}
    rfs.roomcam._class = {}
end

function rfs.roomcam.new()
    local result = _roomcam_new()
    debug.setmetatable(result, {
        __index = rfs.roomcam._class,
        __gc = function(self)
            local f = function(gcself)
                _roomobj_destroy(gcself)
            end
            pcall(f, self)
        end
    })
    return result
end

function rfs.roomcam._class:render(self, x, y, w, h)
    _roomcam_render(self, x, y, w, h)
end

rfs.roomcam._class.draw = _roomcam_render
rfs.roomcam._class.set_pos = _roomobj_setpos
rfs.roomcam._class.get_pos = _roomobj_getpos
rfs.roomcam._class.set_angle = _roomobj_setangle
rfs.roomcam._class.get_angle = _roomobj_getangle
rfs.roomcam._class.set_vangle = _roomcam_setvangle
rfs.roomcam._class.get_vangle = _roomcam_getvangle
rfs.roomcam._class.set_layer = _roomobj_setlayer
rfs.roomcam._class.renderstats = _roomcam_renderstats

