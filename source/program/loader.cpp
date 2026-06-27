#include "bloomfilter.hpp"
#include "loader.hpp"
#include "accessor.hpp"

#include "lib.hpp"
#include "nn.hpp"

#define ENABLE_HOT_RELOAD

#ifdef ENABLE_HOT_RELOAD
#include <mutex>
#include <shared_mutex>
#endif
#include <string.h>

constexpr const char cSDCardMountPoint[] = "sd";
constexpr const char cRomfsDirectory[] = "sd:/atmosphere/contents/" STRINGIFY(EXL_PROGRAM_ID) "/romfslite";
constexpr const char cMountDelimiter[] = ":/";

static const char* sContentMountPoint = "content";
static size_t sContentMountPointLength = 7;

HOOK_DEFINE_TRAMPOLINE(MountRom) {
    static Result Callback(const char* mountPoint, void* cache, size_t cacheSize) {
        const auto res = Orig(mountPoint, cache, cacheSize);
        if (R_SUCCEEDED(res)) {
            sContentMountPoint = mountPoint;
            sContentMountPointLength = strlen(sContentMountPoint);
        }
        return res;
    }
};

#ifdef ENABLE_HOT_RELOAD
// static nn::os::Mutex gFilterLock = nn::os::Mutex(false);
static nn::os::ReaderWriterLock gFilterLock = nn::os::ReaderWriterLock();
#endif

/*
    the goal here is a zero-allocation bloom filter of the files in the mod
    we want to both minimize the memory footprint, look-up times, and false positive rates

    if we assume 100k files with a 0.0001 false positive rate:
    FILTER_SIZE = -(100000 * ln(0.0001)) / ln(2)^2 rounded up => 1917016 bits (~256kb)
    HASH_FUNCTION_COUNT = FILTER_SIZE / 100000 * ln(2) rounded up => 14

    but 100k is kind of excessive for the vast majority of mods and we end up paying for more than we need
    for example, challenge mode has < 2000 files
    if we assume 10k files with a 0.0001 false positive rate:
    FILTER_SIZE = -(10000 * ln(0.0001)) / ln(2)^2 rouded up => 191704 (~24kb)
    HASH_FUNCTION_COUNT = FILTER_SIZE / 10000 * ln(2) rounded up => 14

    if we drop it to 5000 files:
    FILTER_SIZE = -(5000 * ln(0.0001)) / ln(2)^2 rouded up => 95856 (~12kb)
    HASH_FUNCTION_COUNT = FILTER_SIZE / 5000 * ln(2) rounded up => 14
    these parameters of course don't scale very well to 100k files (basically 100% false positive rate)

    if it turns out mods really need that many files, we can bump the filter size up later
*/
using Filter = BloomFilter<0x1'8000, 16>; // we'll round them up to nicer numbers
static Filter gPathFilter;

static bool InFilter(const char* path) {
    if (path == nullptr) {
        return false;
    }
    const auto size = strlen(path);
    if (size < 1) {
        return false;
    }
#ifdef ENABLE_HOT_RELOAD
    const auto _ = std::shared_lock(gFilterLock);
#endif
    return gPathFilter.contains({ path, path[size - 1] == '/' ? size - 1 : size });
}

static bool CheckPrefix(const char* str, const char* prefix) {
    if (str == nullptr || prefix == nullptr) {
        return false;
    }
    while (*str && *prefix) {
        if (*str++ != *prefix++) {
            return false;
        }
    }
    return true;
}

static size_t JoinPath(char* out, size_t outSize, const char* part1, const char* part2) {
    const auto len1 = strlen(part1);
    size_t length = len1 >= outSize ? outSize : len1;
    memcpy(out, part1, length);
    if (length + 1 >= outSize) {
        out[outSize - 1] = '\0';
        return outSize - 1;
    }
    if (part1[len1 - 1] != '/') {
        out[length++] = '/';
    }
    const auto remaining = length >= outSize ? 0 : outSize - length;
    const auto len2 = strlen(part2);
    memcpy(out + length, part2, len2 >= remaining? remaining : len2);
    length += len2;
    length = length >= outSize - 1 ? outSize - 1 : length;
    out[length] = '\0';
    return length;
}

// not sure if there's a great solution to handling OpenDirectory without meddling with the internal DirectoryAccessor class
// the other option is to hook any place where the game is iterating through directories and handle it there, but that's less generic

HOOK_DEFINE_TRAMPOLINE(OpenFile) {
    static Result Callback(nn::fs::FileHandle* outHandle, const char* path, int mode) {
        if (!CheckPrefix(path, sContentMountPoint) || !CheckPrefix(path + sContentMountPointLength, cMountDelimiter)) {
            return Orig(outHandle, path, mode);
        }
        const char* rawPath = path + sContentMountPointLength + sizeof(cMountDelimiter) - 1;
        if (!InFilter(rawPath)) {
            return Orig(outHandle, path, mode);
        }
        char newpath[nn::fs::MaxDirectoryEntryNameSize + 1];
        JoinPath(newpath, sizeof(newpath), cRomfsDirectory, rawPath);
        const auto res = Orig(outHandle, newpath, mode);
        if (R_SUCCEEDED(res)) {
            return res;
        }
        // maybe it was a false positive, maybe the file really doesn't exist
        // regardless, try opening the orignal file instead
        return Orig(outHandle, path, mode);
    }
};

