// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include <assert.h>
#include <errno.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


#include "filesys.h"
#include "luamem.h"
#include "scriptcore.h"
#include "scriptcoreerror.h"
#include "scriptcorefilesys.h"
#include "vfs.h"
#include "vfspak.h"


static int _filesys_setexecbit(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *executable = lua_tostring(l, 1);
    if (!executable) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int result = filesys_SetExecBit(executable);
    if (!result) {
        lua_pushstring(l, "failed to set exec bit");
        return lua_error(l);
    }
    return 0;
}


static int _filesys_launchexec(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1+ args of type string");
        return lua_error(l);
    }
    const char *executable = lua_tostring(l, 1);
    if (!executable) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int argc = 0;
    while (lua_gettop(l) >= argc + 2) {
        if (lua_type(l, argc + 2) != LUA_TSTRING) {
            lua_pushstring(l, "expected 1+ args of type string");
            return lua_error(l);
        }
        argc++;
    }
    char **argv = malloc(sizeof(*argv) * (argc + 1));
    if (!argv) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int i = 0;
    while (i < argc) {
        // XXX: intended const violation (we won't write to it):
        argv[i] = (char *)lua_tostring(l, i + 2);
        if (!argv[i]) {
            free(argv);
            lua_pushstring(l, "out of memory");
            return lua_error(l);
        }
        i++;
    }
    int result = filesys_LaunchExecutable(
        executable, argc, (const char **)argv
    );
    free(argv);
    if (!result) {
        lua_pushstring(l, "failed to launch executable");
        return lua_error(l);
    }
    return 0;
}


static int _filesys_currentdir(lua_State *l) {
    char *cd = filesys_GetCurrentDirectory();
    if (!cd) {
        lua_pushnil(l);
        lua_pushstring(l, "failed to get current directory");
        return 2;
    }
    lua_pushstring(l, cd);
    free(cd);
    return 1;
}

static int _filesys_documentsfolder(lua_State *l) {
    const char *p = filesys_DocumentsFolder();
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, p);
    return 1;
}

static int _filesys_createappdatasubfolder(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *appname = lua_tostring(l, 1);
    if (!appname) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char *p = filesys_AppDataSubFolder(appname);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, p);
    free(p);
    return 1;
}

static int _filesys_fclose(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FILE) {
        lua_pushstring(l, "expected arg of type file");
        return lua_error(l);
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    VFSFILE *f = (VFSFILE*)(uintptr_t)fref->value;
    fref->value = 0;
    if (!f) {
        lua_pushstring(l, "file already closed");
        return lua_error(l);
    }
    vfs_fclose(f);
    return 0;
}

static int _filesys_fseek(lua_State *l) {
    if (lua_gettop(l) < 3 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FILE ||
            lua_type(l, 2) != LUA_TSTRING ||
            lua_type(l, 3) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 3 args of type file, "
            "string, number");
        return lua_error(l);
    }
    const char *mode = lua_tostring(l, 2);
    if (!mode) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int64_t amount = lua_tointeger(l, 3);
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    VFSFILE *f = (VFSFILE*)(uintptr_t)fref->value;
    if (!f) {
        lua_pushstring(l, "file already closed");
        return lua_error(l);
    }
    if (strcmp(mode, "set") == 0) {
        if (amount < 0 || vfs_fseek(f, amount) != 0) {
            lua_pushstring(l, "seek failed");
            return lua_error(l);
        }
    } else if (strcmp(mode, "cur") == 0) {
        int64_t curr = vfs_ftell(f);
        if (curr < 0) {
            lua_pushstring(l, "seek failed");
            return lua_error(l);
        }
        if (curr + amount < 0 || vfs_fseek(f, curr + amount) != 0) {
            lua_pushstring(l, "seek failed");
            return lua_error(l);
        }
    } else if (strcmp(mode, "end") == 0) {
        int64_t curr = vfs_ftell(f);
        if (curr < 0) {
            lua_pushstring(l, "seek failed");
            return lua_error(l);
        }
        if (!vfs_fseektoend(f)) {
            lua_pushstring(l, "seek failed");
            return lua_error(l);
        }
        int64_t curr2 = vfs_ftell(f);
        if (curr2 < 0) {
            vfs_fseek(f, curr);
            lua_pushstring(l, "seek failed");
            return lua_error(l);
        }
        if (curr2 + amount < 0 || vfs_fseek(f, curr2 + amount) != 0) {
            vfs_fseek(f, curr);
            lua_pushstring(l, "seek failed");
            return lua_error(l);
        }
    } else {
        lua_pushstring(l, "unknown seek mode");
        return lua_error(l);
    }
    return 0;
}

