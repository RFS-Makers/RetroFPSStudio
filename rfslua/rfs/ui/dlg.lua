-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.ui == nil then rfs.ui = {} end
if rfs.ui.dlg == nil then
    rfs.ui.dlg = {}
    rfs.ui.dlg.bordersize = 10
    rfs.ui.dlg.bordercolor = {0.1, 0.1, 0.1}
    rfs.ui.dlg.focuscolor = {0.7, 0.0, 0.7}
    rfs.ui.dlg.textcolor = {0, 0, 0}
    rfs.ui.dlg.bgcolor = {1.0, 0.9, 1.0}
end


function rfs.ui.dlg.on_mousemove(x, y)
    if not rfs.ui.dlg.isactive() then
        return false
    end
    rfs.ui.dlg.update_sizes()
    if x < rfs.ui.dlg.dlgx or y < rfs.ui.dlg.dlgy or
            x > rfs.ui.dlg.dlgx + rfs.ui.dlg.dlgw or
            y > rfs.ui.dlg.dlgy + rfs.ui.dlg.dlgh then
        return false
    end
    x = x - rfs.ui.dlg.dlgx
    y = y - rfs.ui.dlg.dlgy
    local buttonidx = 0
    for _, button in ipairs(rfs.ui.dlg.buttons) do
        buttonidx = buttonidx + 1
        if x > button.x and x < button.x + button.width and
                y > button.y and
                y < button.y + button.height then
            if buttonidx ~= rfs.ui.dlg.focus_index then
                rfs.ui.dlg.focus_index = buttonidx
                rfs.ui.playsound(rfs.ui.focussound)
                rfs.ui.dlg.__focus_ts = rfs.time.ticks()
                rfs.ui.dlg.__last_draw_scale = nil  -- redraw.
            end
            return true
        end
    end
    return true
end

function rfs.ui.dlg.on_keydown(t)
    if type(t) ~= "string" then
        error("expected arg of type string")
    end
    if not rfs.ui.dlg.isactive() then
        return false
    end
    if rfs.ui.dlg.widget ~= nil and
            rfs.ui.dlg.widget.on_keydown ~= nil then
        if rfs.ui.dlg.widget:on_keydown(t) == true then
            rfs.ui.dlg.__last_draw_scale = nil  -- redraw.
            return true
        end
    end
    if (t == "up" or t == "left" or t == "w" or t == "a") and
            #rfs.ui.dlg.buttons > 1 then
        rfs.ui.dlg.focus_index = rfs.ui.dlg.focus_index - 1
        if rfs.ui.dlg.focus_index < 1 then
            rfs.ui.dlg.focus_index = #rfs.ui.dlg.buttons
        end
        rfs.ui.playsound(rfs.ui.focussound)
        rfs.ui.dlg.__focus_ts = rfs.time.ticks()
        rfs.ui.dlg.__last_draw_scale = nil  -- redraw.
        return true
    elseif (t == "down" or t == "right" or t == "s" or t == "d") and
            #rfs.ui.dlg.buttons > 1 then
        rfs.ui.dlg.focus_index = rfs.ui.dlg.focus_index - 1
        if rfs.ui.dlg.focus_index < 1 then
            rfs.ui.dlg.focus_index = #rfs.ui.dlg.buttons
        end
        rfs.ui.playsound(rfs.ui.focussound)
        rfs.ui.dlg.__focus_ts = rfs.time.ticks()
        rfs.ui.dlg.__last_draw_scale = nil  -- redraw.
        return true
    elseif (t == "return" or t == "e") and
            rfs.ui.dlg.focus_index >= 1 and
            rfs.ui.dlg.focus_index <= #rfs.ui.dlg.buttons then
        local button = rfs.ui.dlg.buttons[
            rfs.ui.dlg.focus_index]
        rfs.ui.playsound(rfs.ui.oksound)
        if type(button.func) == "function" then
            button.func(rfs.ui.dlg.get_result())
        end
        return true
    end
    return true
end

function rfs.ui.dlg.on_keyup(t)
    if type(t) ~= "string" then
        error("expected arg of type string")
    end
    if not rfs.ui.dlg.isactive() then
        return false
    end
    return true
end

function rfs.ui.dlg.get_result()
    if rfs.ui.dlg.widget ~= nil and
            rfs.ui.dlg.widget_info.type == "entry" then
        return rfs.ui.dlg.widget.text
    end
    return nil
end

