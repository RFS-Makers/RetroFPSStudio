-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

if rfs == nil then rfs = {} end
if rfs.font == nil then
    rfs.font = {}
    rfs.font._fontcache = {}
    rfs.font.classtable = {}
end

rfs.font.load = function(path)
    if type(path) ~= "string" then
        error("loaded font path must be a string")
    end
    path = os.normpath(path)
    if (path:lower()):ends(".png") or
            (path:lower()):ends(".jpg") or
            (path:lower()):ends(".jpeg") or
            (path:lower()):ends(".webp") or
            (path:lower()):ends(".tga") then
        path = path:sub(1, path:rfind(".") - 1)
    end
    noendingpath = path
    if not (path:lower()):ends(".json") then
        path = path .. ".json"
    end
    if rfs.font._fontcache[path] ~= nil then
        return rfs.font._fontcache[path]
    end
    if not rfs.vfs.exists(path) then
        error("font not found in vfs: " .. path)
    end
    font_img_dir = os.dirname(path)
    local jsonfile = rfs.vfs.open(path, "rb")
    local jsontxt = jsonfile:read("*all")
    jsonfile:close()
    local data = rfs.json.parse(jsontxt)
    if type(data) ~= "table" or
            type(data["letterHeight"]) ~= "number" or
            type(data["letterWidth"]) ~= "number" then
        error("font data not valid, lacks letter sizes")
    end
    data["letterWidth"] = math.max(1, math.round(data["letterWidth"]))
    data["letterHeight"] = math.max(1, math.round(data["letterHeight"]))
    if type(data["letters"]) ~= "string" then
        error("font data not valid, lacks letters string")
    end
    data["letterStarts"] = {}
    local offset = 1
    local letters = {}
    local starts = {}
    local i = 1
    while i <= #data["letters"] do
        local len = rfs.widechar.utf8_char_len(
            data["letters"], i
        )
        table.insert(starts, offset)
        table.insert(letters, (data["letters"]):sub(i, i + len - 1))
        i = i + len
        offset = offset + len
    end
    data["letterStarts"] = starts
    data["letters"] = letters
    if data["fontFolder"] == nil then
        data["fontFolder"] = font_img_dir
    end
    if data["imageName"] ~= nil then
        data["imagePath"] = os.pathjoin(
            data["fontFolder"], data["imageName"]
        )
    else
        data["imagePath"] = os.pathjoin(
            data["fontFolder"], os.basename(noendingpath) .. ".png"
        )
    end
    data["image"] = rfs.gfx.get_tex(data["imagePath"])
    local tw, th = rfs.gfx.get_tex_size(data["image"])
    data.lettersPerRow = math.floor(tw / data.letterWidth)
    data.lettersPerCol = math.floor(th / data.letterHeight)
    if data.lettersPerRow < 1 or
            data.lettersPerCol < 1 then
        error("invalid font with less than one letter per " ..
            "column or per row (due to large letterWidth/letterHeight)")
    end
    if #data["letters"] > data["lettersPerRow"] * data["lettersPerCol"] then
        error("invalid font with more letters specified than " ..
            "fit in image (due to overly long letters list)")
    end
    data.lettersX = {}
    data.lettersY = {}
    local _currLetterX = -1
    local _currLetterY = 0
    local i = 1
    while i <= #data.letters do
        _currLetterX = _currLetterX + 1
        if _currLetterX >= data.lettersPerRow then
            _currLetterX = 0
            _currLetterY = _currLetterY + 1
        end
        data.lettersX[data.letters[i]] = _currLetterX
        data.lettersY[data.letters[i]] = _currLetterY
        i = i + 1
    end
    local font = {
        _data = data
    }
    setmetatable(font, {
        __index = rfs.font.classtable
    })
    rfs.font._fontcache[path] = font
    return font
end

function rfs.font.classtable.lettercount(self)
    return #(self._data.letters)
end

function rfs.font.classtable.splitlines(self, text)
    if type(text) ~= "string" then
        error("expected arg of type string")
    end
    local lines = {}
    local currentLine = text
    local i = 1
    while i < #currentLine do
        if currentLine:sub(i, i) == "\n" or
                currentLine:sub(i, i) == "\r" then
            table.insert(lines, currentLine:sub(1, i - 1))
            if currentLine:sub(i, i) == "\r" and
                    currentLine:sub(i + 1, i + 1) == "\n" then
                currentLine = currentLine:sub(i + 2)
            else
                currentLine = currentLine:sub(i + 1)
            end
            i = 1
        else
            i = i + 1
        end
    end
    table.insert(lines, currentLine)
    return lines
end

