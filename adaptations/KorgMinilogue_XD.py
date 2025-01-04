#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
# Based on DeepMind adaptation.  Modified to Minilogue XD version by aka Andy2No

import hashlib  # for fingerprint comparison
from typing import List

import testing

korg_id = 0x42


def name():
    return "Minilogue XD"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def needsChannelSpecificDetection():
    return False


#  DEVICE INQUIRY REPLY
# +---------+------------------------------------------------+
# | Byte[H] |                Description                     |
# +---------+------------------------------------------------+
# |   F0    | Exclusive Status                               |
# |   7E    | Non Realtime Message                           |
# |   0g    | MIDI Global Channel  ( Device ID )             |
# |   06    | General Information                            |
# |   02    | Identity Reply                                 |
# |   42    | KORG ID              ( Manufacturers ID )      |
# |   51    | minilogue xd  ID     ( Family ID   (LSB))      |
# |   01    |                      ( Family ID   (MSB))      |
# |   00    |                      ( Member ID   (LSB))      |
# |   00    |                      ( Member ID   (MSB))      |
# |   xx    |                      ( Minor Ver.  (LSB))      |
# |   xx    |                      ( Minor Ver.  (MSB))      |
# |   xx    |                      ( Major Ver.  (LSB))      |
# |   xx    |                      ( Major Ver.  (MSB))      |
# |   F7    | END OF EXCLUSIVE                               |
# +---------+------------------------------------------------+
#
#   This message is transmitted whenever a INQUIRY MESSAGE REQUEST is received.
def channelIfValidDeviceResponse(message):
    if (len(message) > 9
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            # [2] may vary -  "nn    | MIDI Channel (Device ID)"
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Identity Reply
            and message[5] == korg_id
            and message[6] == 0x51
            and message[7] == 0x01
            and message[8] == 0x00  # ( Member ID   (LSB))
            and message[9] == 0x00):  # ( Member ID   (MSB)) . Ignore the rest, hope for the best

        # Reply with the device ID of the Minilogue XD - Global MIDI channel (see minilogue_xd__MIDIImp.txt)
        return message[2]
    return -1


# (1) CURRENT PROGRAM DATA DUMP REQUEST                               R
# +----------------+--------------------------------------------------+
# |     Byte       |             Description                          |
# +----------------+--------------------------------------------------+
# | F0,42,3g,      | EXCLUSIVE HEADER                                 |
# |    00,01,51    |                                                  |
# | 0001 0000 (10) | CURRENT PROGRAM DATA DUMP REQUEST      10H       |
# | 1111 0111 (F7) | EOX                                              |
# +----------------+--------------------------------------------------+
def createEditBufferRequest(channel):
    # p. 141 of the Deepmind manual
    return [0xf0] + [korg_id] + [0x30 + (channel & 0x0f), 0x00, 0x01, 0x51, 0x10, 0xf7]


# (4) CURRENT PROGRAM DATA DUMP                                     R/T
# +----------------+--------------------------------------------------+
# |     Byte       |             Description                          |
# +----------------+--------------------------------------------------+
# | F0,42,3g,      | EXCLUSIVE HEADER                                 |
# |    00,01,51    |                                                  |
# | 0100 0000 (40) | CURRENT PROGRAM DATA DUMP              40H       |
# | 0ddd dddd (dd) | Data                                             |
# | 0ddd dddd (dd) |  :         Data Size         Conv. Size          |
# | 0ddd dddd (dd) |  :      384Bytes (7bit) -> 336Bytes (8bit)       |
# | 0ddd dddd (dd) |  :                                               |
# | 1111 0111 (F7) | EOX                        (See NOTE 1, TABLE 2) |
# +----------------+--------------------------------------------------+
#
#  Receive this message & data, save them to Edit Buffer and transmits Func=23 or Func=24 message.
#  Receive Func=10 message, and transmits this message & data from Edit Buffer.
#  When "Program Dump" is executed, transmit this message & data from Edit Buffer.
def isEditBufferDump(message):
    # print('Possible Edit Buffer Dump starts with ', message[0:10])
    return (len(message) > 7
            and message[0] == 0xf0
            and message[1] == korg_id
            # and message[2] == 0x30 + (channel & 0x0f)  # don't know the value of global channel, so ignore that
            and message[3] == 0x00
            and message[4] == 0x01
            and message[5] == 0x51
            and message[6] == 0x40)  # Edit Buffer Dump Response
    # Ignore the Comms Protocol Version for now, an update might not make the Adaptation invalid
    # and message[7] == 0x06) # Comms Protocol Version. They seem to have updated and are with 7 right now?


