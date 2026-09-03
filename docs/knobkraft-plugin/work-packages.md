# KnobKraft Recall Plugin Work Packages

Architecture and product decisions are defined in [README.md](README.md). This document divides the implementation into reviewable worktree-sized changes.

## 1. Integration rules

- Each worktree produces one focused pull request.
- Branch names below are suggestions and use the repository's `codex/` prefix.
- Do not mix opportunistic refactors with a work package.
- Contract changes are merged before consumers depend on them.
- Every new public contract arrives with tests and a fixture or example.
- Hardware-independent tests are required before a hardware vertical slice.
- A worktree must rebase after any dependency PR merges; do not copy competing versions of shared headers between branches.
- New code under `MidiKraft/` follows its `AGENTS.md`: module-local `include/` and `src/`, narrow public interfaces, and JUCE-friendly style.
- Existing unrelated changes in a developer checkout are not part of these packages.

### 1.1 MidiKraft submodule workflow

`MidiKraft/` is a separate Git repository referenced as a submodule by KnobKraft Orm. Work packages that change it therefore need deliberate two-repository handling.

- WP-00 and WP-01 are primarily MidiKraft pull requests, followed by a small KnobKraft Orm pull request that advances the submodule pointer and integrates the new target.
- WP-02 may require paired pull requests: reusable identity types belong in MidiKraft, while application configuration and migration belong in KnobKraft Orm.
- WP-04 belongs in MidiKraft only if its transport implementation remains reusable and free of application/plugin UI dependencies. Otherwise keep the wire contracts in MidiKraft and the process adapters in the superproject.
- WP-03 and WP-05 through WP-11 are primarily KnobKraft Orm changes. They may advance the MidiKraft pointer only to an already reviewed dependency commit.
- Never implement the same contract independently in both repositories.
- Merge the MidiKraft pull request first. Then update the submodule pointer in the dependent KnobKraft Orm branch.
- A new superproject worktree does not inherit uncommitted changes currently present inside the `MidiKraft` checkout. Start each package from committed dependency revisions.
- Record both commit IDs in a paired pull request description so reviewers can reproduce the combination.

Suggested ownership split:

| Concern | Repository |
| --- | --- |
| Session value types, codecs, service interface, protocol envelopes | MidiKraft |
| Reusable fake service and transport tests | MidiKraft |
| Persistent identity fields reusable by synth/device code | MidiKraft |
| Application configuration migration and instance registry | KnobKraft Orm |
| VST3 target, processor, editor, and engine client adapter | KnobKraft Orm |
| Application server lifecycle and engine adapter | KnobKraft Orm |
| Plugin Sessions toolbar widget and drawer | KnobKraft Orm |

## 2. Dependency map

```text
WP-00 Contract and codecs
  |\
  | +--> WP-02 Configured synth identity
  | +--> WP-03 Plugin shell
  |
  +----> WP-01 Fake session service
           |
           +--> WP-04 IPC transport
                    |\
                    | +--> WP-05 KnobKraft engine bridge
                    | +--> WP-06 Plugin connection and state UI
                    |
                    +--> WP-07 Plugin Sessions widget

WP-02 + WP-03 + WP-05 + WP-06 + WP-07
                    |
                    +--> WP-08 Matrix-1000 vertical slice
                              |
                              +--> WP-09 Single-sound MVP hardening
                                        |
                                        +--> WP-10 Safe total recall
                                                  |
                                                  +--> WP-11 Project sound sets
```

WP-01, WP-02, and WP-03 can begin in parallel after WP-00 is merged. WP-05, WP-06, and WP-07 can be developed in parallel after the IPC contract in WP-04 is stable, but they should use the same fake server fixtures.

## 3. Shared-file hotspots

Avoid assigning these files to several active worktrees simultaneously:

