#
#   Custom KnobKraft Adaptation for Behringer UB-Xa
#   Session-based FDS parser (multi-device safe)
#

from typing import List
from knobkraft import unescapeSysex_deepmind as unescape_sysex
import hashlib
import testing

_MAX_NAME_CHARS = 16

# ---------------------------------------------------------------------------
# Manufacturer / Product IDs & Undocumented Wrappers
# ---------------------------------------------------------------------------

behringer_id = [0x00, 0x20, 0x32]
ubxa_product = [0x00, 0x01, 0x21]

sysex_prefix = [0xF0] + behringer_id + ubxa_product
sysex_suffix = 0xF7

_PL = len(sysex_prefix)

# Undocumented FDS wrappers discovered via official app trace
_BIN_PREFIX = [0x7F, 0x42, 0x49, 0x4E, 0x20] # 7F B I N (space)
_FW_VERSION = [0x00, 0x03, 0x7C]             # v0.3.124

# ---------------------------------------------------------------------------
# FDS session tracking
# ---------------------------------------------------------------------------

class _FDSSession:
    def __init__(self, device_id: int):
        self.device_id = device_id
        self.messages = []
        self.has_header = False
        self.has_eof = False
        self.complete = False

_fds_sessions = {}

def _get_session(device_id: int) -> _FDSSession:
    if device_id not in _fds_sessions:
        _fds_sessions[device_id] = _FDSSession(device_id)
    return _fds_sessions[device_id]

# ---------------------------------------------------------------------------
# Identity
# ---------------------------------------------------------------------------

def name() -> str:
    return "Behringer UB-Xa"

def setupHelp() -> str:
    return (
        "UB-Xa uses FDS SysEx with ACK-per-packet handshake.\n"
        "This adapter incorporates the undocumented V0.3.124+ BIN wrappers.\n\n"
        "IMPORTANT: Ensure 'Rx PC' (Receive Program Change) is turned ON in the UB-Xa Global MIDI Settings so auditioning patches works!"
    )

# ---------------------------------------------------------------------------
# Timing
# ---------------------------------------------------------------------------

def messageTimings():
    return {
        "generalMessageDelay": 30,
        "deviceDetectWaitMilliseconds": 5000,
        "replyTimeoutMs": 8000,
    }

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _split_sysex(data) -> List[List[int]]:
    result = []
    i = 0
    data = list(data)

    while i < len(data):
        if data[i] == 0xF0:
            start = i
            while i < len(data) and data[i] != 0xF7:
                i += 1
            if i < len(data):
                result.append(data[start:i + 1])
        i += 1

    return result

def _is_fds(msg, fds_type=None) -> bool:
    if not hasattr(msg, '__len__') or len(msg) < _PL + 4:
        return False
    if list(msg[0:_PL]) != sysex_prefix:
        return False
    if msg[_PL + 1] != 0x74 or msg[_PL + 2] != 0x07:
        return False
    if fds_type is not None:
        return msg[_PL + 3] == fds_type
    return True

def _fds_device_id(msg) -> int:
    return msg[_PL]

def _fds_packet_num(msg) -> int:
    return msg[_PL + 4] if len(msg) > _PL + 4 else 0x00

# ---------------------------------------------------------------------------
# ACK + SESSION HANDLING
# ---------------------------------------------------------------------------

def isPartOfSingleProgramDump(message):
    if (len(message) == 6
            and message[0] == 0xF0
            and message[1] == 0x7E
            and message[3] == 0x7E  # ACK byte
            and message[5] == 0xF7):
        return True  # absorb with no reply

    if _is_fds(message, 0x01):
        device_id = _fds_device_id(message)
        return True, [0xF0, 0x7E, device_id, 0x7E, 0x00, 0xF7]

    if _is_fds(message, 0x02):
        device_id = _fds_device_id(message)
        pkt_num = _fds_packet_num(message)
        return True, [0xF0, 0x7E, device_id, 0x7E, pkt_num, 0xF7]

    if _is_ubxa_eof(message):
        device_id = message[2]
        pkt_num = message[4]
        return True, [0xF0, 0x7E, device_id, 0x7E, pkt_num, 0xF7]

    return False

def needsChannelSpecificDetection() -> bool:
    return False
    
# ---------------------------------------------------------------------------
# PROGRAM COMPLETION
# ---------------------------------------------------------------------------

def isSingleProgramDump(messages) -> bool:
    if not hasattr(messages, '__len__'):
        return False

    all_msgs = _split_sysex(messages)
    by_device = {}
    for m in all_msgs:
        if _is_fds(m, 0x01) or _is_fds(m, 0x02):
            dev = _fds_device_id(m)
            by_device.setdefault(dev, []).append(m)

    eof_devices = {m[2] for m in all_msgs if _is_ubxa_eof(m)}
    for device_id, msgs in by_device.items():
        has_header = any(_is_fds(m, 0x01) for m in msgs)
        has_data = any(_is_fds(m, 0x02) for m in msgs)
        if has_header and has_data and device_id in eof_devices:
            return True

    return False

