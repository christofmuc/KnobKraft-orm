from pathlib import Path

import knobkraft
import pytest

import Yamaha_SY77 as sy77
import Yamaha_TG77 as tg77
from yamaha.Yamaha77Voice import Yamaha77Voice


FIXTURES = Path(__file__).parent / "testData"
SY77_INTERNAL = "Yamaha_SY77/bank.syx"
TG77_PRESETS = "Yamaha_TG77/Preset1.syx"
TG77_PRESET_ONE = "Yamaha_TG77/Preset1_01_SP_Cosmo__.syx"
TG77_INTERNAL = "Yamaha_SY77-TG77/TG77_test_voices.syx"


def _system_reply(device, voice_channel):
    payload = [*b"LM  8101SY", *([0] * 80)]
    payload[80 - 6] = voice_channel
    return [0xF0, 0x43, device, 0x7A, 0, 90, *payload, (-sum(payload)) & 0x7F, 0xF7]


def _with_memory(voice, device, memory, slot):
    result = voice.copy()
    result[2] = device
    result[30:32] = [memory, slot]
    result[-2] = (-sum(result[6:-2])) & 0x7F
    return result


@pytest.mark.parametrize("module", [sy77, tg77])
@pytest.mark.parametrize("file,count", [
    (SY77_INTERNAL, 64), (TG77_PRESETS, 64), (TG77_PRESET_ONE, 1), (TG77_INTERNAL, 75),
])
def test_both_models_import_all_captured_voices(module, file, count):
    voices = knobkraft.load_sysex(str(FIXTURES / file))
    assert len(voices) == count
    for voice in voices:
        expected_program = (64 if voice[30] == 2 else 0) + voice[31]
        assert module.isSingleProgramDump(voice)
        assert not module.isEditBufferDump(voice)
        assert module.numberFromDump(voice) == expected_program
        edit = module.convertToEditBuffer(11, voice)
        assert module.isEditBufferDump(edit)
        assert not module.isSingleProgramDump(edit)
        assert edit[2] == 11
        assert module.calculateFingerprint(edit) == module.calculateFingerprint(voice)


@pytest.mark.parametrize("system_first", [True, False])
@pytest.mark.parametrize("name,first_preset,other_preset,file,other_file", [
    ("SY77", "GrandPiano", "SP|Cosmo", SY77_INTERNAL, TG77_PRESET_ONE),
    ("TG77", "SP|Cosmo", "GrandPiano", TG77_PRESET_ONE, SY77_INTERNAL),
])
def test_detection_matches_only_its_factory_voice_and_learns_midi_channel(
        system_first, name, first_preset, other_preset, file, other_file):
    synth = Yamaha77Voice(name, first_preset)
    device = 15
    for address in range(16):
        assert knobkraft.splitSysex(synth.createDeviceDetectMessage(address)) == [
            [0xF0, 0x43, 0x20 | address, 0x7A, *b"LM  8101VC",
             *([0] * 14), 2, 0, 0xF7],
            [0xF0, 0x43, 0x20 | address, 0x7A, *b"LM  8101SY",
             *([0] * 16), 0xF7],
        ]

    system = _system_reply(device, 8)
    # The SY77 hardware reply uses the edit-buffer address for the requested preset.
    # The TG77 preset file stands in for a factory preset reply in this test.
    model_data = knobkraft.load_sysex(str(FIXTURES / file))[0]
    match = _with_memory(model_data, device, 0x7F if name == "SY77" else 2, 0)
    other_data = knobkraft.load_sysex(str(FIXTURES / other_file))[0]
    wrong_model = _with_memory(other_data, device, 2, 0)
    assert synth.channelIfValidDeviceResponse(wrong_model) == -1

    replies = [(system, -1), (match, device)]
    for message, expected in replies if system_first else reversed(replies):
        assert synth.channelIfValidDeviceResponse(message) == expected

    expected_pc = [0xC8, 122, 0xC8, 17] if name == "SY77" else [0xC8, 17]
    assert synth.createCustomProgramChange(device, 17) == expected_pc

    other = Yamaha77Voice("TG77" if name == "SY77" else "SY77", other_preset)
    other.createDeviceDetectMessage(device)
    assert other.channelIfValidDeviceResponse(system) == -1
    assert other.channelIfValidDeviceResponse(match) == -1


def test_system_reply_alone_cannot_replace_an_identified_devices_voice_channel():
    synth = Yamaha77Voice("TG77", "SP|Cosmo")
    device = 5
    preset = knobkraft.load_sysex(str(FIXTURES / TG77_PRESET_ONE))[0]
    match = _with_memory(preset, device, 2, 0)
    synth.createDeviceDetectMessage(device)
    assert synth.channelIfValidDeviceResponse(match) == device
    assert synth.channelIfValidDeviceResponse(_system_reply(device, 8)) == -1

    synth.createDeviceDetectMessage(device)
    assert synth.channelIfValidDeviceResponse(_system_reply(device, 3)) == -1
    assert synth.createCustomProgramChange(device, 17) == [0xC8, 17]
    assert synth.channelIfValidDeviceResponse(match) == device
    assert synth.createCustomProgramChange(device, 17) == [0xC3, 17]


