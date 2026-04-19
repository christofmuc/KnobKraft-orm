# Roland SE-02 adaptation for KnobKraft Orm
# Stable v1 workflow: audition via Edit Buffer (DT1). Permanent storage via manual WRITE on the SE-02.

from __future__ import annotations

from typing import Dict, List, Sequence, Union
import hashlib

# -----------------------------
# Constants (SE-02 / Roland DT1)
# -----------------------------
ROLAND_ID = 0x41
MODEL_ID = 0x44
CMD_DT1 = 0x12  # Data Transfer 1 (DT1)

# SE-02 edit buffer dump blocks (DT1 address starts with 0x05)
EDITBUF_ADDR_PREFIX = (0x05,)

# Optional tracing (disabled by default in public release)
TRACE_ENABLED = False
TRACE_PATH = None

# Adaptation version marker (bump when we freeze a stable release)
ADAPTATION_VERSION = "v1.0.0"


def _trace(s: str) -> None:
    # Tracing disabled in public release
    return




# -----------------------------
# Required by Orm
# -----------------------------
def name() -> str:
    return "Roland SE-02"


def setupHelp() -> str:
    return (
        "On the SE-02, enable SysEx receive/transmit in the system MIDI settings. "
        "This adaptation supports importing SE-02 DT1 editor dumps and auditioning "
        "patches by sending them to the edit buffer. Store a sound permanently with "
        "WRITE on the SE-02 after auditioning."
    )


# -----------------------------
# Byte / SysEx helpers
# -----------------------------
def _to_blob(m) -> bytes:
    """Normalize Orm inputs into ONE contiguous bytes blob.

    Handles:
      - bytes/bytearray
      - list[int]
      - list[bytes] / list[MidiMessage] / list[mixed]  -> concatenates each element
      - pybind MidiMessage-like objects that support bytes(...)
    """
    if m is None:
        return b""
    if isinstance(m, (bytes, bytearray)):
        return bytes(m)

    # list/tuple can be either list[int] OR list[messages]
    if isinstance(m, (list, tuple)):
        if not m:
            return b""
        # list[int]
        if all(isinstance(x, int) for x in m):
            return bytes((int(x) & 0xFF) for x in m)
        # list[messages] -> concatenate
        parts = []
        for el in m:
            b = _to_blob(el)
            if b:
                parts.append(b)
        return b"".join(parts)

    try:
        return bytes(m)
    except Exception:
        return b""


def _split_sysex(blob: bytes) -> List[bytes]:
    """
    Split a blob containing multiple SysEx messages into individual messages.
    Each message is expected to start with 0xF0 and end with 0xF7.

    IMPORTANT: return raw bytes here (NOT MidiMessage),
    so detection/import logic stays pure bytes-based.
    """
    out: List[bytes] = []
    i, n = 0, len(blob)
    while i < n:
        while i < n and blob[i] != 0xF0:
            i += 1
        if i >= n:
            break
        j = i + 1
        while j < n and blob[j] != 0xF7:
            j += 1
        if j < n and blob[j] == 0xF7:
            out.append(blob[i : j + 1])
            i = j + 1
        else:
            break
    return out


def _split_sysex_mutable(blob: bytes) -> List[bytearray]:
    """Like _split_sysex, but returns mutable messages for in-place retargeting."""
    return [bytearray(message) for message in _split_sysex(blob)]


def _md5_hexdigest(data: bytes) -> str:
    """Create a non-security md5 digest while staying compatible with older Python."""
    try:
        return hashlib.md5(data, usedforsecurity=False).hexdigest()
    except TypeError:
        return hashlib.md5(data).hexdigest()


# -----------------------------
# Roland 7-bit checksum helper
# -----------------------------
def _roland_checksum_7bit(addr_and_data: bytes) -> int:
    """Roland 7-bit checksum used by DT1.

    Computed over address(4) + data(N), excluding F0..header and excluding checksum/F7.
    """
    s = sum((b & 0x7F) for b in addr_and_data) & 0x7F
    return (-s) & 0x7F


