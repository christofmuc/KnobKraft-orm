#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys
import sequential

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
    raise Exception("Tempest cannot be queried, use the manual dump window!")


def isEditBufferDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == tempest["device_id"]
            and message[3] == 0x60)  # Sound Data in RAM


def numberOfBanks():
    return tempest["banks"]


def numberOfPatchesPerBank():
    return tempest["patches_per_bank"]


def createProgramDumpRequest(channel, patchNo):
    raise Exception("Tempest cannot be queried, use the manual dump window!")


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == tempest["device_id"]
            and message[3] == 0x63)  # Sound from FLASH memory


def nameFromDump(message):
    if isSingleProgramDump(message):
        # Tempest names are 0 terminated and not fixed length
        dataBlock = unescapeSysex(message[6:-1])
        if len(dataBlock) > 0:
            sound_name = ''
            i = 0
            while dataBlock[i] != 0:
                sound_name += chr(dataBlock[i])
                i += 1
            return sound_name
    elif isEditBufferDump(message):
        return f"RAM sound {message[4]:02X}"
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    if isSingleProgramDump(message):
        print("We actually cannot convert the flash sound to a RAM sound, sending it into original position instead")
        return message
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    if isEditBufferDump(message):
        print("We actually cannot convert the RAM sound to a flash sound, sending it into original RAM position instead")
        return message
    elif isSingleProgramDump(message):
        print("Cannot change position of Tempest FLASH sound, sending it into original position instead")
        return message
    raise Exception("Neither edit buffer nor program dump - can't be converted")


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


#
# If this is not loaded as a module, but called as a script, run our unit tests
#


if __name__ == "__main__":
    import unittest

    messages = sequential.load_sysex("testData/Tempest_Factory_Sounds_1.0.syx")
    #for message in messages:
        #print(nameFromDump(message))
    #print(len(messages))

    flash_sound = sequential.load_sysex("testData/Tempest Basic Sound A12 Kick Midi Din from FLASH.syx")
    assert(isSingleProgramDump(flash_sound[0]))
    assert(nameFromDump(flash_sound[0]) == "/S/Kicks/Basic")

    ram_sound = sequential.load_sysex("testData/Tempest Basic Sound A12 Kick Midi Din from RAM.syx")[0]
    assert(not isSingleProgramDump(ram_sound))
    assert(isEditBufferDump(ram_sound))
    assert(nameFromDump(ram_sound) == "RAM sound 12")

    as1_sound = sequential.load_sysex("testData/Tempest 909 AS1 RAM.syx")[0]
    assert(isEditBufferDump(as1_sound))
    assert(nameFromDump(as1_sound) == "RAM sound 02")

    flash_sound = sequential.load_sysex("testData/Tempest 909 AS1 FLASH.syx")[0]
    assert(isSingleProgramDump(flash_sound))
    assert(nameFromDump(flash_sound) == "/S/Kicks/909 AS 1")

    as1_sound = sequential.load_sysex("testData/Tempest Analog 808 Kick v01 RAM.syx")[0]
    assert(isEditBufferDump(as1_sound))
    assert(nameFromDump(as1_sound) == "RAM sound 01")

    flash_sound = sequential.load_sysex("testData/Tempest Analog 808 Kick v01 FLASH.syx")[0]
    assert(isSingleProgramDump(flash_sound))
    assert(nameFromDump(flash_sound) == "/S/Kicks/ANALOG 808 KICK V1")


    #unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(this_module,
    #                                                                     program_dump=messages[1],
    #                                                                     program_name='/S/Toms/Zap~~Lo'))
