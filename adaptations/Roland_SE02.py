# Roland SE-02 adaptation for KnobKraft Orm
# Stable v1 workflow: audition via Edit Buffer (DT1). Permanent storage via manual WRITE on the SE-02.

from __future__ import annotations

from typing import Dict, List, Sequence, Union
import hashlib
from collections import deque

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

# During Synth Bank import, we read everything via EDIT BUFFER (bb==0).
# Orm still wants a stable program number. Orm may call numberFromDump() later
# and multiple times per patch, so we use:
#   - a FIFO queue of requested program slots
#   - an md5->program cache to make numberFromDump idempotent
_REQUESTED_PROG_QUEUE = deque()        # type: ignore[var-annotated]
_DUMP_MD5_TO_PROG: dict[str, int] = {}


def _trace(s: str) -> None:
    # Tracing disabled in public release
    return




# -----------------------------
# Required by Orm
# -----------------------------
def name() -> str:
    return "Roland SE-02"


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


# -----------------------------
# Roland 7-bit checksum helper
# -----------------------------
def _roland_checksum_7bit(addr_and_data: bytes) -> int:
    """Roland 7-bit checksum used by DT1.

    Computed over address(4) + data(N), excluding F0..header and excluding checksum/F7.
    """
    s = sum((b & 0x7F) for b in addr_and_data) & 0x7F
    return (-s) & 0x7F


