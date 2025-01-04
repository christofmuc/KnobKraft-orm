#
#   Copyright (c) 2021-2024 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import itertools
from typing import List

import knobkraft
import testing

systemSettingsAddress = (0x00, 0x00, 0x00)
bulkHeaderAddress = (0x0e, 0x0f, 0x00)
bulkFooterAddress = (0x0f, 0x0f, 0x00)
bulkProgramHeaderAddress = (0x0e, 0x00, 0x00)  # https://coffeeshopped.com/patch-base/help/synth/yamaha/reface-dx
bulkProgramFooterAddress = (0x0f, 0x00, 0x00)
commonVoiceAddress = (0x30, 0x00, 0x00)
legacyDataLength = 38 + 4 * 28  # The old format stored only the required bytes, none of the sysex stuff


def name():
    # This is the same name as the C++ implementation, so it'd better be compatible!
    return "Yamaha Reface DX"


def createDeviceDetectMessage(channel):
    # Just send a request for the system settings address
    return buildRequest(channel, systemSettingsAddress)


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if isOwnSysex(message):
        address = addressFromMessage(message)
        if address == systemSettingsAddress:
            data_block = dataBlockFromMessage(message)
            channel = data_block[1]  # data_block[0] is the transmit channel
            if channel == 0x10:
                print("Warning: Your reface DX is set to OMNI MIDI channel. Surprises await!")
                return 1
            return channel
    return -1


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 32


def nameFromDump(message):
    if isLegacyFormat(message):
        # This is raw data - starting with the common voice block
        return "".join([chr(x) for x in message[0:10]])
    elif isEditBufferDump(message) or isSingleProgramDump(message):
        # Actually, those are multiple messages, find the second one
        messages = knobkraft.findSysexDelimiters(message, 2)
        # The second of which should be common message
        second_message = message[messages[1][0]:messages[1][1]]
        if isCommonVoice(second_message):
            # The first 10 bytes of the data block of the common message are the name
            # The data block contains the 3 byte address at its head, skip those
            return "".join([chr(x) for x in dataBlockFromMessage(second_message)[3:13]])
    return "Invalid"


def renamePatch(message, new_name):
    if isLegacyFormat(message):
        message = convertFromLegacyFormat(0, message)
    messages = knobkraft.splitSysex(message)
    common_voice_data = dataBlockFromMessage(messages[1])[3:]
    used_char = min(10, len(new_name))
    for i in range(used_char):
        common_voice_data[i] = ord(new_name[i])
    for i in range(used_char, 10):
        common_voice_data[i] = ord(" ")
    messages[1] = buildBulkDumpMessage(0, commonVoiceAddress, common_voice_data)
    return [item for sublist in messages for item in sublist]  # flatten again


def createEditBufferRequest(channel):
    # Use the address of the bulk header, this will give us the current program
    return buildRequest(channel, bulkHeaderAddress)


def isPartOfEditBufferDump(message):
    if isLegacyFormat(message):
        return True
    # Accept a certain set of addresses
    return isBulkHeader(message) or isBulkFooter(message) or isCommonVoice(message) or isOperator(message)


def isEditBufferDump(data):
    if isLegacyFormat(data):
        return True

    messages = knobkraft.splitSysex(data)
    headers = sum([1 if isBulkHeader(m) else 0 for m in messages])
    footers = sum([1 if isBulkFooter(m) else 0 for m in messages])
    common = sum([1 if isCommonVoice(m) else 0 for m in messages])
    operators = sum([1 if isOperator(m) else 0 for m in messages])

    return headers == 1 and footers == 1 and common == 1 and operators == 4


def convertToEditBuffer(channel, data):
    if isLegacyFormat(data):
        return convertFromLegacyFormat(channel, data)
    elif isEditBufferDump(data):
        result = []
        messages = knobkraft.splitSysex(data)
        for message in messages:
            # Recompose the message with the new channel in the lower 4 bits
            result.extend(changeChannelInMessage(channel, message))
        return result
    elif isSingleProgramDump(data):
        result = []
        messages = knobkraft.splitSysex(data)
        result.extend(buildBulkDumpMessage(channel, bulkHeaderAddress, []))
        for message in messages[1:6]:
            result.extend(message)
        result.extend(buildBulkDumpMessage(channel, bulkFooterAddress, []))
        return result
    raise Exception("Not an edit buffer dump!")


def isLegacyFormat(message):
    return len(message) == legacyDataLength


