#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

import knobkraft
import testing


ROLAND_ID = 0x41
MODEL_ID = 0x2B
COMMAND_RQ1 = 0x11
COMMAND_DT1 = 0x12

DEFAULT_DEVICE_ID = 0x10
ADDRESS_SIZE = 3
MAX_DT1_DATA = 0x80

SOUND_PATCH_BASE = (0x03, 0x00, 0x00)
SOUND_PATCH_COUNT = 64
SOUND_PATCH_INTERNAL_SIZE = 0x50
SOUND_PATCH_ENCODED_SIZE = SOUND_PATCH_INTERNAL_SIZE * 2
SOUND_PATCH_BANK_ENCODED_SIZE = SOUND_PATCH_COUNT * SOUND_PATCH_ENCODED_SIZE
PATCH_NAME_LENGTH = 12

EDIT_BUFFER_BASE = (0x00, 0x06, 0x00)
SETUP_MEMORY_ADDRESS = (0x00, 0x00, 0x00)
SETUP_MEMORY_SIZE = 0x10

_device_id = DEFAULT_DEVICE_ID


def name() -> str:
    return "Roland U-20 / U-220"


def bankDescriptors() -> List[Dict]:
    return [{"bank": 0, "name": "Internal Sound Patches", "size": SOUND_PATCH_COUNT, "type": "Sound Patch"}]


def friendlyProgramName(program: int) -> str:
    patch = program % SOUND_PATCH_COUNT
    return f"I-{(patch // 8) + 1}{(patch % 8) + 1}"


def _address_to_number(address: Iterable[int]) -> int:
    result = 0
    for value in address:
        result = (result << 7) | (value & 0x7F)
    return result


def _number_to_address(number: int) -> List[int]:
    return [(number >> 14) & 0x7F, (number >> 7) & 0x7F, number & 0x7F]


def _size_as_7bit_list(size: int) -> List[int]:
    return _number_to_address(size)


def _roland_checksum(data: Iterable[int]) -> int:
    return sum(-x for x in data) & 0x7F


def _build_roland_message(device_id: int, command_id: int, address: List[int], data: List[int]) -> List[int]:
    if len(address) != ADDRESS_SIZE:
        raise ValueError("U-20/U-220 SysEx uses three-byte addresses")
    if not all(0 <= value < 0x80 for value in address + data):
        raise ValueError("Roland SysEx data must be 7-bit clean")
    message = [0xF0, ROLAND_ID, device_id & 0x1F, MODEL_ID, command_id] + address + data + [0x00, 0xF7]
    message[-2] = _roland_checksum(message[5:-2])
    return message


def _iter_sysex_messages(messages: List[int]) -> Iterable[List[int]]:
    for start, end in knobkraft.sysex.findSysexDelimiters(messages):
        yield messages[start:end]


def _parse_roland_message(message: List[int]) -> Tuple[int, List[int], List[int]]:
    if not isOwnSysex(message):
        raise ValueError("Not a Roland U-20/U-220 SysEx message")
    expected = _roland_checksum(message[5:-2])
    if expected != message[-2]:
        raise ValueError(f"Checksum error, expected {expected:02x} but message contains {message[-2]:02x}")
    command = message[4]
    address = message[5:8]
    data = message[8:-2]
    return command, address, data


def _nibble(internal_data: Iterable[int]) -> List[int]:
    result: List[int] = []
    for value in internal_data:
        result.append(value & 0x0F)
        result.append((value >> 4) & 0x0F)
    return result


def _denibble(encoded_data: List[int]) -> List[int]:
    if len(encoded_data) % 2 != 0:
        raise ValueError("Nibble-packed U-20/U-220 data must contain an even number of values")
    return [(encoded_data[i] & 0x0F) | ((encoded_data[i + 1] & 0x0F) << 4) for i in range(0, len(encoded_data), 2)]


def _sound_patch_base_number() -> int:
    return _address_to_number(SOUND_PATCH_BASE)


def _sound_patch_start_address(patch_no: int) -> List[int]:
    return _number_to_address(_sound_patch_base_number() + (patch_no % SOUND_PATCH_COUNT) * SOUND_PATCH_ENCODED_SIZE)


def _edit_buffer_base_number() -> int:
    return _address_to_number(EDIT_BUFFER_BASE)


