#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Note that for real life usage the native C++ implementation of the Matrix1000 is more powerful and should be used
# This is an example adaption to show how a fully working adaption can look like
from typing import List

import testing


def name():
    return "Matrix 1000 Adaptation"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    # The Matrix 1000 answers with 15 bytes
    if (len(message) == 15
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x10  # Oberheim
            and message[6] == 0x06  # Matrix
            and message[7] == 0x00
            and message[8] == 0x02  # Family member Matrix 1000
            and message[9] == 0x00):
        # Extract the current MIDI channel from index 2 of the message
        return message[2]
    return -1


def createEditBufferRequest(channel):
    return [0xf0, 0x10, 0x06, 0x04, 4, 0, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x10  # Oberheim
            and message[2] == 0x06  # Matrix
            and message[3] == 0x0d)  # Single Program to Edit Buffer message


def bankDescriptors():
    banks = [{"bank": x, "name": bankName(x), "size": 100, "isROM": x >= 2, "type": "Patch"} for x in range(10)]
    return banks


def bankAndProgramFromPatchNumber(patchNo):
    return patchNo // 100, patchNo % 100


def createBankSelect(bank):
    return [0xf0, 0x10, 0x06, 0x0a, bank, 0xf7]


def createProgramDumpRequest(channel, patchNo):
    bank, program = bankAndProgramFromPatchNumber(patchNo)
    return createBankSelect(bank) + [0xf0, 0x10, 0x06, 0x04, 1, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x10  # Oberheim
            and message[2] == 0x06  # Matrix
            and message[3] == 0x01)  # Single Patch Data


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        # To extract the name from the Matrix 1000 program dump, we
        # need to correctly de-nibble and then force the first 8 bytes into ASCII
        patchData = denibble(message, 5, len(message) - 2)
        # The Matrix 6 stores only 6 bit of ASCII, folding the letters into the range 0 to 31
        return ''.join([chr(x if x >= 32 else x + 0x40) for x in patchData[0:8]])
    raise Exception("Neither edit buffer nor program dump")


def renamePatch(message, new_name):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        # The Matrix 1000 stores only 6 bit of ASCII, folding the letters into the range 0 to 31
        valid_name = [ord(x) if ord(x) < 0x60 else (ord(x) - 0x20) for x in new_name]
        new_name_nibbles = nibble([(valid_name[i] & 0x7f) if i < len(new_name) else 0x20 for i in range(8)])
        return rebuildChecksum(message[0:5] + new_name_nibbles + message[21:])
    raise Exception("Neither edit buffer nor program dump can't be converted")


def numberFromDump(message) -> int:
    if isSingleProgramDump(message):
        print(f"it is a single program dump: {message[4]}")
        return message[4]
    print(message)
    return -1


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    if isSingleProgramDump(message):
        # Both are "single patch data", but must be converted to "single patch data to edit buffer"
        return message[0:3] + [0x0d] + [0x00] + message[5:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def convertToProgramDump(channel, message, patchNo):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        bank, program = bankAndProgramFromPatchNumber(patchNo)
        # Variant 1: Send the edit buffer and then a store edit buffer message
        return convertToEditBuffer(channel, message) + [0xf0, 0x10, 0x06, 0x0e, bank, program, 0, 0xf7]
        # Variant 2: Send a bank select and then a SinglePatchData package
        # return createBankSelect(bank) + message[0:4] + [program] + message[5:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def bankName(bank_number):
    return "%03d-%03d" % (bank_number * 100, (bank_number + 1) * 100 - 1)


def rebuildChecksum(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        data = denibble(message, 5, len(message) - 2)
        checksum = sum(data) & 0x7f
        return message[:-2] + [checksum, 0xf7]
    raise Exception("rebuildChecksum only implemented for single patch data yet")


def denibble(message, start, stop):
    return [message[x] | (message[x + 1] << 4) for x in range(start, stop, 2)]


def nibble(message):
    result = []
    for b in message:
        result.append(b & 0x0f)
        result.append((b & 0xf0) >> 4)
    return result


# Test data picked up by test_adaptation.py
def make_test_data():
    import binascii

    def programs(data) -> List[testing.ProgramTestData]:
        patch_from_device = "f01006011002040e040b0402030a0300020103060302000c0000000901030001000c00000000000300020000000f0101000000010000000000040600000000010000000000020302000000080200000100060000000d000f030a03000000000000000009000f0300000000000000000f030d020f030000000000000000000000000f030d0204010000000000000f03080200000f030203080200000000090008020f000f010f020f03000000000000000000000000000000000f03000005000e0102030f030f03000000000000000000000000000000000000000001000f030b0003000f03040000000000000002000f030b000b000f030c000400010209000400020004000a00090c01000a000f03040057f7"
        patch_message = list(binascii.unhexlify(patch_from_device))
        yield testing.ProgramTestData(message=patch_message, name='BNK2: 16', number=1, rename_name="NEW NAME")

    # Matrix1000 only has uppercase letters, so we need to specify a rename target name for the test
    # Flag for the generic tests that converting a program dump to a program dump might yield a new result, as there are two different
    # ways to express a program dump and we cannot guarantee that this always is idempotent
    return testing.TestData(program_generator=programs, not_idempotent=True)
