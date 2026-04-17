#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from ctypes import *
import binascii
from typing import List

import knobkraft
import testing


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
    return "Yamaha DX7II"


def createDeviceDetectMessage(channel):
    # The DX7II doesn't have a dedicated detect message, but we're just asking it for the system data to extract the
    # MIDI channel it is set to
    return createUniversalDumpRequest(channel, "LM  ", "8973S ")


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    # No idea how fast the DX7II is
    return 500


def channelIfValidDeviceResponse(message):
    if isUniversalBulkDump(message):
        classification, data_format = getClassFromUniversalBulkDump(message)
        if classification == "LM  " and data_format == "8973S ":
            blocks = getDataBlocksFromUniversalBulkDump(message)
            system_data = blocks[0][10:]
            # Ok, here we should have 85 bytes of system setup parameter data. I can guess that the manual
            # of the DX7s specifies these bytes, as 21 parameters are given and there is "64 bytes more".
            # If that is true, the "MIDI system common message RX channel (device No.) is at index 14
            if system_data[14] != (message[2] & 0x0f):
                print("DX7II system parameter common message RX channel does not seem to be device No. Program error?")
            return message[2] & 0x0f
    return -1


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 32


def createEditBufferRequest(channel):
    # The DX7II and DX7s has proper MIDI functionality
    # it has 4 formats
    # 0 - backward compatible, the single VOICE edit buffer
    # 5 - Supplement edit buffer [called "additional voice edit buffer" in the DX7s]
    # 6 - Packed 32 Supplement [in the DX7s manual this is not called supplement but just "additional 32 voices"
    # 9 - Packed 32 Voice, a bank dump
    return createGroupRequest(channel, 0)


def isEditBufferDump(message):
    # It's not really edit buffer dumps, but single voice dumps
    return isOwnSysexOfSubstatusAndGroup(message, 0x00, 0x00)


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        # Patch in the right channel aka device No.
        return [0xf0, 0x43, 0x00 | (channel & 0x0f), 0x00] + message[4:]
    raise Exception("This is not an EditBufferDump")


def createBankDumpRequest(channel, bank):
    # A bank is a single sysex message containing 32 voices (patches). Ask for it.
    return createGroupRequest(channel, 9)


def isPartOfBankDump(message):
    # Format 9 (non-universal bulk dump) is the 32 packed voice message
    return isOwnSysexOfSubstatusAndGroup(message, 0x00, 9) or \
           isOwnSysexOfSubstatusAndGroup(message, 0x00, 6) or \
           isParameterChange(message) or \
           isUniversalBulkDump(message)


def isBankDumpFinished(messages):
    # We only need a single bank dump message
    return any([isPartOfBankDump(m) for m in messages])


def extractPatchesFromBank(message):
    patches = []
    if isOwnSysexOfSubstatusAndGroup(message, 0x00, 9):
        data_block = message[6:-2]
        # Check for hardcoded length of bank dump
        if len(data_block) == (0x20 << 7 | 0x00):
            if checksum(data_block) == message[-2]:
                # Checksum correct
                for i in range(32):
                    voice = packedVoiceToSingleVoice(data_block[i * 128: (i + 1) * 128])
                    patches += singlePatchFromVoice(voice)
                return patches
            print("Checksum error encountered in DX7 bulk dump")
            return []
        print("Got DX7 bulk dump of invalid length - data length is %d but was expected to be %d" % (
            len(data_block), (0x20 << 7)))
    elif isPartOfBankDump(message):
        if isUniversalBulkDump(message):
            blocks = getDataBlocksFromUniversalBulkDump(message)
            print("Ignoring DX7II sysex message of class %s and format %s" % getClassFromUniversalBulkDump(message))
        elif isParameterChange(message):
            print("Ignoring DX7II parameter change message: g:%d h:%s p:%s v:%s" % getParameterChanged(message))
        elif isOwnSysexOfSubstatusAndGroup(message, 0x00, 6):
            print("Ignoring DX7II packed 32 supplement message")
        else:
            print("Ignoring " + repr(binascii.b2a_hex(bytearray(message))))
    else:
        raise Exception("Program error - Got message which is not part of bank dump in extract patches from bank")


def nameFromDump(message):
    if isEditBufferDump(message):
        data_block = message[6:-2]
        if len(data_block) == 155:
            # The last 10 bytes of the data block of the common message are the name
            return "".join([chr(x) for x in data_block[-10:]])
    raise Exception("Can only extract a name from an edit buffer dump")