- Top-level `CMakeLists.txt` and `The-Orm/CMakeLists.txt`.
- Application initialization and shutdown in `The-Orm/Main.cpp`.
- Main toolbar or main-window composition files used by the Plugin Sessions widget.
- New protocol headers in `MidiKraft/session/include/`.
- The plugin target's processor state methods.

Prefer package-local CMake files and make one small integration edit at merge time.

## 4. Phase 0 work packages

### WP-00 — Session contracts and in-memory codecs

Suggested branch: `codex/knobkraft-plugin-00-session-contracts`

Goal: establish the durable state model shared by the application, plugin, and test doubles.

Deliverables:

- New `MidiKraft/session/` static-library target with `include/`, `src/`, and `tests/`.
- `SessionPatch`, `SessionManifest`, binding, recall-policy, and transfer-status value types.
- In-memory JSON codecs with schema version 1.
- Stable adaptation identity and patch fingerprint rules documented in code.
- Size limits and structured decode errors.
- Extraction or reuse of Patch Interchange Format encoding logic without file I/O.
- Golden JSON fixtures for valid, minimal, corrupt, and unknown-field cases.
- Unit tests for round trip, deterministic fingerprinting, corruption, and future-version rejection.

Acceptance criteria:

- A one-sound manifest round-trips byte-for-byte at the semantic level.
- Decoding never needs an active `Synth`, database, MIDI device, Python runtime, or JUCE UI object.
- A full patch is recoverable when all optional provenance is removed.
- Invalid payloads fail without exceptions crossing the public API boundary.
- `schemaVersion` and `formatVersion` behavior is explicit in tests.

Out of scope:

- Networking, UI, database lookup, physical MIDI, or plugin code.

Likely conflicts:

- Patch Interchange Format helpers. Keep changes there small and backwards compatible.

### WP-01 — Abstract and fake session service

Suggested branch: `codex/knobkraft-plugin-01-session-service`

Depends on: WP-00.

Goal: define behavior independently of IPC and hardware.

Deliverables:

- `SessionService` interface for server info, synth listing, patch search/get, edit-buffer transfer, status, cancel, and navigation requests.
- Request and response types with IDs, deadlines, paging, and stable error categories.
- `FakeSessionService` with deterministic synths, patches, progress, failures, and cancellation.
- An observable session/transfer state model usable by both UIs.
- Tests for one client, multiple clients, idempotent repeated request IDs, per-device serialization, and cancellation.

Acceptance criteria:

- Tests can exercise the complete manual-recall workflow without sockets, a database, or MIDI hardware.
- Operations for the same configured synth never interleave.
- Operations for different synths can progress independently.
- Repeating a mutating request ID does not enqueue a duplicate operation.
- A disconnected observer can resubscribe and obtain the current snapshot.

Out of scope:

- TCP framing and real KnobKraft objects.

### WP-02 — Persistent configured-synth identity

Suggested branch: `codex/knobkraft-plugin-02-synth-identity`

Depends on: WP-00.

Goal: distinguish a user's physical/configured synth from a synth model and transient MIDI port names.

Deliverables:

- `ConfiguredSynthInstance` model with persistent UUID, display name, stable adaptation ID, port assignments, channel, online state, and capability summary.
- Persistence integrated with the existing KnobKraft configuration mechanism.
- Migration that assigns IDs to existing configured synths without losing settings.
- Lookup by UUID and explicit rebind support.
- Tests for restart stability, duplicate synth models, renamed instances, missing ports, and legacy configuration migration.

Acceptance criteria:

- Two Matrix-1000 units can be represented and selected independently.
- Renaming an instance or temporarily losing a MIDI port does not change its UUID.
- An unresolved UUID produces a rebind state; it never silently binds to the first synth of the same model.
- Capability reporting distinguishes edit-buffer, program-dump, custom-program-change, and verification support.

Out of scope:

- IPC exposure and changes to device discovery algorithms beyond what identity persistence requires.

Likely conflicts:

- Existing device configuration and `SimpleDiscoverableDevice` ownership. Keep adaptation identity separate from display name.

