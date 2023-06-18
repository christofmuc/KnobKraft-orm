#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import List

import testing
from orm_synth import *


class Korg_DW8000(Synth, DiscoverableDevice, EditBufferCapability, BankDescriptorsCapability):

    def __init__(self):
        Synth.__init__(self)
        DiscoverableDevice.__init__(self)
        EditBufferCapability.__init__(self)
        BankDescriptorsCapability.__init__(self)
        self.channel = 0

    def get_name(self):
        return "Korg DW-8000 Adaptation"

    def bank_descriptors(self) -> List[BankDescriptor]:
        return [BankDescriptor(bank=Bank(0, 64), name="Internal", size=64, type="Patch", is_rom=False)]

    def friendly_program_name(self, bank: Bank, program: Program) -> str:
        return "%d%d" % (program.value() // 8 + 1, (program.value() % 8) + 1)

    def needs_channel_specific_detection(self) -> bool:
        return True

    def create_device_detect_message(self, channel) -> List[MidiMessage]:
        # Page 5 of the service manual - Device ID Request
        return [MidiMessage([0xf0, 0x42, 0x40 | (channel % 16), 0xf7])]

    def channel_if_valid_device_detect_response(self, message: MidiMessage) -> int:
        # Page 3 of the service manual - Device ID
        if (len(message) > 3
                and message[0] == 0xf0  # Sysex
                and message[1] == 0x42  # Korg
                and (message[2] & 0xf0) == 0x30  # Device ID
                and message[3] == 0x03):  # DW-8000
            self.channel = message[2] & 0x0f
            return self.channel
        return -1

    def device_detect_sleep_ms(self) -> int:
        return 100

    def request_edit_buffer(self) -> List[MidiMessage]:
        # Page 5 - Data Save Request
        return [MidiMessage([0xf0, 0x42, 0x30 | self.channel, 0x03, 0x10, 0xf7])]

    def is_edit_buffer(self, messages: List[MidiMessage]) -> bool:
        # Page 3 - Data Dump
        message = messages[0]
        return (len(message) > 4
                and message[0] == 0xf0
                and message[1] == 0x42  # Korg
                and (message[2] & 0xf0) == 0x30  # Format, ignore MIDI Channel in lower 4 bits
                and message[3] == 0x03  # DW-8000
                and message[4] == 0x40  # Data Dump
                )

    def convert_to_edit_buffer(self, patch: List[MidiMessage]) -> List[MidiMessage]:
        if self.is_edit_buffer(patch):
            return [MidiMessage(patch[0][0:2] + [0x30 | self.channel] + patch[0][3:])]
        raise Exception("This is not an edit buffer - can't be converted")


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patch = [x for sublist in data.all_messages[0:2] for x in sublist]
        yield testing.ProgramTestData(message=patch)

    # assert friendlyProgramName(0) == "11"
    return testing.TestData(sysex="testData/KorgDW8000_bank_a.syx", edit_buffer_generator=programs, friendly_bank_name=(63, "88"))


if __name__ == "__main__":
    korg = Korg_DW8000()
    print(korg.get_name())
    print(korg.friendly_program_name(Bank(0, 64), Program.from_zero_base(17)))
    print(korg.create_device_detect_message(3))
    print(korg.bank_descriptors()[0].name)
