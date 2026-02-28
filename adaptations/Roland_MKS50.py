#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib
from typing import Dict, List, Optional, Sequence, Tuple

import knobkraft

ROLAND_ID = 0x41
MKS50_ID = 0x23

OP_APR = 0x35
OP_BLD = 0x37
OP_IPR = 0x36
OP_WSF = 0x40
OP_RQF = 0x41
OP_DAT = 0x42
OP_ACK = 0x43
OP_EOF = 0x45
OP_ERR = 0x4E
OP_RJC = 0x4F

LEVEL_TONE = 0x20
GROUP_TONE = 0x01

PATCH_NAME_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -"
SPACE_CHAR_INDEX = PATCH_NAME_CHARS.index(" ")
APR_TONE_DATA_SIZE = 36
APR_NAME_SIZE = 10
LEGACY_DEFAULT_NAME = "AAAAAAAAAA"

# Mapping copied from C++ MKS50_Patch.cpp (PackedDataFormatInfo table),
# with enum targets resolved to APR tone data indices.
TONE_MAPPING: List[Tuple[int, int, int, int, int]] = [
    (0, 0, 4, 20, 0),
    (0, 4, 4, 13, 0),
    (1, 0, 4, 23, 0),
    (1, 4, 4, 21, 0),
    (2, 0, 4, 35, 0),
    (2, 4, 4, 33, 0),
    (3, 0, 7, 11, 0),
    (4, 0, 7, 12, 0),
    (5, 0, 7, 14, 0),
    (6, 0, 7, 15, 0),
    (7, 0, 7, 16, 0),
    (8, 0, 7, 17, 0),
    (9, 0, 7, 19, 0),
    (10, 0, 7, 18, 0),
    (11, 0, 7, 22, 0),
    (12, 0, 7, 24, 0),
    (13, 0, 7, 25, 0),
    (14, 0, 7, 26, 0),
    (15, 0, 7, 27, 0),
    (16, 0, 7, 28, 0),
    (17, 0, 7, 29, 0),
    (18, 0, 7, 30, 0),
    (19, 0, 7, 31, 0),
    (20, 0, 7, 32, 0),
    (4, 7, 1, 10, 0),
    (5, 7, 1, 0, 1),
    (6, 7, 1, 0, 0),
    (7, 7, 1, 1, 1),
    (8, 7, 1, 1, 0),
    (9, 7, 1, 2, 1),
    (10, 7, 1, 2, 0),
    (11, 7, 1, 5, 2),
    (12, 7, 1, 5, 1),
    (13, 7, 1, 5, 0),
    (14, 7, 1, 4, 2),
    (15, 7, 1, 4, 1),
    (16, 7, 1, 4, 0),
    (17, 7, 1, 3, 1),
    (18, 7, 1, 3, 0),
    (19, 7, 1, 9, 1),
    (20, 7, 1, 9, 0),
    (21, 7, 1, 6, 1),
    (22, 7, 1, 6, 0),
    (23, 7, 1, 7, 1),
    (24, 7, 1, 7, 0),
    (25, 7, 1, 8, 1),
    (26, 7, 1, 8, 0),
    (27, 6, 2, 34, 0),
    (28, 6, 2, 34, 2),
    (29, 6, 2, 34, 4),
    (30, 6, 2, 34, 6),
]

_previous_sysex: Optional[List[int]] = None
_transfer_mode: Optional[str] = None
_num_wsf = 0
_data_packages = 0
_saw_eof = False
_transfer_aborted = False


def _reset_bank_state() -> None:
    global _previous_sysex, _transfer_mode, _num_wsf, _data_packages, _saw_eof, _transfer_aborted
    _previous_sysex = None
    _transfer_mode = None
    _num_wsf = 0
    _data_packages = 0
    _saw_eof = False
    _transfer_aborted = False


def name() -> str:
    return "Roland MKS-50"


