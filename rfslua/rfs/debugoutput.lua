-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details


if rfs == nil then rfs = {} end
if rfs.debugoutput == nil then rfs.debugoutput = {} end

function rfs.debugoutput.draw()
    if rfs.debugoutput._str == nil or
            (rfs.debugoutput._str_ts ~= nil and
            rfs.debugoutput._str_ts + 1000 < rfs.time.ticks()) then
        rfs.debugoutput._str = rfs.debugoutput.compute_string()
        rfs.debugoutput._str_ts = rfs.time.ticks()
        --[[if #rfs.debugoutput._str > 0 then
            print(rfs.debugoutput._str)
        end]]
    end
    if rfs.debugoutput._str == nil or
            rfs.debugoutput._str == "" then
        return
    end
    local scaler = rfs.ui.scaler
    local font = rfs.font.load(rfs.ui.default_font)
    font:draw(
        rfs.debugoutput._str, rfs.window.renderw,
        0, 0,
        1, 1, 1, 1,
        14 * scaler
    )
end

function rfs.debugoutput.compute_string()
    return rfs.scene.on_debugstr()
end
