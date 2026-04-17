#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import List

import sequential
import sys

import testing

this_module = sys.modules[__name__]

# The Mopho X4 has 4 banks, not 3 like the original Mopho

#
# Configure the GenericSequential module
#
sequential.GenericSequential(name="DSI Mopho X4",
                             device_id=0x29,
                             banks=4,
                             patches_per_bank=128,
                             name_position=184,
                             name_len=16,
                             id_list=[0x25, 0x27, 0x29]  # this adds Mopho Keyboard, SE, and X4
                             ).install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message= data.all_messages[0], name= 'Moonster', number= 0)

    return testing.TestData(sysex="testData/Mopho_x4_AllBanks_V1.01.syx", program_generator=programs, expected_patch_count=512)
