-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.song == nil then
    rfs.song = {}
    rfs.song.classtable = {}
end


rfs.song.load = function(path)
    if type(path) ~= "string" then
        error("song path must be a string")
    end
    local song = _h3daudio_loadsong(path)
    assert(type(song) == "userdata")
    debug.setmetatable(song, {
        __index = rfs.song.classtable,
        __gc = function(gcself)
            local f = function(_gcself)
                _h3daudio_destroysong(_gcself)
            end
            pcall(f, gcself)
        end
    })
    return song
end


function rfs.song.classtable:play(volume, looped, dev)
    if dev == nil and rfs.audio.default_device == nil then
        rfs.audio.default_device = rfs.audio.dev.open()
        if rfs.audio.default_device == nil then
            return
        end
    end
    if dev == nil then
        dev = rfs.audio.default_device
    end
    assert(type(dev) == "userdata")
    if type(self) ~= "userdata" then
        error("oops, self reference is wrong")
    end
    if type(volume) ~= "number" and volume ~= nil then
        if type(volume) == "boolean" and type(looped) == "nil" then
            looped = volume
            volume = nil
        else
            error("volume must be number or nil")
        end
    end
    if type(looped) ~= "boolean" and looped ~= nil then
        error("looped must be boolean or nil")
    end
    _h3daudio_playsong(dev, self,
        volume, looped)
end


function rfs.song.classtable:length()
    return _h3daudio_songlength(self)
end


function rfs.song.housekeeping()
    local now = rfs.time.ticks()
    if rfs.song._last_housekeeping == nil or
            rfs.song._last_housekeeping + 5000 < now then
        rfs.song._last_housekeeping = now
        _h3daudio_checkdestroysongsdelayed()
    end
end

