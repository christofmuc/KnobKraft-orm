#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import List

import testing
from knobkraft import unescapeSysex_deepmind as unescapeSysex

behringer_id = [0x00, 0x20, 0x32]


def name():
    return "Deepmind 12"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 8
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply (wrongly specified in Deepmind manual as 0x01?)
            and message[5:8] == behringer_id
            and message[8] == 0x20):  # Deepmind family. Ignore the rest, hope for the best
        # Reply with the device ID of the Deepmind 12 - this is not the same as a MIDI channel, but a sysex channel
        return message[2]
    return -1


def createEditBufferRequest(channel):
    # p. 141 of the Deepmind manual
    return [0xf0] + behringer_id + [0x20, channel & 0x0f, 0x03, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 7
            and message[0] == 0xf0
            and message[1:4] == behringer_id
            and message[4] == 0x20  # Deepmind
            and message[6] == 0x04)  # Edit Buffer Dump Response
            # Ignore the Comms Protocol Version for now, an update might not make the Adaptation invalid
            #and message[7] == 0x06) # Comms Protocol Version. They seem to have updated and are with 7 right now?


def numberOfBanks():
    return 8


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    # p. 141 of the Deepmind manual
    return [0xf0] + behringer_id + [0x20, channel & 0x0f, 0x01, bank, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 7
            and message[0] == 0xf0
            and message[1:4] == behringer_id
            and message[4] == 0x20  # Deepmind
            and message[6] == 0x02)  # Program Dump Response
            # Ignore the Commas Protocol Version for now, an update might not make the Adaptation invalid
            #and message[7] == 0x06) # Comms Protocol Version


def nameFromDump(message):
    nameBaseIndex = 223
    if isEditBufferDump(message):
        data = unescapeSysex(message[8:-1])
        return ''.join([chr(x) for x in data[nameBaseIndex:nameBaseIndex+16]])
    if isSingleProgramDump(message):
        data = unescapeSysex(message[10:-1])
        return ''.join([chr(x) for x in data[nameBaseIndex:nameBaseIndex+16]])
    return 'invalid'


def numberFromDump(message) -> int:
    if isSingleProgramDump(message):
        return message[8] * numberOfPatchesPerBank() + message[9]
    elif isEditBufferDump(message):
        return 0
    return -1


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    if isSingleProgramDump(message):
        # Need to construct a new edit buffer dump from a single program dump. Keep the protocol version intact
        return message[0:5] + [channel, 0x04, message[7]] + message[10:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message):
        # Need to construct a new program dump from an edit buffer dump. Keep the protocol version intact
        return message[0:5] + [channel, 0x02, message[7], bank, program] + message[8:]
    if isSingleProgramDump(message):
        # Need to construct a new program dump from a single program dump. Keep the protocol version intact
        return message[0:5] + [channel, 0x02, message[7], bank, program] + message[10:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


# Test data picked up by test_adaptation.py
def test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], name="Brass Set 1     ", number=896)

    return testing.TestData(sysex="testData/DM12_-_Juno_106_Presets_H.syx", program_generator=programs)
