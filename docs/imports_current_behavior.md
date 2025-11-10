# Legacy Imports Behaviour

## Schema Snapshot
- **`patches`** keeps one row per `(synth, md5)` and stores the `sourceID`/`sourceInfo` metadata emitted during imports (`MidiKraft/database/PatchDatabase.cpp:313-320`).
- **`imports`** is a loose lookup table with `(synth TEXT, name TEXT, id TEXT, date TEXT)` used only for UI labelling and filtering (`MidiKraft/database/PatchDatabase.cpp:327-335`).
- **`lists`**/`patch_in_list` remain independent from imports; order numbers are zero-based via the migration CTE and renumber helpers (`MidiKraft/database/PatchDatabase.cpp:210-312`, `MidiKraft/database/PatchDatabase.cpp:1715-1747`).
- `schema_version` currently tracks value **15**; any migration that turns imports into list entries will bump this number and must keep the upgrade path from ≤15 intact (`MidiKraft/database/PatchDatabase.cpp:37-58`, `MidiKraft/database/PatchDatabase.cpp:321-412`).

## Data Flow
1. **Merge path:** `PatchDatabase::mergePatchesIntoDatabase` batches new patches, derives an `importUID` from `SourceInfo::md5`, or forces `"EditBufferImport"` for edit-buffer captures, and remembers which `(synth, importUID, name)` combinations need `imports` rows (`MidiKraft/database/PatchDatabase.cpp:1186-1312`).
2. **Persist patch:** `putPatch` writes each patch row with the generated `sourceID`, stores a human-readable `sourceName`, and serialises the source blob into `sourceInfo` (`MidiKraft/database/PatchDatabase.cpp:398-433`).
3. **Record import metadata:** `insertImportInfo` ensures the `(synth,id)` entry exists in `imports` and stamps `datetime('now')` into `date` (`MidiKraft/database/PatchDatabase.cpp:1148-1172`).
4. **Filtering:** `PatchFilter` carries both `importID` and `listID`. Query builders attach `sourceID = :SID` when `importID` is set and join `patch_in_list`/order by `order_num` when `listID` is set, meaning imports and lists are mutually exclusive filters today (`MidiKraft/database/PatchFilter.cpp:27-64`, `MidiKraft/database/PatchDatabase.cpp:470-620`).

## UI Integration
- `PatchListTree::newTreeViewItemForImports` pulls `getImportsList`, shortens names, and creates tree nodes whose IDs are `ImportInfo.id`. Selecting a node switches the UI to single-synth mode and calls `onImportListSelected(id)` so the `PatchView` can set `sourceFilterID_` (`The-Orm/PatchListTree.cpp:536-590`).
- `ImportNameListener` listens to rename edits on those tree nodes and proxies to `PatchDatabase::renameImport`, which updates only the lookup row (`The-Orm/PatchListTree.cpp:54-69`, `MidiKraft/database/PatchDatabase.cpp:437-466`).
- `PatchView::setImportListFilter` and `PatchView::currentFilter` push the chosen import into `PatchFilter.importID`, clear the list filter, and refresh the grid (`The-Orm/PatchView.cpp:440-515`, `The-Orm/PatchView.cpp:700-735`).

## Code Paths Touching Imports
- `PatchDatabase::mergePatchesIntoDatabase`, `putPatch`, `insertImportInfo`, `getImportsList`, `renameImport`, and the SQL filter builders (`MidiKraft/database/PatchDatabase.cpp:398-1312`, `MidiKraft/database/PatchDatabase.cpp:470-620`).
- `PatchView` filter plumbing and reindex/delete flows that respect `importID` (`The-Orm/PatchView.cpp:440-780`).
- `PatchListTree` import tree creation plus rename listener (`The-Orm/PatchListTree.cpp:23-120`, `The-Orm/PatchListTree.cpp:536-606`).
- `PatchSearchComponent` clears `importID` when its own filter changes so the tree owns that state (`The-Orm/PatchSearchComponent.cpp:360-380`).

## Fixture: `tests/fixtures/imports_v14.db3`
- SQLite database with schema version **15**, one synth named `TestSynth`, and a single import `import-123` linked to patch `md5-aaa`.
- Tables included: `patches`, `imports`, `lists`, `patch_in_list`, `categories`, and `schema_version` mirroring today’s schema so the current code can open it without migrations.
- Patch row carries a valid `sourceInfo` JSON blob (bulk import referencing a file) and demonstrates how the UI currently derives import lists purely from `patches.sourceID`.
- Use it as the “pre-refactor” fixture in migration doctests: point `PatchDatabase` at the file, assert `getImportsList(TestSynth)` returns one entry, and verify `PatchFilter.importID = "import-123"` scopes results to the linked patch.
