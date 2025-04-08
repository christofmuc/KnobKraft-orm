# NOTE: This has been superseded by the file in the adaptations directory. I archive this here for later reference, and because
# it still has a few code pieces that the official adaptation hasn't. Once all is migrated, we can remove this file.

#
# Licensed under the MIT No Attribution License
# Copyright 2022 Constantin Gonzalez, <constantin@glez.de>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#
# This is an adaptation for KnobKraft ORM for supporting the Hydrasynth Desktop synth.
#
# NOTE: THIS CODE IS EXPERIMENTAL AND BASED ON A CRUDE REVERSE ENGINEERING ATTEMPT FROM WHAT WE SAW
# DURING MIDI DUMPS OF THE NATIVE HYDRASYNTH MANAGER APP INTERACTING WITH THE SYNTH.
# IN PARTICULAR, SENDING A PATCH TO THE SYNTH TRIGGERS A FLASH UPDATE EVERY TIME WHICH MAY OR MAY NOT
# RESULT IN EXTRA WEAR OF THE SYNTH'S FLASH MEMORY.
# ALWAYS BACKUP YOUR SYNTH FIRST.
# USE AT YOUR OWN RISK.
#
# However, if you have any feedback, especially if you found a documentation or protocol feature that
# isn't reflected here, please let me know at constantin@glez.de.
#
# See: https://github.com/christofmuc/KnobKraft-orm to learn more about KnobKraft ORM and
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Programming%20Guide.md
# For how this adaptation was created.
#
# Thanks to Christof Ruch for researching the underlying Hydrasynth messaging protocol.
#
# Run 'pytest ASM_Hydrasynth.py' to run all tests contained here.
#

# Release history:
#
# V 0.2.0 (2022-11-06):
# * Extract Hydrasynth categories from patch and map to KnobKraft tags.
#
# V 0.1.0 (2022-11-06):
# * Identity
# * Storage size
# * Device detection
# * Program dump capability
# * Edit buffer capability (implemented as program dump/write to A001)
# * Number from dump
# * Name from dump
# * Rename patch
# * Friendly bank name
# * Friendly program name
# * Calculate fingerprint
# * Setup help


# Imports

import sys
import logging
from zlib import crc32
from base64 import b64decode, b64encode
import hashlib


# Constants

LOGGING_LEVEL = logging.INFO
VERSION = '0.2.0'

# MIDI
SYSEX_START = bytes.fromhex('f0')
SYSEX_END = bytes.fromhex('f7')

# Hydrasynth specific
SYNTH_NAME = 'ASM Hydrasynth'
MANUFACTURER_ID = bytes.fromhex('00 20 2B')  # ASM
SYSEX_ID = bytes.fromhex('00')
MODEL_ID = bytes.fromhex('6F')  # Hydrasynth Desktop
SYSEX_PREAMBLE = SYSEX_START + MANUFACTURER_ID + SYSEX_ID + MODEL_ID
ENDIANNESS = 'little'
MINIMUM_MESSAGE_SIZE = 4 + 4  # Excluding preamble and SYSEX_END. 4 bytes checksum + 1 byte command + 1 byte payload in b64.
MESSAGES_PER_PATCH = 22

# Commands are one byte.
COMMANDS = {
    'CONNECT': bytes.fromhex('00'),
    'CONNECT_ACK': bytes.fromhex('01'),
    'PATCH_NAME_REQUEST': bytes.fromhex('02'),  # Payload: [0, 0]. Triggers DATA responses with all the patch names for the current bank.
    'PATCH_REQUEST': bytes.fromhex('04'),  # Payload: [0, bank#, patch#]. Triggers the first DATA response with the initial page of patch data, including the name.
    'PATCH_WRITE_REQUEST_RECEIVED': bytes.fromhex('07'), # Payload: [0, bank#, patch#]. Sent from Hydrasynth after completing a DATA transfer that contains a patch write request.
    'UPDATE_FLASH': bytes.fromhex('14'), # Payload: [0]. Sent to Hydrasynth to write data into flash after receiving.
    'UPDATE_FLASH_COMPLETE': bytes.fromhex('15'), # Payload: [0]. Sent from Hydrasynth after completing Flash update.
    'DATA': bytes.fromhex('16'),  # Payload [0, page#, total_pages, data...]. page# starts with 0.
    'DATA_ACK': bytes.fromhex('17'),  # Payload: [0, page#, total_pages]. Triggers next page after acknowledging.
    'BANK_SWITCH': bytes.fromhex('18'),  # Payload: [bank#]. bank# starts at 0 and maps to banks A .. F
    'BANK_SWITCH_ACK': bytes.fromhex('19'),
    'PATCH_REQUEST_COMPLETE': bytes.fromhex('1A'),  # Payload: [0]. Guessing meaning here, comes after a completed patch transfer from HS->PC.
    'PATCH_REQUEST_COMPLETE_ACK': bytes.fromhex('1B'),  # Payload: [0]. Guessing.
    'FIRMWARE_VERSION_REQUEST': bytes.fromhex('28'),  # Payload [0].
    'FIRMWARE_VERSION_RESPONSE': bytes.fromhex('29'),  # Payload [0, v_major, 0, v_minor]. v_major = 155 -> V1.55. v_minor is a guess.
}
COMMANDS_REVERSE = {
    value: key for key, value in COMMANDS.items()
}

DATA_TYPES = {
    'PATCH_DUMP': 5,
    'PATCH_WRITE_DATA': 6
}
DATA_TYPES_REVERSE = {
    value: key for key, value in DATA_TYPES.items()
}

# KnobKraft ORM categories:
KNOBKRAFT_CATEGORIES = [
    'Ambient',
    'Arp',
    'Bass',
    'Bell',
    'Brass',
    'Drone',
    'Drum',
    'Keys',
    'Lead',
    'Organ',
    'Pad',
    'Pluck',
    'SFX',
    'Voice',
    'Wind'
]

HYDRASYNTH_TO_KNOBKRAFT_CATEGORIES = {
    'Ambient': ['Ambient'],
    'Arp': ['Arp'],
    'Bass': ['Bass'],
    'BassLead': ['Bass', 'Lead'],
    'Brass': ['Brass'],
    'Chord': ['Keys', 'Pad'],
    'Drum': ['Drum'],
    'E-piano': ['Keys', 'Pluck'],
    'Fx': ['SFX'],
    'FxMusic': ['SFX', 'Ambient', 'Pad'],
    'Keys': ['Keys'],
    'Lead': ['Lead'],
    'Organ': ['Organ'],
    'Pad': ['Pad'],
    'Perc': ['Drum'],
    'Rhytmic': ['Drum', 'Arp'],
    'Sequence': ['Arp'],
    'Strings': ['Pad'],
    'Vocal': ['Voice']
}

HYDRASYNTH_CATEGORIES = sorted(HYDRASYNTH_TO_KNOBKRAFT_CATEGORIES.keys())

DEFAULT_BANK_NAMES = ['A', 'B', 'C', 'D', 'E']  # Most Hydrasynths have these 5 banks
PATCHES_PER_BANK = 128

# We use this program number as a "fake" edit buffer.
EDIT_BUFFER_BANK_NUMBER = 0
EDIT_BUFFER_PATCH_NUMBER = 0
EDIT_BUFFER_PROGRAM_NUMBER = EDIT_BUFFER_BANK_NUMBER * PATCHES_PER_BANK + EDIT_BUFFER_PATCH_NUMBER