def isSingleProgramDump(data):
    if isLegacyFormat(data):
        return False

    messages = knobkraft.splitSysex(data)
    headers = sum([1 if isBulkProgramHeader(m) else 0 for m in messages])
    footers = sum([1 if isBulkProgramFooter(m) else 0 for m in messages])
    common = sum([1 if isCommonVoice(m) else 0 for m in messages])
    operators = sum([1 if isOperator(m) else 0 for m in messages])

    return headers == 1 and footers == 1 and common == 1 and operators == 4


def isPartOfSingleProgramDump(message):
    if isLegacyFormat(message):
        return False
    # Accept a certain set of addresses
    return isBulkProgramHeader(message) or isBulkProgramFooter(message) or isCommonVoice(message) or isOperator(message)


def convertToProgramDump(channel, message, program_number):
    if isLegacyFormat(message):
        message = convertFromLegacyFormat(channel, message)

    if isEditBufferDump(message) or isSingleProgramDump(message):
        messages = knobkraft.splitSysex(message)
        messages[0] = buildBulkDumpMessage(channel, (bulkProgramHeaderAddress[0], bulkProgramHeaderAddress[1], program_number), [])
        messages[6] = buildBulkDumpMessage(channel, (bulkProgramFooterAddress[0], bulkProgramFooterAddress[1], program_number), [])
        result = []
        for m in messages:
            # Skip non Sysex messages (program change)
            if m[0] == 0xf0:
                result.extend(m)
        return result + [0b11000000 | channel, program_number]
    raise Exception("Can only convert single program dumps to program dumps")


def createProgramDumpRequest(channel, patch_no):
    return [0b11000000 | channel, patch_no] + createEditBufferRequest(channel)


def bankDownloadMethodOverride():
    return "EDITBUFFERS"


def convertFromLegacyFormat(channel, legacy_data):
    # The legacy_data was produced by the C++ implementation, and is only the used data without all the bulk
    # Wrap this into the 7 MIDI messages required for a proper transfer
    result = []
    result.extend(buildBulkDumpMessage(channel, bulkHeaderAddress, []))
    result.extend(buildBulkDumpMessage(channel, commonVoiceAddress, legacy_data[0:38]))
    for op in range(4):
        result.extend(buildBulkDumpMessage(channel, (0x31, op, 0x00), legacy_data[38 + op * 28:38 + (op + 1) * 28]))
    result.extend(buildBulkDumpMessage(channel, bulkFooterAddress, []))
    return result


def convertToLegacyFormat(data):
    legacy_result = []
    messages = knobkraft.splitSysex(data)
    for message in messages:
        # Skip non sysex message (would be the program change message of a program dump)
        if message[0] != 0xf0:
            continue
        address = addressFromMessage(message)
        if address not in [bulkFooterAddress, bulkHeaderAddress, bulkProgramFooterAddress, bulkProgramHeaderAddress]:
            # Strip away the 3 address bytes, we rely on the correct order of messages (yikes)
            legacy_result.extend(dataBlockFromMessage(message)[3:])
    assert isLegacyFormat(legacy_result)
    return legacy_result


def buildBulkDumpMessage(deviceID, address, data):
    message = [0xf0, 0x43, 0x00 | deviceID, 0x7f, 0x1c, 0, 0, 0x05, address[0], address[1], address[2]] + data
    data_len = len(data) + 4  # Include address and checksum in data size
    size_high = (data_len >> 7) & 0x7f
    size_low = data_len & 0x7f
    message[5] = size_high
    message[6] = size_low
    check_sum = 0
    for m in message[7:]:
        check_sum -= m
    message.append(check_sum & 0x7f)
    message.append(0xf7)
    return message


def calculateFingerprint(message):
    # To be backwards compatible with the old C++ implementation of the Reface DX implementation,
    # calculate the fingerprint only across the data blocks, with a blocked out name (10 characters at the start)
    if not isLegacyFormat(message):
        message = convertToLegacyFormat(message)
    for i in range(10):
        message[i] = 0
    return hashlib.md5(bytearray(message)).hexdigest()


def isOwnSysex(message):
    if (len(message) > 7
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x43  # Yamaha
            and message[3] == 0x7f  # Group high
            and message[4] == 0x1c  # Group low
            and message[7] == 0x05):  # Model reface DX
        return True
    return False


def isDefaultName(patch_name):
    return patch_name == "Init Voice"


def friendlyBankName(bank):
    return "Banks 1-4"


def friendlyProgramName(programNo):
    bank = programNo // 8
    patch = programNo % 8
    return f"Bank{bank+1}-{patch+1}"


def numberFromDump(message):
    if isSingleProgramDump(message):
        # The program number is in the bulkprogramheader
        return message[10]
    raise Exception("Can only extract program number from SingleProgramDumps!")


