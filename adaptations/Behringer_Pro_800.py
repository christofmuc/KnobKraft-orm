#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

from typing import List

from knobkraft import unescapeSysex_deepmind as unescape_sysex
import testing

_MAX_NAME_CHARS = 15

behringer_id = [0x00, 0x20, 0x32]
pro800_product = [0x00, 0x01, 0x24, 0x00]
sysex_prefix = [0xf0] + behringer_id + pro800_product
sysex_suffix = 0xf7

_emit_lfo_mask_cc = True


def name() -> str:
    return "Behringer Pro-800"


def createDeviceDetectMessage(channel: int) -> List[int]:
    # Use the documented version request to verify the device responds with a Pro-800 specific header.
    return sysex_prefix + [0x08, 0x00, sysex_suffix]


def needsChannelSpecificDetection() -> bool:
    return False


def channelIfValidDeviceResponse(message: List[int]) -> int:
    if (len(message) >= 12
            and message[0:len(sysex_prefix)] == sysex_prefix
            and message[len(sysex_prefix)] == 0x09
            and message[len(sysex_prefix) + 1] == 0x00):
        return 0
    return -1

def setEmitLfoMaskCc(enable: bool) -> None:
    global _emit_lfo_mask_cc
    _emit_lfo_mask_cc = bool(enable)


def numberOfBanks() -> int:
    return 4


def numberOfPatchesPerBank() -> int:
    return 100


def createProgramDumpRequest(channel: int, patchNo: int) -> List[int]:
    program_lsb = patchNo & 0x7f
    program_msb = (patchNo >> 7) & 0x7f
    return sysex_prefix + [0x77, program_lsb, program_msb, sysex_suffix]


def isSingleProgramDump(message: List[int]) -> bool:
    return (len(message) > len(sysex_prefix) + 3
            and message[0:len(sysex_prefix)] == sysex_prefix
            and message[len(sysex_prefix)] == 0x78)


def renamePatch(message: List[int], new_name: str) -> List[int]:
    if not isSingleProgramDump(message):
        raise Exception("Can only rename Pro-800 single program dumps")

    payload = message[len(sysex_prefix) + 3:-1]
    decoded = list(unescape_sysex(payload))

    name_start = 150
    if len(decoded) <= name_start + 1:
        raise Exception("Program dump too short to contain a name")

    sanitized = _sanitize_name(new_name)

    version = decoded[4] if len(decoded) > 4 else 0

    if version >= 0x6F:
        # Fixed 16-byte name field at offsets 150..165, unused bytes are 0.
        fixed_len = min(16, len(decoded) - name_start)
        for i in range(fixed_len):
            decoded[name_start + i] = sanitized[i] if i < len(sanitized) else 0x00
        # Do NOT write a terminator beyond the fixed 16 bytes; offset 166 is used by other params.
    else:
        # Variable-length name terminated by 0x00. Keep current span to avoid shifting data.
        # Find existing terminator (if any)
        idx = name_start
        while idx < len(decoded) and decoded[idx] != 0x00:
            idx += 1
        span_end = idx  # points at terminator or end
        if span_end <= name_start:
            # No characters currently; define a minimal span within bounds
            span_end = min(name_start + _MAX_NAME_CHARS, len(decoded))

        span_len = max(0, span_end - name_start)
        write_len = min(len(sanitized), span_len)
        # Write within existing span and pad remaining span with 0
        for i in range(span_len):
            if i < write_len:
                decoded[name_start + i] = sanitized[i]
            else:
                decoded[name_start + i] = 0x00
        # Keep the existing terminator intact if present

    new_payload = _escape_sysex(decoded)
    if len(new_payload) != len(payload):
        raise Exception("Re-encoding Pro-800 program dump changed payload length")

    renamed = list(message)
    renamed[len(sysex_prefix) + 3:-1] = new_payload
    return renamed


def numberFromDump(message: List[int]) -> int:
    if isSingleProgramDump(message):
        program_lsb = message[len(sysex_prefix) + 1]
        program_msb = message[len(sysex_prefix) + 2]
        return program_lsb | (program_msb << 7)
    return -1


def convertToProgramDump(channel: int, message: List[int], program_number: int) -> List[int]:
    if isSingleProgramDump(message):
        program_lsb = program_number & 0x7f
        program_msb = (program_number >> 7) & 0x7f
        converted = list(message)
        converted[len(sysex_prefix) + 1] = program_lsb
        converted[len(sysex_prefix) + 2] = program_msb
        return converted
    raise Exception("Can only convert Pro-800 single program dumps")