# Wait this amount of milliseconds between sending messages.
GENERAL_MESSAGE_DELAY_MILLIS = 30  # Seems like a safe value.

# Help test
HELP_TEXT = f'''
Hydrasynth adaptation V{VERSION}.

NOTE: THIS CODE IS EXPERIMENTAL AND BASED ON A CRUDE REVERSE ENGINEERING ATTEMPT FROM WHAT WE SAW DURING MIDI DUMPS OF THE NATIVE HYDRASYNTH MANAGER APP INTERACTING WITH THE SYNTH.
IN PARTICULAR, SENDING A PATCH TO THE SYNTH TRIGGERS A FLASH UPDATE EVERY TIME WHICH MAY OR MAY NOT RESULT IN EXTRA WEAR OF THE SYNTH'S FLASH MEMORY.
ALWAYS BACKUP YOUR SYNTH FIRST.
USE AT YOUR OWN RISK.

This adaptation has been tested with Hydrasynth Desktop. It may or may not work with other Hydrasynth models.

We don't know (yet) if and if yes, how Hydrasynth supports a temporary edit buffer. Therefore, this adaptation uses patch A001 as a "fake" edit buffer.
When using this adaptation, set your Hydrasynth to patch A001 and whenever you click on a new patch, it should appear there.

Please note that each patch update to the synth triggers a write to Flash memory, which takes approx. 10-15 seconds.
DO NOT TURN OFF YOUR SYNTH WHEN UPDATING PATCHES.

Also, when updating patches, we're currently waiting a fixed time between SysEx messages, instead of waiting for the synth to send us a handshake message. This works quite well for now, but may occasionally trigger a reboot on the synth side. In this cases, simply try again.

This adaptation is experimental for now. Please send any feedback, bug reports, suggestions and especially any insights into the Hydrasynth patch management protocol to: constantin@glez.de.

Thanks!

Constantin Gonzalez
'''

# Classes

# KnobKraft ORM doesn't support Python logging. It assumes we're using print() to
# output messages. Sending log output to stdout doesn't seem to work, so we're
# implementing a simple handler that writes log messages through print().
class PrintHandler(logging.Handler):
    """
    A handler class which uses print() to output log messages.
    """
    def __init__(self):
        logging.Handler.__init__(self)

    def emit(self, record):
        print(self.format(record))


# Globals

logging.basicConfig(level=LOGGING_LEVEL, handlers=[PrintHandler()], force=True)
logger = logging.getLogger(__name__)

bank_names = DEFAULT_BANK_NAMES


# Utility functions

# CRC32 JAMCRC is a bitwise NOT of the CRC32 checksum.
# See: https://stackoverflow.com/questions/58861209/crc-python-how-to-calculate-jamcrc-decimal-number-from-string
def crc32_jamcrc(b):  # b should be a bytearray
    return int("0b"+"1"*32, 2) - crc32(b)


# Format a bytes or bytearray object as a nice string of hex tuples.
def to_hex_str(b):
    if not isinstance(b, bytes) or isinstance(b, bytearray):
        b = bytes(b)

    return ' '.join([f'{i:02X}' for i in b])


# Format a bytes or bytearray object as ASCI characters where possible, or hex instead.
def to_str(b):
    result = ''
    for i in list(b):
        if i > 31 and i < 128:  # Printable
            result += chr(i)
        else:  # Non-printable
            result += f'0x{i:02X}'

    return result


# Encode a command and its payload into a Hydrasynth specific SysEx message.
# Command is a string, used as key for the COMMANDS dict above.
# Payload is either a list of ints (to be converted to bytes), a hex string, a bytearray or a bytes object.
# Returns a list of ints (representing byte values) ready for sending back to KnobKraft ORM for sending as MIDI.
def to_hydrasynth(command, payload):
    command_bytes = COMMANDS[command]

    payload_bytes = None
    if isinstance(payload, bytes):
        payload_bytes = payload
    elif isinstance(payload, list):
        payload_bytes = bytes(payload)
    elif isinstance(payload, str):
        payload_bytes = bytes.fromhex(payload)
    else:
        raise(ValueError('Invalid payload format.'))

    message_bytes = command_bytes + payload_bytes
    checksum_bytes = crc32_jamcrc(message_bytes).to_bytes(4, byteorder=ENDIANNESS, signed=False)
    message_b64 = b64encode(checksum_bytes + message_bytes)

    result = list(SYSEX_PREAMBLE + message_b64 + SYSEX_END)
    return result


# Decode a command byte into a human-readable command string.
def decode_command(c):
    return COMMANDS_REVERSE.get(c, 'COMMAND_' + to_hex_str(c))


# Decode and verify a SysEx message coming from Hydrasynth into a command string and a payload.
# The message is either a string with hex values (to allow for debugging MIDI dumps) or a list of values (interpreted as bytes).
# We return the Hydrasynth command as a string (see COMMANDS keys) if known, or a hex dump if not, or None if the message was invalid.
# We return the payload as a list of byte values as ints, or None if the message is invalid.
# If the message is too short to be interpreted as a base 64 decoded, checksummed message, we return None for the command, and
# the raw message body as the payload.
def from_hydrasynth(message):
    message_bytes = None
    if isinstance(message, list):
        message_bytes = bytes(message)
    elif isinstance(message, str):
        message_bytes = bytes.fromhex(message)
    else:
        raise(ValueError('Invalid message format.'))

    if bytes([message_bytes[0]]) != SYSEX_START or bytes([message_bytes[-1]]) != SYSEX_END:
        logger.warning(f'Invalid SysEx message: {message_bytes[0]:02X} ... {message_bytes[-1]:02X}')
        return None, list(message_bytes)

    if message_bytes[:len(SYSEX_PREAMBLE)] != SYSEX_PREAMBLE:
        logger.warning(f'Hydrasynth preamble missing. Returning raw message: {message_bytes}')
        return None, list(message_bytes)

    message_b64 = message_bytes[len(SYSEX_PREAMBLE):-1]
    if len(message_b64) < MINIMUM_MESSAGE_SIZE:
        logger.warning(f'Hydrasynth message length ({len(message_b64)}) is smaller than minimum ({MINIMUM_MESSAGE_SIZE}).')
        logger.warning(f'Returning SysEx message content: {message_b64}')
        return None, list(message_b64)

    # We have a potential Hydrasynth message. Let’s verify its checksum.
    message_decoded = b64decode(message_b64)

    message_checksum = message_decoded[:4]
    message_bytes = message_decoded[4:]
    checksum_bytes = crc32_jamcrc(message_bytes).to_bytes(4, byteorder=ENDIANNESS, signed=False)
    if message_checksum != checksum_bytes:
        logger.warning(f'Message checksum ({message_checksum}) does not match computed checksum ({checksum_bytes}).')
        logger.warning('Returning None.')
        return None, None

    # We have a valid Hydrasynth message! Let’s return its content.
    command_bytes = message_bytes[:1]
    command_str = decode_command(command_bytes)

    payload_bytes = message_bytes[1:]
    payload = list(payload_bytes)

    return command_str, payload
    

# KnobKraft ORM functions

# Identity
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Programming%20Guide.md#identity