def _device_id_from_channel(channel) -> Union[int, None]:
    """Map Orm's zero-based MIDI channel to Roland's 0x10..0x1f device id."""
    if isinstance(channel, int) and 0 <= channel < 16:
        return 0x10 + channel
    return None


def _build_dt1(dev_id: int, addr4: Sequence[int], data: Sequence[int]) -> List[int]:
    """Build a Roland DT1 message and checksum it."""
    address = [(a & 0x7F) for a in list(addr4)[:4]]
    payload = [(d & 0x7F) for d in data]
    checksum = _roland_checksum_7bit(bytes(address + payload))
    return [0xF0, ROLAND_ID, dev_id & 0x7F, 0x00, 0x00, 0x00, MODEL_ID, CMD_DT1] + address + payload + [checksum, 0xF7]


def _retarget_dt1(blob: bytes, channel, target_bb: int):
    """Retarget SE-02 DT1 messages to a device id and 05 bb address slot."""
    msgs = _split_sysex_mutable(blob) if blob else []
    touched_dt1 = 0
    addr_fixed = 0
    dev_fixed = 0
    dev = _device_id_from_channel(channel)

    for m in msgs:
        if not (
            len(m) >= 14
            and m[0] == 0xF0
            and m[1] == ROLAND_ID
            and m[6] == MODEL_ID
            and m[7] == CMD_DT1
            and m[-1] == 0xF7
        ):
            continue

        changed = False
        if dev is not None and m[2] != dev:
            m[2] = dev
            dev_fixed += 1
            changed = True
        if m[8] == 0x05 and m[9] != target_bb:
            m[9] = target_bb
            addr_fixed += 1
            changed = True
        if changed:
            m[-2] = _roland_checksum_7bit(bytes(m[8:-2]))
            touched_dt1 += 1

    out_blob = b"".join(bytes(m) for m in msgs) if msgs else blob
    return out_blob, len(msgs), addr_fixed, dev_fixed, touched_dt1


def _is_se02_dt1(msg: bytes) -> bool:
    # Typical SE-02 DT1:
    # F0 41 <dev> 00 00 00 44 12 ... F7
    return (
        len(msg) >= 14
        and msg[0] == 0xF0
        and msg[1] == ROLAND_ID
        and msg[6] == MODEL_ID
        and msg[7] == CMD_DT1
        and msg[-1] == 0xF7
    )


def _addr4(msg: bytes):
    # Address is 4 bytes right after CMD byte:
    # ... 44 12 aa bb cc dd ...
    if len(msg) < 12:
        return None
    return (msg[8], msg[9], msg[10], msg[11])


def _is_editbuf_dt1(msg: bytes) -> bool:
    if not _is_se02_dt1(msg):
        return False
    a = _addr4(msg)
    return bool(a) and a[0] in EDITBUF_ADDR_PREFIX


# -----------------------------
# Device detect (KEEP STABLE)
# -----------------------------
def createDeviceDetectMessage(channel):
    # Universal Identity Request (broadcast)
    _trace(f"createDeviceDetectMessage(channel={channel}) -> BROADCAST 7F")
    return [0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7]


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    return 200


def channelIfValidDeviceResponse(msg):
    """
    Identity Reply:
      F0 7E <dev> 06 02 41 44 ...
    Roland-style: 0x10..0x1F maps to Orm's zero-based channels 0..15.
    """
    if (
        len(msg) >= 11
        and msg[0] == 0xF0
        and msg[1] == 0x7E
        and msg[3] == 0x06
        and msg[4] == 0x02
        and msg[5] == ROLAND_ID
        and msg[6] == MODEL_ID
    ):
        dev = msg[2]
        if 0x10 <= dev <= 0x1F:
            ch = dev - 0x10
            _trace(f"Identity Reply dev=0x{dev:02X} -> channel={ch}")
            return ch
        if 0x00 <= dev <= 0x0F:
            ch = dev
            _trace(f"Identity Reply dev={dev} -> channel={ch}")
            return ch
        _trace(f"Identity Reply dev=0x{dev:02X} not mappable")
    return -1


