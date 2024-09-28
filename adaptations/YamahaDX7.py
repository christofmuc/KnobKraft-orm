#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from ctypes import *
from typing import List

import knobkraft.sysex
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
    return "Yamaha DX7"


def createDeviceDetectMessage(channel):
    # The DX7 cannot be auto detected, because it doesn't reply to any message sent. Just return an empty MIDI message
    return [0xf0, 0xf7]


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    # If this value is negative, the KnobKraft Orm will skip the auto-detection altogether (HACK!)
    return -1


def channelIfValidDeviceResponse(message):
    # The DX7 cannot be auto detected, because it doesn't reply to any message sent
    return -1


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 32


def createEditBufferRequest(channel):
    # Can the DX7 actually do that? I don't think so.
    return [0xf0, 0xf7]


def isEditBufferDump(message):
    # It's not really edit buffer dumps, but single voice dumps
    return len(message) > 5 and message[:6] == [0xf0, 0x43, 0x00, 0x00, 0x01, 0x1b]


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    raise Exception("This is not an EditBufferDump")


def createBankDumpRequest(channel, bank):
    # A bank is a single sysex message containing 32 voices (patches). Ask for it.
    return [0xf0, 0x43, 0x20, 0x09, 0xf7]


def isPartOfBankDump(message):
    return len(message) > 5 and message[:6] == [0xf0, 0x43, 0x00, 0x09, 0x20, 0x00]


def isBankDumpFinished(messages):
    # We only need a single bank dump message
    return any([isPartOfBankDump(m) for m in messages])


def list_compare(list1, list2):
    worked = True
    for i in range(len(list1)):
        if list1[i] != list2[i]:
            print(f"Position {i}: {list1[i]} vs {list2[i]}")
            worked = False
    assert worked



def extractPatchesFromBank(message):
    patches = []
    if isPartOfBankDump(message):
        data_block = message[6:-2]
        # Check for hardcoded length of bank dump
        if len(data_block) == (0x20 << 7 | 0x00):
            if checksum(data_block) == message[-2]:
                # Checksum correct
                for i in range(32):
                    packed_voice = data_block[i * 128: (i + 1) * 128]
                    voice = packedVoiceToSingleVoice(packed_voice)
                    reverse = singleVoiceToPackedVoice(voice)
                    list_compare(packed_voice, reverse)
                    patches += singlePatchFromVoice(voice)
                return patches
            print("Checksum error encountered in DX7 bulk dump")
            return []
        print("Got DX7 bulk dump of invalid length - data length is %d but was expected to be %d" % (
        len(data_block), (0x20 << 7)))
    return []


def createBankDump(patches: List[List[int]]):
    # Build a bank dump message from a list of individual patches
    for patch in patches:
        if isEditBufferDump(patch):
            # This is a single voice dump, extract the
            voice_data = patch[6:-2]


def nameFromDump(message):
    if isEditBufferDump(
            message):
        data_block = message[6:-2]
        if len(data_block) == 155:
            # The last 10 bytes of the data block of the common message are the name
            return "".join([chr(x) for x in data_block[-10:]])
    raise Exception("Can only extract a name from a single program dump")


def setupHelp():
    return "The Yamaha DX7 is an early synth, it's MIDI implementation is really simple.\n" \
        "Sorry to say that you cannot request the edit buffer or a bank dump from the computer, you need" \
        " to use the receive manual dump function and initiate the dumps from the DX7.\n\n" \
        "To send a patch to the DX7 for audition, make sure the INTERNAL MEMORY PROTECT is set to off."


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


def singleVoiceToPackedVoice(single):
    # Initialize the packed VOICE_PACKED structure
    packed = VOICE_PACKED()

    index = 0

    # Fill in the operators
    for op in packed.operators:
        op.first_section[:] = single[index:index + 11]
        index += 11
        op.scale_left_curve = single[index] & 0x03
        op.scale_right_curve = (single[index] >> 2) & 0x03
        index += 1
        op.rate_scale = single[index] & 0x07
        op.detune = (single[index] >> 3) & 0x0F
        index += 1
        op.amp_mod_sensivity = single[index] & 0x03
        op.key_vel_sensivity = (single[index] >> 2) & 0x07
        index += 1
        op.output_level = single[index]
        index += 1
        op.osc_mode = single[index] & 0x01
        op.freq_coarse = (single[index] >> 1) & 0x1F
        index += 1
        op.freq_fine = single[index]
        index += 1
        op.detune = single[index]
        index += 1

    # Fill in the envelope data
    packed.envs[:] = single[index:index + 8]
    index += 8

    # Fill in the algorithm, feedback, and oscillator key sync
    packed.algorithm = single[index] & 0x1F
    index += 1
    packed.feedback = single[index] & 0x07
    packed.osc_key_sync = (single[index] >> 3) & 0x01
    index += 1

    # Fill in the LFO data
    packed.lfo[:] = single[index:index + 4]
    index += 4
    packed.lfo_sync = single[index] & 0x01
    packed.lfo_wave = (single[index] >> 1) & 0x07
    packed.lfo_pitch_mod_sens = (single[index] >> 4) & 0x0F
    index += 1

    # Fill in the transpose value
    packed.transpose = single[index]
    index += 1

    # Fill in the voice name
    packed.name[:] = single[index:index + 10]

    # Convert the packed structure to a byte buffer
    packed_data = bytes(packed)

    # Return the packed data as a bytearray
    return list(bytearray(packed_data))


def singlePatchFromVoice(voice):
    return [0xf0, 0x43, 0x00, 0x00, 0x01, 0x1b] + voice + [checksum(voice), 0xf7]


def checksum(data_block):
    check_sum = 0
    for d in data_block:
        check_sum -= d
    return check_sum & 0x7f


def make_test_data():

    def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        flat_list_of_messages = [item for sublist in test_data.all_messages for item in sublist]
        assert isBankDumpFinished(test_data.all_messages)
        patches = knobkraft.sysex.splitSysex(extractPatchesFromBank(flat_list_of_messages))
        assert len(patches) == 32
        yield testing.ProgramTestData(patches[0], name="SYN-LEAD 2")

    return testing.TestData(sysex=R"testData/yamahaDX7-ROM2B.SYX", edit_buffer_generator=make_patches)
