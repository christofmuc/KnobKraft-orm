# KnobKraft Recall Plugin

Status: design plan

Related discussion: [GitHub issue #46](https://github.com/christofmuc/KnobKraft-orm/issues/46)

Execution plan: [work-packages.md](work-packages.md)

## 1. Purpose

KnobKraft Recall is a generic VST3 plugin that stores hardware-synth patch state in a DAW project and asks the running KnobKraft Orm application to restore that state to the hardware.

The plugin is deliberately thin. It does not load synth adaptations, access the patch database, discover MIDI devices, or open physical MIDI ports. The existing KnobKraft application remains the synth engine and the single owner of SysEx communication.

This combines the useful parts of established hardware-integration products while keeping KnobKraft small:

- DAW project state and total recall, as used by hardware editor plugins.
- A central engine that owns devices and transfers, as used by systems such as Elektron Overbridge.
- Full patch data embedded in the project, rather than a fragile reference to an external library entry.
- Normal DAW MIDI and audio routing for performance data; the plugin handles sound recall only.

## 2. Product model

The first release supports one logical synth part per plugin instance.

```text
DAW project
  |
  +-- KnobKraft Recall instance "Matrix Bass"
  |     - stores the complete patch snapshot
  |     - stores a binding to a configured synth instance
  |     - communicates with KnobKraft over local IPC
  |
  +-- ordinary MIDI track --------------------> hardware MIDI input
  +-- ordinary audio track <------------------ hardware audio output

KnobKraft Orm
  - owns adaptations and the patch database
  - owns physical MIDI ports
  - detects and identifies configured synths
  - converts patch data to the correct device messages
  - serializes, sends, and optionally verifies transfers
```

The plugin is not a software instrument and does not carry the hardware's audio. It should be implemented initially as a transparent audio effect so it can be placed conveniently on a DAW track without changing audio, latency, or MIDI performance routing.

## 3. Design principles

1. **The DAW project is self-contained.** A saved session contains the complete patch bytes and the metadata needed to interpret them. A database ID is optional provenance, never the only copy.
2. **KnobKraft owns hardware communication.** The plugin never opens a physical MIDI port and never depends on a host forwarding SysEx.
3. **Recall is asynchronous.** No IPC, disk access, adaptation execution, or MIDI transfer may occur on the audio thread.
4. **State is described truthfully.** “Stored in project”, “sent”, “verified”, and “currently sounding” are different states. The UI must not claim that a selected patch is active unless that can be verified.
5. **Recall is safe by default.** The first version uses an explicit Send action. Automatic recall is introduced only after conflict handling and host-lifecycle behavior are understood.
6. **The first version is host-neutral.** No Ableton-specific clip APIs, Max for Live dependency, or assumptions about track and project names.
7. **One binary serves all synths.** Device-specific behavior stays in KnobKraft's native synths and Python adaptations.
8. **Operations are attributable.** KnobKraft shows which plugin instance requested every transfer.

## 4. Scope

### 4.1 Minimum viable product

- A VST3 plugin built with the JUCE version already used by the repository.
- Connection to a running local KnobKraft application.
- Selection of one configured synth instance.
- Selection of one patch from KnobKraft.
- Complete patch state saved in and restored from the DAW project.
- Manual transmission of the stored patch to the synth's edit buffer.
- Progress, success, cancellation, timeout, and useful error reporting.
- A KnobKraft toolbar widget showing connected plugin sessions and their activity.
- Rebinding after a synth, MIDI port, or machine configuration changes.

### 4.2 Later releases

- Ask-on-open and opt-in automatic recall.
- Capture of the synth's current edit buffer into the plugin.
- A project sound set deployed to approved hardware program slots.
- DAW clips selecting deployed sounds using ordinary Bank Select and Program Change messages.
- Verification on synths that can report their current edit buffer or acknowledge writes.
- Multitimbral coordination.
- Additional plugin formats and platforms after VST3 behavior is stable.

### 4.3 Explicit non-goals for the MVP

- Carrying hardware audio through the plugin.
- Sending performance notes, CC, clock, or program changes through the IPC connection.
- Loading Python or the patch database in the plugin process.
- Opening MIDI devices from the plugin.
- Real-time SysEx triggered by the audio callback.
- Knowing or editing DAW clips and loops.
- Parameter automation or a generic synth parameter editor.
- Silently launching KnobKraft while a DAW scans plugins.
- Remote-machine IPC.

## 5. Components

### 5.1 Shared session module

Add a small library under `MidiKraft/session/` with the same `include/` and `src/` layout as the existing modules. It contains only stable data contracts, codecs, and an abstract service API. It must not depend on the KnobKraft UI, database, MIDI ports, or Python runtime.

Suggested public types:

```cpp
struct SessionPatch;
struct SessionManifest;
struct ConfiguredSynthInstance;
struct TransferRequest;
struct TransferProgress;
struct TransferResult;

class SessionPatchCodec;
class SessionManifestCodec;
class SessionService;
```

The existing Patch Interchange Format already serializes patch metadata and base64-encoded SysEx in `MidiKraft/librarian/PatchInterchangeFormat.cpp`. Extract or reuse the relevant in-memory encoding rules rather than introducing a second incompatible patch representation. File I/O must remain outside the codecs used by the plugin.

### 5.2 KnobKraft engine adapter

The existing application gains three small responsibilities:

- `SynthInstanceRegistry` gives configured hardware instances persistent identities.
- `SessionServiceAdapter` translates the shared API into existing database, adaptation, and MIDI operations.
- `PluginBridgeServer` exposes that service to local plugin clients.

The adapter should call existing capability APIs such as `EditBufferCapability::patchToSysex`, `ProgramDumpCapability::patchToProgramDumpSysex`, and `Synth::sendDataFileToSynth`. Conversion and sending remain entirely on the KnobKraft side.

Each physical device has a serialized transfer queue. Multiple plugin clients may submit work concurrently, but two SysEx operations must not interleave on the same device.

### 5.3 Plugin

The plugin contains:

- JUCE `AudioProcessor` and editor classes.
- `PluginState`, which owns the current `SessionManifest`.
- `EngineClient`, which performs asynchronous IPC and reconnects safely.
- A small UI that edits bindings, chooses a project sound, sends it, and reports status.

The audio callback is a transparent pass-through. It reads no mutable network state and performs no allocation, logging, locking, or IPC.

### 5.4 KnobKraft Plugin Sessions widget

KnobKraft gains a persistent toolbar status pill:

```text
● 3 plugin sessions
↻ Sending "Warm Pad" to Matrix-1000
⚠ Plugin needs attention
○ No plugins connected
```

Clicking it opens a compact drawer. It is an activity and health monitor, not a second librarian.

For each plugin connection, show:

- User-editable plugin instance name.
- Host name when the host supplies it reliably.
- Bound configured synth and its online state.
- Project patch name and fingerprint.
- Current or queued operation with progress and Cancel.
- Last result and timestamp.
- A Resolve action for missing synths, busy ports, incompatible data, or protocol errors.
- An Open Patch or Open Synth action where navigation is unambiguous.

Every operation is labelled with its origin, for example `Requested by plugin "Matrix Bass"`.

DAW project and track names are optional context because generic VST3 hosts do not expose them consistently. The durable identity is a plugin-generated UUID plus a user-editable instance name.

## 6. Persistent domain model

The exact JSON spelling can change during the contract work package, but the following information is required.

### 6.1 Configured synth instance

```json
{
  "instanceId": "2d79064b-3d01-4e8f-aacc-108ff73cd7e5",
  "displayName": "Studio Matrix-1000",
  "adaptationId": "Oberheim Matrix 1000",
  "midiInputId": "...",
  "midiOutputId": "...",
  "midiChannel": 1,
  "online": true,
  "capabilities": {
    "editBuffer": true,
    "programDump": true,
    "verification": false
  }
}
```

`instanceId` is persistent and represents the user's configured piece of hardware, not merely a synth model or the current operating-system port name. `adaptationId` is stable across renaming of the configured instance.

### 6.2 Session patch

```json
{
  "formatVersion": 1,
  "adaptationId": "Oberheim Matrix 1000",
  "dataTypeId": "single-program",
  "name": "Warm Bass",
  "fingerprint": "sha256:...",
  "payloadEncoding": "base64",
  "payload": "8F0...",
  "source": {
    "databaseId": "optional",
    "bank": 1,
    "program": 23
  }
}
```

The canonical payload is the data from which KnobKraft can recreate valid outbound messages. The fingerprint covers stable interpretation fields and decoded payload bytes, not cosmetic names or mutable database metadata.

### 6.3 Session manifest

```json
{
  "schemaVersion": 1,
  "pluginInstanceId": "bbb77904-c98d-4c6f-8808-44b287885bf4",
  "instanceName": "Matrix Bass",
  "binding": {
    "configuredSynthInstanceId": "2d79064b-3d01-4e8f-aacc-108ff73cd7e5",
    "fallbackAdaptationId": "Oberheim Matrix 1000"
  },
  "recallPolicy": "manual",
  "selectedSoundId": "sound-1",
  "sounds": [
    {
      "soundId": "sound-1",
      "patch": {}
    }
  ],
  "deploymentPlan": []
}
```

The MVP writes one entry in `sounds`. Keeping it as a collection avoids a breaking state redesign when project sound sets are introduced.

### 6.4 Compatibility rules

- Unknown optional fields are ignored and preserved where practical.
- Unsupported future schema versions fail with an explicit message; they are never silently replaced by defaults.
- Missing optional metadata must not prevent recall when the payload and adaptation identity are valid.
- Corrupt or oversized payloads are rejected before they reach an adaptation.
- Schema migrations are deterministic and covered by fixtures.
- Saving a project while a transfer is running saves the stable project state, not transient progress.

## 7. IPC contract

### 7.1 Transport and discovery

Use a versioned request/response protocol over a loopback-only TCP socket for the first implementation. TCP is available on all target platforms and avoids tying the contract to one operating system's named-pipe API.

The primary KnobKraft process writes a small discovery file in the per-user application-data directory containing:

- Protocol major and minor version.
- Loopback port.
- Server process ID and generation ID.
- A new random authentication token for that server run.

The server binds only to a loopback address. The token prevents an unrelated local process from accidentally issuing synth operations. The file is owner-readable only where the platform supports permissions. Remote access is out of scope.

### 7.2 Lifecycle

- The plugin connects only from a background worker.
- Connection failure leaves the embedded project state usable and displays `KnobKraft not running`.
- Opening the plugin editor may offer `Open KnobKraft`; plugin scanning and state loading never launch it.
- Clients reconnect after application restart and re-register their session identity.
- A heartbeat or bounded idle timeout removes stale sessions from the KnobKraft widget.
- Multiple plugin instances and multiple DAWs may connect simultaneously.
- Initially, only the primary KnobKraft application instance exposes the service.

The existing application currently permits multiple instances. Server ownership therefore needs an explicit primary-instance rule; changing the whole application to a singleton is a separate decision and is not required for the MVP.

### 7.3 Initial operations

```text
getServerInfo()
listConfiguredSynthInstances()
getConfiguredSynthInstance(instanceId)
searchPatches(query, adaptationId, page)
getPatch(patchId)
applyToEditBuffer(request)
getTransferStatus(transferId)
cancelTransfer(transferId)
openKnobKraft(target)
```

Later operations:

```text
captureEditBuffer(instanceId)
planDeployment(instanceId, sounds, approvedSlots)
deployProgramSet(planId)
verifyTransfer(transferId)
sendProgramChange(instanceId, bank, program)
```

Each mutating request carries a unique request ID, plugin instance ID, target configured-synth ID, expected adaptation ID, and payload fingerprint. Retrying the same request ID must not enqueue a duplicate transfer after an ambiguous connection loss.

### 7.4 Progress and errors

Transfers move through explicit states:

```text
accepted -> queued -> preparing -> sending -> verifying -> succeeded
                                               |          |
                                               +-> failed +-> cancelled
```

Not every synth supports verification. A successful unverified transfer is reported as `sent`, not `verified`.

Stable error categories should include:

- KnobKraft unavailable.
- Protocol incompatible.
- Authentication failed.
- Configured synth missing.
- Synth offline.
- MIDI port busy.
- Patch incompatible with bound adaptation.
- Patch data invalid.
- Adaptation error.
- Transfer timed out.
- Transfer cancelled.
- Verification mismatch.

Human-readable detail can evolve without requiring the plugin to parse it.

## 8. Recall behavior

### 8.1 MVP: manual recall

1. The DAW restores the plugin state.
2. The plugin validates the manifest locally and displays the stored project sound immediately.
3. In the background it connects to KnobKraft and resolves the configured-synth binding.
4. The user presses Send.
5. KnobKraft validates adaptation compatibility, queues the operation, generates device-specific messages, and sends them to the edit buffer.
6. Both UIs display progress and the final verified or unverified result.

The plugin must remain able to show and re-save its patch while KnobKraft or the hardware is offline.

### 8.2 Safe total recall

After the manual workflow is reliable, add these policies:

- `manual`: never send without an explicit action.
- `ask`: when a project becomes active and transport is stopped, offer to send project state or keep hardware state.
- `automatic-when-stopped`: opt-in; send once after a stable host activation event, never during scanning or from the audio thread.

When both project and hardware state are available and differ, use an explicit conflict choice:

- Use Project State.
- Use Hardware State.
- Cancel.

No destructive action is taken merely because a plugin instance was constructed or a state block was deserialized.

### 8.3 Project sound sets and loops

A generic VST does not bind directly to a named DAW loop. The later sound-set feature deploys patches to approved writable locations in the hardware. A DAW clip then selects a sound using ordinary Bank Select and Program Change events through the DAW's normal MIDI route.

The plugin shows the actual values to place in the clip:

```text
Intro Pad     Bank MSB 0 / LSB 1 / Program 12
Verse Bass    Bank MSB 0 / LSB 1 / Program 13
Lead          Bank MSB 0 / LSB 1 / Program 14
```

Deployment requires a dry run, an explicit list of writable slots, a destructive-write warning, serialized transfer, and verification where available. Mid-song SysEx recall is not supported.

## 9. User interface

### 9.1 Plugin editor

The first editor should have six compact areas:

1. **Engine:** connection state, protocol compatibility, and Open KnobKraft.
2. **Hardware binding:** configured synth, adaptation, MIDI reachability, and Rebind.
3. **Project sound:** patch name, optional bank/program provenance, fingerprint, and `Stored in project` state.
4. **Actions:** Choose from KnobKraft, Use Hardware State when supported, Send, and Cancel.
5. **Recall policy:** Manual in the MVP, with future policies shown only when implemented.
6. **Status:** queued/sending/progress/result, verification level, timestamp, and actionable error detail.

The editor must clearly separate:

- `Stored in project` — serialized in the DAW state.
- `Bound` — a configured synth identity resolves.
- `Online` — KnobKraft can reach the hardware.
- `Sent` — the transfer completed without a known transport error.
- `Verified` — the hardware state was read or acknowledged and matched.

### 9.2 KnobKraft widget acceptance behavior

- The count updates when plugin clients connect and disconnect.
- A transfer appears in the widget no later than when it enters the device queue.
- Cancelling from either UI updates the other UI.
- Errors remain visible until superseded or acknowledged.
- Closing a plugin editor does not disconnect the plugin processor.
- Closing a DAW or removing an instance eventually removes its stale widget entry.
- Sessions are grouped by plugin instance, then show the target synth and operation.

## 10. Threading and safety

- Plugin state serialization holds a short lock only over an immutable or copy-on-write manifest snapshot.
- The IPC client has one owned worker context and posts UI updates through JUCE's message thread mechanisms.
- The engine server never calls UI objects directly; it publishes observable session and transfer state.
- Device queues own cancellation and lifetime. A disconnected client does not invalidate an operation already executing; policy determines whether it is allowed to finish or cancelled at a safe message boundary.
- Patch size and request-rate limits protect the application from malformed clients.
- Logs omit authentication tokens and avoid dumping full patch payloads by default.

## 11. Testing strategy

### 11.1 Automated tests

- Session patch and manifest round trips.
- Golden fixtures for every schema version and migration.
- Unknown fields, missing optional fields, corruption, invalid base64, fingerprint mismatch, and size limits.
- Fake `SessionService` behavior without database or MIDI hardware.
- Protocol framing, authentication, negotiation, deadlines, reconnect, idempotent requests, and cancellation.
- Multiple clients sharing one synth queue and clients targeting different synths.
- Saving and restoring plugin state while disconnected and during an active transfer.
- Audio pass-through and absence of IPC calls from `processBlock`.
- Stale client cleanup and widget state reduction.
- Plugin validation with JUCE's validator and `pluginval` where available.

### 11.2 Host tests

Validate on Windows first with:

- REAPER, because it is useful for VST3 diagnostics.
- Ableton Live, because it is a primary requested workflow.
- At least one additional VST3 host to catch host-specific assumptions.

Test project save/reopen, Save As, plugin duplication, track duplication, undo/redo, offline KnobKraft, KnobKraft restart, DAW crash/restart, sample-rate changes, transport start during transfer, and multiple DAWs.

### 11.3 Hardware matrix

- Oberheim Matrix-1000 as the first native vertical slice.
- A synth with direct program-dump support.
- At least one Python adaptation.
- A synth without an edit buffer, to verify capability reporting and safe rejection.
- A MIDI interface that is exclusive on Windows, to exercise the port-busy path.

Hardware tests must record synth model, firmware, MIDI interface, connection type, adaptation version, and whether the result was verified or merely sent.

## 12. Delivery phases

### Phase 0: vertical slice

- Shared in-memory patch and manifest codecs.
- Fake service and contract tests.
- Local IPC with one read operation and one transfer operation.
- Minimal transparent VST3.
- Select one Matrix-1000 patch, embed it, restore it, and manually send it to the edit buffer.
- Minimal Plugin Sessions widget with connection, operation, and result.

### Phase 1: single-sound MVP

- Persistent configured-synth IDs and rebind flow.
- Patch search and selection.
- Complete progress, cancellation, timeout, offline, and port-busy behavior.
- State migration and corrupt-state handling.
- Packaging and validation in the host matrix.
- One Python adaptation included in the acceptance run.

### Phase 2: safe total recall

- Ask-on-open.
- Optional automatic recall while transport is stopped.
- Project-versus-hardware conflict dialog.
- Query or acknowledgment-based verification where supported.
- Launch KnobKraft only in response to a UI action.

### Phase 3: project sound sets

- Program map in the manifest and plugin UI.
- Approved writable slots, dry run, deployment warning, progress, and recovery.
- Display of actual Bank Select and Program Change values for DAW clips.

### Phase 4: optional expansion

- Multitimbral session coordination.
- Virtual MIDI routing if port contention proves common.
- Headless or remote engine modes.
- AU and AAX builds.
- Host-specific companion integrations.
- Parameter automation where a generic mapping is genuinely useful.

## 13. Decisions deliberately deferred

- JSON versus a binary framing format after the vertical slice. The semantic protocol must remain transport-independent.
- Whether the standalone application ultimately becomes a strict singleton.
- Whether an installed background service is preferable to the visible application as engine host.
- Whether the plugin should eventually expose MIDI output to the host.
- How a configured synth instance is migrated when operating-system MIDI endpoint IDs change.
- How to coordinate several plugin instances targeting different parts of one multitimbral device.
- Whether the project sound set owns fixed hardware slots or negotiates them per deployment.

These decisions must not be smuggled into early implementations. Record them in this document when evidence from the vertical slice resolves them.

## 14. Definition of MVP done

The single-sound MVP is complete when a user can:

1. Insert the generic KnobKraft Recall VST3 in REAPER or Ableton Live.
2. Bind it to a configured Matrix-1000 through the running KnobKraft application.
3. Choose a patch and save the DAW project.
4. Close both applications, reopen the project with the hardware in a different state, and still see the correct stored project patch while offline.
5. Start KnobKraft, reconnect without reloading the plugin, and manually send the embedded patch.
6. Observe the request and progress in both the plugin and KnobKraft's Plugin Sessions widget.
7. Hear the restored sound through the existing hardware audio route.
8. Receive an honest `Sent` or `Verified` result, or an actionable failure without crashing, hanging the host, or losing the embedded patch.
