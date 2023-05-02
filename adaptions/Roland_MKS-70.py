#
#   Copyright (c) 2020-2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Note that for real life usage the native C++ implementation of the Matrix1000 is more powerful and should be used
# This is an example adaption to show how a fully working adaption can look like

import hashlib


def name():
    return "Roland MKS-70"


def createDeviceDetectMessage(channel):
    return [0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7]


def deviceDetectWaitMilliseconds():
    # Negative value means don't autodetect at all
    return -1


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    if len(message) >= 7:
        # Check if the message is an Identity Reply from MKS-70
        if (message[0] == 0x7E and
                message[2] == 0x06 and
                message[3] == 0x02 and
                message[4] == 0x41):
            # Extract the Device ID from the message
            device_id = message[1]
            # Convert the Device ID to the corresponding MIDI channel
            channel = device_id - 0x10
            return channel

    # If the message does not match the expected format, return -1
    return -1


def createEditBufferRequest(channel):
    return [0xF0, 0x41, 0x10, 0x16, 0x11, 0x00, 0x00, 0x00, 0x00, 0xF7]


def isEditBufferDump(message):
    if len(message) >= 8:
        # Check if the message matches the expected format of the MKS-70's edit buffer dump
        if (message[0] == 0xF0 and
                message[1] == 0x41 and
                message[3] == 0x00 and
                message[4] == 0x24 and
                message[5] == 0x20):
            return True

    # If the message does not match the expected format, return False
    return False



def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 64


def nameFromDump(message):
    if isPartOfBankDump(message):
        # Extract the patch name bytes from the message
        patch_name_bytes = message[8:18]

        # Convert the patch name bytes to an ASCII string
        patch_name = ''.join(chr(byte) for byte in patch_name_bytes)
        return patch_name

    if isSingleProgramDump(message):
        patch_name_bytes = message[7:17]
        patch_name = ''.join(chr(byte) for byte in patch_name_bytes)
        return patch_name

    # If the message is not a program dump or part of a bank dump, return an empty string
    return ''


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        # Extract the data bytes from the message
        data_bytes = message[7:-2]

        # Convert the data bytes to a list of integers
        edit_buffer = [byte for byte in data_bytes]

        return edit_buffer

    # If the message is not an edit buffer dump, return an empty list
    return []


def createProgramDumpRequest(channel, patchNo):
    # Ensure channel is in the range of 0-15
    channel = channel & 0x0F

    # Ensure patchNo is in the range of 0-63 (0x00-0x3F)
    patchNo = patchNo & 0x3F

    # Create the Program Dump Request SysEx message
    program_dump_request = [0xF0, 0x41, 0x10 | channel, 0x16, 0x11, 0x00, 0x00, patchNo, 0xF7]

    return program_dump_request


def isSingleProgramDump(message):
    if len(message) >= 8:
        # Check if the message matches the expected format of the MKS-70's single program dump
        if (message[0] == 0xF0 and
                message[1] == 0x41 and
                message[3] == 0x00 and
                message[4] == 0x24 and
                message[5] == 0x20):
            return True

    # If the message does not match the expected format, return False
    return False


def createBankDumpRequest(channel, bank):
    # Ensure channel is in the range of 0-15
    channel = channel & 0x0F

    # Ensure bank is in the range of 0-1
    bank = bank & 0x01

    # Create the Bank Dump Request SysEx message
    bank_dump_request = [0xF0, 0x41, 0x10 | channel, 0x16, 0x11, 0x00, 0x01, bank, 0xF7]

    return bank_dump_request


def isPartOfBankDump(message):
    return (len(message) >= 8
            # Check if the message matches the expected format of the MKS-70's bank dump
            and message[0] == 0xF0
            and message[1] == 0x41
            and message[3] == 0x00
            and message[4] == 0x24
            and message[5] == 0x30
            )


def isBankDumpFinished(messages):
    if messages and isPartOfBankDump(messages[-1]):
        # Check if the last message in the list is the last part of the bank dump
        if messages[-1][5] == 0x3F:  # 0x3F is the last patch number in a bank
            return True

    return False


def extractPatchesFromBank(messages):
    patches = []
    patch_length = 72  # The length of each patch in bytes

    print("MS1: Input to extractPatchesFromBank:", " ".join(format(x, '02X') for x in messages))

    for message in messages:
        unescaped_message = unescapeSysex(message)
        if isPartOfBankDump(message):
            print("MS2: Input to isPartOfBankDump:", " ".join(format(x, '02X') for x in message))
            patch_data_start = 7  # Patch data starts after the header (7 bytes)
            patch_data_end = len(message) - 2  # Exclude checksum and F7 byte

            # Extract patches from the bank dump message
            for i in range(patch_data_start, patch_data_end, patch_length):
                patch_data = unescaped_message[i:i + patch_length]
                patches.append(patch_data)


def unescapeSysex(data):
    # A function to unescape the SysEx data
    result = []
    i = 0
    while i < len(data):
        if data[i] == 0xF7:
            break
        if data[i] & 0x80:
            result.append(((data[i] & 0x7F) << 7) | (data[i + 1] & 0x7F))
            i += 1
        else:
            result.append(data[i] & 0x7F)
        i += 1
    return result


def calculateFingerprint(message):
    if isSingleProgramDump(message) or isPartOfBankDump(message):
        data = unescapeSysex(message[5:-1])

        # You can blank out specific bytes if they should not matter for the fingerprint.
        # For example, if you want to blank out the patch name bytes (assuming they are at position 8-17):
        data[8:18] = [0] * 10

        # Calculate the fingerprint from the cleaned payload data
        fingerprint = hashlib.md5(bytearray(data)).hexdigest()
        return fingerprint

    # If the message is not a program dump or part of a bank dump, return an empty string
    return ''
