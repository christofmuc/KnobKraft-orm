from pathlib import Path

import pytest

import knobkraft
import Clavia_Nord_Lead as nord
from testing.librarian import Librarian
from testing.mock_midi import MockMidiController, ScriptedMockDevice


DATA = Path(__file__).resolve().parent / "testData" / "Clavia_Nord_Lead"


@pytest.fixture
def patch():
    return knobkraft.load_sysex(str(DATA / "bank0.syx"))[0]


@pytest.mark.parametrize("filename", ["bank0.syx", "bank1.syx"])
def test_official_banks_contain_99_programs_and_ten_unsupported_kits(filename):
    messages = knobkraft.load_sysex(str(DATA / filename))
    programs = [m for m in messages if nord.isSingleProgramDump(m)]
    assert len(messages) == 109
    assert len(programs) == 99
    assert {m[5] for m in programs} == set(range(99))
    kits = [m for m in messages if len(m) == 1063]
    assert len(kits) == 10
    assert all(not nord.isSingleProgramDump(m) and not nord.isEditBufferDump(m) for m in kits)


@pytest.mark.parametrize("filename,count", [("nord-lead-2-internal.mid", 40), ("nord-lead-bank1.mid", 99)])
def test_official_midi_files(filename, count):
    patches = nord.loadPatchesFromLegacyData(list((DATA / filename).read_bytes()))
    assert len(patches) == count
    assert all(nord.isSingleProgramDump(m) for m in patches)
    assert len({nord.numberFromDump(m) for m in patches}) == count


@pytest.mark.parametrize("bank", range(4))
@pytest.mark.parametrize("slot", [0, 98])
@pytest.mark.parametrize("channel", [0, 9, 15])
def test_documented_request_bytes(bank, slot, channel):
    assert nord.createProgramDumpRequest(channel, bank * 99 + slot) == [
        0xF0, 0x33, channel, 0x04, 0x0B + bank, slot, 0xF7
    ]


@pytest.mark.parametrize("bank", range(10))
def test_ten_bank_file_addresses_round_trip_without_changing_sound(patch, bank):
    original = patch.copy()
    program = bank * 99 + 98
    converted = nord.convertToProgramDump(9, patch, program)
    assert converted == [0xF0, 0x33, 9, 4, bank + 1, 98] + patch[6:]
    assert nord.numberFromDump(converted) == program
    assert nord.friendlyProgramName(program) == f"{bank}.99"
    assert nord.friendlyBankName(bank) == f"Bank {bank}"
    edit = nord.convertToEditBuffer(15, converted)
    assert edit == [0xF0, 0x33, 15, 4, 0, 0] + patch[6:]
    assert nord.convertToProgramDump(9, edit, program) == converted
    assert nord.calculateFingerprint(edit) == nord.calculateFingerprint(patch)
    assert patch == original
    if bank >= 4:
        with pytest.raises(ValueError):
            nord.createProgramDumpRequest(9, program)


@pytest.mark.parametrize("program", [-1, 396, 989, 990])
def test_live_request_rejects_undocumented_banks(program):
    with pytest.raises(ValueError):
        nord.createProgramDumpRequest(0, program)


def test_invalid_export_addresses_and_bank_names(patch):
    for number in [-1, 990]:
        with pytest.raises(Exception):
            nord.convertToProgramDump(0, patch, number)
    for bank in [-1, 10]:
        with pytest.raises(ValueError):
            nord.friendlyBankName(bank)


@pytest.mark.parametrize("channel", [0, 9, 15])
def test_detection_requires_requested_slot_a(patch, channel):
    assert nord.createDeviceDetectMessage(channel) == [0xF0, 0x33, channel, 4, 0x0A, 0, 0xF7]
    assert nord.needsChannelSpecificDetection()
    for slot in range(4):
        response = [0xF0, 0x33, channel, 4, 0, slot] + patch[6:]
        assert nord.isEditBufferDump(response)
        assert nord.channelIfValidDeviceResponse(response) == (channel if slot == 0 else -1)
    assert nord.channelIfValidDeviceResponse(patch) == -1


@pytest.mark.parametrize("index,value", [
    (0, 0), (1, 0x41), (2, 16), (2, -1), (3, 5), (4, 11),
    (5, 99), (6, 16), (7, 128), (137, -1), (138, 0),
])
def test_invalid_programs_are_not_recognized(patch, index, value):
    patch[index] = value
    assert not nord.isSingleProgramDump(patch)
    assert not nord.isEditBufferDump(patch)
    assert nord.channelIfValidDeviceResponse(patch) == -1


def test_truncated_and_oversized_dumps_are_rejected(patch):
    for length in range(len(patch)):
        assert not nord.isSingleProgramDump(patch[:length])
        assert not nord.isEditBufferDump(patch[:length])
    assert not nord.isSingleProgramDump(patch + [0])
    invalid_edit = [0xF0, 0x33, 0, 4, 0, 4] + patch[6:]
    assert not nord.isEditBufferDump(invalid_edit)


def test_fingerprint_detects_sound_changes(patch):
    changed = patch.copy()
    changed[6] ^= 1
    assert nord.calculateFingerprint(changed) != nord.calculateFingerprint(patch)


