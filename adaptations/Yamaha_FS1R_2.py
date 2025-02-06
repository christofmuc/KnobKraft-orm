#
#   Copyright (c) 2021-2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import itertools
from copy import copy
from typing import List

import knobkraft
import testing
from knobkraft import load_midi

YAMAHA_ID=0x43
YAMAHA_FS1R=0x5e
SYSTEM_SETTINGS_ADDRESS=[0x00, 0x00, 0x00]
PERFORMANCE_EDIT_BUFFER_ADDRESS=[0x10, 0x00, 0x00]
PERFORMANCE_ADDRESS=[0x11, 0x00]  # Internal PERFORMANCE. Hi, med fixed, Lo is the number of the performance (0-127)
VOICE_EDIT_BUFFFER_PART1=[0x40, 0x00, 0x00]
VOICE_EDIT_BUFFFER_PART2=[0x41, 0x00, 0x00]
VOICE_EDIT_BUFFFER_PART3=[0x42, 0x00, 0x00]
VOICE_EDIT_BUFFFER_PART4=[0x43, 0x00, 0x00]
VOICE_ADDRESS=[0x51, 0x00]  # Internal VOICE

DEVICE_ID_DETECTED=0x00

class DataBlock:
    def __init__(self, address, size, name):
        pass

fs1r_performance_data = [DataBlock((0x10, 0x00, 0x00), 80, "Performance Common"), # The first 12 bytes are the name
                         DataBlock((0x10, 0x00, 0x50), 112, "Effect"),
                         DataBlock((0x30, 0x00, 0x00), 52, "Part 1"),
                         DataBlock((0x31, 0x00, 0x00), 52, "Part 2"),
                         DataBlock((0x32, 0x00, 0x00), 52, "Part 3"),
                         DataBlock((0x33, 0x00, 0x00), 52, "Part 4")]

fs1_all_performance_data = [DataBlock((0x11, 0x00, 0x00), 400, "Performance")]


fs1r_voice_data = [DataBlock((0x40, 0x00, 0x00), 112, "Voice Common")] + \
                  [DataBlock((0x60, op, 0x00), 35, "Operator 1 Voiced") for op in range(8)] + \
                  [DataBlock((0x60, op, 0x23), 27, "Operator 1 Non-Voiced") for op in range(8)] + \
                  [DataBlock((0x61, op, 0x00), 35, "Operator 2 Voiced") for op in range(8)] + \
                  [DataBlock((0x61, op, 0x23), 27, "Operator 2 Non-Voiced") for op in range(8)] + \
                  [DataBlock((0x62, op, 0x00), 35, "Operator 3 Voiced") for op in range(8)] + \
                  [DataBlock((0x62, op, 0x23), 27, "Operator 3 Non-Voiced") for op in range(8)] + \
                  [DataBlock((0x63, op, 0x00), 35, "Operator 4 Voiced") for op in range(8)] + \
                  [DataBlock((0x64, op, 0x23), 27, "Operator 4 Non-Voiced") for op in range(8)]

fs1r_fseq_data = [DataBlock((0x70, 0x00, 0x00), 0x00, "Fseq parameter")]  # 8 bytes of name at the start, byte count not used in here

fs1r_system_data = [DataBlock((0x00, 0x00, 0x00), 0x4c, "System Parameter")]


def name():
    return "Yamaha FS1R"


def createDeviceDetectMessage(device_id):
    # Just send a request for the system settings address
    return buildRequest(device_id, SYSTEM_SETTINGS_ADDRESS)


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    global DEVICE_ID_DETECTED
    if isOwnSysex(message):
        if addressFromMessage(message) == SYSTEM_SETTINGS_ADDRESS:
            data_block = dataBlockFromMessage(message)
            channel = data_block[9]  # data_block[9] is the performance channel
            if channel == 0x10:
                print("Warning: Your FS1R is set to OMNI MIDI channel. Surprises await!")
                return 16
            DEVICE_ID_DETECTED=message[2] & 0x0f
            return channel
    return -1


