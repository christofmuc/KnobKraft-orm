#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# https://github.com/eclab/edisyn/blob/master/edisyn/synth/waldorfm/WaldorfM.java#L1952
import hashlib
from copy import copy
from typing import List, Optional

import testing


WALDORF_ID = 0x3e
WALDORF_M = 0x30
REQUEST_PATCH = 0x74
SINGLE_PATCH = 0x72
BANK_SIZE = 128

DEVICE_ID_DETECTED: Optional[int] = None  # If auto detection works, we can save the real device ID here


def name():
    return "Waldorf M"


def createDeviceDetectMessage(device_id):
    # Just request the edit buffer - allegedly, it does not use the device id so that could be quick
    # The parameter is ignored, it defaults to 0x7f 127 when it hasn't been detected yet
    return createEditBufferRequest(127)


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    return 1000


def channelIfValidDeviceResponse(message):
    global DEVICE_ID_DETECTED
    if isEditBufferDump(message):
        DEVICE_ID_DETECTED = message[3]
        return message[3] & 0x0f
    return -1


def _device_id():
    return DEVICE_ID_DETECTED if DEVICE_ID_DETECTED is not None else 0x7f


def bankDescriptors():
    return [{"bank": x, "name": f"Bank {x:02d}", "size": 128, "type": "Patch"} for x in range(16)]


def createEditBufferRequest(device_id):
    return [0xf0, WALDORF_ID, WALDORF_M, _device_id(), REQUEST_PATCH, 0x00, 0x00, 0x00, 0xf7]


def isEditBufferDump(message: List[int]) -> bool:
    return (len(message) == 512
            and message[0] == 0xf0
            and message[1] == WALDORF_ID
            and message[2] == WALDORF_M
            and message[4] == SINGLE_PATCH
            and message[5] == 0x01)


def convertToEditBuffer(device_id, message):
    if isEditBufferDump(message):
        new_message = copy(message)
        new_message[3] = _device_id()
        new_message[32] = 0x00
        new_message[33] = 0x00
        new_message[34] = 0x01  # Setting this to 1 says "Do not save"
        new_message[35] = 0x00
        return new_message
    raise Exception("Can only convert edit buffers or single programs")


