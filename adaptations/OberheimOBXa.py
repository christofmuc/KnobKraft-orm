#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import re


# Oberheim OB-Xa MIDI implementation for the Encore kit
# Based on https://www.encoreelectronics.com/obxamk.pdf

encore_id = [0x00, 0x00, 0x2f, 0x08]
default_name_detector = re.compile("OB-Xa(:| \\(E\\):) [0-9]+")


# Potential: We don't use the dump all command yet, but requesting single programs
# All request would be F0 00 00 2F 08 01 00 F7
# Also unused is the Save Edit Buffer to Patch command F0 00 00 2F 08 04 <number> F7
# We don't need this as we can convert any edit buffer directly into a Single Program Dump

def name():
    return "Oberheim OB-Xa (Encore)"


def createDeviceDetectMessage(channel):
    # The OB-Xa Encore reacts on Device Inquiry, very nice
    return [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if len(message) > 11 and message[:12] == [0xf0, 0x7e, 0x7f, 0x06, 0x02, 0x00, 0x00, 0x2f, 0x00, 0x00, 0x00, 0x08]:
        # Return any valid channel
        return 1
    # Return invalid channel
    return -1


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 120


def createEditBufferRequest(channel):
    # 0x01 is Request Data Command
    # 0x02 is Edit Buffer Request
    return [0xf0] + encore_id + [0x01, 0x02, 0xf7]


def isEditBufferDump(message):
    # 0x02 is the Edit Buffer Load command
    return len(message) > 5 and message[0] == 0xf0 and isEncore(message) and message[5] == 0x02


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        # Turn a single dump into an edit buffer dump
        return message[:5] + [0x02] + message[7:]
    raise Exception("Can only convert edit buffer dumps or single program dumps")


def createProgramDumpRequest(channel, patchNo):
    # 0x01 is the Request Data command
    # The second 0x01 is to request a single patch
    return [0xf0] + encore_id + [0x01, 0x01, patchNo % numberOfPatchesPerBank(), 0xf7]


def isSingleProgramDump(message):
    # 0x00 is the Single Patch Load command
    return len(message) > 5 and message[0] == 0xf0 and isEncore(message) and message[5] == 0x00


def convertToProgramDump(channel, message, patchNo):
    if isEditBufferDump(message):
        return message[:5] + [0x00, patchNo % numberOfPatchesPerBank()] + getData(message) + [0xf7]
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
        return "OB-Xa (E): %03d" % (program + 1)
    return "invalid name"


def isDefaultName(patchName):
    # This regex ideally matches exactly the default names we produce with the above method nameFromDump()
    return default_name_detector.match(patchName) is not None


def calculateFingerprint(message):
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return hashlib.md5(bytearray(getData(message))).hexdigest()
    raise Exception("Only edit buffer dumps and program dumps can be fingerprinted")


def isEncore(message):
    return message[1:5] == encore_id


def getData(message):
    if isEditBufferDump(message):
        return message[6:-1]
    elif isSingleProgramDump(message):
        return message[7:-1]
    raise Exception("No edit buffer or program dump given to getData, program error")


# End of implementation for KnobKraft. Tests follow

import binascii


def run_tests():
    fake_edit_buffer = list(binascii.unhexlify("F000002F08020F0F0F0FF7"))
    assert isEditBufferDump(fake_edit_buffer)
    patch = convertToProgramDump(0, fake_edit_buffer, 119)
    assert isSingleProgramDump(patch)
    assert numberFromDump(patch) == 119
    patch_new_location = convertToProgramDump(0, patch, 77)
    assert numberFromDump(patch_new_location) == 77
    assert calculateFingerprint(patch) == calculateFingerprint(patch_new_location)
    patch_name = nameFromDump(patch)
    assert patch_name == "OB-Xa (E): 120"
    assert isDefaultName(patch_name)
    assert not isDefaultName("better name")
    request = createProgramDumpRequest(0, 119)
    assert isEncore(request)
    assert not isSingleProgramDump(request)
    assert calculateFingerprint(patch) == calculateFingerprint(fake_edit_buffer)


if __name__ == "__main__":
    run_tests()
