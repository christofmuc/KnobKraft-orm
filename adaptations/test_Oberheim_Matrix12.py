from pathlib import Path

import knobkraft

import Oberheim_Matrix12 as adaptation
from testing.librarian import Librarian


FIXTURE_DIR = Path(__file__).resolve().parent / "testData" / "Oberheim_Xpander"


def _fixture_messages(name: str):
    return knobkraft.load_sysex(str(FIXTURE_DIR / name))


def test_factory_voice_bank_loads_as_100_programs():
    messages = _fixture_messages("rubidium_FACTORY2_M12.syx")
    programs = [message for message in messages if adaptation.isSingleProgramDump(message)]

    assert len(messages) == 100
    assert len(programs) == 100
    assert adaptation.numberFromDump(programs[0]) == 0
    assert adaptation.nameFromDump(programs[0]) == "TOTAHORN"
    assert adaptation.numberFromDump(programs[-1]) == 99
    assert adaptation.nameFromDump(programs[-1]) == "BLANK"


def test_mixed_matrix12_file_contains_voice_and_multi_programs():
    messages = _fixture_messages("presetpatch_Matrix_12.syx")
    voice_programs = [message for message in messages if adaptation.isSingleProgramDump(message)]
    multi_programs = [message for message in messages if adaptation._is_multi_patch_message(message)]
    other_messages = [message for message in messages if not adaptation.isSingleProgramDump(message) and not adaptation._is_multi_patch_message(message)]

    assert len(messages) == 201
    assert len(voice_programs) == 100
    assert len(multi_programs) == 100
    assert other_messages == [[0xF0, 0x10, 0x02, 0x0B, 0x05, 0x00, 0xF7]]


def test_librarian_loads_only_voice_programs_from_mixed_matrix12_file():
    messages = _fixture_messages("presetpatch_Matrix_12.syx")
    patches = Librarian().load_sysex(adaptation, messages)
    patches.sort(key=adaptation.numberFromDump)

    assert len(patches) == 100
    assert adaptation.numberFromDump(patches[0]) == 0
    assert adaptation.nameFromDump(patches[0]) == "TOTOHORN"
    assert adaptation.numberFromDump(patches[-1]) == 99
    assert adaptation.nameFromDump(patches[-1]) == "FLINGS 2"


def test_rename_and_fingerprint_ignore_name_and_program_slot_on_real_matrix12_patch():
    message = _fixture_messages("rubidium_FACTORY2_M12.syx")[0]
    renamed = adaptation.renamePatch(message, "M12TEST")
    moved = adaptation.convertToProgramDump(0, renamed, 42)

    assert adaptation.nameFromDump(renamed) == "M12TEST"
    assert adaptation.numberFromDump(moved) == 42
    assert adaptation.calculateFingerprint(message) == adaptation.calculateFingerprint(renamed)
    assert adaptation.calculateFingerprint(message) == adaptation.calculateFingerprint(moved)
