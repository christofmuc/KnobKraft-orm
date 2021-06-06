#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import sequential
import sys

this_module = sys.modules[__name__]

# TODO Support for the original Prophet 5 (Rev1-Rev3) sysex dumps. They started with F0 01 01, but lacked the F7

#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="Sequential Prophet-5",
                                     device_id=0b00110010,  # This is the ID used in the factory programs available from the website
                                     banks=10,
                                     patches_per_bank=40,
                                     name_len=20,
                                     name_position=65,
                                     ).install(this_module)

#
# If this is not loaded as a module, but called as a script, run our unit tests
#
if __name__ == "__main__":
    import unittest
    messages = sequential.load_sysex("testData/P5_Factory_Programs_v1.02.syx")
    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(this_module,
                                                                         program_dump=messages[2],
                                                                         program_name='Forever Keys'))

