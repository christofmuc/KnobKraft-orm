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
sequential.GenericSequential(name="Sequential Take 5",
                             device_id=0x35,
                             banks=8,
                             patches_per_bank=16,
                             name_len=20,
                             name_position=195,
                             ).install(this_module)


# Test data picked up by test_adaptation.py
def test_data():
    def programs(messages):
        yield {"message": messages[2], "name": 'Prophetic Sync', "number": 2}

    return {"sysex": "testData/Take5_Factory_Set1_v1.0.syx", "program_generator": programs}
