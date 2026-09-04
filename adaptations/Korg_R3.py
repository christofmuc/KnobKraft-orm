#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from typing import List

import knobkraft
import korg
import testing


model_id = [0x7d]
family_id = [0x7d, 0x00]
member_id = [0x00, 0x00]

command_current_program_data_dump_request = 0x10
command_program_data_dump_request = 0x1c
command_program_write_request = 0x11
command_current_program_data_dump = 0x40
command_program_data_dump = 0x4c

program_data_size = 452
program_name_offset = 0
program_name_length = 8
packed_program_data_size = 517


def name():
    return "Korg R3"


def createDeviceDetectMessage(channel):
    return korg.createDeviceDetectMessage(0x7f)


def deviceDetectWaitMilliseconds():
    return 200


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    return korg.channelIfValidDeviceResponse(message, family_id, member_id)


def setupHelp():
    return ('Enable the R3 "SystemEx" global parameter before requesting or sending dumps. '
            "The R3 MIDI implementation says these messages are recognized only when SystemEx is enabled.")


def createEditBufferRequest(channel):
    return korg.buildMessage(channel, model_id, command_current_program_data_dump_request)


def isEditBufferDump(message):
    return (korg.hasKorgHeader(message, model_id, command_current_program_data_dump)
            and len(message) == 5 + packed_program_data_size + 1)


def createProgramDumpRequest(channel, patchNo):
    return korg.buildMessage(channel, model_id, command_program_data_dump_request, korg.split_14bit(patchNo))


def isSingleProgramDump(message):
    return (korg.hasKorgHeader(message, model_id, command_program_data_dump)
            and len(message) == 7 + packed_program_data_size + 1)


def numberOfBanks():
    return 16


def numberOfPatchesPerBank():
    return 8


def friendlyBankName(bank):
    if 0 <= bank < numberOfBanks():
        return chr(ord("A") + bank)
    return f"Bank {bank + 1}"


def friendlyProgramName(program):
    if program < 0:
        return "(Edit Buffer)"
    bank = program // numberOfPatchesPerBank()
    patch = program % numberOfPatchesPerBank()
    return f"{friendlyBankName(bank)}{patch + 1}"


def createCustomProgramChange(channel, patchNo):
    return [0xb0 | (channel & 0x0f), 0x00, 0x00, 0xb0 | (channel & 0x0f), 0x20, 0x00,
            0xc0 | (channel & 0x0f), patchNo & 0x7f]


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return korg.buildMessage(channel, model_id, command_current_program_data_dump, _payload_from_dump(message))
    if isSingleProgramDump(message):
        return korg.buildMessage(channel, model_id, command_current_program_data_dump, message[7:-1])
    raise Exception("Neither edit buffer nor program dump. Can't convert to edit buffer.")


def convertToProgramDump(channel, message, program_number):
    program_number_bytes = korg.split_14bit(program_number)
    if isSingleProgramDump(message):
        return korg.buildMessage(channel, model_id, command_program_data_dump, program_number_bytes + message[7:-1])
    if isEditBufferDump(message):
        return korg.buildMessage(channel, model_id, command_program_data_dump, program_number_bytes + message[5:-1])
    raise Exception("Neither edit buffer nor program dump. Can't convert to program dump.")


def numberFromDump(message):
    if isSingleProgramDump(message):
        return korg.join_14bit(message[5], message[6])
    return -1


def nameFromDump(message):
    data = _program_data_from_dump(message)
    return "".join(chr(x) if 32 <= x <= 126 else " " for x in data[program_name_offset:program_name_offset + program_name_length]).rstrip()


def renamePatch(message, new_name):
    data = _program_data_from_dump(message)
    clean_name = _encode_name(new_name)
    renamed = data[:]
    renamed[program_name_offset:program_name_offset + program_name_length] = clean_name
    packed = korg.escapeSysex(renamed)
    if isSingleProgramDump(message):
        return message[:7] + packed + [0xf7]
    if isEditBufferDump(message):
        return message[:5] + packed + [0xf7]
    raise Exception("Neither edit buffer nor program dump. Can't rename.")


def blankedOut(message):
    data = _program_data_from_dump(message)
    data[program_name_offset:program_name_offset + program_name_length] = [0] * program_name_length
    packed = korg.escapeSysex(data)
    if isSingleProgramDump(message):
        return korg.buildMessage(0, model_id, command_program_data_dump, [0, 0] + packed)
    if isEditBufferDump(message):
        return korg.buildMessage(0, model_id, command_current_program_data_dump, packed)
    raise Exception("Neither edit buffer nor program dump. Can't blank out.")


def calculateFingerprint(message):
    return hashlib.md5(bytearray(blankedOut(message))).hexdigest()


