-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.roomlayer == nil then
    rfs.roomlayer = {}
    rfs.roomlayer._class = {}
end

function rfs.roomlayer.new(id_or_nil)
    local result = _roomlayer_new(id_or_nil)
    debug.setmetatable(result, {
        __index = rfs.roomlayer._class,
        __gc = function(self)
            local f = function(gcself)
                _roomlayer_destroy(gcself)
            end
            pcall(f, self)
        end
    })
    return result
end

rfs.roomlayer._class.room_ids = _roomlayer_getallrooms
rfs.roomlayer._class.room_count = _roomlayer_getroomcount
rfs.roomlayer._class.add_room = _roomlayer_addroom
rfs.roomlayer._class.set_roomprops = function(self, v)
    if type(v) ~= "table" then
        error("expected argument of type table")
    end
    if type(v.id) == "number" then
        return _roomlayer_applyroomprops(self, {v})
    end
    return _roomlayer_applyroomprops(self, v)
end
rfs.roomlayer._class.deserialize_rooms = function(self, v)
    if type(v) ~= "table" then
        error("expected argument of type table")
    end
    return _roomlayer_deserializeroomstolayer(self, v)
end 