static int _filesys_fwrite(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FILE ||
            lua_type(l, 2) != LUA_TSTRING) {
        lua_pushstring(l, "expected 2 args of type file, "
            "string");
        return lua_error(l);
    }
    size_t datalen;
    const char *data = lua_tolstring(l, 2, &datalen);
    if (!data) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    if (datalen == 0)
        return 0;
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    VFSFILE *f = (VFSFILE*)(uintptr_t)fref->value;
    if (!f) {
        lua_pushstring(l, "file already closed");
        return lua_error(l);
    }
    int64_t result = vfs_fwrite(data, 1, datalen, f);
    if (result != (int64_t)datalen) {
        lua_pushstring(l, "I/O error");
        return lua_error(l);
    }
    return 1;
}

static int _filesys_fread(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TUSERDATA ||
            ((scriptobjref*)lua_touserdata(l, 1))->magic !=
                OBJREFMAGIC ||
            ((scriptobjref*)lua_touserdata(l, 1))->type !=
                OBJREF_FILE ||
            (lua_type(l, 2) != LUA_TSTRING &&
            lua_type(l, 2) != LUA_TNUMBER)) {
        lua_pushstring(l, "expected 2 args of type file, "
            "and string or number");
        return lua_error(l);
    }
    int64_t amount = -1;
    if (lua_type(l, 2) == LUA_TSTRING) {
        const char *all = lua_tostring(l, 2);
        if (!all) {
            lua_pushstring(l, "out of memory");
            return lua_error(l);
        }
        if (!strcmp(all, "*all") == 0) {
            lua_pushstring(l, "expected string arg to "
                "be \"*all\"");
            return lua_error(l);
        }
    } else {
        amount = lua_tointeger(l, 2);
        if (amount < 0) {
            lua_pushstring(l, "invalid read amount");
            return lua_error(l);
        }
    }
    scriptobjref *fref = ((scriptobjref*)lua_touserdata(l, 1));
    VFSFILE *f = (VFSFILE*)(uintptr_t)fref->value;
    if (!f) {
        lua_pushstring(l, "file already closed");
        return lua_error(l);
    }

    int64_t resultlen = 1024;
    int64_t resultfill = 0;
    char *result = malloc(1024);
    if (!result) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    while (amount < 0 || amount > 0) {
        int64_t doread = 1024;
        if (amount > 0) {
            doread = amount;
            if (doread > 1024) doread = 1024;
        }
        char buf[1024];
        uint64_t didread = vfs_fread(
            buf, 1, doread, f
        );
        if (didread == 0 && errno == EIO) {
            free(result);
            lua_pushstring(l, "I/O error");
            return lua_error(l);
        }
        if ((int64_t)didread + resultfill > resultlen) {
            char *newresult = realloc(
                result, didread + resultfill
            );
            if (!newresult) {
                free(result);
                lua_pushstring(l, "out of memory");
                return lua_error(l);
            }
            result = newresult;
            resultlen += didread;
        }
        if (didread > 0)
            memcpy(result + resultfill, buf, didread);
        else
            break;
        resultfill += didread;
    }
    lua_pushlstring(l, result, resultfill);
    free(result);
    return 1;
}

static int _filesys_fopen(lua_State *l) {
    if (lua_gettop(l) < 3 || lua_type(l, 1) != LUA_TSTRING ||
            lua_type(l, 2) != LUA_TSTRING ||
            lua_type(l, 3) != LUA_TBOOLEAN) {
        lua_pushstring(l, "expected 3 args of types string, "
            "string, boolean");
        return lua_error(l);
    }
    const char *path = lua_tostring(l, 1);
    const char *mode = lua_tostring(l, 2);
    if (!path || !mode) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int ondisk = (lua_toboolean(l, 3) != 0);
    int flags = (ondisk ? VFSFLAG_NO_VIRTUALPAK_ACCESS :
        VFSFLAG_NO_REALDISK_ACCESS);
    VFSFILE *f = vfs_fopen(path, mode, flags);
    if (!f) {
        lua_pushstring(l, "failed to open file");
        return lua_error(l);
    }
    scriptobjref *sobj = lua_newuserdata(l, sizeof(*sobj));
    if (!sobj) {
        vfs_fclose(f);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    memset(sobj, 0, sizeof(*sobj));
    sobj->magic = OBJREFMAGIC;
    sobj->type = OBJREF_FILE;
    sobj->value = (int64_t)(uintptr_t)f;
    return 1;
}

static int _filesys_basename(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    char *s = (p ? filesys_Basename(p) : NULL);
    if (!s) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, s);
    free(s);
    return 1;
}


static int _filesys_binarypath(lua_State *l) {
    char *p = filesys_GetOwnExecutable();
    if (!p) {
        lua_pushstring(l, "I/O error or out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, p);
    free(p);
    return 1;
}

static int _filesys_abspath(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    char *s = (p ? filesys_ToAbsolutePath(p) : NULL);
    if (!s) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, s);
    free(s);
    return 1;
}

static int _filesys_dirname(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    char *s = (p ? filesys_Dirname(p) : NULL);
    if (!s) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, s);
    free(s);
    return 1;
}

