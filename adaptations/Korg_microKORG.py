#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#
#   Make sure microKORG is ready for sysex:
#   1) Turn off write protect
#      SHIFT + Program 8 -> Turn knob 1/Cutoff to confirm write protect is off.
#   2) Confirm MIDI FILTER setting:
#      SHIFT + Program 4 -> Turn knob 4/EG Release to confirm E-E is shown.
#
#   Sysex Dump Process Summary:
#   The dump is not a simple request/response. It's a multi-step conversation:
#   1. Handshake: The editor sends a generic "Identity Request". The microKORG S replies, identifying itself.
#   2. Preparation: The editor sends two preparatory messages to get the synth ready.
#   3. Request: The editor sends the "All Program Data Dump Request".
#   4. The Dump: The synth responds with a *single, massive sysex message* containing the data for all 256 patches.
#
from typing import List
import logging

import knobkraft

logging.basicConfig(level=logging.DEBUG)

# Global variable to track the last patch index for recall
_last_patch_index = 0

def name():
    return "Korg microKORG S"


def createDeviceDetectMessage(channel):
    # Sysex generic device detect message
    return [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]  # 0x7f is the global channel


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    # It's safer to have a small delay between messages to avoid overwhelming the synth's MIDI buffer.
    return 100


def needsChannelSpecificDetection():
    return False  # microKORG replies with its channel


def channelIfValidDeviceResponse(message):
    # microKORG S on channel 1 will reply with : f0 7e 00 06 02 42 40 01 ... f7
    # We confirm this is a microKORG S and extract the channel from index 2.
    if (len(message) > 8
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x42  # KORG ID
            and message[6] == 0x40  # microKORG Series ID
            and message[7] == 0x01):  # microKORG S Model

        # Extract the current MIDI channel from index 2 of the message
        return message[2]
    return -1


def createEditBufferRequest(channel):
    # microKORG S-native: F0 42 3n 00 01 40 10 F7
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x10, 0xf7]


def isEditBufferDump(message):
    # This identifies a single patch dump, e.g., the current edit buffer.
    # microKORG S-native: F0 42 3n 00 01 40 40 ... F7
    return (len(message) > 7
            and message[0] == 0xf0
            and message[1] == 0x42
            and (message[2] & 0xf0) == 0x30
            and message[3] == 0x00
            and message[4] == 0x01
            and message[5] == 0x40
            and message[6] == 0x40)


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 256


def getMidiBankFromIndex(index):
    return 0 if index < 128 else 1


def getMidiProgramFromIndex(index):
    return index % 128


def getPatchDisplayName(index, patch_message):
    # Combine patch location and patch name for display
    patch_location = patch_name_from_index(index)
    patch_name = nameFromDump(patch_message)
    return f"{patch_location}: {patch_name}"


def nameFromDump(message):
    # The patch name is stored as plain ASCII at the start of the data payload.
    # It does not need to be unescaped.
    # The payload starts at index 7.
    if isEditBufferDump(message) or isPartOfBankDump(message):
        try:
            # The name is the first 12 bytes of the payload
            name_bytes = message[7:19]
            # Convert to string, keeping all 12 characters (pad with spaces if needed)
            name = ''.join(chr(b) if 32 <= b <= 126 else ' ' for b in name_bytes)
            name = name.ljust(12)[:12]
            logging.debug(f"nameFromDump: extracted name '{name}' from bytes {name_bytes}")
            return name
        except (IndexError, UnicodeDecodeError):
            return "Invalid Name"
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        # Convert a patch from a bank dump into a sendable edit buffer format
        return message[0:2] + [0x30 | (channel & 0x0f)] + message[3:]
    raise Exception("This message can't be converted to an edit buffer.")


def createBankDumpRequest(channel, bank):
    # This sequence is based on the analysis of the sound editor's communication.
    # Knobkraft should send these four messages in order with a small delay.
    handshake = [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]
    prep_msg1 = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x0E, 0xf7]
    prep_msg2 = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x0F, 0xf7]
    # This is the actual "All Program Data Dump Request"
    dump_request = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x1D, 0x00, 0xf7]
    return [handshake, prep_msg1, prep_msg2, dump_request]


def isPartOfBankDump(message):
    # The bank dump is a single, large message. Its command byte is 0x50.
    # It looks similar to a single patch dump, but is much longer.
    return (len(message) > 1000  # A single patch is small, a bank is huge
            and message[0] == 0xF0
            and message[1] == 0x42
            and (message[2] & 0xF0) == 0x30
            and message[3] == 0x00
            and message[4] == 0x01
            and message[5] == 0x40
            and message[6] == 0x50  # This is the "Program Data Dump" command from the synth
            and message[-1] == 0xF7)


