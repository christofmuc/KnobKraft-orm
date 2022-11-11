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
    name = adaptation.name()
    assert name is not None
    assert isinstance(name, str)


@skip_targets("test_data")
def test_detection(adaptation, test_data: TestData):
    if hasattr(test_data, "detection_reply"):
        assert adaptation.channelIfValidDeviceResponse(test_data.detection_reply[0]) == test_data.detection_reply[1]
    else:
        pytest.skip(f"detection_reply test data not defined for adaptation {adaptation.name}")


@skip_targets("test_data")
def test_extract_name(adaptation, test_data: TestData):
    if hasattr(adaptation, "nameFromDump") and test_data is not None:
        for program in test_data.programs:
            assert adaptation.nameFromDump(program["message"]) == program["name"]
    else:
        pytest.skip(f"{adaptation.name} has not implemented nameFromDump")


@skip_targets("test_data")
def test_rename(adaptation, test_data: TestData):
    if hasattr(adaptation, "nameFromDump") and hasattr(adaptation, "renamePatch"):
        binary = test_data.program_dump
        # Rename to the name it already has
        renamed = adaptation.renamePatch(binary, adaptation.nameFromDump(binary))
        # This should not change the extracted name
        assert adaptation.nameFromDump(renamed) == adaptation.nameFromDump(binary)
        # This should not change the fingerprint
        if hasattr(adaptation, "calculateFingerprint"):
            assert adaptation.calculateFingerprint(renamed) == adaptation.calculateFingerprint(binary)
        # Now rename
        if "rename_name" in test_data.test_dict:
            new_name = test_data.test_dict["rename_name"]
        else:
            new_name = "new name"
        with_new_name = adaptation.renamePatch(binary, new_name)
        new_name = adaptation.nameFromDump(with_new_name)
        assert new_name.strip() == new_name.strip()
    else:
        pytest.skip(f"{adaptation.name} has not implemented nameFromDump and renamePatch")


@skip_targets("test_data")
def test_is_program_dump(adaptation, test_data: TestData):
    for program in test_data.programs:
        if "is_edit_buffer" in program:
            assert adaptation.isEditBufferDump(program["message"])
        else:
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
                edit_buffer = adaptation.convertToEditBuffer(0x00, program)
                if "convert_to_edit_buffer_produces_program_dump" in test_data.test_dict and test_data.test_dict["convert_to_edit_buffer_produces_program_dump"]:
                    # This is a special case for the broken implementation of the Andromeda edit buffer
                    assert adaptation.isSingleProgramDump(edit_buffer)
                else:
                    assert adaptation.isEditBufferDump(edit_buffer)
                if not hasattr(adaptation, "convertToProgramDump"):
                    # Not much more we can test here
                    return
                if "not_idempotent" in test_data.test_dict:
                    pass
                else:
                    idempotent = adaptation.convertToProgramDump(0x00, program, previous_number)
                    assert knobkraft.list_compare(idempotent, program)
                program_buffer = adaptation.convertToProgramDump(0x00, edit_buffer, 11)
            elif adaptation.isEditBufferDump(program):
                program_buffer = adaptation.convertToProgramDump(0x00, program, 11)
                assert adaptation.isSingleProgramDump(program_buffer)
                edit_buffer = adaptation.convertToEditBuffer(0x00, program_buffer)
            else:
                program_buffer = program
                edit_buffer = None
            if hasattr(adaptation, "numberFromDump"):
                assert adaptation.numberFromDump(program_buffer) == 11
            if hasattr(adaptation, "nameFromDump") and edit_buffer is not None:
                assert adaptation.nameFromDump(program_buffer) == adaptation.nameFromDump(edit_buffer)
    else:
        pytest.skip(f"{adaptation.name} has not implemented convertToEditBuffer")


@skip_targets("test_data")
def test_number_from_dump(adaptation, test_data: TestData):
    if hasattr(adaptation, "numberFromDump"):
        for program in test_data.programs:
            assert adaptation.numberFromDump(program["message"]) == program["number"]
    else:
        pytest.skip(f"{adaptation.name} has not implemented numberFromDump")


@skip_targets("test_data")
def test_layer_name(adaptation, test_data: TestData):
    if hasattr(adaptation, "layerName"):
        for program in test_data.programs:
            assert adaptation.layerName(program["message"], 1) == program["second_layer_name"]
            assert adaptation.isSingleProgramDump(program["message"])
            new_messages = adaptation.setLayerName(program["message"], 1, 'changed layer')
            assert adaptation.layerName(new_messages, 1) == 'changed layer'
            assert adaptation.isSingleProgramDump(new_messages)
    else:
        pytest.skip(f"{adaptation.name} has not implemented layerName")


@skip_targets("test_data")
def test_fingerprinting(adaptation, test_data: TestData):
    if hasattr(adaptation, "calculateFingerprint"):
        for program in test_data.programs:
            md5 = adaptation.calculateFingerprint(program["message"])
            blank1 = None
            if hasattr(adaptation, "blankedOut"):
                blank1 = adaptation.blankedOut(program["message"])
            if hasattr(adaptation, "isSingleProgramDump") and hasattr(adaptation, "convertToProgramDump") and adaptation.isSingleProgramDump(
                    program["message"]):
                # Change program place and make sure the fingerprint didn't change
                changed_position = adaptation.convertToProgramDump(0x09, program["message"], 0x31)
                if hasattr(adaptation, "blankedOut"):
                    blank2 = adaptation.blankedOut(changed_position)
                    assert knobkraft.list_compare(blank1, blank2)
                assert adaptation.calculateFingerprint(changed_position) == md5
            # Change name and make sure the fingerprint didn't change
            if hasattr(adaptation, "renamePatch"):
                renamed = adaptation.renamePatch(program["message"], "iixxoo")
                assert adaptation.calculateFingerprint(renamed) == md5
    else:
        pytest.skip(f"{adaptation.name} has not implemented calculateFingerprint")


@skip_targets("test_data")
def test_device_detection(adaptation, test_data: TestData):
    found = False
    if "device_detect_call" in test_data.test_dict:
        assert adaptation.createDeviceDetectMessage(0x00) == knobkraft.stringToSyx(test_data.test_dict["device_detect_call"])
        found = True
    if "device_detect_reply" in test_data.test_dict:
        assert adaptation.channelIfValidDeviceResponse(knobkraft.stringToSyx(test_data.test_dict["device_detect_reply"])) == 0x00
        found = True
    if not found:
        pytest.skip(f"{adaptation.name} has not provided test data for the device_detect_call or the device_detect_reply")


@skip_targets("test_data")
def test_program_dump_request(adaptation, test_data: TestData):
    if "program_dump_request" in test_data.test_dict:
        assert knobkraft.list_compare(adaptation.createProgramDumpRequest(0x00, 0x00),
                                      knobkraft.stringToSyx(test_data.test_dict["program_dump_request"]))
    else:
        pytest.skip(f"{adaptation.name} has not provided test data for the program_dump_request")


@skip_targets("test_data")
def test_friendly_bank_name(adaptation, test_data: TestData):
    if "friendly_bank_name" in test_data.test_dict:
        bank_data = test_data.test_dict["friendly_bank_name"]
        assert adaptation.friendlyBankName(bank_data[0]) == bank_data[1]
    else:
        pytest.skip(f"{adaptation.name} has not implemented friendly_bank_name")
