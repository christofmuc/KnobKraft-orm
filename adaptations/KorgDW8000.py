#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import List

import testing


def name():
    return "Korg DW-8000 Adaptation"


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
    if isEditBufferDump(message):
        return message[0:2] + [0x30 | channel] + message[3:]
    raise Exception("This is not an edit buffer - can't be converted")


def friendlyProgramName(program):
    return "%d%d" % (program // 8 + 1, (program % 8) + 1)


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patch = [x for sublist in data.all_messages[0:2] for x in sublist]
        yield testing.ProgramTestData(message=patch)

    assert friendlyProgramName(0) == "11"
    return testing.TestData(sysex="testData/KorgDW8000_bank_a.syx", edit_buffer_generator=programs, friendly_bank_name=(63, "88"))