### WP-03 — Transparent VST3 shell

Suggested branch: `codex/knobkraft-plugin-03-vst3-shell`

Depends on: WP-00.

Goal: produce a loadable, stateful, host-neutral plugin without networking.

Deliverables:

- `KnobKraft-Plugin/` target created with `juce_add_plugin`, VST3 only initially.
- Transparent audio pass-through with zero reported latency.
- `PluginState` backed by an immutable or copy-on-write `SessionManifest` snapshot.
- JUCE state serialization/deserialization using WP-00 codecs.
- Minimal editor showing disconnected state and an embedded fixture patch.
- Processor tests where practical plus validator scripts or documented commands.

Acceptance criteria:

- The plugin loads in JUCE's plugin host and at least one real VST3 host.
- Audio input is bit-identical at the output for representative layouts.
- State survives project save/reopen and plugin duplication.
- Unsupported or corrupt state displays an error and preserves recoverable raw state; it does not reset silently.
- `processBlock` performs no IPC, file access, logging, allocation introduced by this feature, or contended locking.

Out of scope:

- Physical MIDI, real patch browser, and KnobKraft connection.

Likely conflicts:

- Top-level CMake integration. Keep the target definition local and make the root edit minimal.

### WP-04 — Local IPC transport

Suggested branch: `codex/knobkraft-plugin-04-ipc`

Depends on: WP-01.

Goal: carry the service contract safely between processes without depending on UI or hardware.

Deliverables:

- Loopback TCP server and asynchronous client libraries.
- Length-bounded framing and versioned message envelope.
- Per-run discovery file with port, PID, generation, protocol version, and random token.
- Authentication, protocol negotiation, request IDs, deadlines, progress events, cancel, heartbeat, and reconnect.
- Fake-service server executable or test harness.
- Tests for partial frames, malformed messages, bad tokens, version mismatch, server restart, stale discovery, timeout, reconnect, multiple clients, and idempotent retry.

Acceptance criteria:

- A client can discover a server, list fake synths, start a fake transfer, observe progress, cancel it, and reconnect after server restart.
- The server listens only on loopback.
- Payload and frame size limits are enforced before allocation of unbounded data.
- Authentication tokens and full patch payloads do not appear in normal logs.
- No transport callback depends on a JUCE component or physical MIDI object.

Out of scope:

- TLS, remote hosts, named-pipe optimization, and real database/MIDI behavior.

### WP-05 — KnobKraft engine bridge

Suggested branch: `codex/knobkraft-plugin-05-engine-bridge`

Depends on: WP-02 and WP-04.

Goal: expose existing KnobKraft behavior through the service contract.

Deliverables:

- `SessionServiceAdapter` connected to configured synths, patch database, capabilities, and existing sending code.
- `PluginBridgeServer` started and stopped with the primary application process.
- Primary-server ownership rule when several KnobKraft processes exist.
- Patch search/get mapping to `SessionPatch`.
- Edit-buffer application with compatibility validation, per-device queuing, progress, cancellation, and structured errors.
- Application logs that identify the requesting plugin instance without logging payloads.
- Unit/integration tests with fake MIDI endpoints where possible.

Acceptance criteria:

- The protocol can list the application's configured synth instances and retrieve one database patch.
- A compatible patch reaches the existing edit-buffer conversion/sending path.
- An incompatible adaptation, missing edit-buffer capability, offline synth, and busy MIDI port each return distinct errors.
- Multiple client operations for one synth are serialized.
- A second KnobKraft process does not replace or corrupt the primary discovery record.

Out of scope:

- Automatic recall, hardware-state capture, program-slot deployment, and remote IPC.

Likely conflicts:

- `The-Orm/Main.cpp`, database access ownership, and the application's current MIDI send lifetime.

### WP-06 — Plugin engine connection and manual-recall UI

Suggested branch: `codex/knobkraft-plugin-06-client-ui`