def extractPatchesFromBank(messages):
    # This function expects a list of all messages received during the dump.
    # It finds the single large bank dump message and extracts the 256 patches from it.
    bank_dump_message = None
    for msg in messages:
        if isPartOfBankDump(msg):
            bank_dump_message = msg
            break

    if not bank_dump_message:
        raise Exception("No valid bank dump message found in the provided list.")

    # The actual patch parameter data (which needs unescaping) starts after the sysex header
    data_payload = bank_dump_message[7:-1]
    unescaped_data = unescapeSysex(data_payload)

    PATCH_DATA_SIZE = 256  # microKORG S patch size
    PATCH_COUNT = 256

    patches = []
    channel = bank_dump_message[2] & 0x0f

    for i in range(PATCH_COUNT):
        start = i * PATCH_DATA_SIZE
        end = start + PATCH_DATA_SIZE
        if end <= len(unescaped_data):
            patch_data_segment = unescaped_data[start:end]
            header = [0xf0, 0x42, 0x30 | channel, 0x00, 0x01, 0x40, 0x40]
            escaped_patch_data = escapeSysex(patch_data_segment)
            full_patch_message = header + escaped_patch_data + [0xf7]
            patches.append(full_patch_message)
    return patches


def extractPatchesFromAllBankMessages(messages):
    return extractPatchesFromBank(messages)


# --- Utility functions for Sysex 7-bit <-> 8-bit data conversion ---

def unescapeSysex(sysex_7bit_data):
    # This function converts 7-bit sysex data back to 8-bit data.
    data_8bit = []
    i = 0
    while i < len(sysex_7bit_data):
        msbs = sysex_7bit_data[i]
        i += 1
        for j in range(7):
            if i < len(sysex_7bit_data):
                byte = sysex_7bit_data[i]
                if (msbs >> j) & 1:
                    byte |= 0x80
                data_8bit.append(byte)
                i += 1
    return data_8bit


def escapeSysex(data_8bit):
    # This function converts 8-bit data to 7-bit sysex format.
    sysex_7bit_data = []
    i = 0
    while i < len(data_8bit):
        chunk = data_8bit[i:i+7]
        msbs = 0
        for j, byte in enumerate(chunk):
            if byte & 0x80:
                msbs |= (1 << j)
        
        sysex_7bit_data.append(msbs)
        for byte in chunk:
            sysex_7bit_data.append(byte & 0x7f)
            
        i += 7
    return sysex_7bit_data


def isBankDumpFinished(messages):
    logging.debug(f"isBankDumpFinished: {len(messages)} messages")
    for i, msg in enumerate(messages):
        logging.debug(f"Message {i}: len={len(msg)} isPartOfBankDump={isPartOfBankDump(msg)}")
        if isPartOfBankDump(msg) and len(msg) > 70000:
            logging.debug(f"isBankDumpFinished: True (big message received, len={len(msg)})")
            return True
    logging.debug("isBankDumpFinished: False (waiting for big message)")
    return False


def createProgramDumpRequest(channel, patchNo):
    # Request a specific program dump
    # microKORG S-native: F0 42 3n 00 01 40 10 F7
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x10, 0xf7]


def supportsBankSelect():
    # Return True if this synth supports bank select
    return True


def bankSelect(channel, bank):
    global _last_patch_index
    midi_bank = 0 if _last_patch_index < 128 else 1
    print(f"bankSelect: channel={channel}, requested bank={bank}, patch_index={_last_patch_index}, midi_bank={midi_bank}")
    return [0xb0 | (channel & 0x0f), 32, midi_bank]


def patch_name_from_index(index):
    # The microKORG S has 4 banks (A, B, C, D) with 8 program banks (1-8), each with 8 programs (1-8)
    bank_char = chr(ord('A') + (index // 64))  # A, B, C, or D
    program_bank = (index % 64) // 8 + 1       # 1-8 (program bank)
    program_num = (index % 8) + 1              # 1-8 (program number)
    return f"{bank_char}{program_bank}{program_num}"


def friendlyProgramName(program_no):
    global _last_patch_index
    _last_patch_index = program_no
    bank = 0 if program_no < 128 else 1
    prog = program_no % 128
    return f"{bank:02d}-{prog:03d}"
