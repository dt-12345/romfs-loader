this is a simple romfs-loading mod to overcome limitations of later firmware versions where system memory is too limited to build a full layeredfs for certain games

this is largely inspired by [ultracam](https://gamebanana.com/mods/480138)'s romfslite behavior (by maxlastbreath) and aims to load mods of the same format (the implementation details are likely different, not sure how the original romfslite works)

romfs files are loaded from `sd:/atmosphere/contents/<title_id>/romfslite/` if present

limitations:
- directory iteration is unhandled (i.e. if a game calls `nn::fs::OpenDirectory`, it does not receive a merged view of the romfs directory and the romfslite directory)
  - most games should not do this so hopefully this is fine

the loader is implemented as a zero-allocation bloom filter built from the files present in the romfslite directory

with the default bloom filter parameters, this gives a ~0.01% false positive rate with 5000 files in the mod and ~12kb of memory consumption

by default, hot reloading of files is enabled - this spawns an indexer thread that re-indexes the romfslite directory every 5 seconds

# exlaunch
A framework for injecting C/C++ code into Nintendo Switch applications/applet/sysmodules.

> [!NOTE]
> This project is a work in progress. If you have issues, reach out to `shad0w0.` on Discord.

# Credit
- Atmosphère: A great reference and guide.
- oss-rtld: Included for (pending) interop with rtld in applications (License [here](https://github.com/shadowninja108/exlaunch/blob/main/source/lib/reloc/rtld/LICENSE.txt)).
