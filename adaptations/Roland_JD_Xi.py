#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import itertools
import sys
from typing import List

import knobkraft
import testing
from roland import DataBlock, RolandData, GenericRoland, command_dt1


this_module = sys.modules[__name__]


_jdxi_sn_tone_blocks = [
    DataBlock((0x00, 0x00, 0x00, 0x00), 0x40, "SuperNATURAL synth tone common"),
    DataBlock((0x00, 0x00, 0x20, 0x00), 0x3d, "SuperNATURAL synth tone partial 1"),
    DataBlock((0x00, 0x00, 0x21, 0x00), 0x3d, "SuperNATURAL synth tone partial 2"),
    DataBlock((0x00, 0x00, 0x22, 0x00), 0x3d, "SuperNATURAL synth tone partial 3"),
    DataBlock((0x00, 0x00, 0x50, 0x00), 0x25, "SuperNATURAL synth tone modify"),
]

_jdxi_sn_tone_edit_buffer = RolandData(
    "JD-Xi temporary SuperNATURAL synth tone, Digital Synth Part 2",
    1,
    4,
    4,
    (0x19, 0x21, 0x00, 0x00),
    _jdxi_sn_tone_blocks,
)

_jdxi_system_common = RolandData(
    "JD-Xi system common",
    1,
    4,
    4,
    (0x02, 0x00, 0x00, 0x00),
    [DataBlock((0x00, 0x00, 0x00, 0x00), 0x2b, "System common")],
)

_jdxi_sn_tone = GenericRoland(
    "Roland JD-Xi",
    model_id=[0x00, 0x00, 0x00, 0x0e],
    address_size=4,
    edit_buffer=_jdxi_sn_tone_edit_buffer,
    program_dump=_jdxi_sn_tone_edit_buffer,
    device_detect_message=_jdxi_system_common,
)


def name():
    return _jdxi_sn_tone.name()


def createDeviceDetectMessage(channel: int) -> List[int]:
    return _jdxi_sn_tone.createDeviceDetectMessage(channel)


def channelIfValidDeviceResponse(message: List[int]) -> int:
    return _jdxi_sn_tone.channelIfValidDeviceResponse(message)


def needsChannelSpecificDetection() -> bool:
    return _jdxi_sn_tone.needsChannelSpecificDetection()


def createEditBufferRequest(channel) -> List[int]:
    return _jdxi_sn_tone.createEditBufferRequest(channel)


def isPartOfEditBufferDump(message):
    return _jdxi_sn_tone.isPartOfEditBufferDump(message)


def isEditBufferDump(messages):
    return _jdxi_sn_tone.isEditBufferDump(messages)


def convertToEditBuffer(channel, message):
    return _jdxi_sn_tone.convertToEditBuffer(channel, message)


def nameFromDump(message) -> str:
    return _jdxi_sn_tone.nameFromDump(message)


def renamePatch(message: List[int], new_name: str) -> List[int]:
    return _jdxi_sn_tone.renamePatch(message, new_name)


def blankedOut(message):
    return _jdxi_sn_tone.blankedOut(message)


def calculateFingerprint(message):
    return _jdxi_sn_tone.calculateFingerprint(message)


def setupHelp():
    return ("This first JD-Xi adaptation supports SuperNATURAL Digital Synth tone dumps. "
            "The current edit-buffer request targets Digital Synth Part 2 at address 19 21 00 00.")


def _system_common_reply():
    address, _ = _jdxi_system_common.address_and_size_for_sub_request(0, 0)
    return _jdxi_sn_tone.buildRolandMessage(0x10, command_dt1, address, [0] * 0x2b)


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

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        raw_data = list(itertools.chain.from_iterable(test_data.all_messages))
        for message in test_data.all_messages:
            assert isPartOfEditBufferDump(message)
        yield testing.ProgramTestData(message=raw_data, name="Atmo Pad    ", rename_name="Atmo Pad 2  ")

    def single_edit_buffer_mock_device(test_data: testing.TestData, adaptation):
        first_edit_buffer = next(iter(test_data.edit_buffers)).message.byte_list
        return ScriptedMockDevice(_responses_for_edit_buffer(adaptation, first_edit_buffer))

    return testing.TestData(
        sysex="testData/Roland_JD_Xi/JDXi-SN-Atmo_Pad.syx",
        edit_buffer_generator=edit_buffers,
        expected_patch_count=1,
        device_detect_call=createDeviceDetectMessage(0),
        device_detect_reply=(_system_common_reply(), 0),
        single_edit_buffer_mock_device_factory=single_edit_buffer_mock_device,
        send_to_synth_patch=lambda test_data: test_data.edit_buffers[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: knobkraft.splitSysex(
            adaptation.convertToEditBuffer(0, test_data.edit_buffers[0].message.byte_list)
        ),
    )