static int _filesys_listdir(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    char **contents = NULL;
    int _err = 0;
    if (!filesys_ListFolder(
            lua_tostring(l, 1), &contents, 0, &_err)) {
        lua_pushstring(l, "failed to list directory");
        return lua_error(l);
    }
    lua_newtable(l);
    int i = 0;
    while (contents[i]) {
        lua_pushnumber(l, i + 1);
        lua_pushstring(l, contents[i]);
        lua_rawset(l, -3);
        i++;
    }
    filesys_FreeFolderList(contents);
    return 1;
}

static int _vfs_listdir(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    char **contents = NULL;
    int _err = 0;
    if (!vfs_ListFolder(
            lua_tostring(l, 1), &contents, 0,
            VFSFLAG_NO_REALDISK_ACCESS)) {
        lua_pushstring(l, "failed to list directory");
        return lua_error(l);
    }
    lua_newtable(l);
    int i = 0;
    while (contents[i]) { 
        lua_pushnumber(l, i + 1);
        lua_pushstring(l, contents[i]);
        lua_rawset(l, -3);
        i++;
    }
    vfs_FreeFolderList(contents);
    return 1;
}

static int _filesys_exists(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int result = 0;
    if (!vfs_Exists(p, &result,
            VFSFLAG_NO_VIRTUALPAK_ACCESS)) {
        lua_pushstring(l, "exists failed - out of memory?");
        return lua_error(l);
    }
    lua_pushboolean(l, result);
    return 1;
}

static int _vfs_exists(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int result = 0;
    if (!vfs_Exists(p, &result,
            VFSFLAG_NO_REALDISK_ACCESS)) {
        lua_pushstring(l, "exists failed - out of memory?");
        return lua_error(l);
    }
    lua_pushboolean(l, result);
    return 1;
}

static int _filesys_mkdir(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    const char *p = lua_tostring(l, 1);
    if (!p) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int result = filesys_CreateDirectory(p);
    if (!result) {
        lua_pushnil(l);
        lua_pushstring(l, "failed to create directory");
        return 2;
    }
    lua_pushboolean(l, 1);
    return 1;
}


static int _filesys_binarydir(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 0) {
        lua_pushstring(l, "expected 0 args");
        return lua_error(l);
    }
    char *exepath = filesys_GetOwnExecutable();
    if (!exepath) {
        lua_pushstring(l, "failed to get executable path");
        return lua_error(l);
    }
    char *exedir_possiblysymlink = filesys_ParentdirOfItem(exepath);
    free(exepath);
    if (!exedir_possiblysymlink) {
        lua_pushstring(l, "failed to get executable path");
        return lua_error(l);
    }
    char *exedir = filesys_GetRealPath(exedir_possiblysymlink);
    free(exedir_possiblysymlink);
    if (!exedir) {
        lua_pushstring(l, "failed to get executable path");
        return lua_error(l);
    }
    lua_pushstring(l, exedir);
    free(exedir);
    return 1;
}

static int _filesys_isdir(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    int result = 0;
    if (!vfs_IsDirectory(lua_tostring(l, 1), &result,
            VFSFLAG_NO_VIRTUALPAK_ACCESS)) {
        lua_pushstring(l, "failure in vfs_IsDirectory - out of memory?");
        return lua_error(l);
    }
    lua_pushboolean(l, result);
    return 1;
}

static int _vfs_isdir(lua_State *l) {
    int n = lua_gettop(l);
    if (n != 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    int result = 0;
    if (!vfs_IsDirectory(lua_tostring(l, 1), &result,
            VFSFLAG_NO_REALDISK_ACCESS)) {
        lua_pushstring(l, "failure in vfs_IsDirectory - out of memory?");
        return lua_error(l);
    }
    lua_pushboolean(l, result);
    return 1;
}

static int _vfs_removedatapak(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TNUMBER) {
        lua_pushstring(l, "expected 1 arg of type number");
        return lua_error(l);
    }
    uint64_t id = lua_tointeger(l, 1);
    if (!vfs_RemovePak(id)) {
        lua_pushstring(l, "failed to remove datapak");
        return lua_error(l);
    }
    return 0;
}