def numberOfBanks():
    # return 4    # works reasonably well if treated as 4 banks of 128, but patches are really just numbered 1-500
    # return 5 # Since Program Change only works for 1-100, it seems it's meant to be treated as 5 banks of 100
    return 1  # One big bank, of 500 patches
    # The Minilogue XD has 500 program slots, and a 2 byte program number, but seems to be just one bank,
    # according to the MIDI implementation document, which also says it only has 8 bits - not enough
    # It turns out it's 2 bits: 7 bits, so treat it as 4 x banks of 128
    # The fourth bank is shorter, by 12, but that doesn't seem to be ar problem
    # - retrieving bank 4, or all, currently just gives 13 copies of the patch 500, at the end
    # (not sure why they're not seen as duplicates)


# |   Cn   | pp (pp) | --    (--)  | Program Change (pp=0~99)                 *5-1    |     [[range confirmed]]
# |   Bn   | 00 (00) | vv    (vv)  | Bank Select (MSB) vv=0                   *5-1    |     [[should be 0-4?]]
#  *5-1 : This message is recognized when the "MIDI Rx Prog Chg" is set to "On".
def numberOfPatchesPerBank():
    # See comments under numberOfBanks()
    # return 100  # Program change only works for choosing patches 1-100 (0-99).
    # Bank select is not properly documented... and doesn't appear to work.
    return 500  # either 5 banks of 100, or one big bank of 500 works, but using one big bank is better for numbering


def friendlyProgramName(program):
    return str(program + 1).zfill(3)  # By default, import from synth gives 0-499.
    # Add one and pad with leading zeros to make it match the display


# (2) PROGRAM DATA DUMP REQUEST (1 PROG)                              R
# +----------------+--------------------------------------------------+
# |     Byte       |             Description                          |
# +----------------+--------------------------------------------------+
# | F0,42,3g,      | EXCLUSIVE HEADER                                 |
# |    00,01,51    |                                                  |
# | 0001 1100 (1C) | PROGRAM DATA DUMP REQUEST              1CH       |
# | 0ppp pppp (pp) | Source Program No.(LSB bit  7~0)                 |
# | 0000 000p (PP) | Source Program No.(MSB bit    8)                 |
# | 1111 0111 (F7) | EOX                                              |
# +----------------+--------------------------------------------------+
#
#  Receive this message, and transmits Func=4C or Func=24 message.
def createProgramDumpRequest(channel, patchNo):
    # print('createProgramDumpRequest for ', patchNo)

    # bank = patchNo // numberOfPatchesPerBank()
    # progMSB = patchNo // numberOfPatchesPerBank()
    progMSB = patchNo >> 7  # This doesn't work as per the documentation.  See comments under numberOfBanks()
    # progLSB = patchNo % numberOfPatchesPerBank()
    progLSB = patchNo & 0x7f
    # Patch sysex has no concept of a bank, just a two byte number, with LS 7 bits in the first one
    # There are 500 program slots, so the MSB appears to use 2 bits, not as documented
    # print('createProgramDumpRequest =>', [0xf0], [korg_id], [0x30 + (channel & 0x0f), 0x00, 0x01, 0x51, 0x1c, progLSB, progMSB, 0xf7])
    return [0xf0] + [korg_id] + [0x30 + (channel & 0x0f), 0x00, 0x01, 0x51, 0x1c, progLSB, progMSB, 0xf7]