function rfs.ui.dlg.on_text(t)
    if type(t) ~= "string" then
        error("expected arg of type string")
    end
    if not rfs.ui.dlg.isactive() then
        return false
    end
    if rfs.ui.dlg.widget ~= nil and
            rfs.ui.dlg.widget.on_text ~= nil then
        rfs.ui.dlg.widget:on_text(t)
        rfs.ui.dlg.__last_draw_scale = nil  -- redraw.
    end
    return true
end

function rfs.ui.dlg.close()
    if not rfs.ui.dlg.isactive() then
        return
    end
    rfs.ui.dlg.__active = false
    if rfs.ui.dlg.widget ~= nil and
            rfs.ui.dlg.widget.unset_focus ~= nil then
        rfs.ui.dlg.widget:unset_focus()
    end
    rfs.ui.dlg.widget = nil
end

function rfs.ui.dlg.on_click(x, y, button)
    if not rfs.ui.dlg.isactive() then
        return false
    end
    rfs.ui.dlg.update_sizes()
    if x < rfs.ui.dlg.dlgx or y < rfs.ui.dlg.dlgy or
            x > rfs.ui.dlg.dlgx + rfs.ui.dlg.dlgw or
            y > rfs.ui.dlg.dlgy + rfs.ui.dlg.dlgh then
        return false
    end
    x = x - rfs.ui.dlg.dlgx
    y = y - rfs.ui.dlg.dlgy
    if button == "left" then
        for _, button in ipairs(rfs.ui.dlg.buttons) do
            if x > button.x and x < button.x + button.width and
                    y > button.y and
                    y < button.y + button.height then
                rfs.ui.playsound(rfs.ui.oksound)
                if type(button.func) == "function" then
                    button.func(rfs.ui.dlg.get_result())
                end
            end
        end
    end
    return true
end

function rfs.ui.dlg.isactive()
    return (rfs.ui.dlg.__active == true)
end

function rfs.ui.dlg.update_widget_size()
    if rfs.ui.dlg.widget == nil then
        rfs.ui.dlg.widgeth = 0
        rfs.ui.dlg.widgetw = 0
        return
    end
    local effectiveborder = rfs.ui.dlg.effectiveborder
    rfs.ui.dlg.widgety = (
        effectiveborder * 1.5 + rfs.ui.dlg.text_height +
        effectiveborder
    )
    rfs.ui.dlg.widgetx = effectiveborder * 1.5
    if rfs.ui.dlg.__icon ~= nil then
        rfs.ui.dlg.widgetx = (rfs.ui.dlg.widgetx +
            rfs.ui.dlg.iconw + effectiveborder)
    end
    rfs.ui.dlg.widgetw = rfs.ui.dlg.content_width
    local widget = rfs.ui.dlg.widget
    if widget.set_pt ~= nil then
        widget:set_pt(rfs.ui.dlg.__fontpt * rfs.ui.scaler)
    end
    widget:set_width(rfs.ui.dlg.widgetw)
    rfs.ui.dlg.widgeth = rfs.ui.dlg.widget.height
end

function rfs.ui.dlg.update_sizes()
    local animf = math.min(1.0, math.max(0.0, (
        rfs.time.ticks() - rfs.ui.dlg.__launch_ts
    ) / 300.0))
    local effectiveborder = math.max(1,
        math.round(rfs.ui.scaler * rfs.ui.dlg.bordersize)
    )
    rfs.ui.dlg.effectiveborder = effectiveborder
    rfs.ui.dlg.widgeth = 0
    rfs.ui.dlg.widgetw = 0
    rfs.ui.dlg.dlgw = rfs.window.renderw * 0.95
    rfs.ui.dlg.dlgx = rfs.window.renderw * 0.025
    if rfs.ui.dlg.__icon ~= nil then
        rfs.ui.dlg.iconw = math.round(rfs.window.renderw * 0.2)
        local iw, ih = rfs.gfx.get_tex_size(rfs.ui.dlg.__icon)
        if iw > ih then
            rfs.ui.dlg.iconw = math.round(rfs.window.renderw * 0.2)
            rfs.ui.dlg.iconh = math.round(rfs.ui.dlg.iconw * (
                ih / iw
            ))
        else
            rfs.ui.dlg.iconh = math.round(rfs.window.renderw * 0.2)
            rfs.ui.dlg.iconw = math.round(rfs.ui.dlg.iconh * (
                iw / ih
            ))
        end
        rfs.ui.dlg.iconscale = rfs.ui.dlg.iconw / iw
    end
    rfs.ui.dlg.update_text_height()
    rfs.ui.dlg.update_buttons_size()
    rfs.ui.dlg.update_widget_size()
    rfs.ui.dlg.dlgh = (
        effectiveborder * 3 + rfs.ui.dlg.text_height
    )
    if #rfs.ui.dlg.buttons > 0 then
        rfs.ui.dlg.dlgh = (
            rfs.ui.dlg.dlgh +
            effectiveborder * 2 +
            rfs.ui.dlg.buttons_area_height
        )
    end
    if rfs.ui.dlg.widgeth > 0 then
        rfs.ui.dlg.dlgh = (
            rfs.ui.dlg.dlgh +
            effectiveborder + rfs.ui.dlg.widgeth)
        for _, button in ipairs(rfs.ui.dlg.buttons) do
            button.y = (button.y +
                effectiveborder + rfs.ui.dlg.widgeth)
        end
    end 
    if rfs.ui.dlg.dlgh - effectiveborder * 3 <
            rfs.ui.dlg.iconh then
        local iconstuffing = (
            rfs.ui.dlg.iconh - (rfs.ui.dlg.dlgh - effectiveborder * 3)
        )
        rfs.ui.dlg.dlgh = rfs.ui.dlg.dlgh + iconstuffing
    end
    rfs.ui.dlg.dlgy = (
        rfs.window.renderh * 0.5 - rfs.ui.dlg.dlgh * 0.5
    ) + (1.0 - animf) * rfs.window.renderh * 0.2
