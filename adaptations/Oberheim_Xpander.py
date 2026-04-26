#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from copy import copy
from typing import List
import knobkraft
import testing
PROGRAM_LENGTH = 399
RAW_DATA_START = 6
RAW_DATA_END = PROGRAM_LENGTH - 1
RAW_NAME_START = 382
RAW_NAME_END = 398
PATCH_NAME_START = 188
PATCH_NAME_END = 196
PATCH_NAME_LENGTH = PATCH_NAME_END - PATCH_NAME_START
PATCH_COUNT = 100
TRANSFER_DELAY_MS = 150
SIX_BIT_CHARSET = [
    "@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_",
    " ", "!", '"', "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
]
SAMPLE_PROGRAM = knobkraft.stringToSyx(
    "f01002010009120000001f003f0001000400000000001f003f00010000000d00000001000000000001003f00000000000a0000000a00000001000400000014003f003000000000000200000013003f003000000000000000000000003f003000000000000000000000003f003000000000000000000000003f00210100001f0024003400000014003f002501000000000000220000001e003f0021010000000000000000000000003f00010100000000000000003f0000003f00010100000000000000003f0000003f00000000000f001f002f003f00000000000f001f002f003f00000000000f001f002f003f001f00000000001f00000000001f00000000001f00000000000c00340006000d003f00090013003f00000013003f000000130033000c0014003f00080000000f000a000e003f0007000c00360007000c0013000c001f0000003f001f0000003f001f0000003f001f0000003f001f0000003f001f0000003f001f0000003f001f0000003f001f0000003f001f0000003f00570045004900520044004c0046004f00f7"
)
def name():
    return "Oberheim Xpander"
def setupHelp():
    return (
        "The Xpander adaptation currently works from single program dumps. "
        "Automatic detection requests program 00, and bank downloads request the 100 single programs one by one."
    )
def createDeviceDetectMessage(channel):
    return createProgramDumpRequest(channel, 0)
def needsChannelSpecificDetection():
    return False
def deviceDetectWaitMilliseconds():
    return 1000
def channelIfValidDeviceResponse(message):
    if isSingleProgramDump(message) and numberFromDump(message) == 0:
        return 1
    return -1
def numberOfBanks():
    return 1
def numberOfPatchesPerBank():
    return PATCH_COUNT
def createProgramDumpRequest(channel, patchNo):
    return [0xF0, 0x10, 0x02, 0x01, 0x00, patchNo % PATCH_COUNT, 0xF7]
def isSingleProgramDump(message):
    return (
        len(message) == PROGRAM_LENGTH
        and message[0] == 0xF0
        and message[1] == 0x10
        and message[2] == 0x02
        and message[3] == 0x01
        and message[4] == 0x00
        and message[-1] == 0xF7
    )
def numberFromDump(message):
    if isSingleProgramDump(message):
        return message[5]
    raise Exception("Can only extract program numbers from Xpander program dumps")
def nameFromDump(message):
    if isSingleProgramDump(message):
        raw_name = _drop_odd_bytes(message[RAW_NAME_START:RAW_NAME_END])
        patch_name = _name_array_to_string(raw_name)
        return patch_name if patch_name else "no name"
    raise Exception("Can only extract names from Xpander program dumps")
def renamePatch(message, new_name):
    if not isSingleProgramDump(message):
        raise Exception("Can only rename Xpander program dumps")
    renamed = copy(message)
    renamed[RAW_NAME_START:RAW_NAME_END] = _add_odd_bytes(_create_padded_name_bytes(new_name))
    return renamed
def convertToProgramDump(channel, message, patchNo):
    if isSingleProgramDump(message):
        converted = copy(message)
        converted[3] = 0x01
        converted[4] = 0x00
        converted[5] = patchNo % PATCH_COUNT
        return converted
    raise Exception("Can only convert Xpander program dumps")
def generalMessageDelay():
    return TRANSFER_DELAY_MS
def blankedOut(message):
    if not isSingleProgramDump(message):
        raise Exception("Can only blank out Xpander program dumps")
    blanked = copy(message)
    blanked[5] = 0x00
    for index in range(RAW_NAME_START, RAW_NAME_END):
        blanked[index] = 0x00
    return blanked
def calculateFingerprint(message):
    if isSingleProgramDump(message):
        return hashlib.md5(bytearray(blankedOut(message))).hexdigest()
    raise Exception("Can only fingerprint Xpander program dumps")
def _drop_odd_bytes(message):
    return [message[index] for index in range(0, len(message), 2)]
def _add_odd_bytes(message):
    result = []
    for byte in message:
        result.append(byte)
        result.append(0x00)
    return result
def _name_array_to_string(name_array):
    chars = []
    for value in name_array:
        chars.append(_map_oberheim_hex_to_ascii(value & 0x3F))
    return "".join(chars).strip()
def _map_oberheim_hex_to_ascii(value):
    if 0 <= value < len(SIX_BIT_CHARSET):
        return SIX_BIT_CHARSET[value]
    return " "
def _create_padded_name_bytes(input_name):
    upper_name = input_name.upper()
    sanitized = []
    for char in upper_name:
        if char in SIX_BIT_CHARSET:
            sanitized.append(ord(char))
        else:
            sanitized.append(ord(" "))
    sanitized = sanitized[:PATCH_NAME_LENGTH]
    while len(sanitized) < PATCH_NAME_LENGTH:
        sanitized.append(ord(" "))
    return sanitized
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(
            message=SAMPLE_PROGRAM,
            name="WEIRDLFO",
            number=9,
            rename_name="WEIRDLFP",
            target_no=11,
        )
    return testing.TestData(
        program_generator=programs,
        program_dump_request=(0, 0, [0xF0, 0x10, 0x02, 0x01, 0x00, 0x00, 0xF7]),
        device_detect_call=[0xF0, 0x10, 0x02, 0x01, 0x00, 0x00, 0xF7],
        device_detect_reply=(renamePatch(convertToProgramDump(0, SAMPLE_PROGRAM, 0), "WEIRDLFO"), 1),
        expected_patch_count=1,
    )
