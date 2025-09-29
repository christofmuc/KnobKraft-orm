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

    max_chars = min(_MAX_NAME_CHARS, len(decoded) - name_start - 1)
    write_length = min(len(sanitized), max_chars)

    for offset in range(max_chars):
        if offset < write_length:
            decoded[name_start + offset] = sanitized[offset]
        else:
            decoded[name_start + offset] = 0x00

    terminator_index = name_start + max_chars
    if terminator_index < len(decoded):
        decoded[terminator_index] = 0x00

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


def nameFromDump(message: List[int]) -> str:
    if not isSingleProgramDump(message):
        return "invalid"

    payload = message[len(sysex_prefix) + 3:-1]
    decoded = unescape_sysex(payload)
    if len(decoded) <= 150:
        return "invalid"

    name_bytes = []
    for value in decoded[150:]:
        if value == 0x00:
            break
        name_bytes.append(value)

    if not name_bytes:
        return "Unnamed"

    try:
        return ''.join(chr(b) for b in name_bytes)
    except ValueError:
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

