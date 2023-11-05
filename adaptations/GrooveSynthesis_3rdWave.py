#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import List

import testing

groove_synth_id = [0x0, 0x02, 0x4a]


def name():
    return "3rd Wave"


def createDeviceDetectMessage(channel):
    # Universal System Exclusive Device Inquiry
    return [0xF0, 0b01111110, 0x7f, 0b00000110, 1, 0xF7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 5
            and message[0] == 0xf0  # Sysex
            and message[1] == 0b01111110  # Non-realtime
            and message[3] == 0b00000110  # Inquiry Message
            and message[4] == 0b00000010  # Inquiry Reply
            and message[5:8] == groove_synth_id
            and message[8] == 0x01):  # 3rd Wave Family LS
        return message[2] & 0x7f
    return -1


def numberOfBanks():
    return 5


def numberOfPatchesPerBank():
    return 100


def bankSelect(channel, bank):
    return [0xb0 | (channel & 0x0f), 32, bank]


def createEditBufferRequest(channel):
    return [0xf0] + groove_synth_id + [0x01, 0b00000110, 0xf7]


def isEditBufferDump(message):
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1:4] == groove_synth_id
            and message[4] == 0x01
            and message[5] == 0b00000011)


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    raise Exception("This is not an edit buffer - can't be converted")


def createProgramDumpRequest(channel, program_no):
    bank = program_no // numberOfPatchesPerBank()
    program = program_no % numberOfPatchesPerBank()
    return [0xf0] + groove_synth_id + [0x01, 0b00000101, bank, program, 0xf7]


def isSingleProgramDump(message):
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1:4] == groove_synth_id
            and message[4] == 0x01
            and message[5] == 0b00000010)


def convertToProgramDump(channel, data, program_no):
    if not (0 <= program_no < 500):
        raise Exception("Program_no must be in the range 0 to 499")
    bank = program_no // numberOfPatchesPerBank()
    program = program_no % numberOfPatchesPerBank()
    if isEditBufferDump(data):
        return data[0:5] + [0b00000010, bank, program] + data[6:]
    if isSingleProgramDump(data):
        # Replace bank and program slot in the single program dump message
        return data[0:6] + [bank, program] + data[8:]
    raise Exception("Can only convert edit buffers and single programs")


def nameFromDump(message):
    if isEditBufferDump(message):
        nrpns = parse_nrpn_messages(message, 6)
    elif isSingleProgramDump(message):
        nrpns = parse_nrpn_messages(message, 8)
    else:
        raise Exception("Only single program dumps can be named")
    return "".join([chr(nrpns[index]) for index in range(1, 33)]).replace(chr(0x00), '')


def parse_nrpn_messages(sysex_message, start_index):
    nrpn_section = sysex_message[start_index:]
    nrpn_messages = {}

    # Iterate over the section in steps of 4, as each NRPN message has two bytes for param number and two bytes for value
    for i in range(0, len(nrpn_section)-1, 4):
        nrpn_number = nrpn_section[i+1] + (nrpn_section[i] << 7)
        value = nrpn_section[i+3] + (nrpn_section[i+2] << 7)
        nrpn_messages[nrpn_number] = value

    return nrpn_messages


def numberFromDump(message) -> int:
    if isSingleProgramDump(message):
        return message[6] * numberOfPatchesPerBank() + message[7]
    raise Exception("Only single program dumps have program numbers")


def friendlyBankName(bank):
    return f"B{bank+1}"


def friendlyProgramName(program_no):
    bank = program_no // numberOfPatchesPerBank()
    program = program_no % numberOfPatchesPerBank()
    return friendlyBankName(bank) + f"-P{program+1:03d}"


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(data.all_messages[0], name="PAD_Analog P5")

    return testing.TestData(edit_buffer_generator=programs, sysex="testData/GrooveSynthesis3rdWave/Pad-Analog-P5.syx",
                            device_detect_call=[0xF0, 0b01111110, 0x7f, 0b00000110, 1, 0xF7],
                            device_detect_reply=([0xf0, 0b01111110, 0x0, 0b0110, 0b0010, 0x0, 0x2, 0x4a, 0x1, 0, 0, 0, 0, 0, 0xf7], 0),
                            friendly_bank_name=(1, "B2"))
