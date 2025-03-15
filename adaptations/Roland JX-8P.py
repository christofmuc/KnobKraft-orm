#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Note that for real life usage the native C++ implementation of the Matrix1000 is more powerful and should be used
# This is an example adaption to show how a fully working adaption can look like
from typing import List

import testing


def name():
    return "Roland JX-8P"


def createDeviceDetectMessage(channel):
    # The JX-8P cannot be detected - at all. There does not seem to be a single MIDI message we can send to it that it
    # will reply to. Send a meaningless empty Sysex to make our code work
    return [0xf0, 0xf7]


def deviceDetectWaitMilliseconds():
    # Negative value means don't autodetect at all
    return -1


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # Return invalid channel (-1)
    return -1


def createEditBufferRequest(channel):
    # The JX-8P has no Edit Buffer Request, but sending this will make the Orm wait for a reply
    return [0xf0, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 6
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x41  # Roland ID
            and message[2] == 0b00110101  # APR All tone parameters
            and message[4] == 0b00100001  # JX-8P
            and message[5] == 0b00100000  # Level
            and message[6] == 0b00000001  # Group
            )


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 1


def nameFromDump(message):
    if isEditBufferDump(message):
        return ''.join([chr(x) for x in message[7:17]])


def convertToEditBuffer(channel, message):
    # The JX-8P only has edit buffer messages (called All Tone Parameters).
    # To turn it into a program dump you need to append a "program number message"
    # But we need to poke the channel into position 3
    message[3] = channel if channel != -1 else 0
    return message


def make_test_data():

    def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=test_data.all_messages[0], name=" PIANO 1  ")
        for message in test_data.all_messages[1:]:
            if isEditBufferDump(message):
                yield testing.ProgramTestData(message=message)
            else:
                print(f'Ignoring unknown message from factory bank sysex {message}')

    return testing.TestData(sysex=R"testData/Roland_JX_8P_FACTORY_BANK_1-32.syx", edit_buffer_generator=make_patches, expected_patch_count=32)