def name():
    return SYNTH_NAME


# Storage size (new method)
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Programming%20Guide.md#new-method

# New method for describing the synth bank structure.
def bankDescriptors():
    return [
        {
            'bank': i,
            'name': n,
            'size': PATCHES_PER_BANK,
            'type': 'Patch',
            'isROM': False
        }
        for i, n in enumerate(bank_names)
    ]


def test_bankDescriptors():
    result = bankDescriptors()
    for i, b in enumerate(result):
        assert isinstance(b, dict)
        assert b['bank'] == i
        assert isinstance(b['name'], str)
        assert b['size'] == PATCHES_PER_BANK
        assert b['type'] == 'Patch'
        assert b['isROM'] == False
    

# Device detection.
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Programming%20Guide.md#device-detection

'''
When Hydrasynth manager connects to Hydrasynth Desktop, we see:
08:52:30.709	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 41 4F 30 6D 76 67 41 41 F7
Decoded: CONNECT, '0x00'

08:52:30.719	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 31 bytes	F0 00 20 2B 00 6F 4B 4B 43 61 59 41 45 41 51 54 70 43 4F 6B 4D 36 52 44 70 46 41 41 3D 3D F7
Decoded: CONNECT_ACK, '0x00A:B:C:D:E0x00'

We'll use this as identification procedure.

We also see:
08:52:30.760	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 71 6B 4E 37 34 79 67 41 F7
Decoded: FIRMWARE_VERSION_REQUEST, '0x00'

08:52:30.761	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 71 6E 4E 45 36 69 6B 41 6D 77 41 43 F7
Decoded: FIRMWARE_VERSION_RESPONSE, '0x000x9B0x000x02'

08:52:30.761	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 8 bytes	F0 00 20 2B 00 6F 01 F7
Decoded: Weird, as 01 F7 are not enough to embed a full, base 64 encoded message with its checksum.
... but will ignore it for now.
'''


'''
08:52:30.709	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 41 4F 30 6D 76 67 41 41 F7
'''
def createDeviceDetectMessage(_):  # usually, we would take a channel here, but Hydrasynth doesn't support it.
    message = to_hydrasynth('CONNECT', [0])
    logger.debug(f'Creating device detection message: {to_hex_str(message)}')
    return message


def test_createDeviceDetectMessage():
    channel = 1
    result = createDeviceDetectMessage(channel)
    assert isinstance(result, list)
    for i in result:
        assert isinstance(i, int)
        assert i >= 0 and i < 256

    result_str = to_hex_str(result)
    assert result_str == 'F0 00 20 2B 00 6F 41 4F 30 6D 76 67 41 41 F7'


# No channel specific detection supported.
def needsChannelSpecificDetection():
    return False


# Verify if we got a valid response from our device detect message.
# Look for:
# 08:52:30.719	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 31 bytes	F0 00 20 2B 00 6F 4B 4B 43 61 59 41 45 41 51 54 70 43 4F 6B 4D 36 52 44 70 46 41 41 3D 3D F7
# Decoded: CONNECT_ACK, '0x00A:B:C:D:E0x00' The synth tells us the banks it supports and their names.
def channelIfValidDeviceResponse(message):
    global bank_names
    command, payload = from_hydrasynth(message)

    if command != 'CONNECT_ACK':
        logger.warning(f'Invalid command received during device detect: {command}')
        return -1
        
    # Hydrasynth Desktop returns: 0x00A:B:C:D:E0x00 listing available bank names.
    if payload[0] != 0 and payload[-1] != 0:
        logger.warning(f'Invalid payload received during device detect: {to_hex_str(payload)}')
        return -1

    try:
        bank_string = bytes(payload[1:-1]).decode('ascii', errors='strict')
    except UnicodeDecodeError: 
        logger.warning('Cannot decode bank names as ASCII.')
        return -1

    if ':' not in bank_string:
        logger.warning('Bank names not delimited by ":"')
        return -1

    bank_names = bank_string.split(':')
    if len(bank_names) not in [5, 8]:
        logger.warning(f'Unusual number of banks detected: {len(bank_names)}. Expected 5 or 8.)')

    logger.debug(f'Got valid Hydrasynth device response message with bank names: {bank_names}. Message: {to_hex_str(message)}.')
    return 1  # We don't support channel detection, so return valid channel 1 if all ok.


def test_channelIfValidDeviceResponse():
    correct_message = list(bytes.fromhex('F0 00 20 2B 00 6F 4B 4B 43 61 59 41 45 41 51 54 70 43 4F 6B 4D 36 52 44 70 46 41 41 3D 3D F7'))
    assert channelIfValidDeviceResponse(correct_message) == 1

    # Wrong preamble
    incorrect_message = list(bytes.fromhex('F0 00 20 20 00 6F 4B 4B 43 61 59 41 45 41 51 54 70 43 4F 6B 4D 36 52 44 70 46 41 41 3D 3D F7'))
    assert channelIfValidDeviceResponse(incorrect_message) == -1

    # Wrong command/payload
    incorrect_message = list(bytes.fromhex('F0 00 20 2B 00 6F 71 6E 4E 45 36 69 6B 41 6D 77 41 43 F7'))
    assert channelIfValidDeviceResponse(incorrect_message) == -1


# Edit Buffer capability.
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Programming%20Guide.md#edit-buffer-capability

# Right now, we don't know if Hydrasynths support edit buffers. We will start by treating bank A, Patch 0 as an edit buffer.
# Then we can experiment with different messages and see if we can get something like an edit buffer out.
# This means we'll implement this capability in parallel to the Program dump capability, because they work very similarly.
#
# Program Dump capability
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptions/Adaptation%20Programming%20Guide.md#program-dump-capability

