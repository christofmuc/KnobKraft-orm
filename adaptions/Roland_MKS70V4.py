#
#   Copyright (c) 2020-2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib
import knobkraft
from typing import Dict, List

MIDI_channel_A = 0  # MIDI function 21
MIDI_channel_B = 1  # MIDI function 31
MIDI_control_channel = 0  # MIDI function 11. In other synths, this is called the sysex device ID. but here it is used for program change messages

roland_id = 0b01000001  # = 0x41

operation_pgr = 0b00110100  # = 0x34
operation_apr = 0b00110101  # = 0x35
operation_ipr = 0b00110110  # = 0x36
operation_bld = 0b00110111  # = 0x37
operation_vec_apr = 0x38  # Vecoven (OS 4) All Parameters
operation_vec_bld = 0x3a  # Vecoven (OS 4) Bulk Dump

format_type_jx10 = 0b00100100  # = 0x24

patch_level = 0b00110000  # = 0x30
tone_level = 0b00100000  # = 0x20

tone_a = 0b00000001
tone_b = 0b00000010


#
# The MKS-70 has multiple formats, and generally devices its data into patches and tones
# The Tones are more traditionally the sounds, while the patches aggregate two tones plus play controls into a "performance"
#
# A patch has 51 bytes of data, of which the first 18 are the name
# A tone has 59 bytes of data, of which the first 10 are the name
#
# The two tones used by a patch are addressed by tone number
# The synth stores 50 tones internally and 50 in the cartridge
# plus 64 patches internally and 64 in the cartridge
#

def name():
    return "Roland MKS-70 V4"


def setupHelp():
    return """
This adaptation is for the Roland MKS-70 with the Vecoven V4 ROM.
Also known as the PWM mod, this new EPROM also brings a new sysex format
which has the advantage of being documented, and so we can load 
bulk dumps, dissect them, and convert the patches and tones into edit buffer
messages to audition sounds without changing the synth's memory.
"""


def createDeviceDetectMessage(channel):
    # If I interpret the sysex docs correctly, we can just send a MIDI program change, and it should reply with sysex
    # Just request Tone #0
    return createProgramDumpRequest(channel & 0x0f, 128)


def deviceDetectWaitMilliseconds():
    return 150


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    global MIDI_control_channel
    # We expect an APR message for a patch
    if isAprMessage(message):
        if isToneAprMessage(message):
            MIDI_control_channel = message[3]
            return message[3]
    return -1


def bankDescriptors() -> List[Dict]:
    return [{"bank": 0, "name": "Internal Patches", "size": 64, "type": "Patch"},
            {"bank": 1, "name": "Cartridge Patches", "size": 64, "type": "Patch"}]


def createMessageHeader(opcode, channel, level):
    return [0xf0, roland_id, opcode, channel & 0x0f, format_type_jx10, level]


def createPgrMessage(patch_no, tone_group):
    # Check if we are requesting a patch or a tone
    if 0 <= patch_no < 128:
        return createMessageHeader(operation_pgr, MIDI_control_channel, patch_level) + [0x01, 0x00, patch_no, 0x00, 0xf7]
    elif 128 <= patch_no < 228:
        tone_number = patch_no - 128
        return createMessageHeader(operation_pgr, MIDI_control_channel, tone_level) + [tone_group, 0x00, tone_number, 0x00, 0xf7]
    raise Exception(f"Invalid patch_no given to createPgrMessage: {patch_no}")


def createProgramDumpRequest(channel: int, patch_no: int):
    # Check if we are requesting a patch or a tone
    if 0 <= patch_no < 128:
        # This is a patch. To make the synth send the patch, we need to send it a program change message on its control channel
        return [0b11000000 | channel, patch_no]
    elif 128 <= patch_no < 228:
        tone_number = patch_no - 128
        # TODO I don't understand how MIDI_channel_A and MIDI_channel_B influence this command
        return [0b11000000 | MIDI_channel_A, tone_number]
    else:
        raise Exception(f"Invalid patch number given to createProgramDumpRequest: {patch_no}")