# See comment block before convertToEditBuffer(), below, for sysex definition
def isSingleProgramDump(message):
    # print('Possible Program Dump starts with ', message[0:10])
    return (len(message) > 7
            and message[0] == 0xf0
            and message[1] == korg_id
            # and message[2] == 0x30 + (channel & 0x0f)  # don't know the value of global channel, so ignore that
            and message[3] == 0x00
            and message[4] == 0x01
            and message[5] == 0x51
            and message[6] == 0x4c)  # Program Dump Response "PROGRAM DATA DUMP"
    # Ignore the Commas Protocol Version for now, an update might not make the Adaptation invalid
    # and message[7] == 0x06) # Comms Protocol Version


# This "function currently is not used, but is the basis for the upcoming feature of bank mangement - to send a patch into a specific position in the synths memory, not just the edit buffer or a hard-coded "pseudo edit buffer" like slot 100."
# - but needs to be implemented to be able to dump as Program Dump format?
def convertToProgramDump(channel, message, program_number):
    progLSB = program_number & 0x7f
    progMSB = (program_number >> 7) & 0x7f
    if isEditBufferDump(message):
        return message[0:6] + [0x4c, progLSB, progMSB] + message[7:]
    if isSingleProgramDump(message):
        # Need to construct a new program buffer dump from a single program dump. Insert the desired program position
        return message[0:7] + [progLSB, progMSB] + message[9:]
    raise Exception("Neither edit buffer nor program dump.  Can't be converted")


# " will be used if present to detect the original program slot location stored in a program dump for archival purposes."
def numberFromDump(message):  # Is this ever called?
    progLSB = message[7]
    progMSB = message[8]
    # print('numberFromDump ',  progLSB, ' ', progMSB, ' => ', progLSB + progMSB * numberOfPatchesPerBank() + 1)
    # return progLSB + progMSB * numberOfPatchesPerBank() + 1
    # print('numberFromDump ',  progLSB, ' ', progMSB, ' => ', progLSB + (progMSB << 7) + 1)
    return progLSB + (progMSB << 7)


# Program names are not in standard ASCII
# TABLE 2 : PROGRAM PARAMETER
# +---------+-------+---------+---------------------------------------------+
# |  Offset |  Bit  |  Range  |  Description                                |
# +---------+-------+---------+---------------------------------------------+
# |   0~3   |       |  ASCII  |  'PROG'                                     |
# +---------+-------+---------+---------------------------------------------+
# |   4~15  |       |  ASCII  |  PROGRAM NAME [12]                 *note P1 |
# +---------+-------+---------+---------------------------------------------+
# *note P1 (PROGRAM NAME)
#   32         : ' '
#   33         : '!'
#   35         : '#'
#   36         : '$'
#   37         : '%'
#   38         : '&'
#   39         : '''
#   40         : '('
#   41         : ')'
#   42         : '*'
#   44         : ','
#   45         : '-'
#   46         : '.'
#   47         : '/'
#   48~57      : '0'~'9'
#   58         : ':'
#   63         : '?'
#   65~90      : 'A'~'Z'
#   97~122     : 'a'~'z'
def nameFromDump(message):  # see comment block above createProgramDumpRequest for sysex format
    # print('nameFromDump : starts with ', message[0:11])
    nameBaseIndex = 4
    nameLen = 12
    if isEditBufferDump(message):
        data = unescapeSysex(message[7:-1])
        # return ''.join([decodeNameChar(chr(x)) for x in data[nameBaseIndex:nameBaseIndex + nameLen]])
        return ''.join([chr(x) for x in data[nameBaseIndex:nameBaseIndex + nameLen]]).strip("\0")
    if isSingleProgramDump(message):
        data = unescapeSysex(message[9:-1])
        #    return ''.join([decodeNameChar(chr(x)) for x in data[nameBaseIndex:nameBaseIndex + nameLen]])
        return ''.join([chr(x) for x in data[nameBaseIndex:nameBaseIndex + nameLen]]).strip("\0")
    return 'invalid'


