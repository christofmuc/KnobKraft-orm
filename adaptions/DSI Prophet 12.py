#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib

prophet12_ID = 0b00101010  # See Page 82 of the prophet 12 manual
prophet12_Desktop_ID = 0x2b # The Pro12 Desktop module calls itself 0x2b,
name_len = 17  # Strangely, the Prophet12 has 17 characters for a patch name. Is this right?


def name():
    return "DSI Prophet 12"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


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
            and (message[6] == prophet12_ID or message[6] == prophet12_Desktop_ID)
            and message[7] == 0x01  # Family MS is 1
            and message[8] == 0x00  # Family member
            and message[9] == 0x00):  # Family member
        # Extract the current MIDI channel from index 2 of the message
        # If the device is set to OMNI it will return 0x7f as MIDI channel - we use 1 for now which will work
        return message[2] if message[2] != 0x7f else 1
    return -1


def createEditBufferRequest(channel):
    return [0xf0, 0x01, prophet12_ID, 0b00000110, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and (message[2] == prophet12_ID or message[2] == prophet12_Desktop_ID)
            and message[3] == 0b00000011  # Edit Buffer Data
            )


def numberOfBanks():
    return 8


def numberOfPatchesPerBank():
    return 99


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return [0xf0, 0x01, prophet12_ID, 0b00000101, bank, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and (message[2] == prophet12_ID or message[2] == prophet12_Desktop_ID)
            and message[3] == 0b00000010  # Program Data
            )


