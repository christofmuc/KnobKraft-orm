# References:
#
# Yamaha YC61 Owners Manual (see pg. 56-69)
# https://usa.yamaha.com/files/download/other_assets/4/1311174/yc61_en_om_a0.pdf
#
# SoundMondo reface-panel JS
# https://soundmondo.yamahasynth.com/components/reface-panel/reface-panel.js
# * function sendSystemCommonRequest
# * function recvBulkDumpMsg
#
# THE STRUCTURE OF A MIDI EVENT:
# http://www.petesqbsite.com/sections/express/issue18/midifilespart1.html
#
# YAMAHA Keyboard - MIDI - Messages
# http://www.jososoft.dk/yamaha/articles/midi_10.htm
#
# KnobKraft Orm Adaptation Programming Guide
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptations/Adaptation%20Programming%20Guide.md
#
# KnobKraft Orm Adaptation Testing Guide
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptations/Adaptation%20Testing%20Guide.md

# MIDI-OX
# http://www.midiox.com/
#
# Options: MIDI Devices, set in and out for "YC Series"
# Options: [X] Pass SysEx
# View > SysEx...
# In SysEx View and Scratchpad, enter SysEx to send, e.g. "Bulk Header: Current Sound Buffer"
# F0 43 20 7f 1c 09 0e 7f 00 F7
# Display > Send/Receive SysEx, Click "Done"

# To Test:
#
# cd ~\KnobKraft-Adaptations
# python.exe -m venv venvdir
# .\venvdir\Scripts\Activate.ps1
# pip.exe install -r (Join-Path (scoop prefix knobkraftorm) 'testing\requirements.txt')
# cd adaptations
# python -m pytest test_adaptations.py --adaptation YamahaYC61.py
# (Note: test_adaptations.py is only on repo)

# Some quality sounds:
#
# YC61/73/88 Sounds by Blake Angelos
# https://soundmondo.yamahasynth.com/voices?reface=YC61&user=8
#
# YC61/73/88 Sounds by Katsunori UJIIE / 氏家克典
# https://soundmondo.yamahasynth.com/voices?reface=YC61&user=16054

# Note: There is an identity request which could also be used to detect the device:
#
# (3-4-1-1) IDENTITY REQUEST
# F0 7E 0n 06 01 F7 (“n” = Device No. However, this instrument receives under “omni.”)
#
# (3-4-1-2) IDENTITY REPLY
# F0 7E 7F 06 02 43 00 41 dd dd mm 00 00 7F F7H
# dd: Device family number/code (YC61: 5CH 06H)
# mm=tenth of a version, starting at version 1.0
# example: mm=05, version=1 + (5/10) = version 1.5

from typing import List, Tuple, Dict
import hashlib

##############################################################################
# SysEx FORMAT

# Bulk Dump/Request
# =================
# Request: F0 43 2n <gh gl> id <ah am al> F7
#
# Dump:
# [+00] F0
# [+01] 43
# [+02] 0n
# [+03] <gh gl> (== 7f 1c)
# [+05] <bh bl> (==len(dt)+4)
# [+07] id (== 9)
# [+08] <ah am al>
# [+0B] dt ...
# [-2 ] cc
# [-1 ] F7
#
# F0: Exclusive status
# 43: YAMAHA ID
# n: Device Number
# gh: Group Number High (7F)
# gl: Group Number Low (1C)
#   bh: Byte Count High (see MIDI Data Table)
#   bl: Byte Count Low (see MIDI Data Table)
# id: Model ID (09 = YC61/YC73/YC88)
# ah: Parameter Address High (see MIDI Data Table)
# am: Parameter Address Middle (see MIDI Data Table)
# al: Parameter Address Low (see MIDI Data Table)
#   dt: Data
#   cc: Data Checksum
# f7: End of Exclusive

# Parameter Base Address (ah am al)
# =================================
# System:
# Common:                   20 00 00
# MEQ (Master EQ):          20 40 00
# Soundmondo Format Vers:   00 7f 00
#
# Bulk Control:
# Header:                   0E 00 00
# Footer:                   0F 00 00
#
# Store to Flash:
# Store to Flash:           0D 00 00
#
# Live Set Sound Zone:
# Common:                   46 00 00
# Zone 1-4 (zz: 00-03):     4a zz 00
#
# Organ Section:
# Common:                   50 00 00
# Part (p: 0=UPR;1=LWR):    50 1p 00
#
# Keys Sections:
# Section (s: 0=A; 1=B):    60 0s 00

