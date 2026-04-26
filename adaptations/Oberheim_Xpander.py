#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from copy import copy
from typing import Dict, List

import knobkraft
import testing

MANUFACTURER_ID = 0x10
DEVICE_ID = 0x02
SINGLE_PATCH_CLASS = 0x00
REQUEST_SINGLE_PROGRAM = 0x00
DUMP_SINGLE_PROGRAM = 0x01
REQUEST_ALL_DATA = 0x02

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

# Workaround for a framework ambiguity: an Xpander all-data dump is a stream of normal
# single-program dumps. Current KnobKraft parsing can load those once as program dumps
# and again as a bank dump, so this flag suppresses program-dump detection while a bank
# request is in flight. See BUG_bank_dump_program_dump_double_parse.md in the repo root.
_expect_bank_dump = False


def name():
    return "Oberheim Xpander"


def setupHelp():
    return (
        "Enable SYSTEMX (System Exclusive) on the Xpander MIDI page before using this adaptation. "
        "Automatic detection requests single program 00. Send-to-synth uses program 99 as a scratch slot and selects "
        "it after transfer to simulate an edit-buffer write. The adaptation also understands the documented all-data "
        "dump format for file import and live bank download."
    )


def createDeviceDetectMessage(channel):
    return createProgramDumpRequest(channel, 0)


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    return 1000


def channelIfValidDeviceResponse(message):
    if _is_single_program_message(message) and numberFromDump(message) == 0:
        return 1
    return -1


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return PATCH_COUNT


def friendlyBankName(bank):
    return "Internal"


def defaultProgramPlace():
    return PATCH_COUNT - 1


def createProgramDumpRequest(channel, patchNo):
    global _expect_bank_dump
    # Workaround for a framework ambiguity: an Xpander all-data dump is a stream of normal
# single-program dumps. Current KnobKraft parsing can load those once as program dumps
# and again as a bank dump, so this flag suppresses program-dump detection while a bank
# request is in flight. See BUG_bank_dump_program_dump_double_parse.md in the repo root.
_expect_bank_dump = False
    return [0xF0, MANUFACTURER_ID, DEVICE_ID, REQUEST_SINGLE_PROGRAM, SINGLE_PATCH_CLASS, patchNo % PATCH_COUNT, 0xF7]


def createBankDumpRequest(channel, bank):
    global _expect_bank_dump
    _expect_bank_dump = True
    return [0xF0, MANUFACTURER_ID, DEVICE_ID, REQUEST_ALL_DATA, SINGLE_PATCH_CLASS, 0xF7]


def _is_single_program_message(message):
    return (
        len(message) == PROGRAM_LENGTH
        and message[0] == 0xF0
        and message[1] == MANUFACTURER_ID
        and message[2] == DEVICE_ID
        and message[3] == DUMP_SINGLE_PROGRAM
        and message[4] == SINGLE_PATCH_CLASS
        and message[-1] == 0xF7
    )


def isSingleProgramDump(message):
    return _is_single_program_message(message) and not _expect_bank_dump


def isPartOfBankDump(message):
    if message and isinstance(message[0], list):
        return any(isPartOfBankDump(part) for part in message)
    return _is_single_program_message(message)


def isBankDumpFinished(messages):
    return set(_collect_bank_programs(messages).keys()) == set(range(PATCH_COUNT))


def extractPatchesFromBank(bank):
    flattened = []
    for patch in extractPatchesFromAllBankMessages(bank):
        flattened.extend(patch)
    return flattened


def extractPatchesFromAllBankMessages(messages):
    global _expect_bank_dump
    programs = _collect_bank_programs(messages)
    # Workaround for a framework ambiguity: an Xpander all-data dump is a stream of normal
# single-program dumps. Current KnobKraft parsing can load those once as program dumps
# and again as a bank dump, so this flag suppresses program-dump detection while a bank
# request is in flight. See BUG_bank_dump_program_dump_double_parse.md in the repo root.
_expect_bank_dump = False
    return [programs[number] for number in sorted(programs.keys())]


def numberFromDump(message):
    if _is_single_program_message(message):
        return message[5]
    raise Exception("Can only extract program numbers from Xpander program dumps")


