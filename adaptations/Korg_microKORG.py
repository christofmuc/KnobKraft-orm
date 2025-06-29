#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#
#   Make sure microKORG is ready for sysex:
#   1) Turn off write protect
#   SHIFT + Program 8 - Turn knob 1/Cutoff to confirm write protect is off.
#   2) Confirm MIDI FILTER setting:
#   SHIFT + Program 4. Turn knob 4/EG Release to confirm E-E is shown.

from typing import List

import knobkraft


def name():
    return "Korg microKORG S"


def createDeviceDetectMessage(channel):
    # Sysex generic device detect message
    return [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7] # 0x7f is the global channel


def deviceDetectWaitMilliseconds():
    return 200


#def generalMessageDelay():
    # The microKORG doesn't seem to like to get the messages too fast, so wait a bit between messages
#    return 400


def needsChannelSpecificDetection():
    return False # microKORG replies with his channel


def channelIfValidDeviceResponse(message):
    # microKORG on channel 1 will reply with : f0 7e 00 06 02 42 40 01 01 00 00 01 01 00 f7 we need to confirm the this is a microKORG S and extract the channel from index 2
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
    # microKORG S-native: F0 42 3n 00 01 40 40 ... F7
    return (len(message) > 6
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
    return 256 # The microKORG S has 128 patches in its main bank

def patch_name_from_index(index):
    # The microKORG S has banks A and B, plus user banks C and D.
    # A11-A88, B11-B88, etc.
    bank_char = chr(ord('A') + (index // 64))
    bank_num = (index % 64) // 8 + 1
    prog_num = (index % 8) + 1
    return f"{bank_char}{bank_num}{prog_num}"

# Helper to get display name for a patch: e.g., 'A11 Pump St'
def getPatchDisplayName(index, patch_message):
    # Combine patch location and patch name for display
    return f"{patch_name_from_index(index)} {nameFromDump(patch_message)}"

def nameFromDump(message):
    #print(f"[Korg_microKORG] nameFromDump called. Message length: {len(message)}")
    if isEditBufferDump(message) or isPartOfBankDump(message):
        try:
            # Unescape the sysex data (skip header and F7)
            patchData = unescapeSysex(message[7:-1])
            # The name is usually in the first 12 bytes, but sometimes the first byte is a reserved/null
            start = 0
            if not (32 <= patchData[0] <= 126):
                start = 1
            name_bytes = patchData[start:start+12]
            name = ''.join(chr(b) for b in name_bytes if 32 <= b <= 126).strip()
            #print(f"[Korg_microKORG] nameFromDump extracted: '{name}'")
            return name
        except (IndexError, UnicodeDecodeError) as e:
            print(f"[Korg_microKORG] nameFromDump error: {e}")
            return "Invalid Name"
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        # The data structures are the same, only byte 2 seems to contain the device ID (or channel), and
        # byte 6 should be 0x40 for a current program data dump
        return message[0:2] + [0x30 | (channel & 0x0f)] + message[3:6] + [0x40] + message[7:]
    raise Exception("Neither edit buffer nor program dump can't be converted")

def createBankDumpRequest(channel, bank):
    print(f"[Korg_microKORG] createBankDumpRequest called with channel={channel}, bank={bank}")
    # This sequence is based on the analysis of the sound editor's communication.
    handshake = [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]
    prep_msg1 = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x0E, 0xf7]
    prep_msg2 = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x0F, 0xf7]
    dump_request = [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x00, 0x01, 0x40, 0x1D, 0x00, 0xf7]
    result = [handshake, prep_msg1, prep_msg2, dump_request]
    print(f"[Korg_microKORG] createBankDumpRequest returning {len(result)} messages")
    return result


def isPartOfBankDump(message):
    print(f"[Korg_microKORG] isPartOfBankDump called. First 8 bytes: {message[:8]}, len={len(message)}")
    return (
        len(message) > 100 and
        message[0] == 0xF0 and
        message[1] == 0x42 and
        (message[2] & 0xF0) == 0x30 and
        message[3] == 0x00 and
        message[4] == 0x01 and
        message[5] == 0x40 and
        message[6] == 0x50 and
        message[-1] == 0xF7
    )


def extractPatchesFromBank(messages):
    print(f"[Korg_microKORG] extractPatchesFromBank called with {len(messages)} messages")
    patches = []
    for msg in messages:
        print(f"[Korg_microKORG] Checking message: {msg[:8]}, len={len(msg)}")
        if isPartOfBankDump(msg):
            print("[Korg_microKORG] Message is part of bank dump")
            # Unescape the sysex data (if needed)
            data = unescapeSysex(msg[7:-1])
            PATCH_SIZE = 256  # or whatever the correct size is
            PATCH_COUNT = 256
            for i in range(PATCH_COUNT):
                patch = data[i*PATCH_SIZE:(i+1)*PATCH_SIZE]
                if len(patch) == PATCH_SIZE:
                    # Re-wrap as a single patch sysex message
                    program_dump = [0xf0, 0x42, 0x30 | (msg[2] & 0x0f), 0x00, 0x01, 0x40, 0x40] + escapeSysex(patch) + [0xf7]
                    patches.append(program_dump)
    print(f"[Korg_microKORG] Extracted {len(patches)} patches from bank dump.")
    return patches

# --- Utility functions for Sysex data handling ---

def unescapeSysex(sysex):
    # This function converts the 7-bit sysex data back to 8-bit data.
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        # The first byte of each 8-byte block contains the MSBs of the following 7 bytes.
        ms_bits = sysex[dataIndex]
        dataIndex += 1
        for i in range(7):
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex] | ((ms_bits & (1 << i)) << (7 - i)))
            dataIndex += 1
    return result


def escapeSysex(data):
    # This function converts 8-bit data to 7-bit sysex format.
    result = []
    dataIndex = 0
    while dataIndex < len(data):
        ms_bits = 0
        # Collect the MSBs from the next 7 bytes.
        for i in range(7):
            if dataIndex + i < len(data):
                ms_bits |= ((data[dataIndex + i] & 0x80) >> (7 - i))
        result.append(ms_bits)
        # Append the 7-bit data bytes.
        for i in range(7):
            if dataIndex + i < len(data):
                result.append(data[dataIndex + i] & 0x7f)
        dataIndex += 7
    return result