#
#   Copyright (c) 2020-2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Note that for real life usage the native C++ implementation of the Matrix1000 is more powerful and should be used
# This is an example adaption to show how a fully working adaption can look like

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
    return "Roland MKS-70"


def setupHelp():
    return """
The Roland MKS-70 does not allow to initiate a bank dump (all 64 patches and 50 tones) from 
the device itself. You need to enter receive manual dump via the KnobKraft MIDI menu, and then
on the device:

How to enter 'BULK DUMP' mode:
  * Press both MIDI and WRITE button
  * Select BULK DUMP by ALPHA-DIAL, then press ENTER 
"""


def createDeviceDetectMessage(channel):
    # If I interpret the sysex docs correctly, we can just send a MIDI program change, and it should reply with sysex
    # request Tone #0
    return createProgramDumpRequest(channel & 0x0f, 128)


def deviceDetectWaitMilliseconds():
    # Negative value means don't autodetect at all
    return 150


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    global MIDI_control_channel
    # We expect an APR message for a patch
    if isAprMessage(message):
        if isToneMessage(message):
            MIDI_control_channel = message[3]
            return message[3]
    return -1


def bankDescriptors() -> List[Dict]:
    return [{"bank": 0, "name": "Internal Patches", "size": 64, "type": "Patch"},
            {"bank": 1, "name": "Cartridge Patches", "size": 64, "type": "Patch"},
            {"bank": 2, "name": "Internal Tones", "size": 50, "type": "Tone"},
            {"bank": 3, "name": "Cartridge Tones", "size": 50, "type": "Tone"}]


def createPgrMessage(patch_no, tone_group):
    # Check if we are requesting a patch or a tone
    if 0 <= patch_no < 128:
        return [0xf0, roland_id, operation_pgr, MIDI_control_channel, format_type_jx10, patch_level, 0x01, 0x00, patch_no, 0x00, 0xf7]
    elif 128 <= patch_no < 228:
        tone_number = patch_no - 128
        tone_group = 0x01  # TODO Tone A, when would I use Tone B?
        return [0xf0, roland_id, operation_pgr, MIDI_control_channel, format_type_jx10, tone_level, tone_group, 0x00, tone_number, 0x00, 0xf7]
    raise Exception(f"Invalid patch_no given to createPgrMessage: {patch_no}")


def createProgramDumpRequest(channel, patch_no):
    # Check if we are requesting a patch or a tone
    if 0 <= patch_no < 128:
        # This is a patch. To make the synth send the patch, we need to send it a program change message on its control channel
        return [0b11000000 | MIDI_control_channel, patch_no]
    elif 128 <= patch_no < 228:
        tone_number = patch_no - 128
        # TODO I don't understand how MIDI_channel_A and MIDI_channel_B influence this command
        return [0b11000000 | MIDI_channel_A, tone_number]
    else:
        raise Exception(f"Invalid patch number given to createProgramDumpRequest: {patch_no}")


def isOperationMessage(message, operation):
    if len(message) > 5:
        if (message[0] == 0xF0 and
                message[1] == roland_id and
                message[2] == operation and
                message[4] == format_type_jx10):
            return True
    # If the message does not match the expected format, return False
    return False


def isBulkMessage(message):
    return isOperationMessage(message, operation_bld)


def isAprMessage(message):
    return isOperationMessage(message, operation_apr)


def isToneMessage(message):
    return isAprMessage(message) and message[5] == tone_level


def isPatchMessage(message):
    return isAprMessage(message) and message[5] == patch_level


def isProgramNumberMessage(message):
    return isOperationMessage(message, operation_pgr)


def isPartOfSingleProgramDump(message):
    return isToneMessage(message) or isPatchMessage(message) or isProgramNumberMessage(message)


def bldToApr(bld_message):
    if isBulkMessage(bld_message):
        if bld_message[5] == patch_level:
            # This is a patch BLD
            patch_number = bld_message[8]
            patch_data = denibble(bld_message[9:-1])
            pgr_message = createPgrMessage(patch_number, None)
            assert isProgramNumberMessage(pgr_message)
            #assert len(patch_data) == 51
            apr_message = [0xf0, roland_id, operation_apr, MIDI_control_channel, format_type_jx10, patch_level, 0x01] + patch_data + [0xf7]
            assert isPatchMessage(apr_message)
            return pgr_message, apr_message
        elif bld_message[5] == tone_level:
            # This is a tone, construct a proper single program dump (APR) message for it
            tone_number = bld_message[8]
            tone_data = bld_message[9:-1]
            pgr_message = createPgrMessage(tone_number + 128, tone_a)  # offset by 128 to make sure it is an internal tone number, not a patch number
            assert isProgramNumberMessage(pgr_message)
            assert len(tone_data) == 59
            apr_message = [0xf0, roland_id, operation_apr, MIDI_control_channel, format_type_jx10, tone_level, tone_a] + tone_data + [0xf7]
            assert isToneMessage(apr_message)
            return pgr_message, apr_message


