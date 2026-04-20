#
#   KnobKraft ORM Adaptation for Ensoniq Mirage (SoundProcess OS)
#
#   SysEx structure derived from the Electra One template for this device.
#
#   All SysEx messages use the Ensoniq 3-byte manufacturer ID + device ID header:
#       F0 00 00 23 01 ...
#   (0x00 0x00 0x23 = Ensoniq manufacturer ID, 0x01 = device ID)
#
#   Key commands (from Electra One template):
#       Computer Control ON:    F0 00 00 23 01 02 F7
#       Read Patch (request):   F0 00 00 23 01 04 NN F7   (NN = patch 1-48)
#       Write Patch (response): F0 00 00 23 01 44 NN [124 nybble bytes] F7
#
#   The 124 nybble bytes encode 62 raw parameter bytes (low nybble first, then
#   high nybble), as confirmed by the Electra One response rules (byte indices 0-123).
#
#   Message length: 1(F0) + 5(header) + 1(patch#) + 124(data) + 1(F7) = 132 bytes
#

import hashlib


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
        "Note: The SoundProcess patch dump does not include a patch name, so patches will be "
        "identified by their slot number."
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
    cc_command = [0xF0, 0x00, 0x00, 0x23, 0x01, 0x02, 0xF7]
    read_patch = [0xF0, 0x00, 0x00, 0x23, 0x01, 0x04, 0x01, 0xF7]
    return cc_command + read_patch


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # During Auto-Detect, ONLY return 0 if we see the full 148-byte data.
    # If we see the 8-byte ACK (0x00), return -1 to stay on the line.
    if len(message) == 148 and message[0] == 0xF0 and message[5] == 0x44:
        return 0
    return -1
    
# ---------------------------------------------------------------------------
# Edit buffer
# SoundProcess has no dedicated edit buffer. Patch 1 is used as a stand-in.
# ---------------------------------------------------------------------------

def createEditBufferRequest(channel):
    return createProgramDumpRequest(channel, 0)




def convertToEditBuffer(channel, message):
    # There is no edit buffer on the Mirage — sending a Write Patch command
    # always targets the slot number embedded in the message header.
    # We prepend the CC activation command to ensure the Mirage is ready.
    if isSingleProgramDump(message):
        cc_command = [0xF0, 0x00, 0x00, 0x23, 0x01, 0x02, 0xF7]
        return cc_command + list(message)
    return list(message)


# ---------------------------------------------------------------------------
# Single program dump
# ---------------------------------------------------------------------------

def createProgramDumpRequest(channel, program_number):
    patch_num = (program_number % 48) + 1
    cc_command = [0xF0, 0x00, 0x00, 0x23, 0x01, 0x02, 0xF7]
    read_patch = [0xF0, 0x00, 0x00, 0x23, 0x01, 0x04, patch_num, 0xF7]
    return cc_command + read_patch


def isSingleProgramDump(message):
    if not hasattr(message, '__len__'):
        return False
    return (len(message) == 148 and message[0] == 0xF0 and message[5] == 0x44)

def calculateFingerprint(message):
    # Hash the 140 bytes of actual parameter data (Index 7 to 146)
    if len(message) == 148:
        data = message[7:147]
        return hashlib.md5(bytearray(data)).hexdigest()
    return ""

def numberFromDump(message):
    # The Trace shows the patch number is at index 6.
    # Mirage/SoundProcess uses 1-48, KnobKraft needs 0-47.
    if len(message) == 148:
        return message[6] - 1
    return -1

def nameFromDump(message):
    if len(message) == 148:
        return "Patch {}".format(message[6])
    return "Unknown"


# ---------------------------------------------------------------------------
# Layout
# ---------------------------------------------------------------------------

PATCHES_PER_BANK = 48

def numberOfBanks():
    return 48 // PATCHES_PER_BANK

def numberOfPatchesPerBank():
    return PATCHES_PER_BANK

def createBankDumpRequest(channel, bank):
    cc_command = [0xF0, 0x00, 0x00, 0x23, 0x01, 0x02, 0xF7]
    messages = cc_command
    start = bank * PATCHES_PER_BANK + 1
    for patch_num in range(start, start + PATCHES_PER_BANK):
        messages += [0xF0, 0x00, 0x00, 0x23, 0x01, 0x04, patch_num, 0xF7]
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
    return messages
    
def friendlyBankName(bank_number):
    return "Bank {}".format(bank_number + 1)