#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys
from typing import List

import sequential
import testing

this_module = sys.modules[__name__]


#
# Configure the GenericSequential module
#
# With OS 2.0, the Take 5 got 16 banks instead of 8. Let's just add those
def take5_bank_name(bank) -> str:
    return str(bank) if bank < 10 else chr(ord("A") + (bank - 10))


def take5_program_name(program) -> str:
    bank = program // 16
    program = program % 16
    return take5_bank_name(bank) + f"{program:02d}"


sequential.GenericSequential(name="Sequential Take 5",
                             device_id=0x35,
                             banks=16,
                             patches_per_bank=16,
                             name_len=20,
                             name_position=195,
                             friendlyBankName=take5_bank_name,
                             friendlyProgramName=take5_program_name,
                             ).install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[2], name='Prophetic Sync', number=2, friendly_number="002")

    return testing.TestData(sysex="testData/Take5_Factory_Set1_v1.0.syx", program_generator=programs, friendly_bank_name=(11, "B"), expected_patch_count=128)