def nameFromDump(message):
    dataBlock = getDataBlock(message)
    if len(dataBlock) > 0:
        patchData = unescapeSysex(dataBlock)
        layer_a_name = ''.join([chr(x) for x in patchData[402:402+name_len]]).strip() 
        return layer_a_name
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        # Have to strip out bank and program, and set command to edit buffer dump
        return message[0:3] + [0b00000011] + message[6:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message):
        return message[0:3] + [0b00000010] + [bank, program] + message[4:]
    elif isSingleProgramDump(message):
        return message[0:3] + [0b00000010] + [bank, program] + message[6:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def calculateFingerprint(message):
    raw = getDataBlock(message)
    data = unescapeSysex(raw)
    # Blank out Layer A and Layer B name, they should not matter for the fingerprint
    data[402:402 + name_len] = [0] * name_len
    data[914:914 + name_len] = [0] * name_len  # each layer needs 512 bytes
    return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data


def renamePatch(message, new_name):
    if isEditBufferDump(message):
        header_len = 4
    elif isSingleProgramDump(message):
        header_len = 6
    else:
        raise Exception("Can only rename edit buffer or single program dumps")
    data = unescapeSysex(message[header_len:-1])
    for i in range(name_len):
        data[402+i] = ord(new_name[i]) if i < len(new_name) else 0
    return message[:header_len] + escapeSysex(data) + [0xf7]


def setupHelp():
    return "The DSI Prophet 12 has two relevant global settings:\n\n" \
           "1. You must set MIDI Sysex Enable to On\n" \
           "2. You must choose the MIDI Sysex Cable.\n\n" \
           "Options are DIN MIDI cable or the USB for sysex. USB is much faster.\n\n" \
           "Both settings are accessible via the GLOBALS menu."


def getDataBlock(message):
    if isSingleProgramDump(message):
        return message[6:-1]
    elif isEditBufferDump(message):
        return message[4:-1]
    raise Exception("Neither edit buffer nor program dump - cannot determine data block")


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


def escapeSysex(data):
    result = []
    dataIndex = 0
    while dataIndex < len(data):
        ms_bits = 0
        for i in range(7):
            if dataIndex + i < len(data):
                ms_bits = ms_bits | ((data[dataIndex + i] & 0x80) >> (7 - i))
        result.append(ms_bits)
        for i in range(7):
            if dataIndex + i < len(data):
                result.append(data[dataIndex + i] & 0x7f)
        dataIndex += 7
    return result


binary = [240, 1, 42, 2, 0, 63, 0, 24, 40, 43, 58, 50, 37, 52, 0, 20, 63, 0, 0, 0, 4, 4, 0, 4, 4, 0, 1, 2, 3, 7, 0, 8, 9, 10, 4, 5, 6, 7, 0, 64, 106, 64, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 70, 0, 70, 70, 70, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 0, 47, 1, 0, 0, 64, 0, 6, 127, 127, 0, 106, 22, 0, 80, 60, 7, 9, 5, 11, 40, 0, 50, 33, 33, 54, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 106, 62, 6, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 54, 60, 5, 6, 44, 0, 45, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 56, 127, 127, 0, 127, 127, 0, 127, 46, 0, 70, 85, 4, 5, 0, 3, 5, 33, 0, 67, 67, 0, 0, 127, 15, 0, 66, 85, 49, 52, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 14, 15, 16, 19, 0, 19, 24, 23, 23, 9, 11, 12, 96, 23, 26, 26, 26, 26, 9, 74, 95, 126, 6, 34, 1, 53, 105, 126, 87, 126, 126, 126, 0, 126, 0, 126, 0, 59, 20, 14, 59, 55, 29, 10, 0, 72, 7, 8, 9, 30, 51, 52, 0, 53, 54, 0, 0, 120, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 6, 0, 0, 0, 2, 0, 0, 0, 0, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 97, 114, 109, 0, 111, 110, 105, 99, 83, 112, 97, 0, 114, 107, 108, 105, 101, 115, 0, 0, 0, 60, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 40, 0, 43, 58, 50, 37, 52, 20, 63, 0, 0, 0, 0, 4, 4, 4, 4, 0, 0, 1, 2, 3, 7, 8, 9, 0, 10, 4, 5, 6, 7, 64, 106, 0, 64, 64, 0, 0, 0, 0, 0, 0, 0, 22, 0, 0, 0, 0, 0, 0, 19, 19, 1, 17, 70, 70, 70, 0, 70, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 86, 0, 35, 47, 1, 0, 0, 0, 105, 16, 127, 127, 0, 106, 22, 80, 60, 0, 7, 9, 5, 11, 40, 50, 33, 0, 33, 54, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 106, 62, 6, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 0, 54, 60, 5, 6, 44, 45, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 56, 0, 127, 127, 0, 127, 127, 127, 0, 0, 27, 51, 38, 4, 57, 3, 5, 0, 33, 75, 67, 67, 0, 0, 15, 0, 0, 66, 80, 49, 52, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 14, 15, 16, 19, 19, 24, 0, 23, 23, 9, 11, 12, 23, 26, 120, 26, 26, 26, 9, 74, 126, 6, 117, 34, 127, 53, 105, 126, 126, 126, 21, 126, 0, 126, 0, 126, 59, 20, 0, 14, 59, 55, 29, 10, 72, 7, 0, 8, 9, 30, 51, 52, 53, 54, 0, 0, 0, 120, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 1, 0, 2, 0, 0, 2, 0, 0, 0, 0, 0, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 60, 60, 60, 60, 60, 60, 60, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 120, 120, 120, 0, 120, 120, 120, 120, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 97, 114, 109, 111, 110, 0, 105, 99, 83, 112, 97, 114, 107, 0, 108, 105, 101, 115, 68, 78, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 247]
renamed = renamePatch(binary, nameFromDump(binary))

# Documenting the Sequential/DSI device IDs here for all sequential modules
#
# Evolver    - 0b00100000 0x20 (same as Poly Evolver and all other Evolvers)
# Prophet 08 - 0b00100011 0x23 or 0b00100100 0x24 for special edition
# Mopho      - 0x25 (used in f0f7)
# Tetra      - 0b00100110 0x26
# Mopho KB   - 0b00100111 0x27 (Mopho Keyboard and Mopho SE) or 0b00101001 0x29 (Mopho X4)
# Prophet 12 - 0b00101010 0x2a or 0b00101011 0x2b for module/special edition?
# Pro 2      - 0b00101100 0x2c
# Prophet 6  - 0b00101101 0x2d
# OB 6       - 0b00101110 0x2e
# Rev 2      - 0b00101111 0x2f
# Prophet X  - 0b00110000 0x30
# Pro 3      - 0b00110001 0x31
# Prophet 5  - 0b00110010 0x32 (this is the Rev 4 of course)
