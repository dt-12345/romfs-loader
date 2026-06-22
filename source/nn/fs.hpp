#pragma once

#include "nn/fs/fs_types.hpp"
#include "nn/fs/fs_directories.hpp"
#include "nn/fs/fs_mount.hpp"
#include "nn/fs/fs_files.hpp"

enum {
    Module_Fs = 2,
};

enum {
    FsResult_MountNameAlreadyExists = 60,
};