def _sound_patch_segment(address: List[int], data_length: int) -> Optional[Tuple[int, int]]:
    delta = _address_to_number(address) - _sound_patch_base_number()
    if delta < 0 or delta >= SOUND_PATCH_BANK_ENCODED_SIZE:
        return None
    patch_no = delta // SOUND_PATCH_ENCODED_SIZE
    offset = delta % SOUND_PATCH_ENCODED_SIZE
    expected_length = min(MAX_DT1_DATA, SOUND_PATCH_ENCODED_SIZE - offset)
    if patch_no >= SOUND_PATCH_COUNT or offset not in (0, MAX_DT1_DATA) or data_length != expected_length:
        return None
    return patch_no, offset


def _edit_buffer_segment(address: List[int], data_length: int) -> Optional[int]:
    delta = _address_to_number(address) - _edit_buffer_base_number()
    if delta < 0 or delta >= SOUND_PATCH_ENCODED_SIZE:
        return None
    expected_length = min(MAX_DT1_DATA, SOUND_PATCH_ENCODED_SIZE - delta)
    if delta not in (0, MAX_DT1_DATA) or data_length != expected_length:
        return None
    return delta


def _message_device_id(messages: List[int]) -> int:
    split = list(_iter_sysex_messages(messages))
    if split and isOwnSysex(split[0]):
        return split[0][2]
    return _device_id


def _encoded_data_from_program_dump(messages: List[int]) -> Tuple[int, List[int]]:
    parts: Dict[int, List[int]] = {}
    patch_numbers = set()
    for message in _iter_sysex_messages(messages):
        command, address, data = _parse_roland_message(message)
        if command != COMMAND_DT1:
            raise ValueError("Expected DT1 data-set messages")
        segment = _sound_patch_segment(address, len(data))
        if segment is None:
            raise ValueError("Message is not a sound patch program segment")
        patch_no, offset = segment
        patch_numbers.add(patch_no)
        parts[offset] = data

    if len(patch_numbers) != 1 or set(parts.keys()) != {0, MAX_DT1_DATA}:
        raise ValueError("Incomplete sound patch program dump")
    encoded = parts[0] + parts[MAX_DT1_DATA]
    if len(encoded) != SOUND_PATCH_ENCODED_SIZE:
        raise ValueError("Invalid sound patch program dump size")
    return patch_numbers.pop(), encoded


def _internal_data_from_program_dump(messages: List[int]) -> Tuple[int, List[int]]:
    patch_no, encoded = _encoded_data_from_program_dump(messages)
    return patch_no, _denibble(encoded)


def _encoded_data_from_edit_buffer(messages: List[int]) -> List[int]:
    parts: Dict[int, List[int]] = {}
    for message in _iter_sysex_messages(messages):
        command, address, data = _parse_roland_message(message)
        if command != COMMAND_DT1:
            raise ValueError("Expected DT1 data-set messages")
        offset = _edit_buffer_segment(address, len(data))
        if offset is None:
            raise ValueError("Message is not a sound patch edit-buffer segment")
        parts[offset] = data

    if set(parts.keys()) != {0, MAX_DT1_DATA}:
        raise ValueError("Incomplete sound patch edit buffer")
    encoded = parts[0] + parts[MAX_DT1_DATA]
    if len(encoded) != SOUND_PATCH_ENCODED_SIZE:
        raise ValueError("Invalid sound patch edit-buffer size")
    return encoded


def _internal_data_from_edit_buffer(messages: List[int]) -> List[int]:
    return _denibble(_encoded_data_from_edit_buffer(messages))


def _internal_data_from_any_dump(messages: List[int]) -> Tuple[Optional[int], List[int]]:
    try:
        return _internal_data_from_program_dump(messages)
    except ValueError:
        pass
    try:
        return None, _internal_data_from_edit_buffer(messages)
    except ValueError:
        pass
    raise ValueError("Expected a sound patch program dump or edit buffer")


def _program_dump_from_internal(patch_no: int, internal_data: List[int], device_id: int = DEFAULT_DEVICE_ID) -> List[int]:
    if len(internal_data) != SOUND_PATCH_INTERNAL_SIZE:
        raise ValueError(f"Sound patch data must contain {SOUND_PATCH_INTERNAL_SIZE} bytes")
    encoded = _nibble(internal_data)
    result: List[int] = []
    start = _address_to_number(_sound_patch_start_address(patch_no))
    for offset in range(0, SOUND_PATCH_ENCODED_SIZE, MAX_DT1_DATA):
        chunk = encoded[offset:offset + MAX_DT1_DATA]
        result.extend(_build_roland_message(device_id, COMMAND_DT1, _number_to_address(start + offset), chunk))
    return result


