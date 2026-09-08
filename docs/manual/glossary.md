# Glossary

[Manual contents](../index.md)

| Term | Meaning in this manual |
| --- | --- |
| Adaptation | A synth integration, often a Python script, that recognizes and generates device-specific messages. Its capabilities determine which workflows work. |
| Active synth | A model enabled in Setup; activation does not prove hardware connectivity. |
| Selected synth | The synth currently selected for synth-level commands such as file import and edit-buffer retrieval. |
| Current patch | The patch selected for display; in multi-synth mode it can belong to a different synth from the selected synth. |
| Patch | A sound's supported data and associated library metadata. Some device dumps contain layered or other structured data. |
| Database | The open KnobKraft `.db3` library file containing patches and organization. It is separate from hardware memory. |
| Library | The collection shown from the database, often scoped to a synth, import, list, bank, or filter. |
| Import | Reading recognized data from files or hardware and merging it into the database. |
| By import | Navigation branch grouping imported patches by recorded source. In this workflow, normal reimports do not necessarily record already known patches in a new grouping. |
| User list | Ordered references to library patches, potentially spanning synths; useful for a shortlist. |
| Bank | A set of program positions. Distinguish a bank file, a database user bank, an active synth-bank snapshot, and actual hardware storage. |
| User bank | A database arrangement for a particular synth and target bank. |
| Active synth bank | A database record of a hardware bank and its last synchronization, used to track known positions. |
| Stored hardware program | A sound in a numbered location on the instrument, selected by its supported program/bank protocol. |
| Program change | A MIDI instruction to recall a stored location; it does not carry the database patch's complete sound data. |
| Edit buffer | Temporary current-sound data on hardware that supports it. KnobKraft's identically named send mode can fall back to stored-program sending. |
| Dump | A transfer of device data, commonly SysEx, for one patch, a bank, or another supported data type. |
| SysEx | MIDI System Exclusive messages carrying device-specific data. These are neither audio recordings nor universally interchangeable patches. |
| Audition | Select or send a patch and play the synth to judge the sound. |
| Fingerprint | An integration-dependent identity derived from patch data, used for duplicate recognition. It is not simply the visible name. |
| Metadata | Library information such as name, author, comments, favorite state, categories, and source details. Some names can also be encoded into patch data. |
| Category | A classification assigned manually or by rules and usable as a search filter. |
| Hidden | A patch flag that affects visibility. Hiding is reversible and different from database deletion. |
| Regular | An explicit ordinary-keeper state; enabling it clears favorite and hidden in the UI. |
| Undecided | A patch without favorite, hidden, or regular status. |
| PIF | Patch Interchange Format: JSON with patch data and supported metadata; not a full database backup. |
| Macro | An action triggered by a configured MIDI note or note combination in the Macros tab. |
| Snapshot | A recorded state at a point in time; it can become stale when hardware is changed elsewhere. |

See [library model](01-library-and-hardware.md), [audition](04-browsing-and-auditioning.md) for the behavior behind these definitions.
