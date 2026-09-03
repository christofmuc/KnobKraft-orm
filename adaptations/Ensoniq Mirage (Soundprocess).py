#
#   KnobKraft ORM Adaptation for Ensoniq Mirage (SoundProcess OS)
#
#   SysEx structure derived from the Electra One template for this device.
#
#   This adaptation follows the alternate header used by the Electra One
#   SoundProcess template and community traces:
#       F0 00 00 23 01 ...
#   The scanned SoundProcess manual instead documents F0 0F 7F; this mismatch
#   still needs confirmation with real hardware.
#
#   Key commands (from Electra One template):
#       Computer Control ON:    F0 00 00 23 01 02 F7
#       Read Patch (request):   F0 00 00 23 01 04 NN F7   (NN = patch 1-48)
#       Write Patch (response): F0 00 00 23 01 44 NN [140 nybble bytes] F7
#
#   The SoundProcess manual specifies a 70-byte patch parameter block. MIDI
#   data is low-nybble/high-nybble encoded, producing 140 data bytes on the wire.
#
#   Message length: 1(F0) + 5(header/command) + 1(patch#) + 140(data) + 1(F7) = 148 bytes
#

import hashlib
import knobkraft
import testing


SYSEX_PREFIX = [0xF0, 0x00, 0x00, 0x23, 0x01]
COMPUTER_CONTROL = 0x02
READ_PATCH = 0x04
WRITE_PATCH = 0x44
PATCH_DATA_SIZE = 140
PATCHES_PER_BANK = 48


def name():
    return "Ensoniq Mirage (SoundProcess)"


def setupHelp():
    return (
        "The Ensoniq Mirage must be booted with the SoundProcess OS disk.\n\n"
        "Put the Mirage in Computer Control mode before use — the display should show 'CC'.\n"
        "This adaptation will automatically send the CC activation command before each patch "
        "request, but the Mirage may need a moment to switch modes.\n\n"
        "If patch retrieval is unreliable, add a small inter-message delay in your MIDI "
        "interface settings (50-100ms is usually sufficient).\n\n"
        "SoundProcess supports 48 patches. Bank dump is not supported by the OS, so patches "
        "are fetched one at a time.\n\n"
        "SoundProcess patch dumps include an eight-character patch name."
    )


def messageTimings():
    return {
        "generalMessageDelay": 300,
        "deviceDetectWaitMilliseconds": 3000,
        "replyTimeoutMs": 3000,
    }


def createDeviceDetectMessage(channel):
    # This runs when KnobKraft is looking for the synth.
    # We send CC ON (0x02) + a small "trash" request (0x04) to clear the buffer.
    cc_command = SYSEX_PREFIX + [COMPUTER_CONTROL, 0xF7]
    read_patch = SYSEX_PREFIX + [READ_PATCH, 0x01, 0xF7]
    return cc_command + read_patch


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if isSingleProgramDump(message):
        return 0
    return -1
    
# ---------------------------------------------------------------------------
# Edit buffer
# SoundProcess has no dedicated edit buffer. Patch 1 is used as a stand-in.
# ---------------------------------------------------------------------------

def createEditBufferRequest(channel):
    return createProgramDumpRequest(channel, 0)


def convertToEditBuffer(channel, message):
    # SoundProcess has no volatile edit buffer; patch slot 1 is the stand-in.
    return convertToProgramDump(channel, message, 0)


def isEditBufferDump(message):
    return isSingleProgramDump(message) and message[6] == 1


# ---------------------------------------------------------------------------
# Single program dump
# ---------------------------------------------------------------------------

def createProgramDumpRequest(channel, program_number):
    patch_num = (program_number % 48) + 1
    cc_command = SYSEX_PREFIX + [COMPUTER_CONTROL, 0xF7]
    read_patch = SYSEX_PREFIX + [READ_PATCH, patch_num, 0xF7]
    return cc_command + read_patch


