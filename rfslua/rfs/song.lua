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
            pcall(f, _gcself)
        end
    })
    return song
end


function rfs.song.classtable:play(volume, looped)
    if rfs.audio.default_device == nil then
        rfs.audio.default_device = rfs.audio.dev.open()
    end
    assert(type(rfs.audio.default_device) == "userdata")
    if type(self) ~= "userdata" then
        error("oops, self reference is wrong")
    end
    if type(volume) ~= "number" and volume ~= nil then
        error("volume must be number or nil")
    end
    if type(looped) ~= "boolean" and looped ~= nil then
        error("looped must be boolean or nil")
    end
    _h3daudio_playsong(rfs.audio.default_device, self,
        volume, looped)
end


function rfs.song.classtable:length()
    return _h3daudio_songlength(self)
end

