#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from ctypes import *
from enum import IntEnum
from typing import List

import knobkraft.sysex
import testing


class DataFormat(IntEnum):
    VOICE_DUMP = 0x00
    PERFORMANCE_DUMP = 0x01
    PERFORMANCE64_FORMAT = 0x02
    VOICE32_DUMP = 0x09
    CONDITION_ACKNOWLEDGE = 125


class OPERATOR_PACKED(Structure):
    _pack_ = 1
    _fields_ = [
        ("first_section", c_uint8 * 11),
        ("scale_left_curve", c_uint8, 2),
        ("scale_right_curve", c_uint8, 2),
        ("unused1", c_uint8, 4),
        ("rate_scale", c_uint8, 3),
        ("detune", c_uint8, 4),
        ("unused2", c_uint8, 1),
        ("amp_mod_sensivity", c_uint8, 2),
        ("key_vel_sensivity", c_uint8, 3),
        ("unused3", c_uint8, 3),
        ("output_level", c_uint8),
        ("osc_mode", c_uint8, 1),
        ("freq_coarse", c_uint8, 5),
        ("unused4", c_uint8, 2),
        ("freq_fine", c_uint8),
    ]


class VOICE_PACKED(Structure):
    _pack_ = 1
    _fields_ = [
        ("operators", OPERATOR_PACKED * 6),
        ("envs", c_uint8 * 8),
        ("algorithm", c_uint8, 5),
        ("unused1", c_uint8, 3),
        ("feedback", c_uint8, 3),
        ("osc_key_sync", c_uint8, 1),
        ("unused1", c_uint8, 4),
        ("lfo", c_uint8 * 4),
        ("lfo_sync", c_uint8, 1),
        ("lfo_wave", c_uint8, 3),
        ("lfo_pitch_mod_sens", c_uint8, 4),
        ("transpose", c_uint8),
        ("name", c_uint8 * 10)
    ]


def name():
    return "Yamaha TX7"


def setupHelp():
    return "The TX7 needs to be set to MIDI channel 1 for this adaptation to work. Beware that the memory protection is ignored, so make sure to not rely on it!"


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 32


def needsChannelSpecificDetection():
    return True


def createDeviceDetectMessage(channel):
    return makeDumpRequest(channel, DataFormat.VOICE_DUMP)


def channelIfValidDeviceResponse(message):
    if isVoiceBulkData(message):
        return message[2] & 0x0f
    return -1


def isOwnSysex(message):
    return (len(message) > 3
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x43)  # Yamaha


def getFormatCode(message):
    return message[3]


def getByteCount(message):
    return message[4] << 7 | message[5]


def isVoiceBulkData(message):
    if (len(message) > 5
            and isOwnSysex(message)
            and getFormatCode(message) == DataFormat.VOICE_DUMP
            and getByteCount(message) == 155):
        return True
    return False


def isPerformanceBulkData(message):
    if (len(message) > 5
            and isOwnSysex(message)
            and getFormatCode(message) == DataFormat.PERFORMANCE_DUMP
            and getByteCount(message) == 94):
        return True
    return False


def makeDumpRequest(channel, format):
    if format not in [DataFormat.VOICE_DUMP, DataFormat.PERFORMANCE_DUMP, DataFormat.PERFORMANCE64_FORMAT,
                      DataFormat.VOICE32_DUMP, 125]:
        raise Exception(f"Invalid format for TX7 dump request given: {format}")
    return [0xf0, 0x43, 0x20 | (channel & 0x0f), format, 0xf7]


def createEditBufferRequest(channel):
    return makeDumpRequest(channel, DataFormat.VOICE_DUMP)


def isEditBufferDump(message):
    return isVoiceBulkData(message)


def isPartOfEditBufferDump(message):
    return isVoiceBulkData(message)


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    raise Exception("Message is not an edit buffer for TX7")


def createBankDumpRequest(channel, bank):
    return makeDumpRequest(channel, DataFormat.VOICE32_DUMP)


