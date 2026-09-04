from pathlib import Path

import pytest

import knobkraft
import sequential
from testing.librarian import Librarian
from testing.mock_midi import MockMidiController, ScriptedMockDevice

import Sequential_Fourm as adaptation

SAMPLE_FILE = Path(__file__).resolve().parent / "testData" / "Sequential_Fourm" / "a_new_legend.syx"


def test_fixture_round_trips_through_dsi_packing():
    message = knobkraft.load_sysex(str(SAMPLE_FILE), as_single_list=True)

    assert adaptation.isSingleProgramDump(message)
    assert adaptation.nameFromDump(message) == "A New Legend"

    unpacked = sequential.GenericSequential.unescapeSysex(message[6:-1])
    repacked = sequential.GenericSequential.escapeSysex(unpacked)

    assert len(unpacked) == 4102
    assert len(repacked) == 4688
    assert message[:6] + repacked + [0xF7] == message


@pytest.mark.parametrize("program", [0, 127, 128, 255, 256, 383, 384, 511])
def test_program_addressing_at_bank_boundaries(program):
    message = knobkraft.load_sysex(str(SAMPLE_FILE), as_single_list=True)
    original = message.copy()
    bank, slot = divmod(program, 128)
    assert adaptation.createProgramDumpRequest(9, program) == [
        0xF0, 0x01, 0x3B, 0x05, bank, slot, 0xF7
    ]
    assert adaptation.bankSelect(9, bank) == [0xB9, 32, bank]
    converted = adaptation.convertToProgramDump(9, message, program)
    assert converted == [0xF0, 0x01, 0x3B, 0x02, bank, slot] + message[6:]
    assert adaptation.numberFromDump(converted) == program
    edit = adaptation.convertToEditBuffer(9, converted)
    assert edit == [0xF0, 0x01, 0x3B, 0x03] + message[6:]
    assert adaptation.convertToProgramDump(9, edit, program) == converted
    assert adaptation.calculateFingerprint(edit) == adaptation.calculateFingerprint(converted)
    assert message == original


def test_broadcast_discovery_and_other_model_rejection():
    assert not adaptation.needsChannelSpecificDetection()
    assert adaptation.createDeviceDetectMessage(0x7F) == [0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7]
    reply = [0xF0, 0x7E, 9, 0x06, 0x02, 0x01, 0x3B, 0, 0, 0, 0, 0, 0xF7]
    assert adaptation.channelIfValidDeviceResponse(reply) == 9
    reply[6] = 0x39  # Trigon-6 is not a Fourm.
    assert adaptation.channelIfValidDeviceResponse(reply) == -1


@pytest.mark.parametrize("bank", range(4))
def test_download_each_bank_over_mock_midi(bank):
    # Relocate the real fixture to model a complete bank. Expected wire bytes
    # are assembled independently of the adaptation's conversion functions.
    message = knobkraft.load_sysex(str(SAMPLE_FILE), as_single_list=True)
    patches = [[0xF0, 0x01, 0x3B, 0x02, bank, slot] + message[6:] for slot in range(128)]
    requests = [[0xF0, 0x01, 0x3B, 0x05, bank, slot, 0xF7] for slot in range(128)]
    device = ScriptedMockDevice({tuple(request): [patch] for request, patch in zip(requests, patches)})
    midi = MockMidiController(device)
    downloaded = []
    Librarian().start_downloading_all_patches(midi, 9, adaptation, bank, downloaded.extend)
    midi.drain()
    assert midi.finished
    assert midi.sent_messages == requests
    assert downloaded == patches


def test_audition_uses_edit_buffer_and_export_can_be_reimported():
    message = knobkraft.load_sysex(str(SAMPLE_FILE), as_single_list=True)
    expected = [0xF0, 0x01, 0x3B, 0x03] + message[6:]
    midi = MockMidiController(ScriptedMockDevice({tuple(expected): []}))
    librarian = Librarian()
    librarian.send_patch_to_synth(midi, 9, adaptation, message)
    assert midi.sent_messages == [expected]  # No stored-program write or program change.
    assert librarian.load_sysex(adaptation, [expected]) == [expected]
    assert librarian.load_sysex(adaptation, [message, expected]) == [message]
