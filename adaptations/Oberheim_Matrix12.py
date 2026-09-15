#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from copy import copy
from pathlib import Path
from typing import List

import knobkraft
import testing

MANUFACTURER_ID = 0x10
VOICE_DEVICE_ID = 0x02
MULTI_DEVICE_ID = 0x04
SINGLE_PATCH_CLASS = 0x00
MULTI_PATCH_CLASS = 0x01
REQUEST_SINGLE_PROGRAM = 0x00
DUMP_SINGLE_PROGRAM = 0x01

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

FIXTURE_DIR = Path(__file__).resolve().parent / "testData" / "Oberheim_Xpander"
FACTORY_FIXTURE = FIXTURE_DIR / "rubidium_FACTORY2_M12.syx"
MIXED_FIXTURE = FIXTURE_DIR / "presetpatch_Matrix_12.syx"


def name():
    return "Oberheim Matrix-12"


def setupHelp():
    return (
        "Enable SYSTEMX (System Exclusive) on the Matrix-12 MIDI page before using this adaptation. "
        "Automatic detection requests single voice program 00. Automatic download currently iterates the 100 voice "
        "programs one by one. Matrix-12 multi patches and card-specific behavior are not implemented yet. "
        "Send-to-synth uses program 99 as a scratch slot and selects it after transfer to simulate an edit-buffer write."
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


def friendlyBankName(bank):
    return "Internal"


def defaultProgramPlace():
    return PATCH_COUNT - 1


def createProgramDumpRequest(channel, patchNo):
    return [0xF0, MANUFACTURER_ID, VOICE_DEVICE_ID, REQUEST_SINGLE_PROGRAM, SINGLE_PATCH_CLASS, patchNo % PATCH_COUNT, 0xF7]


def _is_voice_program_message(message):
    return (
        len(message) == PROGRAM_LENGTH
        and message[0] == 0xF0
        and message[1] == MANUFACTURER_ID
        and message[2] == VOICE_DEVICE_ID
        and message[3] == DUMP_SINGLE_PROGRAM
        and message[4] == SINGLE_PATCH_CLASS
        and message[-1] == 0xF7
    )


def _is_multi_patch_message(message):
    return (
        len(message) >= 7
        and message[0] == 0xF0
        and message[1] == MANUFACTURER_ID
        and message[2] == MULTI_DEVICE_ID
        and message[3] == DUMP_SINGLE_PROGRAM
        and message[4] == MULTI_PATCH_CLASS
        and message[-1] == 0xF7
    )


def isSingleProgramDump(message):
    return _is_voice_program_message(message)


def numberFromDump(message):
    if _is_voice_program_message(message):
        return message[5]
    raise Exception("Can only extract program numbers from Matrix-12 voice program dumps")


def nameFromDump(message):
    if _is_voice_program_message(message):
        raw_name = _drop_odd_bytes(message[RAW_NAME_START:RAW_NAME_END])
        patch_name = _name_array_to_string(raw_name)
        return patch_name if patch_name else "no name"
    raise Exception("Can only extract names from Matrix-12 voice program dumps")


def renamePatch(message, new_name):
    if not _is_voice_program_message(message):
        raise Exception("Can only rename Matrix-12 voice program dumps")
    renamed = copy(message)
    renamed[RAW_NAME_START:RAW_NAME_END] = _add_odd_bytes(_create_padded_name_bytes(new_name))
    return renamed


def convertToProgramDump(channel, message, patchNo):
    if _is_voice_program_message(message):
        converted = copy(message)
        converted[2] = VOICE_DEVICE_ID
        converted[3] = DUMP_SINGLE_PROGRAM
        converted[4] = SINGLE_PATCH_CLASS
        converted[5] = patchNo % PATCH_COUNT
        return converted
    raise Exception("Can only convert Matrix-12 voice program dumps")


def generalMessageDelay():
    return TRANSFER_DELAY_MS


def blankedOut(message):
    if not _is_voice_program_message(message):
        raise Exception("Can only blank out Matrix-12 voice program dumps")
    blanked = copy(message)
    blanked[5] = 0x00
    for index in range(RAW_NAME_START, RAW_NAME_END):
        blanked[index] = 0x00
    return blanked


def calculateFingerprint(message):
    if _is_voice_program_message(message):
        return hashlib.md5(bytearray(blankedOut(message))).hexdigest()
    raise Exception("Can only fingerprint Matrix-12 voice program dumps")


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


def _voice_programs_from_fixture(path: Path) -> List[List[int]]:
    return [message for message in knobkraft.load_sysex(str(path)) if _is_voice_program_message(message)]


def make_test_data():
    from testing.mock_midi import ProgramDumpMockDevice

    voice_programs = _voice_programs_from_fixture(FACTORY_FIXTURE)

    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        for index in [0, 1, 99]:
            message = voice_programs[index]
            yield testing.ProgramTestData(
                message=message,
                name=nameFromDump(message),
                number=numberFromDump(message),
                rename_name="M12TEST",
                target_no=11,
            )

    return testing.TestData(
        sysex=str(FACTORY_FIXTURE),
        program_generator=programs,
        program_dump_request=(0, 0, [0xF0, MANUFACTURER_ID, VOICE_DEVICE_ID, REQUEST_SINGLE_PROGRAM, SINGLE_PATCH_CLASS, 0x00, 0xF7]),
        device_detect_call=[0xF0, MANUFACTURER_ID, VOICE_DEVICE_ID, REQUEST_SINGLE_PROGRAM, SINGLE_PATCH_CLASS, 0x00, 0xF7],
        device_detect_reply=(convertToProgramDump(0, voice_programs[0], 0), 1),
        expected_patch_count=100,
        friendly_bank_name=(0, "Internal"),
        mock_device_factory=lambda _test_data, adaptation: ProgramDumpMockDevice(adaptation, voice_programs),
        expected_wire_patch_count=PATCH_COUNT,
        expected_sent_messages=lambda _test_data, adaptation: [
            adaptation.createProgramDumpRequest(0, patch_no) for patch_no in range(adaptation.PATCH_COUNT)
        ],
        wire_download_banks=[0, 0],
        expected_multi_bank_patch_count=PATCH_COUNT * 2,
        expected_multi_bank_sent_messages=lambda _test_data, adaptation: [
            adaptation.createProgramDumpRequest(0, patch_no) for _bank in range(2) for patch_no in range(adaptation.PATCH_COUNT)
        ],
        send_to_synth_patch=lambda test_data: test_data.programs[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: [
            adaptation.convertToProgramDump(0, test_data.programs[0].message.byte_list, adaptation.defaultProgramPlace()),
            [0xC0, adaptation.defaultProgramPlace()],
        ],
    )
