# KnobKraft Recall VST3 shell

This directory contains the host-neutral, transparent VST3 shell. It embeds a
complete `SessionManifest` in DAW project state but performs no networking,
database, adaptation, MIDI-port, or hardware work.

## Small standalone build

Initialize the JUCE, JSON, and MidiKraft submodules, then configure only the
plugin and its tests:

```powershell
cmake -S KnobKraft-Plugin -B build/knobkraft-plugin -G Ninja -DBUILD_KNOBKRAFT_PLUGIN_TESTS=ON
cmake --build build/knobkraft-plugin --target KnobKraftRecall_VST3 knobkraft-recall-tests
ctest --test-dir build/knobkraft-plugin --output-on-failure
```

For a worktree whose dependency submodules are not initialized, point the small
build at existing read-only checkouts with `-DKNOBKRAFT_JUCE_SOURCE_DIR=<path>`
and `-DKNOBKRAFT_JSON_SOURCE_DIR=<path>`.

The VST3 bundle is emitted below
`build/knobkraft-plugin/KnobKraftRecall_artefacts/<configuration>/VST3/`.
The root project also builds it by default; set `BUILD_KNOBKRAFT_PLUGIN=OFF` to
exclude it from an application-only build.

See [validation.md](validation.md) for validator and host checks.
