# Yamaha SY77 and TG77 voice support

The two adaptations share the Yamaha voice SysEx format. Both expose 64 writable Internal slots and two read-only 64-voice preset banks. The TG77 uses a timed Edit → Dump → Voice sequence to read the edit buffer. The SY77 sends the edit-buffer request directly.

For SY77 Program change send mode, set Program Change to Direct. The app sends the voice-bank selector followed by the slot. For TG77 Program change send mode, set Program Change to Normal and select Internal in Voice Play before auditioning internal patches. Use edit-buffer send mode to audition TG77 presets. Program Change uses the Voice Receive Channel from the System dump, with the SysEx device number as a fallback. Run detection again after changing these settings.

Detection requests the first factory preset and the System dump so it can identify the model and read the MIDI channel without changing Bulk Protect. The app also accepts the TG77's front-panel voice dumps. A front-panel dump of A01 carries the same address as an edit-buffer dump and is classified as an edit-buffer voice.

Turn Bulk Protect off on the instrument before writing to internal memory. 4AFM mono, 4AWM, 2AFM+2AWM, and Drum Set voices require a slot in D01–D16. The adaptation rejects an incompatible destination. The dedicated bank-dump writer is deferred. The app can still send a bank as individual slot writes, so an incompatible slot can stop a transfer after earlier valid slots have been written.

## Contributors and hardware evidence

| Contributor | Hardware evidence from the original contributions |
| --- | --- |
| [@hmmbug](https://github.com/hmmbug) | Wrote the original SY77 and TG77 adaptations, supplied their bank mapping and three fixtures, and reported several weeks of stable use of the SY77 adaptation on an SY77 with ROM 1.5. See [PR #551](https://github.com/christofmuc/KnobKraft-orm/pull/551) and the [SY77 hardware report](https://github.com/christofmuc/KnobKraft-orm/pull/551#issuecomment-4946532875). |
| Giovanni Candiago | Wrote the TG77-tested adaptation and supplied a fixture with 64 internal voices and all 11 voice modes. On a TG77 he verified detection with the original System-dump probe, edit-buffer retrieval including the largest voice and send to the edit buffer, reads of all 64 internal voices, front-panel single and full-memory dumps, and Normal-mode Program Change with SysEx device number 1 and Voice Receive Channel 9. See [PR #597](https://github.com/christofmuc/KnobKraft-orm/pull/597). |

These reports cover the contributors' original versions. The combined version still needs retesting on both instruments.

## Combined-version retest

- On each model, run detection with different SysEx and voice channels. Confirm the model is identified, the program change arrives on the voice channel, and Bulk Protect retains its initial setting. When both units are available, check that only the matching adaptation detects each unit.
- On each model, read Internal and both preset banks. Retrieve a 4AFM mono edit-buffer voice and send a voice back to the edit buffer. On TG77, confirm the front panel returns to Voice Play and verify both a single front-panel dump and a full 64-voice dump.
- On SY77, in Direct mode, audition an internal voice and a voice from each preset bank. Start in Multi mode for the Preset 1 check and verify that the synth switches to the correct Voice bank.
- On TG77, in Normal mode, select Internal in Voice Play and audition an internal voice. Switch to edit-buffer send mode to audition a preset.
- Back up internal memory before testing a write. Test individual writes to scratch slots and read them back before attempting any bank transfer. Check that an incompatible write to A–C leaves the slot intact and that an explicit D-slot write works for the same voice.

The bank selectors are documented in the [Yamaha SY77 operating manual](https://chrisarndt.de/files/yamaha/sy77/SY77%20-%20Operating%20Manual%20-%20Complete.pdf) and [Yamaha TG77 MIDI Data Format](https://manualzz.com/doc/1538269/yamaha-tg77-user-manual).
