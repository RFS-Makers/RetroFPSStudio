-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

if rfs == nil then rfs = {} end
if rfs.gfx == nil then rfs.gfx = {} end

rfs.gfx.force_final_output_size = _graphics_forceoutputsize
rfs.gfx.get_tex = _graphics_gettex
rfs.gfx.tex_to_id = _graphics_textoid
rfs.gfx.id_to_tex = _graphics_idtotex
rfs.gfx.set_draw_target = _graphics_setrt
rfs.gfx.get_draw_target = _graphics_getrt
rfs.gfx.get_tex_size = _graphics_gettexsize
rfs.gfx.create_target = function(w, h, crisp)
    local result = _graphics_creatert(w, h, crisp)
    debug.setmetatable(result, {
        __gc = function(gcself)
            local f = function(_gcself)
                _graphics_deletetex(_gcself)
            end
            pcall(f, _gcself)
        end
    })
    return result
end
rfs.gfx.resize_target = _graphics_resizert
rfs.gfx.update_render = _graphics_updatert
rfs.gfx.clear_render = _graphics_clearrt
rfs.gfx.draw_tex = _graphics_drawtex
rfs.gfx.delete_tex = _graphics_deletetex
rfs.gfx.draw_rect = _graphics_drawrectangle
rfs.gfx.draw_line = _graphics_drawline
rfs.gfx.using_3d_acc = _graphics_is3dacc
rfs.gfx.renderer_name = _graphics_renderername
rfs.gfx.push_scissors = _graphics_pushscissors
rfs.gfx.pop_scissors = _graphics_popscissors