# MIDI Parameter Change Table (BULK CONTROL) (ah am al)
# ==========================================================
# 0e pp 0n Bulk Header: Live Set Sound User (pp=0-19; n=0-7)
# 0e 7f 00 Bulk Header: Current Sound Buffer
# 0f pp 0n Bulk Footer: Live Set Sound User (pp=0-19; n=0-7)
# 0f 7f 00 Bulk Footer: Current Sound Buffer

# Bulk Dump Block (ah am al)
# ===================================
# [SYSTEM PARAMETER BLOCK]
# NOTE: **DOC IS INCORRECT**  Doc says System SIZE = 0x30
# System Common             20 00 00    SIZE = 0x31
# Master EQ (MEQ)           20 40 00    SIZE = 0x14
#
# [LIVE SET SOUND PARAMETER BLOCK]
# Bulk Header               0e pp 0n    SIZE = 0
# Soundmondo Format Version 00 7f 00    SIZE = 4
# NOTE: **DOC IS INCORRECT** Doc says Common SIZE = 1
# Common                    46 00 00    SIZE = 0x48
# Zone 1:                   4a 00 00    SIZE = 0x10
# Zone 2:                   4a 01 00    SIZE = 0x10
# Zone 3:                   4a 02 00    SIZE = 0x10
# Zone 4:                   4a 03 00    SIZE = 0x10
# Organ Section Common:     50 00 00    SIZE = 0x24
# Organ Section Part Upper: 50 10 00    SIZE = 0x14
# Organ Section Part Lower: 50 11 00    SIZE = 0x14
# Keys A Section:           60 00 00    SIZE = 0x3a
# Keys B Section:           60 01 00    SIZE = 0x3a
# Bulk Footer               0f pp 0n    SIZE = 0

###############################################################################
# KnobKraft Exports

#
# Required Functions (Identity, Storage Size, Device Detection)
#


def name() -> str:
    return "Yamaha YC61/YC73/YC88"


def bankDescriptors() -> list[Dict]:
    # Yamaha YC has 20 banks each with 8 sounds, numbered 1-1 through 20-8.
    return [
        {
            "bank": x,
            "name": f"Bank {x + 1}",
            "size": 8,
            "type": "Patch",
            "isROM": False,
        }
        for x in range(20)
    ]


def createDeviceDetectMessage(channel: int) -> list[int]:
    """Message to force reply by device.

    Args:
        channel (int): Unused?

    Returns:
        list[int]: Single MIDI message or multiple MIDI messages in the form of a single list of byte-values integers used to detect the device.
    """

    # 9 = YC61/YC73/YC88
    # [20 00 00] = System Common
    return makeYamahaDumpRequestMessage(channel, 9, 0x20, 0x00, 0x00)


def channelIfValidDeviceResponse(message: list[int]) -> int:
    """Check if reply came

    Args:
        message (list[int]): SysEx message

    Returns:
        _type_: -1 if the message handed in is not the device response we had been expecting,
        or a valid MIDI channel 0..15 indicating which channel the device is currently configured for.
    """

    # Bulk Dump: F0, 43, 0n, gh, gl, bh, bl, id, ah, am, al, dt, ..., cc, F7

    if not isYamahaSysExMessage(message):
        return -1

    model_id = message[7]
    if message[7] != 0x09:  # Model ID (09 = YC)
        return -1

    # byte_count is {id, ah, am, al, data...}, or len(data)+4
    # TODO: Is this << 7 ("7-in-8 encoding") or << 8?
    byte_count = message[5] << 8 | message[6]

    # ah, am, am: SYSTEM COMMON (20 00 00)
    address = message[8 : 8 + 3]
    if address != [0x20, 0x00, 0x00]:
        print("Not SYSTEM COMMON. Found {message[8:8+3]}")
        return -1

    data_offset = 0x0B
    data = message[data_offset:-2]
    checksum = message[-2]

    # Checksum is the value that results in a value of 0 for the lower 7 bits when
    # the Model ID, Start Address, Data and Checksum itself are added.
    # NOTE: **DOC IS INCORRECT** It states "the Byte Count, Start Address, ..."
    vals = [model_id] + address + data + [checksum]
    computed_checksum = sum(vals)
    if (computed_checksum & 0x7F) != 0:
        print(f"Invalid checksum {computed_checksum}")
        return -1

    tx_channel = data[0x0A]
    rx_channel = data[0x0B]
    print(f"tx_channel {tx_channel}; rx_channel {rx_channel}")
    return tx_channel


