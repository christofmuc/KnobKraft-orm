#
#   Copyright (c) 2021-2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib

# TODO - the Evolver has also a Waveshape Dump, that no other Sequential synth has. We need to support this
# TODO - It also has the peculiar NameDataDump, as the name of the patch is not included in the patch data itself
# TODO - currently all patches are listed with name "invalid"

evolver_config = {
    'name': 'DSI Evolver',
    'device_id': 0b00100000,
    'banks': 4,
    'patches_per_bank': 128,
    'file_version': 0x01,
    'name_position': 235,
    # Assuming that the data is first 228 bytes of program data dump, followed by 24 bytes program name data dump
    'name_len': 16
}


def name():
    return evolver_config['name']


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 100


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 9
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == 0x01  # Sequential / Dave Smith Instruments
            and message[6] == evolver_config['device_id']):
        # Family seems to be different, the Prophet 12 has (0x01, 0x00, 0x00) while the Evolver has (0, 0, 0)
        # and message[7] == 0x01  # Family MS is 1
        # and message[8] == 0x00  # Family member
        # and message[9] == 0x00):  # Family member
        # Extract the current MIDI channel from index 2 of the message
        if message[2] == 0x7f:
            # If the device is set to OMNI it will return 0x7f as MIDI channel - we use 1 for now which will work
            return 1
        elif message[2] == 0x10:
            # This indicates the device is in MPE mode (currently Prophet-6, OB-6),
            # and allocates channel 0-5 to the six voices
            return 0
        return message[2]
    return -1


def createEditBufferRequest(channel):
    # Evolver style. We will not issue a request program name for the edit buffer, only an edit buffer dump request
    return _createRequest(channel, 0b00000110)


def isEditBufferDump(message):
    return (len(message) > 4
            and isOwnSysex(message)
            and message[4] == 0b00000011)  # Edit Buffer Data


def numberOfBanks():
    return evolver_config['banks']


def numberOfPatchesPerBank():
    return evolver_config['patches_per_bank']


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    # Evolver style - request both the program dump and the program name dump in one go
    return _createRequest(channel, 0b00000101, bank, program) + _createProgramNameRequest(channel, patchNo)


def _createProgramNameRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    # Evolver style
    return _createRequest(channel, 0b00010000, bank, program)


def isOwnSysex(message):
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == 0x01  # Sequential
            and message[2] == evolver_config['device_id']
            and message[3] == evolver_config['file_version'])


def _createRequest(channel, request, bank=None, program=None):
    # Evolver style. We will not issue a request program name for the edit buffer
    if bank is None or program is None:
        return [0xf0, 0x01, evolver_config['device_id'], evolver_config['file_version'], request, 0xf7]
    else:
        return [0xf0, 0x01, evolver_config['device_id'], evolver_config['file_version'], request, bank, program, 0xf7]


def isSingleProgramDump(messages):
    # This is either a single program dump messages, then without name, ot a pair of program dump and name dump
    return (_isProgramDump(messages) and len(messages) == 228) or _isPairOfProgramDumpAndNameDump(messages)


def _isPairOfProgramDumpAndNameDump(messages):
    if len(messages) == 228 + 24:
        return _isProgramDump(messages[:228]) and _isProgramNameDump(messages[228:])
    return False


def _splitProgramDumpAndNameDump(messages):
    if _isPairOfProgramDumpAndNameDump(messages):
        return messages[:228], messages[228:]
    raise Exception("Should call this only for valid pairs of program dump and name dump")


def isPartOfSingleProgramDump(message):
    return _isProgramNameDump(message) or _isProgramDump(message)


def _isProgramNameDump(message):
    return (len(message) > 4
            and isOwnSysex(message)
            and message[4] == 0b00010001)  # Program Name Data


def _isProgramDump(message):
    return (len(message) > 4
            and isOwnSysex(message)
            and message[4] == 0b00000010)  # Program Data


def nameFromDump(message):
    if isEditBufferDump(message):
        return "Edit Buffer"
    if isSingleProgramDump(message):
        if _isPairOfProgramDumpAndNameDump(message):
            return ''.join(
                [chr(x) for x in
                 message[evolver_config['name_position']:evolver_config['name_position'] + evolver_config[
                     'name_len']]]).strip()
        else:
            return "Unknown"
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        # Have to strip out bank and program, and set command to edit buffer dump. Also, dump program name dump
        program_dump = message[:228]
        return program_dump[0:3 + _extraOffset()] + [0b00000011] + program_dump[6 + _extraOffset():]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message):
        return message[0:3 + _extraOffset()] + [0b00000010] + [bank, program] + message[4 + _extraOffset():]
    elif _isPairOfProgramDumpAndNameDump(message):
        program_dump, name_dump = _splitProgramDumpAndNameDump(message)
        return program_dump[0:3 + _extraOffset()] + [0b00000010] + [bank, program] + program_dump[6 + _extraOffset():] + \
               name_dump[0:3 + _extraOffset()] + [0b00010001] + [bank, program] + name_dump[6 + _extraOffset():]
    elif isSingleProgramDump(message):
        return message[0:3 + _extraOffset()] + [0b00000010] + [bank, program] + message[6 + _extraOffset():]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def calculateFingerprint(message):
    data = _getDataBlock(message[:228])
    return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data


# def renamePatch(message, new_name):
#    header_len = _headerLen(message)
#    data = _unescapeSysex(message[header_len:-1])
#    for i in range(evolver_config['name_len']):
#        data[evolver_config['name_position'] + i] = ord(new_name[i]) if i < len(new_name) else ord(' ')
#    return message[:header_len] + _escapeSysex(data) + [0xf7]


def _getDataBlock(message):
    return message[_headerLen(message):-1]


def _headerLen(message):
    if isEditBufferDump(message):
        return 4 + _extraOffset()
    elif isSingleProgramDump(message):
        return 6 + _extraOffset()
    else:
        raise Exception("Can only work on edit buffer or single program dumps")


def _extraOffset():
    return 0 if evolver_config['file_version'] is None else 1


def _unescapeSysex(sysex):
    result = []
    dataIndex = 0
    while dataIndex < len(sysex):
        msbits = sysex[dataIndex]
        dataIndex += 1
        for i in range(7):
            if dataIndex < len(sysex):
                result.append(sysex[dataIndex] | ((msbits & (1 << i)) << (7 - i)))
            dataIndex += 1
    return result


def _escapeSysex(data):
    result = []
    dataIndex = 0
    while dataIndex < len(data):
        ms_bits = 0
        for i in range(7):
            if dataIndex + i < len(data):
                ms_bits = ms_bits | ((data[dataIndex + i] & 0x80) >> (7 - i))
        result.append(ms_bits)
        for i in range(7):
            if dataIndex + i < len(data):
                result.append(data[dataIndex + i] & 0x7f)
        dataIndex += 7
    return result


def _splitSysexMessage(messages):
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


if __name__ == "__main__":
    pass
    #unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(sys.modules[__name__]))