def _edit_buffer_from_internal(internal_data: List[int], device_id: int = DEFAULT_DEVICE_ID) -> List[int]:
    if len(internal_data) != SOUND_PATCH_INTERNAL_SIZE:
        raise ValueError(f"Sound patch data must contain {SOUND_PATCH_INTERNAL_SIZE} bytes")
    encoded = _nibble(internal_data)
    result: List[int] = []
    start = _edit_buffer_base_number()
    for offset in range(0, SOUND_PATCH_ENCODED_SIZE, MAX_DT1_DATA):
        chunk = encoded[offset:offset + MAX_DT1_DATA]
        result.extend(_build_roland_message(device_id, COMMAND_DT1, _number_to_address(start + offset), chunk))
    return result


def isOwnSysex(message: List[int]) -> bool:
    return (
        len(message) >= 10
        and message[0] == 0xF0
        and message[-1] == 0xF7
        and message[1] == ROLAND_ID
        and message[3] == MODEL_ID
    )


def createDeviceDetectMessage(channel: int) -> List[int]:
    return _build_roland_message((channel + 0x10) & 0x1F, COMMAND_RQ1, list(SETUP_MEMORY_ADDRESS), _size_as_7bit_list(SETUP_MEMORY_SIZE))


def channelIfValidDeviceResponse(message: List[int]) -> int:
    global _device_id
    if isOwnSysex(message):
        try:
            command, address, _ = _parse_roland_message(message)
        except ValueError:
            return -1
        if command == COMMAND_DT1 and address == list(SETUP_MEMORY_ADDRESS):
            _device_id = message[2]
            return message[2] & 0x0F
    return -1


def needsChannelSpecificDetection() -> bool:
    return True


def createProgramDumpRequest(channel: int, patchNo: int) -> List[int]:
    return _build_roland_message(_device_id, COMMAND_RQ1, _sound_patch_start_address(patchNo), _size_as_7bit_list(SOUND_PATCH_ENCODED_SIZE))


def createEditBufferRequest(channel: int) -> List[int]:
    return _build_roland_message(_device_id, COMMAND_RQ1, list(EDIT_BUFFER_BASE), _size_as_7bit_list(SOUND_PATCH_ENCODED_SIZE))


def createBankDumpRequest(channel: int, bank: int) -> List[int]:
    return _build_roland_message(_device_id, COMMAND_RQ1, list(SOUND_PATCH_BASE), _size_as_7bit_list(SOUND_PATCH_BANK_ENCODED_SIZE))


def isPartOfSingleProgramDump(message: List[int]) -> bool:
    if not isOwnSysex(message):
        return False
    try:
        command, address, data = _parse_roland_message(message)
    except ValueError:
        return False
    return command == COMMAND_DT1 and _sound_patch_segment(address, len(data)) is not None


def isSingleProgramDump(messages: List[int]) -> bool:
    try:
        _encoded_data_from_program_dump(messages)
        return True
    except ValueError:
        return False


def isPartOfEditBufferDump(message: List[int]) -> bool:
    if not isOwnSysex(message):
        return False
    try:
        command, address, data = _parse_roland_message(message)
    except ValueError:
        return False
    return command == COMMAND_DT1 and _edit_buffer_segment(address, len(data)) is not None


def isEditBufferDump(messages: List[int]) -> bool:
    try:
        _encoded_data_from_edit_buffer(messages)
        return True
    except ValueError:
        return False


def numberFromDump(message: List[int]) -> int:
    try:
        patch_no, _ = _encoded_data_from_program_dump(message)
        return patch_no
    except ValueError:
        return 0


def nameFromDump(message: List[int]) -> str:
    try:
        _, internal_data = _internal_data_from_any_dump(message)
        return bytes(internal_data[:PATCH_NAME_LENGTH]).decode("ascii", errors="replace")
    except ValueError:
        return "Invalid"


def renamePatch(message: List[int], new_name: str) -> List[int]:
    patch_no, internal_data = _internal_data_from_any_dump(message)
    encoded_name = (new_name or "")[:PATCH_NAME_LENGTH].ljust(PATCH_NAME_LENGTH).encode("ascii", errors="replace")
    internal_data[:PATCH_NAME_LENGTH] = list(encoded_name)
    if patch_no is None:
        return _edit_buffer_from_internal(internal_data, _message_device_id(message))
    return _program_dump_from_internal(patch_no, internal_data, _message_device_id(message))


