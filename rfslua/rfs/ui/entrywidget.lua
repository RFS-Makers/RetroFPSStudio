-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs.ui == nil then rfs.ui = {} end
if rfs.ui.entrywidget == nil then
    rfs.ui.entrywidget = {}
    rfs.ui.entrywidget.classtable = {}
end

function rfs.ui.entrywidget.new(font, pt_size, width, multiline)
    if type(font) ~= "userdata" or
            type(font.calcheight) ~= "function" then
        error("expected arg #1 to be RFS font")
    end
    if type(pt_size) ~= "number" then
        error("expected arg #2 to be number")
    end
    if type(width) ~= "number" then
        error("expected arg #3 to be number")
    end
    local self = {
        font=font,
        focused=false,
        pt_size=math.max(1, math.round(pt_size)),
        border_size=math.max(1, math.round(pt_size * 0.3)),
        width=width,
        multiline=(multiline == true),
        text="",
    }
    if self.multiline == true then
        self.displaylines = 3
    else
        self.displaylines = 1
    end
    setmetatable(self, {
        __index = rfs.ui.entrywidget.classtable
    })
    self:update_size()
    return self
end

function rfs.ui.entrywidget.classtable.on_text(self, t)
    if not self.multiline then
        t = t:gsub("\r", "")
        t = t:gsub("\n", "")
    end
    self.text = self.text .. t
end

function rfs.ui.entrywidget.classtable.on_keydown(self, k)
    if k == "backspace" then
        self.text = self.text:sub(1, #self.text - 1)
        return true
    elseif k == "v" then
        if rfs.events.ctrl_pressed() then
            self:on_text(os.clipboard_paste())
            return true
        end
    elseif k == "w" or k == "s" or k == "a" or k == "d" or
            k == "e" then
        -- Make sure these aren't treated up the chain,
        -- since we need them for text input:
        return true
    end
end


function rfs.ui.entrywidget.classtable.set_focus(self)
    self.focused = true
    rfs.events.start_text_input()
end

function rfs.ui.entrywidget.classtable.unset_focus(self)
    self.focused = false
    rfs.events.stop_text_input()
end

function rfs.ui.entrywidget.classtable.set_width(self, w)
    self.width = w
end

function rfs.ui.entrywidget.classtable.set_pt(self, pt_size)
    self.pt_size = math.max(1, math.round(pt_size))
    self:update_size()
end

function rfs.ui.entrywidget.classtable.update_size(self)
    self.border_size = math.max(1, math.round(self.pt_size * 0.3))
    local t = "Hello"
    local i = 1
    while i < self.displaylines do
        t = t .. "\nHello"
        i = i + 1
    end
    local lineheight = self.font:calcheight(
        t, nil, self.pt_size
    )
    self.height = lineheight + self.border_size * 2
end

function rfs.ui.entrywidget.classtable.draw(self, x, y)
    local fgr = 0.7
    local fgg = 0.7
    local fgb = 0.7
    local bgr = 0.3
    local bgg = 0.3
    local bgb = 0.3
    if self.focused then
        bgr = 0
        bgg = 0
        bgb = 0
        fgr = 1
        fgg = 1
        fgb = 1
    end
    rfs.gfx.draw_rect(
        x, y, self.width, self.height,
        bgr, bgg, bgb, 1
    )
    local xtextoffset = 0
    local wlimit = nil
    if self.multiline then
        wlimit = self.width
    else
        local draw_w = self.font:calcwidth(
            self.text .. "■", self.pt_size
        )
        xtextoffset = math.min(0, -(draw_w - self.width))
    end
    rfs.gfx.push_scissors(
        x, y, self.width, self.height
    )
    self.font:draw(
        self.text .. "■", nil,
        x + self.border_size + xtextoffset,
        y + self.border_size,
        fgr, fgg, fgb, 1, self.pt_size
    )
    rfs.gfx.pop_scissors()
end
