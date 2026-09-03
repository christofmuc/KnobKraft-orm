import knobkraft
from pathlib import Path


def test_split_sysex_splits_complete_concatenated_messages():
    messages = knobkraft.splitSysex([
        0xF0, 0x01, 0xF7,
        0xF0, 0x02, 0x03, 0xF7,
    ])

    assert messages == [
        [0xF0, 0x01, 0xF7],
        [0xF0, 0x02, 0x03, 0xF7],
    ]


def test_split_sysex_preserves_non_sysex_bytes_as_singletons():
    messages = knobkraft.splitSysex([
        0x7E,
        0xF0, 0x01, 0xF7,
        0x00,
    ])

    assert messages == [
        [0x7E],
        [0xF0, 0x01, 0xF7],
        [0x00],
    ]


def test_split_sysex_ignores_unterminated_trailing_sysex():
    messages = knobkraft.splitSysex([
        0xF0, 0x01, 0xF7,
        0xF0, 0x41, 0x37, 0x00,
    ])

    assert messages == [[0xF0, 0x01, 0xF7]]


def test_split_sysex_handles_ezbank1_truncated_tail_fixture():
    fixture = Path(__file__).parent / "testData" / "Roland_MKS50" / "EZBANK1.SYX"
    messages = knobkraft.splitSysex(list(fixture.read_bytes()))

    assert len(messages) == 16
    assert all(message[0] == 0xF0 and message[-1] == 0xF7 for message in messages)
