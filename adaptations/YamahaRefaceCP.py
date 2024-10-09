# Note: There is an identity request which could also be used to detect the device:
#
#### (3-4-1-1) IDENTITY REQUEST (Receive only)
#
# F0 7E 0n 06 01 F7
# ("n" = Device No. However, this instrument receives under "omni.")
#
# (3-4-1-2) IDENTITY REPLY (Transmit only)
#
# F0 7E 7F 06 02 43 00 41 52 06 00 00 00 7F F7
#                         dd dd mm
# dd: Device family number/code (Reface CP: 52H 06H)
# mm=tenth of a version, starting at version 1.0
# example: mm=05, version=1 + (5/10) = version 1.5
# (undocumented in Reface CP manual, but YC series lists this info)

##############################################################################
# SysEx FORMAT

# Note that this keyboard has no patches!
# Only an edit ("TG" or "Tone Generator") buffer!

# Bulk Dump/Request
# =================
# (3-4-4) DUMP REQUEST (sent)
# F0 43 2n <gh gl> id <ah am al> F7
#
# (3-4-3) BULK DUMP (received)
# [+00] F0
# [+01] 43
# [+02] 0n
# [+03] <gh gl> (== 7f 1c)
# [+05] <bh bl> (==len(dt)+4)
# [+07] id
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
# id: Model ID (04 = Reface CP)
# ah: Parameter Address High (see MIDI Data Table)
# am: Parameter Address Middle (see MIDI Data Table)
# al: Parameter Address Low (see MIDI Data Table)
#   dt: Data
#   cc: Data Checksum
# f7: End of Exclusive

# Parameter Base Address (ah am al)
# =================================
# System:       00 00 00
# TG:           30 00 00

# Bulk Dump Block (ah am al)
# ===================================
# [SYSTEM PARAMETER BLOCK]
# Common        00 00 00    SIZE = 0x24
# [TG PARAMETER BLOCK]
# Bulk Header   0e 0f 00    SIZE = 4
# TG Common     30 00 00    SIZE = 20
# Bulk Footer   0f 0f 00    SIZE = 4

# MIDI PARAMETER CHANGE TABLE (SYSTEM)
# (Data format of the block that's returned from a SYSTEM dump request)
# =====================================================================
# 00    00-0F,7F MIDI transmit channel          1-16, off
# 01    00-0F,7F MIDI receive channel           1-16, All (Omni)
# 02    00-00      Master Tune                     -102.4 – +102.3 (cent)
# 03    00-07
# 04    00-0F
# 05    00-0F
# 06    00-01   Local Control                   off,ON
# 07    34-4C   Master Transpose                -12 - +12 (semitones)
# 0B    00-01   Sustain Pedal Select            FC3, FC4/5
# 0C    00-01   Auto Power-Off                  off,ON
# 0D    00-01   Speaker Output                  off,ON
# 0E    00-01   MIDI Control                    off,ON

# MIDI PARAMETER CHANGE TABLE (Tone Generator)
# (data format of the block that's returned from a TG dump request)
# =================================================================
# 00    00-7F   Volume                          0-127
# 02    00-05   Wave Type (TYPE)                Rd I, Rd II, Wr, Clv, Toy, CP
# 03    00-7F   Drive (DRIVE)                   0-127
# 04    00-02   Effect 1 Type (TREMOLO/WAH)     thru (middle position), tremolo (TREMOLO), wah (WAH)
# 05    00-7F   Effect 1 Depth (DEPTH)          0-127
# 06    00-7F   Effect 1 Rate (RATE)            0-127
# 07    00-02   Effect 2 Type (CHORUS/PHASER)   thru (middle position), chorus (CHORUS), phaser (PHASER)
# 08    00-7F   Effect 2 Depth (DEPTH)          0-127
# 09    00-7F   Effect 2 Speed (SPEED)          0-127
# 0A    00-02   Effect 3 Type (D.DELAY/A.DELAY) thru (middle position), Digital Delay (D.DELAY), Analog Delay (A.DELAY)
# 0B    00-7F   Effect 3 Depth (DEPTH)          0-127
# 0C    00-7F   Effect 3 Rate (TIME)            0-127
# 0D    00-7F   Reverb Depth (REVERB DEPTH)     0-127
# Total Size = 16. All other fields are reserved for future use.