Depends on: WP-03 and WP-04.

Goal: implement the complete plugin-side user experience against the fake server first.

Deliverables:

- `EngineClient` background worker with reconnect and observable immutable snapshots.
- Engine status and Open KnobKraft action.
- Configured-synth picker and rebind state.
- Patch search/selection using paged service results.
- Project patch name, fingerprint, provenance, and `Stored in project` indication.
- Manual Send, progress, Cancel, result timestamp, and actionable errors.
- Manual recall policy persisted in the manifest.
- UI tests or state-model tests for disconnected, connected, offline, queued, sending, sent, verified, failed, and cancelled states.

Acceptance criteria:

- The editor immediately shows the embedded patch before a connection exists.
- Closing the editor does not stop the processor's client connection.
- State changes caused by IPC are marshalled onto the message thread.
- Removing or renaming a configured synth produces a clear rebind workflow.
- Saving during a transfer persists the patch and binding, not transient progress.
- Host-provided names are treated as optional; the editable plugin instance name remains stable.

Out of scope:

- Automatic recall and multiple sounds.

### WP-07 — KnobKraft Plugin Sessions widget

Suggested branch: `codex/knobkraft-plugin-07-sessions-widget`

Depends on: WP-04. Develop against WP-01's fake observable state until WP-05 is available.

Goal: make plugin connections and hardware activity visible in the standalone application.

Deliverables:

- Persistent toolbar pill with disconnected, connected-count, active-transfer, and attention states.
- Drawer listing sessions, bindings, online state, stored patch, queue/progress, last result, and timestamp.
- Cancel and Resolve actions.
- Request attribution by plugin instance name and ID.
- Stale-session expiry after heartbeat loss.
- Navigation hooks that can later open a synth or patch without coupling the service to UI classes.
- Reducer/view-model tests independent of JUCE painting.

Acceptance criteria:

- Connect/disconnect updates the count without restarting KnobKraft.
- Fake transfers and errors produce the specified pill and drawer states.
- Cancelling in the drawer reaches the service and updates all observers.
- An editor closing does not remove a still-connected processor session.
- A crashed or closed host disappears after the stale timeout.
- Errors remain visible until acknowledged or replaced by a later result.

Out of scope:

- Patch editing, database browsing, and detailed MIDI diagnostics inside the drawer.

Likely conflicts:

- Main toolbar composition. Coordinate the final insertion point with other UI work.

### WP-08 — Matrix-1000 end-to-end vertical slice

Suggested branch: `codex/knobkraft-plugin-08-matrix-vertical-slice`

Depends on: WP-02, WP-03, WP-05, WP-06, and WP-07.

Goal: integrate the independently tested pieces and prove one real synth workflow.

Deliverables:

- Final CMake and application wiring.
- A tested Matrix-1000 edit-buffer flow from database selection to embedded DAW state to hardware transmission.
- End-to-end diagnostic logging with correlation/request IDs.
- Manual test checklist for REAPER and Ableton Live.
- Screenshots of the plugin and Plugin Sessions drawer during transfer.
- Documented limitations found during the slice.

Acceptance criteria:

- The Phase 0 flow in the architecture document works on a real Matrix-1000.
- Restarting KnobKraft during an idle plugin session reconnects without reloading the DAW project.
- Starting with KnobKraft offline does not lose or hide the embedded patch.
- Port-busy, unplugged synth, cancel, and timeout paths remain responsive.
- Audio remains transparent throughout the transfer.
- The plugin and standalone report the same request, progress, and result.

Out of scope:

- General release packaging and claims of compatibility with all adaptations.

## 5. Phase 1 work package

### WP-09 — Single-sound MVP hardening

Suggested branch: `codex/knobkraft-plugin-09-mvp-hardening`

Depends on: WP-08.

Goal: turn the vertical slice into a releasable, generic single-sound plugin.

Deliverables:

