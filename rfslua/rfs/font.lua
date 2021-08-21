-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.font == nil then
    rfs.font = {}
    rfs.font.classtable = {}
end

rfs.font.load = function(path)
    if type(path) ~= "string" then
        error("loaded font path must be a string")
    end
    local font = _graphicsfont_get(path)
    setmetatable(font, {
        __index = rfs.font.classtable,
        --[[__gc = function(gcself)
            local f = function(_gcself)
                _graphicsfront_close(_gcself)
            end
            pcall(f, _gcself)
        end]]
    })
    return font
end


function rfs.font.classtable.calcwidth(
        self, text, pt_size, letter_spacing
        )
    if type(text) ~= "string" then
        error("expected arg of type string")
    end
    if type(pt_size) ~= "number" then
        error("pt_size must be number, is " .. type(pt_size))
    end
    return _graphicsfont_calcwidth(text, pt_size, letter_spacing)
end

function rfs.font.classtable.textwrap(
        self, text, width, pt_size, letter_spacing
        )
    if type(text) ~= "string" then
        error("expected arg of type string")
    end
    if type(pt_size) ~= "number" then
        error("pt_size must be number, is " .. type(pt_size))
    end
    return _graphicsfont_textwrap(
        text, width, pt_size, letter_spacing)
end

function rfs.font.classtable.calcheight(
        self, text, width, pt_size, letter_spacing
        )
    if type(text) ~= "string" then
        error("expected arg of type string")
    end
    if type(pt_size) ~= "number" then
        error("pt_size must be number, is " .. type(pt_size))
    end
    return _graphicsfont_calcheight(
        text, width, pt_size, letter_spacing)
end

function rfs.font.classtable.draw(
        self, text, x, y, width, r, g, b, a,
        pt_size, letter_spacing
        )
    if type(text) ~= "string" then
        error("expected arg of type string")
    end
    if type(pt_size) ~= "number" then
        error("pt_size must be number, is " .. type(pt_size))
    end
    return _graphicsfont_draw(
        text, width, r, g, b, a,
        pt_size, letter_spacing)
end