# References:
#
# reface CS/DX/CP/YC Data List
# https://usa.yamaha.com/files/download/other_assets/7/794817/reface_en_dl_b0.pdf

# Some quality sounds:
#
# reface CP Sounds by Blake Angelos:
# https://soundmondo.yamahasynth.com/voices?reface=CP&user=8
# reface CP Sounds by Katsunori UJIIE / 氏家克典
# https://soundmondo.yamahasynth.com/voices?reface=CP&user=16054


import hashlib
from typing import Dict

###############################################################################
# KnobKraft Exports


def name() -> str:
    """Identity.

    Returns:
        string: Name of the device, uniquely identifying it.
    """
    return "Yamaha Reface CP"


def bankDescriptors() -> list[Dict]:
    # Just a single edit buffer!
    return [
        {
            "bank": 0,
            "name": "Tone Generator",
            "size": 1,
            "type": "Patch",
            "isROM": False,
        }
    ]


def createDeviceDetectMessage(channel: int) -> list[int]:
    """Message to force reply by device.

    Args:
        channel (_type_): Unused?

    Returns:
        _type_: Single MIDI message or multiple MIDI messages in the form of a single list of byte-values integers used to detect the device.
    """
    # Model ID 04 = Reface CP
    # System [00 00 00]
    return makeYamahaDumpRequestMessage(channel, 4, [0x00, 0x00, 0x00])


def channelIfValidDeviceResponse(message: list[int]) -> int:
    """Check if reply came.

    Args:
        message (_type_): _description_

    Returns:
        int: -1, if the message handed in is not the device response we had been expecting, or valid MIDI channel [0..15]
    """
    if not isYamahaSysExMessage(message):
        return -1

    # Bulk Dump: F0, 43, 0n, gh, gl, bh, bl, id, ah, am, al, dt, ..., cc, F7
    # Data format as described in "MIDI PARAMETER CHANGE TABLE (SYSTEM)"

    model_id = message[7]
    if model_id != 0x04:  # Model ID (04 = Reface CP)
        return -1

    address = message[8 : 8 + 3]
    if address != [0x00, 0x00, 0x00]:
        print("Not SYSTEM COMMON. Found {message[8:8+3]}")
        return -1

    data_offset = 0x0B
    data = message[data_offset:-2]
    checksum = message[-2]

    # Checksum is the value that results in a value of 0 for the lower 7 bits when
    # the Model ID, Start Address, Data and Checksum itself are added.
    vals = [model_id] + address + data + [checksum]
    computed_checksum = sum(vals)
    if (computed_checksum & 0x7F) != 0:
        print(f"Invalid checksum {computed_checksum}")
        return -1

    tx_channel = data[0x00]
    rx_channel = data[0x01]

    print(f"tx_channel {tx_channel}; rx_channel {rx_channel}")

    if tx_channel == 0x10:
        print("Warning: Device is set to transmit on OMNI MIDI channel.")
        tx_channel = 1

    return tx_channel


def needsChannelSpecificDetection() -> bool:
    """Specifying to not to do channel specific detection.

    Returns:
        bool: True if the createDeviceDetectMessage() should be called once for each of the 16 possible MIDI channels and MIDI outputs
    """
    return True


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