- State migration and compatibility fixtures.
- Robust paging/search, offline/rebind, timeout, cancellation, and recovery behavior.
- One native synth with direct program-dump support and one Python adaptation added to the test matrix.
- Capability-based UI for synths without edit buffers.
- Plugin installation and packaging for supported Windows builds.
- JUCE validator and `pluginval` runs plus REAPER, Ableton Live, and third-host project fixtures.
- Performance and thread-safety review.
- User documentation and known limitations.

Acceptance criteria:

- The complete MVP definition in `README.md` passes.
- A project remains recoverable across plugin/engine minor-version mismatch within the supported protocol range.
- No host scan or project load launches KnobKraft or sends MIDI automatically.
- Malformed state and protocol input fail safely.
- At least three different capability profiles produce correct UI and errors.

## 6. Later work packages

### WP-10 — Safe total recall

Suggested branch: `codex/knobkraft-plugin-10-total-recall`

Depends on: WP-09.

Deliverables:

- Ask-on-open policy.
- Opt-in automatic recall only after a stable activation signal and while transport is stopped.
- Project State / Hardware State / Cancel conflict UI.
- Hardware-state capture and verification for adaptations that support it.
- Tests for plugin scanning, project activation, duplicate tracks, undo/redo, transport races, and repeated host lifecycle callbacks.

Safety gate:

- No automatic policy is merged until tests demonstrate that plugin construction, scanning, deserialization, and duplication do not independently trigger a send.

### WP-11 — Project sound sets and program deployment

Suggested branch: `codex/knobkraft-plugin-11-program-sets`

Depends on: WP-10.

Deliverables:

- Multiple named sounds in the session manifest.
- Deployment plan with explicit approved writable hardware slots.
- Dry run, destructive-write warning, serialized deploy, verification, and recovery report.
- Plugin table showing deployed Bank Select and Program Change values.
- Tests for collision, partial failure, reconnect, custom program-change capabilities, and devices without writable program memory.

Safety gate:

- Deployment never guesses that a hardware slot is safe to overwrite. The user or adaptation must explicitly mark the eligible range.

## 7. Worktree kickoff template

Use this checklist when creating a worktree task or issue:

```text
Implement WP-XX from docs/knobkraft-plugin/work-packages.md.

Read docs/knobkraft-plugin/README.md and MidiKraft/AGENTS.md first.
Stay within the package's deliverables and out-of-scope boundaries.
Do not modify unrelated local changes.
Add the specified automated tests and document commands/results.
If a shared contract must change, stop and propose the contract change before
editing consumers in parallel worktrees.
Finish with a concise list of changed files, tests, remaining risks, and any
deviation from the plan.
```

Suggested worktree command after the package's dependencies have merged:

```powershell
git worktree add ..\KnobKraft-Orm-wp00 -b codex/knobkraft-plugin-00-session-contracts
```

Initialize the submodule inside that worktree before starting a package that needs it:

```powershell
Set-Location ..\KnobKraft-Orm-wp00
git submodule update --init MidiKraft
Set-Location MidiKraft
git switch -c codex/knobkraft-plugin-00-session-contracts
```

Use a different sibling directory and branch from the package entry for each concurrent worktree. The superproject branch and submodule branch may share a descriptive suffix, but they remain separate branches and produce separate commits. Do not reuse the dirty `MidiKraft` checkout from another worktree.

## 8. Review checklist for every package

- Does the change preserve full project patch data without requiring the database?
- Is all hardware access still owned by KnobKraft?
- Can any new path run from the audio thread?
- Are stored, sent, and verified states represented separately?
- Are failures structured and actionable in both UIs?
- Are limits applied before parsing or allocating untrusted IPC/state payloads?
- Are mutations idempotent or protected against ambiguous retries?
- Are new public types and protocol fields tested and documented?
- Does the change keep future sound sets possible without implementing them prematurely?
- Are manual hardware tests clearly labelled as verified or unverified?