def addressFromMessage(message):
    if isOwnSysex(message) and len(message) > 10:
        return message[8], message[9], message[10]
    raise Exception("Got invalid data block")


def isBulkHeader(message):
    return isOwnSysex(message) and addressFromMessage(message) == bulkHeaderAddress


def isBulkFooter(message):
    return isOwnSysex(message) and addressFromMessage(message) == bulkFooterAddress


def isBulkProgramHeader(message):
    # Only compare the first two bytes of the address, the third one is the program number
    return isOwnSysex(message) and addressFromMessage(message)[:2] == bulkProgramHeaderAddress[:2]


def isBulkProgramFooter(message):
    # Only compare the first two bytes of the address, the third one is the program number
    return isOwnSysex(message) and addressFromMessage(message)[:2] == bulkProgramFooterAddress[:2]


def isCommonVoice(message):
    return isOwnSysex(message) and addressFromMessage(message) == commonVoiceAddress


def isOperator(message):
    if isOwnSysex(message):
        address = addressFromMessage(message)
        return address[0] == 0x31 and address[2] == 0x00
    return False


def dataBlockFromMessage(message):
    if isOwnSysex(message):
        data_len = message[5] << 7 | message[6]
        if len(message) == data_len + 9:
            # The Check-sum is the value that results in a value of 0 for the
            # lower 7 bits when the Model ID, Start Address, Data and Check sum itself are added.
            checksum_block = message[0x07:-1]
            if (sum(checksum_block) & 0x7f) == 0:
                # return "Data" block
                return message[0x08:-2]
    raise Exception("Got corrupt data block in refaceDX data")


def buildRequest(channel, address):
    # 0x43 Yamaha
    # 0x20 Dump Request
    # 0x7f Group high
    # 0x1c Group low
    # 0x05 Model = reface DX
    return [0xf0, 0x43, 0x20 | (channel & 0x0f), 0x7f, 0x1c, 0x05, address[0], address[1], address[2]] + [0xf7]


def changeChannelInMessage(new_channel, message):
    return message[0:2] + [(message[2] & 0xf0) | (new_channel & 0x0f)] + message[3:]


def make_test_data():
    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        raw_data = list(itertools.chain.from_iterable(test_data.all_messages))
        for d in test_data.all_messages:
            assert isPartOfEditBufferDump(d)
        yield testing.ProgramTestData(message=raw_data, name="Piano 1   ", rename_name="Piano 2   ")

        # Test a Soundmondo file
        message = knobkraft.stringToSyx("F0 43 00 7F 1C 00 04 05 0E 0F 00 5E F7 F0 43 00 7F 1C 00 2A 05 30 00 00 53 6E 61 70 48 61 70 70 79 20 00 00 40 00 02 42 04 00 64 00 00 40 40 40 40 40 40 40 40 03 40 40 07 40 40 00 00 00 21 F7 F0 43 00 7F 1C 00 20 05 31 00 00 01 7F 3A 24 6E 7F 56 00 00 00 00 00 00 00 00 01 01 40 7D 3C 00 00 00 00 40 00 00 00 6E F7 F0 43 00 7F 1C 00 20 05 31 01 00 01 7F 5D 23 5A 7F 3C 00 00 04 0F 06 03 00 00 01 01 3F 70 48 00 00 00 00 40 00 00 00 5F F7 F0 43 00 7F 1C 00 20 05 31 02 00 01 7F 67 3C 5A 7F 60 00 00 04 00 0B 00 00 00 01 01 5C 62 4B 00 00 00 00 40 00 00 00 12 F7 F0 43 00 7F 1C 00 20 05 31 03 00 01 7F 4A 53 5A 7F 66 00 00 08 00 07 00 00 00 01 01 78 60 7F 00 00 00 00 40 00 00 00 43 F7 F0 43 00 7F 1C 00 04 05 0F 0F 00 5D F7")
        yield testing.ProgramTestData(message=message, name="SnapHappy ", rename_name="Piano 2   ")

    def program_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        raw_data = list(itertools.chain.from_iterable(test_data.all_messages))
        for d in test_data.all_messages:
            assert isPartOfEditBufferDump(d)
        program_dump = convertToProgramDump(1, raw_data, 17)
        yield testing.ProgramTestData(message=program_dump, name="Piano 1   ", rename_name="Piano 2   ", number=17, friendly_number="Bank3-2")

    return testing.TestData(sysex="testData/refaceDX-00-Piano_1___.syx", edit_buffer_generator=edit_buffers, program_generator=program_buffers)


