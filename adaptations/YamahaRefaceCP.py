"""
# Yamaha Reface CP

## MIDI Data Format

### (3-4) SYSTEM EXCLUSIVE MESSAGE

Device Number: Bulk Dump=0n; Parameter change=1n; Dump Request=2n; Parameter Request=3n

#### (3-4-1) UNIVERSAL NON REALTIME MESSAGE

#### (3-4-1-1) IDENTITY REQUEST (Receive only)

`F0H 7EH 0nH 06H 01H F7H`

("n" = Device No. However, this instrument receives under "omni.")

#### (3-4-1-2) IDENTITY REPLY (Transmit only)

`F0H 7EH 7FH 06H 02H 43H 00H 41H 52H 06H 00H 00H 00H 7FH F7H`

#### (3-4-3) BULK DUMP

Offset|Value|Details
-|-|-
+00|F0H|Exclusive status
+01|43H|YAMAHA ID
+02|0nH|Device Number
+03|7FH|Group Number High
+04|1CH|Group Number Low
+05|bbbbbbb|Byte Count
+06|bbbbbbb|Byte Count
+07|04H|Model ID
+08|aaaaaaa|Address High
+09|aaaaaaa|Address Mid
+0A|aaaaaaa|Address Low
+0B|0|Data
+0C|...|(variable length, see note)
+-02|ccccccc|Check-sum
+-01|F7H|End of Exclusive

See the following BULK DUMP Table for **Address** and **Byte Count**.

**Byte Count** shows the size of data in blocks from Model ID onward (up to but not including the checksum).

The **Check-sum** is the value that results in a value of 0 for the lower 7 bits when the Model ID, Start Address, Data and Check sum itself are added.

#### (3-4-4) DUMP REQUEST

Offset|Value|Details
-|-|-
+00|F0H|Exclusive status
+01|43H|YAMAHA ID
+02|2nH|Device Number
+03|7FH|Group Number High
+04|1CH|Group Number Low
+05|04H|Model ID
+06|aaaaaaa|Address High
+07|aaaaaaa|Address Mid
+08|aaaaaaa|Address Low
+09|F7H|End of Exclusive

See the following DUMP REQUEST Table for **Address** and **Byte Count**.

## MIDI Data Table

### Bulk Dump Block

**Top Address** indicates the top address of each block designated by bulk dump operation.

**Byte Count** shows the size of data in blocks from Model ID onward (up to but not including the checksum).

To carry out TG bulk dump request, designate its corresponding **Bulk Header** address.

Parameter Block|Description|Byte Count (Dec)|Top Address(hex) - H M L
-|-|-|-
SYSTEM|Common|36|00 00 00
TG (Tone Generator)|Bulk Header|4|0E 0F 00
TG|Common|20|30 00 00
TG|Bulk Footer|4|0F 0F 00

### MIDI PARAMETER CHANGE TABLE (SYSTEM)

This is returned from a "SYSTEM" Bulk Dump Block request.

Address(hex; H=00; M=00) - L|Size|Data Range(hex)|Parameter Name|Description
-|-|-|-|-
00|1|00-0F, 7F|MIDI transmit channel|1-16, off
01|1|00-0F, 10|MIDI receive channel|1-16, All
02|4|00-00<br>00-07<br>00-0F<br>00-0F|Master Tune|-102.4 - +102.3 (cent)<br>1st bit 3-0 : bit 15- 12<br>2nd bit 3-0 : bit 11- 8<br>3rd bit 3-0 : bit 7- 4<br>4th bit 3-0 : bit 3- 0
06|1|Local Control|off, ON
07|1|34-4C|Master Transpose|-12 - +12 (semitones)
0B|1|00-01|Sustain Pedal Select|FC3, FC4/5
0C|1|00-01|Auto Power-Off|off, ON
0D|1|00-01|Speaker Output|off, ON
0E|1|00-01|MIDI Control|off, ON

Total Size = 32 (unlisted fields are reserved)

### MIDI PARAMETER CHANGE TABLE (Tone Generator)

This is returend from a "TG Common" Dump Block Request.

Address(hex; H=30; M=00) - L|Size|Data Range(hex)|Parameter Name|Description|Notes
-|-|-|-|-|-
00|1|00-7F|Volume|0-127
02|1|00-05|Wave Type<br>(TYPE)|Rd I, Rd II, Wr, Clv, Toy,CP
03|1|00-7F|Drive<br>(DRIVE)|0-127
04|1|00-02|Effect 1 Type<br>(TREMOLO/WAH)|thru (middle position),<br>tremolo (TREMOLO),<br>wah (WAH)
05|1|00-7F|Effect 1 Depth<br>(DEPTH)|0-127
06|1|00-7F|Effect 1 Rate<br>(RATE)|0-127
07|1|00-02|Effect 2 Type<br>(CHORUS/PHASER)|thru (middle position),<br>chorus (CHORUS),<br>phaser (PHASER)
08|1|00-7F|Effect 2 Depth<br>(DEPTH)|0-127
09|1|00-7F|Effect 2 Speed<br>(SPEED)|0-127
0A|1|00-02|Effect 3 Type<br>(D.DELAY/A.DELAY)|thru (middle potision),<br>Digital Delay (D.DELAY),<br>Analog Delay (A.DELAY)
0B|1|00-7F|Effect 3 Depth<br>(DEPTH)|0-127
0C|1|00-7F|Effect 3 Time<br>(TIME)|0-127
0D|1|00-7F|Reverb Depth<br>(REVERB DEPTH)|0-127

Total Size = 16 (unlisted fields are reserved)
"""

