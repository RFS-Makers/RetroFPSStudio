-- This is PROPRIETARY CODE, do not modify, reuse, or share.
-- All Rights Reserved.
-- Reading this code for personal education and curiosity is ENCOURAGED!
-- See LICENSE.md for details

if rfs == nil then rfs = {} end
if rfs.vfs == nil then rfs.vfs = {} end

rfs.vfs.fileclass = {}

rfs.vfs.fileclass.read = function(self, amount)
    if amount == nil or amount == "*line" then
        local result = ""
        while true do
            local f = self
            local c = _filesys_fread(f, 1)
            result = result .. c
            if c == "\n" then
                break
            end
            if c == "\r" then
                c = _filesys_fread(f, 1)
                if c ~= "\n" then
                    if #c == 1 then
                        _filesys_fseek(f, "cur", -1)
                    end
                else
                    result = result .. c
                end
                break
            end
        end
        return result
    end
    local f = self
    return _filesys_fread(f, amount)
end

rfs.vfs.fileclass.write = function(self, data)
    local f = self
    return _filesys_fwrite(f, data)
end

rfs.vfs.fileclass.seek = function(self, mode, offset)
    if type(mode) == "number" and offset == nil then
        offset = mode
        mode = nil
    end
    if mode == nil then mode = "cur" end
    if offset == nil then offset = 0 end
    local f = self
    return _filesys_fseek(f, mode, offset)
end

rfs.vfs.fileclass.close = function(self)
    local f = self
    return _filesys_fclose(f, amount)
end

io.open = function(fname, mode)
    if mode == nil then
        mode = "r"
    end
    local result = (
        _filesys_fopen(fname, mode, true)
    )
    debug.setmetatable(result, {
        __index = rfs.vfs.fileclass,
        __gc = function(self)
            local f = function(gcself)
                _filesys_fclose(f, gcself)
            end
            pcall(f, self)
        end
    })
    return result
end

rfs.vfs.open = function(fname, mode)
    if mode == nil then
        mode = "r"
    end
    local result = (
        _filesys_fopen(fname, mode, false)
    )
    debug.setmetatable(result, {
        __index = rfs.vfs.fileclass,
        __gc = function(self)
            function f(gcself)
                _filesys_fclose(f, gcself)
            end
            pcall(f, self)
        end
    })
    return result
end

rfs.vfs.exists = _vfs_exists
rfs.vfs.isdir = _vfs_isdir
rfs.vfs.listdir = _vfs_listdir
rfs.vfs.mount = _vfs_adddatapak
rfs.vfs.unmount = _vfs_removedatapak
os.pathjoin = _filesys_join
os.abspath = _filesys_abspath
os.dirname = _filesys_dirname
os.basename = _filesys_basename
os.normpath = _filesys_normalize
os.setexecbit = _filesys_setexecbit
os.run = _filesys_launchexec
os.exists = _filesys_exists
os.isdir = _filesys_isdir
os.binarypath = _filesys_binarypath
os.documents_folder = _filesys_documentsfolder
os.make_appdata_folder = _filesys_createappdatasubfolder
os.mkdir = _filesys_mkdir
os.listdir = _filesys_listdir
os.copyfile = _filesys_copyfile
os.move = _filesys_move
os.remove = _filesys_removefileoremptyfolder
os.removetree = _filesys_removefolderrecursively
os.makesafetmpfolder = _filesys_safetempfolder
io.read = nil
io.write = nil
io.input = nil
io.output = nil
