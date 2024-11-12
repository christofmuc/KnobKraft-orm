# Line 6 POD Adaptation for Knobkraft-ORM.
# Only verified with a Line 6 POD 1.0 device. Should be easily enhanced to support other POD devices.

# References:
# POD Midi / Sysex Specification and Notes
# https://www.midimanuals.com/manuals/line_6/pod/sysex/pod_sysex_english.pdf
#
# KnobKraft Orm Adaptation Programming Guide
# https://github.com/christofmuc/KnobKraft-orm/blob/master/adaptations/Adaptation%20Programming%20Guide.md

from typing import Dict
from enum import Enum

##############################################################################
# SysEx FORMAT

# UNIVERSAL DEVICE INQUIRY:
#
# POD will respond to the universal system inquiry command if the channel received is
# the same as POD's MIDI channel, the channel received is 7F (all channels), or POD is
# set to omni mode. The received message is in the following format:
#
# F0                SOX
# 7E <chan> 06 01   System inquiry message
# F7                EOX
#
# If <chan> = 7F (Universal All Device Call) POD will respond with the channel also set
# to 7F.
#
# POD's reply to Universal Device Inquiry
#
# F0                SOX
# 7E <chan> 06 02   Universal Device Inquiry Response
# 00 01 0C          Line 6 (Fast Forward) Manufacturer ID
# 00 00             0x0000 = POD Product Family ID (LSB first)
# 00 01             0x0100 = POD Product Family Member (LSB first)
# xx xx xx xx       Software revision, ASCII (ex. 30 31 30 30  = '0100' = 1.00)
# F7                EOX

# DATA DUMP FORMAT:
#
# POD sends and receives Program and Global dump data in High-Low Nibbilized format.
# Data Locations in the dump are described later in this document with reference to ONE
# POD Byte.
#
# ONE POD BYTE (8 bits):
# 0: A7 A6 A5 A4 A3 A2 A1 A0
#
# TRANSMITTED and RECEIVED AS:
# 0: 00 00 00 00 A7 A6 A5 A4
# 1: 00 00 00 00 A3 A2 A1 A0
#
# e.g. 0xA5 (10100101) -> 0x0A 00001010, 0x05 00000101

# SYSTEM EXCLUSIVE FORMAT:
#
#  POD's System Exclusive message format is as follows:
#
#  F0               SOX
#  00 01 0C         Line 6 (Fast Forward) Manufacturer ID
#  01               POD ID
#  xx               Opcode
#  yy...            Data
#  F7               EOX

# SYSTEM EXCLUSIVE OPCODES:
#
# 00 SYSEX DATA DUMP REQUEST (Sent to device):
#   Type (in Data field):
#   00: Program Patch Dump Request
#       0xF0 0x00 0x01 0x0C 0x01 0x00 0x00 <program #> 0xF7
#           <program #> = 0x00 ~ 0x23 (1A ~ 9D internal programs)
#       POD responds with Program Dump (01 00)
#   01: Program Edit Buffer Dump Request
#       0xF0 0x00 0x01 0x0C 0x01 0x00 0x01 0xF7
#       POD responds with Program Edit Buffer Dump (01 01)
#   02: All Programs Dump Request
#       0xF0 0x00 0x01 0x0C 0x01 0x00 0x02 0xF7
#       POD responds by sending an All Program Dump (01 02)


class PodDataDumpFormat(Enum):
    INVALID = 0
    PROGRAM_PATCH = 1
    PROGRAM_EDIT_BUFFER = 2
    ALL_PROGRAMS = 3


# 01 SYSEX DATA DUMP (Returned from device):
#   Type:
#   00: Program Patch Dump
#       0xF0 0x00 0x01 0x0C 0x01 0x01 0x00 <program #> <version> <data...> 0xF7
#           <program #> = 0x00 ~ 0x23 (1A ~ 9D internal programs)
#           <version> = 0x00 ~ 0x7F
#           <data> = 144 bytes nibbilized (71 actual data bytes)
#   01: Program Edit Buffer Dump
#       0xF0 0x00 0x01 0x0C 0x01 0x01 0x01 <<version> data> 0xF7
#           <version> = 0x00 ~ 0x7F
#           <data> = 1 Program = 144 bytes nibbilized (71 actual data bytes)
#   02: All Programs Dump
#       0xF0 0x00 0x01 0x0C 0x01 0x01 0x02 <version> <data...> 0xF7
#           <version> = 0x00 ~ 0x7F
#           <data> = All Programs = 5184 bytes nibbilized (2556 actual data bytes)

# See "pod_sysex_english.pdf" for version data and program data formats.

###############################################################################
# KnobKraft Exports

#
# Identity
#


def name() -> str:
    return "Line 6 POD"


#
# Storage size
#


def bankDescriptors() -> list[Dict]:
    return [{"bank": 0, "name": "Internal Patches", "size": 9 * 4, "type": "Patch"}]


#
#
# Device detection
#


