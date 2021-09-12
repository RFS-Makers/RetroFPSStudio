// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <string.h>

#include "datetime.h"
#include "outputwindow.h"
#include "sdl/sdlkey_to_str.h"
#include "sdl/sdlkeyboard.h"
#include "settings.h"


static int textinputactive = 0;
static int _script_last_seen_w = -1;
static int _script_last_seen_h = -1;
static int windowhasfocus = 1;
static int relativemousemove = 0;
static int relativemousemovetemporarilydisabled = 0;
static void _updatemousemode();
static int lctrlpressed = 0;
static int rctrlpressed = 0;


void scriptcoreevents_SetRelativeMouse(int relative) {
    relativemousemove = relative;
    _updatemousemode();
}


int CountFingers() {
    int num = 0;
    int i = 0;
    while (i < SDL_GetNumTouchDevices()) {
        SDL_TouchID ti = SDL_GetTouchDevice(i);
        num += SDL_GetNumTouchFingers(ti);
        i++;
    }
    return num;
}


static void _updatemousemode() {
    if (relativemousemove && windowhasfocus &&
            !relativemousemovetemporarilydisabled &&
            !SDL_GetRelativeMouseMode()) {
        SDL_SetRelativeMouseMode(1);
    } else if ((!relativemousemove || !windowhasfocus ||
            relativemousemovetemporarilydisabled) &&
            SDL_GetRelativeMouseMode()) {
        SDL_SetRelativeMouseMode(0);
    }
}


static int multiple_fingers_on = 0;
static int single_finger_fid = 0;
static int single_finger_down = 0;
static int single_finger_isdrag = 0;
static int single_finger_canbeclick = 0;
static double single_finger_startx = 0;
static double single_finger_starty = 0;
static double single_finger_currentx = 0;
static double single_finger_currenty = 0;
static int64_t single_finger_downms = -1;
static const int64_t single_finger_maxleftclickms = 1300;


static int scriptcoreevents_iskeydown(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    #ifdef HAVE_SDL
    if (strcmp(p, "leftcontrol") == 0)
        lua_pushboolean(l, lctrlpressed);
    else if (strcmp(p, "rightcontrol") == 0)
        lua_pushboolean(l, rctrlpressed);
    else
        lua_pushboolean(l, sdlkeyboard_IsKeyPressed(p));
    #else
    #error "code path not implemented"
    #endif
    return 0;
}


