#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from typing import List, Dict

import testing
from knobkraft import unescapeSysex_deepmind

OBERHEIM_ID = 0x10
OBX8_ID = 0x58
single_program = 0x02
combi_program = 0x07


def name():
    return "Oberheim OB-X8"


def createDeviceDetectMessage(channel):
    # Unknown how to detect it
    return []


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    return 100


def channelIfValidDeviceResponse(message):
    return -1


def bankDescriptors(self) -> List[Dict]:
    return [{"bank": 0, "name": f"OB-X8", "size": 128, "type": "Single Program"},
            {"bank": 1, "name": f"OB-8", "size": 128, "type": "Single Program"},
            {"bank": 2, "name": f"OB-Xa", "size": 128, "type": "Single Program"},
            {"bank": 3, "name": f"OB-SX", "size": 128, "type": "Single Program"},
            {"bank": 4, "name": f"OB-X", "size": 128, "type": "Single Program"},
            {"bank": 5, "name": f"User", "size": 128, "type": "Single Program"},
            {"bank": 6, "name": f"Splits", "size": 128, "type": "Split Program"},
            {"bank": 7, "name": f"Doubles", "size": 128, "type": "Double Program"}
            ]


def createProgramDumpRequest(channel, patchNo):
    # Unknown how to do this
    return []


def isSingleProgramDump(message):
    return len(message) > 3 and message[0] == 0xf0 and isOberheimX8(message) and message[3] in [single_program, combi_program]


def isSingle(message):
    return message[3] == single_program


def isCombi(message):
    return message[3] == combi_program


def isSplit(message):
    return message[8] == 0x02


def isDouble(message):
    return message[8] == 0x03


def convertToProgramDump(channel, message, patchNo):
    if isSingleProgramDump(message) and isSingle(message):
        bank_no = patchNo / 0x80
        patch_no = patchNo % 0x80
        return [0xf0] + [OBERHEIM_ID, OBX8_ID] + [single_program, bank_no, patch_no] + message[6:]
    raise Exception("Only program dumps can be converted")


def numberFromDump(message):
    if isSingleProgramDump(message):
        if isSingle(message):
            # single programs have bank and patch number
            return message[4] * 0x80 + message[5]
        elif isCombi(message):
            # there are 128 splits and 128 double programs, and we put them after the single programs
            if isSplit(message):
                return 5 * 128 + message[5]
            elif isDouble(message):
                return 6 * 128 + message[5]
    raise Exception("Can't extract program number from message")


def nameFromDump(message):
    if isSingleProgramDump(message):
        data = message[6:-1]
        full_data = unescapeSysex_deepmind(data)
        # characters = [chr(x) for x in full_data]
        if isSingle(message):
            return "".join([chr(x) for x in full_data[102:102 + 13]])
        elif isCombi(message):
            return "".join([chr(x) for x in full_data[3:3 + 13]])
    return "invalid name"


def calculateFingerprint(message):
    if isSingleProgramDump(message):
        return hashlib.md5(bytearray(message[6:-1])).hexdigest()
    raise Exception("Only program dumps can be converted")


def isOberheimX8(message):
    return message[1] == OBERHEIM_ID and message[2] == OBX8_ID


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        program_string = "F0 10 58 02 04 7F 00 01 00 00 20 00 01 01 00 00 00 00 01 00 00 00 00 00 00 00 00 00 01 00 02 00 2F 00 00 01 00 01 00 00 56 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 56 00 00 00 00 00 00 00 00 3A 00 3A 64 7F 20 20 00 00 00 00 00 0C 7F 00 07 05 00 00 01 01 00 33 01 01 00 01 01 00 0C 18 24 30 04 3C 00 49 7F 00 01 7F 00 7F 7F 04 00 42 61 73 00 69 63 20 50 72 6F 67 00 72 61 6D 00 00 00 00 00 00 00 00 00 7F 00 00 00 7F 7F 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 F7"
        yield testing.ProgramTestData(message=program_string,name="Basic Program", number=0x04 * 0x80 + 0x7f)

        combi_program_string = "F0 10 58 07 00 03 00 00 02 00 44 65 66 61 00 75 6C 74 20 53 70 6C 00 69 74 00 00 00 00 00 00 00 00 02 00 00 33 01 00 01 01 01 00 0C 18 24 00 30 3C 02 56 00 00 00 00 00 0C 01 01 00 00 02 00 7F 00 02 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1F 00 3B 24 00 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 20 3C 7F 24 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 F7"
        yield testing.ProgramTestData(message=combi_program_string, name="Default Split", number=5 * 128 + 3)

        yield testing.ProgramTestData(message=data.all_messages[0], name="Jersey Girl B", number=22)

    return testing.TestData(sysex="testData/Oberheim_OBX8/OBx8-prest.syx", program_generator=programs, friendly_bank_name=(11, "B"))
