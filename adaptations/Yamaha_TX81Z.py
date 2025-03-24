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

#  General Notes for the Yamaha TX81Z
#       Note for this synth, checksum = twos complement of the lower 7 bits of the sum of all data bytes.
#       Note a patch consists of 2 sysex messages, the VCED and the ACED
#       The TX81Z has performances built from multiple patches, but these are not accessible via this adaptation presently.
#       Note firmware Ver1.0 has a bug in ACED checksum calculation which results in a transient error message displayed
#       on the synth.  Voice data is transferred correctly, but the ACED additional parameters are not.

#  Future Enhancements
#       ROM banks may be loaded into database by iterating over them using Program Change requests and edit buffer dumps
#       Currently bank dumps only work for the internal user bank.


# General Sysex constants
SOX = 0xf0              # start of sysex message
EOX = 0xf7              # end of sysex message
ASCII_SPACE = 32        # for padding patch names

# Constants applicable to the TX81Z
class Synth:
    dev_name = "Yamaha TX81Z"
    model = "TX81Z"
    vendor_id = 0x43        # Yamaha vendor ID
    device_id = 0x00        # base value for sysex byte 3 for single tx messages. id=0x0n, where n = MIDI channel.
    bulk_req_id = 0x20      # used for sysex byte 3 when requesting bulk dumps.  id=0x2n, where n = MIDI channel.
    banks = 1               # TX81Z has 4 ROM banks and 1 RAM bank for patches.  Can only access the RAM bank currently
    voices = 32             # TX81Z has 32 patches per bank
    performances = 24       # TX81Z has 24 Performance memories made up of patches
    name_size = 10          # number of characters in the patch name
    voice_bytes = 93        # size of a single primary voice dump (data only)   (0x5d)
    voice_extras = 23       # size of a single additional voice parameters message (data only)
    perform_bytes = 120     # size of a single performance dump (data only)
    name_start_addr = 77    # byte location of start of patch name within voice message
    header_size = 6         # TX81Z bulk header is: [SOX][vendor_id][channel][function][data size high][data size low]
    packed_voice_block = 128   # size of a complete patch within a bank dump response message
    packed_voice_size = 73     # size of the primary voice portion (VCED) of a complete patch
    packed_extras_size = 55    # size of the additional voice parameters (ACED) of a complete patch
    specific_detection = True
                            # command id strings for extended functions
    full_voice_cmd = [0x4c, 0x4d, 0x20, 0x20, 0x38, 0x39, 0x37, 0x36, 0x41, 0x45]   # "LM  8976AE"  - ACED signature


class DataFormat(IntEnum):
    # this is sysex byte 4
    VOICE_DUMP = 0x03              # TX81Z single voice (VCED) buffer bulk dump command
    FULL_VOICE_DUMP = 0x7e         #TX81Z single full voice (VCED + ACED) buffer bulk dump (needs extension string)
    PERFORMANCE_DUMP = 0x7E        #  single performance (PCED) buffer dump command (requires extension string)
    PERFORMANCE64_FORMAT = 0x02    # TX81Z performances dump (PMEM) - 24 user presets + 8 initials (needs extension)
    VOICE32_DUMP = 0x04            # TX81Z 32 voice dump (VMEM) command
    CONDITION_ACKNOWLEDGE = 125    # not used in TX81Z


#  Other TX81Z parameters not currently utilized.
VBANK_DUMP_SIZE = 0x2000     # hardcoded message size for voice bank dump


def bankDescriptors():          #  Yamaha TX81Z has 5 banks:  4 ROM banks and 1 RAM bank
    return [{"bank": 0,  # first bank, RAM or Internal
             "name": f"Internal-Patch",  # this is a patch bank
             "size": 32,  # number of patches in this bank
             "type": "Patch",  # type of data stored in the bank
             "isROM": False
             },
            {"bank": 1,  # second bank, ROM bank A
             "name": f"ROM A - Patch",
             "size": 32,
             "type": "Patch",
             "isROM": True
             },
            {"bank": 2,  # third bank, ROM bank B
             "name": f"ROM B - Patch",
             "size": 32,
             "type": "Patch",
             "isROM": True
             },
            {"bank": 3,  # fourth bank, ROM bank C
             "name": f"ROM C - Patch",
             "size": 32,
             "type": "Patch",
             "isROM": True
             },
            {"bank": 4,  # fifth bank, ROM bank D
             "name": f"ROM D - Patch",
             "size": 32,
             "type": "Patch",
             "isROM": True
             }]

