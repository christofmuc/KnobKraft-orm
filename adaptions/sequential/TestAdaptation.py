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
    def __init__(self, synth):
        super().__init__()
        self.synth = synth


class NameTest(AdaptationTestBase):
    def runTest(self):
        self.assertIsNotNone(self.synth.name())


class PatchNameTest(AdaptationTestBase):
    def __init__(self, synth, program_dump, expectedName):
        super().__init__(synth)
        self.program_dump = program_dump
        self.expectedName = expectedName

    def runTest(self):
        self.assertEqual(self.synth.nameFromDump(self.program_dump),
                         self.expectedName.strip())


class RenameTest(AdaptationTestBase):
    def __init__(self, synth, program_dump):
        super().__init__(synth)
        self.program_dump = program_dump

    def runTest(self):
        binary = self.program_dump
        renamed = self.synth.renamePatch(binary, self.synth.nameFromDump(binary))
        self.assertEqual(self.synth.calculateFingerprint(renamed),
                         self.synth.calculateFingerprint(binary))
        new_name = self.synth.renamePatch(binary, "newname")
        result = self.synth.nameFromDump(new_name)
        self.assertEqual(result, "newname")


class IsProgramDumpTest(AdaptationTestBase):
    def __init__(self, synth, program_dump):
        super().__init__(synth)
        self.program_dump = program_dump

    def runTest(self):
        self.assertTrue(self.synth.isSingleProgramDump(self.program_dump))
        data = self.synth.calculateFingerprint(self.program_dump)


# This is needed to dynamically create the test cases as we parametrize them with the module to test and
# additional data like an example patch
def create_tests(synth, program_dump=None, program_name=None):
    suite = unittest.TestSuite()
    suite.addTest(NameTest(synth))
    if hasattr(synth, "nameFromDump") and program_dump is not None and program_name is not None:
        suite.addTest(PatchNameTest(synth, program_dump, program_name))
    if hasattr(synth, "renamePatch") and program_dump is not None:
        suite.addTest(RenameTest(synth, program_dump))
    if hasattr(synth, "isSingleProgramDump") and program_dump is not None:
        suite.addTest(IsProgramDumpTest(synth, program_dump))
    return suite


def load_sysex(filename):
    with open(filename, mode="rb") as midi_messages:
        content = midi_messages.read()
        messages = []
        start_index = 0
        for index, byte in enumerate(content):
            if byte == 0xf7:
                messages.append(list(content[start_index:index + 1]))
                start_index = index + 1
        return messages
