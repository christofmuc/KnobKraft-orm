#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Finally owning a classic Roland so I can make a working and tested example on how to implement the Roland Synths
from typing import Optional

from roland import *
import knobkraft

device_id = 0x10  # The Roland can have a device ID from 0x00 to 0x1f
# - this finally kills my channel/device ID confusion


models_supported = [
    jv_1080, xv_3080
]

# Construct the Roland character set as specified in the MIDI implementation
character_set = [' '] + [chr(x) for x in range(ord('A'), ord('Z') + 1)] + \
                [chr(x) for x in range(ord('a'), ord('z') + 1)] + \
                [chr(x) for x in range(ord('1'), ord('9') + 1)] + ['0', '-']


def model_from_message(message) -> Optional[RolandSynth]:
    for synth in models_supported:
        if synth.isOwnSysex(message):
            return synth
    return None


def name():
    return "Roland XV-3080"


def setupHelp():
    return "Make sure the Receive Exclusive parameter (SYSTEM/COMMON) is ON."


def createDeviceDetectMessage(channel) -> List[int]:
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def channelIfValidDeviceResponse(message) -> int:
    global device_id
    # The XV-3080 will reply on a Universal Device Identity Reply message
    if (len(message) > 9
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x41  # Roland
            and message[6:8] == [0x10, 0x01]  # Family code expected
            and message[8:10] == [0x00, 0x00]):  # Family code
        device_id = message[2]  # Store the device ID for later, we'll need it
        return message[2] & 0x0f  # Simulate MIDI channel, but of course this is stupid
    return -1


def needsChannelSpecificDetection() -> bool:
    return False


def bankDescriptors():
    return [{"bank": 0, "name": "User Patches", "size": 128, "type": "User Patch"}]


def createEditBufferRequest(_channel) -> List[int]:
    return xv_3080.createEditBufferRequest(device_id)


def isPartOfEditBufferDump(message) -> bool:
    model = model_from_message(message)
    # Accept a certain set of addresses
    if model is not None:
        return model.isPartOfEditBufferDump(message)
    return False


def isEditBufferDump(data) -> bool:
    model = model_from_message(data)
    if model is not None:
        return model.isEditBufferDump(data)
    return False


def convertToEditBuffer(_channel, message):
    model = model_from_message(message)
    return model.convertToEditBuffer(device_id, message)


def createProgramDumpRequest(_channel, patchNo):
    return xv_3080.createProgramDumpRequest(device_id, patchNo)


def isPartOfSingleProgramDump(message):
    # Accept a certain set of addresses
    model = model_from_message(message)
    if model is not None:
        return model.isPartOfSingleProgramDump(message)
    return False


def isSingleProgramDump(data):
    model = model_from_message(data)
    if model is not None:
        return model.isSingleProgramDump(data)
    return False


def convertToProgramDump(_channel, message, program_number):
    model = model_from_message(message)
    if model is not None:
        return model.convertToProgramDump(device_id, message, program_number)
    raise Exception("Can only convert edit buffers and program dumps of one of the compatible synths!")


def numberFromDump(message):
    if not isSingleProgramDump(message):
        return 0
    messages = knobkraft.sysex.splitSysexMessage(message)
    model = model_from_message(message)
    command, address, data = model.parseRolandMessage(messages[0])
    return address[1]


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        model = model_from_message(message)
        messages = knobkraft.sysex.splitSysexMessage(message)
        command, address, data = model.parseRolandMessage(messages[0])
        patch_name = ''.join([chr(x) for x in data[0:12]])
        return patch_name.strip()
    return 'Invalid'


def test():
    # Example 1
    set_chorus_performance_common = [0xf0, 0x41, 0x10, 0x00, 0x10, 0x12, 0x10, 0x00, 0x04, 0x00, 0x02, 0x6a, 0xf7]
    assert (xv_3080.isOwnSysex(set_chorus_performance_common))
    command3, address4, data5 = xv_3080.parseRolandMessage(set_chorus_performance_common)
    assert (command3 == 0x12)
    assert (address4 == [0x10, 0x00, 0x04, 0x00])
    assert (data5 == [0x02])
    composed6 = xv_3080.buildRolandMessage(0x10, command_dt1, [0x10, 0x00, 0x04, 0x00], [0x02])
    assert (composed6 == set_chorus_performance_common)

    # Test weird address arithmetic
    assert (DataBlock.size_to_number((0x1, 0x1, 0x1)) == (16384 + 128 + 1))
    for i in range(1200):
        list_address = DataBlock.size_as_7bit_list(i, 4)
        and_back = DataBlock.size_to_number(tuple(list_address))
        assert (i == and_back)

    # Read a bank file
    messages = knobkraft.sysex.load_sysex("testData/AGSOUND1.SYX")
    patch = []
    for message in messages:
        model = model_from_message(message)
        assert (model is not None)
        if isPartOfSingleProgramDump(message):
            patch += message
            if isSingleProgramDump(patch):
                print("Found patch ", nameFromDump(patch))
                edit_buffer = convertToEditBuffer(0x00, patch)
                assert (isEditBufferDump(edit_buffer))
                assert (nameFromDump(edit_buffer) == nameFromDump(patch))
                back = convertToProgramDump(device_id, edit_buffer, numberFromDump(patch))
                assert (knobkraft.list_compare(back, patch))
                patch = []


if __name__ == "__main__":
    test()
