#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib
from pathlib import Path
from typing import List, Tuple

import knobkraft
import testing


MANUFACTURER_ID = 0x33
MODEL_ID_CLASSIC = 0x04

PATCH_DATA_SIZE = 132
PATCH_MESSAGE_SIZE = 6 + PATCH_DATA_SIZE + 1

CLASSIC_BANK_COUNT = 4
PATCHES_PER_BANK = 99
MAX_IMPORT_BANKS = 10
EDIT_BUFFER_SLOTS = 4

EDIT_BUFFER_DUMP_TYPE = 0x00
EDIT_BUFFER_REQUEST_TYPE = 0x0A
FIRST_PROGRAM_DUMP_TYPE = 0x01
FIRST_PROGRAM_REQUEST_TYPE = 0x0B


def name():
    return "Clavia Nord Lead"


def setupHelp():
    return (
        "This adaptation targets the classic Nord Lead SysEx family used by the Nord Lead 1, Nord Lead 2, "
        "and Nord Lead 2X program dumps. Patch dumps do not contain patch names in the documented 66-parameter "
        "payload, so KnobKraft shows location-based names only. Nord Lead 3 and later Nord Lead models use a "
        "different SysEx format and are not covered by this adaptation."
    )


def createDeviceDetectMessage(channel):
    return createEditBufferRequest(channel)


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    return 200


def channelIfValidDeviceResponse(message):
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return message[2] & 0x0F
    return -1


def numberOfBanks():
    return CLASSIC_BANK_COUNT


def numberOfPatchesPerBank():
    return PATCHES_PER_BANK


def friendlyBankName(bank):
    if 0 <= bank < MAX_IMPORT_BANKS:
        return f"Bank {bank}"
    return f"Bank {bank + 1}"


def friendlyProgramName(program):
    if program < 0:
        return "Slot A"
    bank = program // PATCHES_PER_BANK
    patch = program % PATCHES_PER_BANK
    return f"{bank}.{patch + 1:02d}"


def createEditBufferRequest(channel):
    return [0xF0, MANUFACTURER_ID, channel & 0x0F, MODEL_ID_CLASSIC, EDIT_BUFFER_REQUEST_TYPE, 0x00, 0xF7]


def createProgramDumpRequest(channel, patch_no):
    bank, slot = divmod(patch_no, PATCHES_PER_BANK)
    if not (0 <= bank < MAX_IMPORT_BANKS):
        raise Exception(f"Program {patch_no} is out of range for {name()}")
    return [0xF0, MANUFACTURER_ID, channel & 0x0F, MODEL_ID_CLASSIC, FIRST_PROGRAM_REQUEST_TYPE + bank, slot, 0xF7]


def isEditBufferDump(message):
    return (
        len(message) == PATCH_MESSAGE_SIZE
        and _has_classic_header(message)
        and message[4] == EDIT_BUFFER_DUMP_TYPE
        and 0 <= message[5] < EDIT_BUFFER_SLOTS
    )


def isSingleProgramDump(message):
    return (
        len(message) == PATCH_MESSAGE_SIZE
        and _has_classic_header(message)
        and FIRST_PROGRAM_DUMP_TYPE <= message[4] < FIRST_PROGRAM_DUMP_TYPE + MAX_IMPORT_BANKS
        and 0 <= message[5] < PATCHES_PER_BANK
    )


def convertToEditBuffer(channel, message):
    if not (isEditBufferDump(message) or isSingleProgramDump(message)):
        raise Exception("Can only convert classic Nord Lead program or edit buffer dumps")
    return [0xF0, MANUFACTURER_ID, channel & 0x0F, MODEL_ID_CLASSIC, EDIT_BUFFER_DUMP_TYPE, 0x00] + _payload_from_patch_message(message) + [0xF7]


def convertToProgramDump(channel, message, program_number):
    if not (isEditBufferDump(message) or isSingleProgramDump(message)):
        raise Exception("Can only convert classic Nord Lead program or edit buffer dumps")
    bank, slot = divmod(program_number, PATCHES_PER_BANK)
    if not (0 <= bank < MAX_IMPORT_BANKS):
        raise Exception(f"Program {program_number} is out of range for {name()}")
    return [0xF0, MANUFACTURER_ID, channel & 0x0F, MODEL_ID_CLASSIC, FIRST_PROGRAM_DUMP_TYPE + bank, slot] + _payload_from_patch_message(message) + [0xF7]


