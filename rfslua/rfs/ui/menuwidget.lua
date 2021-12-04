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
    if type(font.calcheight) ~= "function" then
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
        selected=-1,
        entries={},
        blink_ts=rfs.time.ticks(),
        custom_height=false,
        outline_size=2,
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


function rfs.ui.menuwidget.classtable.outline_px(self, no)
    assert(self.entries[no] ~= nil)
    if self.entries[no].text ~= nil and
            #self.entries[no].text > 0 then
        return self.font:calcoutlinepx(
            math.max(1, math.round(
                self.pt_size * self.entries[no].scale
            )), self.outline_size)
    else
        return self.font:calcoutlinepx(
            math.max(1, math.round(
                self.pt_size * 1.0
            )), self.outline_size)
    end
end


function rfs.ui.menuwidget.classtable.set_centered(
        self, centered
        )
    self.centered = (centered == true)
    self:update_size()
end


function rfs.ui.menuwidget.classtable.disable_entry(self, no)
    if no < 1 or no > #self.entries then
        return
    end
    self.entries[no].disabled = true
    if self.selected == no then
        self.selected = self.selected + 1
        while self.selected ~= no do
            if self.selected > #self.entries then
                self.selected = 1
            elseif not self.entries[self.selected].disabled then
                break
            else
                self.selected = self.selected + 1
            end
        end
        if self.selected == no then
            self.selected = -1
        end
    end
end


function rfs.ui.menuwidget.classtable.add_entry(
        self, text, icon, func, red, green, blue,
        focus_red, focus_green, focus_blue
        )
    if (type(text) ~= "string" and text ~= nil) or
            (type(icon) ~= "string" and icon ~= nil) then
        error("expected text and icon args to " ..
            "be string if specified")
    end
    if type(func) ~= "function" and func ~= nil then
        error("expected func arg to be function if specified")
    end
    if (type(icon) == "string" and
             not rfs.vfs.exists(icon) and
             not rfs.vfs.exists(icon .. ".png") and
             not rfs.vfs.exists(icon .. ".PNG") and
             not rfs.vfs.exists(icon .. ".JPG") and
             not rfs.vfs.exists(icon .. ".jpg")) then
        error("expected icon to refer to existing image")
    end
    if text == nil and icon == nil then
        error("entry must have either text or icon")
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
    if red == nil then red = 1.0 end
    if green == nil then green = 1.0 end
    if blue == nil then blue = 1.0 end
    if focus_red ~= nil and type(focus_red) ~= "number" then
        error("expected focus_red color arg to be number, " ..
            "or unspecified")
    end
    if focus_green ~= nil and type(focus_green) ~= "number" then
        error("expected focus_green color arg to be number, " ..
            "or unspecified")
    end
    if focus_blue ~= nil and type(focus_blue) ~= "number" then
        error("expected focus_blue color arg to be number, " ..
            "or unspecified")
    end
    if focus_red == nil then focus_red = 1.0 end
    if focus_green == nil then focus_green = 0.0 end
    if focus_blue == nil then focus_blue = 1.0 end

    local scale = 1.0
    if text == nil then
        scale = 2.0
    end
    table.insert(self.entries, {
        text=text, icon=icon, scale=scale,
        red=red, green=green, blue=blue,
        focus_red=focus_red,
        focus_green=focus_green,
        focus_blue=focus_blue,
        func=func
    })
    if self.selected < 0 then
        self.selected = #self.entries
    end
    self:update_size()
end


function rfs.ui.menuwidget.classtable.on_text(self, t)
end


function rfs.ui.menuwidget.classtable.on_keydown(self, k)
    if not self.focused then
        return
    end
    if (k == "down" or k == "s" or k == "right") and
            #self.entries ~= 0 then
        local selected_next = math.max(1, self.selected + 1)
        while true do
            if selected_next > #self.entries then
                selected_next = 1
            elseif selected_next >= 1 and selected_next <=
                    #self.entries and
                    self.entries[selected_next].disabled then
                selected_next = selected_next + 1
            else
                break
            end
        end
        if self.selected ~= selected_next then
            rfs.ui.playsound(rfs.ui.focussound)
            self.blink_ts = rfs.time.ticks()
            self.selected = selected_next
        end
        return true
    elseif (k == "up" or k == "w" or k == "left") and
            #self.entries ~= 0 then
        local selected_next = self.selected - 1
        while true do
            if selected_next < 1 then
                selected_next = #self.entries
            elseif selected_next >= 1 and selected_next <=
                    #self.entries and
                    self.entries[selected_next].disabled then
                selected_next = selected_next - 1
            else
                break
            end
        end
        if self.selected ~= selected_next then
            rfs.ui.playsound(rfs.ui.focussound)
            self.blink_ts = rfs.time.ticks()
            self.selected = selected_next
        end
        return true
    elseif (k == "return" or k == "e") and
            #self.entries > 0 and
            self.selected >= 1 and
            self.selected <= #self.entries and
            not self.entries[self.selected].disabled then
        rfs.ui.playsound(rfs.ui.oksound)
        if self.entries[self.selected].func ~= nil then
            self.entries[self.selected].func()
        end
        return true
    end
end


