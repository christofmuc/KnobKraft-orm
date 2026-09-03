import importlib.util
from pathlib import Path

import knobkraft


def _load_adaptation():
    path = Path(__file__).parent / "Ensoniq Mirage (Soundprocess).py"
    spec = importlib.util.spec_from_file_location("Ensoniq_Mirage_Soundprocess", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


mirage = _load_adaptation()


def _program(patch_number=7):
    raw_patch = list(range(70))
    raw_patch[62:70] = b"SYNTHETC"
    payload = [nibble for value in raw_patch for nibble in (value & 0x0F, value >> 4)]
    return mirage.SYSEX_PREFIX + [mirage.WRITE_PATCH, patch_number] + payload + [0xF7]


def test_documented_70_byte_patch_block_produces_148_byte_message():
    message = _program()

    assert len(message) == 148
    assert mirage.isSingleProgramDump(message)
    assert mirage.numberFromDump(message) == 6
    assert mirage.nameFromDump(message) == "SYNTHETC"


def test_parser_rejects_wrong_header_command_patch_number_and_payload():
    message = _program()

    wrong_header = [*message]
    wrong_header[3] = 0x24
    wrong_command = [*message]
    wrong_command[5] = mirage.READ_PATCH
    wrong_number = [*message]
    wrong_number[6] = 0
    non_nibble_data = [*message]
    non_nibble_data[10] = 0x10

    assert not mirage.isSingleProgramDump(wrong_header)
    assert not mirage.isSingleProgramDump(wrong_command)
    assert not mirage.isSingleProgramDump(wrong_number)
    assert not mirage.isSingleProgramDump(non_nibble_data)
    assert not mirage.isSingleProgramDump(message + [0x00])


def test_edit_buffer_conversion_retargets_patch_one_without_prepending_cc():
    message = _program(23)

    converted = mirage.convertToEditBuffer(0, message)

    assert len(converted) == 148
    assert converted[6] == 1
    assert mirage.isEditBufferDump(converted)
    assert converted[7:147] == message[7:147]
    assert mirage.calculateFingerprint(converted) == mirage.calculateFingerprint(message)


def test_program_conversion_retargets_requested_slot():
    converted = mirage.convertToProgramDump(0, _program(1), 47)

    assert converted[6] == 48
    assert mirage.numberFromDump(converted) == 47


def test_bank_extraction_filters_control_and_malformed_messages():
    first = _program(1)
    second = _program(2)
    control = mirage.SYSEX_PREFIX + [mirage.COMPUTER_CONTROL, 0xF7]
    malformed = [*first]
    malformed[-1] = 0x00
    messages = [control, first, malformed, second]

    assert mirage.extractPatchesFromAllBankMessages(messages) == [first, second]
    assert knobkraft.splitSysex(mirage.extractPatchesFromBank(messages)) == [first, second]
