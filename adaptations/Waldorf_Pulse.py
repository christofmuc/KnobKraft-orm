#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from copy import copy
from typing import List

import testing

WALDORF_ID = 0x3E
PULSE_ID = 0x0B
BROADCAST_DEVICE_ID = 127
SINGLE_PROGRAM_DUMP_REQUEST = 0x40
BULK_PROGRAM_DUMP_REQUEST = 0x41
SINGLE_PROGRAM_DUMP = 0x00
BULK_PROGRAM_DUMP = 0x01
GLOBAL_PARAMETER_REQUEST = 0x48
GLOBAL_PARAMETER_DUMP = 0x08


def name():
    return "Waldorf Pulse"


def createDeviceDetectMessage(device_id):
    return [0xF0, WALDORF_ID, PULSE_ID, BROADCAST_DEVICE_ID, GLOBAL_PARAMETER_REQUEST, 0x00, 0xF7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if len(message) > 5 and message[:5] == [0xF0, WALDORF_ID, PULSE_ID, message[3], GLOBAL_PARAMETER_DUMP]:
        # Actually message[9] contains the device_id again
        # The main problem is that is out of range for the Knobkraft Orm software, so we encode all our send commands with the broadcast ID for now
        return message[9]
    return -1


def bankDescriptors():
    return [{"bank": 0, "name": "RAM", "size": 40, "type": "Patch", "isROM": False },
            {"bank": 1, "name": "ROM", "size": 40+19, "type": "Patch", "isROM": True }]


def createProgramDumpRequest(device_id, program_no):
    return [0xF0, WALDORF_ID, PULSE_ID, BROADCAST_DEVICE_ID, SINGLE_PROGRAM_DUMP_REQUEST, program_no & 0x7f, 0xF7]


def isSingleProgramDump(message):
    return len(message) > 5 and message[:3] == [0xF0, WALDORF_ID, PULSE_ID] and message[4] in [SINGLE_PROGRAM_DUMP, BULK_PROGRAM_DUMP]


def convertToProgramDump(device_id, message, program_no):
    if isSingleProgramDump(message):
        # If I understand the German manual correctly, the normal SINGLE_PROGRAM_DUMP is rather an edit buffer dump, it will not be stored
        # We can only store into the first 40 programs
        return message[:3] + [BROADCAST_DEVICE_ID, BULK_PROGRAM_DUMP, program_no % 40] + message[6:]
    raise Exception("Can only convert singleProgramDumps")


def numberFromDump(message):
    if isSingleProgramDump(message):
        return message[5]
    raise Exception("Only single program dumps have program numbers")


def calculateFingerprint(message):
    if isSingleProgramDump(message):
        blanked_out = copy(message)
        blanked_out[3] = 0  # Ignore Device ID
        blanked_out[4] = SINGLE_PROGRAM_DUMP  # Hardcode to single program dump, to not confuse Bulk programs and single programs
        blanked_out[5] = 0  # Ignore Program Position
        return hashlib.md5(bytes(blanked_out)).hexdigest()
    raise Exception("Can only fingerprint singleProgramDumps")


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        # This bank has no patch numbers, they are all 0
        yield testing.ProgramTestData(message=data.all_messages[0], number=0)

    return testing.TestData(sysex="testData/Waldorf_Pulse/waldorf_pulse_test.syx", program_generator=programs)
