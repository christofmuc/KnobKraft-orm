#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import Optional

import pytest
import knobkraft
import testing
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


@pytest.fixture
def test_data(request) -> Optional[testing.TestData]:
    adaptation = request.getfixturevalue("adaptation")
    if hasattr(adaptation, "test_data"):
        return adaptation.test_data()
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
def test_extract_name(adaptation, test_data: testing.TestData):
    if hasattr(adaptation, "nameFromDump") and test_data is not None:
        for program in test_data.programs:
            assert adaptation.nameFromDump(program.message.byte_list) == program.name
    else:
        pytest.skip(f"{adaptation.name} has not implemented nameFromDump")


@skip_targets("test_data")
def test_rename(adaptation, test_data: testing.TestData):
    if hasattr(adaptation, "nameFromDump") and hasattr(adaptation, "renamePatch"):
        for program in test_data.programs:
            binary = program.message.byte_list
            # Rename to the name it already has
            renamed = adaptation.renamePatch(binary, adaptation.nameFromDump(binary))
            # This should not change the extracted name
            assert adaptation.nameFromDump(renamed) == adaptation.nameFromDump(binary)
            # This should not change the fingerprint
            if hasattr(adaptation, "calculateFingerprint"):
                assert adaptation.calculateFingerprint(renamed) == adaptation.calculateFingerprint(binary)
            # Now rename
            if program.rename_name is not None:
                new_name = program.rename_name
            else:
                new_name = "new name"
            with_new_name = adaptation.renamePatch(binary, new_name)
            new_name = adaptation.nameFromDump(with_new_name)
            assert new_name == new_name
    else:
        pytest.skip(f"{adaptation.name} has not implemented nameFromDump and renamePatch")


@skip_targets("test_data")
def test_is_program_dump(adaptation, test_data: testing.TestData):
    for program in test_data.programs:
        if program.is_edit_buffer:
            assert adaptation.isEditBufferDump(program.message.byte_list)
        else:
            assert adaptation.isSingleProgramDump(program.message.byte_list)


@skip_targets("test_data")
def test_convert_to_edit_buffer(adaptation, test_data: testing.TestData):
    if hasattr(adaptation, "convertToEditBuffer") or hasattr(adaptation, "convertToProgramDump"):
        for program_data in test_data.programs:
            if program_data.target_no is not None:
                target_no = program_data.target_no
            else:
                # Choose something random
                target_no = 11
            program = program_data.message.byte_list
            if hasattr(adaptation, "isSingleProgramDump") and adaptation.isSingleProgramDump(program):
                previous_number = 0
                if hasattr(adaptation, "numberFromDump"):
                    previous_number = adaptation.numberFromDump(program)
                if hasattr(adaptation, "convertToEditBuffer"):
                    edit_buffer = adaptation.convertToEditBuffer(0x00, program)
                    if test_data.convert_to_edit_buffer_produces_program_dump:
                        # This is a special case for the broken implementation of the Andromeda edit buffer
                        assert adaptation.isSingleProgramDump(edit_buffer)
                    else:
                        assert adaptation.isEditBufferDump(edit_buffer)
                if not hasattr(adaptation, "convertToProgramDump"):
                    # Not much more we can test here
                    return
                if test_data.not_idempotent:
                    pass
                else:
                    idempotent = adaptation.convertToProgramDump(0x00, program, previous_number)
                    assert knobkraft.list_compare(idempotent, program)
                if hasattr(adaptation, "convertToEditBuffer"):
                    program_buffer = adaptation.convertToProgramDump(0x00, edit_buffer, target_no)
                else:
                    program_buffer = adaptation.convertToProgramDump(0x00, program, target_no)
            elif hasattr(adaptation, "isEditBufferDump") and adaptation.isEditBufferDump(program):
                program_buffer = adaptation.convertToProgramDump(0x00, program, target_no)
                assert adaptation.isSingleProgramDump(program_buffer)
                edit_buffer = adaptation.convertToEditBuffer(0x00, program_buffer)
            else:
                program_buffer = program
                edit_buffer = None
            if hasattr(adaptation, "numberFromDump"):
                assert adaptation.numberFromDump(program_buffer) == target_no
            if hasattr(adaptation, "nameFromDump") and edit_buffer is not None:
                assert adaptation.nameFromDump(program_buffer) == adaptation.nameFromDump(edit_buffer)
    else:
        pytest.skip(f"{adaptation.name} has not implemented convertToEditBuffer")


