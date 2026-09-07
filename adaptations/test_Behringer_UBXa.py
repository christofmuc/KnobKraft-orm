import importlib.util
from pathlib import Path

from testing.librarian import Librarian
from testing.mock_midi import MockMidiController, ScriptedMockDevice


def _load_adaptation():
    path = Path(__file__).parent / "Behringer_UB-Xa.py"
    spec = importlib.util.spec_from_file_location("Behringer_UBXa", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ubxa = _load_adaptation()


def _header(device_id, filename="PatchX A001     "):
    file_size = [0x00, 0x00, 0x00, 0x10]
    return (ubxa.sysex_prefix + [device_id, 0x74, 0x07, 0x01, 0x00]
            + ubxa._BIN_PREFIX + file_size + [ord(c) for c in filename] + [0xF7])


def _data(device_id, packet=1):
    return ubxa.sysex_prefix + [device_id, 0x74, 0x07, 0x02, packet, 0x00, 0x01, 0x02, 0x00, 0xF7]


def _eof(device_id):
    return [0xF0, 0x7E, device_id, 0x7B, 0x00, 0xF7]


def _flatten(messages):
    return [value for message in messages for value in message]


def test_eof_must_belong_to_same_device_as_header_and_data():
    mixed_devices = _flatten([_header(1), _data(1), _eof(2)])
    complete = _flatten([_header(1), _data(1), _eof(1)])

    assert not ubxa.isSingleProgramDump(mixed_devices)
    assert ubxa.isSingleProgramDump(complete)


def test_eof_must_follow_broadcast_header_and_data():
    stale_eof_then_partial_transfer = _flatten([_eof(0), _header(0x7F), _data(0x7F)])
    complete_transfer_after_stale_eof = _flatten([
        _eof(0), _header(0x7F), _data(0x7F), _eof(0)
    ])

    assert not ubxa.isSingleProgramDump(stale_eof_then_partial_transfer)
    assert ubxa.isSingleProgramDump(complete_transfer_after_stale_eof)


def _issue_574_edit_buffer():
    path = Path(__file__).parent / "testData" / "Behringer_UBXa_issue574-midi-log.txt"
    messages = []
    for line in path.read_text().splitlines():
        if ": In  UB-Xa " not in line or "Sysex [" not in line:
            continue
        timestamp = line[:12]
        if timestamp < "00:35:29.603":
            continue
        payload = line.split("Sysex [", 1)[1].split("]", 1)[0]
        messages.append([int(byte, 16) for byte in payload.split()])
    return messages


def test_broadcast_fds_transfer_accepts_hardware_eof_device_id():
    # Issue #574 hardware trace: FDS header/data use transfer ID 0x7f while
    # the universal SysEx EOF is addressed to device 0x00.
    messages = _issue_574_edit_buffer()
    dump = _flatten(messages)

    assert len(messages) == 9
    assert ubxa.isSingleProgramDump(dump)
    assert ubxa.isEditBufferDump(dump)
    assert len(ubxa._fds_extract_raw(messages)) == 742
    assert ubxa.nameFromDump(dump) == "ARP 7         BB"


def test_issue_574_edit_buffer_download_completes_via_mock_midi():
    messages = _issue_574_edit_buffer()
    device = ScriptedMockDevice(
        {tuple(ubxa.createEditBufferRequest(0)): messages},
        ignore_unmatched=True,
    )
    midi = MockMidiController(device)
    downloaded = []

    Librarian().download_edit_buffer(midi, 0, ubxa, downloaded.extend)
    midi.drain()

    assert midi.finished
    assert downloaded == [_flatten(messages)]
    assert midi.sent_messages[1:] == [
        [0xF0, 0x7E, 0x7F, 0x7E, 0x00, 0xF7],
        [0xF0, 0x7E, 0x7F, 0x7E, 0x00, 0xF7],
        [0xF0, 0x7E, 0x7F, 0x7E, 0x01, 0xF7],
        [0xF0, 0x7E, 0x7F, 0x7E, 0x02, 0xF7],
        [0xF0, 0x7E, 0x7F, 0x7E, 0x03, 0xF7],
        [0xF0, 0x7E, 0x7F, 0x7E, 0x04, 0xF7],
        [0xF0, 0x7E, 0x7F, 0x7E, 0x05, 0xF7],
        [0xF0, 0x7E, 0x7F, 0x7E, 0x06, 0xF7],
        [0xF0, 0x7E, 0x00, 0x7E, 0x00, 0xF7],
    ]


def test_issue_574_bank_b_download_uses_program_requests_and_completes():
    messages = _issue_574_edit_buffer()
    responses = {
        tuple(ubxa.createProgramDumpRequest(0, program)): messages
        for program in range(128, 256)
    }
    midi = MockMidiController(ScriptedMockDevice(responses, ignore_unmatched=True))
    downloaded = []

    Librarian().start_downloading_all_patches(midi, 0, ubxa, 1, downloaded.extend)
    midi.drain(max_steps=2000)

    assert midi.finished
    assert len(downloaded) == 128
    program_requests = [message for message in midi.sent_messages if message[:7] == ubxa.sysex_prefix]
    assert len(program_requests) == 128
    assert b"PatchX B001     " in bytes(program_requests[0])
    assert b"PatchX B128     " in bytes(program_requests[-1])


def test_fds_ack_uses_device_and_packet_number():
    assert ubxa.isPartOfSingleProgramDump(_header(3))[1] == [0xF0, 0x7E, 3, 0x7E, 0x00, 0xF7]
    assert ubxa.isPartOfSingleProgramDump(_data(3, packet=7))[1] == [0xF0, 0x7E, 3, 0x7E, 0x07, 0xF7]
    assert ubxa.isPartOfSingleProgramDump(_eof(3))[1] == [0xF0, 0x7E, 3, 0x7E, 0x00, 0xF7]


def test_conversions_retarget_fds_header_without_touching_data_packets():
    data = _data(1)
    source = _flatten([_header(1), data, _eof(1)])

    edit_buffer = ubxa.convertToEditBuffer(0, source)
    program = ubxa.convertToProgramDump(0, edit_buffer, 128)
    edit_messages = ubxa._split_sysex(edit_buffer)
    program_messages = ubxa._split_sysex(program)

    assert bytes("Upper Patch     ", "ascii") in bytes(edit_messages[0])
    assert bytes("PatchX B001     ", "ascii") in bytes(program_messages[0])
    assert edit_messages[1:] == [data, _eof(1)]
    assert program_messages[1:] == [data, _eof(1)]


def test_all_four_banks_have_128_slots_and_boundary_names_are_stable():
    assert [bank["size"] for bank in ubxa.bankDescriptors()] == [128] * 4
    assert b"PatchX A001     " in bytes(ubxa.createProgramDumpRequest(0, 0))
    assert b"PatchX A128     " in bytes(ubxa.createProgramDumpRequest(0, 127))
    assert b"PatchX B001     " in bytes(ubxa.createProgramDumpRequest(0, 128))
    assert b"PatchX D128     " in bytes(ubxa.createProgramDumpRequest(0, 511))


def test_rename_is_not_advertised_until_payload_repacking_is_implemented():
    assert not hasattr(ubxa, "renamePatch")
