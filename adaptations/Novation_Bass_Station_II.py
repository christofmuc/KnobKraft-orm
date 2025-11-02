#   Copyright (c) 2021-2023 Christof Ruch. All rights reserved.
#   Novation Bass Station II Adaptation version 0.1 by Stefan Ott, 2025.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
# based on the Novation A Station adaption
#
import hashlib
from typing import List

import testing

def wrapSysex(data):
    # https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/bsii2.5newfeaturesuserguide.pdf
    return [0xf0,  # Sysex message
            0x00, 0x20, 0x29, # Novation
            0x00, 0x33, # Bass Station II
            0x00, # Protocol Version
            ] + data + [0xf7]  # EOX


def name():
    return "Novation Bass Station II"


def createDeviceDetectMessage(channel):
    return wrapSysex([0x44])


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    return 100


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) >= 10
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x00  # Novation
            and message[2] == 0x20  # Novation ID1
            and message[3] == 0x29  # Novation ID2
            and message[4] == 0x00
            and message[5] == 0x33
            and message[6] == 0x00
            and message[7] == 0x04):
        return message[9] - 1  # Response is one-based
    return -1


def createEditBufferRequest(channel):
    # Request for SysEx
    # https://github.com/francoisgeorgy/BS2-SysEx
    return wrapSysex([0x40])


def isEditBufferDump(message):
    return (len(message) > 13
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x00  # Novation
            and message[2] == 0x20  # Novation ID1
            and message[3] == 0x29  # Novation ID2
            and message[4] == 0x00
            and message[5] == 0x33
            and message[6] == 0x00
            and message[7] == 0x00
            and message[8] == 0x00
            )


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, patchNo):
    return wrapSysex([0x41, patchNo])


def isSingleProgramDump(message):
    return (len(message) > 13
            and message[0] == 0xf0
            and message[1] == 0x00  # Novation
            and message[2] == 0x20  # Novation ID1
            and message[3] == 0x29  # Novation ID2
            and message[4] == 0x00
            and message[5] == 0x33
            and message[6] == 0x00
            and message[7] == 0x01
            )


def numberFromDump(message):
    if not isSingleProgramDump(message):
        return -1

    return message[8]


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        # Clear Bank / Slot from message
        return message[:7] + [0, 0] + message[9:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return message[:7] + [1, program] + message[9:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def nameFromDump(message):
    if (isEditBufferDump(message) or isSingleProgramDump(message)) and len(message) >= 154:
        name = ''.join([chr(x) for x in message[137:153]])
        if name.isspace():
            return "(Untitled)"
        else:
            return name.strip()

    return "Invalid"


def make_test_data():

    def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        data = "f0 00 20 29 00 33 00 00 00 00 00 00 00 01 2c 7a 47 00 40 6b 44 04 02 00 02 20 10 10 08 00 01 00 43 40 20 00 03 7b 2a 4a 00 00 00 00 02 08 18 1e 00 20 00 0d 47 03 7c 00 40 00 1b 0e 07 78 00 00 00 00 06 03 10 00 00 00 00 0a 46 20 00 20 04 00 1f 19 16 08 04 02 01 14 40 20 1f 70 58 03 11 00 40 40 0f 68 04 02 01 00 40 2f 40 00 00 03 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 50 6f 69 73 6f 6e 20 20 20 20 20 20 20 20 20 20 f7"
        yield testing.ProgramTestData(message=data, name="Poison")

    def make_programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=test_data.all_messages[0], name="Anabass 1", number=0)
        yield testing.ProgramTestData(message=test_data.all_messages[70], name="INIT PATCH", number=70)


    return testing.TestData(sysex="testData/NovationBassStationII_FactoryPack.syx",
                            expected_patch_count=128,
                            program_generator=make_programs,
                            edit_buffer_generator=make_patches,
                            device_detect_call="f0 00 20 29 00 33 00 44 f7",
                            device_detect_reply=("f0 00 20 29 00 33 00 04 00 0a 40 00 05 01 00 08 00 f7", 9))
