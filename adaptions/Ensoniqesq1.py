#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#   Ensoniq ESQ-1 Adaptation version 0.9 by Mark Peters, 2021.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib


def name():
    return "Ensoniq ESQ-1"


def setupHelp():
    return "The Ensoniq ESQ-1 must be prepared to receive programs via sysex.\n\n" \
           "  1. Press black MIDI button - to open MIDI settings.\n" \
           "  2. Press grey bottom-right 'soft' button - to select MIDI Enable features.\n" \
           "  3. Press white Up data-entry button - to enable SX.\n" \
           "  4. Press black Internal button - to select a program page.\n\n" \
           "If a program has been received from the Knobkraft ORM and is in the ESQ-1 edit buffer,\n" \
           "it must be saved or cleared with *EXIT* before another program can be received.\n\n" \
           "This adaptation should work with the ESQ-M and SQ-80 too, but this has not been tested."


def deviceDetectWaitMilliseconds():
    return 100


def createDeviceDetectMessage(channel):
    # See ESQ-1 Software Version 3 Update p 7 for Device Inquiry Request (beware error regarding last byte).
    # Note this might not work for an ESQ-1 with operating system version < 3.0.
    # byte 0: f0 System Exclusive status byte
    # byte 1: 7e Non-real-time message
    # byte 2: 7f All channel broadcast code
    # byte 3: 06 General Information message code
    # byte 4: 01 Device Inquiry Message message code
    # byte 5: f7 End of Exclusive
    return [0b11110000, 0b01111110, 0b01111111, 0b00000110, 0b00000001, 0b11110111]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # See ESQ-1 Software Version 3 Update p 7 (beware errors regarding last 3 bytes),
    # ESQ-M Musician's Manual p 111 and SQ-80 Musician's Manual p 196 for their Device ID Messages:
    # The ESQ-1 responds with this message regardless of whether SX is enabled on the MIDI page.
    if (len(message) > 7
            and message[0] == 0b11110000  # f0 Sysex
            and message[1] == 0b01111110  # 7e Non-real-time message
            # byte 2: 00-0f Base MIDI Channel
            and message[3] == 0b00000110  # 06 General Information message code
            and message[4] == 0b00000010  # 02 Device ID Message code
            and message[5] == 0b00001111  # 0f ENSONIQ System Exclusive manufacturer's code
            and message[6] == 0b00000010  # 02 ESQ Product Family code (lsb)
            and message[7] == 0b00000000):  # 00 ESQ Product Family code (msb)
        # byte 8: 01 ESQ-1 Family Member code (lsb) ESQ-1: 01; ESQ-M: 10; SQ-80: 11.
        # byte 9: 00 ESQ-1 Family Member code (msb)
        # byte 10: 00 Software revision information
        # byte 11: 00 - unused -
        # byte 12: 32 Minor Version number (decimal fraction)
        # byte 13: 03 Major Version number (integer portion)
        # byte 14: f7 End of Exclusive
        return message[2]
    return -1


def createEditBufferRequest(channel):
    # See ESQ-1 Musician's Manual appendix p A-9 for Current Program Dump Request.
    return [0xf0, 0x0f, 0x02, channel, 0x09, 0xf7]


def isEditBufferDump(message):
    # See ESQ-1 Musician's Manual appendix pp A-5 and A-6 for single-program header.
    return (len(message) > 4
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x0f  # Ensoniq
            and message[2] == 0x02  # ESQ-1
            # byte 3: MIDI channel
            and message[4] == 0x01  # Single Program Dump
            )


def createProgramDumpRequest(channel, program_number):
    # See ESQ-1 Musician's Manual appendix p A-9 for Current Program Dump Request.
    # To select a specific program, send a program change in the range 0-119 to make that program current.
    # If the ESQ-1 does not have a program cartridge, the program number x will default to x modulo 40.
    return [0xc0 | channel, program_number] + createEditBufferRequest(channel)


def isSingleProgramDump(message):
    # This is only ever the edit buffer.
    return isEditBufferDump(message)


def createBankDumpRequest(channel, bank):
    # See ESQ-1 Musician's Manual appendix p A-9 for All Program Dump Request.
    return [0xf0, 0x0f, 0x02, channel, 0x0a, 0xf7]


def isPartOfBankDump(message):
    # See ESQ-1 Musician's Manual appendix pp A-5 and A-6 for all-program header.
    return (len(message) > 4
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x0f  # Ensoniq
            and message[2] == 0x02  # ESQ-1
            # byte 3: MIDI channel
            and message[4] == 0x02  # All Program Dump
            )


def isBankDumpFinished(messages):
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False


def extractPatchesFromBank(message):
    # A bank dump consists of 8166 bytes: 5 in the header, 8160 (in 40 programs of 204), 1 in the footer.
    # Why is 'patch' mixed up with 'program' here?
    if isPartOfBankDump(message):
        channel = message[2]
        data = message[5:-1]
        # After removing the sysex header and footer we are left with 40 programs of 204 bytes each
        data_pointer = 0
        result = []
        while data_pointer + 203 < len(data):
            # Read one more patch
            next_patch = data[data_pointer:data_pointer + 204]
            next_program_dump = [0xf0, 0x0f, 0x02, channel, 0x01] + next_patch + [0xf7]
            print("Found patch " + nameFromDump(next_program_dump))
            result += next_program_dump
            data_pointer += 204
        return result


def numberOfBanks():
    # The ESQ-1 may be fitted with a program cartridge adding 2 more banks of 40. Assume it is not fitted.
    # In any case, only the Internal bank of 40 is available via sysex.
    return 1


def numberOfPatchesPerBank():
    return 40


def nameFromDump(message):
    # The 6 characters of the name are encoded immediately after the header, in 2 bytes per character.
    name = ''
    if len(message) > 17:  # 5 bytes of sysex header plus 12 bytes for name
        oddchrs = {  # The ESQ-1 has some peculiar non-ASCII characters, safely approximated thus:
            95: "v", 33: "0.", 35: "1.", 37: "2.", 40: "3.", 41: "4.", 58: "5.", 59: "6.", 91: "7.", 92: "8.", 93: "9."
        }
        i = 0
        while i < 12:
            code = message[i + 5] + message[i + 6] * 16  # decode little endian hex
            if code in oddchrs:
                name += oddchrs[code]  # lookup peculiar character(s), otherwise...
            else:
                name += chr(code)  # ...use ordinary ASCII
            i += 2
    return name


def convertToEditBuffer(channel, message):
    # Instead of just returning the same message, this ensures that the most recently chosen channel is used.
    if isEditBufferDump(message):
        return message[:3] + [channel] + message[4:]


def calculateFingerprint(message):
    # ignore 5 bytes of sysex header, 12 bytes of name, and last byte (sysex footer)
    data = message[17:-1]
    return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from sound values


def friendlyBankName(bank_number):
    return "Internal"


# Test data picked up by test_adaptation.py
def test_data():
    def programs(messages):
        yield {"message": messages[0], "name": "RADZIC"}

    return {"sysex": "testData/Radzic-ESQ1.syx", "program_generator": programs}
