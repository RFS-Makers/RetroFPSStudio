-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- See LICENSE.md for details.

if rfs == nil then rfs = {} end
if rfs.events == nil then rfs.events = {} end


os.clipboard_paste = _os_clipboardpaste

rfs.events.ctrl_pressed = _events_ctrlpressed

function rfs.events.start_text_input()
    _events_starttextinput()
end

function rfs.events.stop_text_input()
    _events_stoptextinput()
end

