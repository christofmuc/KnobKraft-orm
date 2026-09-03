#
#   Adapted for Casio CZ-101 / CZ-1000 / CZ-5000
#

import hashlib
import knobkraft
from typing import Dict, List

CASIO_ID = 0x44

def name():
    return "Casio CZ-101 / CZ-1000"

def setupHelp():
    return """
The Casio CZ series uses a two-way handshake for SysEx dumps. 
Ensure both MIDI IN and MIDI OUT are connected to your interface.
SysEx must be enabled on the synthesizer (usually enabled by default).
Note: CZ synthesizers do not store patch names internally.
"""

def createDeviceDetectMessage(channel):
    # Command 0x10 is "Send Request" (Ask synth to send data to computer).
    # We pack the 0x31 "Go Ahead" command into the exact same string to bypass the handshake!
    return [0xF0, CASIO_ID, 0x00, 0x00, 0x70 | (channel & 0x0F), 0x10, 0x20, 0x70 | (channel & 0x0F), 0x31, 0xF7]

def deviceDetectWaitMilliseconds():
    return 200

def needsChannelSpecificDetection():
    return True

def channelIfValidDeviceResponse(message):
    # When we send a Send Request (0x10), the CZ responds with an ACK (0x30).
    # We explicitly check for this ACK to avoid validating our own echoed requests (e.g., via IAC Bus).
    if len(message) >= 6 and message[0] == 0xF0 and message[1] == CASIO_ID:
        channel_byte = message[4]
        command_byte = message[5]
        
        # 0x70 is the base channel identifier, 0x30 is the Casio ACK response
        if (channel_byte & 0xF0) == 0x70 and command_byte == 0x30:
            return channel_byte & 0x0F
    return -1

def bankDescriptors() -> List[Dict]:
    # 0x20..0x2F (Internal 1-16) and 0x40..0x4F (Cartridge 1-16)
    return [
        {"bank": 0, "name": "Internal Sounds", "size": 16, "type": "Patch"},
        {"bank": 1, "name": "Cartridge Sounds", "size": 16, "type": "Patch"}
    ]

def denibble_cz(data: List[int]) -> List[int]:
    """
    Casio CZ transmits bytes as two nibbles, LOW nibble first.
    e.g., 0x5F is transmitted as 0x0F, 0x05.
    """
    unpacked_data = []
    for i in range(0, len(data) - 1, 2):
        byte = (data[i] & 0x0F) | ((data[i + 1] & 0x0F) << 4)
        unpacked_data.append(byte)
    return unpacked_data

def nibble_cz(data: List[int]) -> List[int]:
    """
    Packs standard 8-bit bytes into Casio's Low-nibble, High-nibble format.
    """
    packed_data = []
    for byte in data:
        packed_data.append(byte & 0x0F)
        packed_data.append((byte >> 4) & 0x0F)
    return packed_data

def isSingleProgramDump(message: List[int]) -> bool:
    # A CZ single patch payload is 128 bytes sent as 256 nibbles + SysEx headers.
    # The total length is typically 263 bytes.
    if len(message) >= 263 and message[0] == 0xF0 and message[1] == CASIO_ID:
        return True
    return False

def isEditBufferDump(message: List[int]) -> bool:
    # The CZ edit buffer (temporary sound area) uses program number 0x60
    return isSingleProgramDump(message)

def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    if isSingleProgramDump(message):
        # To send to the edit buffer, we'd need to format a "Receive Request 1" 
        # targeting program 0x60, followed by the data. 
        # This requires adjusting the header bytes depending on how KnobKraft handles the handshake.
        return message
    raise Exception("Can only convert single program dumps")

def nameFromDump(message):
    # CZ-101 patches do not contain ASCII name data. 
    return "CZ Patch"

def calculateFingerprint(message):
    if isSingleProgramDump(message):
        # Extract the payload (ignoring headers) to hash the actual patch data
        index = knobkraft.sysex.findSysexDelimiters(message)
        if len(index) > 0:
            start, end = index[0]
            # Strip standard headers and F7
            payload = message[start+5:end-1]
            return hashlib.md5(bytearray(payload)).hexdigest()
    raise Exception("Can't calculate fingerprint of non-program dump message")

