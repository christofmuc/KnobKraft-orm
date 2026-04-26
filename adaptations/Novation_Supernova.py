#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from pathlib import Path
from typing import List

import testing

NOVATION_ID = [0x00, 0x20, 0x29]
PRODUCT_TYPE = 0x01
REQUEST_MODEL_ID = 0x20
SUPPORTED_MODEL_IDS = {0x20, 0x21, 0x22, 0x23, 0x24}
UNIVERSAL_DEVICE_ID = 0x7F
REQUEST_COMMAND = 0x13
PROGRAM_DUMP_TYPE = 0x02
EDIT_BUFFER_TYPE = 0x00
EDIT_BUFFER_LOCATION = 0x09
PROGRAM_BANK_BASE = 0x05
NAME_LEN = 16
PROGRAM_NAME_OFFSET = 10
EDIT_BUFFER_NAME_OFFSET = 9
NAME_SPECIALCHARSET = " !\"#$%&'()*+,-./:;<=>?@[]^_`{|}"


def name():
    return "Novation Supernova"


def setupHelp():
    return "Set Global MIDI channel as needed, turn Memory Protect off, and set SysEx Reception to Normal (Rx as sent)."


def isDefaultName(patchName):
    return patchName == "Init Program"


def generalMessageDelay():
    return 150


def _wrap_request(model_id, device_id, command, location, program):
    return [0xF0] + NOVATION_ID + [PRODUCT_TYPE, model_id, device_id, command, location, program, 0xF7]


def _is_own_sysex(message):
    return (
        len(message) >= 9
        and message[0] == 0xF0
        and message[1:4] == NOVATION_ID
        and message[4] == PRODUCT_TYPE
        and message[5] in SUPPORTED_MODEL_IDS
        and message[-1] == 0xF7
    )


def _decode_name(name_bytes):
    return "".join(chr(x) if 32 <= x < 127 else " " for x in name_bytes).strip()


def _encode_name(new_name):
    clean_name = new_name.strip()[:NAME_LEN].ljust(NAME_LEN, " ")
    return [ord(x) if x.isalnum() or x in NAME_SPECIALCHARSET else ord("_") for x in clean_name]


def createDeviceDetectMessage(channel):
    return createEditBufferRequest(channel)


def deviceDetectWaitMilliseconds():
    return 300


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if isEditBufferDump(message) or isSingleProgramDump(message):
        if 0 <= message[6] < 16:
            return message[6]
        return 1
    return -1


def numberOfBanks():
    return 4


def numberOfPatchesPerBank():
    return 128


def friendlyBankName(bank_number):
    return chr(ord("A") + bank_number)


def friendlyProgramName(program):
    bank = program // numberOfPatchesPerBank()
    slot = program % numberOfPatchesPerBank()
    return f"{friendlyBankName(bank)}{slot:03d}"


def bankSelect(channel, bank):
    return [0xB0 | (channel & 0x0F), 32, PROGRAM_BANK_BASE + bank]


def createEditBufferRequest(channel):
    return _wrap_request(REQUEST_MODEL_ID, UNIVERSAL_DEVICE_ID, REQUEST_COMMAND, EDIT_BUFFER_LOCATION, 0x00)


def isEditBufferDump(message):
    return (
        _is_own_sysex(message)
        and len(message) == 296
        and message[7] == EDIT_BUFFER_TYPE
        and 0x00 <= message[8] <= EDIT_BUFFER_LOCATION
    )


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return _wrap_request(REQUEST_MODEL_ID, UNIVERSAL_DEVICE_ID, REQUEST_COMMAND, PROGRAM_BANK_BASE + bank, program)


def isSingleProgramDump(message):
    return (
        _is_own_sysex(message)
        and len(message) == 297
        and message[7] == PROGRAM_DUMP_TYPE
        and PROGRAM_BANK_BASE <= message[8] <= PROGRAM_BANK_BASE + 7
        and 0x00 <= message[9] < numberOfPatchesPerBank()
    )


def numberFromDump(message):
    if not isSingleProgramDump(message):
        return -1
    bank = message[8] - PROGRAM_BANK_BASE
    return bank * numberOfPatchesPerBank() + message[9]


def nameFromDump(message):
    if isSingleProgramDump(message):
        return _decode_name(message[PROGRAM_NAME_OFFSET:PROGRAM_NAME_OFFSET + NAME_LEN])
    if isEditBufferDump(message):
        return _decode_name(message[EDIT_BUFFER_NAME_OFFSET:EDIT_BUFFER_NAME_OFFSET + NAME_LEN])
    return "Invalid"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    if isSingleProgramDump(message):
        return message[:7] + [EDIT_BUFFER_TYPE, EDIT_BUFFER_LOCATION] + message[10:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isSingleProgramDump(message):
        return message[:7] + [PROGRAM_DUMP_TYPE, PROGRAM_BANK_BASE + bank, program] + message[10:]
    if isEditBufferDump(message):
        return message[:7] + [PROGRAM_DUMP_TYPE, PROGRAM_BANK_BASE + bank, program] + message[9:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def renamePatch(message, new_name):
    encoded_name = _encode_name(new_name)
    if isSingleProgramDump(message):
        return message[:PROGRAM_NAME_OFFSET] + encoded_name + message[PROGRAM_NAME_OFFSET + NAME_LEN:]
    if isEditBufferDump(message):
        return message[:EDIT_BUFFER_NAME_OFFSET] + encoded_name + message[EDIT_BUFFER_NAME_OFFSET + NAME_LEN:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def calculateFingerprint(message):
    if isSingleProgramDump(message):
        payload = message[PROGRAM_NAME_OFFSET:-1]
    elif isEditBufferDump(message):
        payload = message[EDIT_BUFFER_NAME_OFFSET:-1]
    else:
        raise Exception("Can't fingerprint unknown message")
    payload = list(payload)
    payload[:NAME_LEN] = [0] * NAME_LEN
    return hashlib.md5(bytearray(payload)).hexdigest()


def make_test_data():
    fixture_dir = Path(__file__).resolve().parent / "testData" / "Novation_Supernova"
    bank_fixture = fixture_dir / "nova preset banks a & b.syx"
    model20_edit_buffer = list((fixture_dir / "autechre.syx").read_bytes())

    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], name="Sintillator M-Wh", number=0, friendly_number="A000")
        yield testing.ProgramTestData(message=data.all_messages[128], name="Filtered H2O", number=128, friendly_number="B000")
        yield testing.ProgramTestData(message=data.all_messages[255], name="Init Program", number=255, friendly_number="B127")

    def edit_buffers(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=model20_edit_buffer, name="AUTECHRE", target_no=35)
        yield testing.ProgramTestData(message=convertToEditBuffer(0, data.all_messages[0]), name="Sintillator M-Wh", target_no=0)

    return testing.TestData(
        sysex=str(bank_fixture),
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        program_dump_request=(0, 0, createProgramDumpRequest(0, 0)),
        device_detect_call=createEditBufferRequest(0),
        device_detect_reply=(model20_edit_buffer, 11),
        friendly_bank_name=(2, "C"),
        rename_name="Cr4zy Name_$",
        expected_patch_count=256,
    )
