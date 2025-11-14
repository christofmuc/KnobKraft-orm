# Imports as Lists – Target Schema & API

## Storage Model
1. **`lists` table extension**  
   - Add `list_type INTEGER NOT NULL` (default `0` for legacy rows) to describe the semantic of each list.  
   - Enumerate values (persisted as ints) to map to specific PatchList subclasses:
     | Value | Meaning | Instantiated class |
     | --- | --- | --- |
     | 0 | `USER_LIST` | `PatchList` |
     | 1 | `USER_BANK` | `UserBank` |
     | 2 | `SYNTH_BANK` | `SynthBank` |
     | 3 | `ACTIVE_SYNTH_BANK` | `ActiveSynthBank` |
     | 4 | `IMPORT_LIST` | Import bucket (new) |
   - Import lists will use `list_type = 4`, `synth NOT NULL`, `midi_bank_number NULL`, `name` = import display name, and `last_synced` = UNIX epoch milliseconds of the import timestamp.

2. **ID generation**  
   - Every import list reuses the existing `sourceID` string, prefixed with the synth to avoid cross-synth collisions:  
     `list_id = "import:" + synth_name + ":" + sourceID`.  
   - This keeps referential integrity (uniqueness) and gives human-readable hints for debugging.  
   - Existing user lists/banks continue to use their current UUID-style IDs.

3. **`patch_in_list` usage**  
   - Import membership is represented solely via `patch_in_list` rows referencing the new list IDs.  
   - `order_num` stays zero-based; creation/migration logic must set `ROW_NUMBER() - 1` consistently so UI consumers can continue to pass zero-based indices.
   - Import lists default to insertion order (`order_num` derived from `patches.rowid` inside the import batch), giving deterministic display without bespoke ordering SQL.

4. **Legacy `imports` table**  
   - After migration, drop the standalone `imports` table entirely. All metadata (name, timestamp, count) can be derived from the `lists` row plus `patch_in_list`.  
   - For safety, we can provide a compatibility view (`CREATE VIEW imports AS ...`) in the migration if older builds still query it, but the target schema treats imports as regular lists only.

## API & Behavioural Invariants
1. **Patch ingestion** (`PatchDatabase::mergePatchesIntoDatabase`)  
   - After creating/identifying the `sourceID`, call a helper to upsert the corresponding import list (ensuring `lists` has the row, filling `name`, `last_synced`, `list_type`, `synth`).  
   - Every inserted patch must then receive a `patch_in_list` membership referencing that list id. Duplicate patches within the same import should not create duplicate rows—respect `ON CONFLICT` or detect beforehand.

2. **Filtering**  
   - `PatchFilter.importID` becomes a thin alias for `PatchFilter.listID`; eventually we can remove `importID`, but during transition the UI can keep writing to `importID` and the filter builder will translate it into the shared list-based join.  
   - When `importID` (or the derived list id) is set, the SQL must switch to the same join/order path already used for arbitrary lists (ensuring ordering by `order_num`).

3. **Listing imports**  
   - Replace `getImportsList(Synth*)` with `getImportLists(Synth*)` returning `ListInfo + count`. Implementation: `SELECT id,name,last_synced FROM lists WHERE list_type = IMPORT_LIST AND synth = :SYN ORDER BY last_synced`.  
   - `PatchListTree` can reuse the existing “lists tree” logic by treating import lists exactly like any other list subtype, i.e., `getPatchList` will instantiate a lightweight `PatchList` (or a small `ImportList` subclass if needed for UI labelling).

4. **Invariants**  
   - `lists.list_type = IMPORT_LIST` ⇒ `lists.synth IS NOT NULL`, `lists.midi_bank_number IS NULL`.  
   - `last_synced` for import lists must store the original import timestamp (in ms since epoch). When importing from files lacking timestamps, fall back to `sqlite datetime('now')` converted to ms.  
   - `patch_in_list.order_num` is always zero-based and compact (no gaps) after any mutation; helpers like `renumList` remain responsible for re-packing indexes.

5. **Patch metadata**  
   - `patches.sourceID` was kept temporarily (schema ≤17) so the migration CTEs could discover legacy imports. Schema 18 removes the column entirely; new imports rely purely on `lists`/`patch_in_list` membership.

## Migration Expectations
1. Add `list_type` column with default `USER_LIST` (0) and backfill existing rows based on current heuristics (active banks vs. user banks).  
2. Create import list rows from `imports` joined to `patches`, generating `list_id`, `name`, `last_synced`, and populating `patch_in_list` membership for every patch referencing that `sourceID`.  
3. Remove or replace the `imports` table (drop table or convert to a read-only view pointing to `lists`).  
4. Ensure all public APIs (`getImportsList`, `renameImport`, etc.) now operate on the lists infrastructure to keep the UI unchanged while the underlying data moves.  
5. Regression tests must cover migrating a schema-14/15 database into the new layout and validating the invariants above.