def isOperationMessage(message: List[int], operations: List[int]) -> bool:
    if len(message) > 5:
        if (message[0] == 0xF0 and
                message[1] == roland_id and
                message[2] in operations and
                message[4] == format_type_jx10):
            return True
    # If the message does not match the expected format, return False
    return False


def isBulkMessage(message: List[int]) -> bool:
    # Refuse to load old (pre V4) bulk messages for now, until we have successfully reverse engineered them
    return isOperationMessage(message, [operation_vec_bld])


def isAprMessage(message: List[int]) -> bool:
    return isOperationMessage(message, [operation_apr, operation_vec_apr])


def isToneAprMessage(message: List[int]) -> bool:
    return isAprMessage(message) and message[5] == tone_level


def isPatchAprMessage(message: List[int]) -> bool:
    return isAprMessage(message) and message[5] == patch_level


def isProgramNumberMessage(message: List[int]) -> bool:
    return isOperationMessage(message, [operation_pgr])


def isPartOfSingleProgramDump(message: List[int]) -> bool:
    return isToneAprMessage(message) or isPatchAprMessage(message) or isProgramNumberMessage(message)


# def bldToApr(bld_message: List[int]):
#     if isBulkMessage(bld_message):
#         if bld_message[5] == patch_level:
#             # This is a patch BLD
#             patch_number = bld_message[8]
#             patch_data = denibble(bld_message[9:-1])
#             pgr_message = createPgrMessage(patch_number, None)
#             assert isProgramNumberMessage(pgr_message)
#             # assert len(patch_data) == 51
#             apr_message = createMessageHeader(operation_apr, MIDI_control_channel, patch_level) + [0x01] + patch_data + [0xf7]
#             assert isPatchAprMessage(apr_message)
#             return pgr_message, apr_message
#         elif bld_message[5] == tone_level:
#             # This is a tone, construct a proper single program dump (APR) message for it
#             tone_number = bld_message[8]
#             tone_data = bld_message[9:-1]
#             pgr_message = createPgrMessage(tone_number + 128, tone_a)  # offset by 128 to make sure it is an internal tone number, not a patch number
#             assert isProgramNumberMessage(pgr_message)
#             assert len(tone_data) == 59
#             apr_message = [0xf0, roland_id, operation_apr, MIDI_control_channel, format_type_jx10, tone_level, tone_a] + tone_data + [0xf7]
#             assert isToneAprMessage(apr_message)
#             return pgr_message, apr_message


def createEditBufferRequest(channel):
    # I have not found a way to request the edit buffer from the synth, that is, the transient state of patch and tones.
    # You need to send a program change, but that would recall that program and erase the transient edit buffer.
    # Do nothing here.
    return []


def isEditBufferDump(messages):
    # We define an edit buffer as one to three APR messages
    # one APR patch
    # one APR Tone A (upper), optional because it might be a ROM tone used
    # one APR Tone B (lower), optional because it might be a ROM tone used
    index = knobkraft.sysex.findSysexDelimiters(messages)
    if len(index) < 1:
        return False
    if not isAprMessage(messages[index[0][0]:index[0][1]]):
        return False
    if len(index) > 1 and not isAprMessage(messages[index[1][0]:index[1][1]]):
        return False
    if len(index) > 2 and not isAprMessage(messages[index[2][0]:index[2][1]]):
        return False
    return True


def convertToEditBuffer(channel, message):
    # TODO - channel remapping is not yet implemented
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        messages = knobkraft.findSysexDelimiters(message, 6)
        result = []
        # Strip the PRG messages from the Single Program Dump to convert it to an edit buffer
        for index in messages:
            m = message[index[0]:index[1]]
            if isAprMessage(m):
                result.extend(m)
        if len(result) > 0:
            return result
        raise Exception("Failed to convert single program dump to edit buffer - program error?")
    raise Exception("Can only convert edit buffer dumps or single program dumps to edit buffer dumps!")