def needsChannelSpecificDetection() -> bool:
    """Specifying to not to do channel specific detection.

    Returns:
        bool: True if the createDeviceDetectMessage() should be called once for each of the 16 possible MIDI channels and MIDI outputs
    """
    return False


def deviceDetectWaitMilliseconds() -> int:
    """Time the main program will wait for the synth to answer before it moves on testing the next MIDI output.

    Returns:
        int: Number of milliseconds
    """
    return 100


#
# Edit Buffer Capability
#
# Used to retrieve the Edit Buffer and to send a patch into the edit buffer for
# audition.
# Also, if no other capabilities are implemented and the synth reacts on program
# change messages, it will be used by the Librarian to retrieve, one by one,
# all patches from the synth.
#


def createEditBufferRequest(channel: int) -> list[int]:
    """Requests the edit buffer from the synth.

    Args:
        channel (int): the channel detected

    Returns:
        list[int]: single MIDI message that makes the device send its Edit Buffer
    """
    # print(f"createEditBufferRequest called. channel {channel}")

    # MIDI PARAMETER CHANGE TABLE (BULK CONTROL)
    # 9 = YC61/YC73/YC88
    # [0e 7f 00] = Current Sound Buffer
    return makeYamahaDumpRequestMessage(channel, 9, 0x0E, 0x7F, 0x00)


def isPartOfEditBufferDump(message: list[int]) -> bool:
    """Handle edit buffer dumps that consist of more than one MIDI message.

    Args:
        message (list[int]): SysEx message

    Returns:
        bool: true if message presented should be part of the messages parameter
        to the isEditBufferDump() message.
        In turn, the isEditBufferDump() should return only true if it has enough
        messages to complete the full edit buffer.
    """
    # Expected to receive:
    #
    # Bulk Header                   0e 7f 00    SIZE = 0  (Note this differs from bulk sound header)
    # Soundmondo Format Version     00 7f 00    SIZE = 4
    # Live Set Sound Zone Common    46 00 00    SIZE = 72
    # Zone 1:                       4a 00 00    SIZE = 16
    # Zone 2:                       4a 01 00    SIZE = 16
    # Zone 3:                       4a 02 00    SIZE = 16
    # Zone 4:                       4a 03 00    SIZE = 16
    # Organ Section Common:         50 00 00    SIZE = 36
    # Organ Section Part Upper:     50 10 00    SIZE = 20
    # Organ Section Part Lower:     50 11 00    SIZE = 20
    # Keys A Section:               60 00 00    SIZE = 58
    # Keys B Section:               60 01 00    SIZE = 58
    # Bulk Footer                   0f 7f 00    SIZE = 0  (Note this differs from bulk sound footer)

    if not isYamahaSysExMessage(message):
        return False

    address = message[8 : 8 + 3]

    # Bulk Header
    if address == [0x0E, 0x7F, 00]:
        return True

    # Soundmondo Format Version
    if address == [0x00, 0x7F, 0x00]:
        return True

    # Live Set Sound Zone Common
    if address == [0x46, 0x00, 0x00]:
        return True

    # Zone 1-4
    if address[0] == 0x4A and 0 <= address[1] <= 3 and address[2] == 0x00:
        return True

    # Organ Section
    if address[0] == 0x50 and address[1] in [0x00, 0x10, 0x11] and address[2] == 0x00:
        return True

    # Keys Section
    if address[0] == 0x60 and 0x00 <= address[1] <= 0x01 and address[2] == 0x00:
        return True

    # Bulk Footer
    if address == [0x0F, 0x7F, 0x00]:
        return True

    # dumpYamahaSysex("not isPartOfEditBufferDump", message)
    return False


