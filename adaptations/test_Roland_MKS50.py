from pathlib import Path

import knobkraft
import pytest

import Roland_MKS50 as mks50
from testing.librarian import Librarian
from testing.mock_midi import MockMidiController, ScriptedMockDevice


def _encode_packed_patch(name: str) -> list[int]:
    packed = [0] * 32
    for i, ch in enumerate(name[:10]):
        packed[21 + i] = mks50.PATCH_NAME_CHARS.index(ch)
    return packed


def _nibblize(packed: list[int]) -> list[int]:
    wire = []
    for byte in packed:
        wire.append(byte & 0x0F)
        wire.append((byte >> 4) & 0x0F)
    return wire


def _build_bld_block(channel: int = 0) -> list[int]:
    payload = []
    for name in ["ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZabcd", "efghijklmn"]:
        payload.extend(_nibblize(_encode_packed_patch(name)))
    return [
        0xF0,
        mks50.ROLAND_ID,
        mks50.OP_BLD,
        channel & 0x0F,
        mks50.MKS50_ID,
        mks50.LEVEL_TONE,
        mks50.GROUP_TONE,
        0x00,
        0x00,
    ] + payload + [0xF7]


def _build_dat_block(block_no: int, channel: int = 0) -> list[int]:
    payload = []
    for patch_no in range(4):
        name = f"{block_no:02d}{patch_no:02d}ABCDEF"
        payload.extend(_nibblize(_encode_packed_patch(name)))
    assert len(payload) == 256
    checksum = (-sum(payload)) & 0x7F
    return [
        0xF0,
        mks50.ROLAND_ID,
        mks50.OP_DAT,
        channel & 0x0F,
        mks50.MKS50_ID,
    ] + payload + [checksum, 0xF7]


def _build_dat_bank_messages(channel: int = 0, duplicate_first_dat: bool = False) -> list[list[int]]:
    messages = [[0xF0, mks50.ROLAND_ID, mks50.OP_WSF, channel & 0x0F, mks50.MKS50_ID, 0xF7]]
    for block_no in range(16):
        dat = _build_dat_block(block_no, channel)
        messages.append(dat)
        if duplicate_first_dat and block_no == 0:
            messages.append(dat.copy())
    messages.append([0xF0, mks50.ROLAND_ID, mks50.OP_EOF, channel & 0x0F, mks50.MKS50_ID, 0xF7])
    return messages


def _ack(channel: int = 0) -> list[int]:
    return [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, channel & 0x0F, mks50.MKS50_ID, 0xF7]


def _fixture_path(filename: str) -> Path:
    return Path(__file__).parent / "testData" / "Roland_MKS50" / filename


def _load_fixture_messages(filename: str) -> list[list[int]]:
    return knobkraft.load_sysex(str(_fixture_path(filename)))


def test_extract_bld_into_apr_and_edit_buffer_conversion():
    message = _build_bld_block(channel=1)
    patches = mks50.extractPatchesFromAllBankMessages([message])

    assert len(patches) == 4
    assert all(mks50.isEditBufferDump(patch) for patch in patches)
    assert mks50.nameFromDump(patches[0]) == "ABCDEFGHIJ"

    converted = mks50.convertToEditBuffer(7, patches[0])
    assert converted[3] == 7


@pytest.mark.parametrize(
    "filename, expected_names",
    [
        ("FACTORYA.SYX", ["PolySynth1", "JazzGuitar", "Xylophone "]),
        ("GLORIOUS_ALPHA.SYX", ["AlphaBrass", "Synth Brew", "HyperPulse"]),
        ("ALIEN_MKS50_aJUNO1-2.SYX", ["RAVEMASTER", "Wave   EXP", "Null      "]),
        ("MIKEJUNO.SYX", ["Euro Bass ", "Acidic 1  ", "What Stab "]),
        ("MISC1_MKS50_aJUNO1-2.SYX", ["MunaStrong", "Fell Gated", "Hombrah   "]),
        ("EZBANK1.SYX", ["EZKick    ", "EZTom     ", "EZSnare   "]),
    ],
)
def test_real_tone_bank_fixtures_extract_64_patches(filename, expected_names):
    messages = _load_fixture_messages(filename)
    patches = mks50.extractPatchesFromAllBankMessages(messages)

    assert len(patches) == 64
    assert [mks50.nameFromDump(patch) for patch in patches[:3]] == expected_names
    assert all(mks50.isEditBufferDump(patch) for patch in patches)


