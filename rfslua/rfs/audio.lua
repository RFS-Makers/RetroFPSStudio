-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.audio == nil then
    rfs.audio = {dev={classtable={}}, sound={classtable={}}}
end


rfs.audio.play = function(path, volume, pan, looped, dev)
    if type(path) ~= "string" and type(path) ~= "userdata" then
        error("sound path must be string or predecodedsound")
    end
    if type(dev) ~= "nil" and type(dev) ~= "userdata" then
        error("device if specified must be a sound device")
    end
    if rfs.audio.default_device == nil then
        rfs.audio.default_device = rfs.audio.dev.open()
    end
    if type(volume) ~= "number" and volume ~= nil then
        error("volume must be number or nil")
    end
    if type(pan) ~= "number" and pan ~= nil then
        error("pan must be number or nil")
    end
    if type(looped) ~= "boolean" and looped ~= nil then
        error("looped must be boolean or nil")
    end
    if dev == nil and rfs.audio.default_device == nil then
        rfs.audio.default_device = rfs.audio.dev.open()
        if rfs.audio.default_device == nil then
            return
        end
    end
    if dev == nil then
        dev = rfs.audio.default_device
    end
    _h3daudio_playsound(dev, path,
        volume, pan, looped)
end


rfs.audio.preloadsfx = function(path, dev)
    if dev == nil and rfs.audio.default_device == nil then
        rfs.audio.default_device = rfs.audio.dev.open()
        -- We don't bail here even if opening the device fails.
        -- This way, we can still quietly preload the sound.
    end
    if dev == nil then
        dev = rfs.audio.default_device
    end
    local sound = _h3daudio_preloadsound(dev, path)
    debug.setmetatable(sound, {
        __gc = function(gcself)
            local f = function(_gcself)
                _h3daudio_destroypreloadedsound(_gcself)
            end
            pcall(f, gcself)
        end
    })
    return sound
end


rfs.audio.dev.open = function(name)
    if type(name) ~= "string" and type(name) ~= "nil" then
        error("device name must be string or nil")
    end
    local self = _h3daudio_opendevice(path)
    debug.setmetatable(self, {
        __index = rfs.audio.dev.classtable,
        __gc = function(gcself)
            local f = function(_gcself)
                _h3daudio_destroydevice(_gcself)
            end
            pcall(f, gcself)
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

