from pathlib import Path

import pytest

import knobkraft
import Sequential_Trigon6 as trigon


@pytest.mark.parametrize("name", ["Basic Program", "Basic Program       "])
def test_basic_program_is_a_default_name(name):
    assert trigon.isDefaultName(name)


@pytest.mark.parametrize("name", ["084 CCJ Brass", "Basic Program 2", "Dakota Chorale"])
def test_custom_names_are_not_default_names(name):
    assert not trigon.isDefaultName(name)


def test_renaming_a_bank_preserves_order_addresses_and_fingerprints():
    fixture = Path(__file__).parent / "testData" / "Sequential_Trigon6" / "T6_Programs_v1.0.syx"
    programs = knobkraft.load_sysex(str(fixture))
    assert len(programs) == 500
    # Exercise every slot, including 80-85 from #543, in both dump formats.
    for slot, program in enumerate(programs[:100]):
        assert trigon.numberFromDump(program) == slot
        fingerprint = trigon.calculateFingerprint(program)
        for source in [program, trigon.convertToEditBuffer(0, program)]:
            renamed = trigon.renamePatch(source, f"{slot:03} CCJ Brass")
            assert trigon.nameFromDump(renamed) == f"{slot:03} CCJ Brass"
            assert trigon.calculateFingerprint(renamed) == fingerprint
            restored = trigon.convertToProgramDump(0, renamed, slot)
            assert trigon.numberFromDump(restored) == slot
            assert trigon.nameFromDump(restored) == f"{slot:03} CCJ Brass"
            assert trigon.calculateFingerprint(restored) == fingerprint