def setupHelp() -> str:
    return (
        "The MKS-50 cannot be queried for full bank dumps from software.\n\n"
        "To download banks, start a bulk dump from the MKS-50 front panel.\n"
        "Use Tone dump modes (for example T-a / T-b), not patch/chord dump modes.\n\n"
        "For two-way dumps, the adaptation now sends handshake ACK/RJC replies automatically."
    )


def numberOfBanks() -> int:
    return 2


def numberOfPatchesPerBank() -> int:
    return 64


def bankDescriptors() -> List[Dict]:
    return [
        {"bank": 0, "name": "Bank A", "size": 64, "type": "Tone"},
        {"bank": 1, "name": "Bank B", "size": 64, "type": "Tone"},
    ]


def friendlyBankName(bank_number: int) -> str:
    return "Bank A" if bank_number == 0 else "Bank B"


def bankSelect(channel: int, bank: int) -> List[int]:
    # MKS-50 program selection is done by simple program change in this adaptation.
    return []


def createDeviceDetectMessage(channel: int) -> List[int]:
    # Native implementation is passive as well: we cannot actively detect the MKS-50.
    return []


def deviceDetectWaitMilliseconds() -> int:
    return 100


def needsChannelSpecificDetection() -> bool:
    return False


def channelIfValidDeviceResponse(message: List[int]) -> int:
    if _is_own_sysex(message):
        return message[3] & 0x0F
    return -1


def createEditBufferRequest(channel: int) -> List[int]:
    # MKS-50 has no direct edit-buffer request. Program change triggers APR tone dump.
    return []


def isEditBufferDump(message: List[int]) -> bool:
    messages = _normalize_sysex_list(message)
    if len(messages) == 1 and _is_tone_apr(messages[0]):
        return True
    return _is_legacy_apr_payload(message) or _is_legacy_apr_payload_with_name(message)


def isPartOfEditBufferDump(message: List[int]) -> bool:
    return _is_tone_apr(message)


def convertToEditBuffer(channel: int, message: List[int]) -> List[int]:
    messages = _normalize_sysex_list(message)
    if len(messages) == 1 and _is_tone_apr(messages[0]):
        converted = messages[0][:]
        converted[3] = channel & 0x0F
        return converted
    if _is_legacy_apr_payload(message):
        return _make_apr_message_from_payload(channel, message, _string_to_data(LEGACY_DEFAULT_NAME))
    if _is_legacy_apr_payload_with_name(message):
        return _make_apr_message_from_payload(channel, message[:APR_TONE_DATA_SIZE], message[APR_TONE_DATA_SIZE:])
    raise Exception("MKS-50 convertToEditBuffer expects a single APR tone dump")


def createProgramDumpRequest(channel: int, patchNo: int) -> List[int]:
    # Native code also relies on program change + empty edit-buffer request.
    return [0xC0 | (channel & 0x0F), patchNo & 0x7F]


def isSingleProgramDump(message: List[int]) -> bool:
    return isEditBufferDump(message)


def isPartOfSingleProgramDump(message: List[int]) -> bool:
    return _is_tone_apr(message)


def convertToProgramDump(channel: int, message: List[int], program_number: int) -> List[int]:
    # MKS-50 has no direct "store to location" sysex in this adaptation path.
    del program_number
    return convertToEditBuffer(channel, message)


