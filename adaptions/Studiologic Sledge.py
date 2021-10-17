#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

sledgeDeviceID = 0x15


def name():
    return "Studiologic Sledge"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    return 100


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 6
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x3e
            and message[6] == sledgeDeviceID):
            #and message[7] == 0x00  # Family MS is 0
            #and message[8] == 0x00  # Family member
            #and message[9] == 0x00):  # Family member
        return message[2]
    return -1


def createEditBufferRequest(channel):
    return [0xf0, 0x3e, sledgeDeviceID, channel, 0x00, 0x7f, 0x00, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 6
            and message[0] == 0xf0
            and message[1] == 0x3e
            and message[2] == sledgeDeviceID
            and message[4] == 0x10  # Sound Dump Data
            and message[5] == 0x7f  # Edit buffer
            and message[6] == 0x00  # Sound mode, not multi instrument mode
            )


def numberOfBanks():
    return 8


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return [0xf0, 0x3e, sledgeDeviceID, channel, 0x00, bank, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1] == 0x3e
            and message[2] == sledgeDeviceID
            and message[4] == 0x10  # Sound Dump Data
            and 0x00 <= message[5] < numberOfBanks()  # Bank number
            )


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        sdata_base_index = 7
        name_base_index = 363
        return ''.join([chr(x) for x in message[sdata_base_index + name_base_index:sdata_base_index + name_base_index + 16]])
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message[:3] + [channel] + message[4:]
    elif isSingleProgramDump(message):
        # Have to set bank to 0x7f and program to 0x00. The checksum does not need to be recalculated
        return message[:3] + [channel, 0x10, 0x7f, 0x00] + message[7:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return message[:3] + [channel, 0x10, bank, program] + message[7:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")
