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


def require_testdata(test_data_field):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            paramA = kwargs['test_data']
            if paramA is None:
                pytest.skip('No test_data provided, skipping')
            if not hasattr(paramA, test_data_field) or getattr(paramA, test_data_field) is None or getattr(paramA, test_data_field) == []:
                pytest.skip(f'test_data does not provide "{test_data_field}", skipping')
            func(*args, **kwargs)

        return wrapper

    return decorator


def require_implemented(function_implemented):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            adaptation_module = kwargs['adaptation']
            if adaptation_module is None:
                raise Exception("No adaptation parameter specified, can't check require")
            if not hasattr(adaptation_module, function_implemented):
                pytest.skip(f'{function_implemented} is missing in adaptation, skipping')
            func(*args, **kwargs)

        return wrapper

    return decorator


@pytest.fixture
def test_data(request) -> Optional[testing.TestData]:
    adaptation = request.getfixturevalue("adaptation")
    if hasattr(adaptation, "make_test_data"):
        return adaptation.make_test_data()
    else:
        return None


#
# These are generic tests every adaptation must pass
#
def test_name_is_not_none(adaptation):
    name = adaptation.name()
    assert name is not None
    assert isinstance(name, str)


@require_implemented("nameFromDump")
@require_testdata("programs")
def test_extract_name_from_program(adaptation, test_data: testing.TestData):
    count = 0
    # Loop all programs created by the generator, and if name is given check that we can extract it!
    for program in test_data.programs:
        if hasattr(program, "name") and program.name is not None:
            assert adaptation.nameFromDump(program.message.byte_list) == program.name
            count += 1
    if count == 0:
        # Nothing was generated that has a name attached, but nameFromDump was implemented. Fail test!
        pytest.fail(f"{adaptation.name()} did not generate a single program with name to test nameFromDump")


@require_implemented("nameFromDump")
@require_testdata("edit_buffers")
def test_extract_name_from_edit_buffer(adaptation, test_data: testing.TestData):
    count = 0
    # Loop all programs created by the generator, and if name is given check that we can extract it!
    for program in test_data.edit_buffers:
        if hasattr(program, "name") and program.name is not None:
            assert adaptation.nameFromDump(program.message.byte_list) == program.name
            count += 1
    if count == 0:
        # Nothing was generated that has a name attached, but nameFromDump was implemented. Fail test!
        pytest.fail(f"{adaptation.name()} did not generate a single program with name to test nameFromDump")


def get_rename_target_name(program, test_data):
    if hasattr(program, "rename_name") and program.rename_name is not None:
        return program.rename_name
    elif hasattr(test_data, "rename_name") and test_data.rename_name is not None:
        # Or we have specified one name for all programs?
        return test_data.rename_name
    else:
        # We have specified nothing specific in make_test_data, just use "new name"
        return "new name"


@require_implemented("nameFromDump")
@require_implemented("renamePatch")
@require_testdata("programs")
def test_rename_programs(adaptation, test_data: testing.TestData):
    for program in test_data.programs:
        if program.dont_rename:
            continue
        binary = program.message.byte_list
        # Rename to the name it already has
        renamed = adaptation.renamePatch(binary, adaptation.nameFromDump(binary))
        # This should not change the extracted name
        assert knobkraft.list_compare(binary, renamed)
        assert adaptation.nameFromDump(renamed) == adaptation.nameFromDump(binary)
        # Now rename. We might have a specific name specified for this program to use
        new_name = get_rename_target_name(program, test_data)
        with_new_name = adaptation.renamePatch(binary, new_name)
        renamed_name = adaptation.nameFromDump(with_new_name)
        # Here, we need to strip because the target name specified might not contain enough spaces!
        assert new_name.strip() == renamed_name.strip()


@require_implemented("nameFromDump")
@require_implemented("renamePatch")
@require_testdata("edit_buffers")
def test_rename_edit_buffers(adaptation, test_data: testing.TestData):
    for program in test_data.edit_buffers:
        if program.dont_rename:
            continue
        binary = program.message.byte_list
        # Rename to the name it already has
        renamed = adaptation.renamePatch(binary, adaptation.nameFromDump(binary))
        # This should not change the extracted name
        assert adaptation.nameFromDump(renamed) == adaptation.nameFromDump(binary)
        # Now rename. We might have a specific name specified for this program to use
        new_name = get_rename_target_name(program, test_data)
        with_new_name = adaptation.renamePatch(binary, new_name)
        renamed_name = adaptation.nameFromDump(with_new_name)
        # Here, we need to strip because the target name specified might not contain enough spaces!
        assert new_name.strip() == renamed_name.strip()


@require_implemented("nameFromDump")
@require_implemented("renamePatch")
@require_implemented("calculateFingerprint")
@require_testdata("programs")
def test_rename_should_not_change_fingerprint(adaptation, test_data: testing.TestData):
    for program in test_data.programs:
        if program.dont_rename:
            continue
        binary = program.message.byte_list
        new_name = get_rename_target_name(program, test_data)
        renamed = adaptation.renamePatch(binary, new_name)
        # This should not change the fingerprint
        assert adaptation.calculateFingerprint(renamed) == adaptation.calculateFingerprint(binary)


