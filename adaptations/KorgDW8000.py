#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import re
from typing import List

import testing


def name():
    return "Korg DW 8000"


def createDeviceDetectMessage(channel):
    # Page 5 of the service manual - Device ID Request
    return [0xf0, 0x42, 0x40 | (channel % 16), 0xf7]


def deviceDetectWaitMilliseconds():
    return 100


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    # Page 3 of the service manual - Device ID
    if (len(message) > 3
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x42  # Korg
            and (message[2] & 0xf0) == 0x30  # Device ID
            and message[3] == 0x03):  # DW-8000
        return message[2] & 0x0f
    return -1


def createEditBufferRequest(channel):
    # Page 5 - Data Save Request
    return [0xf0, 0x42, 0x30 | (channel % 16), 0x03, 0x10, 0xf7]


def isEditBufferDump(message):
    if isLegacyFormat(message):
        return True
    # Page 3 - Data Dump
    return (len(message) > 4
            and message[0] == 0xf0
            and message[1] == 0x42  # Korg
            and (message[2] & 0xf0) == 0x30  # Format, ignore MIDI Channel in lower 4 bits
            and message[3] == 0x03  # DW-8000
            and message[4] == 0x40  # Data Dump
            )


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 64


def convertToEditBuffer(channel, message):
    if isLegacyFormat(message):
        return convertFromLegacyFormat(channel, message)
    elif isEditBufferDump(message):
        return message[0:2] + [0x30 | channel] + message[3:]
    raise Exception("This is not an edit buffer - can't be converted")


def friendlyProgramName(program):
    return "%d%d" % (program // 8 + 1, (program % 8) + 1)


def program_change(channel, program_number):
    return [0xc0 | (channel & 0x0f), program_number]


def createStoreToProgramMessage(channel, program_number):
    # 0x11 is the WRITE_REQUEST message
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x03, 0x11, program_number & 0x3f, 0xf7]


def createProgramDumpRequest(channel, patchNo):
    return program_change(channel, patchNo) + createEditBufferRequest(channel)


def isSingleProgramDump(message):
    # Single Program Dumps are just edit buffer dumps
    return isEditBufferDump(message)


def convertToProgramDump(channel, message, program_number):
    if isEditBufferDump(message):
        return message + createStoreToProgramMessage(channel, program_number)
        #return program_change(channel, program_number) + message + createStoreToProgramMessage(channel, program_number)
    raise Exception("Can only convert edit buffers!")


def isLegacyFormat(message):
    return len(message) == 51


def convertFromLegacyFormat(channel, message):
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x03, 0x40] + message + [0xf7]


def convertToLegacyFormat(message):
    return message[5:-1]


def calculateFingerprint(message):
    # To be backwards compatible with the old C++ implementation of the Korg DW 8000 implementation,
    # calculate the fingerprint only across the data block
    if not isLegacyFormat(message):
        message = message[5:-1]
    return hashlib.md5(bytearray(message)).hexdigest()


def isDefaultName(patchName):
    return re.match("[1-8][1-8]", patchName) is not None


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patch = [x for sublist in data.all_messages[0:2] for x in sublist]
        yield testing.ProgramTestData(message=patch)

    assert friendlyProgramName(0) == "11"
    return testing.TestData(sysex="testData/KorgDW8000_bank_a.syx", edit_buffer_generator=programs, friendly_bank_name=(63, "88"))