def numberFromDump(message):
    if isSingleProgramDump(message):
        bank = message[4] - FIRST_PROGRAM_DUMP_TYPE
        return bank * PATCHES_PER_BANK + message[5]
    return -1


def blankedOut(message):
    if not (isEditBufferDump(message) or isSingleProgramDump(message)):
        raise Exception("Can only blank out classic Nord Lead program or edit buffer dumps")
    return [0xF0, MANUFACTURER_ID, 0x00, MODEL_ID_CLASSIC, EDIT_BUFFER_DUMP_TYPE, 0x00] + _payload_from_patch_message(message) + [0xF7]


def calculateFingerprint(message):
    return hashlib.md5(bytearray(blankedOut(message))).hexdigest()


def legacyLoadSupportedExtensions() -> List[str]:
    return [".mid"]


def loadPatchesFromLegacyData(data: List[int]) -> List[List[int]]:
    return [
        message
        for message in _extract_sysex_from_midi_bytes(bytes(data))
        if isSingleProgramDump(message) or isEditBufferDump(message)
    ]


def _has_classic_header(message: List[int]) -> bool:
    return (
        len(message) >= 7
        and message[0] == 0xF0
        and message[1] == MANUFACTURER_ID
        and message[3] == MODEL_ID_CLASSIC
        and message[-1] == 0xF7
    )


def _payload_from_patch_message(message: List[int]) -> List[int]:
    return message[6:-1]


def _read_vlq(data: bytes, index: int) -> Tuple[int, int]:
    value = 0
    while True:
        if index >= len(data):
            raise ValueError("Unexpected end of MIDI data while decoding VLQ")
        byte = data[index]
        index += 1
        value = (value << 7) | (byte & 0x7F)
        if byte < 0x80:
            return value, index


def _skip_channel_message(status: int, index: int) -> int:
    if 0xC0 <= status <= 0xDF:
        return index + 1
    return index + 2


def _extract_sysex_from_midi_bytes(data: bytes) -> List[List[int]]:
    if len(data) < 14 or data[:4] != b"MThd":
        return []

    header_length = int.from_bytes(data[4:8], "big")
    if header_length < 6:
        return []

    track_count = int.from_bytes(data[10:12], "big")
    index = 8 + header_length
    messages: List[List[int]] = []
    running_status = None
    pending_sysex: List[int] = []

    for _ in range(track_count):
        if index + 8 > len(data) or data[index:index + 4] != b"MTrk":
            break
        track_length = int.from_bytes(data[index + 4:index + 8], "big")
        track = data[index + 8:index + 8 + track_length]
        index += 8 + track_length

        pos = 0
        running_status = None
        pending_sysex = []
        while pos < len(track):
            _, pos = _read_vlq(track, pos)
            if pos >= len(track):
                break

            status = track[pos]
            if status >= 0x80:
                pos += 1
                running_status = None if status in (0xF0, 0xF7, 0xFF) else status
            elif running_status is not None:
                status = running_status
            else:
                break

            if status == 0xFF:
                if pos >= len(track):
                    break
                pos += 1
                meta_length, pos = _read_vlq(track, pos)
                pos += meta_length
                continue

            if status in (0xF0, 0xF7):
                sysex_length, pos = _read_vlq(track, pos)
                payload = list(track[pos:pos + sysex_length])
                pos += sysex_length
                if status == 0xF0:
                    pending_sysex = [0xF0] + payload
                elif pending_sysex:
                    pending_sysex.extend(payload)
                else:
                    pending_sysex = [0xF0] + payload

                if pending_sysex and pending_sysex[-1] == 0xF7:
                    messages.append(pending_sysex)
                    pending_sysex = []
                continue

            if 0x80 <= status <= 0xEF:
                pos = _skip_channel_message(status, pos)
                continue

            if status in (0xF1, 0xF3):
                pos += 1
            elif status == 0xF2:
                pos += 2

    return messages