HOOK_DEFINE_TRAMPOLINE(OpenDirectory) {
    static Result Callback(nn::fs::DirectoryHandle* outHandle, const char* path, int mode) {
        if (!CheckPrefix(path, sContentMountPoint) || !CheckPrefix(path + sContentMountPointLength, cMountDelimiter)) {
            return Orig(outHandle, path, mode);
        }
        R_UNLESS(outHandle != nullptr, nn::fs::ResultNullptrArgument());
        const char* rawPath = path + sContentMountPointLength + sizeof(cMountDelimiter) - 1;
        if (!InFilter(rawPath)) {
            return Orig(outHandle, path, mode);
        }
        char newpath[nn::fs::MaxDirectoryEntryNameSize + 1];
        JoinPath(newpath, sizeof(newpath), cRomfsDirectory, rawPath);
        nn::fs::DirectoryHandle handle1;
        const auto res1 = Orig(&handle1, newpath, mode);
        if (R_SUCCEEDED(res1)) {
            nn::fs::DirectoryHandle handle2;
            const auto res2 = Orig(&handle2, path, mode);
            if (R_SUCCEEDED(res2)) {
                nn::fs::detail::FileSystemAccessor* fsa;
                const char* fs_path;
                const auto res = nn::fs::detail::FindFileSystem(&fsa, &fs_path, path);
                if (R_SUCCEEDED(res)) {
                    outHandle->_internal = reinterpret_cast<u64>(new nn::fs::detail::DirectoryAccessor(std::make_unique<MergedDirectory>(handle1, handle2), *fsa));
                    R_SUCCEED();
                } else {
                    nn::fs::CloseDirectory(handle1);
                    nn::fs::CloseDirectory(handle2);
                    return res;
                }
            } else { // this directory only exists in the mod
                *outHandle = handle1;
                R_SUCCEED();
            }
        }
        return Orig(outHandle, path, mode);
    }
};

static void ProcessDirectoryRecursive(const char* path) {
    nn::fs::DirectoryHandle handle{};
    if (OpenDirectory::Orig(&handle, path, nn::fs::OpenDirectoryMode_All) != 0) {
        return;
    }
    s64 count = 0;
    if (nn::fs::GetDirectoryEntryCount(&count, handle) != 0) {
        return;
    }
    s64 readCount = 0;
    for (s64 i = 0; i < count; ++i) {
        nn::fs::DirectoryEntry entry{};
        if (nn::fs::ReadDirectory(&readCount, &entry, handle, 1) != 0) {
            continue;
        }
        char subpath[nn::fs::MaxDirectoryEntryNameSize + 1];
        const auto size = JoinPath(subpath, sizeof(subpath), path, entry.m_Name);
        if (entry.m_Type == nn::fs::DirectoryEntryType_Directory) {
            ProcessDirectoryRecursive(subpath);
        }
        if (size < sizeof(cRomfsDirectory) + 1) {
            continue;
        }
#ifdef ENABLE_HOT_RELOAD
        const auto _ = std::scoped_lock(gFilterLock);
#endif
        // no size - 1 here bc the directory path doesn't include /
        gPathFilter.add({ subpath + sizeof(cRomfsDirectory), static_cast<std::size_t>(size - sizeof(cRomfsDirectory)) });
    }
}

#ifdef ENABLE_HOT_RELOAD
class Thread {
public:
    Thread() = default;

    void Initialize(const char* name, nn::os::ThreadFunction func, void* arg, int priority) {
        nn::os::CreateThread(&mThread, func, arg, mStack, sizeof(mStack), priority);
        nn::os::SetThreadName(&mThread, name);
        nn::os::StartThread(&mThread);
    }

    ~Thread() {
        nn::os::DestroyThread(&mThread);
    }

private:
    alignas(nn::os::ThreadStackAlignment) char mStack[0x5000];
    nn::os::ThreadType mThread;
};

static Thread gIndexerThread;

static void ThreadMain(void*) {
    for (;;) {
        ProcessDirectoryRecursive(cRomfsDirectory);
        // TODO: is there some event that fires when the sd card is updated? ideally we'd only awaken in those cases
        nn::os::SleepThread(nn::TimeSpan::FromSeconds(5));
    }
}

static void InitializeIndexerThread() {
    gIndexerThread.Initialize("Indexer", ThreadMain, nullptr, 1);
}
#endif

void InitializeLoader() {
    const auto mountResult = nn::fs::MountSdCard(cSDCardMountPoint);
    if (mountResult != 0 && mountResult != exl::result::MakeResult(Module_Fs, FsResult_MountNameAlreadyExists)) {
        return;
    }

    MountRom::InstallAtFuncPtr(nn::fs::MountRom);
    OpenFile::InstallAtFuncPtr(nn::fs::OpenFile);
    OpenDirectory::InstallAtFuncPtr(nn::fs::OpenDirectory);

    ProcessDirectoryRecursive(cRomfsDirectory);

#ifdef ENABLE_HOT_RELOAD
    InitializeIndexerThread();
#endif
}