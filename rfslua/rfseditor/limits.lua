-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

if rfseditor == nil then rfseditor = {} end
if rfseditor.limits == nil then
    rfseditor.limits = {
        max_levels=10000,
        max_rooms=_roomlayer_global_getmaxroomslimit(),
        max_walls=_roomlayer_global_getmaxwallslimit(),
        max_decals=_roomlayer_global_getmaxdecalslimit(),
        smallest_colider_size=_roomlayer_global_getsmallestcolliderlimit(),
        largest_collider_size=_roomlayer_global_getlargestcolliderlimit(),
    }
end

