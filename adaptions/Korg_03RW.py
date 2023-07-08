#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

#
#   Built upon the Korg MS2000 file.
#
#   This works for program mode only, combination mode seems to be more complex to support
from typing import List

import knobkraft
import testing


def name():
    return "Korg 03R/W"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel & 0x0f, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    # The 03R/W doesn't seem to like to get the messages too fast, so wait a bit between messages
    # The better implementation would be probably to do a handshake implementation, as it will likely reply
    # with a 0x23 DATA LOAD COMPLETED message on a program change message?
    return 400


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    # The MS2000 answers with 15 bytes
    # +---------+------------------------------------------------+
    # | Byte[H] |                Description                     |
    # +---------+------------------------------------------------+
    # |   F0    | Exclusive Status                               |
    # |   7E    | Non Realtime Message                           |
    # |   0g    | MIDI Global Channel  ( Device ID )             |
    # |   06    | General Information                            |
    # |   02    | Identity Reply                                 |
    # |   42    | KORG ID              ( Manufacturers ID )      |
    # |   30    | 03R/W                ( Family ID   (LSB))      |
    # |   00    |                      ( Family ID   (MSB))      |
    # |   00    |                      ( Member ID   (LSB))      |
    # |   00    |                      ( Member ID   (MSB))      |
    # |   xx    |                      ( Minor Ver.  (LSB))      |
    # |   xx    |                      ( Minor Ver.  (MSB))      |
    # |   xx    |                      ( Major Ver.  (LSB))      |
    # |   xx    |                      ( Major Ver.  (MSB))      |
    # |   F7    | END OF EXCLUSIVE                               |
    # +---------+------------------------------------------------+
    if (len(message) > 9
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x42  # KORG ID
            and message[6] == 0x30  # 03R/W
            and message[7] == 0x00
            and message[8] == 0x00  # 03R/W
            and message[9] == 0x00):
        # Extract the current MIDI channel from index 2 of the message
        return message[2]
    return -1


def createEditBufferRequest(channel):
    # (2) CURRENT PROGRAM DATA DUMP REQUEST                              R
    # +----------------+--------------------------------------------------+
    # |     Byte       |             Description                          |
    # +----------------+--------------------------------------------------+
    # | F0,42,3g,30    | EXCLUSIVE HEADER                                 |
    # | 0001 0000 (10) | CURRENT PROGRAM DATA DUMP REQUEST      10H       |
    # | 1111 0111 (F7) | EOX                                              |
    # +----------------+--------------------------------------------------+
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x30, 0x10, 0xf7]


def isEditBufferDump(message):
    # +----------------+--------------------------------------------------+
    # |     Byte       |             Description                          |
    # +----------------+--------------------------------------------------+
    # | F0,42,3g,58    | EXCLUSIVE HEADER                                 |
    # | 0100 0000 (40) | CURRENT PROGRAM DATA DUMP              40H       |
    # | 0000 0000 (00) |                                                  |
    # | 0ddd dddd (dd) | Data                                  (NOTE 1,6) |
    # |     :          |  :                                               |
    # | 1111 0111 (F7) | EOX                                              |
    # +----------------+--------------------------------------------------+
    return (len(message) > 4
            and message[0] == 0xf0
            and message[1] == 0x42  # Korg
            and (message[2] & 0xf0) == 0x30
            and message[3] == 0x30  # 03R:W
            and message[4] == 0x40)  # Current Program Data Dump


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 100


def nameFromDump(message):
    if isEditBufferDump(message):
        # We need to first convert from 7 bit data to 8 bit data, before we can extract the program name
        patchData = unescapeSysex(message[5:-1])
        return ''.join([chr(x) for x in patchData[0:10]])
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        # The data structures are the same, only byte 2 seems to contain the device ID (or channel), and
        # byte 4 should be 0x40, current program data dump
        return message[0:2] + [0x030 | (channel & 0x0f), 0x30, 0x40] + message[5:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def createBankDumpRequest(channel, bank):
    # |     1C      | PROGRAM DATA DUMP REQUEST                  |
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x30, 0x1c, 0x00, 0xf7]


def isPartOfBankDump(message):
    return (len(message) > 4
            and message[0] == 0xf0
            and message[1] == 0x42  # Korg
            and (message[2] & 0xf0) == 0x30
            and message[3] == 0x30  # 03R/W
            and (message[4] == 0x40 or message[4] == 0x4c))  # Program Data dump or All Data dump


def isBankDumpFinished(messages):
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False


def extractPatchesFromBank(message):
    if isPartOfBankDump(message):
        channel = message[2] & 0x0f
        data = unescapeSysex(message[6:-1])
        # There are different files out there with different number of patches (64 or 128), plus the global data
        data_pointer = 0
        result = []
        while data_pointer + 171 < len(data):
            # Read one more patch
            next_patch = data[data_pointer:data_pointer + 172]
            next_program_dump = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x30, 0x40] + escapeSysex(next_patch) + [0xf7]
            #print("Found patch " + nameFromDump(next_program_dump))
            result = result + next_program_dump
            data_pointer = data_pointer + 172
        return result
    raise Exception("This code can only read a single message of type 'ALL DATA DUMP'")


def unescapeSysex(sysex):
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


def escapeSysex(data):
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
    testData = [x for x in range(254)]
    escaped = escapeSysex(testData)
    back = unescapeSysex(escaped)
    assert testData == back

    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patches = knobkraft.splitSysex(extractPatchesFromBank(data.all_messages[0]))
        yield testing.ProgramTestData(message=patches[0], name='AnalogFeel')

    def banks(data: testing.TestData) -> List:
        yield data.all_messages

    return testing.TestData(sysex="testData/Korg_03RW/vicbank1.syx", edit_buffer_generator=programs, bank_generator=banks)