def bankDescriptors():
    return [{"bank": 0, "name": "Performances", "size": 128, "type": "Performance", "isROM": False, },
            {"bank": 1, "name": "Voices", "size": 128, "type": "Voice", "isROM": False, }]


def _nameLength(message):
    if isPerformance(message):
        return 12
    elif isVoice(message):
        return 10
    else:
        raise Exception("neither a performance nor a voice")


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        name_len = _nameLength(message)
        return "".join([chr(x) for x in dataBlockFromMessage(message)[:name_len]])
    return "Invalid"


def renamePatch(message, new_name):
    if isSingleProgramDump(message):
        name_len = _nameLength(message)
        name_list = [ord(x) for x in new_name.ljust(name_len, " ")]
        result = copy(message)
        result[9:9+name_len] = name_list
        return recalculateChecksum(result)
    raise Exception("can only rename single program dumps!")


# def createEditBufferRequest(channel):
#     # How do we request the Voices? Just send all requests at once?
#     return buildRequest(channel, PERFORMANCE_EDIT_BUFFER_ADDRESS) + \
#            buildRequest(channel, VOICE_EDIT_BUFFFER_PART1) + \
#            buildRequest(channel, VOICE_EDIT_BUFFFER_PART2) + \
#            buildRequest(channel, VOICE_EDIT_BUFFFER_PART3) + \
#            buildRequest(channel, VOICE_EDIT_BUFFFER_PART4)
#
#
# def isPartOfEditBufferDump(message):
#     # Accept a certain set of addresses
#     if isOwnSysex(message):
#         address = addressFromMessage(message)
#         return address in [PERFORMANCE_EDIT_BUFFER_ADDRESS, VOICE_EDIT_BUFFFER_PART1, VOICE_EDIT_BUFFFER_PART2, VOICE_EDIT_BUFFFER_PART3, VOICE_EDIT_BUFFFER_PART4]
#     return False


def isEditBufferDump(data):
    if isOwnSysex(data):
        address = addressFromMessage(data)
        return address == PERFORMANCE_EDIT_BUFFER_ADDRESS or address == VOICE_EDIT_BUFFFER_PART1
    return False


def convertToEditBuffer(channel, data):
    if isEditBufferDump(data):
        return data
    elif isSingleProgramDump(data):
        program = copy(data)
        if isVoice(data):
            setAddress(program, VOICE_EDIT_BUFFFER_PART1)
        elif isPerformance(data):
            setAddress(program, PERFORMANCE_EDIT_BUFFER_ADDRESS)
        return recalculateChecksum(program)
    raise Exception("Can't convert to edit buffer dump!")


def isSingleProgramDump(data):
    # Just one message access the whole data set
    return isPerformance(data) or isVoice(data)


def convertToProgramDump(channel, message, program_number):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        new_message = message[:2] + [DEVICE_ID_DETECTED & 0x0f] + message[3:8] + [program_number % 128] + message[9:]
        if isVoice(message):
            setAddress(new_message, VOICE_ADDRESS + [program_number & 0x7f])
        elif isPerformance(message):
            setAddress(new_message, PERFORMANCE_ADDRESS + [program_number & 0x7f])
        return recalculateChecksum(new_message)
    raise Exception("Can only convert single program dumps to program dumps")


def createProgramDumpRequest(channel, patch_no):
    if 0 <= patch_no < 128:
        # Request performance
        return buildRequest(DEVICE_ID_DETECTED, PERFORMANCE_ADDRESS + [patch_no])
    elif 128 <= patch_no < 256:
        # Request voice
        return buildRequest(DEVICE_ID_DETECTED, VOICE_ADDRESS + [patch_no])
    raise Exception("Can only request 128 performances or 128 voices")


#def bankDownloadMethodOverride():
#    return "EDITBUFFERS"


