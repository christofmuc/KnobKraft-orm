#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#   Novation Mini/Ultranova Adaptation version 0.6 by Cedric Tessier, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
# based on my own (partial) reverse engineering of Novation UltraNova librarian.
#
import hashlib
from typing import List

import testing

NAME_OFFSET = 15
NAME_LEN = 16
NAME_SPECIALCHARSET = ' !"#$%&\'()*+,-./:;<=>?@[]^_`{|}'


def setupHelp():
    return '''\
Make sure the synth is not in protected mode (Global -> Protect -> Off).
'''


def wrapSysex(data):
    return [0xf0,  # Sysex message
            0x00, 0x20, 0x29,  # Novation
            0x03,  # Product type?
            0x01,  # ?
            0x7f   # ?
            ] + data + [0xf7]  # EOX


def name():
    return 'Novation UltraNova'


def isDefaultName(patchName):
    return patchName == 'Init Patch'


def createDeviceDetectMessage(channel):
    return wrapSysex([0x60, 0x21, 0x00, 0x00, 0x00, 0x00])


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
            and message[2] == 0x20
            and message[3] == 0x29
            and message[4] == 0x03  # ?
            and message[5] == 0x01  # ?
            and message[6] == 0x7f  # ?
            and message[7] == 0x20):
        # TODO: message[11] is protection status
        # Extract midi channel from message
        return message[13] & 0xf
    return -1


def createEditBufferRequest(channel):
    return wrapSysex([0x40, 0x21, 0x00, 0x00, 0x00, 0x00])


def isEditBufferDump(message):
    return (len(message) > 13
            and message[0] == 0xf0
            and message[1] == 0x00  # Novation
            and message[2] == 0x20
            and message[3] == 0x29
            and message[4] == 0x03
            and message[5] == 0x01
            and message[6] == 0x7f
            and message[7] == 0x00  # ?
            # and message[8] == 0x00  # ?
            # and message[9] == 0x10  # ?
            # and message[10] == 0x00  # ?
            and message[11] == 0x00  # Bank number
            and message[12] == 0x00  # Slot number
            )


def numberOfBanks():
    return 4


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return wrapSysex([0x41, 0x00, 0x00, 0x00, bank+1, program])


def isSingleProgramDump(message):
    return (len(message) > 13
            and message[0] == 0xf0
            and message[1] == 0x00  # Novation
            and message[2] == 0x20
            and message[3] == 0x29
            and message[4] == 0x03
            and message[5] == 0x01
            and message[6] == 0x7f
            and message[7] in [0x00, 0x01]
            # and message[8] == 0x00  # ?
            # and message[9] == 0x10  # ?
            # and message[10] == 0x00  # ?
            and 0x00 <= message[11] <= numberOfBanks()  # Bank number [1-4] (can be 0 for an unknown reason...)
            and 0x00 <= message[12] < numberOfPatchesPerBank()  # Slot number [0-127]
            and not message[7] == message[11] == 0  # make sure this is NOT an edit buffer
            )


def numberFromDump(message):
    if not isSingleProgramDump(message):
        return -1
    bank = message[11]
    bank = bank - 1 if bank > 0 else 0  # some program patch have bank == 0
    program = message[12]
    return bank * numberOfPatchesPerBank() + program


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        name = ''.join([chr(x) for x in message[NAME_OFFSET:NAME_OFFSET+NAME_LEN]])
        return name.strip()
    return 'Invalid'


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message[:8] + [0] + message[9:]
    elif isSingleProgramDump(message):
        # Clear Bank / Slot from message
        return message[:7] + [0, 0] + message[9:11] + [0, 0] + message[13:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return message[:7] + [1, 0] + message[9:11] + [bank+1, program] + message[13:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def renamePatch(message, new_name):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        # name is alpha-num + special chars (replace everything else by '_' and pad with spaces)
        clean_name = new_name.strip()[:NAME_LEN].ljust(NAME_LEN, ' ')
        valid_name = [ord(x) if x.isalnum() or x in NAME_SPECIALCHARSET else ord('_') for x in clean_name]
        return message[:NAME_OFFSET] + valid_name + message[NAME_OFFSET+NAME_LEN:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def friendlyBankName(bank_number):
    bank = bank_number + 1
    return {1: 'A', 2: 'B', 3: 'C', 4: 'D'}.get(bank, '')


# TODO: current implementation is broken as 'program' is never above 128...
#def friendlyProgramName(program):
#    return "%s%d" % (friendlyBankName(program // numberOfPatchesPerBank()), program % numberOfPatchesPerBank())


def calculateFingerprint(message):
    # Skip sysex, some header bytes and patch name
    data = message[1:7] + message[NAME_OFFSET+NAME_LEN:-1]
    return hashlib.md5(bytearray(data)).hexdigest()


def make_test_data():

    def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        # Additional test for rename
        patch = test_data.all_messages[0]
        same = renamePatch(patch, "Cr4zy Name°$    overflow")
        assert nameFromDump(same) == "Cr4zy Name_$"

        yield testing.ProgramTestData(message=test_data.all_messages[0], name="Poppy", rename_name="Paraver", number=35)

    return testing.TestData(sysex=R"testData/Novation_Ultranova_poppy.syx", program_generator=make_patches, friendly_bank_name=(2, 'C'))
