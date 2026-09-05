from pathlib import Path

import knobkraft

import Oberheim_Xpander as adaptation
from testing.librarian import Librarian


FIXTURE_DIR = Path(__file__).resolve().parent / "testData" / "Oberheim_Xpander"
REAL_BANK_FIXTURES = {
    "rubidium_FACTORY1_XP.syx": {"count": 100, "first": (0, "SYNCOM 8"), "last": (99, "BLANK"), "missing": []},
    "rubidium_FACTORY2_M12.syx": {"count": 100, "first": (0, "TOTAHORN"), "last": (99, "BLANK"), "missing": []},
    "rubidium_SANCHEZ.syx": {"count": 98, "first": (0, "PUNCHY"), "last": (99, "MOGASAUR"), "missing": [42, 59]},
    "rubidium_SPRINGER.syx": {"count": 100, "first": (0, "MONSTER"), "last": (99, "ALIENS"), "missing": []},
    "rubidium_JOHNSON1.syx": {"count": 100, "first": (0, "MARIMBIX"), "last": (99, "BASIC"), "missing": []},
    "rubidium_URBANEK.syx": {"count": 100, "first": (0, "DEFAULT"), "last": (99, "PATTERN"), "missing": []},
    "rubidium_XP_94.syx": {"count": 100, "first": (0, "OBERHEIM"), "last": (99, "BASIC"), "missing": []},
    "presetpatch_Matrix_12.syx": {"count": 100, "first": (0, "TOTOHORN"), "last": (99, "FLINGS 2"), "missing": []},
}


def _bank_messages():
    return [adaptation.convertToProgramDump(0, adaptation.SAMPLE_PROGRAM, patch_no) for patch_no in range(adaptation.PATCH_COUNT)]


def _fixture_messages(name: str):
    return knobkraft.load_sysex(str(FIXTURE_DIR / name))


def _recognized_programs(name: str):
    programs = [message for message in _fixture_messages(name) if adaptation._is_single_program_message(message)]
    programs.sort(key=adaptation.numberFromDump)
    return programs


def _reset_program_mode():
    adaptation.createProgramDumpRequest(0, 0)


def test_sample_program_shape_and_name_round_trip():
    message = adaptation.SAMPLE_PROGRAM
    assert adaptation.isSingleProgramDump(message)
    assert len(message) == adaptation.PROGRAM_LENGTH
    assert len(message[adaptation.RAW_DATA_START:adaptation.RAW_DATA_END]) == 392
    assert len(adaptation._drop_odd_bytes(message[adaptation.RAW_DATA_START:adaptation.RAW_DATA_END])) == 196
    assert adaptation.nameFromDump(message) == "WEIRDLFO"


def test_rename_and_fingerprint_ignore_name_and_program_slot():
    original = adaptation.SAMPLE_PROGRAM
    renamed = adaptation.renamePatch(original, "weirdlfp")
    moved = adaptation.convertToProgramDump(0, renamed, 42)

    assert adaptation.nameFromDump(renamed) == "WEIRDLFP"
    assert adaptation.numberFromDump(moved) == 42
    assert adaptation.calculateFingerprint(original) == adaptation.calculateFingerprint(renamed)
    assert adaptation.calculateFingerprint(original) == adaptation.calculateFingerprint(moved)


def test_request_opcodes_and_scratch_slot_follow_spec():
    assert adaptation.createProgramDumpRequest(0, 9) == [0xF0, 0x10, 0x02, 0x00, 0x00, 0x09, 0xF7]
    assert adaptation.createBankDumpRequest(0, 0) == [0xF0, 0x10, 0x02, 0x02, 0x00, 0xF7]
    assert adaptation.defaultProgramPlace() == 99
    _reset_program_mode()


def test_bank_dump_extraction_waits_for_all_100_programs():
    partial = _bank_messages()[:10]
    complete = _bank_messages()

    assert not adaptation.isBankDumpFinished(partial)
    assert adaptation.isBankDumpFinished(complete)

    adaptation.createBankDumpRequest(0, 0)
    patches = adaptation.extractPatchesFromAllBankMessages(complete)
    assert len(patches) == adaptation.PATCH_COUNT
    assert adaptation.numberFromDump(patches[0]) == 0
    assert adaptation.numberFromDump(patches[-1]) == adaptation.PATCH_COUNT - 1
    assert all(adaptation.isSingleProgramDump(patch) for patch in patches)


def test_real_single_file_contains_program_dump_and_trailing_command():
    messages = _fixture_messages("rubidium_single_09_weirdlfo.syx")
    assert len(messages) == 2
    assert adaptation._is_single_program_message(messages[0])
    assert adaptation.nameFromDump(messages[0]) == "WEIRDLFO"
    assert adaptation.numberFromDump(messages[0]) == 99
    assert messages[1] == [0xF0, 0x10, 0x02, 0x0B, 0x05, 0x00, 0xF7]

    patches = Librarian().load_sysex(adaptation, messages)
    assert len(patches) == 1
    assert adaptation.nameFromDump(patches[0]) == "WEIRDLFO"


def test_real_bank_fixtures_have_expected_recognized_program_counts():
    for name, expected in REAL_BANK_FIXTURES.items():
        programs = _recognized_programs(name)
        slots = [adaptation.numberFromDump(program) for program in programs]
        assert len(programs) == expected["count"], name
        assert len(set(slots)) == expected["count"], name
        assert [slot for slot in range(100) if slot not in slots] == expected["missing"], name
        assert (adaptation.numberFromDump(programs[0]), adaptation.nameFromDump(programs[0])) == expected["first"], name
        assert (adaptation.numberFromDump(programs[-1]), adaptation.nameFromDump(programs[-1])) == expected["last"], name


def test_real_bank_fixtures_extract_expected_programs_in_bank_context():
    for name, expected in REAL_BANK_FIXTURES.items():
        adaptation.createBankDumpRequest(0, 0)
        patches = adaptation.extractPatchesFromAllBankMessages(_fixture_messages(name))
        slots = [adaptation.numberFromDump(patch) for patch in patches]
        assert len(patches) == expected["count"], name
        assert len(set(slots)) == expected["count"], name
        assert [slot for slot in range(100) if slot not in slots] == expected["missing"], name
        assert (adaptation.numberFromDump(patches[0]), adaptation.nameFromDump(patches[0])) == expected["first"], name
        assert (adaptation.numberFromDump(patches[-1]), adaptation.nameFromDump(patches[-1])) == expected["last"], name
