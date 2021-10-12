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

