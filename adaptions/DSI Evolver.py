#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sequential

#
# TODO - the Evolver has also a Waveshape Dump, that no other Sequential synth has. We need to support this
# TODO - It also has the peculiar NameDataDump, as the name of the patch is not included in the patch data itself

#
# Configure the GenericSequential module for the Prophet 12
#
synth = sequential.GenericSequential(name="DSI Evolver",
                                     device_id=0b00100000,  # See Page 82 of the prophet 12 manual
                                     banks=4,
                                     patches_per_bank=128,
                                     file_version=0x01
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


# def nameFromDump(message):
#    return synth.nameFromDump(message)


def convertToEditBuffer(channel, message):
    return synth.convertToEditBuffer(channel, message)


def convertToProgramDump(channel, message, program_number):
    return synth.convertToProgramDump(channel, message, program_number)


def calculateFingerprint(message):
    return synth.calculateFingerprint(message)


# def renamePatch(message, new_name):
#    return synth.renamePatch(message, new_name)


if __name__ == "__main__":
    import sys
    import unittest

    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(sys.modules[__name__],
                                                                         []))