end

function rfs.ui.dlg.update_text_height()
    rfs.ui.dlg.text_width = math.max(1,
        rfs.window.renderw * 0.95 - rfs.ui.dlg.effectiveborder * 3
    )
    rfs.ui.dlg.content_width = rfs.ui.dlg.text_width
    if rfs.ui.dlg.__icon ~= nil then
        rfs.ui.dlg.text_width = math.max(1,
            rfs.ui.dlg.text_width - rfs.ui.dlg.iconw -
            rfs.ui.dlg.effectiveborder)
    end
    rfs.ui.dlg.font = rfs.font.load(rfs.ui.dlg.__fontname)
    rfs.ui.dlg.text_height = rfs.ui.dlg.font:calcheight(
        rfs.ui.dlg.text, rfs.ui.dlg.text_width,
        rfs.ui.dlg.__fontpt * rfs.ui.scaler
    )
    return rfs.ui.dlg.text_height
end

function rfs.ui.dlg.update_buttons_size()
    local buttonborder = math.max(1,
        math.round(rfs.ui.dlg.effectiveborder * 1.0)
    )
    rfs.ui.dlg.button_border_width = buttonborder
    if #rfs.ui.dlg.buttons == 0 then
        rfs.ui.dlg.buttons_area_width = 0
        rfs.ui.dlg.buttons_area_height = 0
        rfs.ui.dlg.buttons_vertical = false
        return
    end
    local verti_arrangement_height = 0
    local hori_arrangement_width = 0
    local firstbutton = true
    for _, button in ipairs(rfs.ui.dlg.buttons) do
        local t = tostring(button.text)
        local tw = rfs.ui.dlg.font:calcwidth(
            t, rfs.ui.dlg.__fontpt * rfs.ui.scaler
        )
        local th = rfs.ui.dlg.font:calcheight(
            t, tw, rfs.ui.dlg.__fontpt * rfs.ui.scaler
        )
        button.text_width = tw
        button.text_height = th
        button.width = buttonborder * 2 + tw
        button.height = buttonborder * 2 + th
        if firstbutton then
            firstbutton = false
        else
            hori_arrangement_width = (
                hori_arrangement_width + buttonborder * 2
            )
            verti_arrangement_height = (
                verti_arrangement_height + buttonborder * 2
            )
        end
        hori_arrangement_width = (
            hori_arrangement_width + button.width
        )
        verti_arrangement_height = (
            verti_arrangement_height + button.height
        )
    end
    if hori_arrangement_width > rfs.ui.dlg.text_width then
        rfs.ui.dlg.buttons_vertical = true
        rfs.ui.dlg.buttons_area_width = (
            0
        )
        for _, button in ipairs(rfs.ui.dlg.buttons) do
            rfs.ui.dlg.buttons_area_width = math.max(
                rfs.ui.dlg.buttons_area_width, button.width
            )
        end
        rfs.ui.dlg.buttons_area_height = (
            verti_arrangement_height
        )
    else
        rfs.ui.dlg.buttons_vertical = false
        rfs.ui.dlg.buttons_area_width = (
            hori_arrangement_width
        )
        rfs.ui.dlg.buttons_area_height = (
            rfs.ui.dlg.buttons[1].height
        )
    end
    local effectiveborder = rfs.ui.dlg.effectiveborder
    local by = (effectiveborder * 1.5 +
        rfs.ui.dlg.text_height + effectiveborder * 2)
    local bx = (rfs.ui.dlg.dlgw * 0.5 -
        rfs.ui.dlg.buttons_area_width * 0.5)
    if rfs.ui.dlg.__icon ~= nil then
        bx = bx + (rfs.ui.dlg.iconw + effectiveborder) * 0.5
    end
    if not rfs.ui.dlg.buttons_vertical then
        for _, button in ipairs(rfs.ui.dlg.buttons) do
            button.x = bx
            button.y = by
            bx = bx + button.width
            bx = bx + rfs.ui.dlg.button_border_width * 2
        end
    else
        for _, button in ipairs(rfs.ui.dlg.buttons) do
            button.x = bx
            button.y = by
            by = by + button.height
            by = by + rfs.ui.dlg.button_border_width * 2
        end
    end
