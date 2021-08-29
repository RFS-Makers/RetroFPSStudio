
Retro FPS Studio
================

![logo of RFS](./rfslua/res/ui/$logo.png)

Welcome to **Retro FPS Studio**, the game making tool for
raycasting retro fun! It produces true 2.5D, oldschool goodness
alike to the original [DOOM](
https://www.mobygames.com/game/dos/doom/screenshots) or [Hexen
](https://www.mobygames.com/game/dos/hexen-beyond-heretic/screenshots)
DOS games:

![screenshot of RFS](./misc/scr_promo_1.png)

*Note: this public, partial copy of RFS2 is sadly
**NOT free software** and has restrictive terms,
see [the license](LICENSE.md) for explanations.
Hopefully, you still find it informative, and I
apologize that it's not more liberal.*


How to use RFS
--------------

To get started, [obtain Retro FPS Studio](
    https://rfs.horse64.org/get) from the website.

To get help with RFS2, [visit the community spaces](
    https://rfs.horse64.org/#community) with other creators!

There is a [technical FAQ](https://rfs.horse64.org/engine_faq) for some
basics on how *RFS2 Engine* works.


What is in this repository?
---------------------------

**RFS** is written in C and Lua. The core renderer,
physics, and resource handling are in C for speed,
while Lua handles the higher level program UI.

Here are some interesting locations:

- `rfsc/` folder holds the core C engine

  - `rfsc/main.c` contains the program entrypoint.

  - `rfsc/scriptcore.c` sets up and launches the lua code.

  - `rfsc/roomcam.c` has the camera code, which includes perspective
    calculation, and the entire 2.5D raycast renderer.

  - `rfsc/room.h` holds the lowlevel map structure definitions
    of the [rooms](
    https://rfs.horse64.org/docs/concepts#what-is-a-room).

  - `rfsc/roomserialize.c` takes room data from Lua's so-called tables,
    or dumps it back to them, for serialization of levels.

- `rfslua/` folder contains the Lua UI, higher program logic, and
  gameplay code.

  - `rfslua/rfseditor/titlescreen.lua` sets up and handles the
    opening screen of **RFS** when you launch it.
