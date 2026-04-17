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

# TODO Support for the original Prophet 5 (Rev1-Rev3) sysex dumps. They started with F0 01 01, but lacked the F7

#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="Sequential Prophet-5",
                                     device_id=0b00110010,  # This is the ID used in the factory programs available from the website
                                     banks=10,
                                     patches_per_bank=40,
                                     name_len=20,
                                     name_position=65,
                                     id_list=[0b00110010, 0b00110011],
                                     ).install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[2], name='Forever Keys', number=2)

    return testing.TestData(sysex="testData/P5_Factory_Programs_v1.02.syx", program_generator=programs, expected_patch_count=200)