function rfs.font.classtable.calcwidth(self, pt_size, text)
    if type(text) ~= "string" then
        error("expected arg of type string")
    end
    pt_size = math.round(pt_size)
    if pt_size < 1 then
        return 0
    end
    local fac = (pt_size / self._data.letterHeight)
    if fac >= 1 then
        fac = math.round(fac)
    end
    local lines = self:splitlines(text)
    local width = 0
    for _, line in ipairs(lines) do
        width = math.max(
            width, math.max(1, math.round(self._data.letterWidth *
            fac)) *
            rfs.widechar.utf8_codepoint_count(text) +
            math.max(1, math.round(1.0 * fac)) *
            math.max(0, rfs.widechar.utf8_codepoint_count(text) - 1)
        )
    end
    return width
end

function rfs.font.classtable.textwrap(self, pt_size, text, width)
    if type(pt_size) ~= "number" then
        error("pt_size must be number, is " .. type(pt_size))
    end
    pt_size = math.round(pt_size)
    if pt_size < 1 or width == nil then
        return self:splitlines(text)
    end
    local fac = (pt_size / self._data.letterHeight)
    if fac >= 1 then
        fac = math.round(fac)
    end
    local lettersPerLine = 1
    local remainingW = width - math.max(
        1, math.round(self._data.letterWidth * fac)
    )
    lettersPerLine = lettersPerLine + math.max(0,
        math.floor(remainingW / (math.max(1,
            math.round(self._data.letterWidth * fac)
        ) + math.max(1, math.round(1.0 * fac)))))
    local lines = {}
    local currentlinestart = 1
    local lettersthisline = 0
    local lastspace = nil
    local i = 1
    while i <= #text do
        local len = rfs.widechar.utf8_char_len(
            text, i
        )
        local char = text:sub(i, i + len - 1)
        if (char == " " or char == "-" or char == "," or
                char == ":") and lettersthisline + 1 < lettersPerLine then
            lastspace = i
        elseif char == "\n" or char == "\r" then
            table.insert(lines,
                text:sub(currentlinestart, i - 1))
            if char == "\r" and
                    text:sub(i + 1, i + 1) == "\n" then
                i = i + 1
            end
            currentlinestart = i
            lettersthisline = -1
        end
        if lettersthisline + 1 > lettersPerLine then
            if lastspace ~= nil and lastspace > currentlinestart + 1 and (
                    lastspace < i or
                    text:sub(lastspace, lastspace) == " ") then
                if text:sub(lastspace, lastspace) ~= " " then
                    table.insert(lines,
                        text:sub(currentlinestart, lastspace))
                    i = lastspace + 1
                else
                    table.insert(lines,
                        text:sub(currentlinestart, lastspace - 1))
                    i = lastspace + 1
                end
                lastspace = nil
            else
                table.insert(lines,
                    text:sub(currentlinestart, i - 1))
            end
            currentlinestart = i
            lettersthisline = 0
        else
            i = i + len
            lettersthisline = lettersthisline + 1
        end
    end
    table.insert(lines, text:sub(currentlinestart))
    return lines
end

function rfs.font.classtable.calcheight(self, pt_size, text, width)
    local lines = (
        self:textwrap(pt_size, text, width)
    )
    local fac = (pt_size / self._data.letterHeight)
    if fac >= 1 then
        fac = math.round(fac)
    end
    local lineHeight = math.max(
        1, math.round(self._data.letterHeight * fac)
    )
    local lineSpacing = math.max(1, math.round(1 * fac))
    return #lines * lineHeight + (
        math.max(0, #lines - 1) * lineSpacing
    )
end

function rfs.font.classtable.draw(
        self, pt_size, text, x, y, width, r, g, b, a
        )
    x = math.round(x)
    y = math.round(y)
    if r == nil then r = 0 end
    if g == nil then g = 0 end
    if b == nil then b = 0 end
    if a == nil then a = 1 end
    local lines = {}
    if width ~= nil and width >= 0 then
        lines = self:textwrap(pt_size, text, width)
    else
        lines = self:splitlines(text)
    end
    pt_size = math.round(pt_size)
    local fac = (pt_size / self._data.letterHeight)
    if fac >= 1 then
        fac = math.round(fac)
    end
    local origx = x
    local currentx = origx
    local currenty = y
    local letterW = self._data.letterWidth
    local letterH = self._data.letterHeight
    for _, line in ipairs(lines) do
        currentx = origx
        local i = 1
        while i <= #line do
            local len = rfs.widechar.utf8_char_len(
                line, i
            )
            local char = line:sub(i, i + len - 1)
            if char ~= " " and self._data.lettersX[char] == nil and
                    self._data.lettersX["?"] ~= nil then
                char = "?"
            end
            if self._data.lettersX[char] ~= nil then
                local scissorX = self._data.lettersX[char] * letterW
                local scissorY = self._data.lettersY[char] * letterH
                rfs.gfx.draw_tex(
                    self._data.image, currentx, currenty, fac, fac,
                    r, g, b, a, scissorX, scissorY, letterW, letterH
                )
            end
            i = i + len
            currentx = currentx + math.max(1, math.round(letterW * fac))
            currentx = currentx + math.max(1, math.round(1.0 * fac))
        end
        currenty = currenty + math.max(1, math.round(letterH * fac)) +
            math.max(1, math.round(1 * fac))
    end
end
