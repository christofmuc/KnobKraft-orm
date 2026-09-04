# FAQ / Help

Start with the symptom. Keep the original patch files and make a [database copy](manual/07-export-and-backups.md#save-a-database-copy) before changing an important library.

## My synth is not detected or does not change sound

Check MIDI in both directions, the selected send/receive ports and the synth's own SysEx settings. Notes working does not prove that the instrument accepts patch data. Check the **Settings** help for the exact model; device ID, channel and USB/DIN choices are device-dependent.

Use **MIDI Log** to distinguish outgoing requests from incoming replies. A “sent” log entry is not an acknowledgement from the instrument. Follow [MIDI setup](manual/02-midi-setup.md) and check the manufacturer's instructions. Do not apply a DX7II procedure to an original DX7 just because their names are similar.

## Clicking a patch plays the wrong sound, or nothing

Check **send mode**. **program change** recalls a location known from a retrieved hardware bank; with no known location it sends nothing. **automatic** uses a known location when possible, otherwise it sends patch data. A hardware bank changed outside KnobKraft can leave that recorded location stale. Use **Import again** to refresh it.

The **edit buffer** mode sends data through the integration. Some synths receive that data in stored memory. Establish the destination before using it. See [audition modes](manual/04-browsing-and-auditioning.md#choose-how-a-patch-is-sent). Old advice to delete a bank to force a SysEx send predates this chooser.

## An import added fewer sounds than the bank contains

KnobKraft recognises duplicates using the synth's fingerprint calculation. Multiple source slots can represent one library patch, and an import grouping may contain only newly added sounds. Reimporting an already known bank can add nothing new. This is separate from **Duplicate Names**, which finds repeated names rather than identical sound data.

Keep the original file for its original packaging. Use the synth's whole library and clear filters when checking results. See [import counts](manual/03-importing.md#understand-a-smaller-than-expected-import).

## Where did my patches go?

First select the synth's whole library or **All patches**, then click **Clear filters**. Clearing filters alone does not leave a selected import, list or bank. For a hidden patch, enable **Hidden**; if it was also a favourite, enable **Faves** too. Turn off **Hide** in **Current Patch** to recover it.

Deleting a list and deleting its patches are different operations. Patches referenced by banks can be hidden instead of deleted to preserve the arrangement. See [search and categories](manual/05-search-and-categories.md) and [lists and banks](manual/06-lists-and-banks.md).

## Do I need to save a user bank manually?

Bank slot edits are saved in the database. **Send to synth** remains a separate action that writes the hardware arrangement. Older instructions mentioning a special **Save to database** step describe earlier behaviour; the [2.6.1 release notes](https://github.com/christofmuc/KnobKraft-orm/blob/master/release_notes/2.6.1.md) describe its removal. See [prepare a user bank](manual/06-lists-and-banks.md#prepare-a-user-bank).

## Is an exported file a complete backup?

A database copy preserves patches and library organisation. SysEx/MIDI exports carry compatible patch messages. PIF carries patch data and supported metadata, but does not preserve every flag or the list/bank definitions. Keep application settings and custom adaptations separately too. See [backup choices](manual/07-export-and-backups.md).

## Do I need an old Python installation on my Mac?

The macOS DMG bundles Python starting with KnobKraft 2.5.0. Old Homebrew and symlink workarounds for older versions are not current installation steps. Use the [download instructions](download.md); include your exact macOS version, processor and KnobKraft version if a build fails to launch.

## Can KnobKraft recall my whole DAW session?

The released workflow described here is a standalone librarian. It manages patches and transfers them to supported hardware. Do not rely on a DAW project to restore the library or hardware state; save your library and document your hardware bank arrangement separately.

## Ask for help or report a problem

Search [GitHub Discussions](https://github.com/christofmuc/KnobKraft-orm/discussions) for setup questions and [issues](https://github.com/christofmuc/KnobKraft-orm/issues) for bugs or missing synth support. Include:

- KnobKraft version, operating system and exact synth model/firmware.
- MIDI interface and port configuration; the action you tried and the expected result.
- The relevant log excerpt and a small reproducible patch file, if you have permission to share it.

Please report successful testing of alpha/beta integrations too. It helps improve the compatibility information for everyone.
