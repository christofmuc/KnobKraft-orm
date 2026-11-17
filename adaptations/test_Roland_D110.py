import pytest

import knobkraft
import Roland_D110
from Roland_D110 import d110
from roland.GenericRoland import command_rq1, command_dt1

TONE_SIZE = 246


@pytest.fixture(autouse=True)
def default_unit():
    # The adaptation remembers the Unit # it detected; leave it as the other tests expect it
    yield
    d110.device_id = 0x10


def tone_of(message):
    return message[8:8 + TONE_SIZE]


def test_requests_match_the_manual():
    # Owner's manual p. 118, "Example of RQ1 and DT1 application --- 1": with Unit # 17, the tone of part 2 is
    # obtained from the temporary area with this message. It checks the framing, the size and the checksum.
    part_2 = knobkraft.stringToSyx("F0 41 10 16 11 04 01 76 00 01 76 0E F7")
    assert d110.buildRolandMessage(0x10, command_rq1, [0x04, 0x01, 0x76], [0x00, 0x01, 0x76]) == part_2
    # Part 1 is one tone before, at the start of the area; Tone Memory #1 and #64 are at 08 00 00 and 08 7E 00 (p. 117)
    assert Roland_D110.createEditBufferRequest(0) == knobkraft.stringToSyx("F0 41 10 16 11 04 00 00 00 01 76 05 F7")
    assert Roland_D110.createProgramDumpRequest(0, 0) == knobkraft.stringToSyx("F0 41 10 16 11 08 00 00 00 01 76 01 F7")
    assert Roland_D110.createProgramDumpRequest(0, 63) == knobkraft.stringToSyx("F0 41 10 16 11 08 7E 00 00 01 76 03 F7")


def test_detected_unit_is_used_for_requests_and_uploads():
    # Unit # 22 is Device ID 15H (p. 116); it is looked for on "channel" 5 and answers from the System Area
    assert Roland_D110.createDeviceDetectMessage(5) == knobkraft.stringToSyx("F0 41 15 16 11 10 00 00 00 00 01 6F F7")
    assert Roland_D110.channelIfValidDeviceResponse(knobkraft.stringToSyx("F0 41 15 16 12 10 00 00 4A 26 F7")) == 5

    assert Roland_D110.createEditBufferRequest(5) == knobkraft.stringToSyx("F0 41 15 16 11 04 00 00 00 01 76 05 F7")
    assert Roland_D110.createProgramDumpRequest(5, 4) == knobkraft.stringToSyx("F0 41 15 16 11 08 08 00 00 01 76 79 F7")

    # A tone stored from Unit # 17 goes to the unit detected, not to the one it came from
    stored = Roland_D110.edisyn_init_tone()
    assert stored[2] == 0x10
    edit_buffer = Roland_D110.convertToEditBuffer(5, stored)
    assert edit_buffer[:8] == [0xF0, 0x41, 0x15, 0x16, 0x12, 0x04, 0x00, 0x00]
    program = Roland_D110.convertToProgramDump(5, stored, 63)
    assert program[:8] == [0xF0, 0x41, 0x15, 0x16, 0x12, 0x08, 0x7E, 0x00]


def test_tones_of_any_unit_are_recognized():
    stored = Roland_D110.edisyn_init_tone()
    for device_id in (0x00, 0x11, 0x1F):
        other_unit = stored.copy()
        other_unit[2] = device_id
        assert Roland_D110.isSingleProgramDump(other_unit)
        assert Roland_D110.nameFromDump(other_unit) == "D110 Init "


def test_memory_tone_is_sent_as_the_246_bytes_of_the_sound():
    stored = Roland_D110.edisyn_init_tone()
    assert len(stored) == 8 + 256 + 2
    for sent in (Roland_D110.convertToEditBuffer(0, stored), Roland_D110.convertToProgramDump(0, stored, 4)):
        command, _, data = d110.parseRolandMessage(sent)  # raises on a wrong checksum
        assert command == command_dt1
        assert len(data) == TONE_SIZE
        assert data[10:] == tone_of(stored)[10:]


def test_name_is_sent_padded_with_spaces():
    # Edisyn pads the name with a zero; TONE NAME is ASCII 32 to 127 (p. 117)
    stored = Roland_D110.edisyn_init_tone()
    assert stored[8:18] == [ord(c) for c in "D110 Init"] + [0x00]
    for sent in (Roland_D110.convertToEditBuffer(0, stored), Roland_D110.convertToProgramDump(0, stored, 4)):
        assert sent[8:18] == [ord(c) for c in "D110 Init "]
    # The stored tone is not modified
    assert stored == Roland_D110.edisyn_init_tone()


def test_fingerprint_covers_the_sound_only():
    stored = Roland_D110.edisyn_init_tone()
    fingerprint = Roland_D110.calculateFingerprint(stored)

    # Name
    assert Roland_D110.calculateFingerprint(Roland_D110.renamePatch(stored, "Warm Pad")) == fingerprint
    # Unit #
    other_unit = stored.copy()
    other_unit[2] = 0x1F
    assert Roland_D110.calculateFingerprint(other_unit) == fingerprint
    # Tone Memory number, Tone Temporary Area, and the length of the block that carries the tone
    d110.device_id = 0x15
    assert Roland_D110.calculateFingerprint(Roland_D110.convertToProgramDump(5, stored, 37)) == fingerprint
    assert Roland_D110.calculateFingerprint(Roland_D110.convertToEditBuffer(5, stored)) == fingerprint
    # The 10 bytes a Tone Memory carries after the sound in a bulk dump
    spare = stored.copy()
    spare[8 + TONE_SIZE] = 0x55
    spare[-2] = d110.roland_checksum(spare[5:-2])
    assert Roland_D110.isSingleProgramDump(spare)
    assert Roland_D110.calculateFingerprint(spare) == fingerprint

    # Any byte of the sound changes it: the first after the name, and the last
    for position in (8 + 10, 8 + TONE_SIZE - 1):
        other_sound = stored.copy()
        other_sound[position] ^= 0x01
        other_sound[-2] = d110.roland_checksum(other_sound[5:-2])
        assert Roland_D110.isSingleProgramDump(other_sound)
        assert Roland_D110.calculateFingerprint(other_sound) != fingerprint


def test_messages_are_spaced_as_the_manual_requires():
    # p. 110, one-way transfer procedure: at least 20 milliseconds between messages
    assert Roland_D110.generalMessageDelay() >= 20
