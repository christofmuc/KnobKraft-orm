#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys
from typing import List

import testing
from roland import DataBlock, RolandData, GenericRoland
import knobkraft

this_module = sys.modules[__name__]

# JV-80. The JV-880/JV-90/JV-1000 share the model ID of the JV-80, but have one respectively two bytes more size than the JV-80. How do I handle that?
_jv80_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x22, "Patch common"),
                    DataBlock((0x00, 0x00, 0x08, 0x00), 0x73, "Patch tone 1"),  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x09, 0x00), 0x73, "Patch tone 2"),  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x0A, 0x00), 0x73, "Patch tone 3"),  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x0B, 0x00), 0x73, "Patch tone 4")]  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
_jv80_edit_buffer_addresses = RolandData("JV-80 Temporary Patch"
                                         , num_items=1
                                         , num_address_bytes=4
                                         , num_size_bytes=4
                                         , base_address=(0x00, 0x08, 0x20, 0x00)
                                         , blocks=_jv80_patch_data)
_jv80_program_buffer_addresses = RolandData("JV-80 Internal Patch"
                                            , num_items=0x40
                                            , num_address_bytes=4
                                            , num_size_bytes=4
                                            , base_address=(0x01, 0x40, 0x20, 0x00)
                                            , blocks=_jv80_patch_data)
_jv80_system_common = RolandData("JV-80 System Common", 1, 4, 4, (0x00, 0x00, 0x00, 0x00),
                                 [DataBlock((0x00, 0x00, 0x00, 0x00), 0x21, "System common")])
jv_80 = GenericRoland("Roland JV-80",
                      model_id=[0x46],
                      address_size=4,
                      edit_buffer=_jv80_edit_buffer_addresses,
                      program_dump=_jv80_program_buffer_addresses,
                      device_detect_message=_jv80_system_common)

jv_80.install(this_module)


