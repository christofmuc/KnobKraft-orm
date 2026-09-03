# KnobKraft Recall VST3 plugin

This directory contains the host-neutral, transparent VST3 plugin. It embeds a
complete `SessionManifest` in DAW project state and uses the local Recall IPC
transport to connect to a running KnobKraft application. The processor owns the
reconnecting client, so closing the editor does not end the engine session.

The editor supports configured-synth binding, paged patch search, embedding a
selected patch, and explicit manual Send/Cancel with progress and errors.
Physical MIDI, adaptations, and the database remain exclusively in KnobKraft.
Automatic recall and multiple project sounds are intentionally not implemented.

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