# References:
#
# reface CS/DX/CP/YC Data List
# https://usa.yamaha.com/files/download/other_assets/7/794817/reface_en_dl_b0.pdf
#
# THE STRUCTURE OF A MIDI EVENT:
# http://www.petesqbsite.com/sections/express/issue18/midifilespart1.html
#
# YAMAHA Keyboard - MIDI - Messages
# http://www.jososoft.dk/yamaha/articles/midi_10.htm
#
# reface CP Sounds by Blake Angelos:
# https://soundmondo.yamahasynth.com/voices?reface=CP&user=8
# reface CP Sounds by Katsunori UJIIE / 氏家克典
# https://soundmondo.yamahasynth.com/voices?reface=CP&user=16054


import hashlib
import re

# Bulk Dump Block Top Addresses.
DUMP_ADDR_SYS_COMMON = [0x00, 0x00, 0x00]
DUMP_ADDR_PART_HEADER = [0x0E, 0x0F, 0x00]
DUMP_ADDR_PART_COMMON = [0x30, 0x00, 0x00]
DUMP_ADDR_PART_FOOTER = [0x0F, 0x0F, 0x00]

# Yamaha Reface CP specific values
# See "this._models" in https://soundmondo.yamahasynth.com/components/reface-panel/reface-panel.js
YAMAHA_REFACE_CP = {
    "Identity": [0x52, 0x06],
    "GroupNumber": [0x7F, 0x1C],
    "ModelId": 0x04,
}

#
# System Messages
#
# System messages are intended for the whole MIDI system - not channel specific.
#
# Note that any non-realtime status byte ends a System Exclusive message;
# F7 (EOX) is not required at the end of a SysEx message.
#
# Realtime status bytes may appear any time in the MIDI data stream, including
# in the middle of a SysEx message.
#

# SysEx start and end bytes (offset +0, -1)
SYSEX_EXCLUSIVE_STATUS = 0xF0
SYSEX_END_OF_EXCLUSIVE = 0xF7

# Universal system exclusive messages (offset +1)
SYSEX_ID_NON_REALTIME = 0x7E

# Yamaha specific system exclusive message (offset +1)
SYSEX_MANUFACTURER_ID_YAMAHA = 0x43

# For SYSEX_MANUFACTURER_ID_YAMAHA,
# Value passed to device number at top nibble of "Device Number" (offset +2)
# Low nibble is device ID (channel).
# Bulk Dump=0n; Parameter change=1n; Dump Request=2n; Parameter Request=3n
SYSEX_CMD_BULK_DUMP = 0x00
SYSEX_CMD_DUMP_REQUEST = 0x20

#
# Realtime Messages
#
# Realtime messages are not associated with any one MIDI channel.
#
# They can appear in the MIDI data stream at any time.
#
# These messages consist of a single status byte; they have no data bytes.
#

