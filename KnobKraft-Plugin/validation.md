# VST3 validation

## Automated checks

Build and run `knobkraft-recall-tests`. They cover bit-identical mono/stereo
float and double pass-through, zero reported latency, codec-backed state round
trip, plugin duplication, recoverable corrupt-state behavior, the complete UI
state reducer, and the manual-recall workflow over a temporary fake IPC server.
The IPC test covers connection, configured-synth rebind, patch search/selection,
Send/Cancel, save during transfer, editor close/reopen, and server reconnect.

For a `pluginval` installation, run:

```powershell
./scripts/validate-plugin.ps1 -PluginValPath C:/Tools/pluginval.exe -Vst3Path 'build/knobkraft-plugin/KnobKraftRecall_artefacts/Release/VST3/KnobKraft Recall.vst3'
```

## JUCE AudioPluginHost

Build `extras/AudioPluginHost` from the checked-out JUCE version, scan the VST3
bundle above, insert **KnobKraft Recall** on mono and stereo audio, and verify:

1. audio is unchanged and the host reports zero samples of plugin latency;
2. with KnobKraft closed, the editor says `Engine: Disconnected` and still shows
   the embedded fixture;
3. saving, closing, and reopening a project restores the same patch and fingerprint;
4. duplicating the plugin preserves its complete embedded state;
5. replacing the saved state with corrupt or future-version JSON shows a state
   error and a subsequent save preserves the rejected bytes rather than silently
   replacing them with defaults.

## Real hosts

Repeat save/reopen, duplication, mono/stereo audio, and offline editor checks in
REAPER and Ableton Live. With a compatible KnobKraft engine bridge running,
verify connection, explicit synth rebind, patch search/selection, manual Send,
progress, Cancel, and reconnect after restarting KnobKraft. No action during
plugin scanning or project loading may launch KnobKraft or send MIDI. Record host
and plugin versions alongside results; these checks require locally installed
hosts and are not automated by this repository.

### WP-08 Matrix-1000 vertical slice

Use a Matrix-1000 that is already configured in KnobKraft with a working MIDI
output and at least one Matrix patch in the open database. The plugin never
opens MIDI hardware itself.

1. Start the matching `KnobKraftOrm.exe`. Its **Plugin Sessions** pill should
   initially report that no plugins are connected.
2. Insert **KnobKraft Recall** on the Ableton Live or REAPER track whose clips
   play the Matrix-1000. The plugin should change from disconnected to connected,
   and the standalone pill should show one session.
3. In the plugin, choose the configured Matrix-1000 instance. Do not accept a
   model-name fallback: the binding must use that configured synth's persistent
   identity.
4. Search the KnobKraft database, select one Matrix patch, and verify that its
   name, fingerprint, and **Stored in project** state appear immediately.
5. Press **Send**. Both the plugin and the standalone sessions drawer should
   show the same queued/sending/result transition and identify the requesting
   plugin instance. The Matrix-1000 should change sound only after this explicit
   action.
6. Play the track's clips or loops. Recall is bound to the plugin instance on the
   track, not to an individual clip; every clip routed through that track uses
   the currently loaded hardware sound.
7. Save the project during or after the transfer, close the host, and reopen it
   with KnobKraft stopped. The embedded patch and configured-synth binding must
   remain visible while the engine reports disconnected.
8. Start KnobKraft. The plugin should reconnect and reappear in **Plugin
   Sessions**, but it must not send MIDI automatically. Press **Send** to restore
   the saved sound.
9. Restart KnobKraft while the project remains open and verify reconnect. Also
   test an unplugged synth, a busy output port, Cancel, and two plugin instances.
10. Confirm that bypassing Recall does not change audio and that scanning,
    loading, duplicating, and closing the editor never trigger a hardware send.

Current WP-08 limitations:

- Recall is manual and stores one sound per plugin instance.
- Successful Matrix sends are reported as sent but unverified.
- Cancel takes effect at safe transfer boundaries; it cannot interrupt an
  individual SysEx block already handed to the MIDI output.
- Standalone navigation currently brings KnobKraft forward but does not yet
  select a specific synth or patch.
- Program-slot deployment, automatic recall, and hardware-state capture remain
  later work packages.
