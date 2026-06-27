this is a simple romfs-loading mod to overcome limitations of later firmware versions where system memory is too limited to build a full layeredfs for certain games

this is largely inspired by [ultracam](https://gamebanana.com/mods/480138)'s romfslite behavior (by maxlastbreath) and aims to load mods of the same format (the implementation details are likely different, not sure how the original romfslite works)

romfs files are loaded from `sd:/atmosphere/contents/<title_id>/romfslite/` if present

limitations:
- `nn::fs::OpenDirectory` provides a merged view of the mod directory and normal directory without de-duplicating entries
  - this should not cause any issues unless a game explicitly relies on the file count since opening a file will resolve it correctly

the loader is implemented as a zero-allocation bloom filter built from the files present in the romfslite directory

with the default bloom filter parameters, this gives a ~0.01% false positive rate with 5000 files in the mod and ~12kb of memory consumption

by default, hot reloading of files is enabled - this spawns an indexer thread that re-indexes the romfslite directory every 5 seconds

currently configured for totk, but this should work generically for any game that may require this

to port to a different game, edit the title id in `config.json` and the program id in `config.mk`

# exlaunch
A framework for injecting C/C++ code into Nintendo Switch applications/applet/sysmodules.

> [!NOTE]
> This project is a work in progress. If you have issues, reach out to `shad0w0.` on Discord.

# Credit
- Atmosphère: A great reference and guide.
- oss-rtld: Included for (pending) interop with rtld in applications (License [here](https://github.com/shadowninja108/exlaunch/blob/main/source/lib/reloc/rtld/LICENSE.txt)).
