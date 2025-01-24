#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from copy import copy
from typing import List

import testing


def name():
    return "Akai AX80"


def setupHelp():
    return "This adaptation works only for the Tauntek firmware for the AKAI AX80.\n\nThe original Akai AX80 had no MIDI sysex capabilities.\n\n" \
            "There is no way to autodetect the synth, you have to configure the incoming MIDI interface manually (channel is ignored)."


def createDeviceDetectMessage(device_id):
    # No way to autodetect the Akai MX80, you have to configure it manually.
    return [0xf0, 0xf7]


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    # Hack - if the wait time is negative, don't autodetect. This needs to be replaced by some proper dynamic cast
    return -1


def channelIfValidDeviceResponse(message):
    # Can't
    return -1


def bankDescriptors():
    return [{"bank": 0, "name": f"RAM", "size": 64, "type": "Patch"}]


def createProgramDumpRequest(device_id, patchNo):
    # The Akai MX80 has no request patch message. It can only send (and I guess receive) program dumps.
    # Implement an empty function to enable at least the sending
    return [0xf0, 0xf7]


def isSingleProgramDump(message: List[int]) -> bool:
    return (len(message) == 50
            and message[0] == 0xf0
            and message[1] == 0x47
            and message[2] == 0x7e)


def numberFromDump(message: List[int]) -> int:
    if isSingleProgramDump(message):
        return message[3]
    return -1


def convertToProgramDump(device_id, message, program_number):
    if isSingleProgramDump(message):
        # Need to construct a new program dump from a single program dump.
        return message[0:3] + [program_number & 0x3f] + message[4:]
    raise Exception("Can only convert program dumps")


def calculateFingerprint(message: List[int]):
    if isSingleProgramDump(message):
        # Blank out program position
        blanked_out = copy(message)
        blanked_out[3] = 0
        return hashlib.md5(bytearray(blanked_out)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint Presets")


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], number=0)
        yield testing.ProgramTestData(message=data.all_messages[42], number=42)

    return testing.TestData(sysex="testData/Akai_AX80/AX8RevKandLRAMPresets(unverified).syx", program_generator=programs)
    #return testing.TestData(sysex="testData/Akai_AX80/AX80RevIRAMpresets.syx", program_generator=programs)