class OPERATOR_PACKED(Structure):
    #   This is the bitwise packing of the 4 voice operators
    _pack_ = 1
    _fields_ = [
        ("attack_rate", c_uint8, 5),
        ("decay_1_rate", c_uint8, 5),
        ("decay_2_rate", c_uint8, 5),
        ("release_rate", c_uint8, 7),       # anomaly note - if the bit size is 4, adds extra zero byte to output!
        ("decay_1_level", c_uint8, 4),
        ("level_scaling", c_uint8, 7),
        ("ame_ebs_kvs", c_uint8, 7),        # 3 parameters in single byte
        ("operator_output_level", c_uint8, 7),
        ("frequency", c_uint8, 6),
        ("ratescale_detune", c_uint8, 5),   # 2 parameters in single byte
    ]

class OPERATOR_XTRA_PACKED(Structure):
    _pack_ = 1
    _fields_ = [
        ("egshift_fixf_fixfrng", c_uint8, 7),        # 3 parameters in single byte, increased bit count.
        ("opwave_finef", c_uint8, 7),                # 2 parameters in single byte
    ]


class VOICE_PACKED(Structure):
    #   This is the bitwise packing of parameters of the 73 byte voice data, top bit reserved.
    _pack_ = 1
    _fields_ = [
        ("operators", OPERATOR_PACKED * 4),
        ("lfosync_fdback_algor", c_uint8, 7),       # 3 parameters in single byte
        ("lfo_speed", c_uint8, 7),
        ("lfo_delay", c_uint8, 7),
        ("pitch_mod_depth", c_uint8, 7),
        ("amplitude_mod_depth", c_uint8, 7),
        ("pms_ams_lfw", c_uint8, 7),                # 3 parameters in single byte
        ("transpose", c_uint8, 6),
        ("pitch_bend_range", c_uint8, 4),
        ("chorus_mono_sus_port_pm", c_uint8, 5),    # 4 parameters in single byte
        ("portamento_time", c_uint8, 7),
        ("foot_control_volume", c_uint8, 7),
        ("mod_wheel_pitch", c_uint8, 7),
        ("mod_wheel_amplitude", c_uint8, 7),
        ("breath_control_pitch", c_uint8, 7),
        ("breath_control_amplitude", c_uint8, 7),
        ("breath_control_pitch_bias", c_uint8, 7),
        ("breath_control_eg_bias", c_uint8, 7),
        ("name", c_uint8 * 10),
        ("unused_parameters", c_uint8 * 6)
    ]

class XTRA_PARMS(Structure):
    #   This is the bitwise packing of additional voice parameters of the 55 byte ACED data. Only 11 bytes used
    _pack_ = 1
    _fields_ = [
        ("operators_xtra", OPERATOR_XTRA_PACKED * 4),
        ("reverb_rate", c_uint8, 5),
        ("foot_controller_pitch", c_uint8, 7),
        ("foot_controller_amplitude", c_uint8, 7),
        ("zero_padding", c_uint8 * 44)
      ]


def name():
    return Synth.dev_name

def setupHelp():
    return ("Set TX81Z  Utility / MIDI Control / 'Exclusive' = 'On'.\n"
            "Ensure 'Basic Rx channel' and 'Transmit channel' are set to the same MIDI channel, usually 1.\n\n"
            "Bank dumps from ROM banks not currently supported.\n"
            "Performance dumps not currently supported.\n\n"
            "Special Note:  TX81Z Rev1.0 firmware contains a checksum defect.  To avoid problems, load\n"
            "patches using manual edit buffer dumps instead of bank dumps for this version.  Later versions\n"
            "do not have this problem."
            )


# Device Identification

def createDeviceDetectMessage(channel):
    return makeDumpRequest(channel, DataFormat.VOICE_DUMP)

def needsChannelSpecificDetection():
    return Synth.specific_detection

def channelIfValidDeviceResponse(message):
    if isVoiceBulkData(message):
        return message[2] & 0x0f
    return -1


def makeDumpRequest(channel, format):
    # TX81Z all bulk dump requests use sysex byte 3 = 0x2n, where n = MIDI channel
    if format not in [DataFormat.VOICE_DUMP, DataFormat.PERFORMANCE_DUMP, DataFormat.PERFORMANCE64_FORMAT,
                      DataFormat.VOICE32_DUMP, 125]:
        raise Exception(f"Invalid format for synth dump request given: {format}")
    return [SOX, Synth.vendor_id, Synth.bulk_req_id | (channel & 0x0f), format, EOX]


