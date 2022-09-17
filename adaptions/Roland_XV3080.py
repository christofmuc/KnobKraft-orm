#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Finally owning a classic Roland so I can make a working and tested example on how to implement the Roland Synths
from roland import *
import knobkraft

roland_id = 0x41  # Roland
models_supported = {
    "JV1010": {"model_id": [0x6a], "address_size": 4},  # JV-1010
    "XV3080": {"model_id": [0x00, 0x10], "address_size": 4},  # XV-3080
    "GS": {"model_id": [0x42], "address_size": 3},  # GS standard
}
command_rq1 = 0x11
command_dt1 = 0x12

device_id = 0x10  # The Roland can have a device ID from 0x00 to 0x1f
# - this finally kills my channel/device ID confusion

edit_buffer_addresses_jv1080 = RolandData("JV-1080 Temporary Patch", 1, 4, 4,
                                          [DataBlock((0x03, 0x00, 0x00, 0x00), 0x48, "Patch common"),
                                           DataBlock((0x03, 0x00, 0x10, 0x00), (0x01, 0x01), "Patch tone 1"),
                                           DataBlock((0x03, 0x00, 0x12, 0x00), (0x01, 0x01), "Patch tone 2"),
                                           DataBlock((0x03, 0x00, 0x14, 0x00), (0x01, 0x01), "Patch tone 3"),
                                           DataBlock((0x03, 0x00, 0x16, 0x00), (0x01, 0x01), "Patch tone 4")])

program_buffer_addresses_jv1080 = RolandData("JV-1080 User Patches", 128, 4, 4,
                                             [DataBlock((0x11, 0x00, 0x00, 0x00), 0x48, "Patch common"),
                                              DataBlock((0x11, 0x00, 0x10, 0x00), (0x01, 0x01), "Patch tone 1"),
                                              DataBlock((0x11, 0x00, 0x12, 0x00), (0x01, 0x01), "Patch tone 2"),
                                              DataBlock((0x11, 0x00, 0x14, 0x00), (0x01, 0x01), "Patch tone 3"),
                                              DataBlock((0x11, 0x00, 0x16, 0x00), (0x01, 0x01), "Patch tone 4")])

edit_buffer_addresses_xv3080 = RolandData("XV-3080 Temporary Patch", 1, 4, 4,
                                          [DataBlock((0x1f, 0x00, 0x00, 0x00), 0x4f, "Patch common"),
                                           DataBlock((0x1f, 0x00, 0x02, 0x00), (0x01, 0x11), "Patch common MFX"),
                                           DataBlock((0x1f, 0x00, 0x04, 0x00), 0x34, "Patch common Chorus"),
                                           DataBlock((0x1f, 0x00, 0x06, 0x00), 0x53, "Patch common Reverb"),
                                           DataBlock((0x1f, 0x00, 0x10, 0x00), 0x29, "Patch common Tone Mix Table"),
                                           DataBlock((0x1f, 0x00, 0x20, 0x00), (0x01, 0x09), "Tone 1"),
                                           DataBlock((0x1f, 0x00, 0x22, 0x00), (0x01, 0x09), "Tone 2"),
                                           DataBlock((0x1f, 0x00, 0x24, 0x00), (0x01, 0x09), "Tone 3"),
                                           DataBlock((0x1f, 0x00, 0x26, 0x00), (0x01, 0x09), "Tone 4")])

