# MIDI setup and detection

[Manual contents](../index.md)

## Connect one synth

**Before you start:** use a synth integration present in your installed build, power on the instrument, and connect MIDI in both directions. For a DIN interface, the computer's MIDI output goes to the synth's MIDI input; the synth's output returns to the computer's input. USB instruments may expose these ports directly. Follow the instrument's own instructions for SysEx reception, device ID, and MIDI mode.

1. Open **Setup**.
2. Activate support for the synth you want to use.
3. Click **Auto-Detect**, or choose **MIDI → Auto-detect synths** (**F1**).
4. Review the synth's **Sent to device**, **Receive from device**, and **MIDI channel** values.
5. Select the synth in the Library and perform a small supported retrieval, such as an [edit-buffer capture](03-importing.md#capture-an-unsaved-edit), to check that patch data returns.

**Expected result:** detection establishes a working bidirectional connection and updates the connection settings. A successful detection is useful evidence of MIDI communication; it does not verify every import/send function or the audio connection.

**Device-dependent:** detection, channel interpretation, and supported requests vary by integration. A supported synth may still need hardware settings before it responds. Use the device-specific help in Settings for the expected device ID and MIDI mode.

## Enter connection details manually

1. In **Setup**, locate the active synth's settings.
2. Set **Sent to device** to the computer output connected to that synth.
3. Set **Receive from device** to the computer input receiving that synth's replies.
4. Set **MIDI channel** to the value expected by the integration and instrument.
5. Choose **MIDI → Quick check connectivity** (**F2**) and inspect the result.

**Expected result:** KnobKraft uses the entered connection details. Changing this channel setting does **not** change the synth's own MIDI channel. Change hardware settings on the instrument when required.

Some paths only require a configured channel; bank management also checks detection. If one action works and another reports **Synth not connected**, recheck detection rather than assuming the action shares the same prerequisites.

## Hear what you send

Patch data and MIDI notes are separate from audio. Connect the synth's audio output to headphones, a mixer, or an audio interface and monitor that signal. A visible patch selection is not proof that the instrument received it or that audio is being monitored.

If you use a separate master keyboard, configure MIDI forwarding in Setup after verifying the synth connection. First establish one dependable input/output path; then add other devices.

## Check communication without guessing

Use **MIDI → Log only Sysex messages** while investigating patch transfers, or **Log all MIDI messages** while investigating notes. Read the **MIDI Log** tab. **MIDI → Check for MIDI loops** sends test messages and reports loops in the log. Record which ports and message directions appear.

The MIDI Log's **Sysex entry** field is an advanced sender. In this workflow, it sends to the current output set, not just the selected synth. It is not needed for routine setup; do not use an arbitrary example message as a connectivity test.

[Troubleshooting](../help.md)