@pytest.mark.parametrize("filename", [
    "x-PatchParameters-MKS50-FACTORYA.SYX",
    "x-ChordMemory-MKS50.SYX",
])
def test_real_non_tone_fixtures_extract_no_tone_patches(filename):
    messages = _load_fixture_messages(filename)

    assert mks50.extractPatchesFromAllBankMessages(messages) == []


@pytest.mark.parametrize(
    "filename",
    [
        "FACTORYA.SYX",
        "GLORIOUS_ALPHA.SYX",
        "ALIEN_MKS50_aJUNO1-2.SYX",
        "MIKEJUNO.SYX",
        "MISC1_MKS50_aJUNO1-2.SYX",
    ],
)
def test_real_tone_banks_round_trip_to_alpha_juno_bld_format(filename):
    messages = _load_fixture_messages(filename)
    patches = mks50.extractPatchesFromAllBankMessages(messages)

    exported = mks50.convertPatchesToBankDump(patches)

    assert exported == messages
    assert len(exported) == 16
    for block_no, message in enumerate(exported):
        assert message[:9] == [
            0xF0,
            mks50.ROLAND_ID,
            mks50.OP_BLD,
            0x00,
            mks50.MKS50_ID,
            mks50.LEVEL_TONE,
            mks50.GROUP_TONE,
            0x00,
            block_no * 4,
        ]
        assert message[-1] == 0xF7


def test_partial_bank_export_pads_to_64_valid_tones():
    source_messages = _load_fixture_messages("GLORIOUS_ALPHA.SYX")
    source_patches = mks50.extractPatchesFromAllBankMessages(source_messages)[:2]

    exported = mks50.convertPatchesToBankDump(source_patches)
    reimported = mks50.extractPatchesFromAllBankMessages(exported)

    assert len(exported) == 16
    assert len(reimported) == 64
    assert [mks50.nameFromDump(patch) for patch in reimported[:3]] == [
        "AlphaBrass",
        "Synth Brew",
        "AAAAAAAAAA",
    ]
    assert mks50.calculateFingerprint(reimported[0]) == mks50.calculateFingerprint(source_patches[0])
    assert mks50.calculateFingerprint(reimported[1]) == mks50.calculateFingerprint(source_patches[1])
    assert all(mks50.isEditBufferDump(patch) for patch in reimported)


def test_bank_export_rejects_more_than_64_patches():
    source_messages = _load_fixture_messages("FACTORYA.SYX")
    source_patches = mks50.extractPatchesFromAllBankMessages(source_messages)

    with pytest.raises(ValueError, match="at most 64"):
        mks50.convertPatchesToBankDump(source_patches + [source_patches[0]])


def test_ezbank1_fixture_split_sysex_ignores_truncated_tail():
    data = list(_fixture_path("EZBANK1.SYX").read_bytes())

    messages = knobkraft.splitSysex(data)

    assert len(messages) == 16
    assert all(message[0] == 0xF0 and message[-1] == 0xF7 for message in messages)