# See *note P1 (PROGRAM NAME)
# This is just ASCII with some characters missing, so no actual need to translate?
def decodeNameChar(c):
    cVal = ord(c)
    #   48~57      : '0'~'9'
    #   58         : ':'
    #   63         : '?'
    #   65~90      : 'A'~'Z'
    #   97~122     : 'a'~'z'
    if cVal > 96:
        return chr(cVal - 97 + ord('a'))
    if cVal > 64:
        return chr(cVal - 65 + ord('A'))
    if cVal == 63: return '?'
    if cVal == 58: return ':'
    if cVal > 47 and cVal < 58:
        return chr(cVal - 48 + ord('0'))
    #   32         : ' '
    if cVal == 32: return ' '
    #   33         : '!'
    if cVal == 33: return '!'
    #   35         : '#'
    if cVal == 35: return '#'
    #   36         : '$'
    if cVal == 36: return '$'
    #   37         : '%'
    if cVal == 37: return '%'
    #   38         : '&'
    if cVal == 38: return '&'
    #   39         : '''
    if cVal == 39: return "'"
    #   40         : '('
    if cVal == 40: return '('
    #   41         : ')'
    if cVal == 41: return ')'
    #   42         : '*'
    if cVal == 42: return '*'
    #   44         : ','
    if cVal == 44: return ','
    #   45         : '-'
    if cVal == 45: return '-'
    #   46         : '.'
    if cVal == 46: return '.'
    #   47         : '/'
    if cVal == 47: return '/'

    return c


# (5) PROGRAM DATA DUMP (1 PROG)                                    R/T
# +----------------+--------------------------------------------------+
# |     Byte       |             Description                          |
# +----------------+--------------------------------------------------+
# | F0,42,3g,      | EXCLUSIVE HEADER                                 |
# |    00,01,51    |                                                  |
# | 0100 1100 (4C) | PROGRAM DATA DUMP                      4CH       |
# | 0ppp pppp (pp) | Program No.(LSB bit  6~0)                        |
# | 0000 000p (PP) | Program No.(MSB bit    7)                        |
# | 0ddd dddd (dd) | Data                                             |
# | 0ddd dddd (dd) |  :         Data Size         Conv. Size          |
# | 0ddd dddd (dd) |  :      384Bytes (7bit) -> 336Bytes (8bit)       |
# | 0ddd dddd (dd) |  :                                               |
# | 1111 0111 (F7) | EOX                        (See NOTE 1, TABLE 2) |
# +----------------+--------------------------------------------------+
#
#  Receive this message & data, save them to Internal Memory and transmits Func=23 or Func=24 message.
#  Receive Func=1C message, and transmits this message & data from Internal Memory.
#  When "All Dump" is executed, transmit this message & data from Internal Memory.

# - note there are 500 program locations, so these details seem to be wrong - the above only allows for 256
def convertToEditBuffer(channel, message):
    # print('convertToEditBuffer : ', message[0:10])
    if isEditBufferDump(message):
        # print('Is already an edit buffer')
        return message
    if isSingleProgramDump(message):
        # Need to construct a new edit buffer dump from a single program dump. Keep the protocol version intact
        # print('Converting single program dump to edit buffer')
        return message[0:6] + [0x40] + message[9:]
    raise Exception("Neither edit buffer nor program dump.  Can't be converted")


def calculateFingerprint(message):
    # print(' calculateFingerprint():', message)

    # data = unescapeSysex(message[6:-1])
    dataStart = 9  # PROGRAM DATA DUMP
    if message[6] == 0x40:  # edit buffer
        dataStart = 7

    data = message[dataStart:-1]

    # Blank out name, they should not matter for the fingerprint
    # data[dataStart + name_len] = [0] * name_len

    return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from


def unescapeSysex(sysex):
    # This implements the algorithm defined on page 141 of the Deepmind user manual. I think it is the same as DSI uses
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        msbits = sysex[dataIndex]  # contains the MS bits of the next 7 bytes
        dataIndex += 1
        for i in range(7):  # the next 7 values are just original_value & 0x7f
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex] | ((msbits & (1 << i)) << (7 - i)))
            dataIndex += 1
    return result


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], number=53, name="1982theme", friendly_number="054")

    return testing.TestData(sysex="testData/KorgMinilogueXD/1982theme.syx", program_generator=programs)
