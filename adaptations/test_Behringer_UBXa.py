import importlib.util
from pathlib import Path


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
