-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

if rfs == nil then rfs = {} end
if rfs.scene == nil then
    rfs.scene = {}
    rfs.scene._current = nil
end


function rfs.scene.on_keydown(t)
    if rfs.ui.dlg.on_keydown(t) then
        return
    end
    if rfs.scene._current ~= nil and
            rfs.scene._current.on_keydown ~= nil then
        rfs.scene._current.on_keydown(t)
    end
end

function rfs.scene.on_keyup(t)
    if rfs.ui.dlg.on_keyup(t) then
        return
    end
    if rfs.scene._current ~= nil and
            rfs.scene._current.on_keyup ~= nil then
        rfs.scene._current.on_keyup(t)
    end
end

function rfs.scene.on_debugstr()
    if rfs.scene._current ~= nil and
            rfs.scene._current.on_debugstr ~= nil then
        return tostring(rfs.scene._current.on_debugstr())
    end
    return ""
end


function rfs.scene.on_update()
    if rfs.scene._current ~= nil and
            rfs.scene._current.on_update ~= nil then
        rfs.scene._current.on_update()
    end
end


function rfs.scene.on_text(t)
    if rfs.ui.dlg.on_text(t) then
        return
    end
    if rfs.scene._current ~= nil and
            rfs.scene._current.on_text ~= nil then
        rfs.scene._current.on_text(t)
    end
end

function rfs.scene.on_click(x, y, button)
    if rfs.ui.dlg.on_click(x, y, button) then
        return
    end
    if rfs.scene._current ~= nil and
            rfs.scene._current.on_click ~= nil then
        rfs.scene._current.on_click(x, y, button)
    end
end

function rfs.scene.enter(scene)
    if scene.on_draw == nil then
        error("invalid scene")
    end
    if rfs.scene._current == scene then
        return
    end
    if rfs.scene._current ~= nil then
        if rfs.scene._current.on_leave ~= nil then
            rfs.scene._current.on_leave()
        end
    end
    rfs.scene._current = scene
    if rfs.scene._current.on_enter ~= nil then
        rfs.scene._current.on_enter()
    end
end

function rfs.scene.draw()
    if rfs.scene._current ~= nil then
        rfs.scene._current.on_draw()
    end
    rfs.ui.draw()
    rfs.debugoutput.draw()
end