def isVoiceBulkData(message):
    # applies to the primary voice data (VCED) message only
    if (len(message) > 5
            and isOwnSysex(message)
            and getFormatCode(message) == DataFormat.VOICE_DUMP
            and getByteCount(message) == Synth.voice_bytes):
        return True
    return False


def isVoiceAddedParms(message):
    # applies to the additional voice data (ACED) message only
    if (len(message) > 16
            and isOwnSysex(message)
            and message[3] == 0x7e
            and message[4] == 0x00
            and message[5] == 0x21
            and message[6:16] == Synth.full_voice_cmd):
        return True
    return False


def isPerformanceBulkData(message):
    if (len(message) > 5
            and isOwnSysex(message)
            and getFormatCode(message) == DataFormat.PERFORMANCE_DUMP
            and getByteCount(message) == Synth.perform_bytes):
        return True
    return False


def isOwnSysex(message):
    return (len(message) > 3
            and message[0] == SOX
            and message[1] == Synth.vendor_id)


def getFormatCode(message):
    return message[3]


def getByteCount(message):
    #  note this is the size of the data block, not the total message size
    return message[4] << 7 | message[5]


# Edit buffer capability

def createEditBufferRequest(channel):
    return requestFullVoice(channel, DataFormat.FULL_VOICE_DUMP)

def requestFullVoice(channel, format):
    # TX81Z all bulk dump requests use sysex byte 3 = 0x2n, where n = MIDI channel
    if format not in [DataFormat.FULL_VOICE_DUMP]:
        raise Exception(f"Invalid format for synth dump request given: {format}")
    return [SOX, Synth.vendor_id, Synth.bulk_req_id | (channel & 0x0f), int(format)] + Synth.full_voice_cmd + [EOX]


def isEditBufferDump(data):
    messages = knobkraft.splitSysexMessage(data)
    main_voice = sum([1 if isVoiceBulkData(m) else 0 for m in messages])
    extra_parameters = sum([1 if isVoiceAddedParms(m) else 0 for m in messages])
    return main_voice == 1 and extra_parameters == 1


def isPartOfEditBufferDump(message):
    return isVoiceBulkData(message) or isVoiceAddedParms(message)

def convertToEditBuffer(channel, data):
    # creates the sysex messages to be sent to the synth edit buffer
    # split out the main voice and added_parms sysex message from the database patch, and assign MIDI channel
    messages = knobkraft.splitSysexMessage(data)
    voice = []
    add_parms = []
    for message in messages:
        if isVoiceBulkData(message):
            voice = message[0:2] + [Synth.device_id | channel] + message[3:]
        elif isVoiceAddedParms(message):
            add_parms = message[0:2] + [Synth.device_id | channel] + message[3:]
        else:
            raise Exception("Message is not an edit buffer for %s" % Synth.model)
    return add_parms + voice


def nameFromDump(data):
    messages = knobkraft.splitSysexMessage(data)
    for message in messages:
        if isVoiceBulkData(message):
            start_index = Synth.header_size             # data starts byte 7 of voice buffer sysex dump
            xname = ''.join(chr(x) for x in message[Synth.name_start_addr + start_index:Synth.name_start_addr
                                                                                   + Synth.name_size + start_index])
            return xname
    raise Exception("Can't extract name from something that is not an edit buffer message")


def renamePatch(data, new_name):
    # data contains the voice and added_parms messages concatenated.  Replace name text only within the voice sysex
    #  and rebuild the combined data
    messages = knobkraft.splitSysexMessage(data)
    result = []
    for message in messages:
        if isVoiceBulkData(message):
            start_index = Synth.header_size
            new_name_bytes = [ord(c) for c in new_name] + [ASCII_SPACE] * (Synth.name_size - len(new_name))
            truncated_name = new_name_bytes[:Synth.name_size]
            rest_of_msg = start_index + Synth.name_start_addr + Synth.name_size
            new_message = message[:start_index + Synth.name_start_addr] + truncated_name + message[rest_of_msg:]
            new_message[-2] = checksum(new_message[start_index:-2])
            result.extend(new_message)
        elif isVoiceAddedParms(message):
            result.extend(message)
        else:
            raise Exception("Can only rename edit buffers of the %s" % Synth.model)
    return result


def checksum(data_block):
    check_sum = 0
    for d in data_block:
        check_sum -= d
    return check_sum & 0x7f


def old2_checksum(data_block):
    # alternative calculation
    check_sum = 1 + ~(sum(data_block) & 0x7f)
    return check_sum & 0x7f