end

function rfs.ui.dlg.show(text, buttons, widget, icon)
    if type(buttons) ~= "table" then
        if type(buttons) ~= "nil" then
            error("buttons must be a table or nil")
        end
        buttons = {}
    end
    if type(widget) ~= "table" then
        if type(widget) ~= "nil" then
            error("widget must be a table or nil")
        end
        widget = {}
    end

    -- First, clear out previous dialog:
    rfs.ui.dlg.close()

    -- Set settings and widget:
    rfs.ui.dlg.__fontname = rfs.ui.default_font
    if rfs.ui.dlg.font == nil then
        rfs.ui.dlg.font = (
            rfs.font.load(rfs.ui.dlg.__fontname))
    end
    rfs.ui.dlg.widget = nil
    rfs.ui.dlg.widget_info = widget
    if widget.type == "entry" then
        rfs.ui.dlg.widget = rfs.ui.entrywidget.new(
            rfs.ui.dlg.font, rfs.ui.dlg.__fontpt,
            64, widget.multiline == true)
        if rfs.ui.dlg.widget.set_focus ~= nil then
            rfs.ui.dlg.widget:set_focus()
        end
    else
        rfs.ui.dlg.widget_info = nil
        widget = nil  -- not a known type
    end
    if rfs.ui.dlg.widget ~= nil and
            rfs.ui.dlg.widget.set_focus ~= nil then
        rfs.ui.dlg.widget:set_focus()
    end
    if type(icon) == "string" then
        rfs.ui.dlg.__icon = rfs.gfx.get_tex(icon)
        rfs.ui.dlg.iconw = 16
        rfs.ui.dlg.iconh = 16
    else
        rfs.ui.dlg.__icon = nil
        rfs.ui.dlg.iconw = 0
        rfs.ui.dlg.iconh = 0
    end
    rfs.ui.dlg.__active = true
    rfs.ui.dlg.__launch_ts = rfs.time.ticks()
    rfs.ui.dlg.__focus_ts = rfs.time.ticks()
    rfs.ui.dlg.__fontname = rfs.ui.default_font
    rfs.ui.dlg.focus_index = 1
    rfs.ui.dlg.__fontpt = 14
    rfs.ui.dlg.__last_draw_focused = false
    rfs.ui.dlg.__last_draw_scale = nil
    rfs.ui.dlg.buttons = buttons
    rfs.ui.dlg.__rt_w = 0
    rfs.ui.dlg.__rt_h = 0
    rfs.ui.dlg.text = text
end

