import hashlib
import importlib.util
from pathlib import Path

import knobkraft
import pytest


def _load_adaptation():
    path = Path(__file__).parent / "Casio CZ-101.py"
    spec = importlib.util.spec_from_file_location("Casio_CZ101", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


cz = _load_adaptation()


def _fixture():
    path = Path(__file__).parent / "testData" / "Casio_CZ101" / "bassbling.syx"
    return path, knobkraft.load_sysex(str(path))[0]


def _device_reply(upload_message, channel=0):
    return [0xF0, 0x44, 0x00, 0x00, 0x70 | channel, 0x30] + upload_message[7:-1] + [0xF7]


def test_public_domain_fixture_is_unchanged_and_recognized():
    path, message = _fixture()

    assert len(message) == 264
    assert hashlib.sha256(path.read_bytes()).hexdigest() == "cdec2a098c5ccd59877dce0d4021529e87a10ee94eb86fe516ac066067946970"
    assert message[:7] == [0xF0, 0x44, 0x00, 0x00, 0x70, 0x20, 0x20]
    assert message[-1] == 0xF7
    assert cz.isSingleProgramDump(message)


def test_recognizes_both_264_byte_upload_and_263_byte_device_reply():
    _, upload = _fixture()
    reply = _device_reply(upload, channel=6)

    assert len(reply) == 263
    assert cz.isSingleProgramDump(upload)
    assert cz.isSingleProgramDump(reply)
    assert cz.channelIfValidDeviceResponse(reply) == 6
    assert cz.calculateFingerprint(upload) == cz.calculateFingerprint(reply)


@pytest.mark.parametrize(
    "mutator",
    [
        lambda message: message + [0x00],
        lambda message: [*message[:5], 0x31, *message[6:]],
        lambda message: [*message[:20], 0x10, *message[21:]],
        lambda message: [*message[:-1], 0x00],
    ],
)
def test_rejects_malformed_program_dumps(mutator):
    _, message = _fixture()

    assert not cz.isSingleProgramDump(mutator(message))


def test_conversions_rebuild_headers_and_preserve_real_payload():
    _, upload = _fixture()
    reply = _device_reply(upload, channel=6)

    edit_buffer = cz.convertToEditBuffer(9, reply)
    program = cz.convertToProgramDump(3, edit_buffer, 17)

    assert len(edit_buffer) == 264
    assert edit_buffer[:7] == [0xF0, 0x44, 0x00, 0x00, 0x79, 0x20, 0x60]
    assert cz.isEditBufferDump(edit_buffer)
    assert program[:7] == [0xF0, 0x44, 0x00, 0x00, 0x73, 0x20, 0x41]
    assert cz.isSingleProgramDump(program)
    assert not cz.isEditBufferDump(program)
    assert upload[7:-1] == reply[6:-1] == edit_buffer[7:-1] == program[7:-1]
    assert cz.calculateFingerprint(upload) == cz.calculateFingerprint(edit_buffer)
    assert cz.calculateFingerprint(upload) == cz.calculateFingerprint(program)


def test_edit_buffer_request_uses_working_memory_address():
    assert cz.createEditBufferRequest(5) == [0xF0, 0x44, 0x00, 0x00, 0x75, 0x10, 0x60, 0x75, 0x31, 0xF7]


def test_program_number_mapping_rejects_out_of_range_values():
    assert cz.createProgramDumpRequest(0, 0)[6] == 0x20
    assert cz.createProgramDumpRequest(0, 15)[6] == 0x2F
    assert cz.createProgramDumpRequest(0, 16)[6] == 0x40
    assert cz.createProgramDumpRequest(0, 31)[6] == 0x4F
    with pytest.raises(ValueError):
        cz.createProgramDumpRequest(0, 32)