def calculateFingerprint(m_message):
    messages = knobkraft.splitSysexMessage(m_message)
    data_v = []
    data_a = []
    for message in messages:
        if isVoiceBulkData(message):
            data_v = message[Synth.header_size:-2]        # strip sysex header and footer
            # Blank out the name bytes, not applicable to the fingerprint
            data_v[Synth.name_start_addr:Synth.name_start_addr + Synth.name_size] = [0] * Synth.name_size
        elif isVoiceAddedParms(message):
            data_a = message[Synth.header_size:-2]
        else:
            raise Exception("Can only fingerprint edit buffers")
    return hashlib.md5(bytearray(data_a + data_v)).hexdigest()  # Calculate the fingerprint from the cleaned payload data


# Program Dump Capability

# def createProgramDumpRequest(channel, patchNo):
# def isSingleProgramDump(message):
# def convertToProgramDump(channel, message, program_number):

#  def friendlyProgramName(program):
#      return "%d%d" % (program // 8 + 1, (program % 8) + 1)


# Bank Dump Capability

def createBankDumpRequest(channel, bank):
    return makeDumpRequest(channel, DataFormat.VOICE32_DUMP)       # synth does not support bank selection

def isPartOfBankDump(message):
    return isOwnSysex(message) and getFormatCode(message) in [DataFormat.VOICE32_DUMP]

def isBankDumpFinished(messages):
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False

def extractPatchesFromAllBankMessages(messages):
    #  the TX81Z embeds all patches within a single bank dump message, but each patch includes two MIDI messages
    all_patches = []
    for message in messages:
        patches = []
        if isPartOfBankDump(message):
            data_block = message[Synth.header_size:-2]
            # Check for hardcoded length of bank dump
            if len(data_block) != (0x20 << 7 | 0x00):
                print("Got bulk dump invalid length - data size is %d but was expected to be %d" % (
                    len(data_block), (0x20 << 7)))
                return []
            elif checksum(data_block) != message[-2]:
                print("Checksum error encountered in bulk dump")
                return []
            else:
                for i in range(numberOfPatchesPerBank()):
                    voice_end = (i * Synth.packed_voice_block) + Synth.packed_voice_size
                    voice = packedVoiceToSingleVoice(data_block[i * Synth.packed_voice_block: voice_end])
                    extras_offset = (i * Synth.packed_voice_block) + Synth.packed_voice_size
                    extras = packedExtrasToSingleExtra(data_block[extras_offset:(i + 1) * Synth.packed_voice_block])
                    combined = singleExtrasFromVoice(extras) + singlePatchFromVoice(voice)
                    # print("Found patch " + nameFromDump(combined))
                    patches.append(combined)
                    # print(f"combined: {list(map(hex, combined))}")
                return patches


def singlePatchFromVoice(voice):
    msg = [SOX, Synth.vendor_id, 0x00, DataFormat.VOICE_DUMP, 0x00, Synth.voice_bytes] + voice + [checksum(voice), EOX]
#    print(f"message voice: {list(map(hex, msg))}")
    return msg

def singleExtrasFromVoice(extras):
    full_datablock = Synth.full_voice_cmd + extras
    msg = [SOX, Synth.vendor_id, 0x00, 0x7e, 0x00, 0x21] + Synth.full_voice_cmd + extras + [checksum(full_datablock), EOX]
    return msg


