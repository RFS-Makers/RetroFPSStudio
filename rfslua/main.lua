-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

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

    if v == "--miditest" then
        if argno + 1 > #pargs then
            error("--miditest needs midi file argument")
        end
        local path = os.normpath(pargs[argno + 1])
        if not os.exists(path) then
            error("no such file: " .. path)
        end
        os.ensureconsole()
        local song = rfs.song.load(path)
        print("Playing song of " .. song:length() ..
            " seconds length...")
        song:play()
        while song:isplaying() and not os.hadendsignal() do
            rfs.time.sleepms(50)
        end
        if os.consolewasspawned() then
            os.anykeytocontinue()
        end
        os.exit(0)
    end
    if v == "--version" or v == "-version" or v == "-v" or
            string.lower(v) == "/version" then
        optionknown = true
        os.ensureconsole()
        print("RFS2 v" .. rfs.version)
        if os.consolewasspawned() then
            os.anykeytocontinue()
        end
        os.exit(0)
    end
    if v == "--sha512crypt" then
        if argno + 3 > #pargs then
            error("--sha512crypt needs 3 arguments: " ..
                "key salt rounds")
        end
        local key = tostring(pargs[argno + 1])
        local salt = tostring(pargs[argno + 2])
        local rounds = tonumber(pargs[argno + 3])
        os.ensureconsole()
        print(_crypto_sha512crypt(key, salt, rounds))
        if os.consolewasspawned() then
            os.anykeytocontinue()
        end
        os.exit(0)
    end
    if v == "--internal-do-update-to" and argno + 1 <= #pargs then
        optionknown = true
        local target = os.normpath(pargs[argno + 1])
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
        local target = os.normpath(pargs[argno + 1])
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
    if v == "--render-stats" then
        optionknown = true
        rfs._show_renderstats = true
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
        os.ensureconsole()
        optionknown = true
        print("Retro FPS Studio. Authentic 2.5D FPS magic! " ..
              "All Rights Reserved.")
        print("Copyright (C) 2021, E.J.T. Version v" .. rfs.version ..
              ".")
        print("")
        print("Usage: RFS2 [..options..]")
        print("")
        print("Available options:")
        print("  --aspect-ratio ...  Enforce a fixed output aspect ")
        print("                      ratio.")
        print("  --help              Print this help text.")
        print("  --miditest ...      Play back given .midi file for ")
        print("                      testing.")
        print("  --no-mouse          Disable physical mice. Useful")
        print("                      when touch input bugs out.")
        print("  --render-stats      Print out render statistics.")
        print("  --resolution ...    Enforce a fixed output resolution.")
        print("  --software          Force-disable any 3d acceleration.")
        print("  --version           Print out the program version.")
        if os.consolewasspawned() then
            os.anykeytocontinue()
        end
        os.exit(0)
    end

    if not optionknown then
        os.ensureconsole()
        print("RFS2: error: unknown option: " .. v)
        if os.consolewasspawned() then
            os.anykeytocontinue()
        end
        os.exit(1)
    end
end


rfseditor.launch()
_rfs_coreinit()