# -----------------------------
# Import path: Orm reads files → calls these
# -----------------------------
def isOwnSysex(message) -> bool:
    """
    Helps Orm decide which adaptation matches incoming SysEx.
    """
    b = _to_blob(message)
    return _is_se02_dt1(b) or (
        len(b) >= 7
        and b[0] == 0xF0
        and b[1] == 0x7E
        and b[3] == 0x06
        and b[4] == 0x02
        and b[5] == 0x41
        and b[6] == MODEL_ID
    )


def isPartOfEditBufferDump(message) -> bool:
    """
    True for DT1 blocks that belong to an edit buffer dump.
    (We accept any DT1 with addr0 == 0x05.)
    """
    b = _to_blob(message)
    # Orm may feed raw MIDI bytes while listening; avoid TRACE spam for tiny fragments.
    if len(b) < 12:
        return False
    ok = _is_editbuf_dt1(b)
    if ok:
        _trace(f"isPartOfEditBufferDump(len={len(b)} ok={ok})")
    return ok


def isEditBufferDump(message) -> bool:
    """
    True if the blob contains a full edit buffer dump (4 DT1 messages).
    We detect this by splitting the blob into SysEx messages and counting DT1 blocks.
    """
    blob = _to_blob(message)
    if len(blob) < 12:
        return False
    msgs = _split_sysex(blob) if blob.count(b"\xF0") >= 2 else ([blob] if blob else [])
    dt1 = [m for m in msgs if _is_editbuf_dt1(m)]
    ok = (len(dt1) == 4)
    if ok:
        _trace(f"isEditBufferDump(len={len(blob)} msgs={len(msgs)} ok={ok})")
    return ok


def nameFromDump(message) -> str:
    """
    Extract patch name from the 4th block:
      address typically 05 00 01 40
    """
    blob = _to_blob(message)
    msgs = _split_sysex(blob) if blob.count(b"\xF0") >= 2 else ([blob] if blob else [])
    for m in msgs:
        if _is_se02_dt1(m):
            a = _addr4(m)
            # Name is stored at 05 bb 01 40 (bb differs depending on source: editor bank save vs edit buffer)
            if a and a[0] == 0x05 and a[2] == 0x01 and a[3] == 0x40:
                data = m[12:-2] if len(m) >= 14 else b""

                # SE-02 name block often contains 0x00 padding BEFORE the ASCII name.
                # Extract the first printable ASCII run, then stop at the first 0x00 AFTER we started.
                chars: List[str] = []
                started = False
                for x in data:
                    if x == 0x00:
                        if started:
                            break
                        continue
                    if 32 <= x <= 126:
                        started = True
                        chars.append(chr(x))
                    else:
                        # Non-ASCII data inside the name block: ignore until started; stop once started.
                        if started:
                            break

                nm = "".join(chars).strip()
                _trace(f"nameFromDump: addr={a} name={'<empty>' if not nm else nm}")
                return nm if nm else "SE-02 (imported)"

    # Fallback: many SE-02 editor exports do not include printable ASCII at 05 bb 01 40.
    # Provide a stable, human-useful name so Orm UI is not blank/identical for all patches.
    bb = None
    for m in msgs:
        if _is_se02_dt1(m):
            a = _addr4(m)
            if a and a[0] == 0x05:
                bb = a[1]
                break
    md5p = _md5_hexdigest(blob)[:6] if blob else "------"
    if bb is None:
        _trace(f"nameFromDump: fallback bb=None md5={md5p}")
        return f"SE-02 {md5p}"
    _trace(f"nameFromDump: fallback bb={bb} md5={md5p}")
    return f"SE-02 {bb:02d} {md5p}"


