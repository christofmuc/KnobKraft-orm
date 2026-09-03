#
#   Adapted for Casio CZ-101 / CZ-1000 / CZ-5000
#

import hashlib
import knobkraft
import testing
from typing import Dict, List

CASIO_ID = 0x44
_CZ_NIBBLE_PAYLOAD_SIZE = 256
_CZ_SEND_DATA = 0x30
_CZ_RECEIVE_DATA = 0x20
_CZ_EDIT_BUFFER = 0x60

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
    # Validate the complete 263-byte send-data reply so an echoed request or
    # unrelated Casio message cannot produce a false-positive detection.
    if isSingleProgramDump(message) and len(message) == 263 and message[5] == _CZ_SEND_DATA:
        return message[4] & 0x0F
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


def _program_byte(patch_no: int) -> int:
    """Map KnobKraft's linear program number to the CZ memory address."""
    if 0 <= patch_no < 16:
        return 0x20 + patch_no
    if 16 <= patch_no < 32:
        return 0x40 + patch_no - 16
    raise ValueError(f"Invalid patch number for CZ-101: {patch_no}")


def _nibble_payload(message: List[int]):
    """Return the patch payload from either documented CZ single-patch form.

    Dumps sent by the synth are 263 bytes long and do not repeat the requested
    program address. Files intended for transmission to the synth are 264 bytes
    long and contain that address after command 0x20.
    """
    if not hasattr(message, "__len__") or len(message) not in (263, 264):
        return None
    if list(message[:4]) != [0xF0, CASIO_ID, 0x00, 0x00] or message[-1] != 0xF7:
        return None
    if (message[4] & 0xF0) != 0x70:
        return None

    if len(message) == 263 and message[5] == _CZ_SEND_DATA:
        payload = message[6:-1]
    elif len(message) == 264 and message[5] == _CZ_RECEIVE_DATA:
        payload = message[7:-1]
    else:
        return None

    if len(payload) != _CZ_NIBBLE_PAYLOAD_SIZE or any(value > 0x0F for value in payload):
        return None
    return list(payload)

def isSingleProgramDump(message: List[int]) -> bool:
    return _nibble_payload(message) is not None

def isEditBufferDump(message: List[int]) -> bool:
    if not isSingleProgramDump(message):
        return False
    # A 263-byte reply does not contain the source address, so a reply to an
    # edit-buffer request cannot be distinguished from a stored-program reply.
    return len(message) == 263 or message[6] == _CZ_EDIT_BUFFER

def convertToEditBuffer(channel, message):
    payload = _nibble_payload(message)
    if payload is None:
        raise ValueError("Can only convert CZ single program dumps")
    return [0xF0, CASIO_ID, 0x00, 0x00, 0x70 | (channel & 0x0F),
            _CZ_RECEIVE_DATA, _CZ_EDIT_BUFFER] + payload + [0xF7]


def convertToProgramDump(channel, message, program_number):
    payload = _nibble_payload(message)
    if payload is None:
        raise ValueError("Can only convert CZ single program dumps")
    return [0xF0, CASIO_ID, 0x00, 0x00, 0x70 | (channel & 0x0F),
            _CZ_RECEIVE_DATA, _program_byte(program_number)] + payload + [0xF7]

def nameFromDump(message):
    # CZ-101 patches do not contain ASCII name data. 
    return "CZ Patch"

def calculateFingerprint(message):
    payload = _nibble_payload(message)
    if payload is None:
        raise ValueError("Can't calculate fingerprint of non-program dump message")
    return hashlib.md5(bytearray(payload)).hexdigest()

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
    program_byte = _program_byte(patch_no)

    # Casio's documented combined Send Request (0x10) and Go Ahead (0x31).
    return [0xF0, CASIO_ID, 0x00, 0x00, 0x70 | (channel & 0x0F), 0x10, program_byte, 0x70 | (channel & 0x0F), 0x31, 0xF7]


def createEditBufferRequest(channel):
    return [0xF0, CASIO_ID, 0x00, 0x00, 0x70 | (channel & 0x0F), 0x10,
            _CZ_EDIT_BUFFER, 0x70 | (channel & 0x0F), 0x31, 0xF7]

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


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(
            message=data.all_messages[0],
            name="CZ Patch",
            target_no=11,
        )

    fixture_payload = knobkraft.load_sysex("testData/Casio_CZ101/bassbling.syx")[0][7:-1]
    device_reply = [0xF0, CASIO_ID, 0x00, 0x00, 0x76, _CZ_SEND_DATA] + fixture_payload + [0xF7]
    return testing.TestData(
        sysex="testData/Casio_CZ101/bassbling.syx",
        program_generator=programs,
        program_dump_request=(3, 17, "F0 44 00 00 73 10 41 73 31 F7"),
        device_detect_call="F0 44 00 00 70 10 20 70 31 F7",
        device_detect_reply=(device_reply, 6),
        expected_patch_count=1,
    )
