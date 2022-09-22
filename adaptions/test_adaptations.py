#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import pytest
import knobkraft

import functools


def skip_targets(param_not_none):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            paramA = kwargs[param_not_none]
            if paramA is None:
                pytest.skip(f'{param_not_none} is None, skipping')
            func(*args, **kwargs)

        return wrapper

    return decorator


#
# Fixtures prepare the test data for the tests below
#
class TestData:
    def __init__(self, adaptation):
        self.test_dict = adaptation.test_data()
        self.all_messages = []
        if "sysex" in self.test_dict:
            self.sysex_file = self.test_dict["sysex"]
            self.all_messages = knobkraft.load_sysex(self.sysex_file)
        if "program_generator" in self.test_dict:
            self.programs = list(self.test_dict["program_generator"](self.all_messages))
            self.program_dump = self.programs[0]["message"]
        if "detection_reply" in self.test_dict:
            self.detection_reply = self.test_dict["detection_reply"]


@pytest.fixture
def test_data(adaptation):
    if hasattr(adaptation, "test_data"):
        return TestData(adaptation)
    else:
        return None


#
# These are generic tests every adaptation must pass
#
def test_name_is_not_none(adaptation):
    assert adaptation.name() is not None


@skip_targets("test_data")
def test_detection(adaptation, test_data: TestData):
    if hasattr(test_data, "detection_reply"):
        assert adaptation.channelIfValidDeviceResponse(test_data.detection_reply[0]) == test_data.detection_reply[1]


@skip_targets("test_data")
def test_extract_name(adaptation, test_data: TestData):
    if hasattr(adaptation, "nameFromDump") and test_data is not None:
        for program in test_data.programs:
            assert adaptation.nameFromDump(program["message"]) == program["name"]


@skip_targets("test_data")
def test_rename(adaptation, test_data: TestData):
    if hasattr(adaptation, "nameFromDump") and hasattr(adaptation, "renamePatch"):
        binary = test_data.program_dump
        # Rename to the name it already has
        renamed = adaptation.renamePatch(binary, adaptation.nameFromDump(binary))
        # This should not change the extracted name
        assert adaptation.nameFromDump(renamed) == adaptation.nameFromDump(binary)
        # This should not change the fingerprint
        assert adaptation.calculateFingerprint(renamed) == adaptation.calculateFingerprint(binary)
        # Now rename
        with_new_name = adaptation.renamePatch(binary, "new name")
        new_name = adaptation.nameFromDump(with_new_name)
        assert new_name.strip() == "new name"


@skip_targets("test_data")
def test_is_program_dump(adaptation, test_data: TestData):
    for program in test_data.programs:
        assert adaptation.isSingleProgramDump(program["message"])


@skip_targets("test_data")
def test_convert_to_edit_buffer(adaptation, test_data: TestData):
    if hasattr(adaptation, "convertToEditBuffer"):
        for program_data in test_data.programs:
            program = program_data["message"]
            if adaptation.isSingleProgramDump(program):
                previous_number = 0
                if hasattr(adaptation, "numberFromDump"):
                    previous_number = adaptation.numberFromDump(program)
                idempotent = adaptation.convertToProgramDump(0x00, program, previous_number)
                assert knobkraft.list_compare(idempotent, program)
                edit_buffer = adaptation.convertToEditBuffer(0x00, program)
                assert adaptation.isEditBufferDump(edit_buffer)
                program_buffer = adaptation.convertToProgramDump(0x00, edit_buffer, 11)
            elif adaptation.isEditBufferDump(program):
                program_buffer = adaptation.convertToProgramDump(program, 11)
                assert adaptation.isSingleProgramDump(program_buffer)
                edit_buffer = adaptation.convertToEditBuffer(program_buffer)
            else:
                program_buffer = program
                edit_buffer = None
            if hasattr(adaptation, "numberFromDump"):
                assert adaptation.numberFromDump(program_buffer) == 11
            if hasattr(adaptation, "nameFromDump") and edit_buffer is not None:
                assert adaptation.nameFromDump(program_buffer) == adaptation.nameFromDump(edit_buffer)


@skip_targets("test_data")
def test_number_from_dump(adaptation, test_data: TestData):
    if hasattr(adaptation, "numberFromDump"):
        for program in test_data.programs:
            assert adaptation.numberFromDump(program["message"]) == program["number"]


@skip_targets("test_data")
def test_layer_name(adaptation, test_data: TestData):
    if hasattr(adaptation, "layerName"):
        for program in test_data.programs:
            assert adaptation.layerName(program["message"], 1) == program["second_layer_name"]
            assert adaptation.isSingleProgramDump(program["message"])
            new_messages = adaptation.setLayerName(program["message"], 1, 'changed layer')
            assert adaptation.layerName(new_messages, 1) == 'changed layer'
            assert adaptation.isSingleProgramDump(new_messages)