'''
Reading out a patch buffer from Bank 0 (A), patch 0, we see:
09:02:32.063	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 57 58 55 39 50 42 67 41 F7
Decoded: BANK_SWITCH, [0]

09:02:32.098	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 47 45 51 6D 4A 52 6B 41 F7
Decoded: BANK_SWITCH_ACK, [0])

09:02:32.202	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 74 4C 66 5A 55 51 51 41 41 41 41 3D F7
Decoded: PATCH_REQUEST, [0, 0, 0]  # 0, patch number (0-based), bank number (0-based)

09:02:32.202	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 191 bytes	F0 00 20 2B 00 6F 56 65 33 6D 68 52 59 41 41 42 59 46 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7
Decoded: DATA, [0, 0, 22, 5, 0, 0, 0, 230, 59, 0, 0, 13, 83, 97, 119, 112, 114, 101, 115, 115, 105, 118, 101, 32, 71, 68, 0, 32, 0, 32, 0, 176, 4, 0, 0, 4, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 115, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0]
# 0, data chunk number (0), total data chunks (22), data format (5 = patch dump), 0, bank number (0-based, 0 = A), patch number (0-based, 0), then raw data.

09:02:32.202	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 6C 50 71 68 35 78 63 41 41 42 59 3D F7
Decoded: DATA_ACK, [0, 0, 22]

09:02:32.206	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 191 bytes	F0 00 20 2B 00 6F 56 68 67 74 58 78 59 41 41 52 59 41 41 41 41 41 41 41 41 41 41 41 4D 41 39 50 38 41 41 47 51 41 41 41 41 42 41 45 41 43 41 41 51 41 41 41 41 41 41 41 41 43 41 41 41 41 41 41 49 41 41 41 41 41 41 41 41 41 42 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 45 41 43 41 41 51 41 41 41 41 41 41 41 41 43 41 41 45 41 41 41 49 41 41 41 41 41 41 41 41 41 42 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 3D 3D F7
Decoded: DATA, [0, 1, 22, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 244, 255, 0, 0, 100, 0, 0, 0, 1, 0, 64, 2, 0, 4, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 64, 2, 0, 4, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] 

09:02:32.206	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 31 63 75 36 2F 68 63 41 41 52 59 3D F7
Decoded: DATA_ACK, [0, 1, 22]

09:02:32.209	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 191 bytes	F0 00 20 2B 00 6F 66 58 34 37 52 42 59 41 41 68 59 41 41 41 41 41 41 41 41 41 41 41 41 41 41 51 41 41 41 67 41 41 41 41 41 41 42 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 67 41 43 41 41 49 41 41 41 41 41 44 41 41 41 41 67 41 43 41 41 41 41 41 41 41 41 42 77 42 73 41 64 73 41 41 41 41 41 41 67 41 43 41 41 49 41 41 77 41 41 41 41 41 41 41 41 41 41 41 41 51 41 41 41 41 41 41 41 49 41 41 67 41 43 41 41 49 41 41 67 41 43 78 77 45 42 41 41 49 41 47 41 49 2B 41 57 67 42 4C 67 41 42 41 41 41 41 41 67 41 41 41 46 55 43 78 41 47 52 41 45 49 42 41 41 41 41 41 41 3D 3D F7
Decoded: DATA, [0, 2, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 2, 0, 0, 0, 0, 12, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 7, 0, 108, 1, 219, 0, 0, 0, 0, 2, 0, 2, 0, 2, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 199, 1, 1, 0, 2, 0, 24, 2, 62, 1, 104, 1, 46, 0, 1, 0, 0, 0, 2, 0, 0, 0, 85, 2, 196, 1, 145, 0, 66, 1, 0, 0, 0, 0]) 

...

09:02:32.241	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 77 53 30 50 79 52 63 41 46 42 59 3D F7
Decoded: DATA_ACK, [0, 20, 22]

09:02:32.243	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 155 bytes	F0 00 20 2B 00 6F 69 71 4B 77 6C 68 59 41 46 52 59 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 3D F7
Decoded: DATA, [0, 21, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])

09:02:32.243	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 67 42 77 55 30 42 63 41 46 52 59 3D F7
Decoded: DATA_ACK, [0, 21, 22]

09:02:32.312	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 32 78 63 4C 44 68 6F 41 F7
Decoded: COMMAND_1A, [0]  # No idea what this means. Could be patch data transfer complete.

09:02:32.312	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 6D 69 59 51 46 78 73 41 F7 
Decoded: COMMAND_1B, [0]


Dumping patch 128 of Bank B, we get:
09:12:54.234	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 57 58 55 39 50 42 67 41 F7
('BANK_SWITCH', [0])

09:12:54.302	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 47 45 51 6D 4A 52 6B 41 F7
('BANK_SWITCH_ACK', [0])

09:12:54.392	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 57 4F 70 34 69 41 51 41 41 58 38 3D F7
('PATCH_REQUEST', [0, 1, 127])

09:12:54.392	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 191 bytes	F0 00 20 2B 00 6F 66 49 67 47 73 68 59 41 41 42 59 46 41 41 46 2F 36 7A 76 2F 41 41 6C 50 62 57 6C 75 62 33 56 7A 55 32 4E 76 63 6D 6C 75 5A 30 46 53 41 41 51 41 73 41 51 41 41 41 51 41 49 41 41 75 41 41 45 41 41 41 41 41 41 41 49 41 41 41 42 2F 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 4D 41 39 50 38 41 41 47 51 41 41 41 41 31 41 44 67 41 4F 51 41 46 41 41 3D 3D F7
('DATA', [0, 0, 22, 5, 0, 1, 127, 235, 59, 255, 0, 9, 79, 109, 105, 110, 111, 117, 115, 83, 99, 111, 114, 105, 110, 103, 65, 82, 0, 4, 0, 176, 4, 0, 0, 4, 0, 32, 0, 46, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3, 0, 244, 255, 0, 0, 100, 0, 0, 0, 53, 0, 56, 0, 57, 0, 5, 0])
# 0, data chunk number (0), total data chunks (22), data format (5 = patch dump), 0, bank number (1, 0-based, 1 = B), patch number (127, 0-based)

09:12:54.392	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 6C 50 71 68 35 78 63 41 41 42 59 3D F7
('DATA_ACK', [0, 0, 22])

...

09:12:54.393	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 155 bytes	F0 00 20 2B 00 6F 69 71 4B 77 6C 68 59 41 46 52 59 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 3D F7
('DATA', [0, 21, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])

09:12:54.393	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 67 42 77 55 30 42 63 41 46 52 59 3D F7
('DATA_ACK', [0, 21, 22])

09:12:54.488	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 32 78 63 4C 44 68 6F 41 F7
('DATA_COMPLETE', [0])

09:12:54.489	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 6D 69 59 51 46 78 73 41 F7
('DATA_COMPLETE_ACK', [0])
'''

# Mapping KnobKraft ORM program numbers (single, continuous numbers) to Hydrasynth patch/bank numbers and back.

# Map program numbers (0 - total number of pathes supported) into Hydrasynth bank and patch numbers.
def programToBankAndPatch(program):
    # KnobKraft ORM starts counting at 1, hydrasynth at 0.
    bank_number = (program) // PATCHES_PER_BANK
    patch_in_bank = (program) % PATCHES_PER_BANK

    return bank_number, patch_in_bank


# Map bank/patch numbers back to KnobKraft program numbers.
def bankAndPatchToProgram(bank, patch):
    return bank * PATCHES_PER_BANK + patch


# For Program Dump capability
# Create a patch dump request for the given patch number.
def createProgramDumpRequest(channel, program):  # We ignore the channel, it is apparently not used.
    bank, patch = programToBankAndPatch(program)

    # Create the patch dump message.
    result = to_hydrasynth('PATCH_REQUEST', [0, bank, patch])    
    logger.debug(f'Created patch dump request for program {program} (bank: {bank}, patch: {patch}): {to_hex_str(result)}')
    return result
    

def test_createProgramDumpRequest():
    result = createProgramDumpRequest(0, 0)  # patch 0, bank A
    result_str = to_hex_str(result)
    # DATA_REQUEST, [0, 0, 0]
    assert result_str == 'F0 00 20 2B 00 6F 74 4C 66 5A 55 51 51 41 41 41 41 3D F7'  # See above dump for patch 0, bank 0

    result = createProgramDumpRequest(0, 1)  # patch 1, bank A
    result_str = to_hex_str(result)
    # DATA_REQUEST, [0, 0, 1]
    assert result_str == 'F0 00 20 2B 00 6F 49 6F 66 65 4A 67 51 41 41 41 45 3D F7'  # See Midi_dump.txt

    result = createProgramDumpRequest(0, 128 + 127)  # patch 127 from bank B
    result_str = to_hex_str(result)
    # DATA_REQUEST, [0, 1, 127]
    assert result_str == 'F0 00 20 2B 00 6F 57 4F 70 34 69 41 51 41 41 58 38 3D F7'  # See Midi_dump.txt


