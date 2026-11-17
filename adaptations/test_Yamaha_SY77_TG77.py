import importlib
from pathlib import Path

import knobkraft


yamaha = importlib.import_module("Yamaha_SY77-TG77")
FIXTURE = Path(__file__).parent / "testData" / "Yamaha_SY77-TG77" / "TG77_test_voices.syx"


def test_detection_requests_only_system_data():
    assert yamaha.needsChannelSpecificDetection()
    for device_number in range(16):
        assert yamaha.createDeviceDetectMessage(device_number) == [
            0xF0, 0x43, 0x20 | device_number, 0x7A,
            *b"LM  8101SY", *([0] * 16), 0xF7,
        ]


def test_system_dump_sets_voice_receive_channel(monkeypatch):
    monkeypatch.setattr(yamaha, "_voice_receive_channels", {})
    system_data = [*b"LM  8101SY", *([0] * 80)]
    system_data[80 - 6] = 8
    reply = [0xF0, 0x43, 0x0F, 0x7A, 0, 90, *system_data, (-sum(system_data)) & 0x7F, 0xF7]

    assert yamaha.channelIfValidDeviceResponse(reply) == 15
    assert yamaha.createCustomProgramChange(15, 21) == [0xC8, 21]
    assert yamaha.createCustomProgramChange(3, 21) == [0xC3, 21]

    reply[80] = 4
    assert yamaha.channelIfValidDeviceResponse(reply) == -1
    assert yamaha.createCustomProgramChange(15, 21) == [0xC8, 21]


def test_large_voices_require_an_explicit_bank_d_slot():
    voices = knobkraft.load_sysex(str(FIXTURE))
    for index in (66, 71, 73, 74):
        voice = voices[index]
        assert yamaha.convertToProgramDump(5, voice, 47) == []
        converted = yamaha.convertToProgramDump(5, voice, 48)
        assert yamaha.numberFromDump(converted) == 48
        assert converted[2] == 5
        assert yamaha.calculateFingerprint(voice) == yamaha.calculateFingerprint(converted)
