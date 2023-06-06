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
synth = sequential.GenericSequential(name="Sequential Pro 3",
                                     device_id=0b00110001,  # See Page 147 of the Pro 3 manual
                                     banks=8,
                                     patches_per_bank=128,
                                     name_len=20,
                                     name_position=321,
                                     ).install(this_module)


# Test data picked up by test_adaptation.py
def test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[2], name='Staircase', number=2)

    return testing.TestData(sysex="testData/P3_Factory_Sounds_v1.01.syx", program_generator=programs)