static int _vfs_adddatapak(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected 1 arg of type string");
        return lua_error(l);
    }
    char *pack = strdup(lua_tostring(l, 1));
    if (!pack) {
        lua_pushstring(l, "alloc failure");
        return lua_error(l);
    }
    if ((strlen(pack) < strlen(".rfspak") ||
            memcmp(pack + strlen(pack) - strlen(".rfspak"),
                   ".rfspak", strlen(".rfspak")) != 0) &&
            (strlen(pack) < strlen(".rfsprj") ||
             memcmp(pack + strlen(pack) - strlen(".rfsprj"),
                   ".rfsprj", strlen(".rfsprj")) != 0)) {
        free(pack);
        lua_pushstring(l, "only rfspak and rfsprj "
            "archives supported");
        return lua_error(l);
    }
    int _exists = 0;
    if (!filesys_TargetExists(pack, &_exists)) {
        free(pack);
        lua_pushstring(l, "I/O error or out of memory");
        return lua_error(l);
    }
    int _isdir = 0;
    if (_exists) {
        if (!filesys_IsDirectory(pack, &_isdir)) {
            free(pack);
            lua_pushstring(l, "given path is a directory");
            return lua_error(l);
        }
    } else {
        free(pack);
        lua_pushstring(l, "given file path not found");
        return lua_error(l);
    }
    uint64_t pakid = 0;
    if (!vfs_AddPak(pack, &pakid)) {
        free(pack);
        lua_pushstring(l, "failed to load rfspak/rfsprj -"
            " wrong format?");
        return lua_error(l);
    }
    free(pack);
    lua_pushinteger(l, pakid);
    return 0;
}

static int _vfs_package_require_module_loader(lua_State *l) {
    lua_settop(l, 0);
    lua_pushvalue(l, lua_upvalueindex(2));  // module name
    lua_pushvalue(l, lua_upvalueindex(1));  // lua source code
    const char *_contents_orig = lua_tostring(l, -1);
    char *contents = (
        _contents_orig ? strdup(_contents_orig) : NULL
    );
    if (!contents) {
        lua_pushstring(l, "failed to get contents from stack");
        return lua_error(l);
    }
    char *abspath = vfs_NormalizePath(lua_tostring(l, -1));
    if (!abspath || !luamem_EnsureFreePools(l)) {
        if (abspath)
            free(abspath);
        free(contents);
        lua_pushstring(l, "alloc fail");
        return lua_error(l);
    }
    char *newcontents = _prefix__file__(contents, abspath);
    free(abspath); free(contents);
    contents = newcontents;
    if (!contents) {
        lua_pushstring(l, "alloc fail");
        return lua_error(l);
    }
    int result = luaL_loadbuffer(
        l, contents, strlen(contents), lua_tostring(l, -2)
    );
    free(contents);
    if (result != 0) {
        char buf[512];
        snprintf(buf, sizeof(buf) - 1,
            "failed to parse code of module: %s", abspath
        );
        lua_pushstring(l, buf);
        return lua_error(l);
    }
    if (lua_gettop(l) > 1) {
        lua_replace(l, 1);
    }
    lua_settop(l, 1);
    lua_call(l, 0, 1);
    return 1;
}

static int _vfs_package_require_searcher(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        char buf[512];
        snprintf(
            buf, sizeof(buf) -1, 
            "expected #1 arg of type string, got #%d (#1 is type %d)",
            lua_gettop(l),
            (lua_gettop(l) >= 1 ? lua_type(l, 1) : -999)
        );
        lua_pushstring(l, buf);
        return lua_error(l);
    }

    // Get module path supplied to us:
    const char *modpath = lua_tostring(l, 1);
    char *path = malloc(strlen(modpath) + strlen(".lua") + 1);
    if (!path)
        return 0;
    unsigned int i = 0;
    while (i < strlen(modpath)) {
        if (modpath[i] == '.') {
            path[i] = '/';
        } else {
            path[i] = modpath[i];
        }
        i++;
    }
    memcpy(path + strlen(modpath), ".lua", strlen(".lua") + 1);

    // See if module exists and how large the file is:
    int result = 0;
    if (!vfs_Exists(path, &result, 0)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_Exists");
        return lua_error(l);
    }
    if (!result) {
        free(path);
        path = NULL;
        return 0;
    }
    if (!vfs_IsDirectory(path, &result, 0)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_IsDirectory");
        return lua_error(l);
    }
    if (result) {
        free(path);
        return 0;
    }
    uint64_t contentsize = 0;
    if (!vfs_Size(path, &contentsize, 0)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_Size");
        return lua_error(l);
    }
    if (contentsize <= 0) {
        free(path);
        return 0;
    }

    // Read contents:
    char *contents = malloc(contentsize + 1);
    if (!contents) {
        free(path);
        return 0;
    }
    if (!vfs_GetBytes(path, 0, contentsize, contents, 0)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_GetBytes");
        return lua_error(l);
    }
    contents[contentsize] = '\0';

    // Keep the cleaned up path:
    char *cleaned_path = vfs_NormalizePath(path);
    free(path);
    path = NULL;
    if (!cleaned_path) {
        free(contents);
        lua_pushstring(l, "path cleanup failure");
        return lua_error(l);
    }

    // Return loader function + name:
    lua_pushstring(l, contents);
    free(contents);
    lua_pushstring(l, cleaned_path);
    lua_pushcclosure(l, _vfs_package_require_module_loader, 2);
    lua_pushstring(l, cleaned_path);
    free(cleaned_path);
    assert(lua_type(l, -1) == LUA_TSTRING);
    assert(lua_type(l, -2) == LUA_TFUNCTION);
    return 2;
}

