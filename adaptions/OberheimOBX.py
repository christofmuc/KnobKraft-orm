#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import re

# Oberheim OB-X MIDI implementation for the Encore kit
# Based on https://www.encoreelectronics.com/obxmk.pdf

encore_id = [0x00, 0x00, 0x2f, 0x06]
default_name_detector = re.compile("OB-X(:| \\(E\\):) [0-9]+")


# Potential: The OB-X Encore also supports a dump all command. I have not implemented it here because
# experience tells it will not be much faster than requesting the individual programs as there are so few of them
# Dump all is f0 00 00 2f 06 01 00 f7


def name():
    return "Oberheim OB-X (Encore)"


def createDeviceDetectMessage(channel):
    # The only message the OB-X replies to is a program dump request...
    return createProgramDumpRequest(channel, 0)


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    # No idea how fast the answer will be, let's wait a second
    return 1000


def channelIfValidDeviceResponse(message):
    # We asked for program 0, see if that is what we got
    if isSingleProgramDump(message) and numberFromDump(message) == 0:
        return 1
    return -1


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 120


def createProgramDumpRequest(channel, patchNo):
    # 0x01 is the Request Data command
    # The second 0x01 is to request a single patch
    return [0xf0] + encore_id + [0x01, 0x01, patchNo % numberOfPatchesPerBank(), 0xf7]


def isSingleProgramDump(message):
    # 0x00 is the Single Patch Load command
    return len(message) > 5 and message[0] == 0xf0 and isEncore(message) and message[5] == 0x00


def convertToProgramDump(channel, message, patchNo):
    if isSingleProgramDump(message):
        return [0xf0] + encore_id + [0x00, patchNo % numberOfPatchesPerBank()] + getData(message) + [0xf7]
    raise Exception("Only program dumps can be converted")


def numberFromDump(message):
    if isSingleProgramDump(message):
        return message[6]
    raise Exception("Can't extract program number from message")


def nameFromDump(message):
    if isSingleProgramDump(message):
        program = numberFromDump(message)
        return "OB-X (E): %03d" % (program + 1)
    return "invalid name"


def isDefaultName(patchName):
    # This regex ideally matches exactly the default names we produce with the above method nameFromDump()
    return default_name_detector.match(patchName) is not None


def calculateFingerprint(message):
    if isSingleProgramDump(message):
        return hashlib.md5(bytearray(getData(message))).hexdigest()
    raise Exception("Only program dumps can be fingerprinted")


def isEncore(message):
    return message[1:5] == encore_id


def getData(message):
    if isSingleProgramDump(message):
        return message[7:-1]
    raise Exception("Non program dump given to getData, program error")


# End of implementation for KnobKraft. Tests follow

import binascii


def run_tests():
    fake_program_buffer = list(binascii.unhexlify("F000002F0600200F0F0F0FF7"))
    assert isSingleProgramDump(fake_program_buffer)
    assert numberFromDump(fake_program_buffer) == 0x20
    patch = convertToProgramDump(0, fake_program_buffer, 119)
    assert isSingleProgramDump(patch)
    assert numberFromDump(patch) == 119
    assert calculateFingerprint(patch) == calculateFingerprint(fake_program_buffer)
    patch_name = nameFromDump(patch)
    assert patch_name == "OB-X (E): 120"
    assert isDefaultName(patch_name)
    assert not isDefaultName("better name")
    request = createProgramDumpRequest(0, 119)
    assert isEncore(request)
    assert not isSingleProgramDump(request)


if __name__ == "__main__":
    run_tests()