# ---------------------------------------------------------------------------
# RAW extraction 
# ---------------------------------------------------------------------------

def _fds_extract_raw(messages: List[List[int]]) -> List[int]:
    pkts = [m for m in messages if _is_fds(m, 0x02)]
    grouped = {}
    for p in pkts:
        dev = _fds_device_id(p)
        grouped.setdefault(dev, []).append(p)

    best = max(grouped.values(), key=len, default=[])
    best.sort(key=lambda m: _fds_packet_num(m))

    raw_encoded = []
    for pkt in best:
        # Start at _PL + 6 to skip the FDS padding byte. 
        # End at -2 to skip the trailing FDS checksum byte and prevent cascading misalignment!
        payload = pkt[_PL + 6 : -2]  
        raw_encoded.extend(payload)

    return list(unescape_sysex(raw_encoded))

# ---------------------------------------------------------------------------
# Device detection
# ---------------------------------------------------------------------------

def createDeviceDetectMessage(channel: int) -> List[int]:
    filename_bytes = [ord(c) for c in "Globals         "] 
    header = sysex_prefix + [0x7F, 0x74, 0x07, 0x03]
    return header + _BIN_PREFIX + filename_bytes + _FW_VERSION + [sysex_suffix]

def channelIfValidDeviceResponse(message: List[int]) -> int:
    if _is_fds(message, 0x01):
        return 0 
    return -1

# ---------------------------------------------------------------------------
# Storage
# ---------------------------------------------------------------------------

# Provide explicit bank descriptors with msb/lsb so KnobKraft knows how to send
# the proper MIDI CC Bank Select messages before the Program Change!
def bankDescriptors() -> List[dict]:
    return [
        {"bank": 0, "name": "Bank A", "size": 128, "msb": 0, "lsb": 0},
        {"bank": 1, "name": "Bank B", "size": 128, "msb": 0, "lsb": 1},
        {"bank": 2, "name": "Bank C", "size": 128, "msb": 0, "lsb": 2},
        {"bank": 3, "name": "Bank D", "size": 128, "msb": 0, "lsb": 3}
    ]

def friendlyBankName(bank_number: int) -> str:
    return "Bank " + chr(ord('A') + bank_number)
    
def bankSelect(channel, bank):
    # CC#0 (MSB=0) + CC#32 (LSB=bank index 0-3 for A-D)
    return [
        0xB0 | (channel & 0x0f), 0,  0,     # Bank Select MSB = 0
        0xB0 | (channel & 0x0f), 32, bank   # Bank Select LSB = bank
    ]
    
def createCustomProgramChange(channel: int, patchNo: int) -> List[int]:
    filename = _program_filename(patchNo)
    filename_bytes = [ord(c) for c in filename]
    
    header = sysex_prefix + [0x7F, 0x74, 0x07, 0x03]
    return header + _BIN_PREFIX + filename_bytes + _FW_VERSION + [sysex_suffix]
    
def friendlyProgramName(program: int) -> str:
    bank = program // 128
    patch = (program % 128) + 1
    return f"{chr(ord('A') + bank)}{patch:03d}"

# ---------------------------------------------------------------------------
# Dump requests
# ---------------------------------------------------------------------------

def createProgramDumpRequest(channel: int, patchNo: int) -> List[int]:
    filename = _program_filename(patchNo)
    filename_bytes = [ord(c) for c in filename]
    
    header = sysex_prefix + [0x7F, 0x74, 0x07, 0x03]
    return header + _BIN_PREFIX + filename_bytes + _FW_VERSION + [sysex_suffix]