def createDeviceDetectMessage(channel: int) -> list[int]:
    """Message to force reply by device.

    Args:
        channel (int): Unused?

    Returns:
        list[int]: Single MIDI message or multiple MIDI messages in the form of a single list of byte-values integers used to detect the device.
    """
    return [
        0xF0,  # SOX
        0x7E,
        0x7F,  # 7F (Universal All Device Call)
        0x06,
        0x01,
        0xF7,  # EOX
    ]


def channelIfValidDeviceResponse(message: list[int]) -> int:
    """Check if reply came

    Args:
        message (list[int]): SysEx message

    Returns:
        _type_: -1 if the message handed in is not the device response we had been expecting,
        or a valid MIDI channel 0..15 indicating which channel the device is currently configured for.
    """
    if message[0] != 0xF0 or message[-1] != 0xF7:
        print("Not SYSEX")

    if message[5 : 7 + 1] != [0x00, 0x01, 0x0C]:
        print(f"Not Line 6 (Fast Forward) Manufacturer ID. Found {message[5:7+1]}")
        return -1

    if message[1] != 0x7E or message[3] != 0x06 or message[4] != 0x02:
        print("Not Universal Device Inquiry Response")
        return -1

    channel = message[2]
    if channel == 0x7F:
        print("POD set to OMNI.")
        channel = 0

    pod_id = message[9] << 8 | message[8]
    if pod_id != 0x0000:
        print(f"WARNING: Pod ID {pod_id} is untested.")

    pod_member = message[11] << 8 | message[10]
    if pod_member != 0x0100:
        print(f"WARNING: Pod member {pod_member} is untested.")

    version = (
        "".join([chr(x) for x in message[12 : 13 + 1]])
        + "."
        + "".join([chr(x) for x in message[14 : 15 + 1]])
    )

    print(
        f"Found POD: channel {channel}; id {pod_id}; member {pod_member}; version {version}"
    )

    return channel


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
    # print(f"createEditBufferRequest. channel {channel}")

    #   01: Program Edit Buffer Dump Request
    #       0xF0 0x00 0x01 0x0C 0x01 0x00 0x01 0xF7
    #       POD responds with Program Edit Buffer Dump (01 01)

    return [
        0xF0,  # SOX
        0x00,  # Line 6 (Fast Forward) Manufacturer ID
        0x01,  # "
        0x0C,  # "
        0x01,  # POD ID
        0x00,  # Opcode
        0x01,  # Data
        0xF7,  # EOX
    ]


def isEditBufferDump(message: list[int]) -> bool:
    """Check if a generic MIDI message is an edit buffer dump

    Args:
        message (list[int]): generic MIDI message

    Returns:
        bool: true if edit buffer dump
    """

    # print("isEditBufferDump")
    return getPodDataDumpFormat(message) == PodDataDumpFormat.PROGRAM_EDIT_BUFFER


