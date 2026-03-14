# KnobKraft ORM - Claude Code Instructions

## Project Overview

KnobKraft ORM is a cross-platform MIDI Sysex Librarian for managing synthesizer patches. Written in C++20 using the JUCE framework, with Python-based synth adaptations via pybind11. Supports 70+ synthesizers.

**License:** Dual licensed (AGPL / MIT available for purchase)
**Original author:** christofmuc
**Our fork:** https://github.com/olaservo/KnobKraft-orm

## Repository Structure

```
KnobKraft-orm/
├── The-Orm/              # Main application (GUI, menus, patch views)
│   ├── MainComponent.cpp # App shell, menu bar, settings, startup
│   ├── PatchView.cpp     # Patch library UI, import, reindex
│   ├── Main.cpp          # Entry point
│   └── CMakeLists.txt    # App build config + InnoSetup installer
├── MidiKraft/             # SUBMODULE (our fork: olaservo/MidiKraft)
│   ├── base/             # Core synth classes (Synth.h/cpp)
│   ├── database/         # PatchDatabase (SQLite), PatchFilter
│   └── librarian/        # PatchHolder, PatchList, Librarian
├── adaptations/          # Python synth adaptations (.py files)
│   ├── GenericAdaptation.cpp  # C++/Python bridge via pybind11
│   ├── testing/librarian.py   # Python test harness for sysex loading
│   ├── sequential/       # Sequential synth shared code
│   └── roland/           # Roland synth shared code
├── synths/               # Native C++ synth implementations
│   ├── access-virus/
│   ├── sequential-rev2/
│   ├── sequential-ob6/
│   └── ... (kawai-k3, matrix1000, roland-mks50, roland-mks80)
├── third_party/          # Submodules: JUCE, SQLiteCpp, pybind11, fmt, spdlog, json, etc.
├── juce-utils/           # SUBMODULE: Utility library (Settings, MIDI helpers, Sysex)
├── juce-widgets/         # SUBMODULE: UI widgets (PatchButton, LogView, etc.)
└── pytschirp/            # SUBMODULE: Python patch scripting
```

## MidiKraft Submodule

**Critical:** The MidiKraft submodule contains the core database, librarian, and synth base classes. Our fork is at https://github.com/olaservo/MidiKraft.

- `.gitmodules` points to `olaservo/MidiKraft` (not the upstream `christofmuc/MidiKraft`)
- Changes to MidiKraft must be committed inside the submodule first, then the submodule pointer committed in the main repo
- Push MidiKraft changes to the `myfork` remote: `git push myfork <branch>`
- The upstream remote is `origin` (christofmuc), our fork remote is `myfork`

### Key MidiKraft files

| File | Purpose |
|------|---------|
| `database/PatchDatabase.h/.cpp` | SQLite database, patch CRUD, merge logic, schema migrations |
| `database/PatchFilter.h/.cpp` | Query filtering (synth, categories, ordering, lists) |
| `librarian/PatchHolder.h/.cpp` | Patch container with md5/fingerprint, metadata, drag-and-drop |
| `base/include/Synth.h`, `base/src/Synth.cpp` | Base synth class, `loadSysex()`, `calculateFingerprint()` |

### Database Schema

- **patches** table: `PRIMARY KEY (synth, md5)` — md5 is the content fingerprint (or unique key when duplicates allowed)
- **patch_in_list** table: `FOREIGN KEY(synth, md5) REFERENCES patches(synth, md5)`
- **lists** table: synth banks, user banks, import lists, patch lists
- **categories** table: category definitions with bit indices
- Schema version tracked in `schema_version` table (currently version 19)
- Migrations in `PatchDatabase.cpp::migrateSchema()` — follow the existing pattern of `if (currentVersion < N)` blocks

### PatchHolder::md5() behavior

`md5()` returns `storedMd5_` if set (patches loaded from DB), otherwise computes via `Synth::calculateFingerprint()`. Use `computeFingerprint()` when you need the actual content hash regardless of stored state.

## Building on Windows

### Prerequisites