def isSingleProgramDump(message):
    indexes = knobkraft.sysex.findSysexDelimiters(message)
    num_prg = 0
    num_apr = 0
    num_patch = 0
    num_tone = 0
    for index in indexes:
        m = message[index[0]:index[1]]
        if isProgramNumberMessage(m):
            num_prg += 1
        elif isAprMessage(m):
            num_apr += 1
            if isPatchAprMessage(m):
                num_patch += 1
            elif isToneAprMessage(m):
                num_tone += 1
    return num_prg == num_apr and num_patch == 1 and num_tone < 3


def getToneNumbers(message: List[int]) -> (int, int):
    if message[5] == patch_level:
        if message[2] == operation_apr:
            # This is a classic APR message from Roland
            upper_tone_number = message[29]
            lower_tone_number = message[38]
            return upper_tone_number, lower_tone_number
        elif message[2] == operation_vec_apr:
            # This is a V4 APR message
            upper_tone_number = message[36]
            lower_tone_number = message[45]
            return upper_tone_number, lower_tone_number
    raise Exception("Program error - can only extract tone numbers from Patch APR messages")


def convertToProgramDump(channel, message, program_number):
    # TODO - channel remapping is not yet implemented
    if isEditBufferDump(message):
        # The edit buffer messages can be sent unmodified, but need to be prefixed with a PRG message each
        index = knobkraft.sysex.findSysexDelimiters(message)
        result = []
        tone_group = 0
        tone_groups = [tone_a, tone_b]
        tone_numbers = []
        for i in index:
            m = message[i[0]:i[1]]
            if isPatchAprMessage(m):
                result.extend(createPgrMessage(program_number, 0x01))
                result.extend(m)
                upper_tone, lower_tone = getToneNumbers(m)
                tone_numbers = [upper_tone, lower_tone]
                if upper_tone >= 50:
                    tone_group = 1
            elif isToneAprMessage(m):
                result.extend(createPgrMessage(128 + tone_numbers[tone_group], tone_groups[tone_group]))
                result.extend(m)
                tone_group += 1

        return result
    elif isSingleProgramDump(message):
        # A program dump is an PGR message which tells the synth where to store the following APR data. We might want to change
        # the program position, however, so we need to create a new program message and just keep the APR message
        messages = knobkraft.sysex.splitSysexMessage(message)
        result = createPgrMessage(program_number, 0x01)
        result.extend(message[messages[1][0]:])  # Keep everything after that. Note this will keep the tones at their original position,
                                                 # possibly overwriting other tones there. This needs more intelligent memory management
        return result
    raise Exception(f"Failed to convert to single program dump: {message}")


def nameFromDump(message):
    name_message = message
    if isSingleProgramDump(message):
        messages = knobkraft.sysex.findSysexDelimiters(message, 2)
        # This is a list of PRG and APR messages, use the patch message
        name_message = message[messages[1][0]:]

    if isToneAprMessage(name_message):
        name_length = 10
    elif isPatchAprMessage(name_message):
        name_length = 18
    else:
        raise Exception("Message is neither tone nor patch, can't extract name")
    base = 7
    patch_name_bytes = name_message[base:base + name_length]
    patch_name = ''.join(chr(byte if byte >= 10 else byte + 48) for byte in patch_name_bytes)
    return patch_name


def isPartOfBankDump(message):
    return isBulkMessage(message)


def isBankDumpFinished(messages):
    return len(messages) == 64 + 50


g_bank_messages = []


# def denibble(data):
#     unpacked_data = []
#     for i in range(0, len(data), 2):
#         byte = (data[i + 1] & 0x0F) | ((data[i] & 0x0F) << 4)
#         unpacked_data.append(byte)
#     return unpacked_data

def calculateFingerprint(message):
    return hashlib.md5(bytearray(message[7:])).hexdigest()
    # if isEditBufferDump(message):
    #     # Just take the bytes after the name
    #     if isToneAprMessage(message):
    #         name_len = 10
    #     elif isPatchAprMessage(message):
    #         name_len = 18
    #     else:
    #         raise Exception("Message in Edit Buffer needs to be either a Tone message or a Patch message")
    #     return hashlib.md5(bytearray(message[7 + name_len:-1])).hexdigest()
    # elif isSingleProgramDump(message):
    #     messages = knobkraft.findSysexDelimiters(message, 2)
    #     second_message = message[messages[1][0]:messages[1][1]]
    #     if isToneAprMessage(second_message):
    #         name_len = 10
    #     elif isPatchAprMessage(second_message):
    #         name_len = 18
    #     else:
    #         raise Exception("Message in Edit Buffer needs to be either a Tone message or a Patch message")
    #     return hashlib.md5(bytearray(second_message[7 + name_len:-1])).hexdigest()
    # # If the message is not a program dump or part of a bank dump, return an empty string
    # raise Exception("Can't calculate fingerprint of non-edit buffer or program dump message")


