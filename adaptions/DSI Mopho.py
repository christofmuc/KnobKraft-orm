#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sequential

#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="DSI Mopho",
                                     device_id=0x25,
                                     banks=3,
                                     patches_per_bank=128,
                                     name_position=184,
                                     name_len=16,
                                     id_list=[0x25, 0x27, 0x29]  # this adds Mopho Keyboard, SE, and X4
                                     )


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


if __name__ == "__main__":
    import sys
    import unittest
    messages = sequential.load_sysex("testData/Mopho_Programs_v1.0.syx")
    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(sys.modules[__name__], messages[0]))
