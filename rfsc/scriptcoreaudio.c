// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio/audio.h"
#include "audio/audiodecodedfile.h"
#include "roomdefs.h"
#include "scriptcore.h"
#include "scriptcoreaudio.h"
#include "scriptcoreerror.h"
#include "vfs.h"

static h3daudiodevice **_opendevices = NULL;
static int _opendevicescount = 0;


int _h3daudio_listsoundcards(lua_State *l) {
    int backend = H3DAUDIO_BACKEND_SDL2;
    if (lua_gettop(l) >= 1 &&
            lua_type(l, 1) == LUA_TBOOLEAN &&
            lua_toboolean(l, 1)) {
        backend = H3DAUDIO_BACKEND_SDL2_EXCLUSIVELOWLATENCY;
    }
    int count = h3daudio_GetDeviceSoundcardCount(backend);
    int returnedcount = 0;
    lua_newtable(l);
    if (count <= 0) {
        lua_pushnumber(l, 1);
        lua_pushstring(l, "default unknown device");
        lua_settable(l, -3);
        return 1;
    }
    int i = 0;
    while (i < count) {
        char *s = h3daudio_GetDeviceSoundcardName(backend, i);
        if (!s) {
            lua_pushstring(l, "out of memory");
            return lua_error(l);
        }
        returnedcount++;
        lua_pushnumber(l, i + 1);
        lua_pushstring(l, s);
        lua_settable(l, -3);
        free(s);
        i++;
    }
    return 1;
}


int _h3daudio_destroypreloadedsound(lua_State *l) {
    if (lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PRELOADEDSOUND) {
        lua_pushstring(
            l, "expected 1 arg of type preloadedsound");
        return lua_error(l);
    }
    h3daudiodecodedfile *f = (h3daudiodecodedfile *)(
        (uintptr_t)((scriptobjref *)lua_touserdata(l, 1))->value);
    if (!f) {
        lua_pushstring(l, "couldn't access preloadedsound - "
            "was it destroyed?");
        return lua_error(l);
    }
    ((scriptobjref *)lua_touserdata(l, 1))->value = 0;
    audiodecodedfile_Free(f);
    return 0;
}


int _h3daudio_preloadsound(lua_State *l) {
    if (lua_gettop(l) < 2 ||
            lua_type(l, 2) != LUA_TSTRING ||
            lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_AUDIODEVICE) {
        lua_pushstring(
            l, "expected 2-5 arguments of types "
            "audiodevice, string"
        );
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!dev) {
        lua_pushstring(l, "couldn't access device - was it closed?");
        return lua_error(l);
    }
    const char *soundpath = lua_tostring(l, 2);
    if (!soundpath) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char *errmsg = NULL;
    h3daudiodecodedfile *f = audiodecodedfile_LoadEx(
        soundpath, 0, h3daudio_GetDeviceSampleRate(dev),
        MAX_SOUND_SECONDS + 0.5, &errmsg);
    if (!f) {
        char buf[512];
        snprintf(buf, sizeof(buf) - 1,
            "failed to preload sound: %s", (errmsg ? errmsg :
            "(unknown error)"));
        lua_pushstring(l, buf);
        return lua_error(l);
    }
    if (audiodecodedfile_GetLengthInSeconds(f) >
            MAX_SOUND_SECONDS) {
        audiodecodedfile_Free(f);
        lua_pushstring(l, "sound file is too long");
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        audiodecodedfile_Free(f);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_PRELOADEDSOUND;
    ref->value = (uintptr_t)f;
    return 1;
}


int _h3daudio_getdevicename(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TUSERDATA) {
        lua_pushstring(l, "expected arg #1 to be "
                       "audiodevice");
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_AUDIODEVICE) {
        lua_pushstring(l, "expected arg #1 to be audiodevice");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!dev) {
        lua_pushstring(l, "couldn't access device - was it closed?");
        return lua_error(l);
    }
    lua_pushstring(l, h3daudio_GetDeviceName(dev));
    return 1;
}


