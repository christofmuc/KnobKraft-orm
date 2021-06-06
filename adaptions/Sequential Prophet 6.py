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
synth = sequential.GenericSequential(name="Sequential Prophet 6",  # Sequential product name is Prophet-6
                                     device_id=0b00101101,
                                     banks=10,
                                     patches_per_bank=100,
                                     name_len=20,
                                     name_position=107,
                                     ).install(this_module)

#
# If this is not loaded as a module, but called as a script, run our unit tests
#
if __name__ == "__main__":
    import unittest
    messages = sequential.load_sysex("testData/P6_Programs_v1.01.syx")
    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(this_module,
                                                                         program_dump=messages[2],
                                                                         program_name='Thick Low Brass'))