REALTIME_MESSAGES = {
    0xF8: "Clock",
    0xF9: "(undefined)",
    0xFA: "Start",
    0xFB: "Continue",
    0xFC: "Stop",
    0xFD: "(undefined)",
    0xFE: "Active Sensing",
    0xFF: "System Reset",
}

#
# Helpers
#


# Helper to do hex dump of response
def dumpBytes(prefix, data):
    # Uncomment next line for verbose debugging.
    # print(f"[CP] {prefix} ({len(data)}):", " ".join("{:02x}".format(x) for x in data))
    return


def getParameterChangeTable(data):
    if len(data) != 16:
        raise Exception("Invalid TG data")
    # See "MIDI PARAMETER CHANGE TABLE (Tone Generator)"
    return {
        # "Volume" -- can only be set.
        # "Volume": data[0x0],
        # Wave Type (TYPE)
        "WaveType": data[0x2],
        # Drive (DRIVE):
        "Drive": data[0x3],
        # Effect 1 Type (TREMOLO/WAH)
        "Effect1Type": data[0x4],
        # Effect 1 Depth (DEPTH)
        "Effect1Depth": data[0x5],
        # Effect 1 Rate (RATE):
        "Effect1Rate": data[0x6],
        # Effect 2 Type (CHORUS/PHASER)
        "Effect2Type": data[0x7],
        # Effect 2 Depth (DEPTH)
        "Effect2Depth": data[0x8],
        # Effect 2 Rate (SPEED)
        "Effect2Rate": data[0x9],
        # Effect 3 Type (D.DELAY/A.DELAY)
        "Effect3Type": data[0xA],
        # Effect 3 Depth (DEPTH)
        "Effect3Depth": data[0xB],
        # Effect 3 Rate (TIME)
        "Effect3Rate": data[0xC],
        # Reverb Depth (REVERB DEPTH)
        "ReverbDepth": data[0xD],
    }


def buildIdentitySysExMessage():
    # See "(3-4-1-1) IDENTITY REQUEST (Receive only)"
    # F0H 7EH 0nH 06H 01H F7H

    # ("n" = Device No. However, this instrument receives under "omni.")
    device_number = 0

    cmd = (SYSEX_CMD_BULK_DUMP & 0xF0) | (device_number & 0x7F)
    sysex = [SYSEX_EXCLUSIVE_STATUS, 0x7E, cmd, 0x06, 0x01, SYSEX_END_OF_EXCLUSIVE]
    dumpBytes("buildIdentitySysExMessage returning", sysex)
    return sysex


# Requires modelInfo: groupNumber and modelId to be set.
def buildDumpRequestSysExMessage(device_number, address, modelInfo):
    # GroupNumber: 2 bytes, ModelId: 1 byte, address: 3 bytes
    cmd = (SYSEX_CMD_DUMP_REQUEST & 0xF0) | (device_number & 0x0F)
    sysex = (
        [SYSEX_EXCLUSIVE_STATUS, SYSEX_MANUFACTURER_ID_YAMAHA, cmd]
        + modelInfo["GroupNumber"]
        + [modelInfo["ModelId"]]
        + address
        + [SYSEX_END_OF_EXCLUSIVE]
    )
    dumpBytes("buildDumpRequestSysExMessage returning", sysex)
    return sysex


# (3-4-3) BULK DUMP
# Requires modelInfo: GroupNumber and ModelId to be set.
def buildBulkDumpSysExMessage(device_number, address, modelInfo, data):
    cmd = (SYSEX_CMD_BULK_DUMP & 0xF0) | (device_number & 0x0F)
    data_len = len(data) + 3 + 1  # Include address (3b) and checksum (1b) in data size

    # Set Byte Count
    size_high = (data_len >> 7) & 0x7F
    size_low = data_len & 0x7F

    # GroupNumber: 2 bytes, ModelId: 1 byte, address: 3 bytes
    sysex = (
        [SYSEX_EXCLUSIVE_STATUS, SYSEX_MANUFACTURER_ID_YAMAHA, cmd]
        + modelInfo["GroupNumber"]
        + [size_high, size_low, modelInfo["ModelId"]]
        + address
        + data
    )

    dumpBytes("pre-checksum", sysex)

    # The Check-sum is the value that results in a value of 0 for the
    # lower 7 bits when the Model ID, Start Address, Data and Check sum itself are added.
    check_sum = 0
    for m in sysex[7:]:
        check_sum -= m
    sysex += [(check_sum & 0x7F)]

    sysex += [SYSEX_END_OF_EXCLUSIVE]

    dumpBytes("buildBulkDumpSysExMessage returning", sysex)
    return sysex