def convertToProgramDump(channel: int, message: List[int], program_number: int) -> List[int]:
    _, internal_data = _internal_data_from_any_dump(message)
    return _program_dump_from_internal(program_number % SOUND_PATCH_COUNT, internal_data, _message_device_id(message))


def convertToEditBuffer(channel: int, message: List[int]) -> List[int]:
    _, internal_data = _internal_data_from_any_dump(message)
    return _edit_buffer_from_internal(internal_data, _message_device_id(message))


def blankedOut(message: List[int]) -> List[int]:
    _, internal_data = _internal_data_from_any_dump(message)
    blanked = internal_data.copy()
    blanked[:PATCH_NAME_LENGTH] = [0] * PATCH_NAME_LENGTH
    return blanked


def calculateFingerprint(message: List[int]) -> str:
    return hashlib.md5(bytearray(blankedOut(message))).hexdigest()


def _bank_message_offset(message: List[int]) -> Optional[int]:
    if not isOwnSysex(message):
        return None
    try:
        command, address, data = _parse_roland_message(message)
    except ValueError:
        return None
    if command != COMMAND_DT1 or len(data) > MAX_DT1_DATA:
        return None
    delta = _address_to_number(address) - _sound_patch_base_number()
    if delta < 0 or delta >= SOUND_PATCH_BANK_ENCODED_SIZE:
        return None
    return delta


def isPartOfBankDump(message) -> bool:
    if message and isinstance(message[0], list):
        return any(isPartOfBankDump(part) for part in message)
    return _bank_message_offset(message) is not None


def isBankDumpFinished(messages) -> bool:
    if not messages:
        return False
    if isinstance(messages[0], int):
        messages = list(_iter_sysex_messages(messages))

    chunks: Dict[int, List[int]] = {}
    for message in messages:
        offset = _bank_message_offset(message)
        if offset is None:
            continue
        try:
            _, _, data = _parse_roland_message(message)
        except ValueError:
            continue
        chunks[offset] = data

    expected_offsets = set(range(0, SOUND_PATCH_BANK_ENCODED_SIZE, MAX_DT1_DATA))
    return set(chunks.keys()) == expected_offsets and all(len(chunks[offset]) == MAX_DT1_DATA for offset in expected_offsets)


def _encoded_bank_from_messages(messages: List[List[int]]) -> List[int]:
    chunks: Dict[int, List[int]] = {}
    for message in messages:
        offset = _bank_message_offset(message)
        if offset is not None:
            _, _, data = _parse_roland_message(message)
            chunks[offset] = data
    if not isBankDumpFinished(messages):
        raise ValueError("Incomplete U-20/U-220 sound patch bank")
    encoded: List[int] = []
    for offset in range(0, SOUND_PATCH_BANK_ENCODED_SIZE, MAX_DT1_DATA):
        encoded.extend(chunks[offset])
    return encoded


def extractPatchesFromAllBankMessages(messages: List[List[int]]) -> List[List[int]]:
    encoded_bank = _encoded_bank_from_messages(messages)
    device_id = messages[0][2] if messages else _device_id
    patches: List[List[int]] = []
    for patch_no in range(SOUND_PATCH_COUNT):
        encoded = encoded_bank[patch_no * SOUND_PATCH_ENCODED_SIZE:(patch_no + 1) * SOUND_PATCH_ENCODED_SIZE]
        patches.append(_program_dump_from_internal(patch_no, _denibble(encoded), device_id))
    return patches


def extractPatchesFromBank(bank) -> List[int]:
    if bank and isinstance(bank[0], list):
        messages = bank
    else:
        messages = list(_iter_sysex_messages(bank))
    flattened: List[int] = []
    for patch in extractPatchesFromAllBankMessages(messages):
        flattened.extend(patch)
    return flattened


