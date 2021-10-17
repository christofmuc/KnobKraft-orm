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
synth = sequential.GenericSequential(name="Sequential Pro 3",
                                     device_id=0b00110001,  # See Page 147 of the Pro 3 manual
                                     banks=8,
                                     patches_per_bank=128,
                                     name_len=20,
                                     name_position=321,
                                     ).install(this_module)

#
# If this is not loaded as a module, but called as a script, run our unit tests
#
if __name__ == "__main__":
    import unittest
    messages = sequential.load_sysex("testData/P3_Factory_Sounds_v1.01.syx")
    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(this_module,
                                                                         program_dump=messages[2],
                                                                         program_name='Staircase'))