int scriptcoreevents_Process(lua_State *l) {
    if (!l)
        return 0;

    lua_newtable(l);
    int tbl_index = lua_gettop(l);

    int count = 0;

    int ww = 1;
    int wh = 1;
    SDL_GetWindowSize(
        outputwindow_GetWindow(), &ww, &wh
    );
    if (ww < 1)
        ww = 1;
    if (wh < 1)
        wh = 1;

    if (_script_last_seen_w < 0 ||
            _script_last_seen_w != ww ||
            _script_last_seen_h != wh) {
        lua_newtable(l);
        lua_pushstring(l, "type");
        lua_pushstring(l, "window_resize");
        lua_settable(l, -3);
        lua_pushstring(l, "width");
        lua_pushnumber(l, ww);
        lua_settable(l, -3);
        lua_pushstring(l, "height");
        lua_pushnumber(l, wh);
        lua_settable(l, -3);
        lua_rawseti(l, tbl_index, count + 1);
        count++;
        _script_last_seen_w = ww;
        _script_last_seen_h = wh;
    }

    _updatemousemode();

    double single_finger_maxclickmove = (
        5.0f * rfswindowdpiscaler
    );
    double evdpimultiply = rfswindowdpiscaler;
    double relmousex = (double)rfswindoww / 2.0f;
    double relmousey = (double)rfswindowh / 2.0f;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        _updatemousemode();
        if (e.type == SDL_QUIT) {
            lua_newtable(l);
            lua_pushstring(l, "type");
            lua_pushstring(l, "quit");
            lua_settable(l, -3);
            lua_rawseti(l, tbl_index, count + 1);
            count++;
        } else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_LCTRL)
                lctrlpressed = 1;
            else if (e.key.keysym.sym == SDLK_RCTRL)
                rctrlpressed = 1;
            char key[15] = "";
            ConvertSDLKeyToStr(e.key.keysym.sym, key);
            if (strlen(key) > 0) {
                if ((e.key.keysym.mod & KMOD_LCTRL) != 0 &&
                        !lctrlpressed &&
                        e.key.keysym.sym != SDLK_LCTRL) {
                    // Buggy on-screen keyboard, maybe.
                    lctrlpressed = 1;
                    lua_newtable(l);
                    lua_pushstring(l, "type");
                    lua_pushstring(l, "keydown");
                    lua_settable(l, -3);
                    lua_pushstring(l, "key");
                    lua_pushstring(l, "leftcontrol");
                    lua_settable(l, -3);
                    lua_rawseti(l, tbl_index, count + 1);
                    count++;
                } else if ((e.key.keysym.mod & KMOD_LCTRL) == 0 &&
                        lctrlpressed &&
                        e.key.keysym.sym != SDLK_LCTRL) {
                    // Buggy on-screen keyboard, maybe.
                    lctrlpressed = 0;
                    lua_newtable(l);
                    lua_pushstring(l, "type");
                    lua_pushstring(l, "keyup");
                    lua_settable(l, -3);
                    lua_pushstring(l, "key");
                    lua_pushstring(l, "leftcontrol");
                    lua_settable(l, -3);
                    lua_rawseti(l, tbl_index, count + 1);
                    count++;
                }
                lua_newtable(l);
                lua_pushstring(l, "type");
                lua_pushstring(l, "keydown");
                lua_settable(l, -3);
                lua_pushstring(l, "key");
                lua_pushstring(l, key);
                lua_settable(l, -3);
                lua_rawseti(l, tbl_index, count + 1);
                count++;
            }
        } else if (e.type == SDL_KEYUP) {
            if (e.key.keysym.sym == SDLK_LCTRL)
                lctrlpressed = 0;
            else if (e.key.keysym.sym == SDLK_RCTRL)
                rctrlpressed = 0;
            char key[15] = "";
            ConvertSDLKeyToStr(e.key.keysym.sym, key);
            if (strlen(key) > 0) {
                lua_newtable(l);
                lua_pushstring(l, "type");
                lua_pushstring(l, "keyup");
                lua_settable(l, -3);
                lua_pushstring(l, "key");
                lua_pushstring(l, key);
                lua_settable(l, -3);
                lua_rawseti(l, tbl_index, count + 1);
                count++;
            }
        } else if (e.type == SDL_MOUSEMOTION) {
            if (e.motion.which == SDL_TOUCH_MOUSEID ||
                    global_mouse_disable) {
                continue;
            }
            if (relativemousemove &&
                    (!windowhasfocus ||
                     relativemousemovetemporarilydisabled))
                continue;
            lua_newtable(l);
            lua_pushstring(l, "type");
            lua_pushstring(l, "mousemove");
            lua_settable(l, -3);
            if (!relativemousemove) {
                lua_pushstring(l, "position");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(l, (double)e.motion.x * evdpimultiply);
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(l, (double)e.motion.y * evdpimultiply);
                lua_settable(l, -3);
                lua_settable(l, -3);
            } else {
                lua_pushstring(l, "position");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(l, relmousex);
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(l, relmousey);
                lua_settable(l, -3);
                lua_settable(l, -3);
                lua_pushstring(l, "movement");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(
                    l, (double)e.motion.xrel / (double)ww
                );
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(
                    l, (double)e.motion.yrel / (double)wh
                );
                lua_settable(l, -3);
                lua_settable(l, -3);
            }
            lua_rawseti(l, tbl_index, count + 1);
            count++;
        } else if (e.type == SDL_FINGERMOTION) {
            if (CountFingers() > 1 ||
                     e.tfinger.windowID == 0) {
                single_finger_canbeclick = 0;
                single_finger_isdrag = 0;
            }
            if (CountFingers() == 1) {
                if (e.tfinger.windowID != 0 &&
                        !multiple_fingers_on) {
                    single_finger_currentx = (
                        e.tfinger.x * (double)rfswindoww
                    );
                    single_finger_currenty = (
                        e.tfinger.y * (double)rfswindowh
                    );
                    double dist = fmax(
                        single_finger_currentx -
                        single_finger_startx,
                        single_finger_currenty -
                        single_finger_starty
                    );
                    if (dist > single_finger_maxclickmove) {
                        single_finger_canbeclick = 0;
                        single_finger_isdrag = 1;
                    }
                } else {
                    single_finger_canbeclick = 0;
                }
            }
        } else if (e.type == SDL_TEXTINPUT &&
                textinputactive) {
            lua_newtable(l);
            lua_pushstring(l, "type");
            lua_pushstring(l, "text");
            lua_settable(l, -3);
            lua_pushstring(l, "text");
            lua_pushstring(l, e.text.text);
            lua_settable(l, -3);
            lua_rawseti(l, tbl_index, count + 1);
            count++;
        } else if (e.type == SDL_FINGERUP) {
            #ifdef DEBUG_TOUCH
            fprintf(stderr,
                "rfsc/scriptcoreevents.c: debug: "
                "SDL_FINGERUP fingerid=%d "
                "windowID=%d x=%f y=%f "
                "fingercount=%d "
                "single_finger_down=%d "
                "single_finger_canbeclick=%d\n",
                (int)e.tfinger.fingerId,
                (int)e.tfinger.windowID,
                (e.tfinger.windowID == 0 ? -1.0 :
                 e.tfinger.x * (double)rfswindoww),
                (e.tfinger.windowID == 0 ? -1.0 :
                 e.tfinger.y * (double)rfswindowh),
                CountFingers(),
                single_finger_down,
                single_finger_canbeclick);
            #endif
            if (CountFingers() >= 1 ||
                    e.tfinger.fingerId != single_finger_fid ||
                    e.tfinger.windowID == 0) {
                single_finger_canbeclick = 0;
                multiple_fingers_on = (
                    CountFingers() > 1
                );
            }
            if (CountFingers() == 0 &&
                    single_finger_down &&
                    single_finger_canbeclick &&
                    e.tfinger.windowID != 0) {
                single_finger_currentx = (
                    e.tfinger.x * (double)rfswindoww
                );
                single_finger_currenty = (
                    e.tfinger.y * (double)rfswindowh
                );
                double dist = fmax(
                    single_finger_currentx -
                    single_finger_startx,
                    single_finger_currenty -
                    single_finger_starty
                );
                if (dist <= single_finger_maxclickmove &&
                        !relativemousemove &&
                        CountFingers() == 0) {
                    lua_newtable(l);
                    lua_pushstring(l, "type");
                    lua_pushstring(l, "click");
                    lua_settable(l, -3);
                    lua_pushstring(l, "button");
                    if ((int64_t)datetime_Ticks() -
                            single_finger_downms
                            < single_finger_maxleftclickms)
                        lua_pushstring(l,"left");
                    else
                        lua_pushstring(l, "right");
                    lua_settable(l, -3);
                    lua_pushstring(l, "position");
                    lua_newtable(l);
                    lua_pushnumber(l, 1);
                    lua_pushnumber(l, single_finger_currentx);
                    lua_settable(l, -3);
                    lua_pushnumber(l, 2);
                    lua_pushnumber(l, single_finger_currenty);
                    lua_settable(l, -3);
                    lua_settable(l, -3);
                    lua_pushstring(l, "touch");
                    lua_pushboolean(l, 1);
                    lua_settable(l, -3);
                    lua_rawseti(l, tbl_index, count + 1);
                    count++;
                }
            }
            if (CountFingers() == 0)
                single_finger_down = 0;
        } else if (e.type == SDL_FINGERDOWN) {
            #ifdef DEBUG_TOUCH
            fprintf(stderr,
                "rfsc/scriptcoreevents.c: debug: "
                "SDL_FINGERDOWN fingerid=%d "
                "windowID=%d x=%f y=%f "
                "fingercount=%d\n",
                (int)e.tfinger.fingerId,
                (int)e.tfinger.windowID,
                (e.tfinger.windowID == 0 ? -1.0 :
                 e.tfinger.x * (double)rfswindoww),
                (e.tfinger.windowID == 0 ? -1.0 :
                 e.tfinger.y * (double)rfswindowh),
                CountFingers());
            #endif
            if (CountFingers() > 1 ||
                     e.tfinger.windowID == 0) {
                single_finger_canbeclick = 0;
                multiple_fingers_on = 1;
                single_finger_isdrag = 0;
            }
            if (CountFingers() == 1) {
                single_finger_canbeclick = 0;
                if (!single_finger_down) {
                    single_finger_canbeclick = (
                        !multiple_fingers_on
                    );
                    single_finger_startx = (
                        e.tfinger.x * (double)rfswindoww
                    );
                    single_finger_starty = (
                        e.tfinger.y * (double)rfswindowh
                    );
                }
                single_finger_isdrag = 0;
                single_finger_down = 1;
                single_finger_fid = (int)e.tfinger.fingerId;
                single_finger_downms = datetime_Ticks();
                if (e.tfinger.windowID != 0) {
                    single_finger_currentx = (
                        e.tfinger.x * (double)rfswindoww
                    );
                    single_finger_currenty = (
                        e.tfinger.y * (double)rfswindowh
                    );
                } else {
                    single_finger_canbeclick = 0;
                }
            }
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            if (e.motion.which == SDL_TOUCH_MOUSEID ||
                    global_mouse_disable ||
                    (e.button.button != SDL_BUTTON_LEFT &&
                    e.button.button != SDL_BUTTON_RIGHT)) {
                continue;
            }
            if (relativemousemove &&
                    (!windowhasfocus ||
                     relativemousemovetemporarilydisabled)) {
                continue;
            }
            lua_newtable(l);
            lua_pushstring(l, "type");
            lua_pushstring(l, "mouseup");
            lua_settable(l, -3);
            lua_pushstring(l, "button");
            lua_pushstring(l,
                (e.button.button == SDL_BUTTON_LEFT ?
                "left" : "right")
            );
            lua_settable(l, -3);
            if (!relativemousemove) {
                lua_pushstring(l, "position");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(l, (double)e.button.x * evdpimultiply);
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(l, (double)e.button.y * evdpimultiply);
                lua_settable(l, -3);
                lua_settable(l, -3);
            } else {
                lua_pushstring(l, "position");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(l, relmousex);
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(l, relmousey);
                lua_settable(l, -3);
                lua_settable(l, -3);
            }
            lua_rawseti(l, tbl_index, count + 1);
            count++;
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.which == SDL_TOUCH_MOUSEID ||
                    global_mouse_disable ||
                    (e.button.button != SDL_BUTTON_LEFT &&
                    e.button.button != SDL_BUTTON_RIGHT)) {
                continue;
            }
            if (relativemousemove &&
                    (!windowhasfocus ||
                     relativemousemovetemporarilydisabled)) {
                if (windowhasfocus &&
                        relativemousemovetemporarilydisabled)
                    relativemousemovetemporarilydisabled = 0;
                continue;
            }
            lua_newtable(l);
            lua_pushstring(l, "type");
            lua_pushstring(l, "mousedown");
            lua_settable(l, -3);
            lua_pushstring(l, "button");
            lua_pushstring(l,
                (e.button.button == SDL_BUTTON_LEFT ?
                "left" : "right")
            );
            lua_settable(l, -3);
            if (!relativemousemove) {
                lua_pushstring(l, "position");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(l, (double)e.button.x * evdpimultiply);
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(l, (double)e.button.y * evdpimultiply);
                lua_settable(l, -3);
                lua_settable(l, -3);
            } else {
                lua_pushstring(l, "position");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(l, relmousex);
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(l, relmousey);
                lua_settable(l, -3);
                lua_settable(l, -3);
            }
            lua_rawseti(l, tbl_index, count + 1);
            count++;
            if (!relativemousemove) {
                lua_newtable(l);
                lua_pushstring(l, "type");
                lua_pushstring(l, "click");
                lua_settable(l, -3);
                lua_pushstring(l, "button");
                lua_pushstring(l,
                    (e.button.button == SDL_BUTTON_LEFT ?
                    "left" : "right")
                );
                lua_settable(l, -3);
                lua_pushstring(l, "position");
                lua_newtable(l);
                lua_pushnumber(l, 1);
                lua_pushnumber(l, (double)e.button.x * evdpimultiply);
                lua_settable(l, -3);
                lua_pushnumber(l, 2);
                lua_pushnumber(l, (double)e.button.y * evdpimultiply);
                lua_settable(l, -3);
                lua_settable(l, -3);
                lua_pushstring(l, "touch");
                lua_pushboolean(l, 0);
                lua_settable(l, -3);
                lua_rawseti(l, tbl_index, count + 1);
                count++;
            }
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                outputwindow_UpdateViewport(1);
                if (_script_last_seen_w != (int)e.window.data1 ||
                        _script_last_seen_h != (int)e.window.data2) {
                    lua_newtable(l);
                    lua_pushstring(l, "type");
                    lua_pushstring(l, "window_resize");
                    lua_settable(l, -3);
                    lua_pushstring(l, "width");
                    lua_pushnumber(l, (int)e.window.data1);
                    lua_settable(l, -3);
                    lua_pushstring(l, "height");
                    lua_pushnumber(l, (int)e.window.data2);
                    lua_settable(l, -3);
                    lua_rawseti(l, tbl_index, count + 1);
                    count++;
                    _script_last_seen_w = (int)e.window.data1;
                    _script_last_seen_h = (int)e.window.data2;
                }
            } else if (e.window.event ==
                    SDL_WINDOWEVENT_MINIMIZED) {
                windowhasfocus = 0;
                relativemousemovetemporarilydisabled = 1;
            } else if (e.window.event ==
                    SDL_WINDOWEVENT_FOCUS_LOST) {
                windowhasfocus = 0;
                relativemousemovetemporarilydisabled = 1;
            } else if (e.window.event ==
                    SDL_WINDOWEVENT_FOCUS_GAINED) {
                windowhasfocus = 1;
            }
        }
    }
    _updatemousemode();
    return 1;
}

