-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.ui == nil then rfs.ui = {} end
if rfs.ui.menuwidget == nil then
    rfs.ui.menuwidget = {}
    rfs.ui.menuwidget.classtable = {}
end

function rfs.ui.menuwidget.new(font, pt_size, width, centered)
    if font == nil then
        font = rfs.font.load(rfs.ui.default_font)
    end
    if type(font) ~= "table" or
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
        entries={},
        pt_size=math.max(1, math.round(pt_size)),
        border_size=math.max(1, math.round(pt_size * 1.5)),
        width=width,
        centered=(centered == true),
    }
    setmetatable(self, {
        __index = rfs.ui.menuwidget.classtable
    })
    self:update_size()
    return self
end

function rfs.ui.menuwidget.classtable.set_centered(
        self, centered
        )
    self.centered = (centered == true)
    self:update_size()
end

function rfs.ui.menuwidget.classtable.add_entry(
        self, text, icon, red, green, blue
        )
    if (type(text) ~= "string" and text ~= nil) or
            (type(icon) ~= "string" and icon ~= nil) then
        error("expected text and icon args to " ..
            "be string if specified")
    end
    if (type(icon) == "string" and
             not rfs.vfs.exists(icon) and
             not rfs.vfs.exists(icon .. ".png") and
             not rfs.vfs.exists(icon .. ".PNG") and
             not rfs.vfs.exists(icon .. ".JPG") and
             not rfs.vfs.exists(icon .. ".jpg")) then
        error("expected icon to refer to existing image")
    end
    if red ~= nil and type(red) ~= "number" then
        error("expected red color arg to be number, " ..
            "or unspecified")
    end
    if green ~= nil and type(green) ~= "number" then
        error("expected green color arg to be number, " ..
            "or unspecified")
    end
    if blue ~= nil and type(blue) ~= "number" then
        error("expected blue color arg to be number, " ..
            "or unspecified")
    end
    if text == nil and icon == nil then
        error("entry must have either text or icon")
    end
    if red == nil then red = 1.0 end
    if green == nil then green = 1.0 end
    if blue == nil then blue = 1.0 end
    local scale = 1.0
    if text == nil then
        scale = 2.0
    end
    table.insert(self.entries, {
        text=text, icon=icon, scale=scale,
        red=red, green=green, blue=blue
    })
    self:update_size()
end

function rfs.ui.menuwidget.classtable.on_text(self, t)
end

function rfs.ui.menuwidget.classtable.on_keydown(self, k)
end


function rfs.ui.menuwidget.classtable.set_focus(self)
    self.focused = true
end

function rfs.ui.menuwidget.classtable.unset_focus(self)
    self.focused = false
end

function rfs.ui.menuwidget.classtable.set_width(self, w)
    self.width = w
    self:update_size()
end

function rfs.ui.menuwidget.classtable.set_pt(self, pt_size)
    self.pt_size = math.max(1, math.round(pt_size))
    self:update_size()
end

function rfs.ui.menuwidget.classtable.calculate_entry_size(
        self, no
        )
    if type(no) ~= "number" or
            no < 1 or no > #self.entries then
        error("invalid entry number")
    end
    local border_size = (
        math.max(1, math.round(self.border_size))
    )
    local line_height = self.font:calcheight(
        math.max(1, math.round(
            self.pt_size * self.entries[no].scale
        )), "X", self.width
    )
    local icon_width = 0
    local icon_height = 0
    if self.entries[no].icon ~= nil then
        icon_height = math.max(1, math.round(
            line_height * self.entries[no].scale))
        local tex = rfs.gfx.get_tex(self.entries[no].icon)
        local w, h = rfs.gfx.get_tex_size(tex)
        icon_width = math.max(1, math.round(
            icon_height * (w / h)
        ))
        local forced_scale = 1
        if icon_width > self.width and icon_width > 0 then
            forced_scale = (self.width / icon_width)
            icon_width = math.max(1, math.round(
                icon_width * forced_scale
            ))
            icon_height = math.max(1, math.round(
                icon_height * forced_scale
            ))
        end
    end
    local icon_with_border_width = 0
    if icon_width > 0 then
        icon_with_border_width = icon_width
        if self.entries[no].text ~= nil then
            icon_with_border_width = (
                icon_with_border_width + border_size
            )
        end
    end
    local text_height = 0
    local text_width = 0
    if self.entries[no].text ~= nil then
        text_width = self.font:calcwidth(
            math.max(1, math.round(self.pt_size *
                self.entries[no].scale)),
            self.entries[no].text)
        local text_max_width = math.max(1, self.width -
            icon_with_border_width)
        text_width = math.min(text_width, text_max_width)
        text_height = self.font:calcheight(
            math.max(1, math.round(self.pt_size *
                self.entries[no].scale)),
            self.entries[no].text, text_width)
    end
    self.entries[no].icon_size = {
        icon_width, icon_height
    }
    self.entries[no].text_size = {
        text_width, text_height
    }
    self.entries[no].text_x = icon_with_border_width
    self.entries[no].width = (icon_with_border_width +
        text_width)
    self.entries[no].height = math.max(
        icon_height, text_height
    )
    return self.entries[no].width, self.entries[no].height
end

function rfs.ui.menuwidget.classtable.update_size(self)
    -- Update border size:
    self.border_size=math.max(
        1, math.round(self.pt_size * 1.5)
    )
    local border_size = (
        math.max(1, math.round(self.border_size))
    )

    local height = 0
    local i = 1
    while i <= #self.entries do
        if i > 1 then
            height = height + border_size
        end
        local w, h = self:calculate_entry_size(i)
        self.entries[i].x = 0
        if self.centered and
                self.entries[i].width < self.width then
            self.entries[i].x = math.max(0, math.round(
                (self.width - self.entries[i].width) / 2
            ))
        end
        self.entries[i].y = height
        height = height + h
        i = i + 1
    end
    self.height = height
end

function rfs.ui.menuwidget.classtable.draw(self, x, y)
    local i = 1
    while i <= #self.entries do
        if self.entries[i].text ~= nil then
            self.font:draw(
                math.max(1, math.round(self.pt_size *
                    self.entries[i].scale)),
                self.entries[i].text .. "",
                x + self.entries[i].text_x +
                    self.entries[i].x,
                y + self.entries[i].y,
                nil,
                self.entries[i].red, self.entries[i].green,
                self.entries[i].blue, 1
            )
        end
        if self.entries[i].icon ~= nil then
            local tex = rfs.gfx.get_tex(
                self.entries[i].icon
            )
            local icon_size = self.entries[i].icon_size
            local w, h = rfs.gfx.get_tex_size(tex)
            rfs.gfx.draw_tex(tex,
                x + self.entries[i].x,
                y + self.entries[i].y,
                icon_size[1] / w, icon_size[2] / h)
        end
        i = i + 1
    end
end
