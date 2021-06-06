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
sequential.GenericSequential(name="DSI Mopho",
                             device_id=0x25,
                             banks=3,
                             patches_per_bank=128,
                             name_position=184,
                             name_len=16,
                             id_list=[0x25, 0x27, 0x29]  # this adds Mopho Keyboard, SE, and X4
                             ).install(this_module)

#
# If this is not loaded as a module, but called as a script, run our unit tests
#
if __name__ == "__main__":
    import unittest
    messages = sequential.load_sysex("testData/Mopho_Programs_v1.0.syx")
    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(this_module,
                                                                         program_dump=messages[1],
                                                                         program_name='Wagnerian'))
