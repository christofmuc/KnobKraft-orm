#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Finally owning a classic Roland so I can make a working and tested example on how to implement the Roland Synths
import sys
from typing import List

import testing.test_data
from roland import DataBlock, RolandData, GenericRoland, GenericRolandWithBackwardCompatibility
from Roland_JV1080 import jv_1080
from Roland_JV80 import jv_80

this_module = sys.modules[__name__]

# XV-3080 and XV-5080. But the XV-5080 has these Patch Split Key messages as well!? We can ignore them?
_xv3080_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x4f, "Patch common"),
                      DataBlock((0x00, 0x00, 0x02, 0x00), (0x01, 0x11), "Patch common MFX"),
                      DataBlock((0x00, 0x00, 0x04, 0x00), 0x34, "Patch common Chorus"),
                      DataBlock((0x00, 0x00, 0x06, 0x00), 0x53, "Patch common Reverb"),
                      DataBlock((0x00, 0x00, 0x10, 0x00), 0x29, "Patch common Tone Mix Table"),
                      DataBlock((0x00, 0x00, 0x20, 0x00), (0x01, 0x09), "Tone 1"),
                      DataBlock((0x00, 0x00, 0x22, 0x00), (0x01, 0x09), "Tone 2"),
                      DataBlock((0x00, 0x00, 0x24, 0x00), (0x01, 0x09), "Tone 3"),
                      DataBlock((0x00, 0x00, 0x26, 0x00), (0x01, 0x09), "Tone 4")]
_xv3080_edit_buffer_addresses = RolandData("XV-3080 Temporary Patch", 1, 4, 4,
                                           (0x1f, 0x00, 0x00, 0x00),
                                           _xv3080_patch_data)
_xv3080_program_buffer_addresses = RolandData("XV-3080 User Patches", 128, 4, 4,
                                              (0x30, 0x00, 0x00, 0x00),
                                              _xv3080_patch_data)
# This can be used as an alternative way to detect the XV-3080
# _xv3080_system_common = RolandData("XV-3080 System Common", 1, 4, 4, (0x00, 0x00, 0x00, 0x00),
#                                   [DataBlock((0x00, 0x00, 0x00, 0x00), 0x28, "System common")])
xv_3080_main = GenericRoland("Roland XV-3080",
                             model_id=[0x00, 0x10],
                             address_size=4,
                             edit_buffer=_xv3080_edit_buffer_addresses,
                             program_dump=_xv3080_program_buffer_addresses,
                             category_index=0x0c,
                             device_family=[0x10, 0x01])  # Interestingly, the XV-3080 seems the first model to support the generic device inquiry
xv_3080 = GenericRolandWithBackwardCompatibility(xv_3080_main, [jv_80, jv_1080])
xv_3080.install(this_module)


#  and XV-5080 and XV-5050?


def setupHelp():
    return "Make sure the Receive Exclusive parameter (SYSTEM/COMMON) is ON, and the synth is set to Patch Mode"


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patch = []
        names = ["RedPowerBass", "Sinus QSB   ", "Super W Bass"]
        i = 0
        # Extract the first 3 programs from the sysex dump loaded, and yield them with name and number to the test code
        for message in data.all_messages:
            if xv_3080.isPartOfSingleProgramDump(message):
                patch.extend(message)
                if xv_3080.isSingleProgramDump(patch):
                    yield testing.ProgramTestData(message=patch, name=names[i], number=i)
                    patch = []
                    i += 1
                    if i >= len(names):
                        break

    return testing.TestData(sysex="testData/jv1080_AGSOUND1.SYX",
                            program_generator=programs,
                            program_dump_request="f0 41 10 00 10 11 30 00 00 00 00 00 00 4f 01 f7",
                            device_detect_call="f0 7e 7f 06 01 f7",
                            device_detect_reply=("f0 7e 10 06 02 41 10 01 00 00 00 00 00 00 f7", 0))
