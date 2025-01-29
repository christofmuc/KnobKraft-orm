#
#   Copyright (c) 2025 Christof Ruch. All rights reserved.
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
synth = sequential.GenericSequential(name="Sequential Trigon-6",
                                     device_id=0b00111001,
                                     banks=10,
                                     patches_per_bank=100,
                                     name_len=20,
                                     name_position=116,
                                     ).install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[2], name='Dakota Chorale', number=2)
        yield testing.ProgramTestData(message=data.all_messages[127], name='Oil Cans', number=127)

    return testing.TestData(sysex="testData/Sequential_Trigon6/T6_Programs_v1.0.syx", program_generator=programs)
