-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.audio == nil then
    rfs.audio = {dev={classtable={}}, sound={classtable={}}}
end


rfs.audio.dev.open = function(name)
    if type(name) ~= "string" and type(name) ~= nil then
        error("device name must be string or nil")
    end
    local self = _h3daudio_opendevice(path)
    debug.setmetatable(self, {
        __index = rfs.audio.dev.classtable,
        __gc = function(gcself)
            local f = function(_gcself)
                _h3daudio_destroydevice(_gcself)
            end
            pcall(f, _gcself)
        end
    })
    return self
end


rfs.audio.dev.classtable.close = function()
    _h3daudio_destroydevice(self)
end


rfs.audio.dev.classtable.play = _h3daudio_playsound
rfs.audio.dev.classtable.stop = _h3daudio_stopsound
rfs.audio.dev.classtable.setpan = _h3daudio_soundsetpanning
rfs.audio.dev.classtable.setvol = _h3daudio_soundsetvolume
rfs.audio.dev.classtable.getpan = _h3daudio_soundgetpanning
rfs.audio.dev.classtable.getvol = _h3daudio_soundgetvolume
rfs.audio.dev.classtable.isplaying = _h3daudio_soundisplaying
rfs.audio.dev.classtable.haderror = _h3daudio_soundhaderror