static int _filesys_normalize(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *origp = lua_tostring(l, 1);
    char *path = (
        origp ? filesys_Normalize(origp) : NULL
    );
    if (!path) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, path);
    free(path);
    return 1;
}

static int _filesys_join(lua_State *l) {
    if (lua_gettop(l) < 2 ||
            lua_type(l, 1) != LUA_TSTRING ||
            lua_type(l, 2) != LUA_TSTRING) {
        lua_pushstring(l, "expected 2+ args of type string");
        return lua_error(l);
    }
    const char *origp = lua_tostring(l, 1);
    char *sofar = (origp ? strdup(origp) : NULL);
    int i = 0;
    while (sofar && lua_gettop(l) >= i + 2 &&
            lua_type(l, i + 2) == LUA_TSTRING) {
        const char *newelement = lua_tostring(l, i + 2);
        if (!newelement) {
            free(sofar);
            sofar = NULL;
            break;
        }
        char *newsofar = (
            filesys_Join(sofar, newelement)
        );
        free(sofar);
        if (!newsofar) {
            sofar = NULL;
            break;
        }
        sofar = newsofar;
        i++;
    }
    if (!sofar) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, sofar);
    free(sofar);
    return 1;
}

static int _vfs_lua_loadordofile(lua_State *l, int runthecode) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    lua_settop(l, 1);
    char *path = filesys_Normalize(lua_tostring(l, 1));
    if (!path) {
        lua_pushstring(l, "alloc failure");
        return lua_error(l);
    }
    #if defined(DEBUG_VFS) && !defined(NDEBUG)
    fprintf(stderr,
        "rfsc/scriptcorefilesys.c: _vfs_lua_loadfile(\"%s\")\n",
        path
    );
    #endif
    int result;
    if (!vfs_Exists(path, &result, 0)) {
        free(path);
        lua_pushstring(l, "I/O error or out of memory");
        return lua_error(l);
    }
    if (!result) {
        char error[2048];
        snprintf(error, sizeof(error) - 1,
            "file not found: %s", path);
        free(path);
        lua_pushstring(l, error);
        return lua_error(l);
    }
    uint64_t contentsize = 0;
    if (!vfs_Size(path, &contentsize, 0)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_Size");
        return lua_error(l);
    }
    char *contents = malloc(contentsize + 1);
    if (!contents) {
        free(path);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    if (contentsize > 0 &&
            !vfs_GetBytes(path, 0, contentsize, contents, 0)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_GetBytes");
        return lua_error(l);
    }
    contents[contentsize] = '\0';
    char *abspath = vfs_NormalizePath(path);
    free(path);
    if (!abspath) {
        free(contents);
        lua_pushstring(l, "unexpected vfs absolute path alloc fail");
        return lua_error(l);
    }
    char *newcontents = _prefix__file__(
        contents, abspath
    );
    free(contents); contents = newcontents;
    if (!contents) {
        free(abspath); abspath = NULL;
        lua_pushstring(l, "unexpected vfs absolute path alloc fail");
        return lua_error(l);
    }
    result = luaL_loadbuffer(
        l, contents, strlen(contents), lua_tostring(l, -1)
    );
    free(contents);
    if (result != 0) {
        if (!runthecode) {
            free(abspath); abspath = NULL;
            lua_pushnil(l);
            lua_pushstring(l, "failed to parse module code");
            return 2;
        }
        char buf[512];
        snprintf(buf, sizeof(buf) - 1,
            "failed to parse code of module: %s", abspath
        );
        free(abspath); abspath = NULL;
        lua_pushstring(l, buf);
        return lua_error(l);
    }
    free(abspath); abspath = NULL;
    if (runthecode) {
        int belowfunctop = lua_gettop(l) - 1;
        lua_call(l, 0, LUA_MULTRET);
        return lua_gettop(l) - belowfunctop;
    } else {
        return 1;
    }
}

static int _vfs_lua_loadfile(lua_State *l) {
    return _vfs_lua_loadordofile(l, 0);
}

static int _vfs_lua_dofile(lua_State *l) {
    return _vfs_lua_loadordofile(l, 1);
}

static int _vfs_readvfsfile(lua_State *l) {
    if (lua_gettop(l) < 1 ||
            lua_type(l, 1) != LUA_TSTRING) {
        return 0;
    }
    lua_settop(l, 1);
    char *path = vfs_NormalizePath(lua_tostring(l, 1));
    if (!path) {
        lua_pushstring(l, "alloc failure");
        return lua_error(l);
    }
    int result;
    if (!result || (vfs_IsDirectory(
            path, &result, VFSFLAG_NO_REALDISK_ACCESS
            ) && result)) {
        free(path);
        return 0;
    }
    uint64_t contentsize = 0;
    if (!vfs_Size(path, &contentsize, VFSFLAG_NO_REALDISK_ACCESS)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_Size");
        return lua_error(l);
    }
    if (contentsize <= 0) {
        free(path);
        return 0;
    }
    char *contents = malloc(contentsize + 1);
    if (!contents) {
        free(path);
        return 0;
    }
    if (!vfs_GetBytes(path, 0, contentsize, contents,
                      VFSFLAG_NO_REALDISK_ACCESS)) {
        free(path);
        lua_pushstring(l, "unexpected failure in vfs_GetBytes");
        return lua_error(l);
    }
    contents[contentsize] = '\0';
    free(path);
    lua_pushstring(l, contents);
    free(contents);
    return 1;
}


