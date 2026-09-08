---
tutorial: library-and-hardware
---
# Know where your sounds live

**Goal:** identify which copy of a sound you are working with before importing, organising or sending it.

## Two places, different jobs

Your **library/database** is a file on the computer. It holds patch data and your organisation. The **synth** has its own stored programs and, on many models, an edit buffer for the current sound. Opening a database does not load its contents into the synth.

| What you see | What it means |
| --- | --- |
| Patch | Sound data with information such as its name, categories and notes |
| By import | A grouping of newly added patches from an import; not necessarily every slot in the source file |
| User list | Ordered references to library patches, useful for a project or shortlist |
| User bank | An arrangement for one synth and a particular target bank |
| In synth / synth bank | KnobKraft's recorded view of hardware slots, which can become out of date |
| Stored program | A sound saved in a numbered location on the instrument |
| Edit buffer | The current editable sound on hardware that supports temporary storage |

## Follow one sound

1. **Import a file.** KnobKraft recognises its patches and merges them into the database. A bank file does not automatically become a user bank.
2. **Add a patch to a user list.** You have made a reference to the library patch. You have not stored it on the synth.
3. **Audition a patch.** Depending on the send mode, KnobKraft recalls a known hardware location or sends patch data. The destination of that data depends on the synth; some integrations write a stored location.
4. **Prepare a user bank.** You arrange slots in the database. **Send to synth** is the separate operation that writes the arrangement to hardware.

Changing programs or sending a new sound can replace an unsaved edit. [Capture an edit you want to keep](../manual/03-importing.md#capture-an-unsaved-edit) before doing either.

## Check your understanding

You add one patch to two lists. How many independent library sounds did you create? **One patch, referenced twice.**

You move sounds around in a user bank. Have the synth's stored programs changed? **Only after a hardware send; arranging the bank saves the computer copy.**

Next: [import a bank and keep a shortlist](import-a-bank.md). For reference, see [the library model](../manual/01-library-and-hardware.md) and [glossary](../manual/glossary.md).