bulk_mapping_patch = {
    "Char1": 9,
    "Char2": 10,
    "Char3": 11,
    "Char4": 12,
    "Char5": 13,
    "Char6": 14,
    "Char7": 15,
    "Char8": 17,  # Surprise, not even the name is stored continuously
    "Char9": 18,
    "Char10": 19,
    "Char11": 20,
    "Char12": 21,
    "Char13": 22,
    "Char14": 23,
    "Char15": 25,  # And another byte skipped
    "Char16": 26,
    "Char17": 27,
    "Char18": 28,
    "A/B Balance": 29,
    "Dual Detune": 30,
    "UpperSplitPT": 31,
    "BendRange": [(32, 1, 3, 0), (40, 1, 2, 1), (62, 1, 1, 2)],
    "Keymode": [(32, 1, 1, 0), (40, 1, 3, 1), (40, 1, 4, 2), (40, 1, 7, 3)],
    "LowerSplitPT": 33,
    "Porta Time": 34,
    "Total Volume": 35,
    "AT Vibrato": 36,
    "AT Brilliance": 37,
    "AT Volume": 38,
    "A Tone Nr.": 39,
    "A-Hold": (40, 1, 1),
    "A Chromatic Shift": 41,
    "A Unison Detune": 42,
    "A LFO Mod Dpth": 43,
    "A Bender": 44,
    "B Tone Nr.": 45,
    "B Chromatic Shift": 46,
    "B Unison Detune": 47,
    "B-Hold": (48, 1, 3),
    "A-Porta": (48, 1, 5),
    "B LFO Mod Dpth": 49,
    "B Bender": 50,
    "Chase Level": 51,
    "Chase Time": 52,
    "A-Keyassign": [(53, 1, 6, 0), (53, 1, 7, 1), (56, 1, 3, 2)],
    "B-Keyassign": [(54, 1, 6, 0), (54, 1, 7, 1), (56, 1, 2, 2)],
    "B-Porta": (56, 1, 7),
    "ChaseMode": [(56, 1, 4, 0), (56, 1, 5, 1)],
    "Chase On/Off": [(63, 1, 2, 0), (63, 1, 5, 1)]
}

apr_mapping_patch = {
    "Char1": 7,
    "Char2": 8,
    "Char3": 9,
    "Char4": 10,
    "Char5": 11,
    "Char6": 12,
    "Char7": 13,
    "Char8": 14,
    "Char9": 15,
    "Char10": 16,
    "Char11": 17,
    "Char12": 18,
    "Char13": 19,
    "Char14": 20,
    "Char15": 21,
    "Char16": 22,
    "Char17": 23,
    "Char18": 24,
    "A/B Balance": 25,
    "Dual Detune": 26,
    "UpperSplitPT": 27,
    "LowerSplitPT": 28,
    "Porta Time": 29,
    "BendRange": [(30, 2, 5, 0), (59, 1, 0, 2)],
    "Keymode": [(31, 2, 0, 0), (58, 2, 0, 2)],
    "Total Volume": 32,
    "AT Vibrato": 33,
    "AT Brilliance": 34,
    "AT Volume": 35,
    "A Tone Nr.": 36,
    "A Chromatic Shift": 37,
    "A-Keyassign": (38, 3, 0),
    "A Unison Detune": 39,
    "A-Hold": (40, 1, 0),
    "A LFO Mod Dpth": 41,
    "A-Porta": (42, 1, 0),
    "A Bender": 43,
    "B Tone Nr.": 45,
    "B Chromatic Shift": 46,
    "B-Keyassign": (47, 3, 0),
    "B Unison Detune": 48,
    "B-Hold": (49, 1, 0),
    "B LFO Mod Dpth": 50,
    "B-Porta": (51, 1, 0),
    "B Bender": 52,
    "Chase Level": 54,
    "Chase Time": 55,
    "ChaseMode": (56, 2, 0),
    "Chase On/Off": (57, 2, 0),
}