def isEditBufferDump(messages: list[int]) -> bool:
    """Check if a generic MIDI message is an edit buffer dump

    Args:
        messages (list[int]): generic MIDI message

    Returns:
        bool: true if edit buffer dump
    """
    # print(f"isEditBufferDump called. messages {byteListToHexString(messages)}")

    # Split the messages parameter into individual SysEx messages
    # they've been combined with calls to isPartOfEditBufferDump
    split_messages = splitSysexMessage(messages)
    addresses = getYamahaSysexMessageAddresses(split_messages)
    header = addresses[0]

    # print(f"isEditBufferDump addresses {listListToHexString(addresses)}")

    # All of the expected addresses must exist to be complete.
    if addresses == [
        [0x0E, 0x7F, 0x00],  # 0e 7f 00 Bulk Header: Current Sound Buffer
        [0x00, 0x7F, 0x00],
        [0x46, 0x00, 0x00],
        [0x4A, 0x00, 0x00],
        [0x4A, 0x01, 0x00],
        [0x4A, 0x02, 0x00],
        [0x4A, 0x03, 0x00],
        [0x50, 0x00, 0x00],
        [0x50, 0x10, 0x00],
        [0x50, 0x11, 0x00],
        [0x60, 0x00, 0x00],
        [0x60, 0x01, 0x00],
        [0x0F, 0x7F, 0x00],  # 0f 7f 00 Bulk Footer: Current Sound Buffer
    ]:
        return True

    # print("not isEditBufferDump")
    return False


def convertToEditBuffer(channel: int, messages: list[int]):
    """Called when a patch is selected in the UI, in order  to send to edit buffer in keyboard.

    Args:
        channel (int): _description_
        messages (list[int]): _description_

    Raises:
        Exception: _description_
    """
    split_messages = splitSysexMessage(messages)
    # print(f"convertToEditBuffer called. split_messages {listListToHexString(split_messages)}")

    result = []

    if isEditBufferDump(messages):
        split_messages = splitSysexMessage(messages)

        # If edit buffer, then just recompose the messages with desired channel.
        for message in split_messages:
            # [+02] 0n (n: Device Number)
            message[0x02] = channel & 0xF
            # No need to rebuild checksum as device number isn't part of the calculation.
            result.extend(message)
    elif isSingleProgramDump(messages):
        # Single program dump, replace "live sound" header [0e pp 0n] and footer [0f pp 0n]
        # to "current sound buffer" header [0e 7f 00] and footer. [0f 7f 00].
        # [+08] <ah am al>
        for message in split_messages:
            address = message[8 : 8 + 3]
            if address[0] in [0x0E, 0x0F]:
                message[0x08 : 0x08 + 3] = [address[0], 0x7F, 0x00]
            # Need to rebuild checksum for each message, as address was modified.
            message[-2] = ((sum(message[0x07:-2]) & 0x7F) ^ 0x7F) + 1
            result.extend(message)
    else:
        raise Exception("convertToEditBuffer - unxpected messages")

    return result


#
# Program Dump Capability
#
# Used instead of the Edit Buffer Capability in enumerating the patches in the synth for download.
#


def createProgramDumpRequest(channel: int, patchNo: int) -> list[int]:
    """Requests a specific program at a specific memory position

    Args:
        channel (int): channel the synth was detected at
        patchNo (int): KnobKraft Orm version linearly counted

        YC-series has 20 Live Set Banks x 8 Patches Each = 160 locations,
        so patchNo=152 indicates bank 20, patch 1: round(152/8)=19; mod(152;8)=0

    Returns:
        list[int]: specific patch from the synths memory
    """
    pp = patchNo // 8
    n = patchNo % 8

    # 9 = YC61/YC73/YC88
    # [0e pp 0n] = Bulk Header: Live Set Sound User (pp=0-19; n=0-7)
    return makeYamahaDumpRequestMessage(channel, 9, 0x0E, pp, n)