# For Edit Buffer capability.
# Create an edit buffer request. We don't know yet if Hydrasynth supports an edit buffer, so we'll start first
# with a hard coded program number that we treat like an edit buffer.
def createEditBufferRequest(channel):    
    result = createProgramDumpRequest(channel, EDIT_BUFFER_PROGRAM_NUMBER)
    return result


# Split the given sequence of bytes into individual SysEx messages. Return a list of lists of individual SysExt messages.
# Everything outside of a SysEx Message is ignored.
def split_sysex_messages(m):
    result = []
    current_message = []
    in_message = False
    for b in m:
        if in_message:
            current_message.append(b)
            if b == SYSEX_END[0]:
                result.append(current_message)
                current_message = []
                in_message = False
        else:
            if b == SYSEX_START[0]:
                current_message.append(b)
                in_message = True

    return result


def test_split_sysex_messages():
    messages_str = ' '.join([
        'F0 00 20 2B 00 6F 57 58 55 39 50 42 67 41 F7',
        'F0 00 20 2B 00 6F 47 45 51 6D 4A 52 6B 41 F7',
        'F0 00 20 2B 00 6F 74 4C 66 5A 55 51 51 41 41 41 41 3D F7'
    ])

    messages_in = list(bytes.fromhex(messages_str))
    result = split_sysex_messages(messages_in)
    result_str = ' '.join([' '.join([f'{i:02X}' for i in m]) for m in result])
    assert result_str == messages_str


# Analyze a message and determine if it is a valid set of program dump messages.
# Returns status, last message received (0-based), total # of messages.
# Status is either False (Invalid) or True (valid).
# A message is only complete, if status is True and the last message received is one less than the total # of messages.
def verifyBufferMessage(message):
    messages = split_sysex_messages(message)
    if len(messages) == 0:
        # No messages so far.
        logger.warning('No messages found, expected at least one message.')
        return False, None, None

    # We got one or more messages. Parse them and figure out if they're complete, or which is next.
    parsed_messages = [from_hydrasynth(m) for m in messages]

    # Figure out total number of messages and verify that the given messages fit.
    current = None
    expected = None
    last = None
    for i, m in enumerate(parsed_messages):
        # We expect these to be all DATA chunks with information about the total # of DATA messages to expect.
        if m[0] != 'DATA':
            logger.warning(f'Message #{i} is not a DATA message. Aborting program dump verification.')
            return False, None, None

        data = m[1]
        if len(data) < 6:
            logger.warning(f'Message #{i} should contain at least 6 bytes, got: {len(data)} instead.')
            return False, None, None

        # Figure out how many messages we expect and verify the given messages.
        _, current, total = data[:3]
        if not expected:
            expected = total
            if expected != MESSAGES_PER_PATCH:
                logger.warning(f'Expected {MESSAGES_PER_PATCH} total messages per patch, but total is {expected} instead.')
                # Not failing here, because this might be a different Hydrasynth or some result of an update.
        elif len(parsed_messages) > expected:
            logger.warning(f'Expected {expected} messages, but got {len(parsed_messages)} instead.')
            return False, current, expected
        elif expected != total:
            logger.warning(f'Expected {expected} messages based on initial message, but message {i} indicates a total of {total} messages.')
            return False, current, expected

        # Number of messages vs. total are within expectations.
        # Verify if message sequence is consistent.
        if not last:
            last = current
        elif current - last != 1:
            logger.warning(f'Unexpected message sequence: Found message {current} after {last}.')
            return False, current, expected
        last = current

    # Everything looking good so far!
    return True, current, expected


def test_verifyBufferMessage():
    status, _, _ = verifyBufferMessage([])
    assert status == False  # Should return False for empty messages.

    messages = to_hydrasynth('BANK_SWITCH_ACK', [0, 0, 0])
    status, _, _ = verifyBufferMessage(messages)
    assert status == False  # Wrong command.

    messages = to_hydrasynth('DATA', [0, 0, 0, 0, 0])
    status, _, _ = verifyBufferMessage(messages)
    assert status == False  # Not enough data.

    total = 3
    messages = []
    for i in range(total + 1):
        messages += to_hydrasynth('DATA', [0, i, total, 0, 0, 0])
    status, _, _ = verifyBufferMessage(messages)
    assert status == False  # More messages than total.

    messages = []
    for i in range(total):
        messages += to_hydrasynth('DATA', [0, i, i, 0, 0, 0])
    status, _, _ = verifyBufferMessage(messages)
    assert status == False  # Total should never change.

    messages = []
    for i in range(total - 1, - 1, -1):  # Count down to total
        messages += to_hydrasynth('DATA', [0, i, total, 0, 0, 0])
    status, _, _ = verifyBufferMessage(messages)
    assert status == False  # Wrong sequence

    messages = []
    for i in range(total - 1):
        messages += to_hydrasynth('DATA', [0, i, total, 0, 0, 0])
    status, received, expected = verifyBufferMessage(messages)
    assert status == True
    assert received == total - 2  # We gave parts 0 .. total - 2.
    assert expected == total

    messages = []
    for i in range(total):
        messages += to_hydrasynth('DATA', [0, i, total, 0, 0, 0])
    status, received, expected = verifyBufferMessage(messages)
    assert status == True  # Should be recognized as complete and valid.
    assert received == total - 1
    assert expected == total


# For Program Dump capability.
# Test if the given set of MIDI SysEx messages contains a complete program dump.
# Also verifies that all dump messages contained in message are in time sequence order.
def isSingleProgramDump(message):
    status, last, total = verifyBufferMessage(message)
    logger.debug(f'isSingleProgramDump: message verification status: {status}, {last}/{total}.')
    result = (status == True and last == total - 1)  # Message is valid and we got all parts (0-based, hence total - 1).
    logger.debug(f'isSingleProgramDump: Returning {result}.')
    return result


# For Edit Buffer capability.)
def isEditBufferDump(message):
    # We're using a specific program as a fake edit buffer, therefore we're leveraring the 
    # related isSingleProgramDump() function here. However, we're not verifying if the
    # embedded bank/patch numbers are the correct ones for our fake edit buffer.
    return isSingleProgramDump(message)


# New as of: https://github.com/christofmuc/KnobKraft-orm/releases/tag/1.17.1
# Support Handshake/ACK messages in the Generic Adaptation. The isPartOfEditBufferDump() and
# isPartOfSingleProgramDump() methods can now optionally return a tuple of a bool and a list of
# MIDI bytes instead of just a bool. If they do, the MIDI bytes are sent to the synth in reply to
# the message received.
# 
# Implemented specifically as a way of returning ACK messages to the synth until we have all the
# pieces complete.
# 
# return createDataAckMessage(current, expected)

# Create a DATA_ACK message with the given parameters.
def createDataAckMessage(page, total_pages):
    return to_hydrasynth('DATA_ACK', [0, page, total_pages])


# For Edit Buffer capability.
def isPartOfEditBufferDump(message):
    status, last, total = verifyBufferMessage(message)
    if status == False:  # Invalid
        logger.debug('isPartOfEditBufferDump(): Returning False (invalid message).')
        return False

    # Message is valid. Is it complete?
    if last == total - 1:
        logger.debug('isPartOfEditBufferDump(): Returning True (message valid and complete).')
        return True
    else:
        ack_message = createDataAckMessage(last, total)
        logger.debug(f'isPartOfEditBufferDump(): Returning True, {to_hex_str(ack_message)} (ACK {last}/{total}, message valid but incomplete).')
        return True, ack_message


