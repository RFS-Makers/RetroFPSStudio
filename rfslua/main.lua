-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

dofile("rfslua/rfs/rfs.lua")
dofile("rfslua/rfseditor/rfseditor.lua")


local argno = 0
for _, v in ipairs(os.args) do
    argno = argno + 1

    if v == "--version" then
        print("RFS2 v" .. rfs.version)
        os.exit(0)
    end
    if v == "--internal-do-update-to" then
        local target = os.args[argno + 1]
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
        local target = os.args[argno + 1]
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
        rfs.__no_fps_lock = true
    end
    if v == "--fullscreen" then
        rfs.window.enable_fullscreen()
    end
    if v == "--software" then
        rfs.window.force_software()
    end
    if v == "--no-mouse" then
        _disablehwmouse()
    end
    if v == "--high-res" then
        rfs._force_full_render_res = true
    end
    if v == "--help" then
        print("Retro FPS Studio. Authentic 2.5d fps magic! " ..
              "All Rights Reserved.")
        print("Copyright (C) 2021, E.J.T. Version v" .. rfs.version ..
              ".")
        print("")
        print("Usage: RFS2 [..options..]")
        print("")
        print("Available options:")
        print("  --help        Print this help text.")
        print("  --no-mouse    Disable physical mice. Useful")
        print("                when touch input bugs out.")
        print("  --software    Force-disable any 3d acceleration.")
        print("  --version     Print out the program version.")
        os.exit(0)
    end
end


rfseditor.launch()
_rfs_coreinit()

