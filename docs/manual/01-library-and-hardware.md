# Your library and your synth

[Manual contents](../index.md)

## Know where the sound is

A **patch** is a sound's data plus the information KnobKraft keeps about it. The **database** stores those patches on your computer. A patch may appear in several lists without becoming several independent copies of the sound.

Your hardware has separate storage. A **stored program** is a sound in a numbered hardware location. An **edit buffer**, where supported, holds the sound currently being edited or played. Selecting another program or sending another patch can replace the current editable sound. Capture an edit you want to keep before doing either.

| From | Action | To |
| --- | --- | --- |
| Patch file | File import | Computer database |
| Hardware programs | Bank retrieval | Computer database |
| Current hardware edit | Capture, if supported | Computer database |
| Computer database | Audition / send | Device-dependent hardware destination |
| Computer database | Export | Patch file |

The audition destination is conditional: some synth integrations send to a stored program location instead. See [audition modes](04-browsing-and-auditioning.md#choose-how-a-patch-is-sent) before clicking patches on connected hardware.

## Choose the right container

| Container | What it represents | Useful for |
| --- | --- | --- |
| Database | The open library file, including patches and organizational data | Keeping your collection together and backing it up |
| By import entry | A source grouping created during import | Finding newly introduced sounds from a file or capture |
| User list | An ordered collection of references to patches; can contain different synths | Shortlists, projects, and listening sessions |
| User bank | A synth-specific arrangement with a target bank and capacity | Preparing a set of hardware program slots |
| Synth bank | KnobKraft's stored view of a hardware bank | Inspecting or rearranging known hardware locations |

A file called a “bank” need not become a user bank when imported. File import adds recognized patches and import organization. Build a [user bank](06-lists-and-banks.md#prepare-a-user-bank) when you need deliberate program placement.

## Recognize a snapshot

The hardware bank view records a retrieval and displays elapsed time for an active synth bank. It is not a continuous readout of every front-panel change. If you rearrange programs on the synth or with another librarian, KnobKraft's known positions can become stale.

In **Current Patch**, **Program** describes the patch's recorded program information; **In synth at** lists locations known from the database's bank records. Neither field alone proves what is in the hardware now. [Import again](06-lists-and-banks.md#refresh-a-hardware-bank) refreshes the bank view.

## Build a useful library

**Suggested workflow:** import a collection, audition a few sounds, mark favorites, add comments, then place the useful sounds in a project list. Use a user bank only when the project needs a hardware slot arrangement. Export the arrangement separately from backing up the database.

KnobKraft's duplicate recognition depends on the synth's patch fingerprint. Importing the same sound again may reuse an existing database patch and update metadata. A name change alone is not a dependable way to create a separate version. See [capture versions](03-importing.md#capture-an-unsaved-edit).

[Glossary](glossary.md)
