from pathlib import Path

import pytest

import Roland_SE02 as se02


FIXTURES = Path(__file__).parent / "testData" / "Roland_SE02"


@pytest.mark.parametrize("stem,filename", [
    ("PATCH_65", "PATCH_65.PRM"),
    ("mapping_coverage", "PATCH_65.PRM"),
])
def test_prm_matches_upstream_except_address_and_checksum(stem, filename):
    data = list((FIXTURES / (stem + ".PRM")).read_bytes())
    patches = se02.loadPatchesFromLegacyData(data, filename)
    assert len(patches) == 1
    assert se02.isEditBufferDump(patches[0])
    assert se02.numberFromDump(patches[0]) == 64

    expected = bytearray((FIXTURES / (stem + "_upstream.syx")).read_bytes())
    # Independent of the adaptation helpers: convert upstream 06 to 05, which
    # increases Roland's negated address/data checksum by one modulo 128.
    start = 0
    for length in (78, 78, 78, 62):
        assert expected[start + 8] == 0x06
        expected[start + 8] = 0x05
        expected[start + length - 2] = (expected[start + length - 2] + 1) % 128
        start += length
    assert bytes(patches[0]) == expected

    audition = se02.convertToEditBuffer(9, patches[0])
    assert se02.isEditBufferDump(audition)
    assert se02.calculateFingerprint(audition) == se02.calculateFingerprint(patches[0])
    for message in se02._split_sysex(bytes(audition)):
        assert message[2] == 0x19
        assert message[8:10] == b"\x05\x00"
        assert sum(message[8:-1]) % 128 == 0
        assert all(0 <= byte < 16 for byte in message[12:-2])


@pytest.mark.parametrize("filename,slot", [
    ("SE02_PATCH001.PRM", 0),
    ("SE02_PATCH128.PRM", 127),
    ("PATCH60.PRM", 59),
    ("PATCH 60.PRM", 59),
    ("060.prm", 59),
    (r"C:\backup\SE02_PATCH060.PRM", 59),
    ("/backup/PATCH_60.PRM", 59),
    ("MySound.prm", 0),
    ("PATCH0.PRM", 0),
    ("PATCH129.PRM", 0),
    ("", 0),
])
def test_prm_filename_context(filename, slot):
    patch = se02.loadPatchesFromLegacyData(list(b"COM_VOLUME(127);"), filename)[0]
    assert se02.numberFromDump(patch) == slot


@pytest.mark.parametrize("data", [
    b"", b"unrelated text", b"UNKNOWN(4);", b"COM_OCT(1);",
    b"\xffCOM_VOLUME(127);", b"COM_VOLUME(nope);",
    b"COM_VOLUME(127);\nFLT_CUTOFF(abc);",
    b"COM_VOLUME(127);\nFLT_CUTOFF(40)",
])
def test_prm_rejects_unrecognized_or_malformed_data(data):
    assert se02.loadPatchesFromLegacyData(list(data), "PATCH_65.PRM") == []


def test_prm_ignores_unmapped_controls_and_supports_text_conventions():
    basic = se02.loadPatchesFromLegacyData(list(b"COM_VOLUME(127);"))
    text = (b"\xef\xbb\xbf  COM_VOLUME(127);\r\nCOM_OCT(1);\r\nCOM_TRNS(-12);\r\n"
            b"COM_PWM_DEPTH(20);\r\nCOM_PWM_RATE(20);\r\nUNKNOWN(12);\r\n")
    assert se02.loadPatchesFromLegacyData(list(text)) == basic
    assert se02.loadPatchesFromLegacyData(list(text), "not_a_patch.txt") == []


def test_prm_calls_do_not_mutate_template():
    data = list((FIXTURES / "PATCH_65.PRM").read_bytes())
    original = data[:]
    first = se02.loadPatchesFromLegacyData(data)
    se02.loadPatchesFromLegacyData(list(b"COM_VOLUME(1);"))
    assert se02.loadPatchesFromLegacyData(data) == first
    assert data == original