program_buffer_addresses_xv3080 = RolandData("XV-3080 User Patches", 128, 4, 4,
                                             [DataBlock((0x30, 0x00, 0x00, 0x00), 0x4f, "Patch common"),
                                              DataBlock((0x30, 0x00, 0x02, 0x00), (0x01, 0x11), "Patch common MFX"),
                                              DataBlock((0x30, 0x00, 0x04, 0x00), 0x34, "Patch common Chorus"),
                                              DataBlock((0x30, 0x00, 0x06, 0x00), 0x53, "Patch common Reverb"),
                                              DataBlock((0x30, 0x00, 0x10, 0x00), 0x29, "Patch common Tone Mix Table"),
                                              DataBlock((0x30, 0x00, 0x20, 0x00), (0x01, 0x09), "Tone 1"),
                                              DataBlock((0x30, 0x00, 0x22, 0x00), (0x01, 0x09), "Tone 2"),
                                              DataBlock((0x30, 0x00, 0x24, 0x00), (0x01, 0x09), "Tone 3"),
                                              DataBlock((0x30, 0x00, 0x26, 0x00), (0x01, 0x09), "Tone 4")])

# Construct the Roland character set as specified in the MIDI implementation
character_set = [' '] + [chr(x) for x in range(ord('A'), ord('Z') + 1)] + \
                [chr(x) for x in range(ord('a'), ord('z') + 1)] + \
                [chr(x) for x in range(ord('1'), ord('9') + 1)] + ['0', '-']


def name():
    return "Roland XV-3080"


def setupHelp():
    return "Make sure the Receive Exclusive parameter (SYSTEM/COMMON) is ON."


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def channelIfValidDeviceResponse(message):
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


def needsChannelSpecificDetection():
    return False


def bankDescriptors():
    return [{"bank": 0, "name": "User Patches", "size": 128, "type": "User Patch"}]


def createEditBufferRequest(_channel):
    # The edit buffer is called Patch mode temporary patch address
    result = []
    for i in range(len(edit_buffer_addresses_xv3080.data_blocks)):
        address, size = edit_buffer_addresses_xv3080.address_and_size_for_sub_request(i, 0)
        result += buildRolandMessage(models_supported["XV3080"], device_id, command_rq1, address, size)
    return result


def isPartOfEditBufferDump(message):
    # Accept a certain set of addresses
    if isOwnSysex(message):
        model, command, address, data = parseRolandMessage(message)
        return command == command_dt1 and tuple(ignoreProgramPosition(address)) in set([x.address for x in edit_buffer_addresses_xv3080.data_blocks])
    return False


def isEditBufferDump(data):
    addresses = set()
    for message in knobkraft.sysex.splitSysexMessage(data):
        if isOwnSysex(message):
            model, command, address, data = parseRolandMessage(message)
            addresses.add(tuple(address))
    return all(a.address in addresses for a in edit_buffer_addresses_xv3080.data_blocks)


def convertToEditBuffer(_channel, message):
    print("Calling convert to edit buffer")
    editBuffer = []
    if isEditBufferDump(message) or isSingleProgramDump(message):
        # We need to poke the device ID and the edit buffer address into the 5 messages
        for message in knobkraft.sysex.splitSysexMessage(message):
            model, command, address, data = parseRolandMessage(message)
            editBuffer = editBuffer + buildRolandMessage(model, device_id, command_dt1, [0x1f, 0x00, address[2], address[3]], data)
        return editBuffer
    raise Exception("Can only convert edit buffers to edit buffers!")


def ignoreProgramPosition(address):
    # The address[1] part is where the program number. To compare addresses we set it to 0
    return address[0], 0x00, address[2], address[3]


def createProgramDumpRequest(_channel, patchNo):
    # Patches are called User Patch USER: 001 to USER: 128 on the JV1080
    address, size = program_buffer_addresses_xv3080.address_and_size_for_all_request(patchNo % 128)
    return buildRolandMessage(models_supported["XV3080"], device_id, command_rq1, address, size)


def isPartOfSingleProgramDump(message):
    # Accept a certain set of addresses
    if isOwnSysex(message):
        model, command, address, data = parseRolandMessage(message)
        matches = tuple(ignoreProgramPosition(address)) in set([x.address for x in program_buffer_addresses_xv3080.data_blocks])
        return command == command_dt1 and matches
    return False


