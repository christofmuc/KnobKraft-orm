#
#   Copyright (c) 2021-2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import itertools
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


def nameFromDump(message):
    if isSingleProgramDump(message):
        if isPerformance(message):
            name_len = 12
        elif isVoice(message):
            name_len = 10
        else:
            return "invalid"
        return "".join([chr(x) for x in dataBlockFromMessage(message)[:name_len]])
    return "Invalid"


#def renamePatch(message, new_name):
#    messages = knobkraft.splitSysex(message)
#    common_voice_data = dataBlockFromMessage(messages[1])[3:]
#    used_char = min(10, len(new_name))
#    for i in range(used_char):
#        common_voice_data[i] = ord(new_name[i])
#    for i in range(used_char, 10):
#        common_voice_data[i] = ord(" ")
#    messages[1] = buildBulkDumpMessage(0, commonVoiceAddress, common_voice_data)
#    return [item for sublist in messages for item in sublist]  # flatten again


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
#
#
# def isEditBufferDump2(data):
#     if isOwnSysex(data):
#         messages = knobkraft.splitSysexMessage(data)
#         if len(messages) != 5:
#             return False
#         all_five = [False, False, False, False, False]
#         for m in messages:
#             address = addressFromMessage(m)
#             if address == PERFORMANCE_EDIT_BUFFER_ADDRESS:
#                 all_five[0] = True
#             elif address == VOICE_EDIT_BUFFFER_PART1:
#                 all_five[1] = True
#             elif address == VOICE_EDIT_BUFFFER_PART2:
#                 all_five[2] = True
#             elif address == VOICE_EDIT_BUFFFER_PART3:
#                 all_five[3] = True
#             elif address == VOICE_EDIT_BUFFFER_PART4:
#                 all_five[4] = True
#         return all(all_five)
#     return False
#
#
# def convertToEditBuffer(channel, data):
#     if isEditBufferDump(data):
#         return data
#         #result = []
#         #messages = knobkraft.splitSysex(data)
#         #for message in messages:
#         #    # Recompose the message with the new channel in the lower 4 bits
#         #    result.extend(changeChannelInMessage(channel, message))
#         #return result
#     elif isSingleProgramDump(data):
#         #result = []
#         #messages = knobkraft.splitSysex(data)
#         #result.extend(buildBulkDumpMessage(channel, bulkHeaderAddress, []))
#         #for message in messages[1:6]:
#         #    result.extend(message)
#         #result.extend(buildBulkDumpMessage(channel, bulkFooterAddress, []))
#         #return result
#         return []
#     raise Exception("Not an edit buffer dump!")


def isSingleProgramDump(data):
    # Just one message access the whole data set
    return isPerformance(data) or isVoice(data)


def convertToProgramDump(channel, message, program_number):
    if isSingleProgramDump(message):
        new_message = message[:2] + [DEVICE_ID_DETECTED & 0x0f] + message[3:8] + [program_number % 128] + message[9:]
        assert sum(message[4:-2]) % 128 == 0
        assert sum([-m for m in message[4:-3]]) & 0x7f == message[-3]
        recalculateChecksum = sum([-m for m in new_message[4:-3]]) & 0x7f
        new_message[-3] = recalculateChecksum
        dataBlockFromMessage(new_message)
        return new_message
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


def isPerformance(message):
    return isOwnSysex(message) and addressFromMessage(message)[:2] == PERFORMANCE_ADDRESS


def isVoice(message):
    return isOwnSysex(message) and addressFromMessage(message)[:2] == VOICE_ADDRESS


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
    raise Exception("Got corrupt data block in refaceDX data")


def buildRequest(device_id, address):
    return [0xf0, YAMAHA_ID, 0x20 | (device_id & 0x0f), YAMAHA_FS1R, address[0], address[1], address[2], 0xf7]


#def changeChannelInMessage(new_channel, message):
#    return message[0:2] + [(message[2] & 0xf0) | (new_channel & 0x0f)] + message[3:]


def make_test_data():
    messages = load_midi("testData/Yamaha_FS1R/Vdfs1r01.mid")

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        raw_data = list(itertools.chain.from_iterable(test_data.all_messages))
        for d in test_data.all_messages:
            isPartOfEditBufferDump(d)
        yield testing.ProgramTestData(message=raw_data, name="Piano 1   ", rename_name="Piano 2   ")

        # Test a Soundmondo file
        message = knobkraft.stringToSyx("F0 43 00 7F 1C 00 04 05 0E 0F 00 5E F7 F0 43 00 7F 1C 00 2A 05 30 00 00 53 6E 61 70 48 61 70 70 79 20 00 00 40 00 02 42 04 00 64 00 00 40 40 40 40 40 40 40 40 03 40 40 07 40 40 00 00 00 21 F7 F0 43 00 7F 1C 00 20 05 31 00 00 01 7F 3A 24 6E 7F 56 00 00 00 00 00 00 00 00 01 01 40 7D 3C 00 00 00 00 40 00 00 00 6E F7 F0 43 00 7F 1C 00 20 05 31 01 00 01 7F 5D 23 5A 7F 3C 00 00 04 0F 06 03 00 00 01 01 3F 70 48 00 00 00 00 40 00 00 00 5F F7 F0 43 00 7F 1C 00 20 05 31 02 00 01 7F 67 3C 5A 7F 60 00 00 04 00 0B 00 00 00 01 01 5C 62 4B 00 00 00 00 40 00 00 00 12 F7 F0 43 00 7F 1C 00 20 05 31 03 00 01 7F 4A 53 5A 7F 66 00 00 08 00 07 00 00 00 01 01 78 60 7F 00 00 00 00 40 00 00 00 43 F7 F0 43 00 7F 1C 00 04 05 0F 0F 00 5D F7")
        yield testing.ProgramTestData(message=message, name="SnapHappy ", rename_name="Piano 2   ")

    def program_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        program_data = []
        for d in messages:
            if isSingleProgramDump(d):
                program_data.append(d)
        yield testing.ProgramTestData(message=program_data[0], name="HARDPNO PF  ", rename_name="Piano 2   ", number=0, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=program_data[127], name="Sitar       ", rename_name="Piano 2   ", number=127, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=program_data[128], name="HARDPNO PF", rename_name="Piano 2   ", number=128, target_no=140, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=program_data[255], name="Sitar     ", rename_name="Piano 2   ", number=255, target_no=244, friendly_number="Bank3-2")

    return testing.TestData(program_generator=program_buffers)