int _h3daudio_opendevice(lua_State *l) {
    int backend = H3DAUDIO_BACKEND_SDL2;
    const char *devicename = NULL;
    if (lua_gettop(l) >= 1 &&
            lua_type(l, 1) == LUA_TBOOLEAN) {
        if (lua_toboolean(l, 1))
            backend = H3DAUDIO_BACKEND_SDL2_EXCLUSIVELOWLATENCY;
        if (lua_gettop(l) >= 2 &&
                lua_type(l, 2) == LUA_TSTRING) {
            devicename = lua_tostring(l, 2);
        }
    } else if (lua_gettop(l) == 1 &&
            lua_type(l, 1) == LUA_TSTRING) {
        devicename = lua_tostring(l, 1);
    }

    h3daudiodevice **newopendevices = realloc(
        _opendevices,
        sizeof(*_opendevices) * (_opendevicescount + 1)
    );
    if (!newopendevices) {
        lua_pushstring(l, "failed to allocate devices list");
        return lua_error(l);
    }
    _opendevices = newopendevices;

    char *error = NULL;
    h3daudiodevice *dev = h3daudio_OpenDeviceEx(
        48000, 2048, backend, devicename, &error
    );
    if (!dev) {
        if (error) {
            char buf[512];
            snprintf(
                buf, sizeof(buf) - 1,
                "failed to open device: %s", error
            );
            lua_pushstring(l, buf);
            free(error);
        } else {
            lua_pushstring(l, "failed to open device: unknown error");
        }
        return lua_error(l);
    }
    _opendevices[_opendevicescount] = dev;
    _opendevicescount++;

    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    if (!ref) {
        _opendevicescount--;
        h3daudio_DestroyDevice(_opendevices[_opendevicescount]);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_AUDIODEVICE;
    ref->value = h3daudio_GetDeviceId(dev);
    return 1;
}


int _h3daudio_destroydevice(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_AUDIODEVICE) {
        lua_pushstring(l, "expected arg of type audiodevice");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!dev) {
        lua_pushstring(l, "couldn't access device - was it closed?");
        return lua_error(l);
    }
    int i = 0;
    while (i < _opendevicescount) {
        if (_opendevices[i] == dev) {
            if (i + 1 < _opendevicescount)
                memcpy(&_opendevices[i],
                    &_opendevices[i + 1],
                    sizeof(*_opendevices) *
                    (_opendevicescount - i - 1));
            _opendevicescount--;
            continue;
        }
        i++;
    }
    ((scriptobjref*)lua_touserdata(l, 1))->value = -1;
    h3daudio_DestroyDevice(dev);
    return 0;
}


int _h3daudio_playsound(lua_State *l) {
    if (lua_gettop(l) < 2 ||
            (lua_type(l, 2) != LUA_TSTRING && (
             lua_type(l, 2) != LUA_TUSERDATA ||
             ((scriptobjref*)lua_touserdata(l, 2))->magic !=
             OBJREFMAGIC ||
             ((scriptobjref*)lua_touserdata(l, 2))->type !=
             OBJREF_PRELOADEDSOUND ||
             ((scriptobjref*)lua_touserdata(l, 2))->value ==
             0)) ||
            lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_AUDIODEVICE
            ) {
        lua_pushstring(
            l, "expected 2-5 arguments of types "
            "audiodevice, string or preloadedsound, "
            "number, number, boolean"
        );
        return lua_error(l);
    }
    h3daudiodecodedfile *soundref = NULL;
    const char *soundpath = NULL;
    if (lua_type(l, 2) == LUA_TSTRING) {
        const char *soundpath_unfixed = lua_tostring(l, 2);
        soundpath = (soundpath_unfixed ? audio_FixSfxPath(
            soundpath_unfixed) : NULL);
        int existsresult = 0;
        if (!vfs_Exists(soundpath, &existsresult, 0)) {
            lua_pushstring(l, "vfs_Exists() failed - out of memory?");
            return lua_error(l);
        }
        if (!existsresult) {
            char buf[512];
            snprintf(buf, sizeof(buf) - 1,
                     "sound file not found: %s", soundpath);
            lua_pushstring(l, buf);
            return lua_error(l);
        }
    } else {
        assert(lua_type(l, 2) == LUA_TUSERDATA);
        soundref = (h3daudiodecodedfile *)(
            (uintptr_t)((scriptobjref *)
            lua_touserdata(l, 2))->value
        );
        assert(soundref != NULL);
    }
    double volume = (
        (lua_gettop(l) >= 3 && lua_type(l, 3) == LUA_TNUMBER) ?
        lua_tonumber(l, 3) : 1.0
    );
    double panning = (
        (lua_gettop(l) >= 4 && lua_type(l, 4) == LUA_TNUMBER) ?
        lua_tonumber(l, 4) : 0.0
    );
    int loop = (
        (lua_gettop(l) >= 5 && lua_type(l, 5) == LUA_TBOOLEAN) ?
        lua_toboolean(l, 5) : 0
    );
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value
    );
    if (!dev) {
        lua_pushstring(l, "couldn't access device - was it closed?");
        return lua_error(l);
    }
    volume = fmax(0, fmin(1, volume));
    panning = fmax(-1, fmin(1, panning));
    uint64_t soundid = 0;
    if (soundref == NULL) {
        soundid = h3daudio_PlayStreamedSoundFromFile(
            dev, soundpath, volume, panning, loop
        );
    } else {
        soundid = h3daudio_PlayPredecodedSound(
            dev, soundref, volume, panning, loop
        );
    }
    if (soundid == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf) - 1,
                 "failed to play sound: %s", soundpath);
        lua_pushstring(l, buf);
        return lua_error(l);
    }
    scriptobjref *ref = lua_newuserdata(l, sizeof(*ref));
    memset(ref, 0, sizeof(*ref));
    ref->magic = OBJREFMAGIC;
    ref->type = OBJREF_PLAYINGSOUND;
    ref->value = soundid;
    ref->value2 = h3daudio_GetDeviceId(dev);
    return 1;
}


