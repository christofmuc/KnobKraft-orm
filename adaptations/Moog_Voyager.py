#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# https://github.com/eclab/edisyn/blob/master/edisyn/synth/waldorfm/WaldorfM.java#L1952
import hashlib
from copy import copy
from typing import List, Tuple

import knobkraft.sysex
import testing


MOOG_ID = 0x04
VOYAGER = 0x01
ALL_PRESETS_DUMP = 0x01
PANEL_DUMP = 0x02
SINGLE_PANEL_DUMP = 0x03
ALL_PRESETS_DUMP_REQUEST = 0x04
PANEL_DUMP_REQUEST = 0x05
SINGLE_PANEL_DUMP_REQUEST = 0x06
ROM_DUMP = 0x70  # This is special, the the ROM sector number 0b00-0b11 uses bits 0 and 1, and the ROM sector counter 0b01-0b11 uses bits 2 and 3
BANK_SIZE = 128


def name():
    return "Moog Voyager"


def createDeviceDetectMessage(device_id):
    # Just request the edit buffer
    return createEditBufferRequest(device_id)


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    return 200


def channelIfValidDeviceResponse(message):
    if isEditBufferDump(message):
        return message[3] & 0x7f  # Hold the horses, the device identifier is not 0..15 but 0..127. This could lead to problems in old code.
    return -1


def bankDescriptors():
    return [{"bank": 0, "name": f"RAM", "size": 128, "type": "Single Preset Dump"}]


def createEditBufferRequest(device_id):
    return [0xf0, MOOG_ID, VOYAGER, device_id & 0x7f, PANEL_DUMP_REQUEST, 0xf7]


def isEditBufferDump(message: List[int]) -> bool:
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1] == MOOG_ID
            and message[2] == VOYAGER
            and message[4] == PANEL_DUMP)


def convertToEditBuffer(device_id, message):
    if isEditBufferDump(message):
        if message[3] & 0x7f != device_id & 0x7f:
            # Need to change device_id
            new_message = copy(message)
            new_message[3] = device_id & 0x7f
            return new_message
        else:
            return message
    elif isSingleProgramDump(message):
        # Drop the program number and change type to PANEL_DUMP
        return message[:3] + [device_id & 0x7f, PANEL_DUMP] + message[6:]
    raise Exception("Can only convert edit buffers or single programs")


def createProgramDumpRequest(device_id, patchNo):
    return [0xf0, MOOG_ID, VOYAGER, device_id & 0x7f, SINGLE_PANEL_DUMP_REQUEST, patchNo & 0x7f, 0xf7]


def isSingleProgramDump(message: List[int]) -> bool:
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1] == MOOG_ID
            and message[2] == VOYAGER
            and message[4] == SINGLE_PANEL_DUMP)


def numberFromDump(message: List[int]) -> int:
    if isSingleProgramDump(message):
        return message[5]
    return -1


def nameFromDump(message: List[int]) -> str:
    full, _, _ = nameFromDumpInternal(message)
    return full


def split_into_lines(int_list):
    line1 = []
    line2 = []
    current_line = line1

    for num in int_list:
        # Check if the highest bit is set (assuming 8-bit integers)
        if num & 0x80:
            # Clear the highest bit to get the actual character
            current_line.append(chr(num & 0x7F))
            # Switch to the second line
            current_line = line2
        else:
            current_line.append(chr(num))

    # Convert the lists of characters into strings
    line1 = ''.join(line1)
    line2 = ''.join(line2)

    return (line1, line2)


def nameFromDumpInternal(message: List[int]) -> Tuple[str, str, str]:
    if isSingleProgramDump(message):
        data = unpack_sysex(message[6:-1])
    elif isEditBufferDump(message):
        data = unpack_sysex(message[5:-1])
    else:
        return "invalid", "invalid", ""
    line1, line2 = split_into_lines(data[84:84+24])
    return line1 + line2, line1, line2


def numberOfLayers(messages):
    return 2


