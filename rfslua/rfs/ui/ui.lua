-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.ui == nil then rfs.ui = {} end

dofile("rfslua/rfs/ui/dlg.lua")
dofile("rfslua/rfs/ui/entry.lua")

rfs.ui.default_font = "rfslua/res/ui/font/font_default"

function rfs.ui.draw()
    rfs.ui.dlg.draw()
end