bulk_mapping_tone = {
    "Char1": 9,
    "Char2": 10,
    "Char3": 11,
    "Char4": 12,
    "Char5": 13,
    "Char6": 14,
    "Char7": 15,
    "Char8": 16,
    "Char9": 17,
    "Char10": 18,
    "Char11": 19,
    "DCO1_tune": 20,
    "DCO1_lfoDpth": 21,
    "DCO1_envDpth": 22,
    "DCO2_tune": 23,
    "DCO2_fineTun": 24,
    "DCO2_lfoDpth": 25,
    "DCO2_envDpth": 26,
    "PWM1_width": 27,
    "PWM1_envDpth": 28,
    "PWM1_lfoDpth": 29,
    "PWM2_width": 30,
    "PWM2_envDpth": 31,
    "PWM2_lfoDpth": 32,
    "Mix_DCO1": 33,
    "Mix_DCO2": 34,
    "Mix_EnvDpth": 35,
    "VCF_freq": 36,
    "VCF_res": 37,
    "VCF_lfo1": 38,
    "VCF_lfo2": 39,
    "VCFenvDpth": 40,
    "VCF_kf": 41,
    "VCA_level": 42,
    "LFO1_delay": 43,
    "LFO1_rate": 44,
    "LFO1_level": 45,
    "LFO2_delay": 46,
    "LFO2_rate": 47,
    "LFO2_level": 48,
    "env1_t1": 49,
    "env1_l1": 50,
    "env1_t2": 51,
    "env1_l2": 52,
    "env1_t3": 53,
    "env1_l3": 54,
    "env1_t4": 55,
    "env2_t1": 56,
    "env2_l1": 57,
    "env2_t2": 58,
    "env2_l2": 59,
    "env2_t3": 60,
    "env2_l3": 61,
    "env2_t4": 62,
    "env3_att": 63,
    "env3_decy": 64,
    "env3_sus": 65,
    "env3_rel": 66,
    "env4_att": 67,
    "env4_decy": 68,
    "env4_sus": 69,
    "env4_rel": 70,
    'dco1rng': [(73, 2, 0, 0)],
    'dco1wf': [(73, 2, 2, 0)],
    'dco2rng': [(73, 2, 4, 0)],
    'dco2wf': [(73, 1, 6, 0), (80, 1, 6, 1)],
    'dcoXmod': [(74, 2, 0, 0)],
    'dco1Vel': [(74, 2, 2, 0)],
    'dco2Vel': [(74, 2, 4, 0)],
    'mixVel': [(74, 1, 6, 0), (80, 1, 5, 1)],
    'dco1Lfo': [(75, 2, 0, 0)],
    'dco2Lfo': [(75, 2, 2, 0)],
    'pwm1Lfo': [(75, 2, 4, 0)],
    'pwm2Lfo': [(75, 1, 6, 0), (80, 1, 4, 1)],
    'pwm1Vel': [(76, 2, 0, 0)],
    'pwm2Vel': [(76, 2, 2, 0)],
    'lfo1Sync': [(76, 2, 4, 0)],
    'lfo2Sync': [(76, 1, 6, 0), (80, 1, 3, 1)],
    'env1key': [(77, 3, 0, 0)],
    'env2key': [(77, 3, 3, 0)],
    'hpf': [(77, 1, 6, 0), (80, 1, 2, 1)],
    'dco1Env': [(78, 3, 0, 0)],
    'dco2Env': [(78, 3, 3, 0)],
    'vcaVel': [(78, 1, 6, 0), (80, 1, 1, 1)],
    'mixEnv': [(79, 3, 0, 0)],
    'vcfEnv': [(79, 3, 3, 0)],
    'vcfVel': [(79, 1, 6, 0), (80, 1, 0, 1)],
    'vcaEnv': [(81, 2, 0, 0)],
    'lfo1Wf': [(81, 3, 3, 0)],
    'chorus': [(81, 1, 6, 0), (85, 1, 3, 1)],
    'lfo2Wf': [(82, 3, 0, 0)],
    'env3key': [(82, 3, 3, 0)],
    'pwm1Env': [(83, 3, 0, 0)],
    'pwm2Env': [(83, 3, 3, 0)],
    'env4key': [(84, 3, 0, 0)]
}