def isPartOfSingleProgramDump(message: list[int]) -> bool:
    # Expected to receive:
    #
    # Bulk Header                   0e pp 0n    SIZE = 0  (pp=0-19; n=0-7)
    # Soundmondo Format Version     00 7f 00    SIZE = 4
    # Live Set Sound Zone Common    46 00 00    SIZE = 72
    # Zone 1:                       4a 00 00    SIZE = 16
    # Zone 2:                       4a 01 00    SIZE = 16
    # Zone 3:                       4a 02 00    SIZE = 16
    # Zone 4:                       4a 03 00    SIZE = 16
    # Organ Section Common:         50 00 00    SIZE = 36
    # Organ Section Part Upper:     50 10 00    SIZE = 20
    # Organ Section Part Lower:     50 11 00    SIZE = 20
    # Keys A Section:               60 00 00    SIZE = 58
    # Keys B Section:               60 01 00    SIZE = 58
    # Bulk Footer                   0f pp 0n    SIZE = 0  (pp=0-19; n=0-7)
    if not isYamahaSysExMessage(message):
        return False

    if not isYamahaSysExMessage(message):
        return False

    address = message[8 : 8 + 3]

    # Voice Header
    if address[0] == 0x0E and 0 <= address[1] <= 19 and 0 <= address[2] <= 7:
        return True

    # Soundmondo Format Version
    if address == [0x00, 0x7F, 0x00]:
        return True

    # Live Set Sound Zone Common
    if address == [0x46, 0x00, 0x00]:
        return True

    # Zone 1-4
    if address[0] == 0x4A and 0 <= address[1] <= 3 and address[2] == 0x00:
        return True

    # Organ Section
    if address[0] == 0x50 and address[1] in [0x00, 0x10, 0x11] and address[2] == 0x00:
        return True

    # Keys Section
    if address[0] == 0x60 and 0x00 <= address[1] <= 0x01 and address[2] == 0x00:
        return True

    # Voice Footer
    if address[0] == 0x0F and 0 <= address[1] <= 19 and 0 <= address[2] <= 7:
        return True

    # dumpYamahaSysex("not isPartOfSingleProgramDump", message)
    return False


def isSingleProgramDump(messages: list[int]) -> bool:
    """Check if a generic MIDI message is a single program dump

    Args:
        messages (list[int]): generic MIDI message

    Returns:
        bool: true if single program dump
    """

    # Split the messages parameter into individual SysEx messages
    # they've been combined with calls to isPartOfEditBufferDump
    split_messages = splitSysexMessage(messages)
    addresses = getYamahaSysexMessageAddresses(split_messages)

    # print(f"isSingleProgramDump addresses {listListToHexString(addresses)}")

    # Sound Header and Footer are variable, so can't statically check.
    header = addresses[0]
    if not (header[0] == 0x0E and 0 <= header[1] <= 19 and 0 <= header[2] <= 7):
        return False

    footer = addresses[-1]
    if not (footer[0] == 0x0F and 0 <= footer[1] <= 19 and 0 <= footer[2] <= 7):
        return False

    # All of the expected addresses must exist to be complete.
    if addresses == [
        header,  # 0e pp 0n Bulk Header: Live Set Sound User (pp=0-19; n=0-7)
        [0x00, 0x7F, 0x00],
        [0x46, 0x00, 0x00],
        [0x4A, 0x00, 0x00],
        [0x4A, 0x01, 0x00],
        [0x4A, 0x02, 0x00],
        [0x4A, 0x03, 0x00],
        [0x50, 0x00, 0x00],
        [0x50, 0x10, 0x00],
        [0x50, 0x11, 0x00],
        [0x60, 0x00, 0x00],
        [0x60, 0x01, 0x00],
        footer,  # 0f pp 0n Bulk Footer: Live Set Sound User (pp=0-19; n=0-7)
    ]:
        return True

    return False


def numberFromDump(messages: list[int]) -> int:
    if isEditBufferDump(messages):
        # Singular edit buffer, so just return 0.
        return 0

    split_messages = splitSysexMessage(messages)

    # Not edit buffer, assume it's a patch.
    # Bank and patch is in "Bulk Header: Live Set Sound User (pp=0-19; n=0-7)" [0e pp 0n]:
    address = split_messages[0][8 : 8 + 3]
    pp = address[1]
    n = address[2]
    if address[0] == 0x0E and 0 <= pp <= 19 and 0 <= n <= 7:
        return pp * 8 + n

    raise Exception("numberFromDump - unexpected sysex")


def convertToProgramDump(
    channel: int, message: list[int], program_number: int
) -> list[int]:
    # Not currently used in KnobKraft.
    raise Exception(f"convertToProgramDump NYI")