@require_implemented("isEditBufferDump")
@require_testdata("edit_buffers")
def test_is_edit_buffer(adaptation, test_data: testing.TestData):
    count = 0
    for program in test_data.edit_buffers:
        assert adaptation.isEditBufferDump(program.message.byte_list)
        count += 1
    if count == 0:
        # Nothing was generated
        pytest.fail(f"{adaptation.name()} did not generate a single edit buffer to test isEditBufferDump")


@require_implemented("isSingleProgramDump")
@require_testdata("programs")
def test_is_program_dump(adaptation, test_data: testing.TestData):
    count = 0
    for program in test_data.programs:
        assert adaptation.isSingleProgramDump(program.message.byte_list)
        count += 1
    if count == 0:
        # Nothing was generated. Maybe this should fail the test?
        pytest.fail(f"{adaptation.name} did not generate a single program buffer to test")


@require_implemented("convertToEditBuffer")
@require_testdata("programs")
def test_convert_programs_to_edit_buffer(adaptation, test_data: testing.TestData):
    for program_data in test_data.programs:
        edit_buffer = adaptation.convertToEditBuffer(0x00, program_data.message.byte_list)
        if test_data.convert_to_edit_buffer_produces_program_dump:
            # This is a special case for the broken implementation of the Andromeda edit buffer
            assert adaptation.isSingleProgramDump(edit_buffer)
        else:
            assert adaptation.isEditBufferDump(edit_buffer)


@require_implemented("convertToEditBuffer")
@require_testdata("edit_buffers")
def test_convert_edit_buffers_to_edit_buffers(adaptation, test_data: testing.TestData):
    # Now try to convert program buffers to edit buffers and back to program buffers
    for buffer in test_data.edit_buffers:
        edit_buffer = adaptation.convertToEditBuffer(0x00, buffer.message.byte_list)
        if test_data.convert_to_edit_buffer_produces_program_dump:
            # This is a special case for the broken implementation of the Andromeda edit buffer
            assert adaptation.isSingleProgramDump(edit_buffer)
        else:
            assert adaptation.isEditBufferDump(edit_buffer)


def get_target_program_no(program: testing.ProgramTestData):
    if program.target_no is not None:
        return program.target_no
    else:
        # Choose something random that hopefully all synths have
        return 11


@require_implemented("convertToProgramDump")
@require_implemented("isSingleProgramDump")
@require_testdata("edit_buffers")
def test_convert_edit_buffers_to_programs(adaptation, test_data: testing.TestData):
    for edit_buffer in test_data.edit_buffers:
        target_no = get_target_program_no(edit_buffer)
        program_buffer = adaptation.convertToProgramDump(0x00, edit_buffer.message.byte_list, target_no)

        # Make sure neither number nor name were changed when extraction functions are implemented
        assert adaptation.isSingleProgramDump(program_buffer)
        if hasattr(adaptation, "numberFromDump"):
            assert adaptation.numberFromDump(program_buffer) == target_no
        if hasattr(adaptation, "nameFromDump"):
            assert adaptation.nameFromDump(program_buffer) == adaptation.nameFromDump(edit_buffer.message.byte_list)


@require_implemented("convertToProgramDump")
@require_implemented("isSingleProgramDump")
@require_testdata("programs")
def test_convert_programs_to_programs(adaptation, test_data: testing.TestData):
    if test_data.not_idempotent:
        pytest.skip(f"Skipping program to program conversion test for {adaptation.name()} as it indicates this conversion is not idempotent")

    for program_buffer in test_data.programs:
        target_no = get_target_program_no(program_buffer)
        rebuild_program = adaptation.convertToProgramDump(0x00, program_buffer.message.byte_list, target_no)

        # Make sure neither number nor name were changed when extraction functions are implemented
        assert adaptation.isSingleProgramDump(rebuild_program)
        if hasattr(adaptation, "numberFromDump"):
            assert adaptation.numberFromDump(rebuild_program) == target_no
        if hasattr(adaptation, "nameFromDump"):
            assert adaptation.nameFromDump(rebuild_program) == adaptation.nameFromDump(program_buffer.message.byte_list)


@require_implemented("numberFromDump")
@require_testdata("programs")
def test_number_from_dump(adaptation, test_data: testing.TestData):
    count = 0
    for program in test_data.programs:
        if program.number is not None:
            assert adaptation.numberFromDump(program.message.byte_list) == program.number
            count += 1
    if count == 0:
        pytest.fail(f"{adaptation.name()} has not created a single program with program number to test numberFromDump")


@require_implemented("layerName")
@require_testdata("programs")
def test_layer_name(adaptation, test_data: testing.TestData):
    count = 0
    for program in test_data.programs:
        if program.second_layer_name is not None:
            assert adaptation.layerName(program.message.byte_list, 1) == program.second_layer_name
            count += 1
    if count == 0:
        pytest.fail(f"{adaptation.name} has not created a single program and given the second_layer_name to test layerName")


