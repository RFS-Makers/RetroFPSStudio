-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

if rfs == nil then rfs = {} end
if rfs.json == nil then rfs.json = {} end

function rfs.json.parse(v)
    return _json_decode(v)
end

function rfs.json.dump(v)
    return _json_encode(v)
end