int _h3daudio_stopsound(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TUSERDATA) {
        lua_pushstring(
            l, "expected arg #1 to be "
            "sound"
        );
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PLAYINGSOUND) {
        lua_pushstring(l, "expected arg #1 to be sound");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value2
    );
    if (!dev) {
        // Device is already gone, so the sound is too. Do nothing.
        return 0;
    }
    uint64_t soundid = ((scriptobjref*)lua_touserdata(l, 1))->value;
    h3daudio_StopSound(dev, soundid);
    return 0;
}


int _h3daudio_soundsetvolume(lua_State *l) {
    if (lua_gettop(l) < 2 ||
            lua_type(l, 1) != LUA_TUSERDATA ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(
            l, "expected args of types "
            "sound, number"
        );
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PLAYINGSOUND) {
        lua_pushstring(l, "expected arg #1 to be sound");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value2
    );
    if (!dev) {
        lua_pushboolean(l, 0);
        return 1;
    }
    uint64_t soundid = ((scriptobjref*)lua_touserdata(l, 1))->value;
    double volume = 0.7;
    double panning = 0;
    h3daudio_GetSoundVolume(dev, soundid, &volume, &panning);
    volume = fmax(0, fmin(1.0, lua_tonumber(l, 2)));
    h3daudio_ChangeSoundVolume(dev, soundid, volume, panning);
    return 0;
}


int _h3daudio_soundsetpanning(lua_State *l) {
    if (lua_gettop(l) < 2 ||
            lua_type(l, 1) != LUA_TUSERDATA ||
            lua_type(l, 2) != LUA_TNUMBER) {
        lua_pushstring(
            l, "expected args of types "
            "sound, number"
        );
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PLAYINGSOUND) {
        lua_pushstring(l, "expected arg #1 to be sound");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value2
    );
    if (!dev) {
        lua_pushboolean(l, 0);
        return 1;
    }
    uint64_t soundid = ((scriptobjref*)lua_touserdata(l, 1))->value;
    double volume = 0.7;
    double panning = 0;
    h3daudio_GetSoundVolume(dev, soundid, &volume, &panning);
    panning = fmax(-1.0, fmin(1.0, lua_tonumber(l, 2)));
    h3daudio_ChangeSoundVolume(dev, soundid, volume, panning);
    return 0;
}


int _h3daudio_soundisplaying(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TUSERDATA) {
        lua_pushstring(
            l, "expected arg #1 to be "
            "sound"
        );
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PLAYINGSOUND) {
        lua_pushstring(l, "expected arg #1 to be sound");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value2
    );
    if (!dev) {
        lua_pushboolean(l, 0);
        return 1;
    }
    uint64_t soundid = ((scriptobjref*)lua_touserdata(l, 1))->value;
    lua_pushboolean(l, h3daudio_IsSoundPlaying(dev, soundid));
    return 1;
}