def _testdata_path(filename: str) -> str:
    return str(Path(__file__).resolve().parent / "testData" / "Clavia_Nord_Lead" / filename)


def _legacy_file_bytes(filename: str) -> List[int]:
    return list(Path(_testdata_path(filename)).read_bytes())


def _assert_first_patch_is_program(adaptation, patches: List[List[int]]):
    assert len(patches) > 0
    assert adaptation.isSingleProgramDump(patches[0])


def make_test_data():
    from testing.mock_midi import EditBufferMockDevice, ProgramDumpMockDevice

    bank0_messages = knobkraft.load_sysex(_testdata_path("bank0.syx"))
    bank1_messages = knobkraft.load_sysex(_testdata_path("bank1.syx"))
    bank0_programs = [message for message in bank0_messages if isSingleProgramDump(message)]
    bank1_programs = [message for message in bank1_messages if isSingleProgramDump(message)]
    normalized_bank0 = [
        convertToProgramDump(0, message, program_no)
        for program_no, message in enumerate(bank0_programs)
    ]
    normalized_bank1 = [
        convertToProgramDump(0, message, PATCHES_PER_BANK + program_no)
        for program_no, message in enumerate(bank1_programs)
    ]

    def programs(_test_data: testing.TestData) -> List[testing.ProgramTestData]:
        selected = [normalized_bank0[0], normalized_bank0[41], normalized_bank0[-1]]
        return [
            testing.ProgramTestData(
                message=message,
                number=numberFromDump(message),
                friendly_number=friendlyProgramName(numberFromDump(message)),
            )
            for message in selected
        ]

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        return [testing.ProgramTestData(message=convertToEditBuffer(0, test_data.programs[0].message.byte_list))]

    def banks(_test_data: testing.TestData) -> List[List[List[int]]]:
        return [bank0_messages, bank1_messages]

    def mock_device(_test_data: testing.TestData, _adaptation):
        return ProgramDumpMockDevice(_adaptation, normalized_bank0 + normalized_bank1)

    def edit_buffer_mock_device(test_data: testing.TestData, adaptation):
        first_buffer = test_data.edit_buffers[0].message.byte_list
        return EditBufferMockDevice(adaptation, [first_buffer.copy() for _ in range(PATCHES_PER_BANK)])

    return testing.TestData(
        sysex=_testdata_path("bank0.syx"),
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,
        expected_patch_count=PATCHES_PER_BANK,
        program_dump_request=(0, 11, createProgramDumpRequest(0, 11)),
        device_detect_call=createDeviceDetectMessage(0),
        device_detect_reply=(convertToEditBuffer(0, normalized_bank0[0]), 0),
        friendly_bank_name=(1, "Bank 1"),
        legacy_loader_cases=[
            testing.LegacyLoaderTestData(
                file_extension=".mid",
                file_content=_legacy_file_bytes("nord-lead-2-internal.mid"),
                expected_patch_count=40,
                patch_inspector=_assert_first_patch_is_program,
            ),
            testing.LegacyLoaderTestData(
                file_extension=".mid",
                file_content=_legacy_file_bytes("nord-lead-bank1.mid"),
                expected_patch_count=PATCHES_PER_BANK,
                patch_inspector=_assert_first_patch_is_program,
            ),
        ],
        mock_device_factory=mock_device,
        expected_wire_patch_count=PATCHES_PER_BANK,
        expected_sent_messages=lambda _test_data, adaptation: [
            adaptation.createProgramDumpRequest(0, program_no)
            for program_no in range(PATCHES_PER_BANK)
        ],
        single_edit_buffer_mock_device_factory=edit_buffer_mock_device,
        expected_single_edit_buffer_count=1,
        wire_download_banks=[0, 1],
        expected_multi_bank_patch_count=PATCHES_PER_BANK * 2,
        expected_multi_bank_sent_messages=lambda _test_data, adaptation: [
            adaptation.createProgramDumpRequest(0, program_no)
            for program_no in range(PATCHES_PER_BANK * 2)
        ],
        send_to_synth_patch=lambda test_data: test_data.programs[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: knobkraft.splitSysex(
            adaptation.convertToEditBuffer(0, test_data.programs[0].message.byte_list)
        ),
    )
