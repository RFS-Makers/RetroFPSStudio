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
    debug.setmetatable(font, {
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
    return _graphicsfont_calcwidth(
        self, text, pt_size, letter_spacing
    )
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
        self, text, width, pt_size, letter_spacing)
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
        self, text, width, pt_size, letter_spacing)
end

function rfs.font.classtable.draw(
        self, text, width, x, y, r, g, b, a,
        pt_size, letter_spacing
        )
    if type(text) ~= "string" then
        error("expected arg of type string")
    end
    if type(pt_size) ~= "number" then
        error("pt_size must be number, is " .. type(pt_size))
    end
    if type(x) ~= "number" then
        error("x must be number, is " .. type(pt_size))
    end
    if type(y) ~= "number" then
        error("x must be number, is " .. type(pt_size))
    end
    return _graphicsfont_draw(
        self, text, width, x, y, r, g, b, a,
        pt_size, letter_spacing)
end
