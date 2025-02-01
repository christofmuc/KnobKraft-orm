#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from typing import List

import testing


EMU_ID = 0x18
PROTEUS_ID = 0x04  # Changed to Proteus ID
COMMAND_PRESET_REQUEST = 0x00
COMMAND_PRESET_DATA = 0x01
COMMAND_VERSION_REQUEST = 0x0a
COMMAND_VERSION_DATA = 0x0b


def name():
    return "E-mu Proteus"  # Changed to Proteus


def createDeviceDetectMessage(device_id):
    # Use this command to determine the model (version code) and the
    # software revision. different models and revisions may have more or
    # less features, so use the command to verify what machine is being
    # controlled.
    return [0xF0, EMU_ID, PROTEUS_ID, device_id & 0x0F, COMMAND_VERSION_REQUEST, 0xF7]


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    # Better safe than sorry, let's start with a 300 ms delay
    return 300


def channelIfValidDeviceResponse(message):
    if (len(message) > 5
            and message[0:3] == [0xF0, EMU_ID, PROTEUS_ID]  # Changed to Proteus ID
            and message[4] == COMMAND_VERSION_DATA):
        return message[3]  # Device ID is at index 3
    return -1


def bankDescriptors():
    return [
        {"bank": 0, "name": "RAM", "size": 128, "type": "Preset"},
        {"bank": 1, "name": "ROM", "size": 128, "type": "Preset"},
        {"bank": 2, "name": "CARD", "size": 64, "type": "Preset"}  # Adjusted for Proteus
    ]


def createProgramDumpRequest(device_id, patchNo):
    # Request preset. Uses preset index values (not the same as program numbers).
    # Preset numbers are: 0-127 RAM, 128-255 ROM, 256-319 CARD.
    if not 0 <= patchNo < 320:
        raise ValueError(f"Patch number out of range: {patchNo} should be from 0 to 319")
    return [0xF0, EMU_ID, PROTEUS_ID, device_id & 0x0F, COMMAND_PRESET_REQUEST, patchNo & 0x7F, (patchNo >> 7) & 0x7F, 0xF7]


def isSingleProgramDump(message: List[int]) -> bool:
    return (len(message) > 5
            and message[0] == 0xF0
            and message[1] == EMU_ID
            and message[2] == PROTEUS_ID  # Changed to Proteus ID
            and message[4] == COMMAND_PRESET_DATA)


def nameFromDump(message: List[int]) -> str:
    if isSingleProgramDump(message):
        # Extract the name from the sysex message
        name_bytes = message[7:7+2*12]  # 12 characters * 2 bytes = 24 bytes
        name = []
        for i in range(0, 24, 2):
            # Combine the low byte and high byte to get the character value
            char_value = name_bytes[i] + (name_bytes[i + 1] << 7)
            name.append(chr(char_value))
        return ''.join(name)
    return 'invalid'


def numberFromDump(message: List[int]) -> int:
    if isSingleProgramDump(message):
        return message[5] + (message[6] << 7)
    return -1


def convertToProgramDump(device_id, message, program_number):
    if isSingleProgramDump(message):
        # Need to construct a new program dump from a single program dump. Keep the protocol version intact
        return message[0:3] + [device_id & 0x0F] + message[4:5] + [program_number & 0x7F, (program_number >> 7) & 0x7F] + message[7:]
    raise Exception("Can only convert program dumps")


def renamePatch(message: List[int], new_name: str) -> List[int]:
    if isSingleProgramDump(message):
        name_params = [(ord(c), 0) for c in new_name.ljust(12, " ")]
        return message[:7] + [item for sublist in name_params for item in sublist] + message[31:]
    raise Exception("Can only rename Presets!")


def calculateFingerprint(message: List[int]):
    if isSingleProgramDump(message):
        data = message[7:-1]
        # Blank out name, 12 characters stored as 14-bit values
        data[0:24] = [0] * 24
        return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint Presets")


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], name="FMstylePiano", number=64)  # Adjusted test data for Proteus
        yield testing.ProgramTestData(message=data.all_messages[63], name="BarberPole  ", number=127)

    return testing.TestData(sysex="testData/E-mu_Proteus/Proteus1Presets.syx", program_generator=programs)