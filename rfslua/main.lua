-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

dofile("rfslua/rfs/rfs.lua")
dofile("rfslua/rfseditor/rfseditor.lua")


local pargs = {}
local first = true
for _, v in ipairs(os.args) do
    if first then
        first = false
    else
        table.insert(pargs, v)
    end
end

local argno = 0
while argno + 1 <= #pargs do
    argno = argno + 1
    local v = pargs[argno]
    local optionknown = false

    if v == "--version" then
        optionknown = true
        print("RFS2 v" .. rfs.version)
        os.exit(0)
    end
    if v == "--internal-do-update-to" then
        optionknown = true
        local target = pargs[argno + 1]
        rfseditor._updatetask = function()
            if not os.exists(target) then
                error("not given a valid update target")
            end
            os.remove(target)
            os.copyfile(os.binarypath(), target)
            os.setexecbit(target)
            os.run(target, "--internal-delete-update-source",
                os.abspath(os.binarypath()))
        end
        break
    end
    if v == "--internal-delete-update-source" then
        optionknown = true
        local target = pargs[argno + 1]
        rfseditor._updatetask = function()
            if not os.exists(target) then
                error("not given a valid update source")
            end
            os.remove(target)
            os.run(os.binarypath())
        end
        break
    end
    if v == "--no-fps-lock" then
        optionknown = true
        rfs.__no_fps_lock = true
    end
    if v == "--fullscreen" then
        optionknown = true
        rfs.window.enable_fullscreen()
    end
    if v == "--software" then
        optionknown = true
        rfs.window.force_software()
    end
    if v == "--no-mouse" then
        optionknown = true
        _disablehwmouse()
    end
    if v == "--high-res" then
        optionknown = true
        rfs._force_full_render_res = true
    end
    if v == "--aspect-ratio" then
        optionknown = true
        if pargs[argno + 1] == nil or
                pargs[argno + 1]:sub(1, 1) == "-" or
                pargs[argno + 1]:find(":", 1, true) == nil then
            print("RFS2: error: --aspect-ratio needs " ..
                "a valid ratio argument, e.g. 4:3")
            os.exit(1)
        end
        local res = pargs[argno + 1]
        local comma = res:find(":", 1, true)
        local w = tonumber(res:sub(1, comma - 1))
        local h = tonumber(res:sub(comma + 1))
        if type(w) ~= "number" or w < 1 or
                type(h) ~= "number" or h < 1 then
            print("RFS2: error: --aspect-ratio needs " ..
                "a valid ratio argument, e.g. 4:3")
            os.exit(1)
        end
        rfs._force_rt_ratio = {w, h}
        argno = argno + 1
    end
    if v == "--resolution" then
        optionknown = true
        if pargs[argno + 1] == nil or
                pargs[argno + 1]:sub(1, 1) == "-" or
                pargs[argno + 1]:find("x", 1, true) == nil then
            print("RFS2: error: --resolution needs " ..
                "a valid resolution argument, e.g. 320x240")
            os.exit(1)
        end
        local res = pargs[argno + 1]
        local comma = res:find("x", 1, true)
        local w = tonumber(res:sub(1, comma - 1))
        local h = tonumber(res:sub(comma + 1))
        if type(w) ~= "number" or w < 1 or
                type(h) ~= "number" or h < 1 then
            print("RFS2: error: --resolution needs " ..
                "a valid resolution argument, e.g. 320x240")
            os.exit(1)
        end
        rfs._force_rt_res = {w, h}
        argno = argno + 1
    end
    if v == "--help" then
        optionknown = true
        print("Retro FPS Studio. Authentic 2.5d fps magic! " ..
              "All Rights Reserved.")
        print("Copyright (C) 2021, E.J.T. Version v" .. rfs.version ..
              ".")
        print("")
        print("Usage: RFS2 [..options..]")
        print("")
        print("Available options:")
        print("  --aspect-ratio Enforce a fixed output aspect ratio.")
        print("  --help         Print this help text.")
        print("  --no-mouse     Disable physical mice. Useful")
        print("                 when touch input bugs out.")
        print("  --resolution   Enforce a fixed output resolution.")
        print("  --software     Force-disable any 3d acceleration.")
        print("  --version      Print out the program version.")
        os.exit(0)
    end

    if not optionknown then
        print("RFS2: error: unknown option: " .. v)
        os.exit(1)
    end
end


rfseditor.launch()
_rfs_coreinit()