# ==============================================================================
# CZ-101 Parameter Mapping (256 bytes un-nibbled)
# This dictionary maps parameter names to their byte offset in the 256-byte array.
# Bitwise decoding/encoding would be needed for shared bytes (e.g. PFLAG).
# ==============================================================================

cz101_mapping = {
    "PFLAG": 0,           # Line select, Octave range
    "PDS": 1,             # Detune direction (0 = +, 1 = -)
    "PDETL": 2,           # Detune fine data
    "PDETH": 3,           # Detune octave and note data
    "PVK": 4,             # Vibrato wave number
    "PVDLD_1": 5,         # Vibrato delay (Byte 1)
    "PVDLD_2": 6,         # Vibrato delay (Byte 2)
    "PVDLV": 7,           # Vibrato delay (Byte 3)
    "PVSD_1": 8,          # Vibrato rate (Byte 1)
    "PVSD_2": 9,          # Vibrato rate (Byte 2)
    "PVSV": 10,           # Vibrato rate (Byte 3)
    "PVDD_1": 11,         # Vibrato depth (Byte 1)
    "PVDD_2": 12,         # Vibrato depth (Byte 2)
    "PVDV": 13,           # Vibrato depth (Byte 3)
    "MFW_1": 14,          # DCO1 waveform (Byte 1)
    "MFW_2": 15,          # DCO1 waveform (Byte 2)
    "MAMD": 16,           # DCA1 key follow (Byte 1)
    "MAMV": 17,           # DCA1 key follow (Byte 2)
    "MWMD": 18,           # DCW1 key follow (Byte 1)
    "MWMV": 19,           # DCW1 key follow (Byte 2)
    "PMAL": 20,           # End step number of DCA1 envelope
    # DCA1 Envelope Rate/Level (16 bytes starting at 21)
    # ... Add remaining offsets sequentially based on the 25-section table ...
}

def load_cz_parameter(unpacked_msg, offset):
    """Loads a parameter from the un-nibbled 256-byte array."""
    return unpacked_msg[offset]

def save_cz_parameter(unpacked_msg, offset, value):
    """Saves a parameter to the un-nibbled 256-byte array."""
    unpacked_msg[offset] = value
    
def messageTimings():
    return {
        # 263 bytes takes ~85ms to travel over MIDI. 
        # 200ms gives the CZ-101 plenty of time to finish dumping before we ask for the next patch!
        "generalMessageDelay": 300, 
    }

def createProgramDumpRequest(channel, patch_no):
    # Map KnobKraft's 0-31 absolute index to CZ-101's internal program hex values
    if 0 <= patch_no < 16:
        # Bank 0 (Internal Sounds): 0x20 .. 0x2F
        program_byte = 0x20 + patch_no
    elif 16 <= patch_no < 32:
        # Bank 1 (Cartridge Sounds): 0x40 .. 0x4F
        program_byte = 0x40 + (patch_no - 16)
    else:
        raise Exception(f"Invalid patch number for CZ-101: {patch_no}")
    
    # 1. The Send Request (0x10) + 2. The Go Ahead Command (0x31)
    # Packed tightly together into one illegal-but-effective 10-byte SysEx missile!
    return [0xF0, CASIO_ID, 0x00, 0x00, 0x70 | (channel & 0x0F), 0x10, program_byte, 0x70 | (channel & 0x0F), 0x31, 0xF7]

def createBankDumpRequest(channel, bank_id):
    messages = []
    # Bank 0 starts at index 0, Bank 1 (Cartridge) starts at index 16
    start_patch = bank_id * 16
    for i in range(16):
        # Fire our blind handshake for each patch in the bank
        messages.extend(createProgramDumpRequest(channel, start_patch + i))
    return messages

def isPartOfBankDump(message):
    # Any valid individual patch dump we receive back counts as part of the bank
    return isSingleProgramDump(message)

def isBankDumpFinished(messages):
    # 'messages' is a list of individual MIDI messages (List[List[int]])
    # Count how many valid 263-byte patch dumps we've received so far.
    valid_patches = sum(1 for msg in messages if isSingleProgramDump(msg))
    return valid_patches >= 16

def extractPatchesFromAllBankMessages(messages):
    # KnobKraft hands us all the messages it collected. 
    # We just filter out the valid patches and return them to be saved!
    patches = []
    for msg in messages:
        if isSingleProgramDump(msg):
            patches.append(msg)
    
    return patches