#
# Bank Dump Capability
#
# Some synths do not work with individual MIDI messages per patch, or even
# multiple MIDI messages for one patch, but rather with one big MIDI message
# which contains all patches of a bank.
#

# def createBankDumpRequest(channel: int, bank: int):
# def isPartOfBankDump(message: list[int]) -> bool:
# def isBankDumpFinished(messages: list[list[int]]) -> bool:
# def extractPatchesFromBank(messages):

#
# Testing
#

# def make_test_data():
#     def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
#     def program_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:

###############################################################################
# Patch/Bank/Program names


def calculateFingerprint(messages: list[int]) -> str:
    split_messages = splitSysexMessage(messages)

    # split_messages[2] has "Live Set Sound Zone Common" [46 00 00]
    address = split_messages[2][8 : 8 + 3]

    if address == [0x46, 0x00, 0x00]:
        data_offset = 0x0B
        # Make name all spaces so that the same patch that's been renamed will be seen as a duplicate.
        for i in range(15):
            split_messages[2][data_offset + i] = ord(
                " "
            )  # 0 isn't a valid name character, so use space.

    joined_messages = []
    for message in split_messages:
        joined_messages.extend(message)

    return hashlib.md5(bytearray(joined_messages)).hexdigest()


def nameFromDump(messages: list[int]) -> str:
    split_messages = splitSysexMessage(messages)
    # print(f"nameFromDump called. split_messages {listListToHexString(split_messages)}")

    # split_messages[2] has "Live Set Sound Zone Common" [46 00 00]
    address = split_messages[2][8 : 8 + 3]

    if address == [0x46, 0x00, 0x00]:
        # First 15 bytes of data ASCII name (not null terminated)
        data_offset = 0x0B
        return "".join(
            [chr(x) for x in split_messages[2][data_offset + 0 : data_offset + 0 + 15]]
        )

    raise Exception("numberFromDump: live set sound zone common not found")


# def isDefaultName(patchName: str) -> bool:
# def renamePatch(messages: list[list[int]], new_name):


# TODO: Not called by Knobkraft?
# def friendlyBankName(bank_number: int) -> str:
#     print(f'friendlyBankName called. bank_number {bank_number}')
#     return f'{bank_number+1}'


def friendlyProgramName(program: int) -> str:
    numberOfPatchesPerBank = 8
    bank = program // numberOfPatchesPerBank
    program = program % numberOfPatchesPerBank
    #  Use 1-based index for UI
    # print(f"friendlyProgramName called. program {program} -> bank {bank+1} program {program+1}")
    return f"{bank+1}-{program+1}"


###############################################################################
# Helper methods (internal)


def byteListToHexString(data: list[int]) -> str:
    return " ".join("{:02x}".format(x) for x in data)


def listListToHexString(data: list[list[int]]) -> str:
    ret = ""
    for lst in data:
        ret += "["
        ret += " ".join(list(map(lambda v: "%02x" % v, lst)))
        ret += "] "
    return ret


#
# SysEx helpers
#


def isSysExMessage(message: list[int]) -> bool:
    # SysEx_EXCLUSIVE_STATUS, SysEx_END_OF_EXCLUSIVE
    return message[0] == 0xF0 and message[-1] == 0xF7


def splitSysexMessage(messages: list[int]) -> list[list[int]]:
    """Extract SysEx Messsages from concatenated list

    Example:

    messages = [0xf0, 1, 2, 3, 0xf7,   0xf0, 4, 5, 0xf7]

    returns  [ [0xf0, 1, 2, 3, 0xf7], [0xf0, 4, 5, 0xf7] ]

    Args:
        messages (list[int]): One or more SysEx messages concatenated.

    Returns:
        list[list[int]]: One or more SysEx messages split.
    """
    result = []
    start = 0
    read = 0
    while read < len(messages):
        if messages[read] == 0xF0:
            start = read
        elif messages[read] == 0xF7:
            result.append(messages[start : read + 1])
        read = read + 1
    return result