def numberFromDump(message) -> int:
    """Derive a program/slot number from the dump when available.

    Many editor exports encode the slot in the address as 05 bb ....
    For true edit-buffer dumps this is typically bb==0.

    We return bb as an integer (0..127) so Orm can display something stable.
    """
    blob = _to_blob(message)
    msgs = _split_sysex(blob) if blob.count(b"\xF0") >= 2 else ([blob] if blob else [])
    for m in msgs:
        if _is_se02_dt1(m):
            a = _addr4(m)
            if a and a[0] == 0x05:
                bb = int(a[1])
                _trace(f"numberFromDump: addr={a} -> {bb}")
                return bb
    return 0


# -----------------------------
# Orm program/bank number hooks (some Orm versions use these names)
# -----------------------------
def midiBankNumberFromDump(message) -> int:
    # SE-02 exports we handle do not encode a MIDI bank number.
    return 0


def midiProgramNumberFromDump(message) -> int:
    return numberFromDump(message)


def blankedOut(message) -> List[int]:
    """Normalize a patch for stable comparisons.

    Program slot, device id, and DT1 checksum are transport/location details.
    They should not affect duplicate detection.
    """
    blob = _to_blob(message)
    msgs = _split_sysex(blob) if blob.count(b"\xF0") >= 2 else ([blob] if blob else [])
    normalized: List[int] = []
    for msg in msgs:
        data = bytearray(msg)
        if _is_se02_dt1(bytes(data)):
            data[2] = 0x00
            if data[8] == 0x05:
                data[9] = 0x00
            data[-2] = 0x00
        normalized.extend(data)
    return normalized


def calculateFingerprint(message) -> str:
    normalized = blankedOut(message)
    if not normalized:
        normalized = list(_to_blob(message))
    return _md5_hexdigest(bytes(normalized))


def extractPatches(messages):
    """
    File import hook:
    Orm gives a list of SysEx messages; we return a list of "patch blobs".
    SE-02 edit buffer dump = 4 DT1 messages.
    We concatenate 4 DT1 blocks into one blob.
    """
    _trace(f"extractPatches(count={len(messages) if messages is not None else 'None'})")
    if not messages:
        return []

    dt1_blocks: List[bytes] = []
    for m in messages:
        b = _to_blob(m)
        if _is_editbuf_dt1(b):
            dt1_blocks.append(b)

    _trace(f"extractPatches: editbuf_dt1_blocks={len(dt1_blocks)}")

    patches: List[bytes] = []
    for i in range(0, len(dt1_blocks) // 4):
        blocks = dt1_blocks[i * 4 : (i + 1) * 4]
        patches.append(b"".join(blocks))
    remainder = len(dt1_blocks) % 4
    if remainder:
        _trace(f"extractPatches: ignored incomplete trailing block group of {remainder} DT1 messages")

    _trace(f"extractPatches: returning patches={len(patches)}")
    return patches


# -----------------------------
# Optional: some Orm versions use this older hook
# -----------------------------
def isPatch(message):
    """
    Back-compat: treat a blob containing >= 4 editbuf DT1 as a patch.
    """
    blob = _to_blob(message)
    msgs = _split_sysex(blob) if blob.count(b"\xF0") >= 2 else ([blob] if blob else [])
    dt1 = [m for m in msgs if _is_editbuf_dt1(m)]
    ok = (len(dt1) >= 4)
    _trace(f"isPatch(len={len(blob)} msgs={len(msgs)} ok={ok})")
    return ok


# -----------------------------
# Send path: Orm sends patch to synth
# -----------------------------
def createEditBufferRequest(channel):
    """
    SE-02 doesn't reliably support requesting an edit buffer dump from the synth.
    We implement this so Orm is satisfied; return empty = no request.
    """
    return []


def convertToEditBuffer(channel, message):
    """Retarget incoming DT1 blocks to the SE-02 edit buffer (bb=0x00).

    Verified stable workflow for auditioning patches from Orm.
    """
    blob = _to_blob(message)

    # Remember input shape so we can return something compatible.
    in_is_bytes = isinstance(message, (bytes, bytearray))
    in_is_int_list = isinstance(message, (list, tuple)) and all(isinstance(x, int) for x in message)
    out_blob, msg_count, addr_fixed, dev_fixed, touched_dt1 = _retarget_dt1(blob, channel, 0)

    _trace(
        f"convertToEditBuffer: channel={channel} out_len={len(out_blob)} msgs={msg_count} "
        f"addr_fixed={addr_fixed} dev_fixed={dev_fixed} dt1_touched={touched_dt1}"
    )

    if in_is_int_list:
        return list(out_blob)
    if in_is_bytes:
        return out_blob
    return out_blob





# -----------------------------
# Bank / program management (PROBE + COMPAT)
#
# Orm's built-in adaptations commonly implement:
#   - createProgramDumpRequest(channel, patchNo)
#   - convertToProgramDump(channel, message, program_number)
#   - createProgramChangeMessage(channel, program)
#
# Our earlier probe signatures included (bank, program). This Orm build
# appears to prefer the simpler (program_number) shape, so we provide
# compatible functions below.
# -----------------------------

# ---- RQ1 helper for SE-02 (Roland) ----

def _rq1_request(dev_id: int, addr4: Sequence[int], size4: Sequence[int]) -> List[int]:
    """Build a Roland RQ1 (Data Request 1) SysEx for SE-02 (model 0x44).

    TODO: enable program-bank reads once the SE-02 RQ1 address/size map is verified.

    Format:
      F0 41 <dev> 00 00 00 44 11 aa bb cc dd ss tt uu vv cs F7

    Checksum is Roland 7-bit over address(4)+size(4).
    """
    payload = bytes([(a & 0x7F) for a in list(addr4)[:4]] + [(s & 0x7F) for s in list(size4)[:4]])
    cs = _roland_checksum_7bit(payload)
    return [0xF0, 0x41, dev_id & 0x7F, 0x00, 0x00, 0x00, MODEL_ID, 0x11,
            payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7],
            cs, 0xF7]