def setupHelp():
    return "On the DX7II/DX7IId, you need to turn memory protection off (Button #14), and set MIDI IN to normal (button #29).\n\n" \
           "Without MIDI IN normal it will not accept the sysex data but give no error message.\n\n" \
           "The second bank in the DX7s cannot be addressed from the computer, but you have to select the transmit and "\
           "receive block to be used for the bank requests in the MIDI menu of the synth (button #32).\n"\
           "Actually they could be set using parameter transfer system setup parameter #76 and #77. "\
           "It is just not implemented in the adaptation. Feel free to edit!"


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


def isUniversalBulkDump(message):
    return isOwnSysexOfSubstatusAndGroup(message, 0x00, 0x7e)


def getClassFromUniversalBulkDump(message):
    if isUniversalBulkDump(message):
        return "".join([chr(c) for c in message[6:10]]), "".join([chr(c) for c in message[10:16]])
    raise Exception("Need universal bulk dump message here")


def getDataBlocksFromUniversalBulkDump(message):
    if isUniversalBulkDump(message):
        # The Universal Bulk Dump can contain repeats of data blocks. This is important to know to parse it
        # and calculate the length and checksum correctly...
        blocks = []
        read_ptr = 4
        while read_ptr < len(message) and message[read_ptr] != 0xf7:
            data_len = (message[read_ptr] << 7) | message[read_ptr + 1]
            read_ptr += 2
            data_block = message[read_ptr:read_ptr + data_len]
            read_ptr += data_len
            if len(data_block) != data_len:
                raise Exception("Corrupted data - data block in universal bulk dump doesn't contain enough data")
            expected_chk = message[read_ptr]
            read_ptr += 1
            if expected_chk != checksum(data_block):
                raise Exception("Corrupted data - invalid checksum in universal bulk dump.")
            blocks.append(data_block)
        return blocks


def isParameterChange(message):
    return (len(message) > 5 and message[0] == 0xf0
            and message[1] == 0x43  # Yamaha
            and (message[2] & 0xf0) == 0x10)


def getParameterChanged(message):
    if isParameterChange(message):
        g = message[3] >> 2
        h = message[3] & 0x03
        p = message[4]
        v = message[5]
        return g, h, p, v
    raise Exception("Can only extract parameter info from a parameter change message!")


def isOwnSysexOfSubstatusAndGroup(message, substatus, group):
    return (len(message) > 5 and message[0] == 0xf0
            and message[1] == 0x43  # Yamaha
            and (message[2] & 0xf0) == substatus
            and message[3] == group)



def createGroupRequest(channel, group):
    return [0xf0, 0x43, 0x20 | (channel & 0x0f), group, 0xf7]


def createUniversalDumpRequest(channel, classification_name, data_format_name):
    if len(classification_name) != 4:
        raise Exception("classification_name needs to be a string of length 4, most likely 'LM  '")
    if len(data_format_name) != 6:
        raise Exception("data_format_name must be a string of length 6, like '8973PE'")
    return [0xf0, 0x43, 0x20 | (channel & 0x0f), 0x7e] + [ord(c) for c in classification_name] + [ord(c) for c in
                                                                                                  data_format_name] + [0xf7]


def checksum(data_block):
    check_sum = 0
    for d in data_block:
        check_sum -= d
    return check_sum & 0x7f


def make_test_data():

    def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        names_of_first_program = ["Talkbox001", "OrganX01..", "unknown"]
        count = 0
        for message in test_data.all_messages:
            assert isPartOfBankDump(message)
            patch_data = extractPatchesFromBank(message)
            if patch_data is not None:
                patches = knobkraft.sysex.splitSysex(patch_data)
                yield testing.ProgramTestData(message=patches[0], name=names_of_first_program[count])
                count += 1
            if isUniversalBulkDump(message):
                classification, data_format = getClassFromUniversalBulkDump(message)
                if classification == "LM  " and data_format == "8973S ":
                    blocks = getDataBlocksFromUniversalBulkDump(message)
                    system_data = blocks[0][10:]
                    print("MIDI transmit channel", system_data[0])
                    print("MIDI receive channel 1", system_data[2])
                    print("MIDI receive channel 2", system_data[3])
                    print("MIDI device ID", system_data[14])

    return testing.TestData(sysex=R"testData/yamahaDX7II-STUDIOREINE BANK.syx", edit_buffer_generator=make_patches, expected_patch_count=64)