def isPartOfBankDump(message: List[int]):
    global _previous_sysex, _transfer_mode, _num_wsf, _data_packages, _saw_eof, _transfer_aborted

    if len(message) == 0:
        # Timeout sentinel from Librarian callback path.
        _reset_bank_state()
        return False

    if not _is_own_sysex(message):
        return False

    operation = _operation(message)
    is_duplicate = _previous_sysex == message
    _previous_sysex = message[:]

    # Native code drops duplicate messages from potentially looped setups, but
    # transfer handshake frames still need ACK/RJC replies when they repeat.
    if is_duplicate and not _is_handshake_transfer_frame(message):
        return False

    if operation == OP_BLD:
        _transfer_mode = "BLD"
        _data_packages += 1
        return True

    if operation == OP_WSF:
        if not is_duplicate:
            _num_wsf += 1
        if _num_wsf > 2:
            _transfer_aborted = True
            return False, _build_handshake_reply(OP_RJC, message)
        return False, _build_handshake_reply(OP_ACK, message)

    if operation == OP_DAT:
        if _num_wsf < 1:
            _transfer_aborted = True
            return False, _build_handshake_reply(OP_RJC, message)
        _transfer_mode = "DAT"
        if not is_duplicate:
            _data_packages += 1
        return True, _build_handshake_reply(OP_ACK, message)

    if operation == OP_RQF:
        _transfer_aborted = True
        return False, _build_handshake_reply(OP_RJC, message)

    if operation == OP_EOF:
        _transfer_mode = "DAT"
        _saw_eof = True
        return False, _build_handshake_reply(OP_ACK, message)

    if operation in (OP_RJC, OP_ERR):
        _transfer_aborted = True
        return False

    return False


def isBankDumpFinished(messages: List[List[int]]):
    normalized = _normalize_bank_message_list(messages)

    if _transfer_aborted:
        _reset_bank_state()
        return True, []

    if _transfer_mode == "DAT":
        is_finished = _saw_eof and _data_packages >= 16
        if is_finished:
            _reset_bank_state()
        return is_finished, []

    if _transfer_mode == "BLD":
        is_finished = _data_packages >= 16
        if is_finished:
            _reset_bank_state()
        return is_finished, []

    # Fallback path for offline file parsing without transfer state.
    bulk_blocks = sum(1 for m in normalized if _is_own_sysex(m) and _operation(m) in (OP_BLD, OP_DAT))
    is_finished = bulk_blocks >= 16
    if is_finished:
        _reset_bank_state()
    return is_finished, []


def createBankDumpRequest(channel: int, bank: int) -> List[int]:
    del channel, bank
    _reset_bank_state()
    # Passive: user starts bulk dump on front panel.
    return []


def extractPatchesFromAllBankMessages(messages: List[List[int]]) -> List[List[int]]:
    normalized = _normalize_bank_message_list(messages)
    result: List[List[int]] = []
    dat_package_counter = 0

    for message in normalized:
        if not _is_own_sysex(message):
            continue

        operation = _operation(message)

        if operation == OP_BLD:
            if _is_tone_bld(message):
                result.extend(_extract_apr_patches_from_block(message, 9))
        elif operation == OP_DAT:
            if len(message) == 263:
                if not _valid_dat_checksum(message):
                    raise Exception("MKS-50 DAT checksum error")
                if dat_package_counter < 16:
                    tone_patches = _extract_apr_patches_from_block(message, 5)
                    for patch in tone_patches:
                        if nameFromDump(patch) == "AAAAAAAAAA":
                            raise Exception("MKS-50 dump is patch data, not tone data. Use Bulk Dump [T-a]/[T-b].")
                    result.extend(tone_patches)
                dat_package_counter += 1
            # DAT chord blocks (length 199) are ignored like native code.
        elif operation == OP_APR and _is_tone_apr(message):
            result.append(message)

    return result


def extractPatchesFromBank(message: List[int]) -> List[int]:
    patches = extractPatchesFromAllBankMessages([message])
    result: List[int] = []
    for patch in patches:
        result.extend(patch)
    return result