def createEditBufferRequest(channel):
    """Requests the edit buffer from the synth.

    Args:
        channel (_type_): the channel detected

    Returns:
        _type_: single MIDI message that makes the device send its Edit Buffer
    """
    # Model ID 04 = Reface CP
    # 0e 0f 00: TG Bulk Header
    return makeYamahaDumpRequestMessage(channel, 4, [0x0E, 0x0F, 0x00])


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
    if not isYamahaSysExMessage(message):
        return False

    address = message[8 : 8 + 3]

    # createEditBufferRequest sent [0e 0f 00] to request TG Bulk Dump Block.
    #
    # Expected to receive:
    #
    # Bulk Header [0e 0f 00]
    # TG Common [30 00 00]
    # Bulk Footer [0f 0f 00]
    if address in [[0x0E, 0x0F, 0x00], [0x30, 0x00, 0x00], [0x0F, 0x0F, 0x00]]:
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

    # All of the expected addresses must exist to be complete:
    # Bulk Header [0e 0f 00]
    # TG Common [30 00 00]
    # Bulk Footer [0f 0f 00]
    if addresses == [[0x0E, 0x0F, 0x00], [0x30, 0x00, 0x00], [0x0F, 0x0F, 0x00]]:
        return True

    # print("not isEditBufferDump")
    return False


def convertToEditBuffer(channel: int, messages: list[int]):
    """Send edit buffer dump, to not overwrite the synths patch memory.

    Args:
        channel (_type_): the channel detected
        message (_type_): patch stored in the database

    Returns:
        _type_: MIDI message or multiple MIDI messages (all in a single list of integers)
    """
    # The Reface CP only has an edit buffer, so the only thing to do is
    # recompose the messages with desired channel.
    split_messages = splitSysexMessage(messages)
    result = []
    for message in split_messages:
        # [+02] 0n (n: Device Number)
        message[0x02] = channel & 0xF
        # No need to rebuild checksum as device number isn't part of the calculation.
        result.extend(message)
    return result


def nameFromDump(messages: list[int]) -> str:
    # Build a "name" based on values from "MIDI PARAMETER CHANGE TABLE (Tone Generator)"
    # as Reface CP doesn't have patch names, not even in SysEx.
    split_messages = splitSysexMessage(messages)

    # print(f'nameFromDump called. split_messages {listListToHexString(split_messages)}')

    # split_messages[1] has "MIDI PARAMETER CHANGE TABLE (Tone Generator)" [30 00 00]
    address = split_messages[1][8 : 8 + 3]
    if address != [0x30, 0x00, 0x00]:
        raise Exception("numberFromDump: TG data not found")

    data_offset = 0x0B
    data = split_messages[1][data_offset:-2]

    # Only referencing know indices in TG table.
    # See "MIDI PARAMETER CHANGE TABLE (Tone Generator)"
    name = ""
    for i in range(2, 0xD + 1):
        b = data[i]
        name += chr((b >> 4) + 65) + chr((b & 0b1111) + 97)

    print(f"data {byteListToHexString(data)} -> name {name}")
    return name


#
# Leaving helpful setup information specific for a synth
#


def setupHelp():
    return (
        "Yamaha reface CP\n"
        "\n"
        "Owner's Manual: https://usa.yamaha.com/files/download/other_assets/6/438816/reface_en_om_b0.pdf\n"
        "Reference Manual: https://usa.yamaha.com/files/download/other_assets/7/438827/reface_en_rm_a0.pdf\n"
        "Data List: https://usa.yamaha.com/files/download/other_assets/7/794817/reface_en_dl_b0.pdf\n"
        "\n"
        "To enable MIDI control, with unit off, hold E2 and turn unit on."
        "The [TYPE] knob’s Clv lamp and the lamps from the TREMOLO/WAH to D.DELAY/A.DELAY sections light up to indicate MIDI control is on."
        "If the lights flash, MIDI control is off."
    )


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
    device_number: int, model_id: int, address: list[int]
) -> list[int]:
    # (3-4-4) DUMP REQUEST
    # Bulk Dump Request: F0, 43, 2n, gh, gl, id, ah, am, al, F7
    return [
        0xF0,  # Exclusive status
        0x43,  # YAMAHA ID
        0x20 | (device_number & 0x0F),  # Device Number
        0x7F,  # Group ID High
        0x1C,  # Group ID Low
        model_id,
        address[0],  # Address High
        address[1],  # Address Mid
        address[2],  # Address Low
        0xF7,  # End of Exclusive
    ]
