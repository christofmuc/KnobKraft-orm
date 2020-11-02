#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#


sequential_prophet_5_device_id = 0b00110010  # This is the ID used in the factory programs available from the website
sequential_prophet_10_device_id = 0b00110001  # Guesswork - this is the ID that is listed in the prophet 5 manual

counter = 0


def name():
    return "Prophet-5 Rev4"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message, with channel set to 0x7f the device will always respond
    return [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 100


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 9
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x01  # Sequential / Dave Smith Instruments
            and (message[6] == sequential_prophet_5_device_id or message[6] == sequential_prophet_10_device_id)
            and message[7] == 0x01  # Family MS
            and message[8] == 0x00  # Family
            and message[9] == 0x00):  # Family
        # Extract the current MIDI channel from index 2 of the message
        return message[2] & 0x0f
    return -1


def createEditBufferRequest(channel):
    return [0xf0, 0x01, sequential_prophet_5_device_id, 0b00000110, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == sequential_prophet_5_device_id
            and message[3] == 0b00000011  # Edit Buffer Data
            )


def numberOfBanks():
    return 10


def numberOfPatchesPerBank():
    return 40


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return [0xf0, 0x01, sequential_prophet_5_device_id, 0b00000101, bank, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == sequential_prophet_5_device_id
            and message[3] == 0b00000010  # Program Data
            ) or (
            len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01 # Sequential
            and message[2] == 0x01 # Prophet 5 Rev 3
            )


def nameFromDump(message):
    dataBlock = []
    if isSingleProgramDump(message):
        # Support for the old Prophet 5 Rev 3 factory sysex files - they have no name in them!
        if len(message) == 0x34:
            global counter
            counter += 1
            return "Prophet 5 Rev3 #" + str(counter)
        dataBlock = message[6:-1]
    elif isEditBufferDump(message):
        dataBlock = message[4:-1]
    if len(dataBlock) > 0:
        patchData = unescapeSysex(dataBlock)
        return ''.join([chr(x) for x in patchData[65:85]])
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        # Have to strip out bank and program, and set command to edit buffer dump
        return message[0:3] + [0b00000011] + message[6:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def unescapeSysex(sysex):
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        msbits = sysex[dataIndex]
        dataIndex += 1
        for i in range(7):
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex] | ((msbits & (1 << i)) << (7 - i)))
            dataIndex += 1
    return result
