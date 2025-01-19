#
#   Copyright (c) 2024 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

from typing import List, Tuple, Dict

Kawai_ID = 0x40
Kawai_K4_ID = 0x04
Function_one_patch_data_request = 0x00
Function_block_patch_data_request = 0x01
Function_all_patch_data_request = 0x01
Function_parameter_send = 0x10
Function_one_patch_data_dump = 0x20
Function_block_data_dump = 0x21
Function_all_block_data_dump = 0x22
Function_edit_buffer_dump = 0x23  # Can only be sent, not requested
Function_program_change = 0x30
Function_write_complete = 0x40
Function_write_error = 0x41
Function_write_error_protected = 0x42
Function_write_error_nocard = 0x43


def name():
    return "Kawai K4/K4r"


def setupHelp():
    return "*************************\n" \
           "Check the following parameters on your keyboard:\n" \
           "1. Internal Memory Protect = OFF\n" \
           "2. MIDI Receive Channel (in SYSTEM) = 1\n" \
           "(use channel 10 for XD-5)\n" \
           "3. Receive Exclusive (also in SYSTEM) = ON\n" \
           "*************************"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    # The Kawai K4 answers
    if (len(message) == 15
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == Kawai_ID
            and message[6] == 0x00
            and message[7] == 0x00
            and message[8] == 0x04  # Family member Kawai K4
            and message[9] == 0x00):
        # Extract the current MIDI channel from index 2 of the message
        return message[2]
    return -1


def buildKawaiK4Message(channel, function, sub, data):
    return [0xf0, Kawai_ID, channel & 0x0f, function & 0x7f, 0x00, Kawai_K4_ID, sub[0], sub[1]] + data + [0xf7]


def createEditBufferRequest(channel):
    # Not implemented - the Kawai K4 can not be requested to send its edit buffer
    return []


def isOwnSysex(message) -> bool:
    return (len(message) > 3
            and message[0] == 0xf0
            and message[1] == Kawai_ID
            and message[4:5] == [0x00, Kawai_K4_ID]
            )


def functionFromMessage(message) -> int:
    return message[3]


def subfunctionFromMessage(message) -> Tuple[int, int]:
    return message[6], message[7]


def isEditBufferDump(message) -> bool:
    return isOwnSysex(message) and functionFromMessage(message) == Function_edit_buffer_dump


def bankDescriptors() -> List[Dict]:
    banks = [{"bank": 0, "name": "Internal Single", "size": 64, "isROM": False, "type": "Single"},
             {"bank": 1, "name": "Internal Multi", "size": 64, "isROM": False, "type": "Multi"},
             {"bank": 2, "name": "External Single", "size": 64, "isROM": False, "type": "Single"},
             {"bank": 3, "name": "External Multi", "size": 64, "isROM": False, "type": "Multi"}]
    return banks


def isSingleAndNotMulti(patchNo):
    return patchNo < 64 or (128 <= patchNo < 128 + 64)


def isInternalAndNotExternal(patchNo):
    return patchNo < 128


def createProgramDumpRequest(channel, patchNo) -> List[int]:
    internal_or_external = 0x00 if isInternalAndNotExternal(patchNo) else 0x02
    return buildKawaiK4Message(channel, function=Function_one_patch_data_request, sub=(internal_or_external, patchNo % 128), data=[])


def isSingleProgramDump(message) -> bool:
    return isOwnSysex(message) and functionFromMessage(message) == Function_one_patch_data_dump


def nameFromDump(message) -> str:
    if isSingleProgramDump(message):
        patchData = message[8:-1]
        return ''.join([chr(x) for x in patchData[0:10]])
    raise Exception("Not a program dump")


def numberFromDump(message) -> int:
    if isSingleProgramDump(message):
        return message[7]
    raise Exception("Can extract number only from single program dump messages")


def friendlyProgramName(program_no):
    suffix = "S" if isSingleAndNotMulti(program_no) else "M"
    slot = program_no % 64
    return f"{chr(ord('A') + slot // 16)}-{(slot % 16) + 1}{suffix}"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return buildKawaiK4Message(channel, function=Function_edit_buffer_dump, sub=(message[6], message[7]), data=message[8:-1])
    elif isSingleProgramDump(message):
        internal_or_external = message[6]
        program_no = message[7]
        return buildKawaiK4Message(channel, function=Function_edit_buffer_dump, sub=(0x00, 0x00 if isSingleAndNotMulti(program_no) else 0x40), data=message[8:-1])
    raise Exception("Neither edit buffer nor program dump can't be converted")


def convertToProgramDump(channel, message, patchNo):
    internal_or_external = 0x00 if isInternalAndNotExternal(patchNo) else 0x02
    if isEditBufferDump(message):
        return buildKawaiK4Message(channel, function=Function_one_patch_data_dump, sub=(internal_or_external, patchNo%128), data=message[8:-1])
    elif isSingleProgramDump(message):
        return buildKawaiK4Message(channel, function=Function_one_patch_data_dump, sub=(internal_or_external, patchNo%128), data=message[8:-1])
    raise Exception("Neither edit buffer nor program dump can't be converted")

# Test data picked up by test_adaptation.py
# def make_test_data():
#    import binascii

#    def programs(data) -> List[testing.ProgramTestData]:
#        patch_from_device = "f01006011002040e040b0402030a0300020103060302000c0000000901030001000c00000000000300020000000f0101000000010000000000040600000000010000000000020302000000080200000100060000000d000f030a03000000000000000009000f0300000000000000000f030d020f030000000000000000000000000f030d0204010000000000000f03080200000f030203080200000000090008020f000f010f020f03000000000000000000000000000000000f03000005000e0102030f030f03000000000000000000000000000000000000000001000f030b0003000f03040000000000000002000f030b000b000f030c000400010209000400020004000a00090c01000a000f03040057f7"
#        patch_message = list(binascii.unhexlify(patch_from_device))
#        yield testing.ProgramTestData(message=patch_message, name='BNK2: 16', number=16, rename_name="NEW NAME")

# Matrix1000 only has uppercase letters, so we need to specify a rename target name for the test
# Flag for the generic tests that converting a program dump to a program dump might yield a new result, as there are two different
# ways to express a program dump and we cannot guarantee that this always is idempotent
#    return testing.TestData(program_generator=programs, not_idempotent=True)
