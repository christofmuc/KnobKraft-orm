#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import itertools

EDIT_BUFFER_TYPE = 0
EDIT_BUFFER_STREAM = 1
BANK_DUMP_STREAM = 2


def name():
    return "Yamaha reface DX"


def createDeviceDetectMessage(channel):
    # Just send a request for the system settings address
    return buildRequest(channel, 0x00, 0x00, 0x00)


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if isOwnSysex(message):
        address = addressFromMessage(message)
        if address == (0x00, 0x00, 0x00):
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
    # Actually, those are multiple messages
    messages = splitSysexMessage(message)
    # The second of which should be common message
    if isCommonVoice(messages[1]):
        # The first 10 bytes of the data block of the common message are the name
        return "".join([chr(x) for x in dataBlockFromMessage(messages[1])[:10]])
    return "Invalid"


def dataTypeIDs():
    return {"Single": EDIT_BUFFER_TYPE}


def dataRequestIDs():
    return {"Edit Buffer": (EDIT_BUFFER_STREAM, 0), "Bank Dump": (BANK_DUMP_STREAM, 0)}


def createStreamRequest(channel, streamType, startIndex):
    if streamType == EDIT_BUFFER_STREAM:
        # Use the address of the bulk header, this will give us the current program
        return buildRequest(channel, 0x0e, 0x0f, 0x00)
    elif streamType == BANK_DUMP_STREAM:
        # Same thing, just prefix a program change message
        return [0xc0 | (channel & 0x0f), startIndex & 0x7f] + buildRequest(channel, 0x0e, 0x0f, 0x00)
    raise Exception("Invalid streamType given, only supporting EDIT_BUFFER_STREAM and BANK_DUMP_STREAM")


def messagesPerStreamType(streamType):
    if streamType == EDIT_BUFFER_STREAM:
        return 7
    elif streamType == BANK_DUMP_STREAM:
        return 32 * 7
    raise Exception("Invalid streamType given, only supporting EDIT_BUFFER_STREAM and BANK_DUMP_STREAM")


def isPartOfStream(streamType, message):
    # Accept a certain set of addresses
    if isOwnSysex(message):
        address = addressFromMessage(message)
        return (address == (0x0e, 0x0f, 0x00)  # Bulk header
                or address == (0x0f, 0x0f, 0x00)  # Bulk footer
                or address == (0x30, 0x00, 0x00)  # Common voice data
                or (address[0] == 0x31 and address[2] == 0x00))  # Operator data, ignore address mid
    return False


def isStreamComplete(streamType, messages):
    headers = sum([1 if isBulkHeader(m) else 0 for m in messages])
    footers = sum([1 if isBulkFooter(m) else 0 for m in messages])
    common = sum([1 if isCommonVoice(m) else 0 for m in messages])
    operators = sum([1 if isOperator(m) else 0 for m in messages])

    if streamType == EDIT_BUFFER_STREAM:
        return headers == 1 and footers == 1 and common == 1 and operators == 4
    elif streamType == BANK_DUMP_STREAM:
        return headers == 32 and footers == 32 and common == 32 and operators == 4 * 32
    else:
        raise Exception("Invalid stream type")


def shouldStreamAdvance(streamType, messages):
    headers = sum([1 if isBulkHeader(m) else 0 for m in messages])
    footers = sum([1 if isBulkFooter(m) else 0 for m in messages])
    # Only the bank dump stream needs to advance, the edit buffer stream is complete without issuing a new request
    return (headers == footers) and streamType == BANK_DUMP_STREAM


def loadStreamIntoPatches(streamType, messages):
    result = []
    expected = [lambda x: isBulkHeader(x),
                lambda x: isCommonVoice(x),
                lambda x: isOperator(x),
                lambda x: isOperator(x),
                lambda x: isOperator(x),
                lambda x: isOperator(x),
                lambda x: isBulkFooter(x)]
    i = 0
    while i < len(messages):
        # Are the next 7 messages what we expect?
        if all([expected[j](messages[i+j]) for j in range(7)]):
            # Concat the 7 lists into a single byte list with 7 MIDI messages
            result.append(list(itertools.chain(*messages[i:i+7])))
            i += 7
        else:
            print("Found additional message in reface DX dump")
            i += 1
    return result


def isOwnSysex(message):
    if (len(message) > 7
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x43  # Yamaha
            and message[3] == 0x7f  # Group high
            and message[4] == 0x1c  # Group low
            and message[7] == 0x05):  # Model reface DX
        return True
    return False


def addressFromMessage(message):
    if isOwnSysex(message) and len(message) > 10:
        return message[8], message[9], message[10]
    raise Exception("Got invalid data block")


def isBulkHeader(message):
    return isOwnSysex(message) and addressFromMessage(message) == (0x0e, 0x0f, 0x00)


def isBulkFooter(message):
    return isOwnSysex(message) and addressFromMessage(message) == (0x0f, 0x0f, 0x00)


def isCommonVoice(message):
    return isOwnSysex(message) and addressFromMessage(message) == (0x30, 0x00, 0x00)


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
            check_sum = 0
            for d in data_block:
                check_sum -= d
            if ((check_sum - 0x05) & 0x7f) == message[-2]:
                # Check sum passed
                return data_block
            else:
                raise Exception("Checksum error in reface DX data")
    raise Exception("Got corrupt data block in refaceDX data")


def buildRequest(channel, addressHigh, addressMid, addressLow):
    # 0x43 Yamaha
    # 0x20 Dump Request
    # 0x7f Group high
    # 0x1c Group low
    # 0x05 Model = reface DX
    return [0x43, 0x20 | (channel & 0x0f), 0x7f, 0x1c, 0x05, addressHigh, addressMid, addressLow]


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
    with open(R"D:\Christof\Music\YamahaRefaceDX\RefaceLegacy\Reface-DS8\00-Piano_1___.syx", "rb") as sysex:
        data = splitSysexMessage(list(sysex.read()))
        assert isPartOfStream(EDIT_BUFFER_STREAM, data[0])
        block = dataBlockFromMessage(data[0])
        patches = loadStreamIntoPatches(EDIT_BUFFER_STREAM, data)
        print(patches)
        print(nameFromDump(patches[0]))


if __name__ == "__main__":
    run_tests()