def createProgramDumpRequest(device_id, patchNo):
    bank = (patchNo // BANK_SIZE) + 1
    program = patchNo % BANK_SIZE
    return [0xf0, WALDORF_ID, WALDORF_M, _device_id(), REQUEST_PATCH, 0x00, bank & 0x7f, program & 0x7f, 0xf7]


def isSingleProgramDump(message: List[int]) -> bool:
    return isEditBufferDump(message)


def nameFromDump(message: List[int]) -> str:
    if isEditBufferDump(message):
        return ''.join([chr(x) for x in message[6:6+23]])
    return "invalid"


def convertToProgramDump(device_id, message, program_number):
    if isEditBufferDump(message):
        # Need to construct a new program dump from a single program dump.
        new_message =  message[0:3] + [_device_id()] + message[4:]
        new_message[32] = (program_number // BANK_SIZE) + 1
        new_message[33] = program_number % BANK_SIZE
        new_message[34] = 0x00  # Setting this to 0 allows to save it in the synth
        new_message[35] = 0x00
        return new_message
    raise Exception("Can only convert program dumps")


def calculateFingerprint(message: List[int]):
    if isEditBufferDump(message):
        # Blank out program position and name
        blanked_out = copy(message)
        blanked_out[32:35] = [0, 0]
        blanked_out[32:35] = [0, 0]
        blanked_out[6:6+23] = [0] * 23
        blanked_out[3] = 0x00
        return hashlib.md5(bytearray(blanked_out)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint Presets")


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], number=0, name="Wavetable El. Piano    ")

    def editbuffers(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], number=0, name="Wavetable El. Piano    ")
        editBuffer = "F0 3E 30 00 72 01 20 59 6F 75 27 76 65 20 67 6F 74 20 69 74 21 20 20 20 20 20 20 20 20 20 20 00 00 00 00 00 00 40 00 40 7F 3F 02 40 00 40 0A 40 03 40 01 40 14 40 " \
                "03 40 0A 40 00 40 00 40 00 40 00 40 00 40 01 40 02 40 00 40 0A 40 03 40 01 40 14 40  04 40 0A 40 00 40 00 40 00 40 00 40 00 40 09 40 32 40  01 40 5D 3F 00 40 00 40 03 40 01 40 00 40 03 40 00 40  01 40 00 40 09 40 32 40 01 40 " \
                "5D 3F 00 40 00 40 03 40 01 40 00 40 03 40 00 40 01 40  00 40 00 40 40 40 40 40 00 40 00 40 00 40 00 40 00 40  00 40 00 40 00 40 00 40 00 40 22 40 00 40 39 40 14 40  00 40 3F 40 03 40 01 40 00 40 " \
                "04 40 05 40 03 40 00 40 5A 40 3F 40 00 40 00 40 03 40  01 40 00 40 03 40 00 40 00 40 03 40 00 40 36 40 5A 40  00 40 3C 40 19 40 00 40 18 40 00 40 1B 40 00 40 1A 40  00 40 01 40 02 40 06 40 41 40 " \
                "58 40 00 40 41 40 03 40 00 40 19 40 00 40 18 40 00 40  1B 40 00 40 1A 40 00 40 01 40 01 40 5F 40 7F 40 50 40  7F 40 7F 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40  00 40 00 40 00 40 00 40 00 40 " \
                "00 40 00 40 00 40 00 40 00 40 00 40 06 40 00 40 00 40  00 40 00 40 00 40 7F 40 00 40 00 40 00 40 00 40 00 40  00 40 1E 40 00 40 00 40 00 40 19 40 00 40 1E 40 00 40  00 40 00 40 7F 40 00 40 0A 40 " \
                "00 40 00 40 00 40 00 40 00 40 00 40 20 40 01 40 00 40  01 40 00 40 00 40 28 40 00 40 00 40 00 40 00 40 00 40  00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40  00 40 00 40 00 40 00 40 00 40 " \
                "00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40  00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40  00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40  00 00 00 00 00 40 00 40 7F F7 "
        yield testing.ProgramTestData(message=editBuffer, number = 0, name = " You've got it!        ")
        program = "f0 3e 30 00 72 01 4a 6f 75 72 6e 65 79 20 74 6f 20 4d 20 56 53 20 20 20 20 20 20 20 20 20 20 00 00 00 00 00 00 40 00 40 00 40 04 40 00 40 0a 40 03 40 03 40 00 40 03 40 0b 40 00 40 00 40 00 40 01 40 00 40 04 40 04 40 00 40 0a 40 03 40 03 40 00 40 03 40 09 40 00 40 00 40 00 40 00 40 00 40 0c 40 00 40 01 40 2d 40 00 40 00 40 03 40 03 40 00 40 03 40 00 40 01 40 00 40 19 40 3a 40 01 40 6f 3f 00 40 00 40 03 40 03 40 00 40 03 40 00 40 00 40 00 40 00 40 40 40 40 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 48 40 00 40 03 40 02 40 2e 40 00 40 03 40 03 40 00 40 03 40 00 40 1a 40 7c 3f 38 40 12 40 3e 40 00 40 03 40 03 40 00 40 03 40 00 40 00 40 03 40 0b 40 1d 40 5d 40 68 40 46 40 03 40 00 40 00 40 00 40 03 40 00 40 03 40 00 40 01 40 10 40 00 40 1d 40 3d 40 0f 40 7f 40 03 40 00 40 03 40 00 40 03 40 00 40 03 40 00 40 03 40 00 40 01 40 01 40 1d 40 7f 40 45 40 00 40 46 40 7f 40 00 40 00 40 01 40 00 40 45 40 7f 40 45 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 02 40 01 40 00 40 06 40 00 40 00 40 00 40 00 40 00 40 7f 40 00 40 00 40 00 40 00 40 00 40 00 40 15 40 00 40 00 40 00 40 19 40 1e 40 06 40 00 40 01 40 00 40 7f 40 00 40 5f 40 00 40 3f 40 07 40 00 40 00 40 00 40 00 40 01 40 00 40 00 40 00 40 00 40 28 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 40 00 00 00 00 00 40 00 40 7f f7"
        yield testing.ProgramTestData(message=program, name="Journey to M VS        ")

    return testing.TestData(sysex="testData/Waldorf_M/Wavetable-El.-Piano.syx", program_generator=programs, edit_buffer_generator=editbuffers, friendly_bank_name=(7, "Bank 07"))