def convertPatchesToBankDump(patches) -> List[List[int]]:
    if patches and isinstance(patches[0], int):
        split_messages = knobkraft.splitSysex(patches)
        grouped_patches = []
        current_patch: List[int] = []
        for message in split_messages:
            current_patch.extend(message)
            if isSingleProgramDump(current_patch):
                grouped_patches.append(current_patch)
                current_patch = []
        patches = grouped_patches

    internal_by_number: Dict[int, List[int]] = {}
    for patch in patches:
        if isinstance(patch[0], int):
            if isSingleProgramDump(patch):
                patch_no, internal_data = _internal_data_from_program_dump(patch)
                internal_by_number[patch_no] = internal_data
        else:
            flat_patch = [byte for message in patch for byte in message]
            patch_no, internal_data = _internal_data_from_program_dump(flat_patch)
            internal_by_number[patch_no] = internal_data

    if set(internal_by_number.keys()) != set(range(SOUND_PATCH_COUNT)):
        raise ValueError("A complete U-20/U-220 sound patch bank requires all 64 sound patches")

    encoded_bank: List[int] = []
    for patch_no in range(SOUND_PATCH_COUNT):
        encoded_bank.extend(_nibble(internal_by_number[patch_no]))

    bank_messages: List[List[int]] = []
    base = _sound_patch_base_number()
    for offset in range(0, SOUND_PATCH_BANK_ENCODED_SIZE, MAX_DT1_DATA):
        chunk = encoded_bank[offset:offset + MAX_DT1_DATA]
        bank_messages.append(_build_roland_message(_device_id, COMMAND_DT1, _number_to_address(base + offset), chunk))
    return bank_messages


def _sample_sysex_path() -> Optional[Path]:
    candidates = [
        Path(__file__).resolve().with_name("U220Factory.Syx"),
        Path(__file__).resolve().parent / "testData" / "U220Factory.Syx",
        Path(__file__).resolve().parent / "testData" / "roland_u220_factory.syx",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def _load_factory_sound_patches() -> List[List[int]]:
    sample = _sample_sysex_path()
    if sample is None:
        return []
    messages = knobkraft.load_sysex(str(sample))
    return extractPatchesFromAllBankMessages(_factory_bank_messages(messages))


def _factory_bank_messages(messages: List[List[int]]) -> List[List[int]]:
    return [message for message in messages if isPartOfBankDump(message)]


def _load_factory_edit_buffer() -> Optional[List[int]]:
    sample = _sample_sysex_path()
    if sample is None:
        return None
    messages = knobkraft.load_sysex(str(sample))
    edit_messages = [message for message in messages if isPartOfEditBufferDump(message)]
    if len(edit_messages) >= 2:
        edit_buffer: List[int] = []
        for message in edit_messages[:2]:
            edit_buffer.extend(message)
        if isEditBufferDump(edit_buffer):
            return edit_buffer
    return None


def make_test_data():
    from testing.mock_midi import BankDumpMockDevice

    sample = _sample_sysex_path()
    messages = knobkraft.load_sysex(str(sample)) if sample is not None else []
    bank_messages = _factory_bank_messages(messages)
    patches = _load_factory_sound_patches()
    edit_buffer = _load_factory_edit_buffer()

    def programs(_: testing.TestData) -> List[testing.ProgramTestData]:
        if not patches:
            return []
        return [
            testing.ProgramTestData(message=patches[0], name="Acoust Piano", number=0, friendly_number="I-11", rename_name="New Patch"),
            testing.ProgramTestData(message=patches[1], name="Chorus Piano", number=1, friendly_number="I-12", rename_name="New Patch"),
            testing.ProgramTestData(message=patches[63], name="Catastrophe ", number=63, friendly_number="I-88", rename_name="New Patch"),
        ]

    def edit_buffers(_: testing.TestData) -> List[testing.ProgramTestData]:
        if edit_buffer is None:
            return []
        return [testing.ProgramTestData(message=edit_buffer, name="Future Pad  ", number=0, rename_name="New Patch")]

    return testing.TestData(
        sysex=str(sample) if sample is not None else None,
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        program_dump_request=(0, 0, createProgramDumpRequest(0, 0)),
        device_detect_call=createDeviceDetectMessage(0),
        expected_patch_count=SOUND_PATCH_COUNT + 1,
        mock_device_factory=lambda _test_data, adaptation: BankDumpMockDevice(adaptation, bank_messages),
        expected_wire_patch_count=SOUND_PATCH_COUNT,
        expected_sent_messages=lambda _test_data, adaptation: [adaptation.createBankDumpRequest(0, 0)],
        wire_download_banks=[0, 0],
        expected_multi_bank_patch_count=SOUND_PATCH_COUNT * 2,
        expected_multi_bank_sent_messages=lambda _test_data, adaptation: [
            adaptation.createBankDumpRequest(0, 0),
            adaptation.createBankDumpRequest(0, 0),
        ],
    )
