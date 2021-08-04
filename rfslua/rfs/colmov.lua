-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.


if rfs == nil then rfs = {} end
if rfs.colmov == nil then
    rfs.colmov = {}
    rfs.colmov._class = {}
end

function rfs.colmov.new_invisible()
    local result = _roomobj_newinviscolmov()
    debug.setmetatable(result, {
        __index = rfs.colmov._class,
        __gc = function(self)
            local f = function(gcself)
                _roomobj_destroy(gcself)
            end
            pcall(f, self)
        end
    })
    return result
end

rfs.colmov._class.set_pos = _roomobj_setpos
rfs.colmov._class.get_pos = _roomobj_getpos
rfs.colmov._class.set_angle = _roomobj_setangle
rfs.colmov._class.get_angle = _roomobj_getangle
rfs.colmov._class.set_layer = _roomobj_setlayer

