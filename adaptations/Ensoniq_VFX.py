#
#   Copyright (c) 2024 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from enum import IntEnum
from typing import List, Dict

import knobkraft
import testing

Ensoniq_ID = 0x0f


class MessageType(IntEnum):
    Command_Message_Type = 0x00
    Error_Message_Type = 0x01
    One_Program_Message_Type = 0x02
    All_Programs_Message_Type = 0x03
    One_Preset_Message_Type = 0x04
    All_Presets_Message_Type = 0x05
    Single_Sequence_Dump_Message_Type = 0x09
    All_Sequence_Dump_Message_Type = 0x0a
    Track_Parameters_Message_Type = 0x0b


class Command(IntEnum):
    Virtual_Button_Command = 0x00
    Parameter_Change_Command = 0x01
    Edit_Change_Status = 0x02  # Only transmitted by VFX
    Single_Program_Dump_Request = 0x05
    Single_Preset_Dump_Request = 0x06
    Track_Parameter_Dump_Request = 0x07
    Dump_Everything_Request = 0x08
    Internal_Program_Bank_Dump_Request = 0x09
    Internal_Preset_Bank_Dump_Request = 0x0a
    Single_Sequence_Dump = 0x0b
    All_Sequence_Memory_Dump = 0x0c
    Single_Sequence_Dump_Request = 0x0d
    All_Sequence_Dump_Request = 0x0e


class ErrorMessage(IntEnum):
    NAK_Message = 0x00
    Invalid_Parameter_Number_Message = 0x01
    Invalid_Parameter_Value_Message = 0x02
    Invalid_Button_Number_Message = 0x03
    ACK_Message = 0x04


def name():
    return "Ensoniq VFX"


def createDeviceDetectMessage(channel):
    # This is a sysex generic device detect message
    return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # The Ensoniq VFX answer
    if (len(message) > 10
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Non-realtime
            and message[3] == 0x06  # Device request
            and message[4] == 0x02  # Device request reply
            and message[5] == Ensoniq_ID
            and message[6] == 0x05  # VFX Family
            and message[7] == 0x00
            and message[8] == 0x01  # Family member VFX
            and message[9] == 0x00):
        # Extract the current MIDI channel from index 2 of the message
        return message[2]
    return -1


def buildMessage(channel, message_type: MessageType, data=None):
    return [0xf0, Ensoniq_ID, 0x05, 0x00, channel & 0x0f, message_type & 0x7f] + (data if data is not None else []) + [0xf7]


def createEditBufferRequest(channel):
    return buildMessage(channel, MessageType.Command_Message_Type, [Command.Single_Program_Dump_Request])


def isOwnSysex(message) -> bool:
    return len(message) > 4 and message[0:4] == [0xf0, Ensoniq_ID, 0x05, 0x00]


def messageTypeFromMessage(message) -> int:
    return message[5]


def commandFromCommandMessage(message) -> int:
    return message[6]


def isEditBufferDump(message) -> bool:
    return isOwnSysex(message) and messageTypeFromMessage(message) == MessageType.One_Program_Message_Type


def bankDescriptors() -> List[Dict]:
    banks = [{"bank": 0, "name": "Internal Programs", "size": 60, "isROM": False, "type": "Program"}]
    return banks


def denibble_hi_then_lo(message):
    return [message[x + 1] | (message[x] << 4) for x in range(0, len(message), 2)]


def nameFromDump(message) -> str:
    if isEditBufferDump(message):
        patchData = denibble_hi_then_lo(message[6:-1])
        return ''.join([chr(x) for x in patchData[498:498 + 11]])
    raise Exception("Not a program dump")


def friendlyProgramName(program_no):
    return f"{program_no}"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return buildMessage(channel, message_type=MessageType.One_Program_Message_Type, data=message[6:-1])
    raise Exception("Not an edit buffer, can't be converted")


def createBankDumpRequest(channel, bank):
    return buildMessage(channel, message_type=MessageType.Command_Message_Type, data=[Command.Internal_Program_Bank_Dump_Request])


def isPartOfBankDump(message):
    return isOwnSysex(message) and messageTypeFromMessage(message) == MessageType.All_Programs_Message_Type


def isBankDumpFinished(messages):
    for message in messages:
        if isPartOfBankDump(message):
            return True
    return False


def extractPatchesFromBank(message: List[int]) -> List[List[int]]:
    # A bank dump consists of 63607 bytes: 6 in the header, 63600 (in 60 programs of 1060 bytes, to be denibbled to 530), 1 in the footer.
    if isPartOfBankDump(message):
        channel = message[4]  # Surprisingly, the VFX has the channel after the synth model
        data = message[6:-1]
        result = []
        for data_start in range(0, len(data), 1060):
            # Read one more patch
            next_patch = data[data_start:data_start + 1060]
            next_program_dump = buildMessage(channel, MessageType.One_Program_Message_Type, data=next_patch)
            assert len(next_program_dump) == 1067
            print("Found patch " + nameFromDump(next_program_dump))
            result += next_program_dump
        return result


# Test data picked up by test_adaptation.py
def make_test_data():

    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        bank = data.all_messages[0]
        programs = knobkraft.splitSysex(extractPatchesFromBank(bank))
        yield testing.ProgramTestData(message=programs[0], name='ARTIC-ELATE', rename_name="NEW NAME")
        yield testing.ProgramTestData(message=programs[1], name='SAMPLE+HOLD', number=16, rename_name="NEW NAME")
        yield testing.ProgramTestData(message=programs[2], name=' FAT-BRASS ', number=16, rename_name="NEW NAME")

    return testing.TestData(edit_buffer_generator=programs, sysex="testData/Ensoniq_VFX/101.syx", expected_patch_count=60)
