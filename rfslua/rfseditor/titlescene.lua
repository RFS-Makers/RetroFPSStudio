-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfseditor == nil then rfseditor = {} end
if rfseditor.titlescene == nil then rfseditor.titlescene = {} end


dofile("rfslua/rfseditor/_titlescene_license.lua")
dofile("rfslua/rfseditor/_titlescene_update.lua")


function rfseditor.titlescene.draw_particles()
    if rfseditor.titlescene._particles == nil or
            #rfseditor.titlescene._particles == 0 or
            rfseditor.titlescene._particle_ts == nil then
        return
    end
    local w, h = rfs.gfx.get_tex_size(
        "rfslua/res/ui/titlemenu_cloudbg")
    rfs.gfx.draw_tex(
        rfs.gfx.get_tex("rfslua/res/ui/titlemenu_cloudbg"),
        0, 0, rfs.window.renderw / w, rfs.window.renderh / h,
        1, 1, 1, 1)
    local scaler = rfs.ui.scaler
    local i = 1
    while i <= #rfseditor.titlescene._particles do
        local w, h = rfs.gfx.get_tex_size(
            rfseditor.titlescene._particles[i].tex)
        local size = 3.5
        rfs.gfx.draw_tex(
            rfseditor.titlescene._particles[i].tex,
            rfs.window.renderw *
                rfseditor.titlescene._particles[i].x -
                0.5 * size * scaler * w,
            rfs.window.renderh *
                rfseditor.titlescene._particles[i].y -
                0.5 * size * scaler * h,
            size * scaler, size * scaler, 1, 1, 1, 1
        )
        i = i + 1
    end
end

function rfseditor.titlescene.update_particles()
    if rfseditor.titlescene._particles == nil or
            #rfseditor.titlescene._particles == 0 or
            rfseditor.titlescene._particle_ts == nil then
        rfseditor.titlescene._particles = {}
        local i = 1
        while i <= 13 do
            table.insert(rfseditor.titlescene._particles, {
                x = -0.1 + 1.1 * math.random(),
                y = 0.1 + 0.1 * math.random(),
                vx = -0.00001 + 0.00002 * math.random(),
                tex = rfs.gfx.get_tex(
                    "rfslua/res/ui/titlemenu_cloud")
            })
            i = i + 1
        end
        rfseditor.titlescene._particle_ts = rfs.time.ticks()
    else
        local now = rfs.time.ticks()
        while rfseditor.titlescene._particle_ts + 0.06 <
                now do
            local i = 1
            while i <= #rfseditor.titlescene._particles do
                rfseditor.titlescene._particles[i].x = (
                    rfseditor.titlescene._particles[i].x +
                    rfseditor.titlescene._particles[i].vx
                )
                if rfseditor.titlescene._particles[i].x > 1.21 then
                    rfseditor.titlescene._particles[i].x = -0.2
                elseif rfseditor.titlescene._particles[i].x < -0.21 then
                    rfseditor.titlescene._particles[i].x = 1.2
                end
                i = i + 1
            end
            rfseditor.titlescene._particle_ts = (
                rfseditor.titlescene._particle_ts + 0.06
            )
        end
    end
end

function rfseditor.titlescene.on_debugstr()
    if rfs._show_renderstats and
            rfseditor._democam ~= nil then
        local stats = rfseditor._democam:renderstats()
        return rfs.json.dump(stats)
    end
    return ""
end

function rfseditor.titlescene.show_titlemenu()
    rfseditor.state = "titlemenu"
    local scaler = rfs.ui.scaler
    rfseditor.titlescene._menu = (
        rfs.ui.menuwidget.new(nil, 17 * scaler, 50)
    )
    rfseditor.titlescene._menu:set_focus()
    rfseditor.titlescene._menu:set_centered(true)
    rfseditor.titlescene._menu.id = "titlemenu"
    rfseditor.titlescene._menu:add_entry(
        nil, "rfslua/res/ui/$rgsmenu_create"
    )
    rfseditor.titlescene._menu:add_entry(
        nil, "rfslua/res/ui/$rgsmenu_play"
    )
    rfseditor.titlescene._menu:add_entry(
        "Settings"
    )
    rfseditor.titlescene._menu:add_entry(
        "Quit"
    )
    rfseditor.titlescene.update_menu()
end

