import pytest

import KorgDW6000
import KorgDW8000
from knobkraft import splitSysex
from testing.librarian import Librarian, UploadStatus
from testing.mock_midi import EditBufferMockDevice, MockMidiController, ScriptedMockDevice


@pytest.fixture(params=[
    (KorgDW6000, 0x30, 0x04, 26),
    (KorgDW8000, 0x35, 0x03, 51),
])
def korg_bank(request):
    synth, format_id, model, payload_size = request.param
    header = [0xf0, 0x42, format_id, model]
    expected_messages = []
    messages = []
    for slot in range(64):
        payload = [slot] + [0] * (payload_size - 1)
        # Older DW-6000 exports used a channel nibble. DW-8000 dumps
        # from another synth or session may also use a different channel.
        patch = [0xf0, 0x42, 0x32, model, 0x40] + payload + [0xf7]
        messages.extend(splitSysex(synth.convertToProgramDump(5, patch, slot)))
        expected_messages.extend([
            header + [0x40] + payload + [0xf7],
            header + [0x11, slot, 0xf7],
        ])
    assert messages == expected_messages
    return synth, header, messages


def test_bank_advances_only_after_each_write_is_acknowledged(korg_bank):
    synth, header, messages = korg_bank
    controller = MockMidiController(ScriptedMockDevice({}, ignore_unmatched=True))
    results = []

    Librarian().send_block_of_messages_to_synth(controller, synth, messages, results.append)
    for slot in range(64):
        expected = messages[:2 * (slot + 1)]
        assert controller.sent_messages == expected
        assert not results
        controller.receive(messages[0])
        assert controller.sent_messages == expected
        controller.receive(header + [0x21, 0xf7])

    assert controller.sent_messages == messages
    assert len(results) == 1
    assert results[0].status == UploadStatus.ACKNOWLEDGED
    assert results[0].completed_messages == len(messages)


@pytest.mark.parametrize("status", [UploadStatus.DEVICE_ERROR, UploadStatus.TIMEOUT])
def test_bank_stops_when_a_write_fails(korg_bank, status):
    synth, header, messages = korg_bank
    controller = MockMidiController(ScriptedMockDevice({}, ignore_unmatched=True))
    librarian = Librarian()
    results = []

    librarian.send_block_of_messages_to_synth(controller, synth, messages, results.append)
    controller.receive(header + [0x21, 0xf7])
    if status == UploadStatus.DEVICE_ERROR:
        controller.receive(header + [0x22, 0xf7])
        assert "program 12" in results[0].message
    else:
        librarian.timeout_upload()

    assert controller.sent_messages == messages[:4]
    assert len(results) == 1
    assert results[0].status == status
    assert results[0].completed_messages == 3


def test_dw8000_retargets_a_saved_pair_and_still_accepts_legacy_patch_data():
    payload = [1] * 51
    patch = [0xf0, 0x42, 0x32, 0x03, 0x40] + payload + [0xf7]
    write = [0xf0, 0x42, 0x32, 0x03, 0x11, 0, 0xf7]
    audition = [0xf0, 0x42, 0x35, 0x03, 0x40] + payload + [0xf7]
    new_write = [0xf0, 0x42, 0x35, 0x03, 0x11, 63, 0xf7]

    assert KorgDW8000.convertToEditBuffer(5, patch + write) == audition
    assert KorgDW8000.convertToProgramDump(5, patch + write, 63) == audition + new_write
    assert KorgDW8000.convertToProgramDump(5, payload, 63) == audition + new_write


def test_dw6000_bank_download_preserves_slot_order():
    header = [0xf0, 0x42, 0x30, 0x04]
    patches = [header + [0x40, slot] + [0] * 25 + [0xf7] for slot in range(64)]
    controller = MockMidiController(EditBufferMockDevice(KorgDW6000, patches, channel=2))
    results = []

    Librarian().start_downloading_all_patches(controller, 2, KorgDW6000, 0, results.extend)
    controller.drain()

    expected = []
    for slot in range(64):
        expected.extend([[0xc2, slot], header + [0x10, 0xf7]])
    assert controller.sent_messages == expected
    assert results == patches