def createEditBufferRequest(channel):
    # The documentation suggests we could issue a tone number change, and then the synth would send out PRG and APR message on its own
    # we need to test this, this would be a program change message? But to which program to change to?
    return []


def isEditBufferDump(messages):
    # We define an edit buffer as a three APR messages
    # one APR patch
    # one APR Tone A (upper)
    # one APR Tone B (lower)
    index = knobkraft.sysex.findSysexDelimiters(messages)
    if len(index) < 3:
        return False
    return all([isAprMessage(messages[i[0]:i[1]]) for i in index])


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        raise Exception("not implemented yet")
        messages = knobkraft.findSysexDelimiters(message, 2)
        second_message = message[messages[1][0]:messages[1][1]]
        return second_message
    raise Exception("Can only convert edit buffer dumps or single program dumps to edit buffer dumps!")


def isSingleProgramDump(messages):
    index = knobkraft.sysex.findSysexDelimiters(messages)
    if len(index) == 3:
        # These should be three BLD messages
        return all([isBulkMessage(message[index[i][0]:index[i][1]]) for i in range(3)])
    elif len(index) == 6:
        # Check if the message matches the expected format of the MKS-70's single program dump
        # It should be three APR messages, one patch and two tones, with an PRG message in front
        return (isPatchMessage(message[index[1][0]:index[1][1]]) and
                isToneMessage(message[index[3][0]:index[3][1]]) and
                isToneMessage(message[index[5][0]:index[5][1]]) and
                all(isProgramNumberMessage(message[index[i][0]:index[i][1]]) for i in [0, 2, 4])
                )
    # This does not work, it needs to be two messages
    return False


def convertToProgramDump(channel, message, program_number):
    raise NotImplemented
    if isEditBufferDump(message):
        # The edit buffer message can be sent unmodified, but needs to be prefixed with a PRG message each
        index = knobkraft.sysex.findSysexDelimiters(message)
        result = createPgrMessage(program_number)
        result.extend(message)
        return result
    elif isSingleProgramDump(message):
        # A program dump is an PGR message which tells the synth where to store the following APR data. We might want to change
        # the program position, however, so we need to create a new program message and just keep the APR message
        messages = knobkraft.sysex.splitSysexMessage(message)
        result = createPgrMessage(program_number)
        result.extend(messages[1])
        return result
    raise Exception("Can only convert single Tone APR messages to a program dump")


def nameFromDump(message):
    base = 7
    if isEditBufferDump(message):
        messages = knobkraft.sysex.splitSysexMessage(message)
        name_message = messages[0]
    elif isSingleProgramDump(message):
        messages = knobkraft.sysex.splitSysexMessage(message)
        if len(messages) == 3:
            # This is a BLD with a nibbled data struct. Need to convert this to APR!
            _, name_message = bldToApr(messages[0])
        else:
            # This is a list of PRG and APR messages, use the patch message
            name_message = messages[1]
    else:
        raise Exception("Message is not a program dump or edit buffer")

    if isToneMessage(name_message):
        name_length = 10
    elif isPatchMessage(name_message):
        name_length = 18
    else:
        raise Exception("Message is neither tone nor patch, can't extract name")
    patch_name_bytes = name_message[base:base + name_length]
    patch_name = ''.join(chr(byte) for byte in patch_name_bytes)
    return patch_name


def isPartOfBankDump(message):
    return isBulkMessage(message)


def isBankDumpFinished(messages):
    return len(messages) == 64 + 50


g_bank_messages = []


def denibble(data):
    unpacked_data = []
    for i in range(0, len(data), 2):
        byte = (data[i+1] & 0x0F) | ((data[i] & 0x0F) << 4)
        unpacked_data.append(byte)
    return unpacked_data