apr_mapping_tone = {
    "Char1": 7,
    "Char2": 8,
    "Char3": 9,
    "Char4": 10,
    "Char5": 11,
    "Char6": 12,
    "Char7": 13,
    "Char8": 14,
    "Char9": 15,
    "Char10": 16,
    "Char11": 17,
    "DCO1_tune": 18,
    "DCO1_lfoDpth": 19,
    "DCO1_envDpth": 20,
    "DCO2_tune": 21,
    "DCO2_fineTun": 22,
    "DCO2_lfoDpth": 23,
    "DCO2_envDpth": 24,
    "PWM1_width": 25,
    "PWM1_envDpth": 26,
    "PWM1_lfoDpth": 27,
    "PWM2_width": 28,
    "PWM2_envDpth": 29,
    "PWM2_lfoDpth": 30,
    "Mix_DCO1": 31,
    "Mix_DCO2": 32,
    "Mix_EnvDpth": 33,
    "VCF_freq": 34,
    "VCF_res": 35,
    "VCF_lfo1": 36,
    "VCF_lfo2": 37,
    "VCFenvDpth": 38,
    "VCF_kf": 39,
    "VCA_level": 40,
    "LFO1_delay": 41,
    "LFO1_rate": 42,
    "LFO1_level": 43,
    "LFO2_delay": 44,
    "LFO2_rate": 45,
    "LFO2_level": 46,
    "env1_t1": 47,
    "env1_l1": 48,
    "env1_t2": 49,
    "env1_l2": 50,
    "env1_t3": 51,
    "env1_l3": 52,
    "env1_t4": 53,
    "env2_t1": 54,
    "env2_l1": 55,
    "env2_t2": 56,
    "env2_l2": 57,
    "env2_t3": 58,
    "env2_l3": 59,
    "env2_t4": 60,
    "env3_att": 61,
    "env3_decy": 62,
    "env3_sus": 63,
    "env3_rel": 64,
    "env4_att": 65,
    "env4_decy": 66,
    "env4_sus": 67,
    "env4_rel": 68,
    "dco1rng": (71, 2, 0),
    "dco1wf": (72, 2, 0),
    "dco2rng": (73, 2, 0),
    "dco2wf": (74, 2, 0),
    "dcoXmod": (75, 2, 0),
    "dco1Vel": (76, 2, 0),
    "dco2Vel": (77, 2, 0),
    "mixVel": (78, 2, 0),
    "dco1Lfo": (79, 2, 0),
    "dco2Lfo": (80, 2, 0),
    "pwm1Lfo": (81, 2, 0),
    "pwm2Lfo": (82, 2, 0),
    "pwm1Vel": (83, 2, 0),
    "pwm2Vel": (84, 2, 0),
    "lfo1Sync": (85, 2, 0),
    "lfo2Sync": (86, 2, 0),
    "env1key": (87, 3, 0),
    "env2key": (88, 3, 0),
    "hpf": (89, 2, 0),
    "dco1Env": (90, 3, 0),
    "dco2Env": (91, 3, 0),
    "vcaVel": (92, 2, 0),
    "mixEnv": (93, 3, 0),
    "vcfEnv": (94, 3, 0),
    "vcfVel": (95, 2, 0),
    "vcaEnv": (96, 2, 0),
    "lfo1Wf": (97, 3, 0),
    "chorus": (98, 2, 0),
    "lfo2Wf": (99, 3, 0),
    "env3key": (100, 3, 0),
    "pwm1Env": (102, 3, 0),
    "pwm2Env": (103, 3, 0),
    "env4key": (105, 3, 0)
}