function rfseditor.titlescene.update_menu()
    local scaler = rfs.ui.scaler
    if rfseditor.titlescene._menu ~= nil and
            rfseditor.titlescene._menu.id == "titlemenu" then
        rfseditor.titlescene._menu:set_pt(
            math.max(1, math.round(17 * scaler))
        )
        rfseditor.titlescene._menu:set_width(
            math.max(1, math.round(rfs.window.renderw * 0.3))
        )
        if rfseditor.titlescene.horimenu then
            rfseditor.titlescene._menu.x = (
                rfs.window.renderw * 0.6
            )
            rfseditor.titlescene._menu.y = math.max(0,
                math.round((
                    rfs.window.renderh * 0.5 -
                    rfseditor.titlescene._menu.height * 0.5
                )))
        else
            rfseditor.titlescene._menu.x = (
                rfs.window.renderw * 0.35
            )
            rfseditor.titlescene._menu.y = (
                rfs.window.renderh * 0.4
            )
        end
    end
end

function rfseditor.titlescene.load_demo_level()
    if rfseditor._demolevel ~= nil then
        return
    end
    rfseditor._demopososcillate = 0
    local lvl = rfs.roomlayer.new(1)
    local rid = 1
    lvl:deserialize_rooms({{
        id=rid,
        light={0.25, 0.2, 0.25},
        floor_z=-(rfseditor.defaults.one_meter_units * 1),
        height=(rfseditor.defaults.one_meter_units * 2),
        walls={
            {corner_x=(rfseditor.defaults.one_meter_units * 3.5),
             corner_y=-(rfseditor.defaults.one_meter_units * 4.5),
             texpath="rfslua/res/default-game-res/texture/brick1",
             portal_to=2},
            {corner_x=(rfseditor.defaults.one_meter_units * 1),
             corner_y=-(rfseditor.defaults.one_meter_units * 3.5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=-(rfseditor.defaults.one_meter_units * 3.5),
             corner_y=-(rfseditor.defaults.one_meter_units * 3.3),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=(0 * 5),
             corner_y=(rfseditor.defaults.one_meter_units * 3.5),
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
             corner_y=-(rfseditor.defaults.one_meter_units * 3.5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=(rfseditor.defaults.one_meter_units * 3.5),
             corner_y=-(rfseditor.defaults.one_meter_units * 4.5),
             texpath="rfslua/res/default-game-res/texture/brick1"},
            {corner_x=(rfseditor.defaults.one_meter_units * 2.5),
             corner_y=-(rfseditor.defaults.one_meter_units * 7),
             texpath="rfslua/res/default-game-res/texture/brick1"},
        },
        ceiling={texpath="rfslua/res/default-game-res/texture/wood1"},
        floor={texpath="rfslua/res/default-game-res/texture/wood1"},  
    }})
    local obj = rfs.movable.new_invisible()
    obj:set_light(1, 0.8, 0, rfseditor.defaults.one_meter_units * 4)
    obj:set_layer(lvl)
    rfseditor._demolevel = lvl
    if rfseditor._democam == nil then
        rfseditor._democam = rfs.roomcam.new()
        assert(rfseditor._democam ~= nil)
    end
    rfseditor._democam:set_layer(rfseditor._demolevel)
end

function rfseditor.titlescene.on_enter()
    rfs.ui.loadsounds()
    rfseditor.titlescene._menu = nil
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
        "reflect the final product.\n\n" ..
        "It's FREE to use. (While that " ..
        "may change, for now there are no immediate plans.)",
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

function rfseditor.titlescene.on_keydown(k)
    if rfseditor.state == "show_press_any_key" and
            k ~= "f11" then
        rfseditor.state = "devwarning"
        rfs.ui.playsound(rfs.ui.confirmsound)
        _devversion_warning()
        return
    end
    if rfseditor.titlescene._menu ~= nil then
        if rfseditor.titlescene._menu:on_keydown(k) == true then
            return
        end
    end
end

function rfseditor.titlescene.on_click(x, y, button)
    if rfseditor.state == "show_press_any_key" then
        rfseditor.state = "devwarning"
        rfs.ui.playsound(rfs.ui.confirmsound)
        _devversion_warning()
    end
end

function rfseditor.titlescene.on_mousemove(x, y)
    if rfseditor.titlescene._menu ~= nil then
        if rfseditor.titlescene._menu:on_mousemove(
                x - rfseditor.titlescene._menu.x,
                y - rfseditor.titlescene._menu.y
                ) == true then
            return
        end
    end
end

function rfseditor.titlescene.on_update()
    rfseditor.titlescene.update_menu()
    _titlescreen_update_on_update()
    _titlescreen_license_on_update()
    rfseditor.titlescene.update_particles()

    -- 3D world pos update:
    if rfseditor._democam ~= nil then
        local now = rfs.time.ticks()
        if rfseditor._democamts == nil then
            rfseditor._democamts = now
        end
        while rfseditor._democamts < now do
            rfseditor._democam:set_angle(
                rfseditor._democam:get_angle() + 0.1
            )
            rfseditor._demopososcillate = (
                rfseditor._demopososcillate + 1
            )
            while rfseditor._demopososcillate >= 1000 do
                rfseditor._demopososcillate = (
                    rfseditor._demopososcillate - 1000
                )
            end
            rfseditor._democamts = rfseditor._democamts + 20
        end
        local xoffset = (
            rfseditor.defaults.one_meter_units *
            math.sin(rfseditor._demopososcillate * math.pi * 2 / 1000.0) / 2.0
        )
        local zoffset = -(
            rfseditor.defaults.one_meter_units *
            math.sin((rfseditor._demopososcillate + 300) * math.pi * 2 /
            1000.0) / 2.0
        )
        rfseditor._democam:set_pos(
            xoffset, 0, zoffset
        )
        local vangle = -math.sin(rfseditor._demopososcillate * math.pi *
            2 / 1000.0) * 10
        rfseditor._democam:set_vangle(vangle)
    end
end

function rfseditor.titlescene.on_draw()
    local scaler = rfs.ui.scaler
    rfseditor.titlescene.horimenu = (
        rfs.window.renderw > rfs.window.renderh * 1.0
    )
    if rfseditor.state == "show_press_any_key" or
            rfseditor.state == "do_update_task" then
        rfseditor.titlescene.horimenu = false
    end
    if rfseditor.state == "titlemenu" and
            (rfseditor.titlescene._menu == nil or
            rfseditor.titlescene._menu.id ~= "titlemenu") then
        rfseditor.titlescene.show_titlemenu()
    end
    rfseditor.titlescene.update_menu()

    -- 3D world draw:
    if rfseditor._democam ~= nil then
        rfseditor._democam:draw(
            0, 0, rfs.window.renderw, rfs.window.renderh
        )
    else
        -- Otherwise, clear with black:
        rfs.gfx.draw_rect(
            0, 0, rfs.window.renderw, rfs.window.renderh,
            0.1, 0, 0.1, 1.0
        )
    end

    -- Fancy particles:
    rfseditor.titlescene.draw_particles()

    -- Logo
    local logow, logoh = rfs.gfx.get_tex_size(
        "rfslua/res/ui/$logo"
    )
    logow = logow * 1.5 * scaler
    logoh = logoh * 1.5 * scaler
    if rfseditor.titlescene.horimenu then
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
        text, rfs.window.renderw, 12 * scaler
    )
    font:draw(
        text, rfs.window.renderw, 0,
        rfs.window.renderh - fontboxh,
        1, 0.9, 1.0, 0.7, 12 * scaler
    )

    -- Show the press any key
    if rfseditor.state == "show_press_any_key" then
        local now = rfs.time.ticks()
        local animf = (now - rfseditor.show_any_key_ts) % 1200
        animf = math.round(animf / 1200)
        local t = "Press Key Or Tap To Start"
        local show_width = math.min(
            font:calcwidth(t, 14 * scaler, animf, 1),
            rfs.window.renderw
        )
        local draw_x = rfs.window.renderw / 2 - show_width / 2
        local draw_y = rfs.window.renderh / 2
        font:draw(
            t, rfs.window.renderw, draw_x, draw_y,
            1, 0.9 * animf, 1.0, 1.0,
            14 * scaler, animf, 1
        )
    end

    -- Draw menu if any:
    if rfseditor.state == "titlemenu" and
            rfseditor.titlescene._menu ~= nil then
        rfseditor.titlescene._menu:draw(
            rfseditor.titlescene._menu.x,
            rfseditor.titlescene._menu.y
        )
    end
end