# ---- Bank descriptors for Orm Library tree ----

def bankDescriptors() -> List[Dict]:
    """Expose synth banks for Orm's Library tree / Synth Bank tab.

    SE-02 effectively has one writable USER bank with 128 program slots.
    """
    desc = [{"bank": 0, "name": "USER", "size": 128, "type": "Patch"}]
    _trace(f"bankDescriptors() -> {desc}")
    return desc


def numberOfBanks() -> int:
    return len(bankDescriptors())


def numberOfPatchesPerBank() -> int:
    return bankDescriptors()[0]["size"]


def friendlyBankName(bank) -> str:
    banks = bankDescriptors()
    try:
        bank_index = int(bank)
    except Exception:
        bank_index = -1
    if 0 <= bank_index < len(banks):
        return banks[bank_index]["name"]
    return f"Bank {bank}"


def friendlyProgramName(program) -> str:
    return f"USER-{(int(program) & 0x7F) + 1:03d}"


def bankSelect(channel, bank):
    """Select a bank on the synth (if needed).

    For SE-02 we treat USER as the only bank; no MIDI message required.
    Returning [] keeps this side-effect free.
    """
    _trace(f"bankSelect(channel={channel}, bank={bank}) called -> []")
    return []


def supportsBankSelect():
    """Return True if this synth supports bank select in Orm UI.

    For SE-02 we expose one logical bank (USER) with 128 program slots.
    """
    _trace("supportsBankSelect() -> True")
    return True


def bankNames():
    """Bank names shown under 'In synth'."""
    _trace("bankNames() -> ['USER']")
    return ["USER"]


def numberOfProgramsInBank(bank: int) -> int:
    """Number of program slots in a bank."""
    _trace(f"numberOfProgramsInBank(bank={bank}) -> 128")
    return 128


def createProgramDumpRequest(channel, patchNo):
    """Request a single program from the synth.

    Public v1.0: not implemented.

    In this setup the SE-02 does not reliably recall patches via incoming Program Change,
    and the RQ1 address/size map for reliable reads is not yet verified.

    Verified stable workflow instead:
      - audition via convertToEditBuffer (DT1 to edit buffer)
      - permanent storage via manual WRITE on the SE-02

    Returning [] prevents Orm from attempting bank-import reads in v1.0.
    """
    _trace(f"createProgramDumpRequest(channel={channel}, patchNo={patchNo}) -> [] (v1.0)")
    return []