def is_parameter_bit(byte, bit, mapping):
    for param in mapping.values():
        if isinstance(param, int):
            if param == byte:
                return True
        elif isinstance(param, tuple):
            if param[0] == byte:
                if (1 << bit) & (((1 << param[1]) - 1) << param[2]) != 0:
                    return True
        elif isinstance(param, list):
            for par in param:
                if par[0] == byte:
                    if (1 << bit) & (((1 << par[1]) - 1) << par[2]) != 0:
                        return True
        else:
            raise Exception("Invalid mapping")
    return False


def read_bits(src_byte, bit_address, bit_width):
    src_bits = (src_byte >> bit_address) & ((1 << bit_width) - 1)
    return src_bits


def write_bits(prev_value, value, bit_address, bit_width):
    dst_bits = (value & ((1 << bit_width) - 1)) << bit_address
    return (prev_value & ~(((1 << bit_width) - 1) << bit_address)) | dst_bits


def load_parameter(src_msg, mapping):
    if isinstance(mapping, list):
        value = 0
        for param_info in mapping:
            src_bits = read_bits(src_msg[param_info[0]], param_info[2], param_info[1])
            value |= src_bits << param_info[3]
        return value
    elif isinstance(mapping, tuple):
        return read_bits(src_msg[mapping[0]], mapping[2], mapping[1])
    elif isinstance(mapping, int):
        return src_msg[mapping]
    else:
        raise Exception("Invalid mapping specification - must be int, tuple, or list")


def save_parameter(dst_msg, mapping, value):
    if isinstance(mapping, list):
        for param_info in mapping:
            bit_value = (value >> param_info[3]) & ((1 << param_info[1]) - 1)
            dst_msg[param_info[0]] = write_bits(dst_msg[param_info[0]], bit_value, param_info[2], param_info[1])
    elif isinstance(mapping, tuple):
        dst_msg[mapping[0]] = write_bits(dst_msg[mapping[0]], value, mapping[2], mapping[1])
    elif isinstance(mapping, int):
        dst_msg[mapping] = value
    else:
        raise Exception("Invalid mapping specification - must be int, tuple, or list")


def convert_message(src_msg, dst_msg_len: int, src_mapping, dest_mapping) -> List[int]:
    dest_msg = [0] * dst_msg_len
    for param_name in src_mapping:
        param_value = load_parameter(src_msg, src_mapping[param_name])
        save_parameter(dest_msg, dest_mapping[param_name], param_value)
    return dest_msg


def convert_into_message(src_msg, dest_msg, src_mapping, dest_mapping) -> None:
    for param_name in src_mapping:
        param_value = load_parameter(src_msg, src_mapping[param_name])
        save_parameter(dest_msg, dest_mapping[param_name], param_value)


