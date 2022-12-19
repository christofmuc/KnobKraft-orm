#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib

CANNOT_FIND_DATA_BLOCK = "Cannot find the message's data block."

def name():
    return "Pioneer Toraiz AS-1"


def createDeviceDetectMessage(channel):
    # See page 33 of the Toraiz AS-1 manual
    return [0xf0, 0x7e, 0x7f, 0b00000110, 1, 0xf7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # The manual states the AS1 replies with a 15 byte long message, see page 33
    if (len(message) == 15
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            # ignore message[2] - that's the current midi channel
            and message[3] == 0b0110  # Device request
            and message[4] == 0b0010  # Reply
            and message[5] == 0b00000000  # Pioneer ID byte 1
            and message[6] == 0b01000000  # Pioneer ID byte 2
            and message[7] == 0b00000101  # Pioneer ID byte 3
            and message[8] == 0b00000000  # Toriaz ID byte 1
            and message[9] == 0b00000000  # Toriaz ID byte 2
            and message[10] == 0b00000001  # Toriaz ID byte 3
            and message[11] == 0b00001000  # Toriaz ID byte 4
            and message[12] == 0b00010000):  # Device ID
        # This is indeed the right package, now extract the MIDI channel from the message
        if message[2] == 0x7f:
            # The Toraiz is set to OMNI. Not a good idea, but let's treat this as channel 1 for now
            return 1
        else:
            return message[2]
    return -1


def createEditBufferRequest(channel):
    # See page 34 of the Toraiz manual
    return [0xf0, 0b00000000, 0b01000000, 0b00000101, 0b00000000, 0b000000000, 0b00000001, 0b00001000, 0b00010000,
            0b00000110, 0xf7]


def isEditBufferDump(message):
    # see page 35 of the manual
    return (len(message) > 9
            and message[0] == 0xf0
            and message[1] == 0b00000000  # Pioneer ID byte 1
            and message[2] == 0b01000000  # Pioneer ID byte 2
            and message[3] == 0b00000101  # Pioneer ID byte 3
            and message[4] == 0b00000000  # Toriaz ID byte 1
            and message[5] == 0b00000000  # Toriaz ID byte 2
            and message[6] == 0b00000001  # Toriaz ID byte 3
            and message[7] == 0b00001000  # Toriaz ID byte 4
            and message[8] == 0b00010000  # Device ID
            and message[9] == 0b00000011)  # Edit Buffer Dump


def numberOfBanks():
    return 10


def numberOfPatchesPerBank():
    return 100


def createProgramDumpRequest(channel, patchNo):
    # Calculate bank and program - the KnobKraft Orm will just think the patches are 0 to 999, but the Toraiz needs a
    # bank number 0-9 and the patch number within that bank
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    # See page 33 of the Toraiz manual
    return [0xf0, 0b00000000, 0b01000000, 0b00000101, 0b00000000, 0b000000000, 0b00000001, 0b00001000, 0b00010000,
            0b00000101, bank, program, 0xf7]


def isSingleProgramDump(message):
    # see page 34 of the manual
    return (len(message) > 9
            and message[0] == 0xf0
            and message[1] == 0b00000000  # Pioneer ID byte 1
            and message[2] == 0b01000000  # Pioneer ID byte 2
            and message[3] == 0b00000101  # Pioneer ID byte 3
            and message[4] == 0b00000000  # Toriaz ID byte 1
            and message[5] == 0b00000000  # Toriaz ID byte 2
            and message[6] == 0b00000001  # Toriaz ID byte 3
            and message[7] == 0b00001000  # Toriaz ID byte 4
            and message[8] == 0b00010000  # Device ID
            and message[9] == 0b00000010)  # Program Dump


def nameFromDump(message):
    """Extracts the patch name from the supplied sysex message."""
    INVALID = "Invalid"
    dataBlockStart = getDataBlockStart(message)
    if dataBlockStart == -1:
        return INVALID
    dataBlock = message[dataBlockStart:-1]
    if len(dataBlock) > 0:
        patchData = unescapeSysex(dataBlock)
        return ''.join([chr(x) for x in patchData[107:107+20]])
    return INVALID


def renamePatch(message, new_name):
    """Returns a copy of the supplied sysex message whose internal name has been replaced with new_name."""
    dataBlockStart = getDataBlockStart(message)
    if dataBlockStart == -1:
        raise Exception(CANNOT_FIND_DATA_BLOCK)
    dataBlock = message[dataBlockStart:-1]
    if len(dataBlock) == 0:
        raise Exception("Data block length was 0.")
    patchData = unescapeSysex(dataBlock)
    # Normalize the name to exactly 20 characters, padding with spaces or truncating.
    if len(new_name) < 20:
        new_name += (" " * (20 - len(new_name)))
    elif len(new_name) > 20:
        new_name = new_name[:20]
    nameBytes = list(map(ord, new_name))
    patchData[107:107+20] = nameBytes
    escapedPatchData = escapeToSysex(patchData)
    # Rebuild the message with the new data block, appending "end of exclusive" (EOX).
    newMessage = message[:dataBlockStart] + escapedPatchData + [0xF7]
    return newMessage

    
def calculateFingerprint(message):
    """Calculates a hash from the message's data block only, ignoring the patch's name."""
    dataBlockStart = getDataBlockStart(message)
    if dataBlockStart == -1:
        raise Exception(CANNOT_FIND_DATA_BLOCK)
    dataBlock = message[dataBlockStart:-1]
    data = unescapeSysex(dataBlock)   
    dummyName = " " * 20
    data[107:107+20] = map(ord, dummyName)
    fingerprint = hashlib.md5(bytearray(data)).hexdigest()    
    return fingerprint


def getDataBlockStart(message):
    """Returns the start index of the data block within a sysex message, or -1 if the data block cannot be found."""
    if isSingleProgramDump(message):
        return 12
    elif isEditBufferDump(message):
        return 10
    else:
        return -1


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    if isSingleProgramDump(message):
        # To turn a single program dump into an edit buffer dump, we need to remove the bank and program number,
        # and switch the command to 0b00000011
        return message[0:9] + [0b00000011] + message[12:]
    raise Exception(
        "Data is neither edit buffer nor single program buffer from Toraiz AS-1")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message):
        return message[0:9] + [0b00000010] + [bank, program] + message[10:]
    elif isSingleProgramDump(message):
        return message[0:10] + [bank, program] + message[12:]
    raise Exception(
        "Neither edit buffer nor program dump - can't be converted")


def unescapeSysex(sysex):
    """Unpacks a 7-bit sysex message into 8-bit bytes."""
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        msbits = sysex[dataIndex]
        dataIndex += 1
        for i in range(7):
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex] | (
                    (msbits & (1 << i)) << (7 - i)))
            dataIndex += 1
    return result


def escapeToSysex(message):
    """Packs a message composed of 8-bit bytes into 7-bit sysex format."""
    result = []
    msBits = 0
    byteIndex = 0
    chunk = []
    while byteIndex < len(message):
        indexInChunk = byteIndex % 7
        if indexInChunk == 0:
            chunk = []
        currentByte = message[byteIndex]
        lsBits = currentByte & 0x7F
        msBit = currentByte & 0x80
        msBits |= msBit >> (7 - indexInChunk)
        chunk.append(lsBits)
        if indexInChunk == 6 or byteIndex == len(message) - 1:
            chunk.insert(0, msBits)
            result += chunk
            msBits = 0
        byteIndex += 1
    return result
    

def isDefaultName(patchName):
    return patchName == "Basic Program"
