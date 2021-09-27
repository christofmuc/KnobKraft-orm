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
                             name_position=321,
                             ).install(this_module)

if __name__ == "__main__":
    import sys
    import unittest

    unittest.TextTestRunner().run(sequential.TestAdaptation.create_tests(sys.modules[__name__]))