def friendlyLayerTitles():
    return ["Line 1", "Line 2"]


def layerName(messages, layerNo):
    full, layer1, layer2 = nameFromDumpInternal(messages)
    if layerNo == 0:
        return layer1
    elif layerNo == 1:
        return layer2
    else:
        raise Exception("Layer number must be 0 or 1")


def renamePatchInternal(message: List[int], new_name: List[int]) -> List[int]:
    if isSingleProgramDump(message):
        data_start = 6
    elif isEditBufferDump(message):
        data_start = 5
    else:
        raise Exception("Can only rename edit buffers or program buffers")
    assert len(new_name) == 24
    data = unpack_sysex(message[data_start:-1])
    data[84:84+24] = new_name
    return message[:data_start] + pack_sysex(data) + [0xf7]


def setLayerName(messages, layerNo, new_name):
    full, layer1, layer2 = nameFromDumpInternal(messages)
    if layerNo == 0:
        name_list = [ord(c) for c in new_name]
        name_list[-1] |= 0x80
        name_list.extend([ord(c) for c in layer2])
    elif layerNo == 1:
        name_list = [ord(c) for c in layer1]
        name_list[-1] |= 0x80
        name_list.extend([ord(c) for c in new_name])
    else:
        raise Exception("LayerNo must be 0 or 1")

    # Ensure the list is exactly 24 items long, padding with 0x20 if necessary
    name_list = name_list[:24] + [0x20] * (24 - len(name_list))
    name_list[-1] |= 0x80
    return renamePatchInternal(messages, name_list)


def convertToProgramDump(device_id, message, program_number):
    if isSingleProgramDump(message):
        # Need to patch device_id and program number
        return message[:3] + [device_id & 0x7f, SINGLE_PANEL_DUMP, program_number & 0x7f] + message[6:]
    elif isEditBufferDump(message):
        # Need to construct a new program dump from an edit buffer dump
        return message[:3] + [device_id & 0x7f, SINGLE_PANEL_DUMP, program_number & 0x7f] + message[5:]
    raise Exception("Can only convert program dumps and edit buffer dumps")


def calculateFingerprint(message: List[int]):
    if isSingleProgramDump(message):
        data_start = 6
    elif isEditBufferDump(message):
        data_start = 5
    else:
        raise Exception("Can only fingerprint single panel dumps or panel dumps")
    # Blank out program name
    data_block = unpack_sysex(message[data_start:-1])
    data_block[84:84+24] = [0] * 24
    return hashlib.md5(bytearray(data_block)).hexdigest()  # Calculate the fingerprint from the cleaned payload data


def createBankDumpRequest(device_id, bank):
    return [0xf0, MOOG_ID, VOYAGER, device_id & 0x7f, ALL_PRESETS_DUMP_REQUEST, 0xf7]


def isBankDumpFinished(messages):
    # We need just a single message
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False


def extractPatchesFromBank(message) -> List[List[int]]:
    patches = []
    if isPartOfBankDump(message):
        patch_size = 128
        data_block = unpack_sysex(message[5:-1])
        original =message[5:-1]
        back = pack_sysex(data_block)
        knobkraft.list_compare(original, back)
        data_block = data_block[3:]
        # Check for hardcoded length of bank dump
        if len(data_block) >= BANK_SIZE*patch_size:
            for i in range(128):
                patch_data = data_block[i * patch_size: (i + 1) * patch_size]
                newpatch = [0xf0, MOOG_ID, VOYAGER, message[3], SINGLE_PANEL_DUMP, i] + pack_sysex(patch_data) + [0xf7]
                print(f"Found patch {nameFromDump(newpatch)}")
                patches += newpatch
            return patches
        print("Got Moog Voyager bank dump of invalid length - data length is %d but was expected to be %d" % (
            len(data_block), (BANK_SIZE*patch_size)))
    return []