# For Program Dump capability.
# Does the same as Edit Buffer capability, since we implemented the edit buffer
# as a specific program dump.
def isPartOfSingleProgramDump(message):
    return isPartOfEditBufferDump(message)


# Sending edit/program buffers to Hydrasynth

'''
When sending a patch to Hydrasynth’s bank A, patch 0, we see:
09:06:18.218	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 57 58 55 39 50 42 67 41 F7
('BANK_SWITCH', [0])

09:06:18.276	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 47 45 51 6D 4A 52 6B 41 F7
('BANK_SWITCH_ACK', [0])

09:06:18.361	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 191 bytes	F0 00 20 2B 00 6F 41 50 4C 6D 30 52 59 41 41 42 59 47 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7
('DATA', [0, 0, 22, 6, 0, 0, 0, 230, 59, 0, 0, 13, 83, 97, 119, 112, 114, 101, 115, 115, 105, 118, 101, 32, 71, 68, 0, 32, 0, 32, 0, 176, 4, 0, 0, 4, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 115, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0]) 
# 0, data chunk number (0), total data chunks (22), data format (6 = patch dump send), 0, bank number (0, 0-based, 0 = A), patch number (0, 0-based)

09:06:18.363	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 6C 50 71 68 35 78 63 41 41 42 59 3D F7
('DATA_ACK', [0, 0, 22])

... until

09:06:18.411	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 155 bytes	F0 00 20 2B 00 6F 69 71 4B 77 6C 68 59 41 46 52 59 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 3D F7
('DATA', [0, 21, 22, ...])  # Last chunk of data (0-based).

09:06:18.414	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 57 68 68 73 51 77 63 41 41 41 41 3D F7
('PATCH_WRITE_REQUEST_RECEIVED', [0, 0, 0])  # 0, bank# (0), patch# (0)

09:06:18.414	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 67 42 77 55 30 42 63 41 46 52 59 3D F7
('DATA_ACK', [0, 21, 22])

09:06:18.480	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 56 54 71 49 6B 42 51 41 F7
('UPDATE_FLASH', [0])

09:06:29.838	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 46 41 75 54 69 52 55 41 F7
('UPDATE_FLASH_COMPLETE', [0])

09:06:29.910	From HYDRASYNTH DR	Control	1	Damper Pedal (Sustain)	0
09:06:30.023	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 32 78 63 4C 44 68 6F 41 F7
('DATA_COMPLETE', [0])

09:06:30.025	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 6D 69 59 51 46 78 73 41 F7
('DATA_COMPLETE_ACK', [0])


And when writing to patch 128 of bank B, we get:
09:15:47.823	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 57 58 55 39 50 42 67 41 F7
('BANK_SWITCH', [0])

09:15:47.865	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 47 45 51 6D 4A 52 6B 41 F7
('BANK_SWITCH_ACK', [0])

09:15:47.965	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 191 bytes	F0 00 20 2B 00 6F 4B 5A 63 47 35 68 59 41 41 42 59 47 41 41 46 2F 36 7A 76 2F 41 41 6C 50 62 57 6C 75 62 33 56 7A 55 32 4E 76 63 6D 6C 75 5A 30 46 53 41 41 51 41 73 41 51 41 41 41 51 41 49 41 41 75 41 41 45 41 41 41 41 41 41 41 49 41 41 41 42 2F 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 4D 41 39 50 38 41 41 47 51 41 41 41 41 31 41 44 67 41 4F 51 41 46 41 41 3D 3D F7
('DATA', [0, 0, 22, 6, 0, 1, 127, 235, 59, 255, 0, 9, 79, 109, 105, 110, 111, 117, 115, 83, 99, 111, 114, 105, 110, 103, 65, 82, 0, 4, 0, 176, 4, 0, 0, 4, 0, 32, 0, 46, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3, 0, 244, 255, 0, 0, 100, 0, 0, 0, 53, 0, 56, 0, 57, 0, 5, 0])
# 0, data chunk number (0), total data chunks (22), data format (6 = patch dump send), 0, bank number (1, 0-based, 1 = B), patch number (127, 0-based)

09:15:47.965	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 6C 50 71 68 35 78 63 41 41 42 59 3D F7
('DATA_ACK', [0, 0, 22])

09:15:47.983	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 155 bytes	F0 00 20 2B 00 6F 69 71 4B 77 6C 68 59 41 46 52 59 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 3D F7
('DATA', [0, 21, 22, ...])

09:15:47.984	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 74 6B 58 4E 6D 67 63 41 41 58 38 3D F7
('PATCH_WRITE_REQUEST_RECEIVED', [0, 1, 127])

09:15:47.984	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 19 bytes	F0 00 20 2B 00 6F 67 42 77 55 30 42 63 41 46 52 59 3D F7
('DATA_ACK', [0, 21, 22])

09:15:48.051	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 56 54 71 49 6B 42 51 41 F7
('UPDATE_FLASH', [0])

09:15:59.154	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 46 41 75 54 69 52 55 41 F7
('UPDATE_FLASH_COMPLETE', [0])

09:15:59.198	From HYDRASYNTH DR	Control	1	Damper Pedal (Sustain)	0
09:15:59.201	To HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 32 78 63 4C 44 68 6F 41 F7
('DATA_COMPLETE', [0])

09:15:59.202	From HYDRASYNTH DR	SysEx		Medeli Electronics Co 15 bytes	F0 00 20 2B 00 6F 6D 69 59 51 46 78 73 41 F7
('DATA_COMPLETE_ACK', [0])

This means:
* Writing a patch to Hydrasynth starts as a regular series of 22 DATA dump messages.
* The first data chunk starts with 6 (= this is a patch dump), the bank number (0-based) and the patch number (0-based). Then we see the actual patch data.
* After receiving the last chunk, Hydrasnyth confirms the patch dump write request with bank number and patch number with a PATCH_WRITE_REQUEST_RECEIVED message.
* Then we send an UPDATE_FLASH message to write the data buffer into Flash...
* ...and Hydrasynth confirms after the flash operation is complete.


'''

# Patch the given patch data messages to qualify as PATCH_WRITE_DATA requests and to specify the given
# bank and patch numbers.
def patchProgramDump(message, dump_type, bank, patch):
    # Split and parse messages for further processing.
    messages = split_sysex_messages(message)
    parsed_messages = [from_hydrasynth(m) for m in messages]
    
    # The first message should indicate the right data type and bank/patch numbers. This means we need to patch whatever the
    # original message said to match where we are supposed to write to.
    if isinstance(dump_type, str):
        dump_type_number = DATA_TYPES[dump_type]
    elif isinstance(dump_type, int):
        dump_type_number = dump_type
    else:
        raise ValueError('Invalid dump type.')
    parsed_messages[0][1][3] = dump_type_number  # 0 = first message, 1 = data bytes, 3 = position of data type.
    parsed_messages[0][1][5] = bank
    parsed_messages[0][1][6] = patch

    # Serialize all messages into a combined message to return to KnobKraft.
    result = []
    for m in parsed_messages:
        result += to_hydrasynth(m[0], m[1])

    return result


