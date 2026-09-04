# Upload acknowledgements in the Python adaptation API

Status: proposed design for [issue #426](https://github.com/christofmuc/KnobKraft-orm/issues/426). Based on the local checkout inspected on 2026-09-04. This document does not implement the API.

## Recommendation

Add an optional `UploadAcknowledgementCapability`, exposed by two Python functions:

```python
def prepareUpload(channel, messages):
    """Return a context dict if this upload expects an acknowledgement, else None."""

def classifyUploadReply(message, context):
    """Return None for unrelated messages, or a success/error result dict."""
```

Keep the existing conversion functions responsible for producing MIDI bytes. A new asynchronous C++ upload operation owns sending, listening, deadlines, cancellation, and advancing to the next patch. Python only describes the device protocol.

For the K5000, the exchange is:

```text
Orm                                      K5000
 | ---- converted program dump ---------> |
 | <--- write complete or write error --- |
 |                                       |
 | ---- next program, after success ----> |
```

This is a write acknowledgement. Existing download callbacks such as `isPartOfSingleProgramDump()` serve a different operation: receiving patch data and optionally returning a MIDI reply to the synth.

## Where this fits today

| Existing component | Current responsibility | Proposed change |
|---|---|---|
| `convertToProgramDump()` / `convertToEditBuffer()` | Convert patch data into outgoing messages | Keep signatures and return types |
| `convertPatchesToBankDump()` | Construct a device-specific bank dump | Keep signature and return type |
| `GenericAdaptation::hasCapability()` | Discover capabilities from Python functions | Discover the two new hooks as a pair |
| `GenericAdaptation::sendBlockOfMessagesToSynth()` | Send bytes, optionally applying a delay | Retain as a low-level transport interface |
| `Synth::sendDataFileToSynth()` | Convert and send an individual data file | Route actual uploads through the new operation |
| `Librarian::sendBankToSynth()` | Send bank messages or loop over programs | Await each required acknowledgement |
| `PatchView::sendBankToSynth()` | Clear dirty state on successful completion | Only clear after the required confirmations |

The existing send primitive also carries dump requests, program changes, and download handshake replies. Adding an unconditional acknowledgement wait inside that primitive would affect all of those callers, and its `void` interface cannot express asynchronous success or device errors.

Conversion must remain independent of I/O: it is also used by file export, interchange, and clipboard operations. Those callers must never register listeners or wait for hardware.

## Python contract

### `prepareUpload(channel, messages)`

Called once before transmitting one logical upload, after conversion has succeeded. `channel` follows the existing zero-based channel/device-address convention used by conversion. For the K5000 it is 0–15.

`messages` is a `list[list[int]]`: one complete MIDI message per inner list, with `F0`/`F7` included for SysEx. This preserves boundaries already present in the C++ `vector<MidiMessage>`; it is deliberately different from the concatenated byte list returned by existing conversion functions.

One logical upload means:

- One converted program, including all messages needed to encode that program.
- One converted edit buffer.
- One bank dump produced by the bank conversion function.

A bank sent as individual programs creates one operation per program. It must not concatenate all programs and expect one acknowledgement for the entire bank.

Return values:

| Return | Meaning |
|---|---|
| `None` | This particular upload does not produce an acknowledgement; send normally |
| A dict, including `{}` | Expect one terminal acknowledgement; retain this operation's context |
| Exception or another type | Invalid/unsupported upload; fail before sending |

The dict is adaptation-owned protocol context, composed of ordinary Python values. It can contain the expected channel, address, command, or transaction identifier derived from the outgoing messages. The framework retains it for this operation and passes it back without interpreting its keys. Hooks should treat it as immutable and avoid module-global transaction state.

The outgoing bytes are unchanged by this hook. `None` must only mean that no acknowledgement is expected; an unsupported protocol or invalid dump must raise an error. The hook performs no MIDI I/O, sleeping, or hardware probing.

For v1, a context describes one terminal success/error reply for the entire logical upload. Protocols requiring acknowledgements between outgoing chunks, request-to-send negotiation, busy/progress replies, or retransmission need a separate future protocol extension. Do not approximate those by sending all chunks upfront.

### `classifyUploadReply(message, context)`

Called with one complete incoming MIDI message from the operation's selected MIDI input. C++ filters the input port; Python validates manufacturer, model, channel/device address, framing, command, and any available correlation fields.

Return values:

```python
None
{"status": "success"}
{"status": "error", "code": "write_protected", "message": "Memory is write protected"}
```

`None` means unrelated or unrecognized traffic: keep waiting without extending the deadline. `success` is terminal. `error` is terminal and requires a nonempty machine-readable string `code` and a nonempty display string `message`.

A boolean, unknown status, missing required field, or Python exception is an adaptation error. Stop the upload and report it; never convert a parsing failure into success or silently disable acknowledgement handling. A malformed incoming MIDI message can return `None` when it cannot safely be identified as a reply.

No reply bytes are returned. The host never stores these acknowledgement messages as patch data or includes them in fingerprints.

### Capability discovery and timing

Neither function present means legacy behavior. Both callable functions present enable the capability. Exactly one present, or either present but noncallable, is an adaptation configuration error: reading/importing can remain available, but uploads must fail with a useful diagnostic.

Add one optional key to the existing timing dictionary:

```python
def messageTimings():
    return {
        "replyTimeoutMs": 1000,        # Existing download timing
        "uploadReplyTimeoutMs": 5000,  # Proposed write acknowledgement budget
    }
```

Default `uploadReplyTimeoutMs` to 5000 when absent. Require a positive integer. This is a proposed conservative default, not a measured K5000 requirement. Keep it independent of `replyTimeoutMs`: downloading and committing a write can have different timing. Existing `generalMessageDelay` remains applicable to outgoing pacing.

## K5000 example

The [K5000 discussion](https://github.com/christofmuc/KnobKraft-orm/issues/72#issuecomment-2665431616) specifies `F0 40 cc aa 00 0A F7` after receiving dump data. The current adaptation already defines constants for the five reply codes.

This example supports the current single-program conversion path. Other K5000 write formats must be added explicitly when their conversion is implemented.

```python
def prepareUpload(channel, messages):
    if len(messages) != 1 or not isSingleProgramDump(messages[0]):
        raise ValueError("K5000 upload acknowledgement supports one program dump")
    if not 0 <= channel <= 15 or messages[0][2] != channel:
        raise ValueError("K5000 upload channel mismatch")
    return {"channel": channel}


def classifyUploadReply(message, context):
    if (len(message) != 7
            or message[:3] != [0xF0, 0x40, context["channel"]]
            or message[4:] != [0x00, 0x0A, 0xF7]):
        return None

    code = message[3]
    if code == 0x40:
        return {"status": "success"}

    errors = {
        0x41: ("write_error", "The K5000 could not write the patch"),
        0x42: ("write_protected", "K5000 memory is write protected"),
        0x44: ("memory_full", "K5000 memory is full"),
        0x45: ("expansion_missing", "The required K5000 expansion board is missing"),
    }
    if code not in errors:
        return None
    error_code, description = errors[code]
    return {"status": "error", "code": error_code, "message": description}
```

The reply has no bank, program number, or transaction identifier. Matching the input, model, and channel is necessary, but cannot disambiguate two writes in flight. Serialize writes to that device and stop the bank on the first failure.

## C++ integration

Introduce `UploadAcknowledgementCapability.h` in MidiKraft's base module and `GenericUploadAcknowledgementCapability.{h,cpp}` alongside the existing Generic capability adapters. A possible shape is:

```cpp
struct UploadReply {
    enum class Status { Unrelated, Success, Error };
    Status status;
    std::string code;
    std::string message;
};

class UploadReplySession {
public:
    virtual ~UploadReplySession() = default;
    virtual UploadReply classify(const MidiMessage& message) = 0;
};

class UploadAcknowledgementCapability {
public:
    virtual ~UploadAcknowledgementCapability() = default;
    // nullptr means explicitly no acknowledgement for this upload.
    // Errors propagate to the upload operation.
    virtual std::unique_ptr<UploadReplySession> prepareUpload(
        MidiChannel channel, const std::vector<MidiMessage>& messages) const = 0;
    virtual int uploadReplyTimeoutMs() const { return 5000; }
};
```

The Generic session owns the Python context and a reference to its adaptation. Python objects stay inside the bridge; the MIDI/librarian layers do not need pybind11 types. Creation, invocation, and destruction of Python objects require the GIL.

Add the function-name constants and registry entries, runtime-capability inheritance, owned implementation instance, and both `hasCapability` overloads using the established `GenericAdaptation` pattern. Initialize the implementation in both constructors. Defer adaptation reload while an upload using that module is active, so a context cannot be passed to a changed function midway through a write.

Introduce an `UploadOperation` in the base layer, shared by individual sends and librarian bank uploads. It owns an immutable message snapshot, the selected input/output/channel, the reply session, listener registration, monotonic deadline, cancellation state, and exactly-once completion. It receives a shared synth reference to keep dependencies alive.

Its result should distinguish `Acknowledged`, `SentWithoutAcknowledgement`, `DeviceError`, `Timeout`, `Cancelled`, `TransportError`, and `AdaptationError`, with device code/message where applicable. For banks, also retain completed count, failed program, and whether that program's outcome is unknown. A boolean alone cannot explain partial completion.

### Operation lifecycle

1. Validate conversion output and prepare the reply session before any write. Empty output is an error, not a successful send. A failed conversion must not fall back to sending the original patch bytes; the current Generic conversion wrappers need an error-propagation path for uploads.
2. Acquire the device operation reservation. Prevent competing uploads, downloads, and detection against the same device while the write is active. A second request can report busy; a bank advances its own sequence under one reservation. Do not queue an unbounded series of audition clicks.
3. Enable and validate the output, and the selected input if a reply is required. Register the input handler before the first byte is submitted. A capability-enabled upload with no usable input fails before writing.
4. Dispatch the converted messages with existing pacing semantics through a nonblocking scheduler. Collect early matching replies during dispatch, but do not advance the bank until dispatch is complete. A terminal error stops any messages not yet submitted.
5. Finish on a terminal reply or the operation's deadline. Unrelated traffic never resets the deadline. If no acknowledgement is expected, complete after dispatch as `SentWithoutAcknowledgement`.
6. On every exit, unregister the listener, cancel scheduled work, release resources, and notify the caller exactly once on the UI thread. In-flight callbacks use weak ownership or operation generation checks to ignore completed operations.

MIDI input callbacks should copy the message and source identifier into the operation's serialized event queue. They must not run Python, block waiting for the GIL, or update UI objects. Copy the identifier rather than retaining a raw `MidiInput*`. Classification and timeout handling execute in the same serialized context, using receive timestamps to resolve replies at the deadline consistently.

Cancellation stops future submissions and waiting; it cannot undo bytes already handed to the MIDI driver. Disconnection and application shutdown follow the same cleanup path. The operation owns the data it needs, and asynchronous UI callbacks must not capture unprotected raw views or progress objects.

### Timing and correlation limits

The current `SafeMidiOutput` interface provides no physical transmission-complete event. Returning from a send call does not prove that the synth has received the final byte. In v1, use a conservative absolute deadline from first submission: scheduled pacing + estimated DIN transmission duration (0.32 ms per byte) + `uploadReplyTimeoutMs`. Account for any delay before first submission separately; do not start the reply budget while an operation is merely waiting to begin. Document the estimate and measure it on hardware.

The deadline belongs to the upload operation. Do not directly reuse the existing `MidiController` handler inactivity timeout: its activity timestamp currently updates for every incoming message, so unrelated MIDI traffic can keep a write waiting indefinitely. Similarly, the existing throttled sender sleeps its caller; it needs scheduled dispatch for this asynchronous path.

No automatic retries in v1. A missing ACK does not prove the write failed, and another write can consume a delayed ACK from the previous attempt. Timeout, transport failure, or cancellation after submission stops the whole sequence and marks the outcome uncertain; drop pending writes and require an explicit recovery/restart. Cancellation before submission has no such ambiguity.

K5000 replies cannot provide perfect correlation even with serialization: an external writer or a delayed duplicate can still look like the current response. Ignore already-queued messages predating the operation, and never automatically continue after an uncertain result. A quiet interval or reconnect does not prove protocol synchronization. After uncertainty, advise checking the synth's stored patch and restoring a known idle device before explicitly starting again; this API cannot manufacture a missing transaction ID.

## Call sites and user-visible behavior

For single-patch audition, `Synth::sendDataFileToSynth()` should submit an upload operation rather than directly invoking the byte sender. Add an asynchronous overload/result callback; retain the existing wrapper with a default error-reporting sink for callers that do not yet consume results. `PatchView::sendPatchAsSysex()` should use the result to display device errors and unknown outcomes. A selected patch in the UI is not evidence that the hardware accepted it.

For bank sending, replace the immediate loop with a continuation that starts the next program only when the previous operation completes successfully. Device-specific bank conversion produces one logical upload and passes through the same capability. Keep the bank and outgoing patch snapshot alive throughout the asynchronous sequence.

For an acknowledged device, increment completed progress only after acknowledgement. On failure, report the patch and reason, such as "Stopped at D017: memory full; 16 patches confirmed." On timeout/cancellation after submission, explicitly say that the current patch may have been written. Keep the bank dirty on any incomplete result; v1 can conservatively retain all dirty flags rather than introducing partial clearing.

On complete success, clear dirty flags only for the unchanged snapshot that was sent. Either prevent editing that bank while its upload runs or compare revisions before clearing; otherwise edits made during the asynchronous operation could be incorrectly marked synchronized.

Adaptations without the hooks preserve their existing sending behavior and completion policy, with the internal outcome labeled `SentWithoutAcknowledgement`. They do not require an input connection. Low-level send calls used for requests, program changes, or download replies remain outside this API, subject to the device reservation when they would conflict with an active upload.

File export, clipboard conversion, offline import, and download recognition do not invoke either hook. Do not change `isPartOfSingleProgramDump()` or `isPartOfEditBufferDump()` to accept write responses. Logging can still observe every incoming message; an upload reply is not patch content.

## Implementation and validation plan

1. Implement the capability, bridge validation, and optional timing key. Document the hooks in the Adaptation Programming Guide and adaptation overview when implementation lands.
2. Implement the asynchronous operation, output pacing, device reservation, and fake-transport tests. Cover source filtering, a reply arriving during send, unrelated traffic through the deadline, cancellation, disconnect, and exactly-once cleanup/completion.
3. Route individual and bank upload paths through it. Test that a failed conversion sends nothing, failure at patch N prevents patch N+1, acknowledgement controls progress, and an incomplete bank remains dirty. Preserve the current empty-slot preflight and legacy adaptation behavior.
4. Add the K5000 hooks and focused Python tests for all five codes, wrong channel/model/framing, unrelated messages, and unsupported upload shapes. Test bridge rejection of partial hook definitions, invalid timing, exceptions, and malformed result dictionaries.
5. Verify the real K5000 with a known patch, write protection, available memory/expansion error conditions, and a missing reply. Capture MIDI traces to validate timing and stop behavior. This is the gate for claiming #426 fixed; simulated ACKs alone cannot establish hardware acceptance.

A later capability can support multi-step upload protocols with explicit outgoing actions and per-step state. The two hooks here deliberately solve the post-write acknowledgement requirement without changing existing conversion APIs or treating write responses as downloaded patches.