def nameFromDump(message: List[int]) -> str:
    messages = _normalize_sysex_list(message)
    if len(messages) != 1:
        if _is_legacy_apr_payload(message):
            return LEGACY_DEFAULT_NAME
        if _is_legacy_apr_payload_with_name(message):
            return _data_to_string(message[APR_TONE_DATA_SIZE:])
        raise Exception("MKS-50 nameFromDump expects a single sysex message")

    data = messages[0]
    if _is_tone_apr(data):
        return _data_to_string(data[43:53])

    if _is_tone_bld(data):
        packed = _extract_single_packed_patch(data, 0, 9)
        return _decode_name_from_packed(packed)

    if _operation(data) == OP_DAT and len(data) == 263:
        packed = _extract_single_packed_patch(data, 0, 5)
        return _decode_name_from_packed(packed)

    raise Exception("MKS-50 nameFromDump cannot parse this message type")


def renamePatch(message: List[int], new_name: str) -> List[int]:
    messages = _normalize_sysex_list(message)
    if len(messages) != 1 or not _is_tone_apr(messages[0]):
        if _is_legacy_apr_payload(message):
            return _make_apr_message_from_payload(0, message, _string_to_data(new_name[:APR_NAME_SIZE]))
        if _is_legacy_apr_payload_with_name(message):
            return _make_apr_message_from_payload(
                0, message[:APR_TONE_DATA_SIZE], _string_to_data(new_name[:APR_NAME_SIZE])
            )
        raise Exception("MKS-50 renamePatch expects a single APR tone dump")

    patched = messages[0][:]
    name_data = _string_to_data(new_name[:10])
    while len(name_data) < 10:
        name_data.append(SPACE_CHAR_INDEX)
    patched[43:53] = name_data
    return patched


def isDefaultName(patchName: str) -> bool:
    return patchName == "AAAAAAAAAA"


def calculateFingerprint(message: List[int]) -> str:
    messages = _normalize_sysex_list(message)
    if len(messages) != 1:
        if _is_legacy_apr_payload(message):
            return hashlib.md5(bytearray(message[:APR_TONE_DATA_SIZE])).hexdigest()
        if _is_legacy_apr_payload_with_name(message):
            return hashlib.md5(bytearray(message[:APR_TONE_DATA_SIZE])).hexdigest()
        raise Exception("MKS-50 calculateFingerprint expects one sysex message")

    data = messages[0]
    if _is_tone_apr(data):
        # Ignore channel, name and wrapper bytes for better duplicate detection.
        payload = data[7:43]
        return hashlib.md5(bytearray(payload)).hexdigest()

    if _is_tone_bld(data):
        packed = _extract_single_packed_patch(data, 0, 9)
        apr_data = _apply_tone_mapping(packed)
        return hashlib.md5(bytearray(apr_data)).hexdigest()

    if _operation(data) == OP_DAT and len(data) == 263:
        packed = _extract_single_packed_patch(data, 0, 5)
        apr_data = _apply_tone_mapping(packed)
        return hashlib.md5(bytearray(apr_data)).hexdigest()

    return hashlib.md5(bytearray(message)).hexdigest()


def _normalize_sysex_list(message: List[int]) -> List[List[int]]:
    return knobkraft.splitSysexMessage(message)


def _normalize_bank_message_list(messages: Sequence) -> List[List[int]]:
    if len(messages) == 0:
        return []
    first = messages[0]
    if isinstance(first, int):
        return knobkraft.splitSysexMessage(messages)  # type: ignore[arg-type]
    return list(messages)  # type: ignore[return-value]


def _is_own_sysex(message: List[int]) -> bool:
    return (
        len(message) >= 6
        and message[0] == 0xF0
        and message[-1] == 0xF7
        and message[1] == ROLAND_ID
        and message[4] == MKS50_ID
    )


def _operation(message: List[int]) -> int:
    return message[2]


def _is_handshake_transfer_frame(message: List[int]) -> bool:
    return _is_own_sysex(message) and _operation(message) in (OP_WSF, OP_DAT, OP_EOF)


def _is_tone_apr(message: List[int]) -> bool:
    return (
        _is_own_sysex(message)
        and _operation(message) == OP_APR
        and len(message) >= 54
        and message[5] == LEVEL_TONE
        and message[6] == GROUP_TONE
    )