# Test data picked up by test_adaptation.py
def test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        # This is from https://www.sequencer.de/synthesizer/threads/ctrlr-editor-fuer-jv-80-90-880-1000.151640/
        xv_80_edit_buffer = "F0 41 10 46 12 00 08 20 00 43 72 79 73 74 61 6C 20 56 6F 78 20 00 04 7F 55 00 01 7F 7D 0B 63 01 08 6A 40 3E 02 00 00 00 01 00 32 10 F7" \
                            "F0 41 10 46 12 00 08 28 00 00 04 0F 01 00 00 00 7F 01 01 05 44 00 40 00 40 00 40 05 41 01 58 00 40 04 46 00 40 00 40 00 40 04 46 01 02 01 5A 01 00 00 15 42 40 42 05 02 01 3C 00 00 00 3C 45 7F 44 4C 40 00 06 40 07 07 07 4C 00 47 5B 3C 66 32 4A 40 01 50 00 00 05 00 4C 07 07 07 7F 00 7F 62 56 67 50 14 7F 68 07 08 00 07 00 00 00 00 60 04 07 07 2A 7F 4D 75 5F 78 50 00 7F 7F 55 F7" \
                            "F0 41 10 46 12 00 08 29 00 00 02 0B 01 00 00 00 7F 01 01 05 44 00 40 00 40 00 40 05 41 00 40 03 7F 04 4D 00 40 00 40 00 40 04 46 01 02 01 5C 01 00 00 15 47 56 4C 05 02 01 47 00 00 00 5A 4A 7B 4D 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 4F 00 00 05 00 60 07 07 07 7F 00 7F 2B 56 43 19 14 7F 6A 07 00 00 07 00 00 0E 00 60 04 07 07 34 68 36 66 4A 6C 50 03 7F 7F 36 F7" \
                            "F0 41 10 46 12 00 08 2A 00 00 02 09 01 00 00 00 7F 01 01 00 40 00 40 00 40 00 40 00 40 00 40 03 6E 00 40 00 40 00 40 00 40 04 46 00 02 01 3C 00 00 00 00 40 40 40 05 02 00 3C 00 00 00 00 40 40 40 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 7F 00 00 05 00 40 07 07 07 40 00 00 00 00 00 00 00 00 7F 07 07 0F 07 00 00 00 00 60 07 07 07 2A 6A 13 79 1A 7F 50 4A 7F 5A 11 F7" \
                            "F0 41 10 46 12 00 08 2B 00 00 02 0E 00 00 00 00 7F 01 01 00 40 00 40 00 40 00 40 00 40 00 40 03 7F 00 40 00 40 00 40 00 40 04 46 00 02 01 3C 00 00 00 00 40 40 40 05 02 00 3C 00 00 00 00 40 40 40 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 7F 00 00 05 00 40 07 07 07 40 00 00 00 00 00 00 00 00 7F 07 04 00 07 00 00 00 00 60 07 07 07 36 6A 13 79 1A 7F 50 4A 7F 7F 5C F7"
        # xv_90_edit_buffer = "F0 41 10 46 12 00 08 20 00 43 72 79 73 74 61 6C 20 56 6F 78 20 00 04 7F 55 00 01 7F 7D 0B 63 01 08 6A 40 3E 02 00 00 00 01 00 32 10 F7" \
        #                    "F0 41 10 46 12 00 08 28 00 00 04 0F 01 00 00 00 7F 01 01 05 44 00 40 00 40 00 40 05 41 01 58 00 40 04 46 00 40 00 40 00 40 04 46 01 02 01 5A 01 00 00 15 42 40 42 05 02 01 3C 00 00 00 3C 45 7F 44 4C 40 00 06 40 07 07 07 4C 00 47 5B 3C 66 32 4A 40 01 50 00 00 05 00 4C 07 07 07 7F 00 7F 62 56 67 50 14 7F 68 07 08 00 07 00 00 00 00 60 04 07 07 2A 7F 4D 75 5F 78 50 00 7F 7F 00 00 55 F7" \
        #                    "F0 41 10 46 12 00 08 29 00 00 02 0B 01 00 00 00 7F 01 01 05 44 00 40 00 40 00 40 05 41 00 40 03 7F 04 4D 00 40 00 40 00 40 04 46 01 02 01 5C 01 00 00 15 47 56 4C 05 02 01 47 00 00 00 5A 4A 7B 4D 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 4F 00 00 05 00 60 07 07 07 7F 00 7F 2B 56 43 19 14 7F 6A 07 00 00 07 00 00 0E 00 60 04 07 07 34 68 36 66 4A 6C 50 03 7F 7F 00 00 36 F7" \
        #                    "F0 41 10 46 12 00 08 2A 00 00 02 09 01 00 00 00 7F 01 01 00 40 00 40 00 40 00 40 00 40 00 40 03 6E 00 40 00 40 00 40 00 40 04 46 00 02 01 3C 00 00 00 00 40 40 40 05 02 00 3C 00 00 00 00 40 40 40 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 7F 00 00 05 00 40 07 07 07 40 00 00 00 00 00 00 00 00 7F 07 07 0F 07 00 00 00 00 60 07 07 07 2A 6A 13 79 1A 7F 50 4A 7F 5A 00 00 11 F7" \
        #                    "F0 41 10 46 12 00 08 2B 00 00 02 0E 00 00 00 00 7F 01 01 00 40 00 40 00 40 00 40 00 40 00 40 03 7F 00 40 00 40 00 40 00 40 04 46 00 02 01 3C 00 00 00 00 40 40 40 05 02 00 3C 00 00 00 00 40 40 40 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 7F 00 00 05 00 40 07 07 07 40 00 00 00 00 00 00 00 00 7F 07 04 00 07 00 00 00 00 60 07 07 07 36 6A 13 79 1A 7F 50 4A 7F 7F 00 00 5C F7"
        test_data = knobkraft.stringToSyx(xv_80_edit_buffer)
        program_dump = jv_80.convertToProgramDump(0x00, test_data, 0x22)
        back_convert = jv_80.convertToEditBuffer(0x00, test_data)
        knobkraft.list_compare(test_data, back_convert)
        messages = knobkraft.splitSysex(program_dump)
        command, address, message = jv_80.parseRolandMessage(messages[0])
        assert address == [0x01, 0x40 + 0x22, 0x20, 0x00]
        yield testing.ProgramTestData(message=program_dump, name="Crystal Vox", number=0x22)

    return testing.TestData(program_generator=programs)
