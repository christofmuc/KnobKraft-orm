#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#


def name():
    return "Korg DW-6000"


def createDeviceDetectMessage(channel):
    # Page 5 of the service manual - Device ID Request
    # Different from the DW-8000, the DW-6000 does not differentiate which channel it is on via sysex
    return [0xf0, 0x42, 0x40, 0xf7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # Page 3 of the service manual - Device ID
    if (len(message) > 3
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x42  # Korg
            and message[2] == 0x30  # Device ID
            and message[3] == 0x04):  # DW-6000
        # Sadly, there is no way to figure out which channel the Korg DW-6000 is set to
        return 1
    return -1


def createEditBufferRequest(channel):
    # Page 5 - Data Save Request
    return [0xf0, 0x42, 0x30, 0x04, 0x10, 0xf7]


def isEditBufferDump(message):
    # Page 3 - Data Dump
    return (len(message) > 4
            and message[0] == 0xf0
            and message[1] == 0x42  # Korg
            and (message[2] & 0xf0) == 0x30  # Format, ignore MIDI Channel in lower 4 bits
            and message[3] == 0x04  # DW-6000
            and message[4] == 0x40  # Data Dump
            )


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 64


def createProgramDumpRequest(channel, patchNo):
    program = patchNo % numberOfPatchesPerBank()
    # This is done by creating a program change request and then an edit buffer request
    return [0b11000000 | channel, program] + createEditBufferRequest(channel)


def isSingleProgramDump(message):
    # The DW-6000 does not differentiate - you need to send program change messages in between to get other programs
    return isEditBufferDump(message)


def nameFromDump(message):
    # The DW-6000 has no patch name memory, so all patches get the same name for a start
    return "DW-6000"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message[0:2] + [0x30 | channel] + message[3:]
    raise Exception("This is not an edit buffer - can't be converted")