def isPartOfSingleProgramDump(message) -> bool:
    """Return True for messages that can be part of a single program dump.

    Orm uses this during bank import while listening to incoming MIDI.
    For SE-02 a program dump (as we currently handle it) is the same 4-DT1
    edit-buffer style dump (address prefix 0x05).
    """
    b = _to_blob(message)
    if len(b) < 12:
        return False
    ok = _is_editbuf_dt1(b)
    if ok:
        _trace(f"isPartOfSingleProgramDump(len={len(b)} ok={ok})")
    return ok


def isSingleProgramDump(message) -> bool:
    """Return True if the blob contains a complete single program dump.

    For SE-02 we treat a complete program dump as 4 DT1 messages.
    """
    return isEditBufferDump(message)


def createProgramChangeMessage(channel, program):
    """PROBE: program change.

    For now we only log. Some synths need this before a dump request.
    """
    _trace(f"createProgramChangeMessage(channel={channel}, program={program}) called -> []")
    return []


def convertToProgramDump(channel, message, program_number):
    """Write a patch into a specific SE-02 program slot (05 bb ....).

    This is the first concrete step toward USER-bank management:
    - Editor bank exports encode slot in bb (05 bb cc dd).
    - We retarget bb to `program_number` and recompute DT1 checksums.

    IMPORTANT: This will overwrite the target slot on the synth if SE-02
    accepts DT1 writes to program memory.
    """
    blob = _to_blob(message)
    try:
        target_bb = int(program_number)
    except Exception:
        target_bb = 0
    if not 0 <= target_bb <= 127:
        _trace(f"convertToProgramDump: invalid program_number={program_number}, defaulting to USER slot 0")
        target_bb = 0
    out_blob, msg_count, addr_fixed, dev_fixed, touched_dt1 = _retarget_dt1(blob, channel, target_bb)
    _trace(
        f"convertToProgramDump: channel={channel} program_number={program_number} target_bb={target_bb} "
        f"out_len={len(out_blob)} msgs={msg_count} addr_fixed={addr_fixed} dev_fixed={dev_fixed} dt1_touched={touched_dt1}"
    )

    # Return same kind Orm gave us
    if isinstance(message, (list, tuple)):
        return [b for b in out_blob]
    if isinstance(message, (bytes, bytearray)):
        return out_blob
    try:
        return message.__class__(out_blob)
    except Exception:
        return [b for b in out_blob]


def make_test_data():
    import testing

    def make_patch(slot: int, name_text: str) -> List[int]:
        dev = 0x10
        name_bytes = [0x00, 0x00] + [ord(c) for c in name_text] + [0x00]
        messages = [
            _build_dt1(dev, [0x05, slot, 0x00, 0x00], [0x01, 0x02, 0x03, 0x04]),
            _build_dt1(dev, [0x05, slot, 0x00, 0x40], [0x11, 0x12, 0x13]),
            _build_dt1(dev, [0x05, slot, 0x01, 0x00], [0x21, 0x22, 0x23]),
            _build_dt1(dev, [0x05, slot, 0x01, 0x40], name_bytes),
        ]
        return [byte for message in messages for byte in message]

    def programs(_test_data):
        yield testing.ProgramTestData(
            message=make_patch(17, "SE02TEST"),
            name="SE02TEST",
            number=17,
            friendly_number="USER-018",
            target_no=23,
        )

    def edit_buffers(_test_data):
        yield testing.ProgramTestData(
            message=make_patch(0, "EDITTEST"),
            name="EDITTEST",
            target_no=23,
        )

    return testing.TestData(
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        device_detect_call=[0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7],
        device_detect_reply=([0xF0, 0x7E, 0x10, 0x06, 0x02, 0x41, MODEL_ID, 0x00, 0x00, 0x00, 0xF7], 0),
        friendly_bank_name=(0, "USER"),
    )
