from pathlib import Path

import pytest

import knobkraft
from conftest import load_adaptation
from testing.librarian import Librarian, SynthBank
from testing.mock_midi import MockMidiController, ScriptedMockDevice


pro3 = load_adaptation(str(Path(__file__).with_name("Sequential Pro 3.py")))


@pytest.fixture(scope="module")
def patches():
    return knobkraft.load_sysex(str(Path(__file__).parent / "testData" / "P3_Factory_Sounds_v1.01.syx"))


@pytest.mark.parametrize("bank,label", enumerate(["U1", "U2", "U3", "U4", "F5", "F6", "F7", "F8"]))
def test_bank_descriptors_and_display_names(bank, label):
    assert pro3.numberOfBanks() == 8
    assert pro3.numberOfPatchesPerBank() == 128
    descriptors = pro3.bankDescriptors()
    assert len(descriptors) == 8
    assert descriptors[bank] == {
        "bank": bank,
        "name": f"{'User' if bank < 4 else 'Factory'} Bank {label}",
        "size": 128,
        "type": "Single Patch",
        "isROM": bank >= 4,
    }
    assert pro3.friendlyBankName(bank) == label
    assert pro3.friendlyProgramName(bank * 128) == f"{label}-P1"
    assert pro3.friendlyProgramName(bank * 128 + 127) == f"{label}-P128"
    assert SynthBank.start_index_in_bank(pro3, bank) == bank * 128


def test_naming_uses_generic_callbacks():
    assert pro3.friendlyBankName.__self__.friendly_bank_name is pro3.pro3_bank_name
    assert pro3.friendlyProgramName is pro3.pro3_program_name
    for bank in [-1, 8]:
        with pytest.raises(ValueError):
            pro3.friendlyBankName(bank)
    for program in [-1, 1024]:
        with pytest.raises(ValueError):
            pro3.friendlyProgramName(program)


def test_existing_factory_file_names_and_addresses_still_parse(patches):
    assert len(patches) == 512
    assert {pro3.numberFromDump(patch) for patch in patches} == set(range(512))
    assert all(pro3.isSingleProgramDump(patch) for patch in patches)
    assert pro3.nameFromDump(patches[2]) == "Staircase"
    assert all(pro3.nameFromDump(patch).strip() not in ("", "Invalid") for patch in patches)


@pytest.mark.parametrize("bank", range(8))
def test_user_and_factory_bank_request_sequences(patches, bank):
    # Fixture relocated to each bank, not a recording from factory ROM.
    # The request table documents banks 0-7 (User's Guide v1.2, p. 148).
    replies = [[0xF0, 1, 0x31, 2, bank, slot] + patches[slot][6:] for slot in range(128)]
    requests = [[0xF0, 1, 0x31, 5, bank, slot, 0xF7] for slot in range(128)]
    midi = MockMidiController(ScriptedMockDevice({tuple(q): [r] for q, r in zip(requests, replies)}))
    downloaded = []
    Librarian().start_downloading_all_patches(midi, 9, pro3, bank, downloaded.extend)
    midi.drain()
    assert midi.finished
    assert midi.sent_messages == requests
    assert downloaded == replies
    assert [pro3.numberFromDump(patch) for patch in downloaded] == list(range(bank * 128, (bank + 1) * 128))


def test_message_pacing_is_used_for_bank_sends(patches):
    assert pro3.generalMessageDelay() == 50
    midi = MockMidiController(ScriptedMockDevice({tuple(p): [] for p in patches[:128]}))
    Librarian().send_block_of_messages_to_synth(midi, pro3, patches[:128])
    assert midi.sent_messages == patches[:128]
    assert midi.sent_message_delays == [50] * 128


def test_audition_does_not_write_a_factory_or_user_bank(patches):
    patch = patches[2]
    original = patch.copy()
    expected = [0xF0, 1, 0x31, 3] + patch[6:]
    midi = MockMidiController(ScriptedMockDevice({tuple(expected): []}))
    Librarian().send_patch_to_synth(midi, 9, pro3, patch)
    assert midi.sent_messages == [expected]
    assert midi.sent_message_delays == [50]
    assert pro3.calculateFingerprint(expected) == pro3.calculateFingerprint(patch)
    assert patch == original
