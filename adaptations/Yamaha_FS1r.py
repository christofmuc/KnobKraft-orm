#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import itertools
from typing import List

import testing
from knobkraft import load_midi
from roland import DataBlock, RolandData, GenericRoland

import sys
this_module = sys.modules[__name__]


systemSettingsAddress = (0x00, 0x00, 0x00)
bulkHeaderAddress = (0x0e, 0x0f, 0x00)
bulkFooterAddress = (0x0f, 0x0f, 0x00)
bulkProgramHeaderAddress = (0x0e, 0x00, 0x00)  # https://coffeeshopped.com/patch-base/help/synth/yamaha/reface-dx
bulkProgramFooterAddress = (0x0f, 0x00, 0x00)
commonVoiceAddress = (0x30, 0x00, 0x00)
legacyDataLength = 38 + 4 * 28  # The old format stored only the required bytes, none of the sysex stuff

YAMAHA_ID=0x43

fs1r_performance_data = [DataBlock((0x10, 0x00, 0x00), 80, "Performance Common"), # The first 12 bytes are the name
                   DataBlock((0x10, 0x00, 0x50), 112, "Effect"),
                   DataBlock((0x30, 0x00, 0x00), 52, "Part 1"),
                   DataBlock((0x31, 0x00, 0x00), 52, "Part 2"),
                   DataBlock((0x32, 0x00, 0x00), 52, "Part 3"),
                   DataBlock((0x33, 0x00, 0x00), 52, "Part 4"),
                   ]

fs1_all_performance_data = [DataBlock((0x11, 0x00, 0x00), 400, "Performance")]

fs1r_voice_data = [DataBlock((0x40, 0x00, 0x00), 112, "Voice Common")] +  \
                  [DataBlock((0x60, op, 0x00), 35, "Operator 1 Voiced") for op in range(8)] + \
                  [DataBlock((0x60, op, 0x23), 27, "Operator 1 Non-Voiced") for op in range(8)] + \
                  [DataBlock((0x61, op, 0x00), 35, "Operator 2 Voiced") for op in range(8)] + \
                  [DataBlock((0x61, op, 0x23), 27, "Operator 2 Non-Voiced") for op in range(8)] + \
                  [DataBlock((0x62, op, 0x00), 35, "Operator 3 Voiced") for op in range(8)] + \
                  [DataBlock((0x62, op, 0x23), 27, "Operator 3 Non-Voiced") for op in range(8)] + \
                  [DataBlock((0x63, op, 0x00), 35, "Operator 4 Voiced") for op in range(8)] + \
                  [DataBlock((0x64, op, 0x23), 27, "Operator 4 Non-Voiced") for op in range(8)]

fs1r_fseq_data = [DataBlock((0x70, 0x00, 0x00), 0x00, "Fseq parameter")]  # 8 bytes of name at the start, byte count not used in here

fs1r_system_data = [DataBlock((0x00, 0x00, 0x00), 0x4c, "System Parameter")]

fs1r_edit_buffer = RolandData(data_name="Edit Buffer", num_items=1, num_address_bytes=3, num_size_bytes=1, base_address=(0x00, 0x00, 0x00), blocks=fs1r_voice_data)
fs1_performance = RolandData(data_name="Performance", num_items=1, num_address_bytes=3, num_size_bytes=1, base_address=(0x00, 0x00, 0x00), blocks=fs1_all_performance_data)

yamaha_fs1r = GenericRoland(name="Yamaha FS1R", model_id=[0x5e], address_size=3, edit_buffer=fs1_performance, program_dump=fs1_performance, uses_consecutive_addresses=True)
yamaha_fs1r.install(this_module)


def make_test_data():
    messages = load_midi("testData/Yamaha_FS1R/Vdfs1r01.mid")

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        raw_data = list(itertools.chain.from_iterable(test_data.all_messages))
        for d in test_data.all_messages:
            isPartOfEditBufferDump(d)
        yield testing.ProgramTestData(message=raw_data, name="Piano 1   ", rename_name="Piano 2   ")

        # Test a Soundmondo file
        message = knobkraft.stringToSyx("F0 43 00 7F 1C 00 04 05 0E 0F 00 5E F7 F0 43 00 7F 1C 00 2A 05 30 00 00 53 6E 61 70 48 61 70 70 79 20 00 00 40 00 02 42 04 00 64 00 00 40 40 40 40 40 40 40 40 03 40 40 07 40 40 00 00 00 21 F7 F0 43 00 7F 1C 00 20 05 31 00 00 01 7F 3A 24 6E 7F 56 00 00 00 00 00 00 00 00 01 01 40 7D 3C 00 00 00 00 40 00 00 00 6E F7 F0 43 00 7F 1C 00 20 05 31 01 00 01 7F 5D 23 5A 7F 3C 00 00 04 0F 06 03 00 00 01 01 3F 70 48 00 00 00 00 40 00 00 00 5F F7 F0 43 00 7F 1C 00 20 05 31 02 00 01 7F 67 3C 5A 7F 60 00 00 04 00 0B 00 00 00 01 01 5C 62 4B 00 00 00 00 40 00 00 00 12 F7 F0 43 00 7F 1C 00 20 05 31 03 00 01 7F 4A 53 5A 7F 66 00 00 08 00 07 00 00 00 01 01 78 60 7F 00 00 00 00 40 00 00 00 43 F7 F0 43 00 7F 1C 00 04 05 0F 0F 00 5D F7")
        yield testing.ProgramTestData(message=message, name="SnapHappy ", rename_name="Piano 2   ")

    def program_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        program_data = []
        for d in messages:
            if yamaha_fs1r.isPartOfSingleProgramDump(d):
                program_data.append(d)
        yield testing.ProgramTestData(message=program_data[0], name="Piano 1   ", rename_name="Piano 2   ", number=17, friendly_number="Bank3-2")

    return testing.TestData(program_generator=program_buffers)