def nameFromDump(message):
    if _is_single_program_message(message):
        raw_name = _drop_odd_bytes(message[RAW_NAME_START:RAW_NAME_END])
        patch_name = _name_array_to_string(raw_name)
        return patch_name if patch_name else "no name"
    raise Exception("Can only extract names from Xpander program dumps")


def renamePatch(message, new_name):
    if not _is_single_program_message(message):
        raise Exception("Can only rename Xpander program dumps")
    renamed = copy(message)
    renamed[RAW_NAME_START:RAW_NAME_END] = _add_odd_bytes(_create_padded_name_bytes(new_name))
    return renamed


def convertToProgramDump(channel, message, patchNo):
    if _is_single_program_message(message):
        converted = copy(message)
        converted[3] = DUMP_SINGLE_PROGRAM
        converted[4] = SINGLE_PATCH_CLASS
        converted[5] = patchNo % PATCH_COUNT
        return converted
    raise Exception("Can only convert Xpander program dumps")


def generalMessageDelay():
    return TRANSFER_DELAY_MS


def blankedOut(message):
    if not _is_single_program_message(message):
        raise Exception("Can only blank out Xpander program dumps")
    blanked = copy(message)
    blanked[5] = 0x00
    for index in range(RAW_NAME_START, RAW_NAME_END):
        blanked[index] = 0x00
    return blanked


def calculateFingerprint(message):
    if _is_single_program_message(message):
        return hashlib.md5(bytearray(blankedOut(message))).hexdigest()
    raise Exception("Can only fingerprint Xpander program dumps")


def _collect_bank_programs(messages) -> Dict[int, List[int]]:
    if not messages:
        return {}
    if isinstance(messages[0], int):
        messages = [messages]

    programs: Dict[int, List[int]] = {}
    for message in messages:
        if _is_single_program_message(message):
            programs[numberFromDump(message)] = copy(message)
    return programs


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


def _xpander_bank_messages():
    return [convertToProgramDump(0, SAMPLE_PROGRAM, patch_no) for patch_no in range(PATCH_COUNT)]


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(
            message=SAMPLE_PROGRAM,
            name="WEIRDLFO",
            number=9,
            rename_name="WEIRDLFP",
            target_no=11,
        )

    def banks(data: testing.TestData):
        yield _xpander_bank_messages()

    return testing.TestData(
        program_generator=programs,
        bank_generator=banks,
        program_dump_request=(0, 0, [0xF0, MANUFACTURER_ID, DEVICE_ID, REQUEST_SINGLE_PROGRAM, SINGLE_PATCH_CLASS, 0x00, 0xF7]),
        device_detect_call=[0xF0, MANUFACTURER_ID, DEVICE_ID, REQUEST_SINGLE_PROGRAM, SINGLE_PATCH_CLASS, 0x00, 0xF7],
        device_detect_reply=(renamePatch(convertToProgramDump(0, SAMPLE_PROGRAM, 0), "WEIRDLFO"), 1),
        expected_patch_count=1,
        friendly_bank_name=(0, "Internal"),
        mock_device_factory=lambda _test_data, adaptation: __import__("testing.mock_midi", fromlist=["BankDumpMockDevice"]).BankDumpMockDevice(adaptation, _xpander_bank_messages()),
        expected_wire_patch_count=PATCH_COUNT,
        expected_sent_messages=lambda _test_data, adaptation: [[0xF0, MANUFACTURER_ID, DEVICE_ID, REQUEST_ALL_DATA, SINGLE_PATCH_CLASS, 0xF7]],
        wire_download_banks=[0, 0],
        expected_multi_bank_patch_count=PATCH_COUNT * 2,
        expected_multi_bank_sent_messages=lambda _test_data, adaptation: [
            [0xF0, MANUFACTURER_ID, DEVICE_ID, REQUEST_ALL_DATA, SINGLE_PATCH_CLASS, 0xF7],
            [0xF0, MANUFACTURER_ID, DEVICE_ID, REQUEST_ALL_DATA, SINGLE_PATCH_CLASS, 0xF7],
        ],
        send_to_synth_patch=lambda test_data: test_data.programs[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: [
            adaptation.convertToProgramDump(0, test_data.programs[0].message.byte_list, adaptation.defaultProgramPlace()),
            [0xC0, adaptation.defaultProgramPlace()],
        ],
    )


