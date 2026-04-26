#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import sys
from pathlib import Path
from typing import List

import knobkraft
import testing
from roland import DataBlock, RolandData, GenericRoland, command_dt1


this_module = sys.modules[__name__]


# The JD-800 patch format is modeled as two DT1 blocks per patch:
# 0x0200 bytes at offset 00 00 00 and 0x0100 bytes at offset 00 02 00.
# Stored patches live in a consecutive address space starting at 05 00 00.
_jd800_patch_blocks = [
    DataBlock((0x00, 0x00, 0x00), (0x00, 0x02, 0x00), "Patch block A"),
    DataBlock((0x00, 0x02, 0x00), (0x00, 0x01, 0x00), "Patch block B"),
]

_jd800_edit_buffer = RolandData(
    "JD-800 temporary patch",
    1,
    3,
    3,
    (0x00, 0x00, 0x00),
    _jd800_patch_blocks,
    uses_consecutive_addresses=True,
)

_jd800_program_dump = RolandData(
    "JD-800 internal patch memory",
    64,
    3,
    3,
    (0x05, 0x00, 0x00),
    _jd800_patch_blocks,
    uses_consecutive_addresses=True,
)

jd_800 = GenericRoland(
    "Roland JD-800",
    model_id=[0x3D],
    address_size=3,
    edit_buffer=_jd800_edit_buffer,
    program_dump=_jd800_program_dump,
    device_detect_message=_jd800_edit_buffer,
    patch_name_length=16,
    uses_consecutive_addresses=True,
)

# GenericRoland assumes modern Roland naming and a one-byte program slot.
# The JD-800 uses 16-character names and a two-byte consecutive program address.
jd_800.edit_buffer.make_black_out_zones(
    jd_800._model_id_len,
    device_id_position=2,
    name_blankout=(0, 0, 16),
)

jd_800.program_dump.make_black_out_zones(
    jd_800._model_id_len,
    program_position=(4 + jd_800._model_id_len, 2),
    device_id_position=2,
    name_blankout=(0, 0, 16),
)

jd_800.install(this_module)


def setupHelp():
    return (
        "Set JD-800 MIDI Unit Number to 17 and enable Rx Exclusive ON-1. "
        "Alternatively, Rx Exclusive ON-2 ignores the configured Unit Number."
    )


def _testdata_path(filename: str) -> str:
    return str(Path(__file__).resolve().parent / "testData" / "Roland_JD800" / filename)


def _programs_from_sysex(test_data: testing.TestData) -> List[testing.ProgramTestData]:
    programs = []
    patch = []
    for message in test_data.all_messages:
        if jd_800.isPartOfSingleProgramDump(message):
            patch.extend(message)
            if jd_800.isSingleProgramDump(patch):
                programs.append(
                    testing.ProgramTestData(
                        message=patch.copy(),
                        name=jd_800.nameFromDump(patch),
                        number=jd_800.numberFromDump(patch),
                        rename_name="Renamed JD-800 ",
                    )
                )
                patch = []
    return programs


def _device_detect_reply() -> List[int]:
    address, _ = _jd800_edit_buffer.address_and_size_for_sub_request(0, 0)
    data = [0] * _jd800_patch_blocks[0].size
    return jd_800.buildRolandMessage(0x10, command_dt1, address, data)


def _responses_for_edit_buffer(adaptation, edit_buffer):
    responses = {}
    request = adaptation.createEditBufferRequest(0)
    for message in knobkraft.splitSysex(edit_buffer):
        responses[tuple(request)] = [message]
        handshake = adaptation.isPartOfEditBufferDump(message)
        request = handshake[1] if isinstance(handshake, tuple) else []
    return responses


def make_test_data():
    from testing.mock_midi import ScriptedMockDevice

    def programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        return _programs_from_sysex(test_data)

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        source_program = next(program for program in _programs_from_sysex(test_data) if program.number == 49)
        edit_buffer = jd_800.convertToEditBuffer(0, source_program.message.byte_list)
        return [
            testing.ProgramTestData(
                message=edit_buffer,
                name=jd_800.nameFromDump(edit_buffer),
                rename_name="Edit JD-800    ",
            )
        ]

    def single_edit_buffer_mock_device(test_data: testing.TestData, adaptation):
        first_edit_buffer = test_data.edit_buffers[0].message.byte_list
        return ScriptedMockDevice(_responses_for_edit_buffer(adaptation, first_edit_buffer))

    return testing.TestData(
        sysex=_testdata_path("jd800_synthetic_bank.syx"),
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        expected_patch_count=3,
        device_detect_call="F0 41 10 3D 11 00 00 00 00 02 00 7E F7",
        device_detect_reply=(_device_detect_reply(), 0),
        program_dump_request=(0, 49, "F0 41 10 3D 11 06 13 00 00 02 00 65 F7"),
        single_edit_buffer_mock_device_factory=single_edit_buffer_mock_device,
        send_to_synth_patch=lambda test_data: test_data.edit_buffers[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: knobkraft.splitSysex(
            adaptation.convertToEditBuffer(0, test_data.edit_buffers[0].message.byte_list)
        ),
    )
