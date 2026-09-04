# Lists and banks

[Manual contents](../index.md)

Use a **user list** to collect sounds for listening or a project. Use a **user bank** when you need an arrangement for a specific synth's program slots. Editing these collections changes the computer database; **Send to synth** is a separate operation.

## Make a shortlist

1. In the Library tree, expand **User lists** and click **Add new list**.
2. Enter a descriptive **Name**.
3. Choose **No fill** under **Auto-fill from grid** for an empty list, then click **OK**.
4. Drag a patch from the grid onto the list. Repeat with other patches.
5. Select the list to inspect its contents. Expand its tree entry to see individual entries.

**Expected result:** the ordered list is stored in the database. A list may contain patches from several synths. Adding a patch to another list references the same library patch; changing that patch's metadata can be visible wherever it appears.

For an initial batch, filter the grid before creating the list and choose **First patches**, **From active patch**, or **Random patches**, with **Maximum number of patches** as appropriate. These are fill operations at creation time, not saved searches that continuously update membership.

To rename a list, double-click its tree entry, edit **Name**, and click **OK**. The same dialog offers **Delete List**, which removes the list definition while leaving the patches in the database.

## Remove an entry without deleting its patch

1. Expand the user list in the tree.
2. Drag that list's individual patch entry onto the trash can.
3. Inspect the list and then the synth library.

**Expected result:** the entry is removed from that list immediately, and the patch remains in the database. Dragging a patch from the main grid to the trash requests database deletion instead. The drag origin matters.

Database deletion has no undo. Patches referenced by bank definitions can be hidden instead of removed; the log explains this outcome. Prefer **Hide** for reversible listening decisions.

## Prepare a user bank

**Device-dependent:** bank count, names, sizes, writable status, and compatible patch types come from the synth integration.

1. Select the intended synth in the Library tree.
2. Open **User Banks** and click **Add new user bank**.
3. Enter a **Name** and select the target **Bank**. Read the target carefully: it will matter when sending to hardware.
4. Choose **No fill** for deliberate slot-by-slot preparation, or a supported **Auto-fill from grid** mode.
5. Click **OK**, select the new bank, and inspect **Synth Bank**.
6. Drag compatible patches onto the intended slots in that panel. Inspect slot order and names after each change.

**Expected result:** the arrangement is saved in the database. The user-bank header identifies the bank it loads into. A bank has finite capacity and belongs to one synth; a mixed-synth list is not a universal hardware bank. List-to-bank drops can filter out other synths and truncate to available space, so always inspect the result.

Changing the target bank after creation is not enabled in the rename dialog. Prepare a new user bank with the intended target when needed. Do not assume banks with different names or sizes are interchangeable.

## Refresh a hardware bank

1. Select the synth's hardware bank under **In synth** in the Library tree.
2. Inspect the bank name, any **[ROM]** marker, and time since synchronization in **Synth Bank**.
3. Click **Import again** when you need the current hardware contents.
4. If prompted about unsent changes, decide whether to preserve the arrangement before replacing it.
5. Wait for retrieval and verify representative slots.

**Expected result:** the database's active bank view is rebuilt from the hardware response. This operation can discard your pending local rearrangement of that active bank. Use a user bank and exports for arrangements you want to keep independently.

## Send an arrangement to hardware

This operation writes sounds to hardware storage where supported. Preserve the existing hardware contents through [retrieval and export](07-export-and-backups.md#preserve-a-hardware-bank) before replacing them.

1. Select the prepared bank and verify synth, target bank, capacity, and slot contents.
2. Confirm the synth is connected and detected, with any required hardware write settings configured.
3. Click **Send to synth** in **Synth Bank**. Do not rely on an additional confirmation appearing.
4. Allow the transfer to finish. If the application reports an incomplete update, treat the hardware bank as potentially only partly changed.
5. Recall representative hardware programs and, when appropriate, reimport into the active bank view to check the result.

**Expected result:** the integration sends the bank arrangement. The sender can transmit an entire bank or individual program dumps; do not infer the number of written slots from the number of visible edits. A successful send callback is not necessarily a readback verification from the instrument. **Send to synth** is shown only for banks the application considers writable.

[Export without sending](07-export-and-backups.md#export-a-bank)
