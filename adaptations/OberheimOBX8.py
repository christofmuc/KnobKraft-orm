#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys
from typing import List, Tuple

import sequential
import testing

this_module = sys.modules[__name__]

SINGLE_PROGRAM = 0x02
COMBI_PROGRAM = 0x07
BANKS = [{"bank": 0, "name": f"OB-X8", "size": 128, "type": "Single Program"},
         {"bank": 1, "name": f"OB-8", "size": 128, "type": "Single Program"},
         {"bank": 2, "name": f"OB-Xa", "size": 128, "type": "Single Program"},
         {"bank": 3, "name": f"OB-SX", "size": 128, "type": "Single Program"},
         {"bank": 4, "name": f"OB-X", "size": 128, "type": "Single Program"},
         {"bank": 5, "name": f"User", "size": 128, "type": "Single Program"},
         {"bank": 6, "name": f"Splits", "size": 128, "type": "Split Program"},
         {"bank": 7, "name": f"Doubles", "size": 128, "type": "Double Program"}
         ]


def bankName(bank_no: int) -> str:
    return BANKS[bank_no]["name"]


def name_info(message: List[int]) -> Tuple[int, int]:
    name_positions = { SINGLE_PROGRAM: (102,13), COMBI_PROGRAM: (3, 13)}
    program_type = message[3]
    return name_positions[program_type]if program_type in name_positions else -1


sequential.GenericSequential(name="Oberheim OB-X8",
                             manufacturer=0x10,  # Oberheim, not Sequential
                             device_id=0x58,
                             program_data_ids=[SINGLE_PROGRAM, COMBI_PROGRAM],
                             banks=8,
                             patches_per_bank=128,
                             name_len=13,
                             name_info_function=name_info,
                             friendlyBankName=bankName
                             ).install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        program_string = "F0 10 58 02 04 7F 00 01 00 00 20 00 01 01 00 00 00 00 01 00 00 00 00 00 00 00 00 00 01 00 02 00 2F 00 00 01 00 01 00 00 56 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 56 00 00 00 00 00 00 00 00 3A 00 3A 64 7F 20 20 00 00 00 00 00 0C 7F 00 07 05 00 00 01 01 00 33 01 01 00 01 01 00 0C 18 24 30 04 3C 00 49 7F 00 01 7F 00 7F 7F 04 00 42 61 73 00 69 63 20 50 72 6F 67 00 72 61 6D 00 00 00 00 00 00 00 00 00 7F 00 00 00 7F 7F 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 F7"
        yield testing.ProgramTestData(message=program_string, name="Basic Program", number=0x04 * 0x80 + 0x7f)

        combi_program_string = "F0 10 58 07 00 03 00 00 02 00 44 65 66 61 00 75 6C 74 20 53 70 6C 00 69 74 00 00 00 00 00 00 00 00 02 00 00 33 01 00 01 01 01 00 0C 18 24 00 30 3C 02 56 00 00 00 00 00 0C 01 01 00 00 02 00 7F 00 02 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1F 00 3B 24 00 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 20 3C 7F 24 20 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 F7"
        yield testing.ProgramTestData(message=combi_program_string, name="Default Split", number=0 * 128 + 3)

        yield testing.ProgramTestData(message=data.all_messages[0], name="Jersey Girl B", number=22)

    return testing.TestData(sysex="testData/Oberheim_OBX8/OBx8-prest.syx", program_generator=programs, friendly_bank_name=(5, "User"))
