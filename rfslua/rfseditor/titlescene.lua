-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

if rfseditor == nil then rfseditor = {} end
if rfseditor.titlescene == nil then rfseditor.titlescene = {} end


dofile("rfslua/rfseditor/_titlescene_license.lua")
dofile("rfslua/rfseditor/_titlescene_update.lua")


function rfseditor.titlescene.on_debugstr()
    if rfs._show_renderstats and
            rfseditor._democam ~= nil then
        local stats = rfseditor._democam:renderstats()
        return rfs.json.dump(stats)
    end
    return ""
end

function rfseditor.titlescene.load_demo_level()
    if rfseditor._demolevel ~= nil then
        return
    end
    local lvl = rfs.roomlayer.new(1)
    local rid = 1
    lvl:deserialize_rooms({{
        id=rid,
        floor_z=-(rfseditor.defaults.one_meter_units * 1),
        height=(rfseditor.defaults.one_meter_units * 2),
        walls={
            {corner_x=(rfseditor.defaults.one_meter_units * 5),
             corner_y=-(rfseditor.defaults.one_meter_units * 5),
             texpath="rfslua/res/default-game-res/texture/brick1",
             portal_to=2},
            {corner_x=(rfseditor.defaults.one_meter_units * 1),
             corner_y=-(rfseditor.defaults.one_meter_units * 5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=-(rfseditor.defaults.one_meter_units * 5),
             corner_y=-(rfseditor.defaults.one_meter_units * 5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=(0 * 5),
             corner_y=(rfseditor.defaults.one_meter_units * 5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
        },
        ceiling={texpath="rfslua/res/default-game-res/texture/wood1"},
        floor={texpath="rfslua/res/default-game-res/texture/wood1"},
    }, {
        id=2,
        floor_z=-(rfseditor.defaults.one_meter_units * 0.6),
        height=(rfseditor.defaults.one_meter_units * 2),
        walls={
            {corner_x=(rfseditor.defaults.one_meter_units * 1),
             corner_y=-(rfseditor.defaults.one_meter_units * 5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=(rfseditor.defaults.one_meter_units * 5),
             corner_y=-(rfseditor.defaults.one_meter_units * 5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=(rfseditor.defaults.one_meter_units * 3),
             corner_y=-(rfseditor.defaults.one_meter_units * 15),
             texpath="rfslua/res/default-game-res/texture/brick1"},
        },
        ceiling={texpath="rfslua/res/default-game-res/texture/wood1"},
        floor={texpath="rfslua/res/default-game-res/texture/wood1"},  
    }})
    rfseditor._demolevel = lvl
    if rfseditor._democam == nil then
        rfseditor._democam = rfs.roomcam.new()
        assert(rfseditor._democam ~= nil)
    end
    rfseditor._democam:set_layer(rfseditor._demolevel)
end

function rfseditor.titlescene.on_enter()
    rfseditor._democamts = nil
    if rfseditor._updatetask == nil then
        rfseditor.show_any_key_ts = rfs.time.ticks()
        rfseditor.state = "show_press_any_key"
        if rfseditor._demolevel == nil then
            rfseditor.titlescene.load_demo_level()
        end
    else
        rfseditor.state = "do_update_task"
    end
end

function _devversion_warning()
    rfs.ui.dlg.show(
        "This is an EARLY ACCESS version. It doesn't " ..
        "reflect the final product.",
        {{text="Got it", func=function()
            if rfseditor.license.got_problem() then
                _titlescreen_license_show_missing()
            else
                _titlescreen_update_offer_check()
            end
        end}},
        nil,
        "rfslua/res/ui/dlg/$devpreview"
    )
end

function rfseditor.titlescene.on_click(x, y, button)
    if rfseditor.state == "show_press_any_key" then
        rfseditor.state = "devwarning"
        _devversion_warning()
    end
end

function rfseditor.titlescene.on_update()
    _titlescreen_update_on_update()
    _titlescreen_license_on_update()
end

function rfseditor.titlescene.on_draw()
    local scaler = rfs.ui.scaler
    local horimenu = (rfs.window.renderw > rfs.window.renderh * 1.45)
    if rfseditor.state == "show_press_any_key" or
            rfseditor.state == "do_update_task" then
        horimenu = false
    end

    -- Nonsense test particles:
    --[[if true then
        local t = rfs.gfx.get_tex("rfslua/res/ui/$logo")
        if rfseditor.titlescene._particles == nil then
            rfseditor.titlescene._particles = {}
            local i = 1
            while i < 150 do
                table.insert(
                    rfseditor.titlescene._particles,
                    {math.random() * 1.2 - 0.1,
                     math.random() * 1.2 - 0.1}
                )
                i = i + 1
            end
        end
        local i = 1
        while i < #rfseditor.titlescene._particles do
            local pos = rfseditor.titlescene._particles[i]
            pos[1] = pos[1] + 0.001
            pos[2] = pos[2] + 0.002
            if pos[1] > 1.1 then
                pos[1] = -0.1
            end
            if pos[2] > 1.1 then
                pos[2] = -0.1
            end
            rfseditor.titlescene._particles[i] = pos
            rfs.gfx.draw_tex(
                rfs.gfx.get_tex("rfslua/res/ui/$logo"),
                pos[1] * rfs.window.renderw,
                pos[2] * rfs.window.renderh,
                0.5 * scaler, 0.5 * scaler, 1, 1, 1, 0.7
            )
            i = i + 1
        end
    end]]

    -- 3D world:
    if rfseditor._democam ~= nil then
        local now = rfs.time.ticks()
        if rfseditor._democamts == nil then
            rfseditor._democamts = now
        end
        while rfseditor._democamts < now do
            rfseditor._democam:set_angle(
                rfseditor._democam:get_angle() + 0.1
            )
            rfseditor._democamts = rfseditor._democamts + 20
        end
        rfseditor._democam:draw(
            0, 0, rfs.window.renderw, rfs.window.renderh
        )
    end

    -- Logo
    local logow, logoh = rfs.gfx.get_tex_size(
        "rfslua/res/ui/$logo"
    )
    logow = logow * 1.5 * scaler
    logoh = logoh * 1.5 * scaler
    if horimenu then
        rfs.gfx.draw_tex(
            rfs.gfx.get_tex("rfslua/res/ui/$logo"),
            rfs.window.renderw * 0.25 - logow * 0.5,
            rfs.window.renderh * 0.5 - logoh * 0.5,
            1.5 * scaler, 1.5 * scaler, 1, 1, 1, 1
        )
    else
        rfs.gfx.draw_tex(
            rfs.gfx.get_tex("rfslua/res/ui/$logo"),
            rfs.window.renderw * 0.5 - logow * 0.5,
            rfs.window.renderh * 0.2 - logoh * 0.5,
            1.5 * scaler, 1.5 * scaler, 1, 1, 1, 1
        )
    end

    -- Footer
    local font = rfs.font.load(rfs.ui.default_font)
    local text = "RFS2-v" .. rfs.version .. " Copyright (C) " ..
        "E.J.T., All Rights Reserved." .. (function()
            if rfs.gfx.using_3d_acc() then
                return " HW/3D Upscale is ON (" ..
                    rfs.gfx.renderer_name() .. ")."
            end
            return " HW/3D Upscale is OFF."
        end)() .. (function()
            if rfs._force_full_render_res == true then
                return " High-Res Debug Mode."
            end
            return ""
        end)()
    local fontboxh = font:calcheight(
        12 * scaler, text, rfs.window.renderw
    )
    font:draw(
        12 * scaler, text,
        0, rfs.window.renderh - fontboxh, rfs.window.renderw,
        1, 0.9, 1.0, 0.7
    )

    -- Show the press any key
    if rfseditor.state == "show_press_any_key" then
        local now = rfs.time.ticks()
        local animf = (now - rfseditor.show_any_key_ts) % 1600
        animf = animf / 800
        if animf > 1 then
            animf = 2.0 - animf
        end
        local t = "Press Key Or Tap To Start"
        local show_width = math.min(
            font:calcwidth(14 * scaler, t), rfs.window.renderw
        )
        font:draw(
            14 * scaler, t,
            rfs.window.renderw / 2 - show_width / 2,
            rfs.window.renderh / 2, rfs.window.renderw,
            1, 0.9 * animf, 1.0, 0.7
        )
    end
end

