# Roland dump validation and fingerprint compatibility

The fixes for #560–562 affect the shared `roland/GenericRoland.py` implementation
(JV-80 family, JV-1080 family, native/compatible XV-3080 imports, and JD-Xi).
The separate D-50 and Juno-DS implementations are unchanged.

## Existing databases

Ordinary, correctly ordered program dumps with the nominal block lengths and
device ID `0x10` keep their previous fingerprints. Regression tests pin the old
JV-80, JV-1080, XV-3080 and JD-Xi fixture hashes as literal values. Program
locations, edit-buffer addresses, device IDs, message order and patch names now
produce the same identity for the same sound.

Some old hashes must change: edit-buffer hashes for models with different
program/edit addresses, reordered dumps, non-default device IDs, and JV variant
lengths whose old positional masks landed in payload data. It is impossible to
retain every old hash while making those representations share one identity.

Before importing more patches with the updated adaptation:

1. Keep a backup of the database and any recordings. Restart Orm after updating
   adaptations, to discard the in-memory fingerprint cache.
2. Select each affected synth separately and use **Edit > Reindex patches...**.
   This includes hidden patches and creates a `-before-reindexing` database backup.
3. Check patch counts and user/bank lists. Equivalent sounds may merge and the
   retained name may differ when duplicates had different names. Keep the backup
   if that choice needs to be reviewed.

This uses the existing explicit migration workflow; installing the adaptation
does not automatically rewrite database keys. Until reindexing, old affected
rows can coexist with newly imported canonical rows. Do not manually replace
hashes in the database: list references must be remapped as well.

### Audit of the existing migration path

At the MidiKraft revision pinned by this change's base,
`30777d50b29ec8abbea6d86269f3a39ebca25488`, `PatchDatabase::reindexPatches`
retrieves old and recalculated hashes, merges patches through the existing
metadata merge logic, updates `patch_in_list` references, and deletes superseded
rows within a SQLite transaction. `PatchView::reindexPatches` makes the backup
and asks the user to confirm. No database schema or submodule changes are needed.

The migration does **not** rename files outside the database: thumbnail `.kkc`
and prehear `.wav` files are named by fingerprint. If an affected patch has a
recording, retain the original and copy it under the new hash after reindexing;
do not overwrite an existing recording when several old hashes merge. Thumbnails
can be regenerated. Keep the database backup together with these files for
rollback. This audit is of the code; the Python regression suite does not run the
C++ database migration or exercise hardware.

Malformed dumps previously admitted by the loose validator are now rejected.
Reindexing cannot repair a missing block, bad checksum or payload already moved
to the wrong address by an earlier conversion. Recover such a patch from its
original valid dump or from the synth before migration.

## Canonical representation

1. Validate every DT1 frame: manufacturer/model, command, 7-bit framing, device
   ID, checksum, concrete address, and supported payload size.
2. Require one block of each identity in a single device/program context.
3. Match source and destination blocks by their relative address identities.
4. Serialize in program-layout order at program zero with device ID `0x10`.
5. Zero each checksum and the legacy program-position byte; zero only the
   configured name block, offset and length. Hash the resulting bytes with MD5.

The transport headers are retained to preserve the ordinary legacy program
hashes and distinguish protocols. Masks are applied inside each actual frame;
variant block lengths cannot shift a mask into the next block. Every sound byte
outside the name region participates, including variant extensions and bytes
immediately before checksums.

## Input policy and model extensions

Reordered blocks are accepted and emitted in declared layout order. Identical
or conflicting duplicates, unknown addresses, missing blocks, mixed devices or
programs, interleaved garbage, truncated frames and unsupported layouts are
rejected. Recognition returns `False`; conversion, rename and fingerprinting
raise `ValueError`. Name/tag lookup returns `Invalid` / an empty list.

JV-80/880/90/1000 tone lengths `0x73`, `0x74`, `0x75` and JV-1080/2080 common
lengths `0x48`, `0x4a` are explicit supported layouts. All four JV-80-family tones
must have the same supported size; mixing variants is rejected. Payloads are
preserved at their original lengths, without padding or truncation.

Adaptations with other addressing schemes should override
`_address_for_sub_request(layout, block_no, item)`, which is shared by recognition,
requests and conversion. The address map must identify every block and item
uniquely and the layout must remain fixed after construction. Use
`patch_name_message_number`, `patch_name_offset` and `patch_name_length` for
names instead of overriding positional stream operations. Declare additional
complete length tuples with `RolandData(..., supported_layouts=[...])`.

The open JD-800, SC-88ST Pro and SD-90 PRs (#538–540) need companion changes to
remove their positional validators/fingerprinters; the SD-90 retains its custom
part-stride address hook. Merge/rebase the shared fix before applying those
companion patches. Those new adaptations are deliberately not introduced here.
