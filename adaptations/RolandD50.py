#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# The Roland D-50 implements the Roland Exclusive Format Type IV, and thus is a good Roland example
import sys
from typing import List

import testing
from roland import DataBlock, RolandData, GenericRoland

roland_id = 0x41  # Roland
model_id = 0b00010100  # D-50
command_rq1 = 0x11
command_dt1 = 0x12

# Construct the Roland character set as specified in the MIDI implementation
character_set = [' '] + [chr(x) for x in range(ord('A'), ord('Z') + 1)] + \
                [chr(x) for x in range(ord('a'), ord('z') + 1)] + \
                [chr(x) for x in range(ord('1'), ord('9') + 1)] + ['0', '-']

this_module = sys.modules[__name__]

d50_patch_data = [DataBlock((0x00, 0x00, 0x00), 0x40, "Upper partial-1"),
                  DataBlock((0x00, 0x00, 0x40), 0x40, "Upper partial-2"),
                  DataBlock((0x00, 0x01, 0x00), 0x40, "Upper common"),
                  DataBlock((0x00, 0x01, 0x40), 0x40, "Lower partial-1"),
                  DataBlock((0x00, 0x02, 0x00), 0x40, "Lower partial-2"),
                  DataBlock((0x00, 0x02, 0x40), 0x40, "Lower common"),
                  DataBlock((0x00, 0x03, 0x00), 0x40, "Patch")]
_d50_edit_buffer_addresses = RolandData("D50 Temporary Patch", 1, 3, 3,
                                           (0x00, 0x00, 0x00),
                                           d50_patch_data)
_d50_program_buffer_addresses = RolandData("D50 User Patches", 64, 3, 3,
                                           (0x20, 0x00, 0x00),
                                           d50_patch_data)

d50 = GenericRoland("Roland D-50",
                    model_id=[model_id],
                    address_size=3,
                    edit_buffer=_d50_edit_buffer_addresses,
                    program_dump=_d50_program_buffer_addresses)
d50.install(this_module)


def loadD50BankDump(messages):
    # The Bank dumps of the D-50 basically are just a lists of messages with the whole memory content of the synth
    # We need to put them together, and then can read the individual data items from the RAM
    synth_ram = [0xff] * (address_to_index([0x04, 0x0c, 0x08]) + 376)
    for message in messages:
        command, address, data = parseRolandMessage(message)
        if command == command_dt1:
            memory_base_index = address_to_index(address)
            synth_ram[memory_base_index:memory_base_index + len(data)] = data

    # Now pull 64 patches out of the RAM and create individual edit buffer messages
    result = []
    for p in range(64):
        patch = []
        patch_base = address_to_index([0x2, 0x00, 0x00]) + p * (7 * 0x40)
        for subsection in range(7):
            target_base = subsection * 0x40
            source_base = patch_base + target_base
            patch = patch + buildRolandMessage(0, command_dt1,
                                               index_to_address(target_base),
                                               synth_ram[source_base:source_base + 0x40])
        result.append(patch)
    return result


def isOwnSysex(message):
    return len(message) > 3 and message[0] == 0xf0 and message[1] == roland_id and message[3] == model_id


def buildRolandMessage(channel, command_id, address, data):
    message = [0xf0, roland_id, channel & 0x0f, model_id, command_id] + address + data + [0, 0xf7]
    message[-2] = roland_checksum(message[5:-2])
    return message


def parseRolandMessage(message):
    if isOwnSysex(message):
        checksum = roland_checksum(message[5:-2])
        if checksum == message[-2]:
            command = message[4]
            address = message[5:8]
            return command, address, message[8:-2]
        raise Exception("Checksum error in Roland message parsing, expected", message[-1], "but got", checksum)
    return None, None, None


def roland_checksum(data_block):
    return sum([-x for x in data_block]) & 0x7f


def address_to_index(address):
    return address[2] + (address[1] << 7) + (address[0] << 14)


def index_to_address(index):
    return [index >> 14, (index & 0x3f80) >> 7, index & 0x7f]


def make_test_data():
    def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        # Quick test of device detect
        # detectMessage = createDeviceDetectMessage(0x7)
        # g_command, g_address, g_data = parseRolandMessage(detectMessage)
        # assert (g_command == command_rq1)
        # assert (g_address == [0x00, 0x01, 0x00])
        # assert (g_data == [0x00, 0x00, 0x40])

        patches = loadD50BankDump(test_data.all_messages)
        yield testing.ProgramTestData(message=patches[0], name="SOUNDTRACK II     ", number=0, friendly_number="1")

    return testing.TestData(sysex=R"testData/Roland_D50_DIGITAL DREAMS.syx", program_generator=make_patches)
