#include "bloomfilter.hpp"
#include "loader.hpp"

#include "fs/fs_types.hpp"
#include "lib.hpp"
#include "nn.hpp"

#define ENABLE_HOT_RELOAD

#include <array>
#ifdef ENABLE_HOT_RELOAD
#include <mutex>
#include <shared_mutex>
#endif
#include <string.h>
#include <string>

constexpr const char cSDCardMountPoint[] = "sd";
constexpr const char cRomfsDirectory[] = "sd:/atmosphere/contents/" STRINGIFY(EXL_PROGRAM_ID) "/romfslite";
constexpr const char cMountDelimiter[] = ":/";

static const char* sContentMountPoint = "content";
static size_t sContentMountPointLength = 7;

HOOK_DEFINE_TRAMPOLINE(MountRom) {
    static Result Callback(const char* mountPoint, void* cache, size_t cacheSize) {
        const auto res = Orig(mountPoint, cache, cacheSize);
        if (res == 0) {
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

// not sure if there's a great solution to handling OpenDirectory without meddling with the internal DirectoryAccessor class
// the other option is to hook any place where the game is iterating through directories and handle it there, but that's less generic

HOOK_DEFINE_TRAMPOLINE(OpenFile) {
    static Result Callback(nn::fs::FileHandle* outHandle, const char* path, int mode) {
        if (!CheckPrefix(path, sContentMountPoint) || !CheckPrefix(path + sContentMountPointLength, cMountDelimiter)) {
            return Orig(outHandle, path, mode);
        }
        const char* rawPath = path + sContentMountPointLength + sizeof(cMountDelimiter) - 1;
        const auto size = strlen(rawPath);
        bool inFilter;
#ifdef ENABLE_HOT_RELOAD
        {
            // const auto _ = std::scoped_lock(gFilterLock);
            const auto _ = std::shared_lock(gFilterLock);
#endif
            inFilter = gPathFilter.contains({ rawPath, size });
#ifdef ENABLE_HOT_RELOAD
        }
#endif
        if (!inFilter) {
            return Orig(outHandle, path, mode);
        }
        char newpath[nn::fs::MaxDirectoryEntryNameSize + 1];
        const auto s = nn::util::SNPrintf(newpath, sizeof(newpath), "%s/%s", cRomfsDirectory, rawPath);
        newpath[s] = '\0';
        const auto res = Orig(outHandle, newpath, mode);
        if (res == 0) {
            return res;
        }
        // maybe it was a false positive, maybe the file really doesn't exist
        // regardless, try opening the orignal file instead
        return Orig(outHandle, path, mode);
    }
};

// TODO: when we get to handling OpenDirectory, replace the call here with the original
static void ProcessDirectoryRecursive(const char* path) {
    nn::fs::DirectoryHandle handle{};
    if (nn::fs::OpenDirectory(&handle, path, nn::fs::OpenDirectoryMode_All) != 0) {
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
        const auto size = nn::util::SNPrintf(subpath, sizeof(subpath), "%s/%s", path, entry.m_Name);
        subpath[size] = '\0';
        if (entry.m_Type == nn::fs::DirectoryEntryType_Directory) {
            ProcessDirectoryRecursive(subpath);
        } else {
            if (static_cast<std::size_t>(size) < sizeof(cRomfsDirectory) + 1) {
                continue;
            }
#ifdef ENABLE_HOT_RELOAD
            const auto _ = std::scoped_lock(gFilterLock);
#endif
            // no size - 1 here bc the directory path doesn't include /
            gPathFilter.add({ subpath + sizeof(cRomfsDirectory), static_cast<std::size_t>(size - sizeof(cRomfsDirectory)) });
        }
    }
}

#ifdef ENABLE_HOT_RELOAD
static nn::os::ThreadType gIndexerThread;
alignas(nn::os::ThreadStackAlignment) static char gIndexerThreadStack[0x5000];

static void ThreadMain(void*) {
    for (;;) {
        ProcessDirectoryRecursive(cRomfsDirectory);
        // TODO: is there some event that fires when the sd card is updated? ideally we'd only awaken in those cases
        nn::os::SleepThread(nn::TimeSpan::FromSeconds(5));
    }
}

static void InitializeIndexerThread() {
    nn::os::CreateThread(&gIndexerThread, ThreadMain, nullptr, gIndexerThreadStack, sizeof(gIndexerThreadStack), 1);
    nn::os::SetThreadName(&gIndexerThread, "Indexer");
    nn::os::StartThread(&gIndexerThread);
}
#endif

void InitializeLoader() {
    const auto mountResult = nn::fs::MountSdCard(cSDCardMountPoint);
    if (mountResult != 0 && mountResult != exl::result::MakeResult(Module_Fs, FsResult_MountNameAlreadyExists)) {
        return;
    }

    ProcessDirectoryRecursive(cRomfsDirectory);

    MountRom::InstallAtFuncPtr(nn::fs::MountRom);
    OpenFile::InstallAtFuncPtr(nn::fs::OpenFile);

#ifdef ENABLE_HOT_RELOAD
    InitializeIndexerThread();
#endif
}