function rfs.ui.menuwidget.classtable.on_click(
        self, x, y, button)
    local i = 1
    while i <= #self.entries do
        if x >= self.entries[i].x and
                x < self.entries[i].x +
                self.entries[i].width and
                y >= self.entries[i].y and
                y < self.entries[i].y +
                self.entries[i].height and
                not self.entries[i].disabled then
            self.selected = i
            rfs.ui.playsound(rfs.ui.oksound)
            self.blink_ts = rfs.time.ticks()
            if self.entries[i].func ~= nil then
                self.entries[i].func()
            end
            return true
        end
        i = i + 1
    end
end


function rfs.ui.menuwidget.classtable.on_mousemove(self, x, y)
    local i = 1
    while i <= #self.entries do
        if x >= self.entries[i].x and
                x < self.entries[i].x +
                self.entries[i].width and
                y >= self.entries[i].y and
                y < self.entries[i].y +
                self.entries[i].height and
                not self.entries[i].disabled then
            if self.selected ~= i then
                rfs.ui.playsound(rfs.ui.focussound)
                self.blink_ts = rfs.time.ticks()
                self.selected = i
            end
            return true
        end
        i = i + 1
    end
end


function rfs.ui.menuwidget.classtable.set_focus(self)
    self.focused = true
end


function rfs.ui.menuwidget.classtable.unset_focus(self)
    self.focused = false
end


function rfs.ui.menuwidget.classtable.set_width(self, w)
    if type(w) ~= "number" then
        error("width must be number")
    end
    self.width = math.max(0, w)
    self:update_size()
end


function rfs.ui.menuwidget.classtable.set_height(self, h)
    if type(h) ~= "number" and h ~= nil then
        error("height must be number or nil")
    end
    if h == nil then
        self.custom_height = false
        self.height = self.natural_height
    else
        self.height = math.max(0, h)
        self.custom_height = true
    end
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
        "X", self.width,
        math.max(1, math.round(
            self.pt_size * self.entries[no].scale
        )), 1
    )
    local icon_outline = 0
    local icon_width = 0
    local icon_height = 0
    if self.entries[no].icon ~= nil then
        icon_outline = self:outline_px(no)
        icon_height = math.max(1, math.round(
            line_height * self.entries[no].scale))
        local tex = rfs.gfx.get_tex(self.entries[no].icon)
        local w, h = rfs.gfx.get_tex_size(tex)
        icon_width = math.max(1, math.round(
            icon_height * (w / h)
        ))
        local forced_scale = 1
        if icon_width + icon_outline * 2 > self.width and
                icon_width > 0 then
            forced_scale = (self.width /
                math.max(0.1, icon_width +
                icon_outline * 2))
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
        icon_with_border_width = (
            icon_width + icon_outline * 2
        )
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
            self.entries[no].text,
            math.max(1, math.round(self.pt_size *
                self.entries[no].scale)),
            1, self.outline_size)
        local text_max_width = math.max(1, self.width -
            icon_with_border_width)
        text_width = math.min(text_width, text_max_width)
        text_height = self.font:calcheight(
            self.entries[no].text, text_width,
            math.max(1, math.round(self.pt_size *
                self.entries[no].scale)),
            1, self.outline_size)
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
    self.natural_height = height
    if not self.custom_height then
        self.height = self.natural_height
    end
end


function rfs.ui.menuwidget.classtable.draw(self, x, y)
    local now = rfs.time.ticks()
    local animf = (now - self.blink_ts) % 1200
    animf = math.round(animf / 1200)

    rfs.gfx.push_scissors(
        x, y, self.width, self.height
    )

    local i = 1
    while i <= #self.entries do
        local draw_selected = (
            i == self.selected and animf == 0 and
            self.focused
        )
        if self.entries[i].text ~= nil then
            local tr = self.entries[i].red
            local tg = self.entries[i].green
            local tb = self.entries[i].blue
            local tlwidth = 1
            if draw_selected then
                tr = self.entries[i].focus_red
                tg = self.entries[i].focus_green
                tb = self.entries[i].focus_blue
                tlwidth = 0
            end
            self.font:draw(
                self.entries[i].text .. "", self.width,
                x + self.entries[i].text_x +
                    self.entries[i].x,
                y + self.entries[i].y,
                tr, tg, tb, 1,
                math.max(1, math.round(self.pt_size *
                    self.entries[i].scale)),
                tlwidth, self.outline_size
            )
        end
        if self.entries[i].icon ~= nil then
            local icon_outline = self:outline_px(i)
            local tex = rfs.gfx.get_tex(
                self.entries[i].icon
            )
            local icon_size = self.entries[i].icon_size
            local w, h = rfs.gfx.get_tex_size(tex)
            local olx = 0
            local olw = 1.0
            local olr = 0
            local olg = 0
            local olb = 0
            if draw_selected then
                olr = self.entries[i].focus_red
                olg = self.entries[i].focus_green
                olb = self.entries[i].focus_blue
                olw = 0.9
                olx = ((icon_size[1] + icon_outline * 2) -
                    ((icon_size[1] * olw) + icon_outline * 2)) / 2
            end
            rfs.gfx.draw_rect(
                x + self.entries[i].x + olx,
                y + self.entries[i].y,
                (icon_size[1] * olw) + icon_outline * 2,
                icon_size[2] + icon_outline * 2,
                olr, olg, olb)
            rfs.gfx.draw_tex(tex,
                x + self.entries[i].x + olx + icon_outline,
                y + self.entries[i].y + icon_outline,
                (icon_size[1] * olw) / w, icon_size[2] / h)
        end
        i = i + 1
    end

    rfs.gfx.pop_scissors()
end