# (Part of program dump capability)
# Generate a series of messages that send the given message data into Hydrasynth as a series of messages that write the data into the given program number (mapped into bank/patch).
def convertToProgramDump(_, message, program):  # we ignore the first argument (channel)
    bank, patch = programToBankAndPatch(program)

    # Double-check consistency.
    if isSingleProgramDump(message) != True:
        logger.warning('Message did not pass isSingleProgramDump() consistency checking.')

    # Consistency checking done. Now assemble sequence of MIDI messages for writing our patch to the given bank/patch numbers.
    
    result = patchProgramDump(message, 'PATCH_WRITE_DATA', bank, patch)

    # Add message to trigger flash updates.
    result += to_hydrasynth('UPDATE_FLASH', [0])

    return result


def test_convertToProgramDump():
    message = []
    bank_old = 1
    patch_old = 2
    for i in range(MESSAGES_PER_PATCH):
        if i == 0:
            message += to_hydrasynth('DATA', [0, i, MESSAGES_PER_PATCH, DATA_TYPES['PATCH_DUMP'], 0, bank_old, patch_old, 1, 2, 3, 4, 5, 6])
        else:
            message += to_hydrasynth('DATA', [0, i, MESSAGES_PER_PATCH, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])

    bank_new = 3
    patch_new = 127
    program = bank_new * PATCHES_PER_BANK + patch_new
    result = convertToProgramDump(0, message, program)

    messages = split_sysex_messages(result)
    parsed_messages = [from_hydrasynth(m) for m in messages]
    first_message = parsed_messages[0]
    assert first_message[1][3] == DATA_TYPES['PATCH_WRITE_DATA']
    assert first_message[1][5] == bank_new
    assert first_message[1][6] == patch_new

    for i, m in enumerate(parsed_messages[:-1]):
        assert m[0] == 'DATA'
        assert m[1][1] == i
        assert m[1][2] == MESSAGES_PER_PATCH

    last_message = parsed_messages[-1]
    assert last_message[0] == 'UPDATE_FLASH'


# (part of edit buffer capability.)
# We're using a specific program (patch, bank) as a fake edit buffer.
# Unfortunately, this is suboptimal since this will trigger a Flash write cycle every
# time we change a patch. We haven't found a better way yet to just temporarily
# send a patch to the synth.
def convertToEditBuffer(channel, message):
    return convertToProgramDump(channel, message, EDIT_BUFFER_PROGRAM_NUMBER)


# Additional functions.

def numberFromDump(message):
    messages = split_sysex_messages(message)
    first_message = from_hydrasynth(messages[0])
    data = first_message[1]
    bank = data[5]
    patch = data[6]
    return bankAndPatchToProgram(bank, patch)


