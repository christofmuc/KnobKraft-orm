#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from typing import Optional, List, Tuple

# This is taken from the Monomachine Manual p. 147/C-1 Appendix C Sysex Reference
# and https://www.elektronauts.com/t/decoding-the-digitone-sysex/62731/90
import testing

ELEKTRON_ID = [0x00, 0x20, 0x3c]
ANALOG_FOUR_ID = 0x06
ANALOG_RYTM_ID = 0x07

AR_TYPE_KIT = 0
AR_TYPE_SOUND = 1
AR_TYPE_PATTERN = 2
AR_TYPE_SONG = 3
AR_TYPE_SETTINGS = 4
AR_TYPE_GLOBAL = 5

AR_SYSEX_DUMP_ID_BASE = 0x52
AR_SYSEX_DUMPX_ID_BASE = 0x58
AR_SYSEX_REQUEST_ID_BASE = 0x62
AR_SYSEX_REQUESTX_ID_BASE = 0x68

VERSION_HIGH = 0x01
VERSION_LOW = 0x01


def name():
    return "Elektron Analog Rytm"


def _createElektronMessage(device_id: int, command: int, data: Optional[List[int]]):
    return [0xf0] + ELEKTRON_ID + [ANALOG_RYTM_ID, 0x00 | (device_id & 0x0f), command] + data + [0xf7]


def _calc_request_id(patch_no: int, data_type: int):
    if patch_no > 127:
        # Edit buffer/work buffer
        return AR_SYSEX_REQUESTX_ID_BASE + data_type
    else:
        return AR_SYSEX_REQUEST_ID_BASE + data_type


def _createRequest(device_id: int, patch_no: int, data_type: int):
    return _createElektronMessage(device_id, _calc_request_id(patch_no, data_type),
                                  [VERSION_HIGH, VERSION_LOW, patch_no & 0x7f, 0x00, 0x00, 0x00, 0x05])


def bankDescriptors():
    # As a start, offer the bank of the 128 Kits only
    return [{"bank": 0, "name": "Kits", "size": 128, "type": "Kit"}]


def createDeviceDetectMessage(device_id):
    # Just send a global setting dump request and see if we get an answer
    return _createRequest(device_id, 0x00, AR_TYPE_SETTINGS)


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    if isOwnSysex(message) and len(message) > 6 and message[6] == AR_SYSEX_DUMP_ID_BASE + AR_TYPE_SETTINGS:
        return message[5] & 0x0f
    return -1


def createProgramDumpRequest(device_id: int, patch_no: int) -> List[int]:
    return _createRequest(device_id, patch_no, AR_TYPE_SOUND)


def isSingleProgramDump(message):
    return isOwnSysex(message) and len(message) > 6 and message[6] in [AR_SYSEX_DUMP_ID_BASE + AR_TYPE_SOUND]


def convertToSingleProgramDump(device_id: int, message: List[int], patch_no: int) -> List[int]:
    if isSingleProgramDump(message):
        return _createElektronMessage(device_id, AR_SYSEX_DUMP_ID_BASE + AR_TYPE_SOUND, [VERSION_HIGH, VERSION_LOW, patch_no & 0x7f] + message[10:-1])
    raise "Can only convert single program dumps"


def isOwnSysex(message):
    return (len(message) > 5
            and message[0] == 0xf0  # Sysex
            and message[1:4] == ELEKTRON_ID
            and message[4] == ANALOG_RYTM_ID)


def nameFromDump(message):
    if isSingleProgramDump(message):
        message_length = message[-3] << 7 | message[-2]
        if message_length + 10 != len(message):
            raise "Ignoring invalid sysex message with wrong length data"
        stored_checksum = message[-5] << 7 | message[-4]
        packed_data, checksum = unescapeSysexElektron(message[0x0a:-5])
        if checksum != stored_checksum:
            raise "Checksum error in patch"
        real_message = packed_data
        name = ""
        dataIndex = 12
        while real_message[dataIndex] != 0 and dataIndex < 12 + 16:
            name += chr(real_message[dataIndex])
            dataIndex += 1
        return name
    return "Invalid"


def renamePatch(message: List[int], new_name: str) -> List[int]:
    if isSingleProgramDump(message):
        data, checksum = unescapeSysexElektron(message[10:-5])
        data[12:12 + 16] = [ord(c) for c in new_name.ljust(16, chr(0x00))]
        escaped_data, new_checksum = escapeSysexElektron(data)
        checksum_hi = (new_checksum >> 7) & 0x7f
        checksum_lo = new_checksum & 0x7f
        return message[:10] + escaped_data + [checksum_hi, checksum_lo] + message[-3:]
    raise "Can only rename program dumps!"


def numberFromDump(message):
    if isSingleProgramDump(message):
        return message[9]
    raise "Can only extract number from sound dump"


def calculateFingerprint(message: List[int]) -> str:
    if isSingleProgramDump(message):
        data, checksum = unescapeSysexElektron(message[10:-5])
        # Blank out name, 16 characters stored as 14 bit values
        data[12:12+16] = [0] * 16
        return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint Presets")


def unescapeSysexElektron(sysex: List[int]) -> Tuple[List[int], int]:
    result = []
    chksum = 0
    data_index = 0

    while data_index < len(sysex):
        msbits = sysex[data_index]
        chksum += msbits
        data_index += 1

        for i in range(7):
            if data_index < len(sysex):
                byte = sysex[data_index] | ((msbits & (1 << i)) << (7 - i))
                result.append(byte)
                chksum += sysex[data_index]
                data_index += 1
    return result, chksum


def escapeSysexElektron(data: List[int]) -> Tuple[List[int], int]:
    result = []
    chksum = 0
    data_index = 0

    while data_index < len(data):
        ms_bits = 0

        for i in range(7):
            if data_index + i < len(data):
                ms_bits |= ((data[data_index + i] & 0x80) >> 7) << i
        result.append(ms_bits)
        chksum += ms_bits
        for i in range(7):
            if data_index < len(data):
                result.append(data[data_index] & 0x7f)
                chksum += result[-1]
                data_index += 1
    return result, chksum


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        for message in data.all_messages:
            print(nameFromDump(message))
        # This bank has no patch numbers, they are all 0
        yield testing.ProgramTestData(message=data.all_messages[0], name="909 TRIANGLE", number=0)
        yield testing.ProgramTestData(message=data.all_messages[5], name="CLASSIC 909 KIC", number=0)

    return testing.TestData(sysex="testData/Elektron_AnalogRytm/909kicks.syx", program_generator=programs)
