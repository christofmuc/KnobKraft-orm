from pathlib import Path

import pytest

import Korg_R3 as r3
import knobkraft


FIXTURE = Path(__file__).parent / "testData/Korg_R3/synthetic.syx"


def test_published_dump_layout():
    messages = knobkraft.load_sysex(str(FIXTURE))
    assert [len(message) for message in messages] == [525, 525, 525, 525, 523]
    for message in messages:
        is_program = message[4] == 0x4c
        assert r3.isSingleProgramDump(message) == is_program
        assert r3.isEditBufferDump(message) == (not is_program)
        packed = message[7:-1] if is_program else message[5:-1]
        assert all(value < 128 for value in packed)
        assert packed[16:24] == [0x55, 0, 1, 2, 3, 4, 5, 6]
        assert packed[-5:] == [5, 0, 1, 0x7e, 0x7f]
        unpacked = r3._program_data_from_dump(message)
        assert len(unpacked) == 452
        assert unpacked[14:21] == [0x80, 1, 0x82, 3, 0x84, 5, 0x86]
        assert unpacked[-4:] == [0x80, 1, 0xfe, 0x7f]


@pytest.mark.parametrize("index", [0, 4])
def test_reject_incomplete_or_wrong_length_dump(index):
    message = knobkraft.load_sysex(str(FIXTURE))[index]
    recognize = r3.isSingleProgramDump if index == 0 else r3.isEditBufferDump
    assert not recognize(message[:-1])  # Missing EOX.
    assert not recognize(message[:-2] + [0xf7])  # Missing payload byte.
    assert not recognize(message[:-1] + [0, 0xf7])  # Extra payload byte.
    wrong_model = message.copy()
    wrong_model[3] = 0x7e
    assert not recognize(wrong_model)


def test_issue_547_prefix_is_not_a_complete_dump():
    # The report ends with an ellipsis; appending EOX does not restore its payload.
    prefix = list(bytes.fromhex("F0 42 30 7D 4C 08 00 00 4C 69 6E 65 72 7A 42 00"))
    assert not r3.isSingleProgramDump(prefix + [0xf7])
    identity = list(bytes.fromhex("F0 7E 00 06 02 42 7D 00 00 00 4F 4B 01 00 F7"))
    assert r3.channelIfValidDeviceResponse(identity) == 0
