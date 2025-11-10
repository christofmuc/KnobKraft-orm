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
def teo5_bank_name(bank) -> str:
    return str(bank) if bank < 10 else chr(ord("A") + (bank - 10))


def teo5_program_name(program) -> str:
    bank = program // 16
    program = program % 16
    return teo5_bank_name(bank) + f"{program:02d}"


sequential.GenericSequential(name="Oberheim Teo-5",
                             manufacturer=0x10,  # Oberheim, not Sequential
                             device_id=0x5a,
                             banks=16,
                             patches_per_bank=16,
                             name_len=20,
                             name_position=159,
                             friendlyBankName=teo5_bank_name,
                             friendlyProgramName=teo5_program_name,
                             ).install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[2], name='OB-X Pad', number=2, friendly_number="002")
        yield testing.ProgramTestData(message=data.all_messages[17], name='Waterfall Cavern', number=17, friendly_number="101")
        yield testing.ProgramTestData(message=data.all_messages[80], name='FlyLike a TEO', number=80, friendly_number="500")

    return testing.TestData(sysex="testData/Oberheim_Teo5/TEO5_Factory_Programs_v1.00.syx", program_generator=programs, friendly_bank_name=(11, "B"), expected_patch_count=256)