def parseNonRealtimeSysEx(message):
    dumpBytes("parseNonRealtimeSysEx", message)

    # See "(3-4-1-2) IDENTITY REPLY (Transmit only)"
    # "7FH 06H 02H 43H 00H 41H 52H 06H 00H 00H 00H"
    if len(message) >= 11:
        # [0]: SysEx Channel [0..0x7f]
        # [1]: Sub-ID -- General Information
        # [2]: Sub-ID2 -- Identity Reply
        # 0xID  Manufacturer's ID
        if (
            message[1 : 2 + 1] == [0x6, 0x2]
            and message[3] == SYSEX_MANUFACTURER_ID_YAMAHA
        ):
            # 0xf1  The f1 and f2 bytes make up the family code. Each
            # 0xf2  manufacturer assigns different family codes to his products.
            # 0xp1  The p1 and p2 bytes make up the model number. Each
            # 0xp2  manufacturer assigns different model numbers to his products.
            # 0xv1  The v1, v2, v3 and v4 bytes make up the version number.
            return {
                "Channel": message[0],
                "ManufacturerID": message[3],
                "Family": message[4 : 5 + 1],
                "Model": message[6 : 7 + 1],
                "Version": message[7 : 10 + 1],
            }

    raise Exception("Invalid NonRealtimeSysEx, message=", message)


# The Check-sum is the value that results in a value of 0 for the
# lower 7 bits when the Model ID, Start Address, Data and Check-sum itself are added.
def checkChecksum(model_id, address, data, checksum):
    vals = (model_id, address[0], address[1], address[2], checksum)
    total = sum(vals)
    for val in data:
        total += val
    if (total & 0x7F) == 0:
        return
    raise Exception("Invalid Checksum")


# message: data starting at "Group Number". Doesn't include "End of Exclusive".
def parseBulkDump(message):
    dumpBytes("parseBulkDump", message)
    if len(message) > 7:
        # Byte Count shows the size of data in blocks from Model ID onward,
        # up to but not including the checksum.
        byte_count = message[2] << 7 | message[3]  # "7-in-8 encoding"
        if len(message) == 4 + byte_count + 1:
            data = None
            if byte_count != 0:
                data = message[8:-1]

            checkChecksum(
                model_id=message[4],
                address=message[5 : 7 + 1],
                data=data,
                checksum=message[-1],
            )

            return {
                "GroupNumber": message[0 : 1 + 1],
                "ModelId": message[4],
                "Address": message[5 : 7 + 1],
                "Data": data,
            }
        else:
            raise Exception(
                "Invalid byte_count, len=", len(message), "byte_count=", byte_count
            )
    raise Exception("Invalid BulkDump")


# message: data starting at "Device Number". Doesn't include "End of Exclusive".
def parseYamahaSysEx(message):
    dumpBytes("parseYamahaSysEx", message)
    if len(message) > 2:
        cmd = message[0] & 0xF0
        if cmd == SYSEX_CMD_BULK_DUMP:
            return parseBulkDump(message[1:])
    raise Exception("Invalid YamahaSysEx")


# message: Complete Bulk Dump object.
# Returns an object representing data in SysEx message.
# raises Exception if invalid.
def parseSysEx(message):
    if message[0] in REALTIME_MESSAGES:
        # Ignore all realtime messages.
        return {}

    dumpBytes("parseSysEx", message)
    if (
        len(message) > 2
        and message[0] == SYSEX_EXCLUSIVE_STATUS
        and message[-1] == SYSEX_END_OF_EXCLUSIVE
    ):
        if message[1] == SYSEX_MANUFACTURER_ID_YAMAHA:
            return parseYamahaSysEx(message[2:-1])
        elif message[1] == SYSEX_ID_NON_REALTIME:
            return parseNonRealtimeSysEx(message[2:-1])
    raise Exception("Unhandled SysEx")


#
# Exports
#