void scriptcorefilesys_RegisterVFS(lua_State *l) {
    #if defined(DEBUG_VFS) && !defined(NDEBUG)
    printf("rfsc/scriptcorefilsys.c: debug: "
           "registering package loader for VFS\n");
    #endif
    const int previous_stack = lua_gettop(l);
    lua_getglobal(l, "package");
    lua_pushstring(l, "searchers");
    lua_gettable(l, -2);
    assert(lua_gettop(l) == previous_stack + 2);
    int searchercount = lua_rawlen(l, -1);
    int insertindex = 2;
    if (insertindex > searchercount + 1)
        insertindex = searchercount + 1;
    assert(lua_type(l, -1) == LUA_TTABLE);
    int k = searchercount + 1;
    while (k > insertindex && k > 1) {
        // Stack: package.searchers (-1)
        lua_pushnumber(l, k - 1);
        assert(lua_type(l, -2) == LUA_TTABLE);
        assert(lua_type(l, -1) == LUA_TNUMBER);
        lua_gettable(l, -2);
        assert(lua_type(l, -1) == LUA_TFUNCTION);
        assert(lua_gettop(l) == previous_stack + 3);
        // Stack: package.searchers (-2), package.searchers[k - 1] (-1)
        lua_pushnumber(l, k);
        lua_pushvalue(l, -2);
        assert(lua_gettop(l) == previous_stack + 5);
        assert(lua_type(l, -1) == LUA_TFUNCTION);
        // Stack: package.searchers (-4), package.searchers[k - 1] (-3),
        //        k, package.searchers[k - 1] (-2, -1)
        lua_settable(l, -4);  // sets package.searchers slot k to k - 1 value
        assert(lua_gettop(l) == previous_stack + 3);
        // Stack: package.searchers (-2), package.searchers[k - 1] (-1)
        lua_pop(l, 1);
        assert(lua_gettop(l) == previous_stack + 2);
        assert(lua_type(l, -1) == LUA_TTABLE);
        k--;
    }
    lua_pushnumber(l, insertindex);
    lua_pushcfunction(l, _vfs_package_require_searcher);
    lua_settable(l, -3);
    lua_settop(l, previous_stack);
}

static int _filesys_safetempfolder(lua_State *l) {
    char *filepath = NULL;
    char *folderpath = NULL;
    FILE *f = filesys_TempFile(
        1, 1, "RFS2", "", &folderpath, &filepath
    );
    assert(f == NULL && filepath == NULL);
    if (!folderpath) {
        lua_pushstring(l, "I/O error or out of memory");
        return lua_error(l);
    }
    lua_pushstring(l, folderpath);
    free(folderpath);
    return 1;
}

static int _filesys_copyfile(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TSTRING ||
            lua_type(l, 2) != LUA_TSTRING) {
        lua_pushstring(l, "expected 2 args of types string, string");
        return lua_error(l);
    }
    const char *src = lua_tostring(l, 1);
    const char *dst = lua_tostring(l, 2);
    if (!src || !dst) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char *dstnorm = filesys_Normalize(dst);
    if (!dstnorm) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    if (strlen(dstnorm) == 0 || strcmp(dstnorm, ".") == 0 ||
            strcmp(dstnorm, "./") == 0) {
        free(dstnorm);
        lua_pushstring(l, "target path exists");
        return lua_error(l);
    }
    int result = 0;
    if (!filesys_TargetExists(dstnorm, &result)) {
        free(dstnorm);
        lua_pushstring(l, "I/O error or out of memory");
        return lua_error(l);
    }
    if (result) {
        free(dstnorm);
        lua_pushstring(l, "target path exists");
        return lua_error(l);
    }
    int err = 0;
    FILE *srcf = filesys_OpenFromPath(
        src, "rb", &err
    );
    if (!srcf) {
        free(dstnorm);
        if (err == FS32_ERR_NOPERMISSION) {
            lua_pushstring(l, "permission denied");
            return lua_error(l);
        } else if (err == FS32_ERR_NOSUCHTARGET) {
            lua_pushstring(l, "source path doesn't exist");
            return lua_error(l);
        }
        lua_pushstring(l, "I/O error");
        return lua_error(l);
    }
    err = 0;
    FILE *destf = filesys_OpenFromPath(
        dstnorm, "wb", &err
    );
    free(dstnorm);
    if (!destf) {
        fclose(srcf);
        if (err == FS32_ERR_NOPERMISSION) {
            lua_pushstring(l, "permission denied");
            return lua_error(l);
        } else if (err == FS32_ERR_TARGETNOTAFILE) {
            lua_pushstring(l, "target path exists");
            return lua_error(l);
        }
        lua_pushstring(l, "I/O error");
        return lua_error(l);
    }
    while (!feof(srcf)) {
        char buf[512];
        int64_t bytes = fread(buf, 1, sizeof(buf), srcf);
        if (bytes <= 0) {
            if (feof(srcf)) {
                break;
            }
            fclose(srcf);
            fclose(destf);
            lua_pushstring(l, "I/O error");
            return lua_error(l);
        }
        int64_t bytesw = fwrite(buf, 1, bytes, destf);
        if (bytesw != bytes) {
            fclose(srcf);
            fclose(destf);
            lua_pushstring(l, "I/O error");
            return lua_error(l);
        }
    }
    fclose(srcf);
    fclose(destf);
    return 0;
}
 
