#
#   Copyright (c) 2021-2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib

systemSettingsAddress = (0x00, 0x00, 0x00)
bulkHeaderAddress = (0x0e, 0x0f, 0x00)
bulkFooterAddress = (0x0f, 0x0f, 0x00)
commonVoiceAddress = (0x30, 0x00, 0x00)
legacyDataLength = 38 + 4 * 28  # The old format stored only the required bytes, none of the sysex stuff


def name():
    # This is the same name as the C++ implementation, so it'd better be compatible!
    return "Yamaha Reface DX adaption"


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
            channel = data_block[0]
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
    elif isEditBufferDump(message):
        # Actually, those are multiple messages
        messages = splitSysexMessage(message)
        # The second of which should be common message
        if isCommonVoice(messages[1]):
            # The first 10 bytes of the data block of the common message are the name
            # The data block contains the 3 byte address at its head, skip those
            return "".join([chr(x) for x in dataBlockFromMessage(messages[1])[3:13]])
    return "Invalid"


def renamePatch(message, new_name):
    if isLegacyFormat(message):
        message = convertFromLegacyFormat(0, message)
    messages = splitSysexMessage(message)
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
    # Accept a certain set of addresses
    return isBulkHeader(message) or isBulkFooter(message) or isCommonVoice(message) or isOperator(message)


def isEditBufferDump(data):
    messages = splitSysexMessage(data)
    headers = sum([1 if isBulkHeader(m) else 0 for m in messages])
    footers = sum([1 if isBulkFooter(m) else 0 for m in messages])
    common = sum([1 if isCommonVoice(m) else 0 for m in messages])
    operators = sum([1 if isOperator(m) else 0 for m in messages])

    return headers == 1 and footers == 1 and common == 1 and operators == 4


def convertToEditBuffer(channel, data):
    result = []
    if isLegacyFormat(data):
        return convertFromLegacyFormat(channel, data)
    elif isEditBufferDump(data):
        messages = splitSysexMessage(data)
        for message in messages:
            # Recompose the message with the new channel in the lower 4 bits
            result.extend(changeChannelInMessage(channel, message))
        return result
    raise Exception("Not an edit buffer dump!")


def isLegacyFormat(message):
    return len(message) == legacyDataLength


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
    messages = splitSysexMessage(data)
    for message in messages:
        address = addressFromMessage(message)
        if address != bulkFooterAddress and address != bulkHeaderAddress:
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
    return f"Bank{bank}-{patch}"


def addressFromMessage(message):
    if isOwnSysex(message) and len(message) > 10:
        return message[8], message[9], message[10]
    raise Exception("Got invalid data block")


def isBulkHeader(message):
    return isOwnSysex(message) and addressFromMessage(message) == bulkHeaderAddress


def isBulkFooter(message):
    return isOwnSysex(message) and addressFromMessage(message) == bulkFooterAddress


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
            data_block = message[8:-2]
            check_sum = -0x05
            for d in data_block:
                check_sum -= d
            if (check_sum & 0x7f) == message[-2]:
                # Check sum passed
                return data_block
            else:
                raise Exception("Checksum error in reface DX data")
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


def splitSysexMessage(messages):
    result = []
    start = 0
    read = 0
    while read < len(messages):
        if messages[read] == 0xf0:
            start = read
        elif messages[read] == 0xf7:
            result.append(messages[start:read + 1])
        read = read + 1
    return result


def run_tests():
    with open("testData/refaceDX-00-Piano_1___.syx", "rb") as sysex:
        raw_data = list(sysex.read())
        data = splitSysexMessage(raw_data)
        for d in data:
            assert isPartOfEditBufferDump(d)
        block = dataBlockFromMessage(data[1])
        assert nameFromDump(raw_data) == "Piano 1   "
        legacy_format = convertToLegacyFormat(raw_data)
        back_to_normal = convertFromLegacyFormat(0, legacy_format)
        assert isEditBufferDump(back_to_normal)
        assert calculateFingerprint(back_to_normal) == calculateFingerprint(raw_data)
        assert back_to_normal == raw_data
        assert convertToEditBuffer(0, raw_data) == raw_data
        assert convertToEditBuffer(0, legacy_format) == raw_data

        same_patch = renamePatch(raw_data, "Piano 1")
        assert same_patch == raw_data
        new_patch = renamePatch(raw_data, "Piano 2")
        assert(nameFromDump(new_patch)) == "Piano 2   "


if __name__ == "__main__":
    run_tests()
