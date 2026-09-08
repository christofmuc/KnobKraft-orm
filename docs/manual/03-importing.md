# Import sounds

[Manual contents](../index.md)

Import puts recognized patch data in the open computer database. Audition and bank sending are separate actions. Begin in the database where you want the sounds to live; use **File → Open database...** to switch libraries.

## Import a file

**Before you start:** know which synth the file was made for. File import uses the selected synth's loader. Hardware need not be connected just to load a compatible file.

1. Activate the matching synth in **Setup** if necessary, then select its Library entry.
2. Choose **Patches → Import from files into database** (**F3**).
3. Select the file or files and open them.
4. If a naming dialog appears for patches without stored names, supply useful names and finish that dialog.
5. Wait for loading and database merging to finish. Open the synth's **By import** branch to inspect the result.
6. Check names and counts before moving on to [auditioning](04-browsing-and-auditioning.md).

**Expected result:** recognized new patches are saved and categorized. When new patches were added, the code refreshes the import tree and attempts to select the corresponding import entry.

The file chooser offers `.syx`, `.mid`, `.zip`, `.txt`, and `.json`, plus additional formats supplied by some integrations. This is a chooser filter, not a promise that every file with one of these extensions is compatible. JSON import expects KnobKraft's Patch Interchange Format; it does not accept arbitrary JSON sound descriptions. Files for a different synth are not automatically converted.

## Understand a smaller-than-expected import

Known patches are merged rather than always duplicated. The normal import path creates import-list entries from patches classified as new. Reimporting an already known file can therefore add no new patches and produce no new import grouping; existing metadata may still be updated.

An import entry is not a guaranteed complete manifest of every slot in the original file. Keep the original file if you need the original bank packaging or an exact source artifact. **Duplicate Names** in search finds repeated names, which is different from fingerprint-based duplicate recognition during import.

If nothing appears, select the synth's whole library and [clear the search filters](05-search-and-categories.md#start-a-fresh-search). Check the MIDI/application log for recognition or merge messages before repeating the operation.

## Retrieve stored programs from hardware

**Device-dependent:** the synth must support bank/program retrieval through its integration. Capture any valuable unsaved edit first; some retrieval strategies select programs while reading them.

1. Verify [MIDI setup](02-midi-setup.md) and select the intended synth.
2. Choose **MIDI → Import patches from synth** (**F7**).
3. Select the banks to retrieve and click **Import selected**, or choose **Import all**.
4. Wait for the transfer to finish and inspect the returned names and bank contents.
5. [Save a database copy or export the bank](07-export-and-backups.md) when the retrieval is satisfactory.

**Expected result:** recognized patches enter the database, and this revision also stores active synth-bank lists for the retrieved banks. Those lists provide known hardware positions for program-change audition. The import grouping may show only newly added sounds; inspect the bank view to assess program arrangement.

## Receive a manual dump

Use this path when the synth can transmit the required dump from its front panel and its integration can decode that dump.

1. Select the synth and verify its receive port.
2. Choose **MIDI → Receive manual dump** (**F9**).
3. While the waiting window is open, start the appropriate dump on the instrument.
4. Wait until the instrument finishes sending, then click **Stop** in the waiting window.
5. Inspect the imported result and log.

**Expected result:** captured messages are passed through the selected synth's manual-dump loader, and recognized patches are merged into the database. Received MIDI messages alone do not establish that the dump was complete or recognized. The instrument's exact transmit command remains a synth-specific prerequisite.

[Capture an unsaved edit](03-importing.md#capture-an-unsaved-edit)

## Capture an unsaved edit

If the integration supports retrieving the current edit, select the synth and choose **MIDI → Import edit buffer from synth** (**F8**). Wait for the returned patch and check its name and import result before selecting another sound. Capture does not itself store that edit in hardware program memory.

To keep two versions, change a sound parameter on the instrument, capture again, and check that both sounds exist in the library. A new name alone may leave the fingerprint unchanged.
