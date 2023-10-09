#   Copyright (c) 2021-2023 Christof Ruch. All rights reserved.
#   Novation Mini/Ultranova Adaptation version 0.6 by Cedric Tessier, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
# based on my own (partial) reverse engineering of Novation UltraNova librarian.
#
# Attempt to refactor for A Station by Rothchild 17-09-23
#
import hashlib
from typing import List

import testing

def wrapSysex(data):
    return [0xf0,  # Sysex message
            0x00, 0x20, 0x29, 0x01, # Novation
            #0x40,  # Product type?
            0x40,  # Device Type (1 = Synth)?
            0x7f   # ?
            ] + data + [0xf7]  # EOX


def name():
    return 'Novation AStation'


def createDeviceDetectMessage(channel):
    return createEditBufferRequest(channel)


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    return 100


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 8
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x00  # Novation
            and message[2] == 0x20  # Novation ID1
            and message[3] == 0x29  # Novation ID2
            and message[4] == 0x01  # Message Type ?
            and message[5] == 0x40  # Device Type
            and message[7] == 0x00):
        # TODO: message[11] is protection status
        # return sysex device ID or 0x7f
        return message[6]
    return -1


def createEditBufferRequest(channel):
    return wrapSysex([0x40, 0x21, 0x00, 0x00, 0x00, 0x00])


def isEditBufferDump(message):
    return (len(message) > 13
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x00  # Novation
            and message[2] == 0x20  # Novation ID1
            and message[3] == 0x29  # Novation ID2
            and message[4] == 0x01
            and message[5] == 0x40
            and message[6] == 0x7f
            and message[7] == 0x00  # ?
            #and message[8] == 0x00  # ?
            #and message[9] == 0x10  # ?
            #and message[10] == 0x00  # ?
            and message[11] == 0x00  # Bank number
            and message[12] == 0x00  # Slot number
            )


def numberOfBanks():
    return 4


def numberOfPatchesPerBank():
    return 100


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return wrapSysex([0x41, 0x00, 0x00, 0x00, bank+1, program])


def isSingleProgramDump(message):
    return (len(message) > 13
            and message[0] == 0xf0
            and message[1] == 0x00  # Novation
            and message[2] == 0x20  # Novation ID1
            and message[3] == 0x29  # Novation ID2
            and message[4] == 0x01
            and message[5] == 0x40
            and message[6] == 0x7f
            and message[7] in [0x00, 0x01]
            #and message[8] == 0x00  # ?
            #and message[9] == 0x10  # ?
            #and message[10] == 0x00  # ?
            and 0x00 <= message[11] <= numberOfBanks()  # Bank number [1-4] (can be 0 for an unknown reason...)
            and 0x00 <= message[12] < numberOfPatchesPerBank()  # Slot number [0-127]
            and not message[7] == message[11] == 0  # make sure this is NOT an edit buffer
            )


def numberFromDump(message):
    if not isSingleProgramDump(message):
        return 0
    bank = message[11]
    bank = bank - 1 if bank > 0 else 0  # some program patch have bank == 0
    program = message[12]
    return bank * numberOfPatchesPerBank() + program


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        # Clear Bank / Slot from message
        return message[:11] + [0, 0] + message[13:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return message[:11] + [bank+1, program] + message[13:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def friendlyBankName(bank_number):
    return f"{bank_number + 1}"


def friendlyProgramName(program):
    return "%s%02d" % (friendlyBankName(program // numberOfPatchesPerBank()), program % numberOfPatchesPerBank())


def make_test_data():

    def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        data = "f0 00 20 29 01 40 7f 00 00 11 02 00 00 30 2c 00 00 40 2e 15 40 40 42 40 40 4e 56 40 40 40 42 40 40 40 2f 40 40 47 42 3f 40 40 40 40 40 40 40 49 40 40 00 00 00 00 7f 00 00 37 29 7f 56 40 40 40 45 55 40 40 40 00 40 00 00 3c 40 00 54 7f 57 6b 2a 00 6e 7d 00 41 00 00 29 00 00 03 00 0f 40 40 40 38 03 40 00 00 00 00 00 71 74 1e 00 00 00 40 64 00 40 3e 00 40 7f 00 24 40 28 7b 40 10 00 4a 53 40 40 14 00 00 0d 00 00 00 49 00 55 f7"
        yield testing.ProgramTestData(message=data)

    def make_programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        data = "f0 00 20 29 01 40 7f 00 00 11 02 00 00 30 2c 00 00 40 2e 15 40 40 42 40 40 4e 56 40 40 40 42 40 40 40 2f 40 40 47 42 3f 40 40 40 40 40 40 40 49 40 40 00 00 00 00 7f 00 00 37 29 7f 56 40 40 40 45 55 40 40 40 00 40 00 00 3c 40 00 54 7f 57 6b 2a 00 6e 7d 00 41 00 00 29 00 00 03 00 0f 40 40 40 38 03 40 00 00 00 00 00 71 74 1e 00 00 00 40 64 00 40 3e 00 40 7f 00 24 40 28 7b 40 10 00 4a 53 40 40 14 00 00 0d 00 00 00 49 00 55 f7"
        edit_buffer = testing.ProgramTestData(message=data, number=35)
        prog = convertToProgramDump(0, edit_buffer.message.byte_list, 35)
        yield testing.ProgramTestData(message=prog, number=35, friendly_number="135")

    return testing.TestData(edit_buffer_generator=make_patches, program_generator=make_programs, friendly_bank_name=(2, '3'))