- **Visual Studio 2022** (or Build Tools) with C++ desktop workload
- **CMake 3.18+** (`winget install Kitware.CMake`)
- **Python 3.13** (must match target machine's Python version)
- **InnoSetup 6** for installer (`winget install JRSoftware.InnoSetup`)
- **Git** with submodule support

### Build Commands

```bash
# 1. Initialize all submodules
git submodule update --init --recursive

# 2. Fetch tags from upstream (needed for version detection)
git remote add upstream https://github.com/christofmuc/KnobKraft-orm.git
git fetch upstream --tags

# 3. Configure (adjust Python path as needed)
cmake --fresh -S . -B builds -G "Visual Studio 17 2022" -A x64 \
  -DPYTHON_EXECUTABLE="C:/Users/johnn/AppData/Local/Programs/Python/Python313/python.exe" \
  -DPYTHON_VERSION_TO_DOWNLOAD="3.13.1" \
  -DINNOSETUP="C:/Users/johnn/AppData/Local/Programs/Inno Setup 6/ISCC.exe"

# 4. Build Release (no installer)
cmake --build builds --config Release

# 5. Build RelWithDebInfo (generates installer)
cmake --build builds --config RelWithDebInfo --target KnobKraftOrm
```

### Build Outputs

- **Executable:** `builds/The-Orm/Release/KnobKraftOrm.exe` (or `RelWithDebInfo/`)
- **Installer:** `builds/The-Orm/knobkraft_orm_setup_<version>.exe` (RelWithDebInfo only)

### Python Version Matters

The app embeds a Python runtime. The `PYTHON_VERSION_TO_DOWNLOAD` CMake variable controls which embedded Python is bundled. **This must match the Python installed on the target machine**, otherwise you get `Module use of pythonXXX.dll conflicts with this version of Python` errors from adaptations. Default is 3.12; we build with 3.13.

### Incremental Builds

After changing only a few .cpp files, use `--target KnobKraftOrm` to skip rebuilding unchanged libraries.

## Settings System

Settings are persisted via `Settings::instance().get(key, default)` / `Settings::instance().set(key, value)`. Stored as key-value string pairs. Read on startup in `MainComponent` constructor.

## Our Custom Changes

### Allow Duplicate Patches (feature/allow-duplicate-patches)

Prevents the librarian from de-duplicating patches on import. Controlled by `Settings::instance().get("AllowDuplicatePatches", "true")`.

**How it works:** When enabled, `mergePatchesIntoDatabaseAllowDuplicates()` assigns each patch a unique md5 key (`fingerprint + "-" + UUID`) so the `(synth, md5)` primary key never collides. The `Synth::loadSysex()` edit-buffer-vs-program-dump dedup is also skipped. Reindexing is disabled.

**Files modified:**
- `MidiKraft/database/PatchDatabase.cpp` — merge logic, reindex guard, reindexing check
- `MidiKraft/database/PatchDatabase.h` — `setAllowDuplicates()` API
- `MidiKraft/librarian/PatchHolder.h/.cpp` — `storedMd5_`, `computeFingerprint()`, `setStoredMd5()`
- `MidiKraft/base/include/Synth.h`, `base/src/Synth.cpp` — static `allowDuplicates_` flag
- `The-Orm/MainComponent.cpp` — Options menu toggle, startup initialization
- `The-Orm/PatchView.cpp` — reindex guard dialog
- `adaptations/testing/librarian.py` — `allow_duplicates` flag on Librarian class

## Testing

### C++ tests
```bash
cmake --build builds --config Release --target patch_database_search_test
cmake --build builds --config Release --target patch_database_migration_test
./builds/tests/Release/patch_database_search_test.exe
./builds/tests/Release/patch_database_migration_test.exe
```

### Python adaptation tests
```bash
pip install -r requirements.txt
cd adaptations
python -m pytest --all . -q --no-header
```

## Common Pitfalls

1. **Forgetting to init submodules** — most build errors are missing submodules. Always `git submodule update --init --recursive`.
2. **No git tags** — `git describe --tags` fails without tags, breaking version detection. Fetch from upstream: `git fetch upstream --tags`.
3. **Python version mismatch** — the embedded Python DLL must match the target system. Build with `-DPYTHON_VERSION_TO_DOWNLOAD=3.13.1` for machines with Python 3.13.
4. **InnoSetup path** — CMake only searches `"c:/program files (x86)/Inno Setup 6"`. If installed elsewhere (e.g., user-local), pass `-DINNOSETUP=<path to ISCC.exe>`.
5. **MidiKraft changes need two commits** — one inside the submodule, one in the main repo to update the submodule pointer.
6. **Installer only builds with RelWithDebInfo** — the `Release` config skips the InnoSetup step.