def test_numberFromDump():
    message = list(bytes.fromhex('F0 00 20 2B 00 6F 56 65 33 6D 68 52 59 41 41 42 59 46 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))
    assert numberFromDump(message) == 0  # Bank A, patch 0
    
    message = list(bytes.fromhex('F0 00 20 2B 00 6F 59 4F 68 62 4B 68 59 41 41 42 59 46 41 41 41 42 35 6A 73 42 41 41 31 48 57 43 42 56 62 48 52 79 59 56 42 68 5A 43 42 51 55 77 41 67 41 42 6B 41 73 41 51 41 41 41 51 41 49 41 41 56 41 41 41 41 41 41 41 44 41 41 49 41 41 77 42 71 41 41 41 41 41 41 41 51 41 45 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 45 41 41 77 41 41 41 41 41 41 5A 41 41 42 41 41 4D 41 44 41 43 6C 41 41 67 41 79 77 43 33 41 42 6F 41 61 51 41 42 41 41 4D 41 41 41 41 47 41 47 51 41 41 41 41 44 41 47 41 41 58 77 41 33 41 41 3D 3D F7'))
    assert numberFromDump(message) == 1  # Bank A, patch 1

    message = list(bytes.fromhex('F0 00 20 2B 00 6F 4B 35 43 33 75 68 59 41 41 42 59 46 41 41 45 41 7A 44 75 41 41 42 46 57 62 32 4E 76 55 33 52 79 61 57 35 6E 5A 58 49 67 49 46 4A 42 41 42 30 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 51 41 41 41 41 49 41 41 67 42 6D 41 41 41 41 41 41 41 51 41 45 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 4D 41 41 4D 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 44 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))
    assert numberFromDump(message) == 128  # Bank B, patch 0


# Extract name from the given dump.
def nameFromDump(message):
    messages = split_sysex_messages(message)
    first_message = from_hydrasynth(messages[0])
    data = first_message[1]
    name_buffer = data[12:28]  # Names are stores as 0-terminated strings here, max. 16 characters long.

    name = ''
    for c in name_buffer:
        if c == 0:
            break
        else:
            name += chr(c)

    return name


def test_nameFromDump():
    message = list(bytes.fromhex('F0 00 20 2B 00 6F 56 65 33 6D 68 52 59 41 41 42 59 46 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))
    assert nameFromDump(message) == 'Sawpressive GD'  # Bank A, patch 0 is 'Sawpressive GD'
    
    message = list(bytes.fromhex('F0 00 20 2B 00 6F 59 4F 68 62 4B 68 59 41 41 42 59 46 41 41 41 42 35 6A 73 42 41 41 31 48 57 43 42 56 62 48 52 79 59 56 42 68 5A 43 42 51 55 77 41 67 41 42 6B 41 73 41 51 41 41 41 51 41 49 41 41 56 41 41 41 41 41 41 41 44 41 41 49 41 41 77 42 71 41 41 41 41 41 41 41 51 41 45 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 45 41 41 77 41 41 41 41 41 41 5A 41 41 42 41 41 4D 41 44 41 43 6C 41 41 67 41 79 77 43 33 41 42 6F 41 61 51 41 42 41 41 4D 41 41 41 41 47 41 47 51 41 41 41 41 44 41 47 41 41 58 77 41 33 41 41 3D 3D F7'))
    assert nameFromDump(message) == 'GX UltraPad PS'  # Bank A, patch 1, maps to program 2

    message = list(bytes.fromhex('F0 00 20 2B 00 6F 4B 35 43 33 75 68 59 41 41 42 59 46 41 41 45 41 7A 44 75 41 41 42 46 57 62 32 4E 76 55 33 52 79 61 57 35 6E 5A 58 49 67 49 46 4A 42 41 42 30 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 51 41 41 41 41 49 41 41 67 42 6D 41 41 41 41 41 41 41 51 41 45 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 4D 41 41 4D 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 44 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))
    assert nameFromDump(message) == 'VocoStringer  RA'  # Bank B, patch 0, maps to program 129


def renamePatch(message, new_name):
    decoded_messages = [from_hydrasynth(m) for m in split_sysex_messages(message)]

    name_buffer = 17 * [0]  # 16 chars and a 0-byte for termination.
    for i, c in enumerate(new_name[:16]):
        name_buffer[i] = ord(c)

    for i, b in enumerate(name_buffer):
        decoded_messages[0][1][12+i] = b  # First message, second component (= data), name starts at pos 12.

    # Merge messages again into one.
    result = []
    for m in decoded_messages:
        result += to_hydrasynth(m[0], m[1])

    return result


def test_renamePatch():
    new_name = 'New Name'
    first_message = list(bytes.fromhex('F0 00 20 2B 00 6F 56 65 33 6D 68 52 59 41 41 42 59 46 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))
    message = 10 * first_message
    decoded_first_message_data = from_hydrasynth(first_message)[1]
    
    result = renamePatch(message, new_name)
    assert nameFromDump(result) == new_name

    # Ensure the first message remained intact around the changed name.
    decoded_messages = [from_hydrasynth(m) for m in split_sysex_messages(message)]
    for i, b in enumerate(decoded_messages[0][1]):
        if i < 12 or i > 28:  # Where the name is stored.
            assert b == decoded_first_message_data[i]

    # The rest should be unchanged.
    for m in decoded_messages[1:]:
        for i, b in enumerate(m[1]):
            assert b == decoded_first_message_data[i]
        

# Return the bank name the way the synth does it.
def friendlyBankName(bank_number):
    return bank_names[bank_number]


# Return the program name the way it is displayed on the synth.
def friendlyProgramName(program):
    logger.debug(f'friendlyProgramName: Generating friendly program name for: {program}')
    bank, patch = programToBankAndPatch(program)
    result = f'{friendlyBankName(bank)}{patch + 1:03d}'
    logger.debug(f'friendlyProgramName: Result: {result}')
    return result
    

def test_friendlyProgramName():
    assert friendlyProgramName(0) == 'A001'
    assert friendlyProgramName(127) == 'A128'
    assert friendlyProgramName(128) == 'B001'


# Hydrasynth expects a handshake protocol for sending patches to the synth.
# We're lazy here: Instead of implementing the handshake, we wait a "safe" time
# between messages, assuming that they have been received correctly.
def generalMessageDelay():
    return GENERAL_MESSAGE_DELAY_MILLIS


def calculateFingerprint(message):
    decoded_messages = [from_hydrasynth(m) for m in split_sysex_messages(message)]
    raw_data = []
    for m in decoded_messages:
        raw_data += m[1]

    # Fill bank and patch numbers with 0.
    raw_data[5] = 0
    raw_data[6] = 0

    # Fill the name part with zeros.
    for i in range(12, 28):
        raw_data[i] = 0

    # Now calculate a checksum.
    return hashlib.md5(bytearray(raw_data)).hexdigest()


# Just a burn-in test.
def test_calculateFingerpint():
    # Sawpressive GD
    sawpressive_gd = list(bytes.fromhex('F0 00 20 2B 00 6F 56 65 33 6D 68 52 59 41 41 42 59 46 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))

    result1 = calculateFingerprint(sawpressive_gd)
    assert isinstance(result1, str)

    # GX UltraPad PS
    gx_ultrapad_ps = list(bytes.fromhex('F0 00 20 2B 00 6F 59 4F 68 62 4B 68 59 41 41 42 59 46 41 41 41 42 35 6A 73 42 41 41 31 48 57 43 42 56 62 48 52 79 59 56 42 68 5A 43 42 51 55 77 41 67 41 42 6B 41 73 41 51 41 41 41 51 41 49 41 41 56 41 41 41 41 41 41 41 44 41 41 49 41 41 77 42 71 41 41 41 41 41 41 41 51 41 45 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 45 41 41 77 41 41 41 41 41 41 5A 41 41 42 41 41 4D 41 44 41 43 6C 41 41 67 41 79 77 43 33 41 42 6F 41 61 51 41 42 41 41 4D 41 41 41 41 47 41 47 51 41 41 41 41 44 41 47 41 41 58 77 41 33 41 41 3D 3D F7'))
    result2 = calculateFingerprint(gx_ultrapad_ps)
    assert isinstance(result2, str)
    assert result1 != result2

    sawpressive_gd_mod1 = renamePatch(sawpressive_gd, 'New name!')
    result3 = calculateFingerprint(sawpressive_gd_mod1)
    assert result3 == result1

    sawpressive_gd_mod2 = patchProgramDump(sawpressive_gd, 'PATCH_DUMP', 1, 42)  # Change bank and patch numbers.
    result4 = calculateFingerprint(sawpressive_gd_mod2)
    assert result4 == result1


def setupHelp():
    return HELP_TEXT


# Patch categories.

'''
Here's the beginning of a Patch dump for "Sawpressive GD", bank 0, patch 0, which is a Pad:

F0 00 20 2B 00 6F 41 50 4C 6D 30 52 59 41 41 42 59 47 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7
('DATA', [0, 0, 22, 5, 0, 0, 0, 230, 59, 0, 0, 13, 83, 97, 119, 112, 114, 101, 115, 115, 105, 118, 101, 32, 71, 68, 0, 32, 0, 32, 0, 176, 4, 0, 0, 4, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 115, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0])

And here's the exact patch at bank 0, patch 1, but categorized as 'Ambient':
F0 00 20 2B 00 6F 7A 38 49 33 53 52 59 41 41 42 59 46 41 41 41 42 6D 77 41 42 41 41 42 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7
('DATA', [0, 0, 22, 5, 0, 0, 1, 155, 00, 1, 0, 00, 83, 97, 119, 112, 114, 101, 115, 115, 105, 118, 101, 32, 71, 68, 0, 32, 0, 32, 0, 176, 4, 0, 0, 4, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 115, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 100, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0])
Changes:                     ^  ^^^  ^^  ^     ^^  S   a   w    p    r    e    s    s    i    v    e    ' ' G   D
                        patch’                 **

We speculate that ** is where the category is stored: 'Ambient' (2nd version) is the very first category available on Hydrasynth and maps nicely to 0.
The original patch is a "Pad", which is at #13 (0-based) in the list of Hydrasynth categories. Looks like a winner!
'''

# Extract category from the given dump.
def storedTags(message):
    first_message = from_hydrasynth(split_sysex_messages(message)[0])
    data = first_message[1]

    category_num = data[11]
    assert category_num < len(HYDRASYNTH_CATEGORIES)

    hs_category_name = HYDRASYNTH_CATEGORIES[category_num]
    return HYDRASYNTH_TO_KNOBKRAFT_CATEGORIES[hs_category_name]


def test_storedTags():
    sawpressive_gd_pad = list(bytes.fromhex('F0 00 20 2B 00 6F 56 65 33 6D 68 52 59 41 41 42 59 46 41 41 41 41 35 6A 73 41 41 41 31 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))
    result1 = storedTags(sawpressive_gd_pad)
    assert result1 == ['Pad']

    sawpressive_gd_ambient = list(bytes.fromhex('F0 00 20 2B 00 6F 7A 38 49 33 53 52 59 41 41 42 59 46 41 41 41 42 6D 77 41 42 41 41 42 54 59 58 64 77 63 6D 56 7A 63 32 6C 32 5A 53 42 48 52 41 41 67 41 43 41 41 73 41 51 41 41 41 51 41 49 41 41 41 41 41 41 41 41 41 41 41 41 41 49 41 41 51 42 7A 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 42 41 41 45 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 77 41 41 41 41 41 41 5A 41 41 41 41 41 4D 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 4D 41 41 41 41 41 41 47 51 41 41 41 41 44 41 41 41 41 41 41 41 41 41 41 3D 3D F7'))
    result2 = storedTags(sawpressive_gd_ambient)
    assert result2 == ['Ambient']
    