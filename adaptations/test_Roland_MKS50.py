import Roland_MKS50 as mks50


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


def test_extract_bld_into_apr_and_edit_buffer_conversion():
    message = _build_bld_block(channel=1)
    patches = mks50.extractPatchesFromAllBankMessages([message])

    assert len(patches) == 4
    assert all(mks50.isEditBufferDump(patch) for patch in patches)
    assert mks50.nameFromDump(patches[0]) == "ABCDEFGHIJ"

    converted = mks50.convertToEditBuffer(7, patches[0])
    assert converted[3] == 7


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
    assert part[0] is False
    assert part[1] == [0xF0, mks50.ROLAND_ID, mks50.OP_ACK, 0x03, mks50.MKS50_ID, 0xF7]

    finished = mks50.isBankDumpFinished([dat])
    assert isinstance(finished, tuple)


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
