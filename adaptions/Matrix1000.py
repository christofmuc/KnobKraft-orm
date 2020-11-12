#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Note that for real life usage the native C++ implementation of the Matrix1000 is more powerful and should be used
# This is an example adaption to show how a fully working adaption can look like


def name():
    return "Matrix 1000 Adaption"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    # The Matrix 1000 answers with 15 bytes
    if (len(message) == 15
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x10  # Oberheim
            and message[6] == 0x06  # Matrix
            and message[7] == 0x00
            and message[8] == 0x02  # Family member Matrix 1000
            and message[9] == 0x00):
        # Extract the current MIDI channel from index 2 of the message
        return message[2]
    return -1


def createEditBufferRequest(channel):
    return [0xf0, 0x10, 0x06, 0x04, 4, 0, 0xf7]


def isEditBufferDump(message):
    # The Matrix1000 sends the edit buffer in the same format as a single program
    return isSingleProgramDump(message)


def numberOfBanks():
    return 10


def numberOfPatchesPerBank():
    return 100


def bankAndProgramFromPatchNumber(patchNo):
    return patchNo // numberOfPatchesPerBank(), patchNo % numberOfPatchesPerBank()


def createBankSelect(bank):
    return [0xf0, 0x10, 0x06, 0x0a, bank, 0xf7]


def createProgramDumpRequest(channel, patchNo):
    bank, program = bankAndProgramFromPatchNumber(patchNo)
    return createBankSelect(bank) + [0xf0, 0x10, 0x06, 0x04, 1, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x10  # Oberheim
            and message[2] == 0x06  # Matrix
            and message[3] == 0x01)  # Single Patch Data


def nameFromDump(message):
    if isSingleProgramDump(message):
        # To extract the name from the Matrix 1000 program dump, we
        # need to correctly de-nibble and then force the first 8 bytes into ASCII
        patchData = denibble(message, 5, len(message) - 2)
        # The Matrix 6 stores only 6 bit of ASCII, folding the letters into the range 0 to 31
        return ''.join([chr(x if x >= 32 else x + 0x40) for x in patchData[0:8]])


def renamePatch(message, new_name):
    if isSingleProgramDump(message):
        # The Matrix 6 stores only 6 bit of ASCII, folding the letters into the range 0 to 31
        valid_name = [ord(x) if ord(x) < 0x60 else (ord(x) - 0x20) for x in new_name]
        new_name_nibbles = nibble([(valid_name[i] & 0x3f) if i < len(new_name) else 0x20 for i in range(8)])
        return rebuildChecksum(message[0:5] + new_name_nibbles + message[21:])
    raise Exception("Neither edit buffer nor program dump can't be converted")


def convertToEditBuffer(channel, message):
    if isSingleProgramDump(message):
        # Both are "single patch data", but must be converted to "single patch data to edit buffer"
        return message[0:3] + [0x0d] + [0x00] + message[5:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def convertToProgramDump(channel, message, patchNo):
    if isSingleProgramDump(message):
        bank, program = bankAndProgramFromPatchNumber(patchNo)
        # Variant 1: Send the edit buffer and then a store edit buffer message
        return convertToEditBuffer(channel, message) + [0xf0, 0x10, 0x06, 0x0e, bank, program, 0, 0xf7]
        # Variant 2: Send a bank select and then a SinglePatchData package
        # return createBankSelect(bank) + message[0:4] + [program] + message[5:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def rebuildChecksum(message):
    if isSingleProgramDump(message):
        data = denibble(message, 5, len(message) - 2)
        checksum = sum(data) & 0x7f
        return message[:-2] + [checksum, 0xf7]
    raise Exception("rebuildChecksum only implemented for single patch data yet")


def denibble(message, start, stop):
    return [message[x] | (message[x + 1] << 4) for x in range(start, stop, 2)]


def nibble(message):
    result = []
    for b in message:
        result.append(b & 0x0f)
        result.append((b & 0xf0) >> 4)
    return result


# Testing patch renaming
# message = [0xf0, 0x10, 0x06, 0x01, 99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
# renamed = renamePatch(message, "test")
# print(nameFromDump(renamed))
