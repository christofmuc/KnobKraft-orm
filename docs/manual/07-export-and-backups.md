# Export and backups

[Manual contents](../index.md)

## Choose what you need to preserve

| Output | Intended use | Important limit |
| --- | --- | --- |
| SysEx or MIDI file | Transfer compatible patch messages to another tool or device | Does not preserve all KnobKraft organization and metadata |
| PIF JSON | Transfer patch data with supported patch metadata | Does not serialize the complete database, lists, or bank definitions |
| Database copy (`.db3`) | Preserve the library, including its organization | Separate application settings, custom adaptations, and rule files need separate preservation |

An exported file is not a hardware write. Loading or playing that file into an instrument later can write programs, depending on the messages it contains.

## Export a filtered collection

1. Select one synth's library, import, or list and set the desired filters.
2. Check the matching patch set. The export uses **all results matching the current filter**, including results beyond the visible page; it does not export only the current patch.
3. Choose **Patches → Export into sysex files**.
4. Select a supported **Sysex format** and **File format**, then click **Export**.
5. Choose the requested destination and verify the output files.

**Expected result:** the librarian writes the matching patches in the requested format. The export dialog offers full-bank, individual program-dump, or individual edit-buffer formats according to the synth's capabilities. File packaging choices are separate files, zipped separate files, one SysEx file, or one MIDI file (SMF).

For an intentional single-patch export, create a [one-entry user list](06-lists-and-banks.md#make-a-shortlist), select it, and verify the scope before exporting. Keep SysEx exports synth-specific until mixed-synth export behavior has been validated for your build. The dialog is created using the current synth, so its presence is not proof that every item in a mixed list can use the same output format.

## Export a bank

1. Select the exact bank in the Library tree and inspect **Synth Bank**.
2. Click **Export bank**, or choose **Patches → Export bank into sysex files**.
3. Choose supported format and packaging options and a destination.
4. Check the resulting file or files in a separate test database or a compatible inspector.

**Expected result:** the selected bank's patch collection is exported. With no bank selected, the action reports **Nothing to export**. Bank message encoding, target addressing, handling of empty slots, and round-trip slot order still require per-synth validation; do not call an untested export a verified hardware restore image.

## Export metadata in PIF

1. Select the library scope and filters you want to export.
2. Choose **Patches → Export into PIF**.
3. Save the `.json` file and check it exists.

**Expected result:** the output contains patch data, names, and supported metadata such as favorite state, manual category decisions, origin, comment, author, and info. The inspected serializer does not include the hidden or regular flags, user-list definitions, or bank definitions. A PIF file therefore complements a database backup rather than replacing it.

The ordinary file-import path loads PIF for the selected synth. **File → Export multiple databases...** writes JSON exports from database files in a chosen directory; **Merge multiple databases...** reads JSON files from a chosen directory into the open database. This is patch merging, not a full reconstruction of the source databases' organization. Make a database copy before using it on a working library.

## Save a database copy

1. Note which database you are working in and choose **File → Save database as...**.
2. Choose a new, distinct `.db3` filename, such as a dated copy.
3. Complete the operation and check the copied library's patches, lists, and banks.
4. If you intend to continue editing the original, use **File → Open database...** to reopen it.

**Expected result:** the application makes a database backup and then **opens the new copy as the active database**. Further edits go to that copy until you switch back.

Keep an independent copy in your own backup storage. The implementation also makes automatic database backups when a read/write database closes and before relevant migrations, with local retention management. Automatic copies beside the database do not protect against losing that storage device.

## Restore or test a backup

1. Preserve the current library with a distinct copy before replacing your working state.
2. Choose **File → Open database...** and select a copy of the backup you want to test.
3. Inspect known patches, comments, categories, lists, and banks.
4. Audition representative patches only after checking the send mode and device connection.

**Expected result:** KnobKraft opens that database as the working library. Opening a database does not restore all of its banks to hardware. Newer code may migrate an older database; retain the original backup if you need to reopen it with an older app version.

## Preserve a hardware bank

**Suggested workflow:** retrieve the bank, inspect its slot arrangement, save a database copy, export the bank, and record the synth model and firmware, KnobKraft version, bank name, file names, and date. A dependable restore procedure also needs a rehearsal on an appropriate target bank, followed by readback or listening checks. Check the restore procedure for your exact instrument before relying on it.
