#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# https://github.com/eclab/edisyn/blob/master/edisyn/synth/waldorfkyra/WaldorfKyra.java
# Seems to be the best source for information on the Kyra sysex
kyra_id = 0x22


def name():
    return "Waldorf Kyra"


def createDeviceDetectMessage(channel):
    # Just trying to get the edit buffer from the synth
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    return 200


def needsChannelSpecificDetection():
    # Unproven, but let's hope it will answer a generic device request just like a Waldorf Blofeld
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 6
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x3e  # Waldorf
            and message[6] == kyra_id):  # Blofeld
        # and message[7] == 0x00  # Family MS is 0
        # and message[8] == 0x00  # Family member
        # and message[9] == 0x00):  # Family member
        # If the Kyra works like the Blofeld, expect the device ID in byte 2 of the message
        return message[2]
    return -1


def createEditBufferRequest(channel):
    return [0xf0, 0x3e, kyra_id, channel, 0x20, 0x01, 0x7f, 0x7f, 0xf7]


def isEditBufferDump(message):
    if isOwnSysex(message):
        message_id, bank, program = getMessageTypeAndParams(message)
        return ((message_id == 0x40  # Single patch dump as received from Kyra
                 or message_id == 0x00)  # Single patch dump as sent to Kyra
                and bank == 0x7f
                and program == 0x7f)


def numberOfBanks():
    # Banks are 'A..Z'
    return 26


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return [0xf0, 0x3e, kyra_id, channel, 0x20, 0x01, bank, program, 0xf7]


def isSingleProgramDump(message):
    if isOwnSysex(message):
        message_id, bank, program = getMessageTypeAndParams(message)
        return ((message_id == 0x40  # Single patch dump
                 or message_id == 0x00)  # Could be the "sent to Kyra" message type as well
                and 0x00 <= bank < numberOfBanks())  # not an edit buffer
    return False


def isOwnSysex(message):
    return (len(message) > 6
            and message[0] == 0xf0
            and message[1] == 0x3e  # Waldorf
            and message[2] == kyra_id)


def getMessageTypeAndParams(message):
    # Ignore message[5], which is version and should be 0x01
    return message[4], message[6], message[7]


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        sdata_base_index = 8
        name_base_index = 200
        name_length = 22
        return ''.join([chr(x) for x in message[sdata_base_index + name_base_index:sdata_base_index + name_base_index + name_length]])
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message) or isSingleProgramDump(message):
        # Have to set bank to 0x7f and program to 0x00. The checksum does not need to be recalculated
        return message[:3] + [channel, 0x00] + message[5] + [0x7f, 0x7f] + message[8:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return message[:3] + [channel, 0x00] + message[5] + [bank, program] + message[8:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")
