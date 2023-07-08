#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import sys
from typing import List

import testing
from roland import DataBlock, RolandData, GenericRoland

this_module = sys.modules[__name__]

# JV-1080/JV-2080 are identical, only that the JV-2080 has 2 more bytes in the patch common section
_jv1080_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x48, "Patch common"),  # 0x4a, 2 bytes more than the JV-1080 for the JV-2080
                      DataBlock((0x00, 0x00, 0x10, 0x00), (0x01, 0x01), "Patch tone 1"),
                      DataBlock((0x00, 0x00, 0x12, 0x00), (0x01, 0x01), "Patch tone 2"),
                      DataBlock((0x00, 0x00, 0x14, 0x00), (0x01, 0x01), "Patch tone 3"),
                      DataBlock((0x00, 0x00, 0x16, 0x00), (0x01, 0x01), "Patch tone 4")]
_jv1080_edit_buffer_addresses = RolandData("JV-1080 Temporary Patch", 1, 4, 4,
                                           (0x03, 0x00, 0x00, 0x00),
                                           _jv1080_patch_data)
_jv1080_program_buffer_addresses = RolandData("JV-1080 User Patches", 128, 4, 4,
                                              (0x11, 0x00, 0x00, 0x00),
                                              _jv1080_patch_data)
# The JV-1080 does not reply to Identity Request. To trick it into being detected, we simply request the first system common block
# But we need to do this for all valid device IDs, as the sysex ID could be set to a non-standard ID (standard is 0x10)
_jv1080_system_common = RolandData("JV-1080 System Common", 1, 4, 4, (0x00, 0x00, 0x00, 0x00),
                                   [DataBlock((0x00, 0x00, 0x00, 0x00), 0x28, "System common")])
jv_1080 = GenericRoland("Roland JV-1080", model_id=[0x6a], address_size=4, edit_buffer=_jv1080_edit_buffer_addresses,
                        program_dump=_jv1080_program_buffer_addresses,
                        device_detect_message = _jv1080_system_common)
jv_1080.install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patch = []
        names = ["RedPowerBass", "Sinus QSB", "Super W Bass"]
        i = 0
        for message in data.all_messages:
            if jv_1080.isPartOfSingleProgramDump(message):
                patch.extend(message)
                if jv_1080.isSingleProgramDump(patch):
                    yield testing.ProgramTestData(message=patch, name=names[i], number= i)
                    patch = []
                    i += 1
                    if i >= len(names):
                        break

    return testing.TestData(sysex="testData/JV1080_AGSOUND1.SYX", program_generator=programs)