def test_bank_handshake_tuple_replies():
    mks50.createBankDumpRequest(0, 0)

    wsf = [0xF0, mks50.ROLAND_ID, mks50.OP_WSF, 0x03, mks50.MKS50_ID, 0xF7]
    part = mks50.isPartOfBankDump(wsf)
    assert isinstance(part, tuple)
    assert part[0] is False
    assert part[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    dat = [0xF0, mks50.ROLAND_ID, mks50.OP_DAT, 0x03, mks50.MKS50_ID, 0x00, 0xF7]
    part = mks50.isPartOfBankDump(dat)
    assert isinstance(part, tuple)
    assert part[0] is True
    assert part[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    eof = [0xF0, mks50.ROLAND_ID, mks50.OP_EOF, 0x03, mks50.MKS50_ID, 0xF7]
    part = mks50.isPartOfBankDump(eof)
    assert isinstance(part, tuple)
    assert part[0] is True
    assert part[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    finished = mks50.isBankDumpFinished([dat])
    assert isinstance(finished, tuple)


def test_channel_detection_and_conversion_use_message_channel():
    message = _build_bld_block(channel=9)
    patch = mks50.extractPatchesFromAllBankMessages([message])[0]

    assert mks50.channelIfValidDeviceResponse(message) == 9
    assert mks50.convertToEditBuffer(6, patch)[3] == 6


def test_duplicate_handshake_frames_still_acknowledge():
    mks50.createBankDumpRequest(0, 0)

    wsf = [0xF0, mks50.ROLAND_ID, mks50.OP_WSF, 0x03, mks50.MKS50_ID, 0xF7]
    assert mks50.isPartOfBankDump(wsf)[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    duplicate_wsf = mks50.isPartOfBankDump(wsf)
    assert isinstance(duplicate_wsf, tuple)
    assert duplicate_wsf[0] is False
    assert duplicate_wsf[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    dat = [0xF0, mks50.ROLAND_ID, mks50.OP_DAT, 0x03, mks50.MKS50_ID, 0x00, 0xF7]
    assert mks50.isPartOfBankDump(dat)[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    duplicate_dat = mks50.isPartOfBankDump(dat)
    assert isinstance(duplicate_dat, tuple)
    assert duplicate_dat[0] is False
    assert duplicate_dat[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    eof = [0xF0, mks50.ROLAND_ID, mks50.OP_EOF, 0x03, mks50.MKS50_ID, 0xF7]
    assert mks50.isPartOfBankDump(eof)[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    duplicate_eof = mks50.isPartOfBankDump(eof)
    assert isinstance(duplicate_eof, tuple)
    assert duplicate_eof[0] is False
    assert duplicate_eof[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]


def test_dat_transfer_finishes_on_eof_and_extracts_64_patches():
    mks50.createBankDumpRequest(0, 0)
    stored_messages = []

    for message in _build_dat_bank_messages(channel=2):
        part = mks50.isPartOfBankDump(message)
        if isinstance(part, tuple):
            is_part = part[0]
        else:
            is_part = part
        if is_part:
            stored_messages.append(message)

    assert len(stored_messages) == 17
    finished = mks50.isBankDumpFinished(stored_messages)
    assert isinstance(finished, tuple)
    assert finished[0] is True

    patches = mks50.extractPatchesFromAllBankMessages(stored_messages)
    assert len(patches) == 64
    assert mks50.nameFromDump(patches[0]) == "0000ABCDEF"
    assert mks50.nameFromDump(patches[-1]) == "1503ABCDEF"


def test_offline_dat_window_can_finish_when_only_checked_after_bank_parts():
    mks50.createBankDumpRequest(0, 0)
    current_bank = []
    finished = False

    for message in _build_dat_bank_messages(channel=2):
        part = mks50.isPartOfBankDump(message)
        if isinstance(part, tuple):
            is_part = part[0]
        else:
            is_part = part
        if is_part:
            current_bank.append(message)
            finished_reply = mks50.isBankDumpFinished(current_bank)
            assert isinstance(finished_reply, tuple)
            if finished_reply[0]:
                finished = True
                break

    assert finished is True
    assert len(mks50.extractPatchesFromAllBankMessages(current_bank)) == 64


def test_mock_midi_dat_download_acks_handshake_and_drops_duplicate_dat():
    result = []
    librarian = Librarian()
    controller = MockMidiController(ScriptedMockDevice({}, ignore_unmatched=True))

    librarian.start_downloading_all_patches(
        controller,
        2,
        mks50,
        0,
        lambda patches: result.extend(patches),
    )
    controller.pending_replies.extend(_build_dat_bank_messages(channel=2, duplicate_first_dat=True))
    controller.drain()

    assert controller.finished is True
    assert len(result) == 64
    assert mks50.nameFromDump(result[0]) == "0000ABCDEF"
    assert mks50.nameFromDump(result[-1]) == "1503ABCDEF"

    assert controller.sent_messages == [_ack(2)] * 19


def test_bank_finished_after_16_bld_blocks():
    mks50.createBankDumpRequest(0, 0)

    # For transfer progress tracking, the callback only needs BLD opcodes.
    for index in range(16):
        bld_marker = [0xF0, mks50.ROLAND_ID, mks50.OP_BLD, 0x00, mks50.MKS50_ID, index & 0x7F, 0xF7]
        assert mks50.isPartOfBankDump(bld_marker) is True

    finished = mks50.isBankDumpFinished([])
    assert isinstance(finished, tuple)
    assert finished[0] is True


def test_bank_timeout_midflight_then_restart_from_beginning():
    mks50.createBankDumpRequest(0, 0)

    wsf = [0xF0, mks50.ROLAND_ID, mks50.OP_WSF, 0x01, mks50.MKS50_ID, 0xF7]
    dat = [0xF0, mks50.ROLAND_ID, mks50.OP_DAT, 0x01, mks50.MKS50_ID, 0x00, 0xF7]
    assert mks50.isPartOfBankDump(wsf)[0] is False
    assert mks50.isPartOfBankDump(dat)[0] is True

    # Timeout sentinel relayed by Librarian is an empty message.
    assert mks50.isPartOfBankDump([]) is False

    # Transfer state must be reset after timeout: DAT without WSF must now be rejected.
    dat_after_timeout = mks50.isPartOfBankDump(dat)
    assert isinstance(dat_after_timeout, tuple)
    assert dat_after_timeout[0] is False
    assert dat_after_timeout[1][2] == mks50.OP_RJC

    # Restart from scratch and complete in BLD mode.
    mks50.createBankDumpRequest(0, 0)
    for index in range(16):
        bld_marker = [0xF0, mks50.ROLAND_ID, mks50.OP_BLD, 0x00, mks50.MKS50_ID, index & 0x7F, 0xF7]
        assert mks50.isPartOfBankDump(bld_marker) is True
    finished = mks50.isBankDumpFinished([])
    assert isinstance(finished, tuple)
    assert finished[0] is True


def test_excessive_wsf_rejects_transfer():
    mks50.createBankDumpRequest(0, 0)
    wsf = [0xF0, mks50.ROLAND_ID, mks50.OP_WSF, 0x01, mks50.MKS50_ID, 0xF7]

    assert mks50.isPartOfBankDump(wsf)[1][2] == mks50.OP_ACK
    assert mks50.isPartOfBankDump([*wsf[:3], 0x02, *wsf[4:]])[1][2] == mks50.OP_ACK
    rejected = mks50.isPartOfBankDump([*wsf[:3], 0x03, *wsf[4:]])

    assert rejected[0] is False
    assert rejected[1][2] == mks50.OP_RJC


def test_rqf_and_err_abort_transfer():
    mks50.createBankDumpRequest(0, 0)
    rqf = [0xF0, mks50.ROLAND_ID, mks50.OP_RQF, 0x01, mks50.MKS50_ID, 0xF7]
    rejected = mks50.isPartOfBankDump(rqf)

    assert rejected[0] is False
    assert rejected[1][2] == mks50.OP_RJC
    assert mks50.isBankDumpFinished([])[0] is True

    mks50.createBankDumpRequest(0, 0)
    err = [0xF0, mks50.ROLAND_ID, mks50.OP_ERR, 0x01, mks50.MKS50_ID, 0xF7]
    assert mks50.isPartOfBankDump(err) is False
    assert mks50.isBankDumpFinished([])[0] is True


def test_dat_checksum_error_is_reported():
    dat = _build_dat_block(0, channel=0)
    dat[-2] = (dat[-2] + 1) & 0x7F

    with pytest.raises(Exception, match="DAT checksum error"):
        mks50.extractPatchesFromAllBankMessages([dat])


def test_dat_patch_data_is_rejected_by_default_name_marker():
    payload = [0] * 256
    checksum = (-sum(payload)) & 0x7F
    dat = [0xF0, mks50.ROLAND_ID, mks50.OP_DAT, 0x00, mks50.MKS50_ID] + payload + [checksum, 0xF7]

    with pytest.raises(Exception, match="patch data, not tone data"):
        mks50.extractPatchesFromAllBankMessages([dat])


def test_legacy_cpp_payload_is_supported():
    legacy_payload = [value & 0x7F for value in range(36)]

    assert mks50.isEditBufferDump(legacy_payload) is True
    assert mks50.nameFromDump(legacy_payload) == "AAAAAAAAAA"

    converted = mks50.convertToEditBuffer(5, legacy_payload)
    assert mks50.isEditBufferDump(converted) is True
    assert converted[3] == 5


def test_legacy_payload_and_apr_have_same_fingerprint():
    legacy_payload = [value & 0x7F for value in range(36)]
    renamed = mks50.renamePatch(legacy_payload, "LEGACYNAME")

    fp_legacy = mks50.calculateFingerprint(legacy_payload)
    fp_apr = mks50.calculateFingerprint(renamed)

    assert fp_legacy == fp_apr
    assert mks50.nameFromDump(renamed) == "LEGACYNAME"


def test_rename_truncates_and_replaces_unsupported_characters():
    legacy_payload = [value & 0x7F for value in range(36)]

    renamed = mks50.renamePatch(legacy_payload, "BAD*NAME!TOOLONG")

    assert mks50.nameFromDump(renamed) == "BAD NAME T"
