#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

from typing import List

import knobkraft
import testing

KORG = 0x42
M1 = 0x19

PROGRAM_PARAMETER_DUMP_REQUEST = 0x10
PROGRAM_PARAMETER_DUMP = 0x40
ALL_PROGRAM_PARAMETER_DUMP_REQUEST = 0x1c  # This is the bank request
ALL_PROGRAM_PARAMETER_DUMP = 0x4c  # A bank of programs
ALL_DATA_DUMP = 0x50  # All data from the synth, including a bank of programs


def name():
    return "Korg M1"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel & 0x0f, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    return 200


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 7
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == KORG  # KORG ID
            and message[6] == M1):  # M1 ID
        # Extract the current MIDI channel from index 2 of the message
        return message[2] & 0x0f
    return -1


def createEditBufferRequest(device_id):
    return [0xf0, KORG, 0x30 | (device_id & 0x0f), M1, PROGRAM_PARAMETER_DUMP_REQUEST, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 4
            and message[0] == 0xf0
            and message[1] == KORG
            and (message[2] & 0xf0) == 0x30
            and message[3] == M1
            and message[4] == PROGRAM_PARAMETER_DUMP)


def bankDescriptors():
    return [{"bank": 0, "name": f"RAM", "size": 64, "type": "Patch"}, {"bank": 1, "name": f"Card", "size": 100, "type": "Patch"}]


def nameFromDump(message):
    if isEditBufferDump(message):
        patchData = unescapeSysex(message[5:-1])
        return ''.join([chr(x) for x in patchData[0:10]])
    return "Invalid"


def renamePatch(message: List[int], new_name: str) -> List[int]:
    if isEditBufferDump(message):
        new_name_list = [ord(c) for c in new_name.ljust(10, " ")]
        data = unescapeSysex(message[5:-1])
        return message[:5] + escapeSysex(new_name_list + data[10:]) + [0xf7]
    raise Exception("Can only rename program data dumps!")


def convertToEditBuffer(device_id, message):
    if isEditBufferDump(message):
        return message[0:2] + [0x30 | (device_id & 0x0f)] + message[3:]
    raise Exception("Not an edit buffer, can't be converted")


def createBankDumpRequest(device_id, bank):
    return [0xf0, KORG, 0x30 | (device_id & 0x0f), M1, ALL_PROGRAM_PARAMETER_DUMP_REQUEST, bank, 0xf7]


def isPartOfBankDump(message):
    return (len(message) > 4
            and message[0] == 0xf0
            and message[1] == KORG
            and (message[2] & 0xf0) == 0x30
            and message[3] == M1
            and (message[4] == ALL_DATA_DUMP or message[4] == ALL_PROGRAM_PARAMETER_DUMP))


def isBankDumpFinished(messages):
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False


def extractPatchesFromBank(message):
    if isPartOfBankDump(message):
        device_id = message[2] & 0x0f
        memory_format = message[5]
        data = unescapeSysex(message[6:-1])
        result = []

        if message[4] == ALL_PROGRAM_PARAMETER_DUMP:
            data_pointer = 0
            data_increment = 143
            while data_pointer + data_increment < len(data):
                # Read one more patch
                next_patch = data[data_pointer:data_pointer + data_increment]
                next_program_dump = [0xf0, KORG, 0x30 | (device_id & 0x0f), M1, PROGRAM_PARAMETER_DUMP] + escapeSysex(next_patch) + [0xf7]
                print("Found patch " + nameFromDump(next_program_dump))
                result = result + next_program_dump
                data_pointer = data_pointer + data_increment
        return result
    raise Exception("This code can only read a single message of type 'PROGRAM PARAMETER DUMP'")


def unescapeSysex(sysex: List[int]) -> List[int]:
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        # The first byte contains the highest (most significant = MS) bit of the next seven data bytes
        ms_bits = sysex[dataIndex]
        dataIndex += 1
        for i in range(7):
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex] | ((ms_bits & (1 << i)) << (7 - i)))
            dataIndex += 1
    return result


def escapeSysex(data: List[int]) -> List[int]:
    result = []
    dataIndex = 0
    while dataIndex < len(data):
        ms_bits = 0
        for i in range(7):
            if dataIndex + i < len(data):
                ms_bits = ms_bits | ((data[dataIndex + i] & 0x80) >> (7 - i))
        result.append(ms_bits)
        for i in range(7):
            if dataIndex + i < len(data):
                result.append(data[dataIndex + i] & 0x7f)
        dataIndex += 7
    return result


def make_test_data():
    def programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        bank = knobkraft.load_sysex("testData/Korg_M1/bank21.syx")
        patches = knobkraft.splitSysex(extractPatchesFromBank(bank[0]))
        yield testing.ProgramTestData(message=patches[0], name="Grandbient")
        yield testing.ProgramTestData(message=patches[49], name="ToyNFlt   ")

    def banks(test_data: testing.TestData) -> List:
        yield test_data.all_messages[0]

    return testing.TestData(sysex="testData/Korg_M1/bank21.syx", edit_buffer_generator=programs, bank_generator=banks, device_detect_reply=("F0 7E 00 06 02 42 19 00 08 00 07 01 01 00 F7", 0))