def isPartOfBankDump(message):
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1] == MOOG_ID
            and message[2] == VOYAGER
            and message[4] == ALL_PRESETS_DUMP)


def unpack_sysex(midi_data):
    result_data = []
    register = 0
    bit_count = 0

    for byte in midi_data:
        register |= byte << bit_count
        bit_count += 7

        if bit_count >= 8:
            result_data.append(register & 0xFF)
            bit_count -= 8
            register >>= 8

    return result_data


def pack_sysex(midi_data):
    result_data = []
    bit_count = 0
    next_byte = 0x0

    for this_byte in midi_data:
        result_data.append(((this_byte << bit_count) | next_byte) & 0x7F)
        next_byte = this_byte >> (7 - bit_count)

        bit_count += 1
        if bit_count == 7:
            result_data.append(next_byte & 0x7F)
            bit_count = 0
            next_byte = 0x0

    if bit_count > 0:  # Fill the last byte
        result_data.append(next_byte & 0x7F)

    return result_data


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        program_data = [0xF0, 0x04, 0x01, 0x00, 0x03, 0x00, 0x03, 0x4C, 0x1C, 0x5C, 0x11, 0x40, 0x46, 0x02, 0x19, 0x00, 0x00, 0x00, 0x40, 0x08, 0x00, 0x00, 0x00, 0x60, 0x01, 0x00, 0x00, 0x42, 0x08, 0x00, 0x68, 0x00, 0x00, 0x00, 0x41, 0x03, 0x1C, 0x3F, 0x00, 0x08, 0x42, 0x28, 0x06, 0x60, 0x17, 0x00, 0x6B, 0x40, 0x04, 0x04, 0x00, 0x10, 0x5C, 0x7F, 0x40, 0x78, 0x43, 0x62, 0x0F, 0x00, 0x20, 0x00, 0x4D, 0x00, 0x00, 0x00, 0x7D, 0x1F, 0x7C, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x78, 0x7F, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x7F, 0x03, 0x05, 0x00, 0x5C, 0x7F, 0x3F, 0x7C, 0x68, 0x76, 0x0F, 0x3F, 0x00, 0x40, 0x3F, 0x53, 0x6A, 0x41, 0x2B, 0x26, 0x0E, 0x48, 0x29, 0x61, 0x6E, 0x01, 0x01, 0x02, 0x14, 0x08, 0x10, 0x20, 0x40, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00, 0x05, 0x10, 0x06, 0x00, 0x19, 0x00, 0x66, 0x00, 0x00, 0x70, 0x1F, 0x06, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0xF7]
        yield testing.ProgramTestData(message=program_data, number=0, name='Super Saw               ')

        all_programs = extractPatchesFromBank(data.all_messages[0])
        messages = knobkraft.splitSysex(all_programs)
        yield testing.ProgramTestData(message=messages[0], name="Tasty Moog  Bass        ", number=0, second_layer_name="Bass        ")
        yield testing.ProgramTestData(message=messages[17], name="Clean       Machine     ", number=17)
        yield testing.ProgramTestData(message=messages[127], name="Stuttering  Evolution   ", number=127)

    def edit_buffers(data: testing.TestData) -> List[testing.ProgramTestData]:
        all_programs = extractPatchesFromBank(data.all_messages[0])
        messages = knobkraft.splitSysex(all_programs)
        yield testing.ProgramTestData(message=convertToEditBuffer(12, messages[0]), name="Tasty Moog  Bass        ")
        yield testing.ProgramTestData(message=convertToEditBuffer(11, messages[17]), name="Clean       Machine     ")

    def banks(data: testing.TestData) -> List[testing.ProgramTestData]:
        bank_dump = data.all_messages[0]
        assert isPartOfBankDump(bank_dump)
        yield bank_dump

    return testing.TestData(sysex="testData/Moog_Voyager/Bank_A_Tasty_Moog_Bass.syx", program_generator=programs, bank_generator=banks, edit_buffer_generator=edit_buffers, expected_patch_count=128)