def convertToEditBuffer(channel: int, message: list[int]):
    """Called when a patch is selected in the UI, in order  to send to edit buffer in keyboard.

    Args:
        channel (int): _description_
        messages (list[int]): _description_

    Raises:
        Exception: _description_
    """
    # print(f"convertToEditBuffer. channel {channel}; {byteListToHexString(message)}")

    format = getPodDataDumpFormat(message)

    if format == PodDataDumpFormat.PROGRAM_EDIT_BUFFER:
        # Already a Program Edit Buffer Dump
        return message

    if format != PodDataDumpFormat.PROGRAM_PATCH:
        # Not supported
        raise Exception("Expected PROGRAM_PATCH")

    # We're converting from:
    #
    # 00: Program Patch Dump
    # 0xF0 0x00 0x01 0x0C 0x01 0x01 0x00 <program #> <version> <data...> 0xF7
    #                               ----
    #
    # to:
    #
    # 01: Program Edit Buffer Dump
    # 0xF0 0x00 0x01 0x0C 0x01 0x01 0x01 <version> <data...> 0xF7
    #                               ----

    return message[0 : 5 + 1] + [0x01] + message[8:]


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

    Returns:
        list[int]: specific patch from the synths memory
    """
    # print(f"createProgramDumpRequest. channel {channel}; patch {patchNo}")

    #   00: Program Patch Dump Request
    #       0xF0 0x00 0x01 0x0C 0x01 0x00 0x00 <program #> 0xF7
    #           <program #> = 0x00 ~ 0x23 (1A ~ 9D internal programs)
    #       POD responds with Program Dump (01 00)

    # 00: Program Patch Dump Request
    return [
        0xF0,  # SOX
        0x00,  # Line 6 (Fast Forward) Manufacturer ID
        0x01,  # "
        0x0C,  # "
        0x01,  # POD ID
        0x00,  # Opcode
        0x00,  # Data
        patchNo,  # 0x00 - 0x23 (1A - 9D internal programs)
        0xF7,  # EOX
    ]


def isPartOfSingleProgramDump(message: list[int]) -> bool:
    # print("isPartOfSingleProgramDump")
    return getPodDataDumpFormat(message) == PodDataDumpFormat.PROGRAM_PATCH


def isSingleProgramDump(message: list[int]) -> bool:
    """Check if a generic MIDI message is a single program dump

    Args:
        message (list[int]): generic MIDI message

    Returns:
        bool: true if single program dump
    """
    format = getPodDataDumpFormat(message)
    # Anything but PodDataDumpFormat.PROGRAM_EDIT_BUFFER
    return format in [
        PodDataDumpFormat.PROGRAM_EDIT_BUFFER,
        PodDataDumpFormat.PROGRAM_PATCH,
    ]


def numberFromDump(message: list[int]) -> int:
    # print(f"numberFromDump. {byteListToHexString(message)}")

    format = getPodDataDumpFormat(message)
    if format == PodDataDumpFormat.PROGRAM_PATCH:
        #  <program #> = 0x00 ~ 0x23 (1A ~ 9D internal programs)
        return message[7]
    elif format == PodDataDumpFormat.PROGRAM_EDIT_BUFFER:
        return 0

    raise Exception("numberFromDump Expecting PROGRAM_PATCH")


def convertToProgramDump(
    channel: int, message: list[int], program_number: int
) -> list[int]:
    # Not currently used in KnobKraft.
    raise Exception("convertToProgramDump NYI")


#
# Better bank names
#


def friendlyBankName(bank_number: int) -> str:
    print(f"friendlyBankName. bank_number {bank_number}")
    return chr(ord("A") + bank_number)


#
# Better program number names
#


def friendlyProgramName(program: int) -> str:
    # print(f"friendlyProgramName. program {program}")
    numberOfPatchesPerBank = 4
    bank = program // numberOfPatchesPerBank
    program = program % numberOfPatchesPerBank
    # e.g. "9D"
    return f"{bank+1}{chr(ord('A') + program)}"


#
# Better duplicate detection
#


def nameFromDump(message: list[int]) -> str:
    data = getDenibbilizedData(message)
    # See "PROGRAM DATA" in "pod_sysex_english.pdf".
    return "".join([chr(x) for x in data[55 : 70 + 1]])


#
# Leaving helpful setup information specific for a synth
#


def setupHelp():
    return "Line 6 POD Adaptation"


###############################################################################
# POD specific helpers (internal)


def getPodDataDumpFormat(message: list[int]) -> PodDataDumpFormat:
    # print(f"getPodDataDumpFormat. {byteListToHexString(message)}")

    if len(message) < 8:
        print("Message too small")
        return PodDataDumpFormat.INVALID

    if message[0] != 0xF0 or message[-1] != 0xF7:
        print("Not SYSEX")
        return PodDataDumpFormat.INVALID

    if message[1 : 3 + 1] != [0x00, 0x01, 0x0C]:
        print(f"Not Line 6 (Fast Forward) Manufacturer ID. Found {message[1:3+1]}")
        return PodDataDumpFormat.INVALID

    if message[4] != 0x01:
        print(f"Unexpected POD ID, {message[4]}")
        return PodDataDumpFormat.INVALID

    # Expecting 01 SYSEX DATA DUMP
    if message[5] != 0x01:
        print(f"Unexpected sysex data dump, got {message[4]}")
        return PodDataDumpFormat.INVALID

    data_len = len(message) - 7

    type = message[6]
    if type == 0:
        # 00: Program Patch Dump
        if data_len != 145:
            print(f"Program Patch Dump invalid len, got {data_len}")
            return PodDataDumpFormat.INVALID
        # print("-> PROGRAM_PATCH")
        return PodDataDumpFormat.PROGRAM_PATCH
    elif type == 1:
        # 01: Program Edit Buffer Dump
        if data_len != 144:
            print(f"Program Edit Buffer Dump invalid len, got {data_len}")
            return PodDataDumpFormat.INVALID
        # print("-> PROGRAM_EDIT_BUFFER")
        return PodDataDumpFormat.PROGRAM_EDIT_BUFFER
    elif type == 2:
        # 02: All Programs Dump
        if data_len != 5184:
            print(f"All Programs Dump invalid len, got {data_len}")
            return PodDataDumpFormat.INVALID
        # print("-> ALL_PROGRAMS")
        return PodDataDumpFormat.ALL_PROGRAMS
    else:
        print(f"Invalid type {type}")
        return PodDataDumpFormat.INVALID


def getDenibbilizedData(message: list[int]) -> list[int]:
    format = getPodDataDumpFormat(message)

    if format == PodDataDumpFormat.PROGRAM_PATCH:
        # 0xF0 0x00 0x01 0x0C 0x01 0x01 0x00 <program #> <version> <data...> 0xF7
        data = message[9:-1]
    elif format == PodDataDumpFormat.PROGRAM_EDIT_BUFFER:
        # 0xF0 0x00 0x01 0x0C 0x01 0x01 0x01 <version> <data...> 0xF7
        data = message[8:-1]
    else:
        #  Not implemented: PodDataDumpFormat.ALL_PROGRAMS
        raise Exception("Unexpected data dump format")

    # See "DATA DUMP FORMAT" note.
    res = []
    for i in range(0, len(data) - 1, 2):
        res.append((data[i] & 0b1111) << 4 | (data[i + 1] & 0b1111))

    return res


###############################################################################
# Helper methods (internal)


def byteListToHexString(data: list[int]) -> str:
    return " ".join("{:02x}".format(x) for x in data)
