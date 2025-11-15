# PatchDatabase Fixture Doctest

Run the doctest with:

```bash
python -m doctest docs/patchdatabase_doctest.md -v
```

This uses the fixture created under `tests/fixtures/imports_v14.db3` to confirm the legacy schema and import wiring still behaves as expected.

>>> from pathlib import Path
>>> import sqlite3, json
>>> db_path = Path("tests/fixtures/imports_v14.db3")
>>> db_path.exists()
True
>>> conn = sqlite3.connect(db_path)
>>> cur = conn.cursor()
>>> cur.execute("SELECT number FROM schema_version").fetchone()[0]
15
>>> cur.execute("SELECT name, id, date FROM imports WHERE synth = ?", ("TestSynth",)).fetchall()
[('Bulk import Jan 2024', 'import-123', '2024-01-01 12:00:00')]
>>> cur.execute("SELECT synth, md5, name FROM patches WHERE synth = ?", ("TestSynth",)).fetchall()
[('TestSynth', 'md5-aaa', 'Bass 01')]
# Legacy fixtures (schema ≤17) also expose the now-removed sourceID column:
>>> cur.execute("PRAGMA table_info(patches)").fetchall()[8][1]
'sourceID'
>>> source_blob = cur.execute("SELECT sourceInfo FROM patches WHERE md5 = 'md5-aaa'").fetchone()[0]
>>> payload = json.loads(source_blob)
>>> sorted(payload.keys())
['bulksource', 'fileInBulk', 'timestamp']
>>> payload["fileInBulk"]["filename"]
'bank1.syx'
>>> conn.close()
