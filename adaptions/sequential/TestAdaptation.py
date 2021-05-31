#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import unittest


#
# Implement a customizable but generic Test Suite for testing any adaptations functionality!
#

class AdaptationTestBase(unittest.TestCase):

    def __init__(self, adaptation_module_to_test, example_patch):
        super().__init__()
        self.adaptation_module_to_test = adaptation_module_to_test
        self.example_patch = example_patch


class RenameTest(AdaptationTestBase):

    def runTest(self):
        binary = self.example_patch
        renamed = self.adaptation_module_to_test.renamePatch(binary, self.adaptation_module_to_test.nameFromDump(binary))
        self.assertEqual(self.adaptation_module_to_test.calculateFingerprint(renamed),
                         self.adaptation_module_to_test.calculateFingerprint(binary))
        new_name = self.adaptation_module_to_test.renamePatch(binary, "newname")
        result = self.adaptation_module_to_test.nameFromDump(new_name)
        self.assertEqual(result, "newname")


# This is needed to dynamically create the test cases as we parametrize them with the module to test and
# additional data like an example patch
def create_tests(adaptation_module_to_test, example_patch):
    suite = unittest.TestSuite()
    if hasattr(adaptation_module_to_test, "renamePatch"):
        suite.addTest(RenameTest(adaptation_module_to_test, example_patch))
    return suite
