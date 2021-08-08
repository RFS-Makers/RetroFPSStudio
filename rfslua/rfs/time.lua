-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.time == nil then rfs.time = {} end

rfs.time.ticks = _time_getticks
rfs.time.sleepms = _time_sleepms

