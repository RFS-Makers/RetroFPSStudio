-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.archive == nil then rfs.archive = {} end

rfs.archive.open = function(path)
    if type(path) ~= "string" then
        error("open argument must be string")
    end
    local self = _archive_open(path)
    debug.setmetatable(self, {
        __index = rfs.archive.open,
        __gc = function(gcself)
            local f = function(_gcself)
                _archive_close(_gcself)
            end
            pcall(f, _gcself)
        end
    })
    return self
end

rfs.archive.entry_count = function(self)
    return _archive_getentrycount(self)
end

rfs.archive.entries = function(self)
    local c = _archive_getentrycount(self)
    local result = {}
    local i = 1
    while i <= #c do
        table.insert(result, _archive_getentryname(self, i))
        i = i + 1
    end
    return result
end
