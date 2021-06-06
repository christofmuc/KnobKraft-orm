#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sequential
import sys

this_module = sys.modules[__name__]

#
# TODO - the Evolver has also a Waveshape Dump, that no other Sequential synth has. We need to support this
# TODO - It also has the peculiar NameDataDump, as the name of the patch is not included in the patch data itself
# TODO - currently all patches are listed with name "invalid"

#
# Configure the GenericSequential module
#
sequential.GenericSequential(name="DSI Evolver",
                             device_id=0b00100000,
                             banks=4,
                             patches_per_bank=128,
                             file_version=0x01
                             ).install(this_module)
if __name__ == "__main__":
    import sys
    import unittest

    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(sys.modules[__name__]))
