-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.net == nil then rfs.net = {} end

rfs.net.dlclass = {}


rfs.net.dlclass.size = function(self)
    local f = self
    return _http_getdownloadbytecount(f)
end


rfs.net.dlclass.done = function(self)
    local f = self
    return _http_isdownloaddone(f)
end


rfs.net.dlclass.contents = function(self)
    local f = self
    return _http_getdownloadcontents(f)
end


rfs.net.escape_plus = function(t)
    if type(t) ~= "string" then
        error("expected arg of type string")
    end
    local tresult = ""
    local i = 1
    while i <= #t do
        local c = t:sub(i, i)
        if c == "&" or c == "/" or
                c == "?" or c == "=" or c == "+" or
                c == "#" or c == "@" or c == ":" or
                c == "\\" or c == "%" or c == "$" or
                c == "(" or c == ")" or c == "[" or
                c == "]" or c == "," or c == "'" or
                c == "\"" or c == ";" or c == "\n" or
                c == "\r" then
            tresult = tresult .. "%"
            local output = string.format("%x", string.byte(c))
            if #output < 2 then
                output = "0" .. output
            end
            tresult = tresult .. output:upper()
        elseif c == " " then
            tresult = tresult .. "+"
        else
            tresult = tresult .. c
        end
        i = i + 1
    end
    return tresult
end


rfs.net.download = function(url, options)
    if type(options) ~= "table" and
            type(options) ~= "nil" then
        error("options must be a table")
    end
    local result = (
        _http_newdownload(url, options)
    )
    debug.setmetatable(result, {
        __index = rfs.net.dlclass,
        __gc = function(self)
            local f = function(gcself)
                _http_freedownload(f, gcself)
            end
            pcall(f, self)
        end
    })
    return result
end