def _is_se02_dt1(msg: bytes) -> bool:
    # Typical SE-02 DT1:
    # F0 41 <dev> 00 00 00 44 12 ... F7
    return (
        len(msg) >= 12
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


def channelIfValidDeviceResponse(msg):
    """
    Identity Reply:
      F0 7E <dev> 06 02 41 44 ...
    Roland-style: 0x10..0x1F maps to channel 1..16
    """
    try:
        if (
            len(msg) >= 11
            and msg[0] == 0xF0
            and msg[1] == 0x7E
            and msg[3] == 0x06
            and msg[4] == 0x02
            and msg[5] == 0x41
        ):
            dev = msg[2]
            if 0x10 <= dev <= 0x1F:
                ch = (dev - 0x10) + 1
                _trace(f"Identity Reply dev=0x{dev:02X} -> channel={ch}")
                return ch
            if 0x00 <= dev <= 0x0F:
                ch = dev + 1
                _trace(f"Identity Reply dev={dev} -> channel={ch}")
                return ch
            _trace(f"Identity Reply dev=0x{dev:02X} not mappable")
            return -1
    except Exception as e:
        _trace(f"channelIfValidDeviceResponse exception: {e}")
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
        len(b) >= 6 and b[0] == 0xF0 and b[1] == 0x7E and b[3] == 0x06 and b[4] == 0x02 and b[5] == 0x41
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
    md5p = hashlib.md5(blob).hexdigest()[:6] if blob else "------"
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
                if bb == 0:
                    # Edit-buffer read: Orm may call this later; bind dump md5 -> queued slot.
                    md5 = hashlib.md5(blob).hexdigest()
                    if md5 in _DUMP_MD5_TO_PROG:
                        prog = _DUMP_MD5_TO_PROG[md5]
                        _trace(f"numberFromDump: addr={a} bb=0 md5hit -> {prog}")
                        return int(prog)
                    prog = int(_REQUESTED_PROG_QUEUE.popleft()) if _REQUESTED_PROG_QUEUE else 0
                    _DUMP_MD5_TO_PROG[md5] = prog
                    _trace(f"numberFromDump: addr={a} bb=0 md5bind -> {prog}")
                    return int(prog)
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

    def _split_sysex_blob(b: bytes):
        out = []
        i, n = 0, len(b)
        while i < n:
            while i < n and b[i] != 0xF0:
                i += 1
            if i >= n:
                break
            j = i + 1
            while j < n and b[j] != 0xF7:
                j += 1
            if j < n and b[j] == 0xF7:
                out.append(bytearray(b[i : j + 1]))
                i = j + 1
            else:
                break
        return out

    msgs = _split_sysex_blob(blob) if blob else []

    touched_dt1 = 0
    addr_fixed = 0
    dev_fixed = 0

    dev = None
    if isinstance(channel, int) and 1 <= channel <= 16:
        dev = 0x10 + (channel - 1)

    for m in msgs:
        if not (
            len(m) >= 13
            and m[0] == 0xF0
            and m[1] == 0x41
            and m[6] == 0x44
            and m[7] == 0x12
            and m[-1] == 0xF7
        ):
            continue

        changed = False

        # Device-id patch (ignore broadcast 0x7F)
        if dev is not None and m[2] != 0x7F and m[2] != dev:
            m[2] = dev
            dev_fixed += 1
            changed = True

        # Force DT1 target address -> EDIT BUFFER (bb=0x00)
        if m[8] == 0x05 and m[9] != 0x00:
            m[9] = 0x00
            addr_fixed += 1
            changed = True

        if changed:
            addr_and_data = bytes(m[8:-2])
            m[-2] = _roland_checksum_7bit(addr_and_data)
            touched_dt1 += 1

    out_blob = b"".join(bytes(m) for m in msgs) if msgs else blob
    _trace(
        f"convertToEditBuffer: channel={channel} out_len={len(out_blob)} msgs={len(msgs)} "
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
    blob = _to_blob(message)
    if len(blob) < 12:
        return False
    msgs = _split_sysex(blob) if blob.count(b"\xF0") >= 2 else ([blob] if blob else [])
    dt1 = [m for m in msgs if _is_editbuf_dt1(m)]
    ok = (len(dt1) == 4)
    if ok:
        _trace(f"isSingleProgramDump(len={len(blob)} msgs={len(msgs)} ok={ok})")
    return ok


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
    # Normalize input -> bytes blob
    blob = _to_blob(message)

    def _split_sysex_blob(b: bytes):
        out = []
        i, n = 0, len(b)
        while i < n:
            while i < n and b[i] != 0xF0:
                i += 1
            if i >= n:
                break
            j = i + 1
            while j < n and b[j] != 0xF7:
                j += 1
            if j < n and b[j] == 0xF7:
                out.append(bytearray(b[i:j+1]))
                i = j + 1
            else:
                break
        return out

    msgs = _split_sysex_blob(blob) if blob else []

    touched_dt1 = 0
    addr_fixed = 0
    dev_fixed = 0

    # Target slot (bb) is 7-bit.
    try:
        target_bb = int(program_number) & 0x7F
    except Exception:
        target_bb = 0

    dev = None
    if isinstance(channel, int) and 1 <= channel <= 16:
        dev = 0x10 + (channel - 1)

    for m in msgs:
        if not (len(m) >= 13 and m[0] == 0xF0 and m[1] == 0x41 and m[6] == 0x44 and m[7] == 0x12 and m[-1] == 0xF7):
            continue

        changed = False

        # Device-id patch (ignore broadcast 0x7F)
        if dev is not None and m[2] != 0x7F and m[2] != dev:
            m[2] = dev
            dev_fixed += 1
            changed = True

        # Retarget DT1 to program slot (05 bb .. ..)
        if m[8] == 0x05 and m[9] != target_bb:
            m[9] = target_bb
            addr_fixed += 1
            changed = True

        if changed:
            addr_and_data = bytes(m[8:-2])
            m[-2] = _roland_checksum_7bit(addr_and_data)
            touched_dt1 += 1

    out_blob = b"".join(bytes(m) for m in msgs) if msgs else blob
    _trace(
        f"convertToProgramDump: channel={channel} program_number={program_number} target_bb={target_bb} "
        f"out_len={len(out_blob)} msgs={len(msgs)} addr_fixed={addr_fixed} dev_fixed={dev_fixed} dt1_touched={touched_dt1}"
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

