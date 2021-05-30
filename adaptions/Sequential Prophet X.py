#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib

prophetX_ID = 0b00110000  # See Page 153 of the prophet X manual


def name():
    return "Sequential Prophet X"


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
            and message[5] == 0x01  # Sequential
            and message[6] == prophetX_ID
            and message[7] == 0x01  # Family MS is 1
            and message[8] == 0x00  # Family member
            and message[9] == 0x00):  # Family member
        # Extract the current MIDI channel from index 2 of the message
        # If the device is set to OMNI it will return 0x7f as MIDI channel - we use 1 for now which will work
        return message[2] if message[2] != 0x7f else 1
    return -1


def createEditBufferRequest(channel):
    return [0xf0, 0x01, prophetX_ID, 0b00000110, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == prophetX_ID
            and message[3] == 0b00000011  # Edit Buffer Data
            )


def numberOfBanks():
    return 8


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return [0xf0, 0x01, prophetX_ID, 0b00000101, bank, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == prophetX_ID
            and message[3] == 0b00000010  # Program Data
            )


def nameFromDump(message):
    dataBlock = []
    if isSingleProgramDump(message):
        dataBlock = message[6:-1]
    elif isEditBufferDump(message):
        dataBlock = message[4:-1]
    if len(dataBlock) > 0:
        patchData = unescapeSysex(dataBlock)
        layer_a_name = ''.join([chr(x) for x in patchData[418:418+20]]).strip()
        return layer_a_name
        # layer_b_name_index = 2466
        # layer_b_name = ''.join([chr(x) for x in patchData[layer_b_name_index:layer_b_name_index+20]]).strip()
        # return layer_a_name if layer_a_name == layer_b_name else layer_a_name + "|" + layer_b_name
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
    if isEditBufferDump(message):
        return hashlib.md5(bytearray(message[4:-1])).hexdigest()  # Calculate the fingerprint from the payload data
    if isSingleProgramDump(message):
        return hashlib.md5(bytearray(message[6:-1])).hexdigest()  # Same here, don't use bank or program place
    raise Exception("can only fingerprint edit buffer dumps or single program dumps")


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


