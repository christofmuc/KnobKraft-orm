#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# https://github.com/eclab/edisyn/blob/master/edisyn/synth/waldorfm/WaldorfM.java#L1952
import hashlib
from copy import copy
from typing import List

import testing


WALDORF_ID = 0x3e
WALDORF_M = 0x30
REQUEST_PATCH = 0x74
SINGLE_PATCH = 0x72
BANK_SIZE = 128


def name():
    return "Waldorf M"


def createDeviceDetectMessage(device_id):
    # Just request the edit buffer - allegedly, it does not use the device id so that could be quick
    return createEditBufferRequest(device_id)


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    return 200


def channelIfValidDeviceResponse(message):
    if isEditBufferDump(message):
        return message[3] & 0x0f
    return -1


def bankDescriptors():
    return [{"bank": x, "name": f"Bank {x+1}", "size": 128, "type": "Patch"} for x in range(16)]


def createEditBufferRequest(device_id):
    return [0xf0, WALDORF_ID, WALDORF_M, device_id & 0x0f, REQUEST_PATCH, 0x00, 0x00, 0x00, 0xf7]


def isEditBufferDump(message: List[int]) -> bool:
    return (len(message) == 512
            and message[0] == 0xf0
            and message[1] == WALDORF_ID
            and message[2] == WALDORF_M
            and message[4] == SINGLE_PATCH
            and message[5] == 0x01
            and message[32] == 0x00)  # bank must be 0


def convertToEditBuffer(device_id, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        new_message = copy(message)
        new_message[32] = 0x00
        new_message[33] = 0x00
        return new_message
    raise "Can only convert edit buffers or single programs"


def createProgramDumpRequest(device_id, patchNo):
    bank = patchNo // BANK_SIZE + 1
    program = patchNo % BANK_SIZE
    return [0xf0, WALDORF_ID, WALDORF_M, device_id & 0x0f, REQUEST_PATCH, 0x00, bank & 0x7f, program & 0x7f, 0xf7]


def isSingleProgramDump(message: List[int]) -> bool:
    return (len(message) == 512
            and message[0] == 0xf0
            and message[1] == WALDORF_ID
            and message[2] == WALDORF_M
            and message[4] == SINGLE_PATCH
            and message[5] == 0x01  # Some kind of version info?
            and message[32] != 0x00)  # Bank must be non-zero


def numberFromDump(message: List[int]) -> int:
    if isSingleProgramDump(message):
        bank = message[32]
        program = message[33]
        return (bank - 1) * BANK_SIZE + program
    return -1


def nameFromDump(message: List[int]) -> str:
    if isSingleProgramDump(message) or isEditBufferDump(message):
        return ''.join([chr(x) for x in message[6:6+23]])
    return "invalid"


def convertToProgramDump(device_id, message, program_number):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        # Need to construct a new program dump from a single program dump.
        new_message =  message[0:3] + [device_id & 0x0f] + message[4:]
        new_message[32] = program_number // BANK_SIZE + 1
        new_message[33] = program_number % BANK_SIZE
        return new_message
    raise Exception("Can only convert program dumps")


def calculateFingerprint(message: List[int]):
    if isSingleProgramDump(message):
        # Blank out program position and name
        blanked_out = copy(message)
        blanked_out[32:34] = [0, 0]
        blanked_out[32:34] = [0, 0]
        blanked_out[6:6+23] = [0] * 23
        blanked_out[3] = 0x00
        return hashlib.md5(bytearray(blanked_out)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint Presets")


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], number=0, name="Wavetable El. Piano    ")

    return testing.TestData(sysex="testData/Waldorf_M/Wavetable-El.-Piano.syx", program_generator=programs)
