# VST3 validation

## Automated checks

Build and run `knobkraft-recall-tests`. They cover bit-identical mono/stereo
float and double pass-through, zero reported latency, codec-backed state round
trip, plugin duplication, and recoverable corrupt-state behavior.

For a `pluginval` installation, run:

```powershell
./scripts/validate-plugin.ps1 -PluginValPath C:/Tools/pluginval.exe -Vst3Path 'build/knobkraft-plugin/KnobKraftRecall_artefacts/Release/VST3/KnobKraft Recall.vst3'
```

## JUCE AudioPluginHost

Build `extras/AudioPluginHost` from the checked-out JUCE version, scan the VST3
bundle above, insert **KnobKraft Recall** on mono and stereo audio, and verify:

1. audio is unchanged and the host reports zero samples of plugin latency;
2. the editor says `Engine • Disconnected` and shows the embedded fixture;
3. saving, closing, and reopening a project restores the same patch and fingerprint;
4. duplicating the plugin preserves its complete embedded state;
5. replacing the saved state with corrupt or future-version JSON shows a state
   error and a subsequent save preserves the rejected bytes rather than silently
   replacing them with defaults.

## Real hosts

Repeat save/reopen, duplication, mono/stereo audio, and offline editor checks in
REAPER and Ableton Live. WP-03 intentionally includes no IPC or MIDI behavior.
Record host and plugin versions alongside results; these checks require locally
installed hosts and are not automated by this repository.