@require_implemented("layerName")
@require_implemented("setLayerName")
@require_testdata("programs")
def test_set_layer_name(adaptation, test_data: testing.TestData):
    count = 0
    for program in test_data.programs:
        old_layer_name = adaptation.layerName(program.message.byte_list, 0)
        new_program = adaptation.setLayerName(program.message.byte_list, 0, old_layer_name)
        assert knobkraft.list_compare(program.message.byte_list, new_program)
        new_messages = adaptation.setLayerName(program.message.byte_list, 1, 'changed layer')
        assert adaptation.layerName(new_messages, 1) == 'changed layer'
        new_messages = adaptation.setLayerName(program.message.byte_list, 0, 'changed layer')
        assert adaptation.layerName(new_messages, 0) == 'changed layer'
        count += 1
    if count == 0:
        pytest.fail(f"{adaptation.name} has not created a single program to test set_layer_name")


@require_implemented("calculateFingerprint")
@require_implemented("convertToProgramDump")
@require_implemented("isSingleProgramDump")
@require_testdata("programs")
def test_fingerprinting_of_programs(adaptation, test_data: testing.TestData):
    for program in test_data.programs:
        md5 = adaptation.calculateFingerprint(program.message.byte_list)
        # Change program place and make sure the fingerprint didn't change
        changed_position = adaptation.convertToProgramDump(0x09, program.message.byte_list, 0x31)
        assert adaptation.calculateFingerprint(changed_position) == md5


@require_implemented("calculateFingerprint")
@require_implemented("convertToProgramDump")
@require_implemented("isSingleProgramDump")
@require_implemented("blankedOut")
@require_testdata("programs")
def test_blanked_out(adaptation, test_data: testing.TestData):
    # The blank out is a generic mechanism to implement fingerprinting so names/program places and the like don't change thr fingerprint
    # But it is not required, just a helpful notion when implementing this. The advantage here is that the test is more
    # meaningful in showing where the blank out fails than if you get just two different md5...
    for program in test_data.programs:
        blank1 = adaptation.blankedOut(program.message.byte_list)
        # Change program place and make sure the fingerprint didn't change
        changed_position = adaptation.convertToProgramDump(0x09, program.message.byte_list, 0x31)
        blank2 = adaptation.blankedOut(changed_position)
        assert knobkraft.list_compare(blank1, blank2)


@require_implemented("calculateFingerprint")
@require_testdata("edit_buffers")
def test_fingerprinting_of_edit_buffers(adaptation, test_data: testing.TestData):
    for program in test_data.edit_buffers:
        # We can't do much, just make sure that an md5 is calculated
        md5 = adaptation.calculateFingerprint(program.message.byte_list)
        assert md5 is not None


@require_implemented("createDeviceDetectMessage")
@require_testdata("device_detect_call")
def test_device_detect_message(adaptation, test_data: testing.TestData):
    assert adaptation.createDeviceDetectMessage(0x00) == test_data.device_detect_call


@require_implemented("channelIfValidDeviceResponse")
@require_testdata("device_detect_reply")
def test_device_detect_reply(adaptation, test_data: testing.TestData):
    assert adaptation.channelIfValidDeviceResponse(test_data.device_detect_reply[0].byte_list) == test_data.device_detect_reply[1]


@require_implemented("createProgramDumpRequest")
@require_testdata("program_dump_request")
def test_program_dump_request(adaptation, test_data: testing.TestData):
    if isinstance(test_data.program_dump_request, tuple):
        assert knobkraft.list_compare(adaptation.createProgramDumpRequest(test_data.program_dump_request[0], test_data.program_dump_request[1]),
                                      test_data.program_dump_request[2].byte_list)
    else:
        assert knobkraft.list_compare(adaptation.createProgramDumpRequest(0x00, 0x00),
                                      test_data.program_dump_request.byte_list)


@require_implemented("friendlyBankName")
@require_testdata("friendly_bank_name")
def test_friendly_bank_name(adaptation, test_data: testing.TestData):
    assert adaptation.friendlyBankName(test_data.friendly_bank_name[0]) == test_data.friendly_bank_name[1]


@require_implemented("extractPatchesFromBank")
@require_testdata("banks")
def test_extract_patches_from_bank(adaptation, test_data: testing.TestData):
    for bank in test_data.banks:
        if adaptation.isPartOfBankDump(bank):
            patches = knobkraft.splitSysex(adaptation.extractPatchesFromBank(bank))
            assert len(patches) > 0
            for patch in patches:
                # TODO: This seems like a peculiar assumption, that extracted patches are always Single Program Dumps
                # unless the synth only supports edit buffer dumps
                if hasattr(adaptation, "isSingleProgramDump"):
                    assert adaptation.isSingleProgramDump(patch)
                else:
                    assert adaptation.isEditBufferDump(patch)
        else:
            print(f"This is not a bank dump: {bank}")
