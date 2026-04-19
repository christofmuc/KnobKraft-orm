import pytest

from testing.mock_midi import MockMidiController, PassiveBankDumpMockDevice, _split_outbound_messages


def test_split_outbound_messages_handles_channel_message_lengths():
    messages = _split_outbound_messages([
        0x90, 0x40, 0x7f,
        0x80, 0x40, 0x00,
        0xa0, 0x40, 0x10,
        0xb0, 0x07, 0x64,
        0xc0, 0x05,
        0xd0, 0x20,
        0xe0, 0x00, 0x40,
        0xf8,
        0xff,
    ])

    assert messages == [
        [0x90, 0x40, 0x7f],
        [0x80, 0x40, 0x00],
        [0xa0, 0x40, 0x10],
        [0xb0, 0x07, 0x64],
        [0xc0, 0x05],
        [0xd0, 0x20],
        [0xe0, 0x00, 0x40],
        [0xf8],
        [0xff],
    ]


@pytest.mark.parametrize("message", [
    [0x90, 0x40],
    [0xb0],
    [0xc0],
    [0xd0],
    [0xe0, 0x00],
])
def test_split_outbound_messages_rejects_truncated_channel_messages(message):
    with pytest.raises(ValueError):
        _split_outbound_messages(message)


def test_mock_controller_drains_passive_initial_messages():
    device = PassiveBankDumpMockDevice([[0xF0, 0x01, 0xF7]])
    controller = MockMidiController(device)
    received = []

    controller.add_message_handler(received.append)
    controller.drain()

    assert received == [[0xF0, 0x01, 0xF7]]


def test_passive_bank_device_accepts_configured_replies():
    ack = [0xF0, 0x41, 0x43, 0x00, 0x23, 0xF7]
    device = PassiveBankDumpMockDevice([], accepted_replies=[ack])
    controller = MockMidiController(device)

    controller.send(ack)

    assert controller.sent_messages == [ack]
    assert device.unmatched_requests == []