def extractPatchesFromBank(message):
    global g_patches_loaded
    if isPartOfBankDump(message):
        # Collect all messages in a global variable, we need them later
        g_bank_messages.append(message)

    patches = []
    if len(g_bank_messages) == 64 + 50:
        print("Found all messages, for a bank, constructing patches with tones")
        data = []
        for i in range(64):
            patch = g_bank_messages[i]
            # We need to find out which two tones belong to this patch
            patch_data = denibble(patch[9:-1])
            data.append(patch_data)

        #for i in range(len(data[0])):
        #    if all([0 <= data[v][i] < 50 for v in range(64)]):
        #        print(f"Candidate data index: {i:0x}")
        for i in range(64):
            patch = g_bank_messages[i]
            # We need to find out which two tones belong to this patch
            patch_data = denibble(patch[9:-1])
            valid_string = "".join([chr(x) for x in patch_data])
            upper_tone_number = patch_data[0x14]  # 1d specified, but possibly also 0x1f
            lower_tone_number = patch_data[0x24]  # 26 specified, but possibly also 0x2e
            #patch.extend(g_bank_messages[upper_tone_number])
            #patch.extend(g_bank_messages[lower_tone_number])
            pgr_patch, apr_patch = bldToApr(patch)
            pgr_upper_tone, apr_upper_tone = bldToApr(g_bank_messages[upper_tone_number+64])
            pgr_lower_tone, apr_lower_tone = bldToApr(g_bank_messages[lower_tone_number+64])
            apr_patch = apr_patch + apr_upper_tone + apr_lower_tone

            if not isEditBufferDump(apr_patch):
                print("Error, created invalid program dump!")
            else:
                patches.append(apr_patch)
                print(f"Discovered patch {nameFromDump(apr_patch)}")
    return len(patches), patches


def unescapeSysex(data):
    # A function to unescape the SysEx data
    result = []
    i = 0
    while i < len(data):
        if data[i] == 0xF7:
            break
        if data[i] & 0x80:
            result.append(((data[i] & 0x7F) << 7) | (data[i + 1] & 0x7F))
            i += 1
        else:
            result.append(data[i] & 0x7F)
        i += 1
    return result


def calculateFingerprint(message):
    if isEditBufferDump(message):
        # Just take the bytes after the name
        if isToneMessage(message):
            name_len = 10
        elif isPatchMessage(message):
            name_len = 18
        else:
            raise Exception("Message in Edit Buffer needs to be either a Tone message or a Patch message")
        return hashlib.md5(bytearray(message[7 + name_len:-1])).hexdigest()
    elif isSingleProgramDump(message):
        messages = knobkraft.findSysexDelimiters(message, 2)
        second_message = message[messages[1][0]:messages[1][1]]
        if isToneMessage(second_message):
            name_len = 10
        elif isPatchMessage(second_message):
            name_len = 18
        else:
            raise Exception("Message in Edit Buffer needs to be either a Tone message or a Patch message")
        return hashlib.md5(bytearray(second_message[7 + name_len:-1])).hexdigest()
    # If the message is not a program dump or part of a bank dump, return an empty string
    raise Exception("Can't calculate fingerprint of non-edit buffer or program dump message")


if __name__ == "__main__":
    single_apr = knobkraft.sysex.stringToSyx(
        "F0 41 35 00 24 20 01 20 4B 41 4C 49 4D 42 41 20 20 20 40 20 00 00 00 60 40 60 5D 39 00 00 7F 7F 7F 00 60 56 00 7F 40 60 00 37 00 00 2B 51 20 60 77 20 00 40 00 3E 00 0E 26 4A 20 12 31 00 30 20 7F 40 F7")
    assert isAprMessage(single_apr)
    assert isToneMessage(single_apr)
    #assert nameFromDump(single_apr) == " KALIMBA  "

    assert channelIfValidDeviceResponse(single_apr) == 0x00
    #bank_dump = knobkraft.load_sysex('testData/RolandMKS70_GENLIB-A.SYX')
    bank_dump = knobkraft.load_sysex('testData/RolandMKS70_MKSSYNTH.SYX')
    patches = []
    for loaded_message in bank_dump:
        number, converted = extractPatchesFromBank(loaded_message)
        if len(converted) > 0:
            # That worked
            for patch in converted:
                patches.append(patch)
    assert len(patches) == 64

    for patch in patches:
        assert isEditBufferDump(patch)

    #mks_message = knobkraft.sysex.stringToSyx("f0 41 3a 00 24 30 01 00 00 53 59 4e 54 48 20 42 00 41 53 53 20 2f 20 50 00 41 44 20 20 3f 54 27 00 26 00 4a 13 3d 00 0b 41 00 4e 1f 00 41 0c 43 00 1b 01 4b 0c 10 30 00 03 00 7e 7f 27 04 00 20 40 f7")
    #assert isPartOfBankDump(mks_message)