@pytest.mark.parametrize("banks", [[0], [1], [2], [3], [3, 0, 2, 0]])
def test_independent_mock_downloads_all_banks_and_repeated_bank(patch, banks):
    # Relocated fixture, not recorded responses: construct expected wire bytes
    # independently so a matching bug in a request helper cannot hide itself.
    requests = [[0xF0, 0x33, 9, 4, 0x0B + bank, slot, 0xF7] for bank in banks for slot in range(99)]
    replies = [[0xF0, 0x33, 9, 4, 1 + bank, slot] + patch[6:] for bank in banks for slot in range(99)]
    midi = MockMidiController(ScriptedMockDevice({tuple(q): [r] for q, r in zip(requests, replies)}))
    downloaded = []
    Librarian().start_downloading_all_patches(midi, 9, nord, banks, downloaded.extend)
    midi.drain()
    assert midi.finished
    assert midi.sent_messages == requests
    assert downloaded == replies


def test_audition_only_replaces_slot_a_and_exports_reimport(patch):
    expected = [0xF0, 0x33, 9, 4, 0, 0] + patch[6:]
    midi = MockMidiController(ScriptedMockDevice({tuple(expected): []}))
    librarian = Librarian()
    librarian.send_patch_to_synth(midi, 9, nord, patch)
    assert midi.sent_messages == [expected]  # No bank select, PC, or stored-program write.
    assert librarian.load_sysex(nord, [expected]) == [expected]
    assert librarian.load_sysex(nord, [patch, expected]) == [patch]


def vlq(value):
    result = [value & 127]
    while value > 127:
        value >>= 7
        result.insert(0, (value & 127) | 128)
    return bytes(result)


EOT = b"\x00\xff\x2f\x00"


def smf(*tracks, format=0):
    return (b"MThd\x00\x00\x00\x06" + format.to_bytes(2, "big")
            + len(tracks).to_bytes(2, "big") + b"\x00\x60"
            + b"".join(b"MTrk" + len(t).to_bytes(4, "big") + t for t in tracks))


def event(status, payload):
    return bytes([0, status]) + vlq(len(payload)) + bytes(payload)


def test_midi_complete_split_and_escaped_sysex(patch):
    for track in [
        event(0xF0, patch[1:]),
        event(0xF0, patch[1:40]) + event(0xF7, patch[40:]),
        event(0xF7, [0xF8] + patch),
    ]:
        assert nord.loadPatchesFromLegacyData(list(smf(track + EOT))) == [patch]
    assert nord.loadPatchesFromLegacyData(list(smf(event(0xF7, patch + patch) + EOT))) == [patch, patch]
    # A lone escape is not a continuation and must not manufacture an F0.
    assert nord.loadPatchesFromLegacyData(list(smf(event(0xF7, patch[1:]) + EOT))) == []


@pytest.mark.parametrize("format", [1, 2])
def test_midi_multiple_tracks_and_channel_running_status(patch, format):
    channel_events = b"\x00\x90\x3c\x40\x00\x3d\x41\x00\xc0\x01\x00\x02\x00\xd0\x30"
    meta = b"\x00\xff\x01\x03abc"
    data = smf(channel_events + meta + EOT, event(0xF0, patch[1:]) + EOT, format=format)
    assert nord.loadPatchesFromLegacyData(list(data)) == [patch]


@pytest.mark.parametrize("track", [
    b"", b"\x00", b"\x80", b"\x81\x80\x80\x80\x00" + EOT,
    b"\x00\x40\x00" + EOT, b"\x00\x90\x40", b"\x00\xc0\x80" + EOT,
    b"\x00\xff", b"\x00\xff\x80\x00" + EOT, b"\x00\xff\x01\x7f" + EOT,
    b"\x00\xff\x2f\x01\x00", EOT + b"\x00", b"\x00\xf0\x7f" + EOT,
    b"\x00\xf1\x00" + EOT, event(0xF0, [0x33]) + EOT,
    event(0xF0, [0x33]) + event(0xF0, [0x33, 0xF7]) + EOT,
    event(0xF0, [0x33, 0x80]) + event(0xF7, [0xF7]) + EOT,
    event(0xF0, [0x33, 0xF7, 0x01]) + EOT,
    event(0xF7, [0xF0, 0x33]) + EOT,
    event(0xF7, [0xF0, 0xF0, 0xF7]) + EOT,
    b"\x00\x90\x3c\x40\x00\xff\x01\x00\x00\x3d\x40" + EOT,
    b"\x00\x90\x3c\x40" + event(0xF0, [0x33, 0xF7]) + b"\x00\x3d\x40" + EOT,
])
def test_malformed_tracks_raise_value_error(track):
    with pytest.raises(ValueError):
        nord.loadPatchesFromLegacyData(list(smf(track)))


def test_bad_headers_and_truncations_never_return_partial_patches(patch):
    good = smf(event(0xF0, patch[1:]) + EOT)
    for length in range(4, len(good)):
        with pytest.raises(ValueError):
            nord.loadPatchesFromLegacyData(list(good[:length]))
    for malformed in [
        good + b"extra", good[:4] + b"\x00\x00\x00\x05" + good[8:],
        smf(EOT, format=3), smf(), smf(EOT, EOT),
        smf(event(0xF0, patch[1:]) + b"\x00\xf0\x7f" + EOT),
        smf(EOT, b"\x00\x3c\x40" + EOT, format=1),
    ]:
        with pytest.raises(ValueError):
            nord.loadPatchesFromLegacyData(list(malformed))
    assert nord.loadPatchesFromLegacyData([1, 2, 3]) == []