def isBankDumpFinished(messages):
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False


def extractPatchesFromBank(message):
    patches = []
    if isPartOfBankDump(message):
        data_block = message[6:-2]
        # Check for hardcoded length of bank dump
        if len(data_block) == (0x20 << 7 | 0x00):
            if checksum(data_block) == message[-2]:
                # Checksum correct
                for i in range(32):
                    voice = packedVoiceToSingleVoice(data_block[i * 128: (i + 1) * 128])
                    patches += singlePatchFromVoice(voice)
                return patches
            print("Checksum error encountered in TX7 bulk dump")
            return []
        print("Got TX7 bulk dump of invalid length - data length is %d but was expected to be %d" % (
            len(data_block), (0x20 << 7)))
    return []


def packedVoiceToSingleVoice(packed):
    # This is the ugly function that needs to unpack 155 bytes from the 128 byte packed data format
    source = VOICE_PACKED.from_buffer_copy(bytearray(packed))
    dest = []
    for op in source.operators:
        dest += op.first_section
        dest.append(op.scale_left_curve)
        dest.append(op.scale_right_curve)
        dest.append(op.rate_scale)
        dest.append(op.amp_mod_sensivity)
        dest.append(op.key_vel_sensivity)
        dest.append(op.output_level)
        dest.append(op.osc_mode)
        dest.append(op.freq_coarse)
        dest.append(op.freq_fine)
        dest.append(op.detune)
    dest += source.envs
    dest.append(source.algorithm)
    dest.append(source.feedback)
    dest.append(source.osc_key_sync)
    dest += source.lfo
    dest.append(source.lfo_sync)
    dest.append(source.lfo_wave)
    dest.append(source.lfo_pitch_mod_sens)
    dest.append(source.transpose)
    dest += source.name
    return dest


def singlePatchFromVoice(voice):
    return [0xf0, 0x43, 0x00, 0x00, 0x01, 0x1b] + voice + [checksum(voice), 0xf7]


def checksum(data_block):
    check_sum = 0
    for d in data_block:
        check_sum -= d
    return check_sum & 0x7f


def isPartOfBankDump(message):
    return isOwnSysex(message) and getFormatCode(message) in [DataFormat.VOICE32_DUMP]


def nameFromDump(message):
    if isEditBufferDump(message):
        start_index = 6
        name = ''.join(chr(x) for x in message[145 + start_index:155 + start_index])
        return name
    raise Exception("Can't extract name from something that is not an edit buffer message")


def renamePatch(message, new_name):
    if isEditBufferDump(message):
        start_index = 6
        new_name_bytes = [ord(c) for c in new_name] + [32] * (10 - len(new_name))
        new_message = message[:start_index + 145] + new_name_bytes + message[start_index + 155:]
        new_checksum = checksum(new_message[6:-2])
        new_message[-1] = new_checksum
        return new_message
    raise Exception("Can only rename edit buffers of the TX7")


def calculateFingerprint(message):
    if isEditBufferDump(message):
        data = message[6:-2]
        # Blank out the 10 name bytes, they should not matter for the fingerprint
        data[145 + 6:155 + 6] = [0] * 10
        return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint edit buffers")


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        first_message = data.all_messages[0]
        assert isPartOfBankDump(first_message)
        patches_list = extractPatchesFromBank(first_message)
        patches = knobkraft.sysex.splitSysex(patches_list)
        assert isEditBufferDump(patches[0])
        yield testing.ProgramTestData(message=patches[0], name="BRASS   1 ")
        yield testing.ProgramTestData(message=patches[3], name="STRINGS 1 ")

    return testing.TestData(sysex="testData/Yamaha_TX7_rom1a.syx",
                            edit_buffer_generator=programs,
                            device_detect_call="f0 43 20 00 f7",
                            device_detect_reply=([0xf0, 0x43, 0x00, 0x00, 0x01, 0x1B], 0), expected_patch_count=32)