static int _filesys_move(lua_State *l) {
    if (lua_gettop(l) < 2 || lua_type(l, 1) != LUA_TSTRING ||
            lua_type(l, 2) != LUA_TSTRING) {
        lua_pushstring(l, "expected 2 args of types string, string");
        return lua_error(l);
    }
    const char *src = lua_tostring(l, 1);
    const char *dst = lua_tostring(l, 2);
    if (!src || !dst) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    char *dstnorm = filesys_Normalize(dst);
    if (!dstnorm) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    if (strlen(dstnorm) == 0 || strcmp(dstnorm, ".") == 0 ||
            strcmp(dstnorm, "./") == 0) {
        free(dstnorm);
        lua_pushstring(l, "target path exists");
        return lua_error(l);
    }
    int existresult = 0;
    if (!filesys_TargetExists(dstnorm, &existresult)) {
        free(dstnorm);
        lua_pushstring(l, "I/O error or out of memory");
        return lua_error(l);
    }
    if (existresult) {
        free(dstnorm);
        lua_pushstring(l, "target path exists");
        return lua_error(l);
    }
    #if defined(_WIN32) || defined(_WIN64)
    wchar_t *srcu16 = AS_U16(src);
    wchar_t *dstu16 = AS_U16(dstnorm);
    free(dstnorm);
    if (!srcu16 || !dstu16) {
        free(srcu16);
        free(dstu16);
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    BOOL result = MoveFileW(srcu16, dstu16);
    free(srcu16);
    free(dstu16);
    if (result == 0) {
        uint32_t werror = GetLastError();
        if (werror == ERROR_FILE_NOT_FOUND) {
            lua_pushstring(l, "source path doesn't exist");
            return lua_error(l);
        } else if (werror == ERROR_ACCESS_DENIED) {
            lua_pushstring(l, "permission denied");
            return lua_error(l);
        } else if (werror == ERROR_FILE_EXISTS) {
            lua_pushstring(l, "target path exists");
            return lua_error(l);
        }
        lua_pushstring(l, "I/O error");
        return lua_error(l);
    }
    #else
    if (rename(src, dstnorm) != 0) {
        free(dstnorm);
        if (errno == EACCES || errno == EPERM) {
            lua_pushstring(l, "permission denied");
            return lua_error(l);
        } else if (errno == EEXIST || errno == ENOTEMPTY) {
            lua_pushstring(l, "target path exists");
            return lua_error(l);
        } else if (errno == ENOTDIR) {
            lua_pushstring(l, "source path doesn't exist");
            return lua_error(l);
        }
        lua_pushstring(l, "I/O error");
        return lua_error(l);
    }
    free(dstnorm);
    #endif
    return 0;
}

static int _filesys_removefolderrecursively(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *dst = lua_tostring(l, 1);
    if (!dst) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int err = 0;
    if (!filesys_RemoveFolderRecursively(dst, &err)) {
        if (err == FS32_ERR_NOPERMISSION) {
            lua_pushstring(l, "permission denied");
            return lua_error(l);
        } else if (errno == FS32_ERR_NOSUCHTARGET) {
            lua_pushstring(l, "target doesn't exist");
            return lua_error(l);
        } else if (err == FS32_ERR_TARGETNOTADIRECTORY) {
            lua_pushstring(l, "target is not a folder");
            return lua_error(l);
        }
        lua_pushstring(l, "I/O error");
        return lua_error(l);
    }
    return 0;
}


static int _filesys_removefileoremptyfolder(lua_State *l) {
    if (lua_gettop(l) < 1 || lua_type(l, 1) != LUA_TSTRING) {
        lua_pushstring(l, "expected arg of type string");
        return lua_error(l);
    }
    const char *dst = lua_tostring(l, 1);
    if (!dst) {
        lua_pushstring(l, "out of memory");
        return lua_error(l);
    }
    int err = 0;
    if (!filesys_RemoveFileOrEmptyDir(dst, &err)) {
        if (err == FS32_ERR_NOPERMISSION) {
            lua_pushstring(l, "permission denied");
            return lua_error(l);
        } else if (errno == FS32_ERR_NOSUCHTARGET) {
            lua_pushstring(l, "target doesn't exist");
            return lua_error(l);
        }
        lua_pushstring(l, "I/O error");
        return lua_error(l);
    }
    return 0;
}

void scriptcorefilesys_AddFunctions(lua_State *l) {
    lua_pushcfunction(l, _vfs_adddatapak);
    lua_setglobal(l, "_vfs_adddatapak");
    lua_pushcfunction(l, _vfs_removedatapak);
    lua_setglobal(l, "_vfs_removedatapak");
    lua_pushcfunction(l, _filesys_currentdir);
    lua_setglobal(l, "_filesys_currentdir");
    lua_pushcfunction(l, _filesys_isdir);
    lua_setglobal(l, "_filesys_isdir");
    lua_pushcfunction(l, _filesys_exists);
    lua_setglobal(l, "_filesys_exists");
    lua_pushcfunction(l, _vfs_isdir);
    lua_setglobal(l, "_vfs_isdir");
    lua_pushcfunction(l, _vfs_exists);
    lua_setglobal(l, "_vfs_exists");
    lua_pushcfunction(l, _filesys_mkdir);
    lua_setglobal(l, "_filesys_mkdir");
    lua_pushcfunction(l, _filesys_binarydir);
    lua_setglobal(l, "_filesys_binarydir");
    lua_pushcfunction(l, _vfs_lua_loadfile);
    lua_setglobal(l, "_vfs_lua_loadfile");
    lua_pushcfunction(l, _vfs_lua_loadfile);
    lua_setglobal(l, "loadfile");
    lua_pushcfunction(l, _vfs_readvfsfile);
    lua_setglobal(l, "_vfs_readvfsfile");
    lua_pushcfunction(l, _filesys_basename);
    lua_setglobal(l, "_filesys_basename");
    lua_pushcfunction(l, _filesys_dirname);
    lua_setglobal(l, "_filesys_dirname");
    lua_pushcfunction(l, _vfs_lua_dofile);
    lua_setglobal(l, "_vfs_lua_dofile");
    lua_pushcfunction(l, _vfs_lua_dofile);
    lua_setglobal(l, "dofile");
    lua_pushcfunction(l, _filesys_fopen);
    lua_setglobal(l, "_filesys_fopen");
    lua_pushcfunction(l, _filesys_fread);
    lua_setglobal(l, "_filesys_fread");
    lua_pushcfunction(l, _filesys_fclose);
    lua_setglobal(l, "_filesys_fclose");
    lua_pushcfunction(l, _filesys_fwrite);
    lua_setglobal(l, "_filesys_fwrite");
    lua_pushcfunction(l, _filesys_fseek);
    lua_setglobal(l, "_filesys_fseek");
    lua_pushcfunction(l, _filesys_normalize);
    lua_setglobal(l, "_filesys_normalize");
    lua_pushcfunction(l, _filesys_listdir);
    lua_setglobal(l, "_filesys_listdir");
    lua_pushcfunction(l, _vfs_listdir);
    lua_setglobal(l, "_vfs_listdir");
    lua_pushcfunction(l, _filesys_safetempfolder);
    lua_setglobal(l, "_filesys_safetempfolder");
    lua_pushcfunction(l, _filesys_copyfile);
    lua_setglobal(l, "_filesys_copyfile");
    lua_pushcfunction(l, _filesys_move);
    lua_setglobal(l, "_filesys_move");
    lua_pushcfunction(l, _filesys_removefileoremptyfolder);
    lua_setglobal(l, "_filesys_removefileoremptyfolder");
    lua_pushcfunction(l, _filesys_removefolderrecursively);
    lua_setglobal(l, "_filesys_removefolderrecursively");
    lua_pushcfunction(l, _filesys_join);
    lua_setglobal(l, "_filesys_join");
    lua_pushcfunction(l, _filesys_documentsfolder);
    lua_setglobal(l, "_filesys_documentsfolder");
    lua_pushcfunction(l, _filesys_createappdatasubfolder);
    lua_setglobal(l, "_filesys_createappdatasubfolder");
    lua_pushcfunction(l, _filesys_binarypath);
    lua_setglobal(l, "_filesys_binarypath");
    lua_pushcfunction(l, _filesys_launchexec);
    lua_setglobal(l, "_filesys_launchexec");
    lua_pushcfunction(l, _filesys_setexecbit);
    lua_setglobal(l, "_filesys_setexecbit");
    lua_pushcfunction(l, _filesys_abspath);
    lua_setglobal(l, "_filesys_abspath");
}