def _payload_from_dump(message):
    if isSingleProgramDump(message):
        return message[7:-1]
    if isEditBufferDump(message):
        return message[5:-1]
    raise Exception("Neither edit buffer nor program dump.")


def _program_data_from_dump(message):
    data = korg.unescapeSysex(_payload_from_dump(message))
    if len(data) != program_data_size:
        raise Exception(f"Expected {program_data_size} bytes of R3 program data, got {len(data)}")
    return data


def _encode_name(new_name):
    return [ord(c) if 32 <= ord(c) <= 126 else 32 for c in new_name[:program_name_length].ljust(program_name_length)]


def _build_program_data(patch_name: str, seed: int) -> List[int]:
    data = [(seed + i * 17) & 0xff for i in range(program_data_size)]
    data[program_name_offset:program_name_offset + program_name_length] = _encode_name(patch_name)
    return data


def _program_dump(channel: int, program_number: int, patch_name: str, seed: int) -> List[int]:
    return korg.buildMessage(
        channel,
        model_id,
        command_program_data_dump,
        korg.split_14bit(program_number) + korg.escapeSysex(_build_program_data(patch_name, seed)),
    )


def _edit_buffer_dump(channel: int, patch_name: str, seed: int) -> List[int]:
    return korg.buildMessage(
        channel,
        model_id,
        command_current_program_data_dump,
        korg.escapeSysex(_build_program_data(patch_name, seed)),
    )


def make_test_data():
    from testing.mock_midi import EditBufferMockDevice, ProgramDumpMockDevice

    program_messages = [
        _program_dump(0, 0, "BassSaw", 0x11),
        _program_dump(0, 1, "HyperVoc", 0x22),
        _program_dump(0, 7, "Screamin", 0x33),
        _program_dump(0, 8, "Justice", 0x44),
    ]
    edit_buffer_message = _edit_buffer_dump(0, "EditR3", 0x55)

    def programs(_: testing.TestData) -> List[testing.ProgramTestData]:
        return [
            testing.ProgramTestData(message=program_messages[0], name="BassSaw", number=0, friendly_number="A1", rename_name="NewBass"),
            testing.ProgramTestData(message=program_messages[1], name="HyperVoc", number=1, friendly_number="A2", rename_name="NewVoc"),
            testing.ProgramTestData(message=program_messages[2], name="Screamin", number=7, friendly_number="A8", rename_name="Scream2"),
            testing.ProgramTestData(message=program_messages[3], name="Justice", number=8, friendly_number="B1", rename_name="Justic2"),
        ]

    def edit_buffers(_: testing.TestData) -> List[testing.ProgramTestData]:
        return [testing.ProgramTestData(message=edit_buffer_message, name="EditR3", rename_name="EditNew")]

    def mock_device(test_data: testing.TestData, adaptation):
        patch_slots = []
        available_programs = list(test_data.programs)
        for program_no in range(numberOfPatchesPerBank() * numberOfBanks()):
            patch_slots.append(adaptation.convertToProgramDump(0, available_programs[program_no % len(available_programs)].message.byte_list, program_no))
        return ProgramDumpMockDevice(adaptation, patch_slots)

    def single_edit_buffer_mock_device(test_data: testing.TestData, adaptation):
        first_edit_buffer = next(iter(test_data.edit_buffers)).message.byte_list
        return EditBufferMockDevice(adaptation, [first_edit_buffer.copy() for _ in range(numberOfPatchesPerBank())])

    return testing.TestData(
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        program_dump_request=(0, 0, createProgramDumpRequest(0, 0)),
        device_detect_call=createDeviceDetectMessage(0),
        device_detect_reply=("F0 7E 00 06 02 42 7D 00 00 00 01 00 01 00 F7", 0),
        friendly_bank_name=(1, "B"),
        mock_device_factory=mock_device,
        expected_wire_patch_count=numberOfPatchesPerBank(),
        expected_sent_messages=lambda _test_data, adaptation: [
            adaptation.createProgramDumpRequest(0, program_no)
            for program_no in range(numberOfPatchesPerBank())
        ],
        single_edit_buffer_mock_device_factory=single_edit_buffer_mock_device,
        expected_single_edit_buffer_count=1,
        wire_download_banks=[0, 1],
        expected_multi_bank_patch_count=numberOfPatchesPerBank() * 2,
        expected_multi_bank_sent_messages=lambda _test_data, adaptation: [
            adaptation.createProgramDumpRequest(0, program_no)
            for program_no in range(numberOfPatchesPerBank() * 2)
        ],
        send_to_synth_patch=lambda test_data: test_data.programs[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: knobkraft.splitSysex(
            adaptation.convertToEditBuffer(0, test_data.programs[0].message.byte_list)
        ),
    )