def _is_legacy_apr_payload(message: List[int]) -> bool:
    return len(message) == APR_TONE_DATA_SIZE and all(0 <= value < 0x80 for value in message)


def _is_legacy_apr_payload_with_name(message: List[int]) -> bool:
    return len(message) == APR_TONE_DATA_SIZE + APR_NAME_SIZE and all(0 <= value < 0x80 for value in message)


def _is_tone_bld(message: List[int]) -> bool:
    return (
        _is_own_sysex(message)
        and _operation(message) == OP_BLD
        and len(message) >= 266
        and message[5] == LEVEL_TONE
        and message[6] == GROUP_TONE
        and message[7] == 0x00
    )


def _build_handshake_reply(operation: int, request: List[int]) -> List[int]:
    return [0xF0, ROLAND_ID, operation, request[3] & 0x0F, MKS50_ID, 0xF7]


def _make_apr_message_from_payload(channel: int, payload: List[int], name_data: List[int]) -> List[int]:
    result = [0xF0, ROLAND_ID, OP_APR, channel & 0x0F, MKS50_ID, LEVEL_TONE, GROUP_TONE]
    result.extend(payload[:APR_TONE_DATA_SIZE])
    normalized_name = name_data[:APR_NAME_SIZE]
    while len(normalized_name) < APR_NAME_SIZE:
        normalized_name.append(SPACE_CHAR_INDEX)
    result.extend(normalized_name)
    result.append(0xF7)
    return result


def _valid_dat_checksum(message: List[int]) -> bool:
    checksum = 0
    for index in range(5, 261):
        checksum = (checksum + message[index]) & 0x7F
    return ((checksum + message[261]) & 0x7F) == 0


def _extract_apr_patches_from_block(message: List[int], block_start: int) -> List[List[int]]:
    channel = message[3] & 0x0F
    patches: List[List[int]] = []
    for patch_no in range(4):
        packed = _extract_single_packed_patch(message, patch_no, block_start)
        name = _decode_name_from_packed(packed)
        apr_data = _apply_tone_mapping(packed)
        patch_message = [0xF0, ROLAND_ID, OP_APR, channel, MKS50_ID, LEVEL_TONE, GROUP_TONE]
        patch_message.extend(apr_data)
        patch_message.extend(_string_to_data(name))
        patch_message.append(0xF7)
        patches.append(patch_message)
    return patches


def _extract_single_packed_patch(message: List[int], patch_no: int, block_start: int) -> List[int]:
    packed: List[int] = []
    patch_offset = block_start + patch_no * 64
    for i in range(0, 64, 2):
        low = message[patch_offset + i] & 0x0F
        high = message[patch_offset + i + 1] & 0x0F
        packed.append(low | (high << 4))
    return packed


def _apply_tone_mapping(packed_data: List[int]) -> List[int]:
    apr_data = [0] * 36
    for source_byte, lsb_index, bit_count, target, target_bit_index in TONE_MAPPING:
        value = (packed_data[source_byte] >> lsb_index) & ((1 << bit_count) - 1)
        apr_data[target] |= value << target_bit_index
    return apr_data


def _decode_name_from_packed(packed_data: List[int]) -> str:
    name_data = [(packed_data[i] & 0x3F) for i in range(21, 31)]
    return _data_to_string(name_data)


def _data_to_string(data: Sequence[int]) -> str:
    result = []
    for byte_value in data:
        if 0 <= byte_value < len(PATCH_NAME_CHARS):
            result.append(PATCH_NAME_CHARS[byte_value])
        else:
            result.append("!")
    return "".join(result)


def _string_to_data(name: str) -> List[int]:
    result: List[int] = []
    for character in name:
        index = PATCH_NAME_CHARS.find(character)
        result.append(index if index >= 0 else SPACE_CHAR_INDEX)
    return result