def convertToProgramDump_cc(channel: int, message: List[int], program_number: int) -> List[int]:
    # New behavior: instead of re-encoding the sysex with a different program number,
    # produce a stream of MIDI CC messages that set all supported parameters to the
    # values contained in the given program dump.
    # Using https://midi.guide/d/behringer/pro-800/
    if not isSingleProgramDump(message):
        raise Exception("Can only convert Pro-800 single program dumps")

    # Extract and decode payload (7-bit -> 8-bit), then map parameters to CCs.
    payload = message[len(sysex_prefix) + 3:-1]
    decoded = list(unescape_sysex(payload))

    def clamp7(v: int) -> int:
        if v < 0:
            return 0
        if v > 127:
            return 127
        return v

    def read_le16(offset: int) -> int:
        if offset + 1 >= len(decoded):
            return 0
        return (decoded[offset] & 0xff) | ((decoded[offset + 1] & 0xff) << 8)

    def scale_to_cc7(v16: int) -> int:
        # Scale 0..65535 to 0..127 with rounding
        if v16 <= 0:
            return 0
        if v16 >= 0xFFFF:
            return 127
        return (v16 * 127 + 32767) // 65535

    status = 0xB0 | (channel & 0x0F)
    out: List[int] = []

    def emit_cc(cc: int, value: int):
        out.extend([status, cc & 0x7F, clamp7(value)])

    # Version-specific handling (name encoding and LFO Aftertouch location)
    version = decoded[4] if len(decoded) > 4 else 0

    # Continuous parameters (2 bytes little-endian) -> scale to CC 0..127
    two_byte_params = [
        (5, 80),   # Freq A -> Osca Freq
        (7, 81),   # Vol A  -> Osca VOL
        (9, 82),   # PWA    -> Oscapw
        (11, 83),  # Freq B -> Oscb Freq
        (13, 84),  # Vol B  -> Oscb VOL
        (15, 85),  # PWB    -> Oscbpw
        (17, 86),  # Fine B -> Oscb Fine
        (19, 87),  # Cutoff -> VCF Freq
        (21, 88),  # Res    -> VCF Reso
        (23, 89),  # Filt Env amount -> Vcfen Vamount
        (25, 90),  # FE R   -> VCF REL
        (27, 91),  # FE S   -> VCF SUS
        (29, 92),  # FE D   -> VCF DEC
        (31, 93),  # FE A   -> VCF Atck
        (33, 94),  # AE R   -> VCA REL
        (35, 95),  # AE S   -> VCA SUS
        (37, 100), # AE D   -> VCA DEC
        (39, 101), # AE A   -> VCA Atck
        (41, 102), # PM Env -> Pmod Filter Envamount
        (43, 103), # PM OscB -> Pmod OSC Bamount
        (45, 104), # LFO Freq
        (47, 105), # LFO Amt
        (49, 106), # Glide
        (51, 107), # Amp Vel -> VCA VEL
        (53, 108), # Filt Vel -> VCF VEL
        (76, 109), # Mod delay
        (78, 110), # Vibrato speed
        (80, 111), # Vibrato amount
        (82, 112), # Detune (Unison Detune)
        (142, 113),# Noise
        (144, 114),# VCA Aftertouch amount
        (146, 115),# VCF Aftertouch amount
    ]

    for offset, cc in two_byte_params:
        if offset + 1 < len(decoded):
            emit_cc(cc, scale_to_cc7(read_le16(offset)))

    # LFO Aftertouch amount depends on version and name encoding
    # In 0x6E: name is zero-terminated starting at 0x96, LFO AT follows name.
    # In 0x6F and newer: name is fixed 16 bytes, LFO AT at 0xA6 (166).
    if version >= 0x6F:
        lfo_at_offset = 166
        if lfo_at_offset + 1 < len(decoded):
            emit_cc(116, scale_to_cc7(read_le16(lfo_at_offset)))
    else:
        name_start = 0x96
        if name_start < len(decoded):
            idx = name_start
            while idx < len(decoded) and decoded[idx] != 0x00:
                idx += 1
            lfo_at_offset = idx + 1
            if lfo_at_offset + 1 < len(decoded):
                emit_cc(116, scale_to_cc7(read_le16(lfo_at_offset)))

    # Boolean and stepped parameters (1 byte)
    def emit_bool(offset: int, cc: int):
        if offset < len(decoded):
            emit_cc(cc, 127 if decoded[offset] != 0 else 0)

    # Oscillator wave toggles and sync
    emit_bool(55, 49)  # Saw A -> Osca SAW
    emit_bool(57, 50)  # Sqr A -> Osca Square
    emit_bool(58, 51)  # Saw B -> Oscb SAW
    emit_bool(59, 52)  # Tri B -> Osbb TRI
    emit_bool(60, 53)  # Sqr B -> Osbb Square
    emit_bool(61, 54)  # Sync   -> Oscb Sync

    # Poly Mod toggles
    emit_bool(62, 55)  # PM Freq   -> Pmod Freq A
    emit_bool(63, 56)  # PM Filt   -> Pmod VCF

    # Enumerations: scale index to 0..127
    def emit_enum(offset: int, cc: int, steps: int):
        if offset < len(decoded) and steps > 1:
            idx = decoded[offset] & 0x7F
            if idx < 0:
                idx = 0
            if idx > steps - 1:
                idx = steps - 1
            emit_cc(cc, (idx * 127) // (steps - 1))

    emit_enum(64, 57, 6)  # LFO Shape
    emit_enum(65, 58, 2)  # LFO Range/Speed

    # LFO Target bitmask; send raw mask (0..63) and mirror to individual destination CCs
    if 66 < len(decoded):
        mask = decoded[66] & 0x7F
        if _emit_lfo_mask_cc:
            emit_cc(59, clamp7(mask))
        # Derive LFO destination CCs from mask bits
        emit_cc(74, 127 if (mask & 0x01) else 0)  # LFO Dest Freq
        emit_cc(75, 127 if (mask & 0x02) else 0)  # LFO Dest Filter
        emit_cc(76, 127 if (mask & 0x04) else 0)  # LFO Dest PWM

    emit_enum(67, 60, 3)  # KeyTrk Off/Half/Full
    emit_enum(68, 61, 2)  # FE Shape Lin/Exp
    emit_enum(69, 62, 2)  # FE Speed Fast/Slow
    emit_enum(70, 63, 2)  # AE Shape Lin/Exp

    # Unison On/Off
    emit_bool(71, 65)

    emit_enum(72, 66, 4)  # Pitchbend target Off,VCO,VCF,Vol
    emit_enum(73, 67, 4)  # Modwheel amt Min,Low,High,Full
    emit_enum(74, 68, 4)  # OscA freq mode Free,Semi,Oct,FiHD
    emit_enum(75, 69, 4)  # OscB freq mode Free,Semi,Oct,FiHD
    emit_enum(84, 70, 2)  # Modwheel target LFO,Vib
    emit_enum(148, 72, 2) # AE Speed Fast/Slow

    return out


def nameFromDump(message: List[int]) -> str:
    if not isSingleProgramDump(message):
        return "invalid"

    payload = message[len(sysex_prefix) + 3:-1]
    decoded = unescape_sysex(payload)
    if len(decoded) <= 150:
        return "invalid"

    version = decoded[4] if len(decoded) > 4 else 0

    try:
        if version >= 0x6F:
            # Read up to the first 0x00 within the fixed 16-byte window
            window_end = min(150 + 16, len(decoded))
            name_bytes: List[int] = []
            for value in decoded[150:window_end]:
                if value == 0x00:
                    break
                name_bytes.append(value)
            return ''.join(chr(b) for b in name_bytes) if name_bytes else "Unnamed"
        else:
            # Read until first 0x00 (variable-length name)
            name_bytes: List[int] = []
            for value in decoded[150:]:
                if value == 0x00:
                    break
                name_bytes.append(value)
            return ''.join(chr(b) for b in name_bytes) if name_bytes else "Unnamed"
    except Exception:
        return "Unnamed"

def _sanitize_name(name: str) -> List[int]:
    sanitized = []
    for char in name:
        code = ord(char) & 0x7f
        if code < 0x20:
            code = 0x20
        sanitized.append(code)
    return sanitized


def _escape_sysex(decoded: List[int]) -> List[int]:
    encoded: List[int] = []
    for index in range(0, len(decoded), 7):
        chunk = decoded[index:index + 7]
        msbits = 0
        data_bytes = []
        for bit_index, value in enumerate(chunk):
            if value & 0x80:
                msbits |= 1 << bit_index
            data_bytes.append(value & 0x7f)
        encoded.append(msbits)
        encoded.extend(data_bytes)
    return encoded

# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], name="Organ I", number=0)
        yield testing.ProgramTestData(message=data.all_messages[99], name="Alien", number=99)

    return testing.TestData(sysex="testData/Behringer_Pro_800/PRO-800_Presets_v1.4.4.syx", program_generator=programs, expected_patch_count=100)
