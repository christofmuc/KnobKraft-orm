#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from typing import List

import testing

EMU_ID = 0x18
PROTEUS_ID = 0x04  # Proteus-specific identifier
COMMAND_PRESET_REQUEST = 0x00
COMMAND_PRESET_DATA = 0x01
COMMAND_VERSION_REQUEST = 0x0a
COMMAND_VERSION_DATA = 0x0b


def name():
    return "E-mu Proteus"


def createDeviceDetectMessage(device_id):
    # Use this command to determine the model (version code) and the
    # software revision. different models and revisions may have more or
    # less features, so use the command to verify what machine is being
    # controlled.
    return [0xf0, EMU_ID, PROTEUS_ID, device_id & 0x0f, COMMAND_VERSION_REQUEST, 0xf7]


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    # Better safe than sorry, let's start with a 300 ms delay
    return 300


def channelIfValidDeviceResponse(message):
    if (len(message) > 5
            and message[0:3] == [0xf0, EMU_ID, PROTEUS_ID]
            and message[4] == COMMAND_VERSION_DATA):
        return message[3]  # Device ID is at index 3
    return -1


def bankDescriptors():
    return [
        {"bank": 0, "name": "RAM", "size": 128, "type": "Preset"},
        {"bank": 1, "name": "ROM", "size": 128, "type": "Preset"},
        {"bank": 2, "name": "CARD", "size": 128, "type": "Preset"}
    ]


def createProgramDumpRequest(device_id, patchNo):
    if not 0 <= patchNo < 320:
        raise ValueError(f"Patch number out of range: {patchNo} should be from 0 to 319")
    return [0xf0, EMU_ID, PROTEUS_ID, device_id & 0x0f, COMMAND_PRESET_REQUEST, patchNo & 0x7f, (patchNo >> 7) & 0x7f, 0xf7]


def isSingleProgramDump(message: List[int]) -> bool:
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1] == EMU_ID
            and message[2] == PROTEUS_ID
            and message[4] == COMMAND_PRESET_DATA)


def nameFromDump(message: List[int]) -> str:
    if isSingleProgramDump(message):
        offset = 7
        name_chars = []
        for _ in range(12):
            val = message[offset] + (message[offset + 1] << 7)
            name_chars.append(chr(val))
            offset += 2
        return ''.join(name_chars)
    return 'invalid'


def numberFromDump(message: List[int]) -> int:
    if isSingleProgramDump(message):
        return message[5] + (message[6] << 7)
    return -1


def convertToProgramDump(device_id, message, program_number):
    if isSingleProgramDump(message):
        return message[0:3] + [device_id & 0x0f] + message[4:5] + [program_number & 0x7f, (program_number >> 7) & 0x7f] + message[7:]
    raise Exception("Can only convert program dumps")


def renamePatch(message: List[int], new_name: str) -> List[int]:
    if isSingleProgramDump(message):
        name_params = [(ord(c) & 0x7f, (ord(c) >> 7) & 0x7f) for c in new_name.ljust(12, " ")]
        return message[:7] + [item for sublist in name_params for item in sublist] + message[31:]
    raise Exception("Can only rename Presets!")


def calculateFingerprint(message: List[int]):
    if isSingleProgramDump(message):
        data = message[7:-1]
        # Blank out name, 12 characters stored as 14-bit values
        data[0:24] = [0] * 24
        return hashlib.md5(bytearray(data)).hexdigest()
    raise Exception("Can only fingerprint Presets")


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], name="FMstylePiano", number=64)  # Adjusted test data for Proteus
        yield testing.ProgramTestData(message=data.all_messages[63], name="BarberPole  ", number=127)

    return testing.TestData(sysex="testData/E-mu_Proteus/Proteus1Presets.syx", program_generator=programs)
