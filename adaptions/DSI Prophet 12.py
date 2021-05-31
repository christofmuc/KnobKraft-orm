#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sequential
import unittest

prophet12_Desktop_ID = 0x2b  # The Pro12 Desktop module calls itself 0x2b,

#
# Configure the GenericSequential module for the Prophet 12
#
synth = sequential.GenericSequential(name="DSI Prophet 12",
                                     device_id=0b00101010,  # See Page 82 of the prophet 12 manual
                                     banks=8,
                                     patches_per_bank=99,
                                     name_len=17,
                                     name_position=402,
                                     blank_out_zones=[(914, 17)]  # Make sure to blank out the layer B name as well
                                     )


#
# Synth specific functions
#


def setupHelp():
    return "The DSI Prophet 12 has two relevant global settings:\n\n" \
           "1. You must set MIDI Sysex Enable to On\n" \
           "2. You must choose the MIDI Sysex Cable.\n\n" \
           "Options are DIN MIDI cable or the USB for sysex. USB is much faster.\n\n" \
           "Both settings are accessible via the GLOBALS menu."


#
# No user serviceable parts below this line, this is only glue code that binds the expected methods to the instance
#


def name():
    return synth.name()


def createDeviceDetectMessage(channel):
    return synth.createDeviceDetectMessage(channel)


def deviceDetectWaitMilliseconds():
    return synth.deviceDetectWaitMilliseconds()


def needsChannelSpecificDetection():
    return synth.needsChannelSpecificDetection()


def channelIfValidDeviceResponse(message):
    return synth.channelIfValidDeviceResponse(message)


def createEditBufferRequest(channel):
    return synth.createEditBufferRequest(channel)


def isEditBufferDump(message):
    return synth.isEditBufferDump(message)


def numberOfBanks():
    return synth.numberOfBanks()


def numberOfPatchesPerBank():
    return synth.numberOfPatchesPerBank()


def createProgramDumpRequest(channel, patchNo):
    return synth.createProgramDumpRequest(channel, patchNo)


def isSingleProgramDump(message):
    return synth.isSingleProgramDump(message)


def nameFromDump(message):
    return synth.nameFromDump(message)


def convertToEditBuffer(channel, message):
    return synth.convertToEditBuffer(channel, message)


def convertToProgramDump(channel, message, program_number):
    return synth.convertToProgramDump(channel, message, program_number)


def calculateFingerprint(message):
    return synth.calculateFingerprint(message)


def renamePatch(message, new_name):
    return synth.renamePatch(message, new_name)


#
# Some device specific tests here
#

class TestStringMethods(unittest.TestCase):

    def test_renaming(self):
        binary = [240, 1, 42, 2, 0, 63, 0, 24, 40, 43, 58, 50, 37, 52, 0, 20, 63, 0, 0, 0, 4, 4, 0, 4, 4, 0, 1, 2, 3, 7, 0,
                  8,
                  9, 10, 4, 5, 6, 7, 0, 64, 106, 64, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 70, 0, 70,
                  70,
                  70, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0,
                  0,
                  0, 0, 0, 60, 0, 47, 1, 0, 0, 64, 0, 6, 127, 127, 0, 106, 22, 0, 80, 60, 7, 9, 5, 11, 40, 0, 50, 33, 33,
                  54,
                  51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 106, 62, 6, 18, 0, 0, 0, 0, 0, 0, 0, 0,
                  0,
                  0, 0, 127, 54, 60, 5, 6, 44, 0, 45, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  127,
                  56, 127, 127, 0, 127, 127, 0, 127, 46, 0, 70, 85, 4, 5, 0, 3, 5, 33, 0, 67, 67, 0, 0, 127, 15, 0, 66, 85,
                  49,
                  52, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 14, 15, 16, 19, 0, 19, 24, 23, 23, 9, 11, 12, 96, 23, 26, 26, 26,
                  26,
                  9, 74, 95, 126, 6, 34, 1, 53, 105, 126, 87, 126, 126, 126, 0, 126, 0, 126, 0, 59, 20, 14, 59, 55, 29, 10,
                  0,
                  72, 7, 8, 9, 30, 51, 52, 0, 53, 54, 0, 0, 120, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 6, 0, 0, 0, 2, 0, 0, 0, 0,
                  60,
                  60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60,
                  60,
                  60, 60, 60, 60, 60, 0, 60, 60, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120,
                  120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 0, 0, 0,
                  0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 97,
                  114,
                  109, 0, 111, 110, 105, 99, 83, 112, 97, 0, 114, 107, 108, 105, 101, 115, 0, 0, 0, 60, 0, 15, 0, 0, 0, 0,
                  0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 40, 0, 43, 58, 50, 37, 52, 20, 63, 0, 0, 0, 0, 4,
                  4, 4,
                  4, 0, 0, 1, 2, 3, 7, 8, 9, 0, 10, 4, 5, 6, 7, 64, 106, 0, 64, 64, 0, 0, 0, 0, 0, 0, 0, 22, 0, 0, 0, 0, 0,
                  0,
                  19, 19, 1, 17, 70, 70, 70, 0, 70, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0,
                  2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 86, 0, 35, 47, 1, 0, 0, 0, 105, 16, 127, 127, 0, 106, 22, 80, 60, 0,
                  7, 9,
                  5, 11, 40, 50, 33, 0, 33, 54, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 106, 62,
                  6,
                  18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 0, 54, 60, 5, 6, 44, 45, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                  0,
                  0, 0, 0, 0, 0, 0, 0, 0, 127, 56, 0, 127, 127, 0, 127, 127, 127, 0, 0, 27, 51, 38, 4, 57, 3, 5, 0, 33, 75,
                  67,
                  67, 0, 0, 15, 0, 0, 66, 80, 49, 52, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 14, 15, 16, 19, 19, 24, 0, 23, 23,
                  9,
                  11, 12, 23, 26, 120, 26, 26, 26, 9, 74, 126, 6, 117, 34, 127, 53, 105, 126, 126, 126, 21, 126, 0, 126, 0,
                  126,
                  59, 20, 0, 14, 59, 55, 29, 10, 72, 7, 0, 8, 9, 30, 51, 52, 53, 54, 0, 0, 0, 120, 0, 0, 0, 1, 0, 0, 0, 0,
                  0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0,
                  1, 0,
                  2, 0, 0, 2, 0, 0, 0, 0, 0, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60,
                  0,
                  60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120,
                  120,
                  120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0,
                  120,
                  120, 120, 120, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0, 0,
                  0, 0, 0, 0, 0, 72, 97, 114, 109, 111, 110, 0, 105, 99, 83, 112, 97, 114, 107, 0, 108, 105, 101, 115, 68,
                  78,
                  32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 247]
        renamed = renamePatch(binary, nameFromDump(binary))
        self.assertEqual(calculateFingerprint(renamed), calculateFingerprint(binary))
        newname = renamePatch(binary, "newname")
        result = nameFromDump(newname)
        self.assertEqual(result, "newname")


if __name__ == "__main__":
    unittest.main()