@pytest.mark.parametrize("program,bank_selector", [(0, 122), (64, 124), (191, 117)])
def test_sy77_direct_mode_selects_voice_bank_then_slot(program, bank_selector):
    assert sy77.createCustomProgramChange(4, program) == [
        0xC4, bank_selector, 0xC4, program % 64,
    ]


def test_tg77_normal_mode_uses_the_selected_internal_bank():
    assert tg77.createCustomProgramChange(4, 63) == [0xC4, 63]
    assert tg77.createCustomProgramChange(4, 64) == []


@pytest.mark.parametrize("module", [sy77, tg77])
@pytest.mark.parametrize("program", [-1, 192])
def test_invalid_program_never_becomes_wire_data(module, program):
    assert module.createCustomProgramChange(3, program) == []
    assert module.createProgramDumpRequest(3, program) == []


@pytest.mark.parametrize("module", [sy77, tg77])
def test_preset_bank_reads_and_writes_are_distinct(module):
    assert [desc["isROM"] for desc in module.bankDescriptors()] == [False, True, True]
    for program, address in ((0, [0, 0]), (63, [0, 63]), (64, [2, 0]), (127, [2, 63]),
                             (128, [3, 0]), (191, [3, 63])):
        request = module.createProgramDumpRequest(5, program)
        assert request[2] == 0x25
        assert request[-3:-1] == address

    preset = knobkraft.load_sysex(str(FIXTURES / TG77_PRESET_ONE))[0]
    assert module.convertToProgramDump(7, preset, 64) == []
    assert module.numberFromDump(module.convertToProgramDump(7, preset, 15)) == 15
    assert not hasattr(module, "convertPatchesToBankDump")


@pytest.mark.parametrize("module", [sy77, tg77])
def test_large_voices_require_an_explicit_bank_d_slot(module):
    voices = knobkraft.load_sysex(str(FIXTURES / TG77_INTERNAL))
    for index in (66, 71, 73, 74):
        voice = voices[index]
        assert module.convertToProgramDump(5, voice, 47) == []
        converted = module.convertToProgramDump(5, voice, 48)
        assert module.numberFromDump(converted) == 48
        assert converted[2] == 5
        assert module.calculateFingerprint(voice) == module.calculateFingerprint(converted)


def test_model_specific_edit_buffer_requests():
    dump = [0xF0, 0x43, 0x26, 0x7A, *b"LM  8101VC", *([0] * 14), 0x7F, 0, 0xF7]
    assert knobkraft.splitSysex(sy77.createEditBufferRequest(6)) == [dump]
    assert knobkraft.splitSysex(tg77.createEditBufferRequest(6)) == [
        [0xF0, 0x43, 0x16, 0x34, 0x0D, 0, 0, 5, 0, 0x7F, 0xF7],
        dump,
        [0xF0, 0x43, 0x16, 0x34, 0x0D, 0, 0, 0, 0, 0x7F, 0xF7],
    ]
    assert tg77.generalMessageDelay() == 800


def test_tg77_front_panel_single_voice_preserves_the_reported_slot():
    voice = knobkraft.load_sysex(str(FIXTURES / TG77_INTERNAL))[0]
    manual = _with_memory(voice, 6, 0x7F, 0x2E)
    assert tg77.numberFromDump(manual) == 0x2E
    assert tg77.isSingleProgramDump(manual)
    assert not tg77.isEditBufferDump(manual)

    # A01 has the same address as the edit buffer.
    manual_a01 = _with_memory(voice, 6, 0x7F, 0)
    assert tg77.isEditBufferDump(manual_a01)
    assert not tg77.isSingleProgramDump(manual_a01)


def test_malformed_dumps_are_rejected_and_renames_preserve_sound():
    voices = knobkraft.load_sysex(str(FIXTURES / TG77_INTERNAL))
    voice = voices[0]
    invalid_length = voice.copy()
    invalid_length[5] ^= 1
    invalid_checksum = voice.copy()
    invalid_checksum[-2] ^= 1
    for corrupt in (voice[:-1], invalid_length, invalid_checksum, _system_reply(0, 8)):
        assert not tg77.isSingleProgramDump(corrupt)
        assert not tg77.isEditBufferDump(corrupt)
        assert tg77.convertToProgramDump(0, corrupt, 0) == []

    renamed = tg77.renamePatch(voice, "Tést")
    assert tg77.nameFromDump(renamed) == "T st"
    assert tg77.isSingleProgramDump(renamed)
    assert tg77.calculateFingerprint(renamed) == tg77.calculateFingerprint(voice)
    assert tg77.calculateFingerprint(voices[19]) != tg77.calculateFingerprint(voice)