function rfs.ui.dlg.draw()
    if not rfs.ui.dlg.isactive() then
        return
    end
    local focusyes = (((
        rfs.time.ticks() - rfs.ui.dlg.__focus_ts
    ) % 1200) < 600);
    if rfs.ui.dlg.rt == nil or
            rfs.ui.dlg.__last_draw_scale ~=
            rfs.ui.scaler or
            rfs.ui.dlg.__rt_w ~= rfs.window.renderw or
            rfs.ui.dlg.__rt_h ~= rfs.window.renderh or
            rfs.ui.dlg.__last_draw_focused ~= focusyes or
            rfs.time.ticks() < rfs.ui.dlg.__launch_ts + 1000 then
        if rfs.ui.dlg.rt == nil then
            rfs.ui.dlg.rt = rfs.gfx.create_target(1, 1, true)
        end
        rfs.gfx.resize_target(
            rfs.ui.dlg.rt, rfs.window.renderw, rfs.window.renderh
        )

        rfs.ui.dlg.update_sizes()
        local effectiveborder = rfs.ui.dlg.effectiveborder
        rfs.ui.dlg.__rt_w = rfs.window.renderw
        rfs.ui.dlg.__rt_h = rfs.window.renderh
        rfs.ui.dlg.__last_draw_focused = focusyes
        rfs.ui.dlg.__last_draw_scale = rfs.ui.scaler
        local oldt = rfs.gfx.get_draw_target()
        rfs.gfx.set_draw_target(rfs.ui.dlg.rt)
        rfs.gfx.clear_render()
        rfs.gfx.draw_rect(
            rfs.ui.dlg.dlgx, rfs.ui.dlg.dlgy,
            rfs.ui.dlg.dlgw, rfs.ui.dlg.dlgh,
            rfs.ui.dlg.bordercolor[1],
            rfs.ui.dlg.bordercolor[2],
            rfs.ui.dlg.bordercolor[3], 1
        )
        rfs.gfx.draw_rect(
            rfs.ui.dlg.dlgx + effectiveborder,
            rfs.ui.dlg.dlgy + effectiveborder,
            rfs.ui.dlg.dlgw - effectiveborder * 2,
            rfs.ui.dlg.dlgh - effectiveborder * 2,
            rfs.ui.dlg.bgcolor[1],
            rfs.ui.dlg.bgcolor[2],
            rfs.ui.dlg.bgcolor[3], 1
        )
        local iconxspacing = 0
        if rfs.ui.dlg.__icon ~= nil then
            iconxspacing = (rfs.ui.dlg.iconw +
                rfs.ui.dlg.effectiveborder)
        end
        rfs.ui.dlg.font:draw(
            rfs.ui.dlg.text, rfs.ui.dlg.text_width,
            rfs.ui.dlg.dlgx + effectiveborder * 1.5 + iconxspacing,
            rfs.ui.dlg.dlgy + effectiveborder * 1.5,
            0, 0, 0, 1,
            rfs.ui.dlg.__fontpt * rfs.ui.scaler
        )
        if rfs.ui.dlg.widget ~= nil then
            rfs.ui.dlg.widget:draw(
                rfs.ui.dlg.dlgx + rfs.ui.dlg.widgetx,
                rfs.ui.dlg.dlgy + rfs.ui.dlg.widgety
            )
        end
        if rfs.ui.dlg.__icon ~= nil then
            rfs.gfx.draw_tex(
                rfs.ui.dlg.__icon,
                rfs.ui.dlg.dlgx + effectiveborder * 1.5,
                rfs.ui.dlg.dlgy + effectiveborder * 1.5,
                rfs.ui.dlg.iconscale
            )
        end
        local buttonidx = 0
        for _, button in ipairs(rfs.ui.dlg.buttons) do
            buttonidx = buttonidx + 1
            local br = (
                rfs.ui.dlg.bgcolor[1] * 2 +
                rfs.ui.dlg.bordercolor[1]
            ) / 3
            local bg = (
                rfs.ui.dlg.bgcolor[2] * 2 +
                rfs.ui.dlg.bordercolor[2]
            ) / 3
            local bb = (
                rfs.ui.dlg.bgcolor[3] * 2 +
                rfs.ui.dlg.bordercolor[3]
            ) / 3
            rfs.gfx.draw_rect(
                rfs.ui.dlg.dlgx + button.x,
                rfs.ui.dlg.dlgy + button.y,
                button.width, button.height,
                br, bg, bb, 1
            )
            local tr = rfs.ui.dlg.textcolor[1]
            local tg = rfs.ui.dlg.textcolor[2]
            local tb = rfs.ui.dlg.textcolor[3]
            if focusyes and buttonidx == rfs.ui.dlg.focus_index then
                tr = rfs.ui.dlg.focuscolor[1]
                tg = rfs.ui.dlg.focuscolor[2]
                tb = rfs.ui.dlg.focuscolor[3]
            end
            rfs.ui.dlg.font:draw(
                button.text, nil,
                rfs.ui.dlg.dlgx + button.x +
                rfs.ui.dlg.button_border_width,
                rfs.ui.dlg.dlgy +
                button.y + rfs.ui.dlg.button_border_width,
                tr, tg, tb, 1,
                rfs.ui.dlg.__fontpt * rfs.ui.scaler
            )
        end
        rfs.gfx.update_render()
        rfs.gfx.set_draw_target(oldt)
    end
    rfs.gfx.draw_tex(rfs.ui.dlg.rt)
end