def _program_filename(patch_no: int) -> str:
    if not 0 <= patch_no < 512:
        raise ValueError(f"Invalid UB-Xa patch number: {patch_no}")
    bank_letter = chr(ord('A') + patch_no // 128)
    patch_number = patch_no % 128 + 1
    return f"PatchX {bank_letter}{patch_number:03d}     "

def _with_filename(messages, filename: str) -> List[int]:
    result = []
    for msg in _split_sysex(messages):
        msg = list(msg)
        if _is_fds(msg, 0x01):
            for bin_idx in range(len(msg) - len(_BIN_PREFIX) + 1):
                if msg[bin_idx:bin_idx + len(_BIN_PREFIX)] == _BIN_PREFIX:
                    # FDS header packets store a four-byte file size between
                    # the BIN marker and the fixed-width filename.
                    filename_start = bin_idx + len(_BIN_PREFIX) + 4
                    filename_end = filename_start + _MAX_NAME_CHARS
                    if filename_end <= len(msg) - 1:
                        msg[filename_start:filename_end] = [ord(c) for c in filename]
                    break
        result.extend(msg)
    return result


def _header_filename(messages) -> str:
    for msg in _split_sysex(messages):
        if not _is_fds(msg, 0x01):
            continue
        for bin_idx in range(len(msg) - len(_BIN_PREFIX) + 1):
            if msg[bin_idx:bin_idx + len(_BIN_PREFIX)] == _BIN_PREFIX:
                filename_start = bin_idx + len(_BIN_PREFIX) + 4
                filename_end = filename_start + _MAX_NAME_CHARS
                if filename_end <= len(msg) - 1:
                    return ''.join(chr(value) for value in msg[filename_start:filename_end])
    return ""


def convertToProgramDump(channel: int, messages, patchNo: int) -> List[int]:
    return _with_filename(messages, _program_filename(patchNo))

def createEditBufferRequest(channel: int) -> List[int]:
    filename = "Upper Patch     "
    filename_bytes = [ord(c) for c in filename]
    
    header = sysex_prefix + [0x7F, 0x74, 0x07, 0x03]
    return header + _BIN_PREFIX + filename_bytes + _FW_VERSION + [sysex_suffix]

def _is_ubxa_eof(msg) -> bool:
    return (len(msg) == 6
            and msg[0] == 0xF0
            and msg[1] == 0x7E
            and msg[3] == 0x7B
            and msg[4] == 0x00
            and msg[5] == 0xF7)

# ---------------------------------------------------------------------------
# Dump parsing
# ---------------------------------------------------------------------------

def nameFromDump(messages) -> str:
    all_msgs = _split_sysex(messages)
    try:
        raw = _fds_extract_raw(all_msgs)
        if len(raw) >= 94:
            chars = []
            for i in range(8):
                val = raw[78 + i * 2] | (raw[78 + i * 2 + 1] << 8)
                
                c_second = val & 0x7F        
                c_first = (val >> 7) & 0x7F  
                
                if c_first == 0:
                    break
                chars.append(chr(c_first))
                
                if c_second == 0:
                    break
                chars.append(chr(c_second))
                
            name = ''.join(chars).strip()
            if name:
                return name
    except Exception:
        pass
    return "Unknown"

def numberFromDump(messages) -> int:
    filename = _header_filename(messages)
    if (len(filename) >= 11 and filename.startswith("PatchX ")
            and filename[7] in "ABCD" and filename[8:11].isdigit()):
        patch_number = int(filename[8:11])
        if 1 <= patch_number <= 128:
            return (ord(filename[7]) - ord('A')) * 128 + patch_number - 1
    return -1

def calculateFingerprint(messages) -> str:
    try:
        raw = _fds_extract_raw(_split_sysex(messages))
        if len(raw) < 10:
            return ""
        data = bytearray(raw)
        return hashlib.md5(data).hexdigest()
    except Exception:
        return ""

# ---------------------------------------------------------------------------
# Edit buffer
# ---------------------------------------------------------------------------

def isEditBufferDump(messages) -> bool:
    return isSingleProgramDump(messages)

def isPartOfEditBufferDump(message):
    return isPartOfSingleProgramDump(message)

def convertToEditBuffer(channel: int, messages) -> List[int]:
    return _with_filename(messages, "Upper Patch     ")


def _escape_synthetic_fds(raw_data: List[int]) -> List[int]:
    encoded = []
    for offset in range(0, len(raw_data), 7):
        group = raw_data[offset:offset + 7]
        msbits = 0
        low_bytes = []
        for index, value in enumerate(group):
            if value & 0x80:
                msbits |= 1 << index
            low_bytes.append(value & 0x7F)
        encoded.extend([msbits] + low_bytes)
    return encoded


def make_test_data():
    # Synthetic structure only: the payload deliberately contains no musical
    # patch data and is generated from the documented FDS 7-bit packing.
    device_id = 1
    filename = "PatchX A001     "
    header = (sysex_prefix + [device_id, 0x74, 0x07, 0x01, 0x00]
              + _BIN_PREFIX + [0x00, 0x00, 0x00, 0x60]
              + [ord(char) for char in filename] + [0xF7])
    encoded = _escape_synthetic_fds([0] * 96)
    data = (sysex_prefix + [device_id, 0x74, 0x07, 0x02, 0x01, 0x00]
            + encoded + [0x00, 0xF7])
    eof = [0xF0, 0x7E, device_id, 0x7B, 0x00, 0xF7]
    program = header + data + eof

    def programs(_data: testing.TestData):
        yield testing.ProgramTestData(
            message=program,
            name="Unknown",
            number=0,
            friendly_number="A001",
            target_no=128,
        )

    return testing.TestData(
        program_generator=programs,
        program_dump_request=(0, 0, createProgramDumpRequest(0, 0)),
        device_detect_call=createDeviceDetectMessage(0),
        device_detect_reply=(header, 0),
    )