def packedVoiceToSingleVoice(packed):
    # This is the ugly function that needs to unpack VCED data from the packed VMEM data format
    source = VOICE_PACKED.from_buffer_copy(bytearray(packed))
    dest = []
    for op in source.operators:
        dest.append(op.attack_rate)
        dest.append(op.decay_1_rate)
        dest.append(op.decay_2_rate)
        dest.append(op.release_rate)
        dest.append(op.decay_1_level)
        dest.append(op.level_scaling)
        dest.append((op.ratescale_detune >> 3) & 0x3)  # extract the rate scaling parameter
        dest.append((op.ame_ebs_kvs >> 3) & 0x7)       # extract the EG bias sensitivity parameter
        dest.append((op.ame_ebs_kvs >> 6) & 0x1)       # extract the amplitude mod enable parameter
        dest.append(op.ame_ebs_kvs & 0x7)              # extract the key velocity sensitivity parameter
        dest.append(op.operator_output_level)
        dest.append(op.frequency)
        dest.append(op.ratescale_detune & 0x7)         # extract the detune parameter
    dest.append(source.lfosync_fdback_algor & 0x7)      # extract the algorithm parameter
    dest.append((source.lfosync_fdback_algor >> 3) & 0x7)      # extract the feedback parameter
    dest.append(source.lfo_speed)
    dest.append(source.lfo_delay)
    dest.append(source.pitch_mod_depth)
    dest.append(source.amplitude_mod_depth)
    dest.append((source.lfosync_fdback_algor >> 6) & 0x1)      # extract the lfo sync parameter
    dest.append(source.pms_ams_lfw & 0x3)               # extract the lfo wave parameter
    dest.append((source.pms_ams_lfw >> 4) & 0x7)        # extract the pitch mod sensitivity parameter
    dest.append((source.pms_ams_lfw >> 2) & 0x3)        # extract the amplitude mod sensitivity parameter
    dest.append(source.transpose)
    dest.append((source.chorus_mono_sus_port_pm >> 3) & 0x1)    # extract poly/mono parameter
    dest.append(source.pitch_bend_range)
    dest.append(source.chorus_mono_sus_port_pm & 0x1)    # extract portamento mode parameter
    dest.append(source.portamento_time)
    dest.append(source.foot_control_volume)
    dest.append((source.chorus_mono_sus_port_pm >> 2) & 0x1)    # extract sustain parameter
    dest.append((source.chorus_mono_sus_port_pm >> 1) & 0x1)    # extract portamento parameter
    dest.append((source.chorus_mono_sus_port_pm >> 4) & 0x1)    # extract chorus parameter
    dest.append(source.mod_wheel_pitch)
    dest.append(source.mod_wheel_amplitude)
    dest.append(source.breath_control_pitch)
    dest.append(source.breath_control_amplitude)
    dest.append(source.breath_control_pitch_bias)
    dest.append(source.breath_control_eg_bias)
    dest += source.name
    dest += source.unused_parameters
    ####     dest.append(source.operators_on_off)    #not part of bulk dump!
    return dest


def packedExtrasToSingleExtra(packed):
    # Unpacks the ACED additional parameters
    source = XTRA_PARMS.from_buffer_copy(bytearray(packed))
    dest = []
    for op in source.operators_xtra:
        dest.append((op.egshift_fixf_fixfrng >> 3) & 0x1)     # extract fixed frequency
        dest.append(op.egshift_fixf_fixfrng & 0x7)            # extract fixed frequency range
        dest.append(op.opwave_finef & 0xf)                    # extract frequency range fine
        dest.append((op.opwave_finef >> 4) & 0x7)             # extract operator waveform
        dest.append((op.egshift_fixf_fixfrng >> 4) & 0x3)     # extract EG shift
    dest.append(source.reverb_rate)
    dest.append(source.foot_controller_pitch)
    dest.append(source.foot_controller_amplitude)
    return dest


def numberOfPatchesPerBank():
    return Synth.voices

def numberOfBanks():
    # It is only possible to dump the RAM bank, ROM banks cannot be accessed via a bank dump command
    return Synth.banks

def friendlyBankName(bank_number):
    return "Internal memory" if bank_number == 0 else "ROM memory"

# bank selection not currently supported.  There is no simple message to change banks.
def unused_bankSelect(channel,bank):
    # TX81Z bank assignments:  Internal(0-31), ROM A(32-63), ROM B(64-95), ROM C(96-127), ROM D(128-160)
    # Performances (161-184)
    prog_select = (bank * 32) + 1   # we will just choose patch 1 of each bank
    return [SOX, Synth.vendor_id, 0x10 | (channel & 0x0f), 0x10, 0x7f,  prog_select, 0x0, 0x0, EOX]



# ##   HAVE NOT MODIFIED THIS FOR TX81Z YET
# Test data picked up by test_adaptation.py
#def make_test_data():
#    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
#        first_message = data.all_messages[0]
#        assert isPartOfBankDump(first_message)
#        patches_list = extractPatchesFromBank(first_message)
#        patches = knobkraft.sysex.splitSysex(patches_list)
#        assert isEditBufferDump(patches[0])
#        yield testing.ProgramTestData(message=patches[0], name="BRASS   1 ")
#        yield testing.ProgramTestData(message=patches[3], name="STRINGS 1 ")
#
#    return testing.TestData(sysex="testData/Yamaha_TX7_rom1a.syx",
#                            edit_buffer_generator=programs,
#                            device_detect_call="f0 43 20 00 f7",
#                            device_detect_reply=([0xf0, 0x43, 0x00, 0x00, 0x01, 0x1B], 0))
