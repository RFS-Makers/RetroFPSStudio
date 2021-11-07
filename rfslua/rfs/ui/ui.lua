-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.ui == nil then rfs.ui = {} end

dofile("rfslua/rfs/ui/dlg.lua")
dofile("rfslua/rfs/ui/entrywidget.lua")
dofile("rfslua/rfs/ui/menuwidget.lua")

rfs.ui.default_font = "rfslua/res/ui/font/font_default"

function rfs.ui.loadsounds()
    if rfs.ui.confirmsound == nil then
        rfs.ui.confirmsound = rfs.audio.preloadsfx(
            "rfslua/res/default-game-res/sfx/confirm01")
    end
    if rfs.ui.focussound == nil then
        rfs.ui.focussound = rfs.audio.preloadsfx(
            "rfslua/res/default-game-res/sfx/click02.wav")
    end
    if rfs.ui.oksound == nil then
        rfs.ui.oksound = rfs.audio.preloadsfx(
            "rfslua/res/default-game-res/sfx/click03.wav")
    end
    if rfs.ui.cancelsound == nil then
        rfs.ui.cancelsound = rfs.audio.preloadsfx(
            "rfslua/res/default-game-res/sfx/click01.wav")
    end
end

function rfs.ui.playsound(sfx, vol)
    if vol == nil then
        vol = 1
    end
    vol = math.max(0.0, math.min(1.0, vol))
    rfs.audio.play(sfx, vol * 0.6)
end

function rfs.ui.draw()
    rfs.ui.dlg.draw()
end

