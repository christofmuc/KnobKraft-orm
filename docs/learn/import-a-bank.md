---
tutorial: import-a-bank
---
# Import a bank and keep a shortlist

**Goal:** load a compatible patch file, keep a few useful sounds together and save a recoverable library copy.

You need KnobKraft and a file for a [supported synth](../supported-synths.md). Hardware is optional for importing and organising. Keep the original file: an import grouping need not preserve the file's original slot arrangement.

## Import the file

1. Activate the synth in **Setup** and select it in **Library**.
2. Choose **Patches → Import from files into database** (**F3**), select the file and open it.
3. Complete any naming dialog and wait for the import to finish.
4. Expand **By import** to find newly added sounds. If nothing new appears, select the synth's whole library and click **Clear filters**.

**Check:** you can find recognised patches in the library. Fewer new patches than file slots can be normal: duplicate recognition uses synth-specific fingerprints. Reimporting known sounds may create no new import entry. [Understand import counts](../manual/03-importing.md#understand-a-smaller-than-expected-import).

## Hear a few sounds, if connected

Read [audition modes](../manual/04-browsing-and-auditioning.md#choose-how-a-patch-is-sent) and confirm the send destination for your model. Preserve any unsaved edit or affected stored program first. Click a patch in the grid, wait for the transfer or recall, then play the instrument. Repeat with another sound.

Working offline? Continue by selecting individual patch entries in the library tree to inspect them. You can organise the file now and audition later.

## Make a shortlist

1. Expand **User lists** and choose **Add new list**.
2. Give it a name such as **First session**, choose **No fill**, and click **OK**.
3. Drag a few patches from the grid onto the list.
4. Select the list and check its contents. These are references to library patches, so no hardware slots have been written by making the list.

## Keep a database copy

Choose **File → Save database as...** and use a new, dated `.db3` name. Check that your patches and list are present. **KnobKraft opens this new copy as the active database.** Reopen the original if that is where you want to continue working.

**Finished:** you can find the imported sounds, reopen the shortlist and identify which database is active. To preserve the whole collection, keep a separate database backup; a SysEx or PIF export has a different purpose. See [export and backups](../manual/07-export-and-backups.md).