def name():
    """Identity.

    Returns:
        string: Name of the device, uniquely identifying it.
    """
    return "Yamaha Reface CP"


def numberOfBanks():
    """How many banks the synth has.

    Returns:
        int: number of banks.
    """
    return 1


def numberOfPatchesPerBank():
    """How many programs per bank the synth has.

    Returns:
        int: number of programs.
    """
    return 1


def createDeviceDetectMessage(channel):
    """Message to force reply by device.

    Args:
        channel (_type_): Unused?

    Returns:
        _type_: Single MIDI message or multiple MIDI messages in the form of a single list of byte-values integers used to detect the device.
    """
    return buildIdentitySysExMessage()


def channelIfValidDeviceResponse(message):
    """Check if reply came.

    Args:
        message (_type_): _description_

    Returns:
        int: -1, if the message handed in is not the device response we had been expecting, or valid MIDI channel [0..15]
    """
    # See "(3-4-1-2) IDENTITY REPLY (Transmit only)"
    try:
        data = parseSysEx(message)
        if len(data) != 0:
            print("channelIfValidDeviceResponse",  data)
    except Exception as e:
        print("[CP] channelIfValidDeviceResponse Exception:", e)
        return -1

    if data.get("Model", None) == YAMAHA_REFACE_CP["Identity"]:
        # Assume channel 0
        return 0

    # Not response we're looking for.
    return -1


def needsChannelSpecificDetection():
    """Specifying to not to do channel specific detection.

    Returns:
        bool: True if the createDeviceDetectMessage() should be called once for each of the 16 possible MIDI channels and MIDI outputs
    """
    return False


def createEditBufferRequest(channel):
    """Requests the edit buffer from the synth.

    Args:
        channel (_type_): the channel detected

    Returns:
        _type_: single MIDI message that makes the device send its Edit Buffer
    """
    return buildDumpRequestSysExMessage(
        channel, DUMP_ADDR_PART_HEADER, YAMAHA_REFACE_CP
    )


def isEditBufferDump(message):
    """Check if a generic MIDI message is the reply to the request we just created.

    Args:
        message (_type_): generic MIDI message

    Returns:
        bool: true if a generic MIDI message is the reply to the request we just created.
    """
    try:
        data = parseSysEx(message)
    except Exception as e:
        print("[CP] isEditBufferDump Exception:", e)
        return False

    if data.get("Address", []) == DUMP_ADDR_PART_COMMON:
        return True

    return False


def convertToEditBuffer(channel, message):
    """Send edit buffer dump, to not overwrite the synths patch memory.

    Args:
        channel (_type_): the channel detected
        message (_type_): patch stored in the database

    Returns:
        _type_: MIDI message or multiple MIDI messages (all in a single list of integers)
    """
    dumpBytes("convertToEditBuffer", message)

    header = buildBulkDumpSysExMessage(
        channel, DUMP_ADDR_PART_HEADER, YAMAHA_REFACE_CP, []
    )
    # common = buildBulkDumpSysExMessage(channel, DUMP_ADDR_PART_COMMON, YAMAHA_REFACE_CP, message)
    # message is the buffer that was sent into isEditBufferDump(). We can re-use that.
    # (Technically, I should probably call isEditBufferDump to validate, but I don't think the CP will send anything else)
    common = message
    footer = buildBulkDumpSysExMessage(
        channel, DUMP_ADDR_PART_FOOTER, YAMAHA_REFACE_CP, []
    )

    dumpBytes("convertToEditBuffer[header]", header)
    dumpBytes("convertToEditBuffer[common]", common)
    dumpBytes("convertToEditBuffer[footer]", footer)
    return header + common + footer


def nameFromDump(message):
    # Build a "name" based on values from "MIDI PARAMETER CHANGE TABLE (Tone Generator)"
    # as Reface CP doesn't have patch names, not even in SysEx.
    try:
        data = parseSysEx(message)
    except Exception as e:
        print("[CP] nameFromDump Exception:", e)
        return "(Invalid)"

    if data.get("Address", []) == DUMP_ADDR_PART_COMMON:
        return str(hash(getParameterChangeTable(data["Data"]).values()))

    return "(Invalid)"


#
# Implementing your own tests
#

# def make_test_data():