def isRealtimeSysExMessage(message: list[int]) -> bool:
    """Check if message is a Realtime SysEx mesages

        Realtime messages are not associated with any one MIDI channel.
        They can appear in the MIDI data stream at any time.
        These messages consist of a single status byte; they have no data bytes.
            0xF8: Clock
            0xF9: (undefined)
            0xFA: Start
            0xFB: Continue
            0xFC: Stop
            0xFD: (undefined)
            0xFE: Active Sensing
            0xFF: System Reset

        For Yamaha YC, we're specifically interested in detecting:
        (3-3-1) ACTIVE SENSING; STATUS 11111110(FEH); Transmitted every 200 msec.

    Args:
        message (list[int]): SysEx message

    Returns:
        bool: true if realtime message.
    """
    return message[0] >= 0xF8


#
# Yamaha-specific SysEx helpers
#


def dumpYamahaSysex(prefix: str, message: list[int]):
    if isRealtimeSysExMessage(message):
        print("(RealtimeSysExMessage)")
        return

    if not isYamahaSysExMessage(message):
        raise Exception(f"Not Yamaha SysEx message {byteListToHexString(message)}")

    # byte_count is {id, ah, am, al, data...}, or len(data) + 4
    data_byte_count = (message[5] << 8 | message[6]) - 4
    address = message[8 : 8 + 3]
    data_offset = 0x0B
    data = message[data_offset : data_offset + data_byte_count]

    print(
        f"{prefix}: '{YamahaYcSysExAddressToString(address)}' {byteListToHexString(address)}"
    )
    print(f"data ({len(data)}):", byteListToHexString(data))


def isYamahaSysExMessage(message: list[int]) -> bool:
    if not isSysExMessage(message):
        return False

    # Note: message[2] is device number (0n)
    # Note: message[5,6] is byte count (bh, bl)
    if (
        len(message) < 8
        or message[1] != 0x43  # Yamaha specific SysEx identifier
        or message[3] != 0x7F  # Yamaha group number high
        or message[4] != 0x1C  # Yamaha group number low
    ):
        return False

    return True


def getYamahaSysexMessageAddresses(messages: list[list[int]]) -> list[list[int]]:
    """Return list of Yamaha SysEx addresses found in SysEx

    Args:
        messages (list[list[int]]): Lists of SysEx messages (likely from splitSysExMessage)
    """
    result = []
    for message in messages:
        if isYamahaSysExMessage(message):
            result += [message[8 : 8 + 3]]
    return result


def makeYamahaDumpRequestMessage(
    device_number: int, model_id: int, address_h: int, address_m: int, address_l: int
) -> list[int]:
    # (3-4-5) DUMP REQUEST
    # Bulk Dump Request: F0, 43, 2n, gh, gl, id, ah, am, al, F7
    return [
        0xF0,  # Exclusive status
        0x43,  # YAMAHA ID
        0x20 | device_number,  # Device Number
        0x7F,  # Group ID High
        0x1C,  # Group ID Low
        model_id,
        address_h,  # Address High
        address_m,  # Address Mid
        address_l,  # Address Low
        0xF7,  # End of Exclusive
    ]


#
# Yamaha YC series-specific SysEx helpers
#


def YamahaYcSysExAddressToString(address: list[int]) -> str:
    # 0e pp 0n Bulk Header: Live Set Sound User (pp=0-19; n=0-7)
    if address[0] == 0x0E:
        return f"Bulk Header (pp={address[1]}; n={address[2] & 0xf})"
    if address[0] == 0x0F:
        return f"Bulk Footer (pp={address[1]}; n={address[2] & 0xf})"

    # 4a zz 00 Zone 1-4 (zz: 00-03)
    if address[0] == 0x4A:
        return f"Zone {address[1]+1}"

    match address:
        case [0x00, 0x7F, 0x00]:
            return "Soundmondo Format Version"
        case [0x46, 0x00, 0x00]:
            return "Common"
        case [0x50, 0x00, 0x00]:
            return "Organ Section Common"
        case [0x50, 0x10, 0x00]:
            return "Organ Section Part Lower"
        case [0x50, 0x11, 0x00]:
            return "Organ Section Part Upper"
        case [0x60, 0x00, 0x00]:
            return "Key A Section"
        case [0x60, 0x01, 0x00]:
            return "Key B Section"
        case [0x0F, 0x7F, 0x00]:
            return "Bulk Footer"
        case _:
            raise Exception(f"Unknown address {address}")