def isSingleProgramDump(data):
    addresses = set()
    programs = set()
    for message in knobkraft.sysex.splitSysexMessage(data):
        model, command, address, data = parseRolandMessage(message)
        addresses.add(ignoreProgramPosition(address))
        programs.add(address[1])
    return len(programs) == 1 and all(a.address in addresses for a in program_buffer_addresses_xv3080.data_blocks)


def convertToProgramDump(_channel, message, program_number):
    programDump = []
    if isSingleProgramDump(message) or isEditBufferDump(message):
        # We need to poke the device ID and the program number into the 5 messages
        for message in knobkraft.sysex.splitSysexMessage(message):
            model, command, address, data = parseRolandMessage(message)
            programDump = programDump + buildRolandMessage(model, device_id, command_dt1, [0x30, program_number & 0x7f, address[2], address[3]], data)
        return programDump
    raise Exception("Can only convert single program dumps to program dumps!")


def numberFromDump(message):
    if not isSingleProgramDump(message):
        return 0
    messages = knobkraft.sysex.splitSysexMessage(message)
    model, command, address, data = parseRolandMessage(messages[0])
    return address[1]


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        messages = knobkraft.sysex.splitSysexMessage(message)
        model, command, address, data = parseRolandMessage(messages[0])
        patch_name = ''.join([chr(x) for x in data[0:12]])
        return patch_name.strip()
    return 'Invalid'


def isOwnSysex(message):
    for model in models_supported.values():
        model_id_len = _model_id_len(model)
        if len(message) > (2 + model_id_len) and message[0] == 0xf0 and message[1] == roland_id and message[
                                                                                                    3:3 + model_id_len] == \
                model["model_id"]:
            return model
    return None


def _model_id_len(model):
    return len(model["model_id"])


def _checksum_start(model):
    return 4 + _model_id_len(model)


def buildRolandMessage(model, device, command_id, address, data):
    message = [0xf0, roland_id, device & 0x1f] + model["model_id"] + [command_id] + address + data + [0, 0xf7]
    message[-2] = roland_checksum(message[_checksum_start(model):-2])
    return message


def parseRolandMessage(message: list):
    model = isOwnSysex(message)
    if model is not None:
        checksum_start = _checksum_start(model)
        model_id_len = _model_id_len(model)
        address_size = model["address_size"]
        checksum = roland_checksum(message[checksum_start:-2])
        if checksum == message[-2]:
            command = message[3 + model_id_len]
            address = message[checksum_start:checksum_start + address_size]
            return model, command, address, message[checksum_start + address_size:-2]
        raise Exception("Checksum error in Roland message parsing, expected", message[-2], "but got", checksum)
    return None, None, None, None


def roland_checksum(data_block):
    return sum([-x for x in data_block]) & 0x7f


def test():
    # First test the address calculations
    model = models_supported["XV3080"]

    # Example 1
    set_chorus_performance_common = [0xf0, 0x41, 0x10, 0x00, 0x10, 0x12, 0x10, 0x00, 0x04, 0x00, 0x02, 0x6a, 0xf7]
    assert (isOwnSysex(set_chorus_performance_common))
    model7, command3, address4, data5 = parseRolandMessage(set_chorus_performance_common)
    assert (command3 == 0x12)
    assert (address4 == [0x10, 0x00, 0x04, 0x00])
    assert (data5 == [0x02])
    composed6 = buildRolandMessage(model, 0x10, command_dt1, [0x10, 0x00, 0x04, 0x00], [0x02])
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
        model = isOwnSysex(message)
        assert (model is not None)
        if isPartOfSingleProgramDump(message):
            patch += message
            if isSingleProgramDump(patch):
                print("Found patch ", nameFromDump(patch))
                edit_buffer = convertToEditBuffer(0x00, patch)
                assert (isEditBufferDump(edit_buffer))
                assert (nameFromDump(edit_buffer) == nameFromDump(patch))
                back = convertToProgramDump(device_id, edit_buffer, numberFromDump(patch))
                assert (back == patch)
                patch = []


if __name__ == "__main__":
    test()
