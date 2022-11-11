#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sequential
import sys

this_module = sys.modules[__name__]

#
# Configure the GenericSequential module
#
sequential.GenericSequential(name="DSI Tetra",
                             device_id=0b00100110,
                             banks=4,
                             patches_per_bank=128,
                             name_position=184,
                             name_len=16,
                             friendlyBankName=lambda x: f"bank {x+1}",
                             friendlyProgramName=lambda x: f"bank {(x//128)+1} - {(x%128)+1}"
                             ).install(this_module)


# Test data picked up by test_adaptation.py
def test_data():
    def programs(messages):
        yield {"message": messages[1], "name": 'Tom Sawyer', "number": 1}

    return {"sysex": "testData/Tetra_ProgramsCombos_1.0.syx", "program_generator": programs}
