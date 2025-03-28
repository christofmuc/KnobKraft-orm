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

#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="DSI Prophet 08",  # DSI product name is Prophet '08 (for the year introduced)
                                     device_id=0b00100011,  # See Page 82 of the prophet 12 manual
                                     banks=2,
                                     patches_per_bank=128,
                                     name_len=16,
                                     name_position=184,
                                     id_list=[0b00100011, 0b00100100],  # Prophet 08 or special edition Prophet 08
                                     ).install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[2], name="T8 Strings", number=2)

    return testing.TestData(sysex="testData/Prophet_08_Programs_v1.0.syx", program_generator=programs, expected_patch_count=256)
