#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#


def name():
    return "Quasimidi Cyber-6"


def createDeviceDetectMessage(channel):
    # Just request master keyboard Program 001 and see if we get an answer
    return createProgramDumpRequest(0x00, 0)


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if len(message) > 4 and message[:5] == [0xf0, 0x3f, 0x00, 0x24, 0x44]:
        # Specified is only device number 0x00, I am speculating that it was planned to support multiple numbers
        return 0x00
    return -1


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, patchNo):
    # For a request, we need the three byte address and a two byte size
    # Address is well specified, for a master keyboard program that would be 0, patchNo, 0
    # The size is more tricky - I counted 157 bytes for a master keyboard program
    guessed_size = 157
    size_lo = guessed_size & 0x7f
    size_hi = (guessed_size >> 7) & 0x7f
    return createCyber6Request(0x00, patchNo % 128, 0x00, size_hi, size_lo)


def isSingleProgramDump(message):
    # This will return true for all data dumps of the Cyber-6 of type master keyboard (the last 0x00 checks that!)
    return len(message) > 5 and message[:6] == [0xf0, 0x3f, 0x00, 0x24, 0x52, 0x00]


def convertToProgramDump(channel, message, program_number):
    if isSingleProgramDump(message):
        # Just create a new message with the new address and the data block from the given message
        return createCyber6Dump(0x00, program_number & 0x7f, 0x00, message[8:-1])
    raise Exception("Can only create program dumps from master keyboard dumps")


def nameFromDump(message):
    if isSingleProgramDump(message):
        data_block = message[8:-1]  # The data block starts at index 8, and does not include the last 0xf7
        name_index = 9 * 8 + 2 * 16  # Taken from page 59 of the manual, the sysex data format
        return ''.join([chr(x) for x in data_block[name_index:name_index + 8]])
    raise Exception("Can only extract name from master keyboard program dump")


def createCyber6Request(ah, am, al, dh, dl):
    return [0xf0, 0x3f, 0x00, 0x24, 0x52, ah, am, al, dh, dl, 0xf7]


def createCyber6Dump(ah, am, al, data):
    return [0xf0, 0x3f, 0x00, 0x24, 0x44, ah, am, al] + data + [0xf7]
