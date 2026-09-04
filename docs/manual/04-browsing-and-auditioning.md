# Browse and audition

[Manual contents](../index.md)

## Choose how a patch is sent

Check **send mode** before selecting a patch on connected hardware. The setting is stored per synth.

| Mode | Source behavior | Prerequisite or consequence |
| --- | --- | --- |
| `program change` | Recalls the first known hardware location for the patch | Import hardware banks first; if no position is known, the code logs a message and does not send the patch |
| `edit buffer` | Sends patch data through the synth integration | A true temporary edit buffer is device-dependent |
| `automatic` | Uses program change when a hardware location is known; otherwise sends patch data | Can take either path, depending on the database's bank records |

**The label `edit buffer` is not a universal guarantee of temporary storage.** The underlying sender can fall back to a stored program location when the synth has program-dump support but no edit buffer. The integration may specify that location; the generic fallback derives one from bank information. Establish the exact behavior for your synth and preserve any affected stored program before using this mode.

Known hardware positions can become stale after changes made outside KnobKraft. In automatic or program-change mode, that can recall a different sound than the database patch. [Refresh the hardware bank](06-lists-and-banks.md#refresh-a-hardware-bank) to update the record.

## Hear two imported sounds

**Before you start:** configure MIDI and audio, import compatible patches, and capture any edit you want to keep. For a temporary audition session, use a synth with verified edit-buffer sending.

1. Select the synth and the desired import or list in **Library**.
2. Choose the appropriate **send mode**.
3. Click a patch in the main grid. Check its name in **Current Patch**.
4. Allow the hardware transfer or recall to complete, then play a short phrase.
5. Click a second patch in the grid and play the same phrase.

**Expected result:** the grid selection becomes the current patch and requests the chosen send behavior. The hardware should sound the selected patch if the connection and device capability work. Allow enough time for the transfer: transfer length and instrument response vary.

Selecting a patch entry in the navigation tree updates the current patch without sending in the navigation tree. Clicking a grid patch, a patch in **Synth Bank** or **Recent Patches**, or the current patch's name invokes selection with sending. Use the grid for the steps above so selection and audition are not confused.

## Keep track of what you like

1. Select a patch and open **Current Patch**.
2. Click **Fav!** to mark a favorite, **Regular** to mark a normal keeper, or **Hide** to set the hidden flag.
3. Use the corresponding [search controls](05-search-and-categories.md#find-favorites-or-recover-hidden-sounds) to browse those groups later.

**Expected result:** the database metadata changes. The current grid is intentionally updated in place for these actions; a patch can remain visible until the view is refreshed even if it no longer matches a filter.

These are toggles, not an undo system. Regular clears favorite and hidden when enabled. Favorite and hidden can coexist in this workflow. To remove an accidental hide, explicitly turn off **Hide**, or mark **Regular** if that is the state you want.

## Return to a recently heard patch

Open **Recent Patches** and click the desired entry. It recalls the patch through the same send-mode logic. Use a [user list](06-lists-and-banks.md#make-a-shortlist) for a shortlist you want to preserve; recent selections are not a version archive or a database backup.

[No sound or wrong sound?](../help.md)
