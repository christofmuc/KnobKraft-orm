---
tutorial: connect
---
# Connect KnobKraft and your synth

**Goal:** establish a MIDI connection in both directions and hear your instrument through its audio output.

## Before you begin

[Install KnobKraft](../download.md), check the [supported-synth list](../supported-synths.md), and power on one instrument. Read its setup help in KnobKraft's **Settings** tab and the instrument's own MIDI instructions. SysEx reception, device ID, USB/DIN selection and memory protection differ between models.

## Make the connections

1. Connect the computer's MIDI **output** to the synth's MIDI **input**. Connect the synth's MIDI **output** back to the computer's MIDI **input**. A USB synth may provide both ports through one cable.
2. Connect the synth's **audio output** to headphones, a mixer or an audio interface. MIDI carries messages, not the sound you hear.
3. Open **Setup**, activate the matching synth and click **Auto-Detect**.
4. Check **Sent to device**, **Receive from device** and **MIDI channel**. If detection cannot fill them in, follow [manual connection setup](../manual/02-midi-setup.md#enter-connection-details-manually).
5. Check a small supported retrieval. For a synth with edit-buffer retrieval, use **MIDI → Import edit buffer from synth** (**F8**) and inspect the returned patch. If your synth requires a manual dump, use its [manual receive procedure](../manual/03-importing.md#receive-a-manual-dump).

## Check your result

You should have the intended MIDI ports and a recognised patch returned by the instrument. Play the synth and confirm that you hear its audio. Successful detection alone does not prove that every transfer function works.

If notes work but patches do not, check SysEx reception and the device-specific settings. If nothing returns, check the cable directions and **MIDI Log** before trying again. See [connection troubleshooting](../help.md#my-synth-is-not-detected-or-does-not-change-sound).

Before selecting library patches, learn [where your sounds live](library-and-hardware.md). The **edit buffer** send-mode label does not guarantee temporary storage on every synth.