int _h3daudio_soundhaderror(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TUSERDATA) {
        lua_pushstring(
            l, "expected arg #1 to be "
            "sound"
        );
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PLAYINGSOUND) {
        lua_pushstring(l, "expected arg #1 to be sound");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value2
    );
    if (!dev) {
        lua_pushboolean(l, 0);
        return 1;
    }
    uint64_t soundid = ((scriptobjref*)lua_touserdata(l, 1))->value;
    lua_pushboolean(l, h3daudio_SoundHadPlaybackError(dev, soundid));
    return 1;
}


int _h3daudio_soundgetvolume(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TUSERDATA) {
        lua_pushstring(
            l, "expected arg #1 to be "
            "sound"
        );
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PLAYINGSOUND) {
        lua_pushstring(l, "expected arg #1 to be sound");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value2
    );
    if (!dev) {
        lua_pushnumber(l, 0);
        return 1;
    }
    uint64_t soundid = ((scriptobjref*)lua_touserdata(l, 1))->value;
    double volume = 0.0;
    double panning = 0;
    h3daudio_GetSoundVolume(dev, soundid, &volume, &panning);
    lua_pushnumber(l, volume);
    return 1;
}


int _h3daudio_soundgetpanning(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TUSERDATA) {
        lua_pushstring(
            l, "expected arg #1 to be "
            "sound"
        );
        return lua_error(l);
    }
    if (((scriptobjref*)lua_touserdata(l, 1))->magic !=
            OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
            OBJREF_PLAYINGSOUND) {
        lua_pushstring(l, "expected arg #1 to be sound");
        return lua_error(l);
    }
    h3daudiodevice *dev = h3daudio_GetDeviceById(
        (int)((scriptobjref*)lua_touserdata(l, 1))->value2
    );
    if (!dev) {
        lua_pushnumber(l, 0);
        return 1;
    }
    uint64_t soundid = ((scriptobjref*)lua_touserdata(l, 1))->value;
    double volume = 0.0;
    double panning = 0;
    h3daudio_GetSoundVolume(dev, soundid, &volume, &panning);
    lua_pushnumber(l, panning);
    return 1;
}


void scriptcoreaudio_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _h3daudio_opendevice);
    lua_setglobal(l, "_h3daudio_opendevice");
    lua_pushcfunction(l, _h3daudio_destroydevice);
    lua_setglobal(l, "_h3daudio_destroydevice");
    lua_pushcfunction(l, _h3daudio_listsoundcards);
    lua_setglobal(l, "_h3daudio_listsoundcards");
    lua_pushcfunction(l, _h3daudio_getdevicename);
    lua_setglobal(l, "_h3daudio_getdevicename");
    lua_pushcfunction(l, _h3daudio_playsound);
    lua_setglobal(l, "_h3daudio_playsound");
    lua_pushcfunction(l, _h3daudio_stopsound);
    lua_setglobal(l, "_h3daudio_stopsound");
    lua_pushcfunction(l, _h3daudio_soundsetpanning);
    lua_setglobal(l, "_h3daudio_soundsetpanning");
    lua_pushcfunction(l, _h3daudio_soundsetvolume);
    lua_setglobal(l, "_h3daudio_soundsetvolume");
    lua_pushcfunction(l, _h3daudio_soundgetpanning);
    lua_setglobal(l, "_h3daudio_soundgetpanning");
    lua_pushcfunction(l, _h3daudio_soundgetvolume);
    lua_setglobal(l, "_h3daudio_soundgetvolume");
    lua_pushcfunction(l, _h3daudio_soundisplaying);
    lua_setglobal(l, "_h3daudio_soundisplaying");
    lua_pushcfunction(l, _h3daudio_soundhaderror);
    lua_setglobal(l, "_h3daudio_soundhaderror");
    lua_pushcfunction(l, _h3daudio_destroypreloadedsound);
    lua_setglobal(l, "_h3daudio_destroypreloadedsound");
    lua_pushcfunction(l, _h3daudio_preloadsound);
    lua_setglobal(l, "_h3daudio_preloadsound");
}
