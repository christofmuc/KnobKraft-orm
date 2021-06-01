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
synth = sequential.GenericSequential(name="DSI Prophet 08",  # DSI product name is Prophet '08 (for the year introduced)
                                     device_id=0b00100011,  # See Page 82 of the prophet 12 manual
                                     banks=2,
                                     patches_per_bank=128,
                                     name_len=16,
                                     name_position=184,
                                     id_list=[0b00100011, 0b00100100],  # Prophet 08 or special edition Prophet 08
                                     ).install(this_module)

#
# If this is not loaded as a module, but called as a script, run our unit tests
#
if __name__ == "__main__":
    import unittest
    messages = sequential.load_sysex("testData/Prophet_08_Programs_v1.0.syx")
    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(this_module,
                                                                         program_dump=messages[2],
                                                                         program_name='T8 Strings'))