import binascii
def run_tests():
    message = binascii.unhexlify("f0 7e 09 06 02 01 30 01 00 00 02 02 00 f7".replace(" ", ""))
    validResponse = channelIfValidDeviceResponse(message)
    assert validResponse == 0x09
    programDump = binascii.unhexlify("f0 01 30 02 03 00 00 18 32 05 00 01 01 01 20 00 00 03 7c 00 73 03 05 67 01 7d 01 00 01 3c 00 00 00 01 46 00 40 00 00 00 00 01 1c 40 00 00 00 1d 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 03 00 00 00 00 00 00 00 18 32 00 00 01 20 01 00 00 00 03 67 03 15 67 03 67 01 73 00 00 00 00 3c 00 00 01 46 00 00 40 00 00 00 01 14 40 00 00 00 16 0e 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 00 00 00 00 00 00 00 00 40 00 00 00 00 00 18 32 7f 01 2a 00 46 01 01 40 00 00 00 00 00 00 32 11 01 0f 46 00 01 01 40 00 00 00 00 00 05 00 40 00 00 00 00 00 00 00 00 00 2b 44 78 00 21 59 00 00 00 20 7f 20 01 05 4e 59 28 04 00 00 03 00 00 01 0c 5d 65 40 28 3d 00 03 00 00 7e 00 00 00 0a 1d 3a 10 00 00 00 00 00 00 7f 00 00 00 00 0b 56 0b 00 00 00 04 00 00 09 01 07 61 40 00 00 00 01 00 00 00 00 01 7e 00 00 7f 50 00 00 00 00 00 00 00 00 15 00 03 43 52 00 00 1b 00 08 20 00 00 00 00 00 44 0d 00 03 00 16 00 23 00 04 10 00 00 00 09 7e 04 00 00 46 00 2f 00 1b 00 00 04 00 2d 7e 04 01 45 00 00 06 00 38 00 00 00 0d 00 0e 05 01 01 01 10 10 00 10 10 10 10 00 00 00 76 00 7e 7e 7f 7e 03 0a 1f 7e 76 29 3a 7e 00 00 00 7f 7f 7f 4a 49 01 0d 00 10 16 1f 25 1d 1e 58 00 2b 28 00 00 00 41 0d 00 7f 00 7f 00 7f 00 7f 00 00 00 00 00 00 00 00 00 7f 7f 7f 7f 7f 7f 7f 00 00 07 07 00 00 00 00 00 00 00 00 00 00 00 00 00 00 78 06 00 02 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 07 00 00 00 00 00 00 00 00 57 61 00 62 62 6c 65 20 41 20 00 20 20 20 20 20 20 20 00 20 20 20 20 64 48 3c 00 4f 4d 3c 4d 4c 3c 48 00 3c 48 43 3c 3c 4a 43 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 40 3c 3c 3c 3c 3c 3c 51 7f 00 28 55 00 53 53 00 7f 4b 00 4b 56 00 00 4b 01 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 78 00 00 00 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 0f 7f 7f 7f 7f 18 32 7f 00 00 01 00 00 00 00 03 55 67 03 67 03 67 01 73 00 00 00 64 3c 00 00 01 00 46 00 40 00 00 00 01 00 14 40 00 00 00 0c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 00 00 00 00 00 00 00 00 18 00 32 00 00 01 00 00 00 54 00 03 67 03 67 03 67 02 01 73 00 00 00 3c 00 00 00 01 46 00 40 00 00 00 00 01 14 40 00 00 00 00 0c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 00 00 00 00 00 00 00 00 40 00 00 00 00 18 00 32 7f 02 40 46 01 01 00 40 00 00 00 00 18 32 00 64 00 40 46 01 01 40 00 00 00 00 00 00 00 40 00 00 00 00 00 00 00 00 02 00 24 00 00 00 52 00 00 00 00 00 7f 00 02 40 00 00 28 7f 00 03 00 00 00 00 0c 40 7f 28 00 00 00 03 00 00 7f 00 00 00 00 1d 16 10 00 00 00 00 00 00 7f 00 00 00 00 7f 00 28 00 00 00 00 00 7f 00 00 00 00 1e 00 00 00 00 00 00 00 00 7f 00 00 00 00 1e 00 00 00 00 00 10 00 00 00 00 06 7f 00 00 00 0d 00 00 00 00 00 04 00 00 06 7f 00 00 00 00 00 00 00 00 00 00 00 01 06 7f 00 00 00 00 00 20 00 00 00 00 00 06 7f 00 00 00 00 00 00 00 00 00 00 00 00 0d 0e 05 00 00 00 00 00 00 00 00 00 60 00 00 00 00 00 48 48 00 7f 7f 7f 7f 7f 7f 7f 00 7f 7f 7f 7f 7f 7f 7f 00 1d 10 01 00 00 00 00 00 00 00 00 00 00 00 00 04 00 00 16 28 7f 00 7f 00 00 7f 00 7f 00 00 00 00 00 00 00 00 7f 7f 7f 00 7f 7f 7f 7f 00 07 07 00 00 00 00 00 00 00 00 00 00 00 00 00 00 78 06 00 00 02 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 07 00 00 00 00 00 00 00 00 57 61 62 62 6c 00 65 20 41 20 20 20 20 00 20 20 20 20 20 20 20 00 20 64 48 3c 4f 4d 3c 00 4d 4c 3c 48 3c 48 43 00 3c 3c 4a 43 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 78 3c 3c 3c 51 00 28 55 7f 00 53 53 00 4b 00 4b 1f 56 00 00 4b 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 3c 00 3c 3c 3c 3c 3c 3c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 7e 00 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 01 7f 00 f7".replace(" ", ""))
    assert isSingleProgramDump(programDump)
    print(nameFromDump(programDump))


if __name__ == "__main__":
    run_tests()