def isSingleProgramDump(message):
    if not hasattr(message, '__len__'):
        return False
    return (
        len(message) == 148
        and list(message[:5]) == SYSEX_PREFIX
        and message[5] == WRITE_PATCH
        and 1 <= message[6] <= PATCHES_PER_BANK
        and all(value <= 0x0F for value in message[7:147])
        and message[147] == 0xF7
    )


def convertToProgramDump(channel, message, program_number):
    if not isSingleProgramDump(message):
        raise ValueError("Can only convert SoundProcess single program dumps")
    converted = list(message)
    converted[6] = (program_number % PATCHES_PER_BANK) + 1
    return converted

def calculateFingerprint(message):
    if isSingleProgramDump(message):
        data = message[7:147]
        return hashlib.md5(bytearray(data)).hexdigest()
    return ""

def numberFromDump(message):
    # The Trace shows the patch number is at index 6.
    # Mirage/SoundProcess uses 1-48, KnobKraft needs 0-47.
    if isSingleProgramDump(message):
        return message[6] - 1
    return -1

def nameFromDump(message):
    if isSingleProgramDump(message):
        payload = message[7:147]
        raw_patch = [payload[index] | (payload[index + 1] << 4)
                     for index in range(0, PATCH_DATA_SIZE, 2)]
        return ''.join(chr(value) if 32 <= value < 127 else ' '
                       for value in raw_patch[62:70]).rstrip()
    return "Unknown"


# ---------------------------------------------------------------------------
# Layout
# ---------------------------------------------------------------------------

def numberOfBanks():
    return 48 // PATCHES_PER_BANK

def numberOfPatchesPerBank():
    return PATCHES_PER_BANK

def createBankDumpRequest(channel, bank):
    cc_command = SYSEX_PREFIX + [COMPUTER_CONTROL, 0xF7]
    messages = cc_command
    start = bank * PATCHES_PER_BANK + 1
    for patch_num in range(start, start + PATCHES_PER_BANK):
        messages += SYSEX_PREFIX + [READ_PATCH, patch_num, 0xF7]
    return messages
    
def isPartOfBankDump(message):
    if not hasattr(message, '__len__'):
        return False
    return isSingleProgramDump(message)

def isBankDumpFinished(messages):
    try:
        return sum(1 for m in messages if isSingleProgramDump(m)) >= PATCHES_PER_BANK
    except TypeError:
        return False
        
def extractPatchesFromBank(messages):
    if not messages:
        return []
    split_messages = knobkraft.splitSysex(messages) if isinstance(messages[0], int) else messages
    return [value for message in split_messages if isSingleProgramDump(message) for value in message]


def extractPatchesFromAllBankMessages(messages):
    return [message for message in messages if isSingleProgramDump(message)]
    
def friendlyBankName(bank_number):
    return "Bank {}".format(bank_number + 1)


def make_test_data():
    # Synthetic protocol fixture based on the manual's 70-byte parameter block.
    raw_patch = list(range(70))
    raw_patch[62:70] = b"SYNTHETC"
    payload = [nibble for value in raw_patch for nibble in (value & 0x0F, value >> 4)]
    program = SYSEX_PREFIX + [WRITE_PATCH, 1] + payload + [0xF7]

    def programs(_data: testing.TestData):
        yield testing.ProgramTestData(
            message=program,
            name="SYNTHETC",
            number=0,
            target_no=11,
            change_number_changes_name=True,
        )

    def edit_buffers(_data: testing.TestData):
        yield testing.ProgramTestData(message=program, name="SYNTHETC", number=0)

    return testing.TestData(
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        program_dump_request=(0, 0, "F0 00 00 23 01 02 F7 F0 00 00 23 01 04 01 F7"),
        device_detect_call="F0 00 00 23 01 02 F7 F0 00 00 23 01 04 01 F7",
        device_detect_reply=(program, 0),
    )
