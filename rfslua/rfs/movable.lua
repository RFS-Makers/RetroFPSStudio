-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details


if rfs == nil then rfs = {} end
if rfs.movable == nil then
    rfs.movable = {}
    rfs.movable._class = {}
end

function rfs.movable.new_invisible()
    local result = _roomobj_newinvismovable()
    debug.setmetatable(result, {
        __index = rfs.movable._class,
        --[[__gc = function(self)
            local f = function(gcself)
                _roomobj_destroy(gcself)
            end
            pcall(f, self)
        end]]
    })
    return result
end

function rfs.movable.new_sprite(path)
    local result = _roomobj_newspritemovable(path)
    debug.setmetatable(result, {
        __index = rfs.movable._class,
        --[[__gc = function(self)
            local f = function(gcself)
                _roomobj_destroy(gcself)
            end
            pcall(f, self)
        end]]
    })
    return result
end

rfs.movable._class.set_pos = _roomobj_setpos
rfs.movable._class.get_pos = _roomobj_getpos
rfs.movable._class.set_angle = _roomobj_setangle
rfs.movable._class.get_angle = _roomobj_getangle
rfs.movable._class.set_layer = _roomobj_setlayer
rfs.movable._class.set_light = _movable_setemit
rfs.movable._class.get_obj_id = _roomobj_getid

