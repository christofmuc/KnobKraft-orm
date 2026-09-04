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

# Lead 1/2 and even the 2X request table document only types 0x0B..0x0E.
# The 2X dump table supports ten banks. Do not extrapolate request types:
# 0x14 (which would be bank 10) is the All Controllers Request command.
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
        "different SysEx format and are not covered by this adaptation. "
        "Set the Global MIDI channel to the channel selected in KnobKraft. Select a normal synth patch "
        "in Slot A (not a percussion kit) for detection and audition; audition replaces Slot A only. "
        "Live downloads expose four banks of 99 program locations (displayed as banks 0-3); bank availability "
        "depends on the instrument and installed memory card. 2X dumps from all ten banks can be imported "
        "and exported, but live requests for banks 4-9 are deliberately unsupported because the 2X manual "
        "does not document their request codes. Use a front-panel dump/file for those banks. "
        "Program-dump playback may overwrite locations in the bank currently selected on the instrument; "
        "normal audition uses the temporary edit buffer instead. Percussion kits and performances are "
        "not supported. Tested with official files and mock MIDI, not with hardware."
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
    if isEditBufferDump(message) and message[5] == 0:
        return message[2]
    return -1


def numberOfBanks():
    return CLASSIC_BANK_COUNT


def numberOfPatchesPerBank():
    return PATCHES_PER_BANK


def friendlyBankName(bank):
    if not 0 <= bank < MAX_IMPORT_BANKS:
        raise ValueError(f"Invalid Nord Lead bank {bank}")
    return f"Bank {bank}"


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
    if not (0 <= bank < CLASSIC_BANK_COUNT):
        raise ValueError(f"Program {patch_no} is outside the four documented live-request banks")
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
    return hashlib.md5(bytearray(blankedOut(message)), usedforsecurity=False).hexdigest()


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
        and 0 <= message[2] <= 0x0F
        and message[3] == MODEL_ID_CLASSIC
        and message[-1] == 0xF7
        and all(0 <= byte <= 0x0F for byte in message[6:-1])
    )


def _payload_from_patch_message(message: List[int]) -> List[int]:
    return message[6:-1]


def _read_vlq(data: bytes, index: int) -> Tuple[int, int]:
    value = 0
    for _ in range(4):
        if index >= len(data):
            raise ValueError("Unexpected end of MIDI data while decoding VLQ")
        byte = data[index]
        index += 1
        value = (value << 7) | (byte & 0x7F)
        if byte < 0x80:
            return value, index
    raise ValueError("MIDI VLQ exceeds four bytes")


def _read_midi_payload(track: bytes, pos: int) -> Tuple[bytes, int]:
    length, pos = _read_vlq(track, pos)
    end = pos + length
    if end > len(track):
        raise ValueError("MIDI event payload extends beyond track")
    return track[pos:end], end


def _extract_sysex_from_midi_bytes(data: bytes) -> List[List[int]]:
    if data[:4] != b"MThd":
        return []
    if len(data) < 14:
        raise ValueError("Truncated MIDI header")
    header_length = int.from_bytes(data[4:8], "big")
    if header_length < 6 or 8 + header_length > len(data):
        raise ValueError("Invalid MIDI header length")
    midi_format = int.from_bytes(data[8:10], "big")
    track_count = int.from_bytes(data[10:12], "big")
    if midi_format not in (0, 1, 2) or track_count == 0 or (midi_format == 0 and track_count != 1):
        raise ValueError("Invalid MIDI format or track count")
    index = 8 + header_length
    messages: List[List[int]] = []
    for _ in range(track_count):
        if index + 8 > len(data) or data[index:index + 4] != b"MTrk":
            raise ValueError("Missing MIDI track")
        track_length = int.from_bytes(data[index + 4:index + 8], "big")
        if index + 8 + track_length > len(data):
            raise ValueError("Truncated MIDI track")
        track = data[index + 8:index + 8 + track_length]
        index += 8 + track_length

        pos = 0
        running_status = None
        pending_sysex = []
        ended = False
        while pos < len(track):
            _, pos = _read_vlq(track, pos)
            if pos >= len(track):
                raise ValueError("Missing MIDI event after delta time")

            status = track[pos]
            if status >= 0x80:
                pos += 1
                running_status = status if 0x80 <= status <= 0xEF else None
            elif running_status is not None:
                status = running_status
            else:
                raise ValueError("MIDI data byte without running status")

            if status == 0xFF:
                if pos >= len(track):
                    raise ValueError("Missing MIDI meta-event type")
                meta_type = track[pos]
                pos += 1
                if meta_type >= 0x80:
                    raise ValueError("Invalid MIDI meta-event type")
                payload, pos = _read_midi_payload(track, pos)
                if meta_type == 0x2F:
                    if payload or pos != len(track) or pending_sysex:
                        raise ValueError("Invalid end-of-track or incomplete SysEx")
                    ended = True
                continue

            if status in (0xF0, 0xF7):
                payload_bytes, pos = _read_midi_payload(track, pos)
                payload = list(payload_bytes)
                if status == 0xF0:
                    if pending_sysex:
                        raise ValueError("New SysEx before previous message completed")
                    pending_sysex = [0xF0] + payload
                elif pending_sysex:
                    pending_sysex.extend(payload)
                else:
                    # An unpaired F7 event is an SMF escape, not an implicit
                    # SysEx start. Only extract explicitly framed messages.
                    escaped = []
                    for byte in payload:
                        if byte == 0xF0:
                            if escaped:
                                raise ValueError("Nested SysEx in MIDI escape")
                            escaped = [byte]
                        elif escaped:
                            escaped.append(byte)
                            if byte == 0xF7:
                                messages.append(escaped)
                                escaped = []
                    if escaped:
                        raise ValueError("Incomplete SysEx in MIDI escape")
                    continue

                sysex_data = pending_sysex[1:-1] if pending_sysex[-1] == 0xF7 else pending_sysex[1:]
                if any(byte >= 0x80 for byte in sysex_data):
                    raise ValueError("Embedded status byte in MIDI SysEx")
                if pending_sysex and pending_sysex[-1] == 0xF7:
                    messages.append(pending_sysex)
                    pending_sysex = []
                continue

            if 0x80 <= status <= 0xEF:
                end = pos + (1 if 0xC0 <= status <= 0xDF else 2)
                if end > len(track) or any(byte >= 0x80 for byte in track[pos:end]):
                    raise ValueError("Truncated or invalid MIDI channel event")
                pos = end
                continue
            raise ValueError(f"Unsupported MIDI file event status {status:#x}")
        if not ended:
            raise ValueError("MIDI track has no end-of-track event")
    if index != len(data):
        raise ValueError("Unexpected data after declared MIDI tracks")
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
