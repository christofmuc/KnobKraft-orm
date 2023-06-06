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
# TODO - the Evolver has also a Waveshape Dump, that no other Sequential synth has. We need to support this
# TODO - It also has the peculiar NameDataDump, as the name of the patch is not included in the patch data itself
# TODO - currently all patches are listed with name "invalid"

#
# Configure the GenericSequential module
#
sequential.GenericSequential(name="DSI Evolver",
                             device_id=0b00100000,
                             banks=4,
                             patches_per_bank=128,
                             file_version=0x01
                             ).install(this_module)


# Test data picked up by test_adaptation.py
def test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[5], number=261)  # It is bank 3, so it starts at 256 + 5 = 261

    return testing.TestData(sysex="testData/Evolver_bank3_1-0.syx", program_generator=programs)
