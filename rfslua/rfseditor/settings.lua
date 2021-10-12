-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfseditor == nil then rfseditor = {} end
if rfseditor.settings == nil then
    rfseditor.settings = {}
end

function rfseditor.settings.load()
    local dirpath = os.make_appdata_folder("RFS2")
    local settingsfile = os.pathjoin(dirpath, "settings.json")
    if os.exists(settingsfile) then
        local f = io.open(settingsfile, "r")
        local filecontents = f:read("*all")
        f:close()
        local json_result = {result = {}}
        pcall(function()
            json_result.result = rfs.json.parse(filecontents)
        end)
        if type(json_result.result) == "table" then
            for k, v in pairs(json_result.result) do
                if type(rfseditor.settings[k]) ~=
                        "function" then
                    rfseditor.settings[k] = v
                end
            end
        end
    end
    if rfseditor.settings.gamma == nil then
        rfseditor.settings.gamma = 128
    end
    if rfseditor.settings.fullscreen == nil then
        rfseditor.settings.fullscreen = false
    end
end


function rfseditor.settings.save()
    local dirpath = os.make_appdata_folder("RFS2")
    local settingsfile = os.pathjoin(dirpath, "settings.json")
    local tostore = {}
    for k, v in pairs(rfseditor.settings) do
        if type(rfseditor.settings[k]) ~= "function" then
            tostore[k] = rfseditor.settings[k]
        end
    end
    local f = io.open(settingsfile, "wb")
    f:write(rfs.json.dump(tostore))
    f:close()
end
