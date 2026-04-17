#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from enum import IntEnum
from typing import List

import knobkraft.sysex
import testing

g_device_id = 0x00

WALDORF_MIDI_ID = 0x3e
MICROWAVE_1_EQUIPMENT_ID = 0x00


class IDM(IntEnum):
    VERSION_NUMBER_REQUEST = 0x00
    DEVICE_STATUS_REQUEST = 0x01
    BASIC_PROGRAM_DUMP_REQUEST = 0x02
    ARR_DUMP_REQUEST = 0x03
    WAVE_DUMP_REQUEST = 0x04
    WAVE_CONTROL_TABLE_REQUEST = 0x05
    TUNING_TABLE_REQUEST = 0x06
    VELOCITY_TABLE_REQUEST = 0x07
    BPR_CHANGE_MAP_REQUEST = 0x08
    ARR_CHANGE_MAP_REQUEST = 0x09

    BPR_BANK_DUMP_REQUEST = 0x10
    ARR_BANK_DUMP_REQUEST = 0x11
    TABLE_DUMP_REQUEST = 0x12
    USER_WAVETABLE_REQUEST = 0x13
    CARTRIDGE_DUMP_REQUEST = 0x14
    ARR_DUMP_WITH_BPR_REQUEST = 0x15


def name():
    return "Waldorf MicroWave"


def setupHelp():
    return "This adaptation supports both software versions V1.0 and V2.0 of the MicroWave. " \
            "This is the MicroWave 1 of course, " \
            "which was not called 1 until the MicroWave 2 came along."


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 64


def friendlyProgramName(programNo):
    return f"A{programNo+1:02d}"



def needsChannelSpecificDetection():
    return False


def createDeviceDetectMessage(channel):
    return makeDumpRequest(0x7f, IDM.DEVICE_STATUS_REQUEST)


def channelIfValidDeviceResponse(message):
    global g_device_id
    if isOwnSysex(message):
        if getFormatCode(message) == (IDM.DEVICE_STATUS_REQUEST | 0x40):
            g_device_id = message[7]
            return g_device_id
    return -1


def isOwnSysex(message):
    return (len(message) > 3
            and message[0] == 0xf0  # Sysex
            and message[1] == WALDORF_MIDI_ID
            and message[2] == MICROWAVE_1_EQUIPMENT_ID)


def getFormatCode(message):
    return message[4]


def makeDumpRequest(device_id, idm):
    return [0xf0, WALDORF_MIDI_ID, MICROWAVE_1_EQUIPMENT_ID, device_id, idm, 0, 0xf7]


def checksum(data):
    checksum = 0
    for b in data:
        checksum = checksum + b
    return checksum & 0x7f


def fillChecksum(list):
    if len(list) < 8:
        return list
    else:
        list[-2] = checksum(list[5:-2])
        return list


def createEditBufferRequest(channel):
    if channel != g_device_id:
        print(f"Microwave warning: Autodetection gave device ID {g_device_id}, but user specified {channel}. Using user's.")
    return makeDumpRequest(channel, IDM.BASIC_PROGRAM_DUMP_REQUEST)


def isEditBufferDump(message):
    return isOwnSysex(message) and getFormatCode(message) == (IDM.BASIC_PROGRAM_DUMP_REQUEST | 0x40)


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    raise Exception("Message is not an edit buffer for Waldorf Microwave 1")


def createBankDumpRequest(channel, bank):
    return makeDumpRequest(channel, IDM.BPR_BANK_DUMP_REQUEST)


def isBankDumpFinished(messages):
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False


def extractPatchesFromBank(message):
    patches = []
    if isPartOfBankDump(message):
        data_block = message[5:-2]
        # Check for hardcoded length of bank dump
        if len(data_block) == 64*180:
            if checksum(data_block) == message[-2]:
                # Checksum correct
                for i in range(64):
                    bpr = data_block[i * 180: (i + 1) * 180]
                    newpatch = fillChecksum(makeDumpRequest(g_device_id, IDM.BASIC_PROGRAM_DUMP_REQUEST|0x40)[:-2] + \
                        bpr + [0x00, 0xf7])
                    patches += newpatch
                    wavetable = newpatch[28]
                    if wavetable > 31:
                        print(f"Warning: Patch {nameFromDump(newpatch)} uses user wave table {wavetable} which may not be loaded!")
                return patches
            print("Checksum error encountered in MW1 bulk dump")
            return []
        print("Got MW1 bulk dump of invalid length - data length is %d but was expected to be %d" % (
            len(data_block), (64*180)))
    return []


def isPartOfBankDump(message):
    return isOwnSysex(message) and getFormatCode(message) == (IDM.BPR_BANK_DUMP_REQUEST | 0x40)


def nameFromDump(message):
    if isEditBufferDump(message):
        start_index = 153
        name = ''.join(chr(x) for x in message[start_index:16 + start_index])
        return name
    raise Exception("Can't extract name from something that is not an edit buffer message of the MW1")


def renamePatch(message, new_name):
    if isEditBufferDump(message):
        start_index = 153
        new_name_bytes = [ord(c) for c in new_name] + [32] * (16 - len(new_name))
        new_message = message[:start_index] + new_name_bytes + message[start_index + 16:]
        return fillChecksum(new_message)
    raise Exception("Can only rename edit buffers of the MW1")


def calculateFingerprint(message):
    if isEditBufferDump(message):
        # Blank out the 16 name bytes, they should not matter for the fingerprint
        message[153:153 + 16] = [0] * 16
        return hashlib.md5(bytearray(message)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint edit buffers")


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        for message in data.all_messages:
            if isPartOfBankDump(message):
                patches_list = extractPatchesFromBank(message)
                patches = knobkraft.sysex.splitSysex(patches_list)
                assert isEditBufferDump(patches[0])
                yield testing.ProgramTestData(message=patches[0], name="Real Bd       CB")
                yield testing.ProgramTestData(message=patches[3], name="808 Bd 3      CB")

    return testing.TestData(sysex="testData/Waldorf MW1_C BRUSE DRUM CARD SINGLE.syx",
                            edit_buffer_generator=programs,
                            device_detect_call="f0 3e 00 7f 01 00 f7",
                            device_detect_reply=([0xf0, 0x3e, 0x00, 0x00, 0x41, 0x00, 0x00, 0x03], 0x03),
                            expected_patch_count=65)