def extractPatchesFromBank(message):
    global g_bank_messages
    if isPartOfBankDump(message):
        # Collect all messages in a global variable, we need them later
        g_bank_messages.append(message)

    patches = []
    if len(g_bank_messages) == 64 + 50:
        tones_used = set()
        print("Found all messages, for a bank, constructing patches with tones")
        for i in range(64):
            patch = g_bank_messages[i]
            if isOperationMessage(patch, [operation_vec_bld]):
                # This is an OS 4 Bulk Dump
                if patch[5] != patch_level:
                    raise Exception("Expected patch bulk dump message - can't convert. Maybe some messages are missing?")
                patch_no = patch[8]
                apr_patch_dump = createMessageHeader(operation_vec_apr, MIDI_control_channel, patch_level) + [0x01] + [0x00] * 53 + [0xf7]
                convert_into_message(patch, apr_patch_dump, bulk_mapping_patch, apr_mapping_patch)
                patch_name = "".join([chr(apr_patch_dump[i]) for i in range(7, 25)])
                # print(f"Read Vecoven patch no {patch_no}: {patch_name}")

                # We need to find out which two tones belong to this patch
                A_tone_number = apr_patch_dump[36]
                B_tone_number = apr_patch_dump[45]
                pgr_patch = createMessageHeader(operation_pgr, MIDI_control_channel, patch_level) + [0x01, 0x00, patch_no, 0x00, 0xf7]
                high, low = getToneNumbers(apr_patch_dump)

                APR_tone_A = []
                pgr_tone_A = []
                # The Tones from 50..99 are ROM tones, which are stored in the device and not part of the bank
                if A_tone_number < 50:
                    A_tone_bulk = g_bank_messages[64 + A_tone_number]
                    APR_tone_A = createMessageHeader(operation_vec_apr, MIDI_control_channel, tone_level) + [tone_a] + [0x00] * 102 + [0xf7]
                    convert_into_message(A_tone_bulk, APR_tone_A, bulk_mapping_tone, apr_mapping_tone)
                    pgr_tone_A = createMessageHeader(operation_pgr, MIDI_control_channel, tone_level) + [tone_a, 0x00, A_tone_number, 0x00, 0xf7]

                APR_tone_B = []
                pgr_tone_B = []
                if B_tone_number < 50:
                    B_tone_bulk = g_bank_messages[64 + B_tone_number]
                    APR_tone_B = createMessageHeader(operation_vec_apr, MIDI_control_channel, tone_level) + [tone_b] + [0x00] * 102 + [0xf7]
                    convert_into_message(B_tone_bulk, APR_tone_B, bulk_mapping_tone, apr_mapping_tone)
                    pgr_tone_B = createMessageHeader(operation_pgr, MIDI_control_channel, tone_level) + [tone_b, 0x00, B_tone_number, 0x00, 0xf7]

                tones_used.add(A_tone_number)
                tones_used.add(B_tone_number)
                apr_patch = pgr_patch + apr_patch_dump + pgr_tone_A + APR_tone_A + pgr_tone_B + APR_tone_B
            # else:
            #     # We need to find out which two tones belong to this patch
            #     patch_data = denibble(patch[9:-1])
            #
            #     # We need to find out which two tones belong to this patch
            #     upper_tone_number = patch_data[0x14]  # 1d specified, but possibly also 0x1f
            #     lower_tone_number = patch_data[0x24]  # 26 specified, but possibly also 0x2e
            #     # patch.extend(g_bank_messages[upper_tone_number])
            #     # patch.extend(g_bank_messages[lower_tone_number])
            #     pgr_patch, apr_patch = bldToApr(patch)
            #     pgr_upper_tone, apr_upper_tone = bldToApr(g_bank_messages[upper_tone_number + 64])
            #     pgr_lower_tone, apr_lower_tone = bldToApr(g_bank_messages[lower_tone_number + 64])
            #     apr_patch = apr_patch + apr_upper_tone + apr_lower_tone

                if not isSingleProgramDump(apr_patch):
                    print("Error, created invalid program dump!")
                else:
                    patches.append(apr_patch)
                    print(f"Discovered patch {nameFromDump(apr_patch)}")
        g_bank_messages = []
        all_tones = set(range(50))
        unused_tones = all_tones - tones_used
        if len(unused_tones) > 0:
            unused_tones_sorted = sorted(list(unused_tones))
            print(f"This bank contains {len(unused_tones)} unused tones: {', '.join([str(x + 1) for x in unused_tones_sorted])}")
    return len(patches), patches


def numberFromDump(messages):
    if isEditBufferDump(messages):
        return 0
    elif isSingleProgramDump(messages):
        # The first message must be the PRG message of the patch, that's what we need
        return messages[8]
    elif isBulkMessage(messages):
        # This works for the first patch of a bulk dump, Vecoven format
        return messages[8]
    raise Exception(f"Undefined operation, can only extract the patch number from known message types: {messages}")


def numberOfLayers(messages):
    return 3


def layerName(messages, layerNo):
    index = knobkraft.sysex.findSysexDelimiters(messages)
    if isEditBufferDump(messages):
        return nameFromDump(messages[index[layerNo][0]:index[layerNo][1]])
    elif isSingleProgramDump(messages):
        mno = layerNo*2+1
        if mno < len(index):
            return nameFromDump(messages[index[layerNo*2+1][0]:index[layerNo*2+1][1]])
        else:
            return "ROM"
    raise Exception("Program error in layerName, can't get layerName for message {messages}")