@skip_targets("test_data")
def test_number_from_dump(adaptation, test_data: testing.TestData):
    if hasattr(adaptation, "numberFromDump"):
        for program in test_data.programs:
            assert adaptation.numberFromDump(program.message.byte_list) == program.number
    else:
        pytest.skip(f"{adaptation.name} has not implemented numberFromDump")


@skip_targets("test_data")
def test_layer_name(adaptation, test_data: testing.TestData):
    if hasattr(adaptation, "layerName"):
        for program in test_data.programs:
            assert adaptation.layerName(program.message.byte_list, 1) == program.second_layer_name
            assert adaptation.isSingleProgramDump(program.message.byte_list)
            new_messages = adaptation.setLayerName(program.message.byte_list, 1, 'changed layer')
            assert adaptation.layerName(new_messages, 1) == 'changed layer'
            assert adaptation.isSingleProgramDump(new_messages)
    else:
        pytest.skip(f"{adaptation.name} has not implemented layerName")


@skip_targets("test_data")
def test_fingerprinting(adaptation, test_data: testing.TestData):
    if hasattr(adaptation, "calculateFingerprint"):
        for program in test_data.programs:
            md5 = adaptation.calculateFingerprint(program.message.byte_list)
            blank1 = None
            if hasattr(adaptation, "blankedOut"):
                blank1 = adaptation.blankedOut(program.message.byte_list)
            if hasattr(adaptation, "isSingleProgramDump") and hasattr(adaptation, "convertToProgramDump") and adaptation.isSingleProgramDump(
                    program.message.byte_list):
                # Change program place and make sure the fingerprint didn't change
                changed_position = adaptation.convertToProgramDump(0x09, program.message.byte_list, 0x31)
                if hasattr(adaptation, "blankedOut"):
                    blank2 = adaptation.blankedOut(changed_position)
                    assert knobkraft.list_compare(blank1, blank2)
                assert adaptation.calculateFingerprint(changed_position) == md5
            # Change name and make sure the fingerprint didn't change
            if hasattr(adaptation, "renamePatch"):
                renamed = adaptation.renamePatch(program.message.byte_list, "iixxoo")
                assert adaptation.calculateFingerprint(renamed) == md5
    else:
        pytest.skip(f"{adaptation.name} has not implemented calculateFingerprint")


@skip_targets("test_data")
def test_device_detection(adaptation, test_data: testing.TestData):
    found = False
    if test_data.device_detect_call is not None:
        assert adaptation.createDeviceDetectMessage(0x00) == test_data.device_detect_call
        found = True
    if test_data.device_detect_reply is not None:
        assert adaptation.channelIfValidDeviceResponse(test_data.device_detect_reply[0].byte_list) == test_data.device_detect_reply[1]
        found = True
    if not found:
        pytest.skip(f"{adaptation.name} has not provided test data for the device_detect_call or the device_detect_reply")


@skip_targets("test_data")
def test_program_dump_request(adaptation, test_data: testing.TestData):
    if test_data.program_dump_request is not None:
        assert knobkraft.list_compare(adaptation.createProgramDumpRequest(0x00, 0x00), test_data.program_dump_request.byte_list)
    else:
        pytest.skip(f"{adaptation.name} has not provided test data for the program_dump_request")


@skip_targets("test_data")
def test_friendly_bank_name(adaptation, test_data: testing.TestData):
    if test_data.friendly_bank_name is not None:
        assert adaptation.friendlyBankName(test_data.friendly_bank_name[0]) == test_data.friendly_bank_name[1]
    else:
        pytest.skip(f"{adaptation.name} has not implemented friendly_bank_name")
