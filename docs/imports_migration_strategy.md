# Migration Strategy (Schema ≥16)

## Goals
Convert legacy imports into first-class list entries while keeping zero-based ordering across every list and preserving historical metadata. The migration bumps `SCHEMA_VERSION` from 15 → 16.

## Steps

### 1. Add `list_type`
```sql
ALTER TABLE lists ADD COLUMN list_type INTEGER DEFAULT 0 NOT NULL;
```
- Use the existing `migrateTable` helper if SQLite’s `ALTER TABLE` fails (e.g., older versions). Default of `0` (`USER_LIST`) ensures legacy rows remain valid until backfilled.
- After altering, `UPDATE lists SET list_type = 0 WHERE list_type IS NULL` as a safeguard.

### 2. Seed `list_type` for existing rows
Inside a transaction:
```sql
UPDATE lists SET list_type = 2             -- SYNTH_BANK
WHERE synth IS NOT NULL AND midi_bank_number IS NULL AND name LIKE 'Active%'; -- adjust to real heuristic

UPDATE lists SET list_type = 1             -- USER_BANK
WHERE synth IS NOT NULL AND list_type = 0; -- all remaining synth-backed rows

-- Active banks (auto-created per connected synth)
UPDATE lists SET list_type = 3
WHERE synth IS NOT NULL AND id LIKE synth || '%';

-- Plain user lists stay at 0.
```
> Exact predicates will mirror current heuristics (ID prefix + `midi_bank_number` presence). The migration should match the logic in `PatchDatabase::getPatchList` to avoid regressions.

### 3. Copy imports → lists / patch_in_list
1. Create import list rows:
```sql
INSERT INTO lists(id, name, synth, midi_bank_number, last_synced, list_type)
SELECT
    'import:' || patches.synth || ':' || imports.id AS list_id,
    imports.name,
    patches.synth,
    NULL,
    COALESCE(STRFTIME('%s', imports.date) * 1000, 0),
    4 -- IMPORT_LIST
FROM imports
JOIN patches ON patches.sourceID = imports.id AND patches.synth = imports.synth
GROUP BY patches.synth, imports.id;
```
2. Populate `patch_in_list` membership:
```sql
INSERT INTO patch_in_list(id, synth, md5, order_num)
SELECT
    'import:' || p.synth || ':' || p.sourceID,
    p.synth,
    p.md5,
    ROW_NUMBER() OVER(
        PARTITION BY p.synth, p.sourceID
        ORDER BY p.midiBankNo, p.midiProgramNo, p.name, p.md5
    ) - 1
FROM patches AS p
WHERE p.sourceID IS NOT NULL;
```
- The ordering clause gives deterministic zero-based order even when bank/program numbers are null (SQLite sorts NULLs first; handle via `COALESCE(p.midiBankNo, -1)` to keep edit-buffer imports at the top if desired).
- Use `INSERT OR IGNORE` to avoid duplicate entries when the migration reruns on partially upgraded DBs.

### 4. Backfill order numbers for all lists
Run once after the import copy:
```sql
WITH ordered AS (
  SELECT id, synth, md5,
         ROW_NUMBER() OVER(PARTITION BY id ORDER BY order_num, ROWID) - 1 AS new_order
  FROM patch_in_list
)
UPDATE patch_in_list
SET order_num = ordered.new_order
FROM ordered
WHERE patch_in_list.id = ordered.id
  AND patch_in_list.synth = ordered.synth
  AND patch_in_list.md5 = ordered.md5;
```
This normalizes any legacy one-based data and compacts gaps introduced by previous bugs before we rely on zero-based invariants.

### 5. Drop `imports`
Once lists hold the data:
```sql
DROP TABLE IF EXISTS imports;
```
Optionally create a compatibility view:
```sql
CREATE VIEW imports AS
SELECT synth,
       name,
       SUBSTR(id, INSTR(id, ':', 8) + 1) AS id, -- strip prefix
       datetime(last_synced / 1000, 'unixepoch') AS date
FROM lists
WHERE list_type = 4;
```

### 6. Update `schema_version`
After successful migration:
```sql
UPDATE schema_version SET number = 16;
```

## Transactions & Backups
- Keep the existing `backupIfNecessary` logic: trigger a `-before-migration` backup before any structural change.
- Wrap each schema bump block (`currentVersion < 16`) inside `SQLite::Transaction` so the whole import conversion either completes or rolls back.
- Temporarily disable `PRAGMA foreign_keys` if we need to drop/recreate tables, then re-enable and `VACUUM` similarly to earlier migrations.
- The heaviest statements are the bulk `INSERT ... SELECT` queries for import lists and memberships. They should run within a single transaction to avoid repeated scanning.

## Performance Notes
- Ensure indexes support the joins: existing `patch_sourceid_idx` keeps the `patches` scan fast. Consider adding `(synth, sourceID)` composite if we observe slow migrations on large DBs.
- Use `INSERT INTO ... SELECT DISTINCT` rather than `GROUP BY` if more efficient on SQLite version in use; test with large fixtures.
- After migration completes, call `removeAllOrphansFromPatchLists()` to clean up entries for patches deleted during import conversion.
