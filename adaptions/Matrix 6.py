#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#


def name():
    return "Matrix 6/6R"


def createDeviceDetectMessage(channel):
    # Trying to detect the Matrix 6/6R with a request to the master data
    return [0xf0, 0x10, 0x06, 0x04, 3, 0, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # The answer is a master data dump
    if (len(message) == 15
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x10  # Oberheim
            and message[2] == 0x06  # Matrix 6/6R
            and message[3] == 0x03  # Master parameter data
    ):
        # Extract the current MIDI channel from byte 11 of the master data
        master_data = denibble(message, 4)
        if len(master_data) != 236:
            print("Error, expected 236 bytes of master data")
        return master_data[11]
    return -1


def createEditBufferRequest(channel):
    # The Matrix 6/6R has no edit buffer request, in contrast to the later Matrix 1000. Leave this empty
    return [0xf0, 0xf7]


def isEditBufferDump(message):
    return False


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 100


def createProgramDumpRequest(channel, patchNo):
    program = patchNo % numberOfPatchesPerBank()
    return [0xf0, 0x10, 0x06, 0x04, 1, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x10  # Oberheim
            and message[2] == 0x06  # Matrix
            and message[3] == 0x01)  # Single Patch Data


def nameFromDump(message):
    if isSingleProgramDump(message):
        # To extract the name from the Matrix 6 program dump, we need to correctly de-nibble and then force the first 8 bytes into ASCII
        patchData = denibble(message, 5)
        return ''.join([chr(x if x >= 32 else x + 0x40) for x in patchData[0:8]])


def convertToEditBuffer(channel, message):
    if isSingleProgramDump(message):
        # The Matrix 6 cannot send to the edit buffer, so we send into program 100
        return message[0:4] + 99 + message[5:]
    raise Exception("This is not a program dump, can't be converted")


def denibble(message, start_index):
    return [message[x] | (message[x + 1] << 4) for x in range(start_index, len(message) - 2, 2)]
