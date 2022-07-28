#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys
import sequential
import hashlib

this_module = sys.modules[__name__]

# Tempest banks are called A and B, with the sounds called A1..A16 and B1..B16
tempest = {"name": "DSI Tempest",
           "device_id": 0x28,
           "banks": 2,
           "patches_per_bank": 16,
           "name_position": 0
           }


def name():
    return tempest["name"]


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
            and message[6] == tempest["device_id"]):
        # Family seems to be different, the Prophet 12 has (0x01, 0x00, 0x00) while the Evolver has (0, 0, 0)
        # and message[7] == 0x01  # Family MS is 1
        # and message[8] == 0x00  # Family member
        # and message[9] == 0x00):  # Family member
        # Extract the current MIDI channel from index 2 of the message
        if message[2] == 0x7f:
            # If the device is set to OMNI it will return 0x7f as MIDI channel - we use 1 for now which will work
            return 1
        elif message[2] == 0x10:
            # This indicates the device is in MPE mode (currently Prophet-6, OB-6),
            # and allocates channel 0-5 to the six voices
            return 0
        return message[2]
    return -1


def createEditBufferRequest(channel):
    # Modern style
    return [0xf0, 0x01, tempest["device_id"], 0b00000110, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == tempest["device_id"]
            and message[3] == 0x63)  # Sound Data


def numberOfBanks():
    return tempest["banks"]


def numberOfPatchesPerBank():
    return tempest["patches_per_bank"]


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    # Modern style
    return [0xf0, 0x01, tempest["device_id"], 0b00000101, bank, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == tempest["device_id"]
            and message[3] == 0b00000010)  # Program Data


def nameFromDump(message):
    # Tempest names are 0 terminated and not fixed length
    dataBlock = unescapeSysex(message[6:-1])
    if len(dataBlock) > 0:
        sound_name = ''
        i = 0
        while dataBlock[i] != 0:
            sound_name += chr(dataBlock[i])
            i += 1
        return sound_name
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        # Have to strip out bank and program, and set command to edit buffer dump
        return message[0:3 + extraOffset()] + [0b00000011] + message[6 + extraOffset():]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message):
        return message[0:3 + extraOffset()] + [0b00000010] + [bank, program] + message[4 + extraOffset():]
    elif isSingleProgramDump(message):
        return message[0:3 + extraOffset()] + [0b00000010] + [bank, program] + message[6 + extraOffset():]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def calculateFingerprint(message):
    raw = getDataBlock(message)
    data = unescapeSysex(raw)
    # Blank out all blank out zones, normally this is the name (or layer names)
    blank_out = [(tempest["name_position"], tempest["name_len"])]
    for zone in blank_out:
        data[zone[0]:zone[0] + zone[1]] = [0] * zone[1]
    return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data


def renamePatch(message, new_name):
    header_len = headerLen(message)
    data = message[header_len:-1]
    data_as_text = "".join([chr(x) for x in message])
    # Name seems to be tilde delimited...
    for i in range(tempest["name_len"]):
        data[tempest["name_position"] + i] = ord(new_name[i]) if i < len(new_name) else ord(' ')
    return message[:header_len] + data + [0xf7]


def getDataBlock(message):
    return message[headerLen(message) + 2:-1]


def headerLen(message):
    if isEditBufferDump(message):
        return 4 + extraOffset()
    elif isSingleProgramDump(message):
        return 6 + extraOffset()
    else:
        raise Exception("Can only work on edit buffer or single program dumps")


def extraOffset():
    return 0


def unescapeSysex(sysex):
    # The Tempest has a different scheme for the high bit as all other DSIs, with a strange 8th byte transmitted after
    # 7 normal bytes. It does not seem to be the MSBs, because that would screw up the ASCII patch name, and
    # I don't see this is a checksum of some sort yet.
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        for i in range(7):
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex])
            dataIndex += 1
        dataIndex += 1
    return result


def unescapeSysex2(sysex):
    # The Tempest has the same scheme for the high bit as all other DSIs, but with the MSB being transmitted after
    # the 7 LSB bytes, not before...
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        msbits = sysex[dataIndex + 7] if dataIndex + 7 < len(sysex) else sysex[-1]
        checksum = 0
        for i in range(7):
            checksum += sysex[dataIndex]
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex])  # | ((msbits & (1 << (7-i))) << (i)))
            dataIndex += 1
        if checksum != msbits:
            print("checksum was ", msbits)
        dataIndex += 1
    return result


def escapeSysex(data):
    raise Exception("fix me")
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


#
# If this is not loaded as a module, but called as a script, run our unit tests
#
if __name__ == "__main__":
    import unittest

    messages = sequential.load_sysex("testData/Tempest_Factory_Sounds_1.0.syx")
    for message in messages:
        print(nameFromDump(message))
    print(len(messages))
    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(this_module,
                                                                         program_dump=messages[1],
                                                                         program_name='Tom Sawyer'))