# def buildBulkDumpMessage(deviceID, address, data):
#     message = [0xf0, 0x43, 0x00 | deviceID, 0x7f, 0x1c, 0, 0, 0x05, address[0], address[1], address[2]] + data
#     data_len = len(data) + 4  # Include address and checksum in data size
#     size_high = (data_len >> 7) & 0x7f
#     size_low = data_len & 0x7f
#     message[5] = size_high
#     message[6] = size_low
#     check_sum = 0
#     for m in message[7:]:
#         check_sum -= m
#     message.append(check_sum & 0x7f)
#     message.append(0xf7)
#     return message


#def calculateFingerprint(message):
#    for i in range(10):
#        message[i] = 0
#    return hashlib.md5(bytearray(message)).hexdigest()


def isOwnSysex(message):
    return (len(message) > 7
            and message[0] == 0xf0  # Sysex
            and message[1] == YAMAHA_ID
            and message[3] == YAMAHA_FS1R)


def numberFromDump(message):
    if isPerformance(message):
        return addressFromMessage(message)[2]  # The lo address is the performance number
    elif isVoice(message):
        return addressFromMessage(message)[2] + 128
    raise Exception("Can only extract program number from Performances and Voices!")


def addressFromMessage(message):
    if isOwnSysex(message) and len(message) > 8:
        return [message[6], message[7], message[8]]
    raise Exception("Got invalid data block")


def setAddress(message, address):
    message[6:9] = address


def isPerformance(message):
    return isOwnSysex(message) and addressFromMessage(message)[:2] == PERFORMANCE_ADDRESS or addressFromMessage(message) == PERFORMANCE_EDIT_BUFFER_ADDRESS


def isVoice(message):
    return isOwnSysex(message) and addressFromMessage(message)[:2] == VOICE_ADDRESS or addressFromMessage(message) == VOICE_EDIT_BUFFFER_PART1


def dataBlockFromMessage(message):
    if isOwnSysex(message):
        data_len = message[4] << 7 | message[5]
        if len(message) == data_len + 12:
            # The Check-sum is the value that results in a value of 0 for the
            # lower 7 bits when the Model ID, Start Address, Data and Check sum itself are added.
            checksum_block = message[0x04:-2]
            if (sum(checksum_block) & 0x7f) == 0:
                # return "Data" block
                data_block =  message[0x09:-3]
                assert len(data_block) == data_len
                return data_block
    raise Exception("Got corrupt data block")


def recalculateChecksum(message):
    if isOwnSysex(message):
        data_len = message[4] << 7 | message[5]
        if len(message) == data_len + 12:
            changed = copy(message)
            checksum_block = message[0x04:-3]
            checksum = sum([-x for x in checksum_block]) & 0x7f
            changed[-3] = checksum
            return changed
    raise Exception("Got corrupt data block")


def buildRequest(device_id, address):
    return [0xf0, YAMAHA_ID, 0x20 | (device_id & 0x0f), YAMAHA_FS1R, address[0], address[1], address[2], 0xf7]


#def changeChannelInMessage(new_channel, message):
#    return message[0:2] + [(message[2] & 0xf0) | (new_channel & 0x0f)] + message[3:]


def make_test_data():
    messages = load_midi("testData/Yamaha_FS1R/Vdfs1r01.mid")
    program_data = []
    for d in messages:
        if isSingleProgramDump(d):
            program_data.append(d)

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        edit1 = convertToEditBuffer(0x01, program_data[0])
        yield testing.ProgramTestData(message=edit1, name="HARDPNO PF  ", rename_name="Piano 2   ")
        edit2 = convertToEditBuffer(0x01, program_data[255])
        yield testing.ProgramTestData(message=edit2, name="Sitar     ", target_no=244, rename_name="Piano 2   ")

    def program_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=program_data[0], name="HARDPNO PF  ", rename_name="Piano 2   ", number=0, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=program_data[127], name="Sitar       ", rename_name="Piano 2   ", number=127, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=program_data[128], name="HARDPNO PF", rename_name="Piano 2   ", number=128, target_no=140, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=program_data[255], name="Sitar     ", rename_name="Piano 2   ", number=255, target_no=244, friendly_number="Bank3-2")

    return testing.TestData(program_generator=program_buffers, edit_buffer_generator=edit_buffers)