static int _getevents(lua_State *l) {
    if (lua_gettop(l) != 0) {
        lua_pushstring(l, "expected 0 args");
        return lua_error(l);
    }
    int count = scriptcoreevents_Process(l);
    return count;
}

static int _disablehwmouse(ATTR_UNUSED lua_State *l) {
    global_mouse_disable = 1;
    return 0;
}

static int _events_starttextinput(ATTR_UNUSED lua_State *l) {
    if (textinputactive)
        return 0;
    textinputactive = 1;
    SDL_StartTextInput();
    return 0;
}

static int _events_stoptextinput(ATTR_UNUSED lua_State *l) {
    if (!textinputactive)
        return 0;
    textinputactive = 0;
    SDL_StopTextInput();
    return 0;
}

static int _events_ctrlpressed(lua_State *l) {
    lua_pushboolean(l, (lctrlpressed || rctrlpressed));
    return 1;
}

static int _os_clipboardpaste(lua_State *l) {
    char *p = SDL_GetClipboardText();
    if (p) {
        lua_pushstring(l, p);
        SDL_free(p);
        return 1;
    }
    lua_pushstring(l, "");
    return 1;
}

void scriptcoreevents_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _getevents);
    lua_setglobal(l, "_getevents");
    lua_pushcfunction(l, _disablehwmouse);
    lua_setglobal(l, "_disablehwmouse");
    lua_pushcfunction(l, _events_starttextinput);
    lua_setglobal(l, "_events_starttextinput");
    lua_pushcfunction(l, _events_stoptextinput);
    lua_setglobal(l, "_events_stoptextinput");
    lua_pushcfunction(l, _events_ctrlpressed);
    lua_setglobal(l, "_events_ctrlpressed");
    lua_pushcfunction(l, scriptcoreevents_iskeydown);
    lua_setglobal(l, "scriptcoreevents_iskeydown");
    lua_pushcfunction(l, _os_clipboardpaste);
    lua_setglobal(l, "_os_clipboardpaste");
}
