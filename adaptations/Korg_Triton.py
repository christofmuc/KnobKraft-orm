#   Korg Triton Classic - Program mode adaptation
import hashlib
from typing import List, Optional, Tuple

import knobkraft
import testing


KORG = 0x42
TRITON_SERIES = 0x50
TRITON_CLASSIC = 0x05  # Only used in identity replies.

# Request functions
CURRENT_PROGRAM_DUMP_REQUEST = 0x10
PROGRAM_PARAMETER_DUMP_REQUEST = 0x1C

# Response functions
CURRENT_PROGRAM_DUMP = 0x40
PROGRAM_PARAMETER_DUMP = 0x4C
ALL_DATA_DUMP = 0x50

PROGRAM_DATA_SIZE = 540
PROGRAM_NAME_LENGTH = 16
PATCHES_PER_BANK = 128
INTERNAL_BANKS = 5
MIN_ALL_DATA_PROGRAM_RUN = 64


def name():
    return "Korg Triton Classic"


def createDeviceDetectMessage(channel):
    return [0xF0, 0x7E, channel & 0x0F, 0x06, 0x01, 0xF7]


def deviceDetectWaitMilliseconds():
    return 300


def generalMessageDelay():
    return 1000


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    if (
        len(message) >= 15
        and message[0] == 0xF0
        and message[1] == 0x7E
        and message[3] == 0x06
        and message[4] == 0x02
        and message[5] == KORG
        and message[6] == TRITON_SERIES
        and message[7] == 0x00
        and message[8] == TRITON_CLASSIC
        and message[9] == 0x00
        and message[-1] == 0xF7
    ):
        return message[2]
    return -1


def createEditBufferRequest(channel):
    return [0xF0, KORG, 0x30 | (channel & 0x0F), TRITON_SERIES, CURRENT_PROGRAM_DUMP_REQUEST, 0x00, 0xF7]


def createProgramDumpRequest(channel, patchNo):
    bank, program = divmod(patchNo, PATCHES_PER_BANK)
    return [
        0xF0,
        KORG,
        0x30 | (channel & 0x0F),
        TRITON_SERIES,
        PROGRAM_PARAMETER_DUMP_REQUEST,
        _selector_byte(0x02, bank),
        program & 0x7F,
        0x00,
        0xF7,
    ]


def isEditBufferDump(message):
    return _has_triton_header(message, CURRENT_PROGRAM_DUMP) and len(message) > 6


def isSingleProgramDump(message):
    return (
        _has_triton_header(message, PROGRAM_PARAMETER_DUMP)
        and len(message) > 8
        and _dump_kind(message) == 0x02
    )


def numberOfBanks():
    return INTERNAL_BANKS


def numberOfPatchesPerBank():
    return PATCHES_PER_BANK


def friendlyBankName(bank):
    if 0 <= bank < INTERNAL_BANKS:
        return chr(ord("A") + bank)
    return f"Bank {bank}"


def friendlyProgramName(program):
    if program < 0:
        return "(Edit Buffer)"
    bank, patch = divmod(program, PATCHES_PER_BANK)
    return f"{friendlyBankName(bank)}{patch:03d}"


def nameFromDump(message):
    data = _program_data_from_dump(message)
    return "".join(chr(x) if 32 <= x <= 126 else " " for x in data[:PROGRAM_NAME_LENGTH]).rstrip()


def renamePatch(message, new_name):
    data = _program_data_from_dump(message)
    renamed = data[:]
    renamed[:PROGRAM_NAME_LENGTH] = _encode_name(new_name)
    return _replace_program_data(message, renamed)


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message[:2] + [0x30 | (channel & 0x0F)] + message[3:]
    if isSingleProgramDump(message):
        return [0xF0, KORG, 0x30 | (channel & 0x0F), TRITON_SERIES, CURRENT_PROGRAM_DUMP, _program_type(message)] + _payload_from_dump(message) + [0xF7]
    raise Exception("Neither edit buffer nor program dump. Can't convert to edit buffer.")


def convertToProgramDump(channel, message, program_number):
    bank, patch = divmod(program_number, PATCHES_PER_BANK)
    payload = _payload_from_dump(message)
    available_banks = _available_banks_byte_from_message(message)
    return _build_single_program_dump(
        channel,
        available_banks,
        bank,
        patch,
        unescapeSysex(payload),
    )


def numberFromDump(message):
    if isSingleProgramDump(message):
        return _dump_bank(message) * PATCHES_PER_BANK + (message[7] & 0x7F)
    return -1


def blankedOut(message):
    data = _program_data_from_dump(message)
    blanked = data[:]
    blanked[:PROGRAM_NAME_LENGTH] = [0x20] * PROGRAM_NAME_LENGTH
    if isSingleProgramDump(message):
        return _build_single_program_dump(0, 0, 0, 0, blanked)
    if isEditBufferDump(message):
        return [0xF0, KORG, 0x30, TRITON_SERIES, CURRENT_PROGRAM_DUMP, _program_type(message)] + escapeSysex(blanked) + [0xF7]
    raise Exception("Neither edit buffer nor program dump. Can't blank out.")


def calculateFingerprint(message):
    return hashlib.md5(bytearray(blankedOut(message))).hexdigest()


def bankDescriptors():
    return [
        {"bank": 0x00, "name": "INT-A", "size": PATCHES_PER_BANK, "type": "Patch"},
        {"bank": 0x01, "name": "INT-B", "size": PATCHES_PER_BANK, "type": "Patch"},
        {"bank": 0x02, "name": "INT-C", "size": PATCHES_PER_BANK, "type": "Patch"},
        {"bank": 0x03, "name": "INT-D", "size": PATCHES_PER_BANK, "type": "Patch"},
        {"bank": 0x04, "name": "INT-E", "size": PATCHES_PER_BANK, "type": "Patch"},
    ]


def createBankDumpRequest(channel, bank):
    return [
        0xF0,
        KORG,
        0x30 | (channel & 0x0F),
        TRITON_SERIES,
        PROGRAM_PARAMETER_DUMP_REQUEST,
        _selector_byte(0x01, bank),
        0x00,
        0x00,
        0xF7,
    ]


def isPartOfBankDump(message):
    if _has_triton_header(message, PROGRAM_PARAMETER_DUMP) and len(message) > 8:
        return _dump_kind(message) in (0x00, 0x01)
    return _has_triton_header(message, ALL_DATA_DUMP) and len(message) > 11


def isBankDumpFinished(messages):
    return any(isPartOfBankDump(message) and message[-1] == 0xF7 for message in messages)


def extractPatchesFromBank(message):
    if not (_has_triton_header(message, PROGRAM_PARAMETER_DUMP) and _dump_kind(message) in (0x00, 0x01)):
        raise Exception("Not a Triton program bank dump")

    channel = message[2] & 0x0F
    available_banks = message[5] & 0x7F
    base_bank = _dump_bank(message)
    raw_programs = _split_unescaped_programs(unescapeSysex(message[8:-1]))

    result = []
    for index, program_data in enumerate(raw_programs):
        bank = base_bank + index // PATCHES_PER_BANK
        patch = index % PATCHES_PER_BANK
        result.extend(_build_single_program_dump(channel, available_banks, bank, patch, program_data))
    return result


def extractPatchesFromAllBankMessages(messages):
    patches = []
    for message in messages:
        if _has_triton_header(message, ALL_DATA_DUMP):
            patches.extend(_extract_patches_from_all_data_dump(message))
        elif isPartOfBankDump(message):
            patches.extend(knobkraft.splitSysex(extractPatchesFromBank(message)))
    return patches


def convertPatchesToBankDump(patches):
    if len(patches) != PATCHES_PER_BANK:
        raise ValueError(f"A Triton bank dump requires exactly {PATCHES_PER_BANK} patches")

    channel = _message_channel(patches[0])
    available_banks = _available_banks_byte_from_message(patches[0])
    first_number = numberFromDump(patches[0]) if isSingleProgramDump(patches[0]) else 0
    bank = first_number // PATCHES_PER_BANK

    unescaped_bank_data = []
    for patch in patches:
        patch_number = numberFromDump(patch) if isSingleProgramDump(patch) else -1
        if patch_number >= 0 and patch_number // PATCHES_PER_BANK != bank:
            raise ValueError("All patches must belong to the same bank")
        unescaped_bank_data.extend(_program_data_from_dump(patch))

    packed = escapeSysex(unescaped_bank_data)
    return [[
        0xF0,
        KORG,
        0x30 | channel,
        TRITON_SERIES,
        PROGRAM_PARAMETER_DUMP,
        available_banks,
        _selector_byte(0x01, bank),
        0x00,
    ] + packed + [0xF7]]


def unescapeSysex(sysex: List[int]) -> List[int]:
    result = []
    index = 0
    while index < len(sysex):
        msb = sysex[index]
        index += 1
        for bit in range(7):
            if index < len(sysex):
                result.append(sysex[index] | (((msb >> bit) & 1) << 7))
                index += 1
    return result


def escapeSysex(data: List[int]) -> List[int]:
    result = []
    index = 0
    while index < len(data):
        msb = 0
        chunk = data[index:index + 7]
        for bit, value in enumerate(chunk):
            if value & 0x80:
                msb |= 1 << bit
        result.append(msb)
        for value in chunk:
            result.append(value & 0x7F)
        index += 7
    return result


def make_test_data():
    from testing.mock_midi import BankDumpMockDevice

    single_program = knobkraft.load_sysex("testData/Korg_Triton/bank1-patch1-korgtriton-noisystabber.syx", as_single_list=True)
    bank_messages = knobkraft.load_sysex("testData/Korg_Triton/bank1-korgtriton-midiox2.syx")
    extracted_bank_programs = knobkraft.splitSysex(extractPatchesFromBank(bank_messages[0]))
    edit_buffer = convertToEditBuffer(0, single_program)

    def programs(_: testing.TestData) -> List[testing.ProgramTestData]:
        return [
            testing.ProgramTestData(message=single_program, name="Noisy Stabber", number=0, friendly_number="A000", rename_name="New Stabber"),
            testing.ProgramTestData(message=extracted_bank_programs[1], name=nameFromDump(extracted_bank_programs[1]), number=1, friendly_number="A001", rename_name="Acoustic New"),
            testing.ProgramTestData(message=extracted_bank_programs[127], name="Krystal Bells", number=127, friendly_number="A127", rename_name="Crystal New"),
        ]

    def edit_buffers(_: testing.TestData) -> List[testing.ProgramTestData]:
        return [testing.ProgramTestData(message=edit_buffer, name="Noisy Stabber", rename_name="Edit Buffer New")]

    return testing.TestData(
        sysex="testData/Korg_Triton/bank1-korgtriton-midiox2.syx",
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        program_dump_request=(0, 0, createProgramDumpRequest(0, 0)),
        device_detect_call=createDeviceDetectMessage(0),
        device_detect_reply=("F0 7E 00 06 02 42 50 00 05 00 00 00 00 00 F7", 0),
        friendly_bank_name=(1, "B"),
        expected_patch_count=PATCHES_PER_BANK,
        mock_device_factory=lambda _test_data, adaptation: BankDumpMockDevice(adaptation, bank_messages, bank=0),
        expected_wire_patch_count=PATCHES_PER_BANK,
        expected_sent_messages=lambda _test_data, adaptation: [adaptation.createBankDumpRequest(0, 0)],
        wire_download_banks=[0, 0],
        expected_multi_bank_patch_count=PATCHES_PER_BANK * 2,
        expected_multi_bank_sent_messages=lambda _test_data, adaptation: [
            adaptation.createBankDumpRequest(0, 0),
            adaptation.createBankDumpRequest(0, 0),
        ],
        send_to_synth_patch=lambda test_data: test_data.programs[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: knobkraft.splitSysex(
            adaptation.convertToEditBuffer(0, test_data.programs[0].message.byte_list)
        ),
    )


def _has_triton_header(message, command: Optional[int] = None):
    if len(message) < 6 or message[0] != 0xF0 or message[1] != KORG or message[-1] != 0xF7:
        return False
    if (message[2] & 0xF0) != 0x30 or message[3] != TRITON_SERIES:
        return False
    if command is not None and message[4] != command:
        return False
    return True


def _selector_byte(kind: int, bank: int):
    return ((kind & 0x03) << 4) | (bank & 0x07)


def _dump_kind(message):
    return (message[6] >> 4) & 0x03


def _dump_bank(message):
    return message[6] & 0x07


def _message_channel(message):
    return message[2] & 0x0F


def _program_type(message):
    if isEditBufferDump(message):
        return message[5] & 0x7F
    if isSingleProgramDump(message):
        return message[5] & 0x01
    return 0x00


def _available_banks_byte():
    return 0x00


def _available_banks_byte_from_message(message):
    if isSingleProgramDump(message):
        return message[5] & 0x7F
    return _available_banks_byte()


def _payload_from_dump(message):
    if isEditBufferDump(message):
        return message[6:-1]
    if isSingleProgramDump(message):
        return message[8:-1]
    raise Exception("Neither edit buffer nor program dump.")


def _program_data_from_dump(message):
    data = unescapeSysex(_payload_from_dump(message))
    if len(data) != PROGRAM_DATA_SIZE:
        raise Exception(f"Expected {PROGRAM_DATA_SIZE} bytes of Triton program data, got {len(data)}")
    return data


def _replace_program_data(message, data):
    packed = escapeSysex(data)
    if isEditBufferDump(message):
        return message[:6] + packed + [0xF7]
    if isSingleProgramDump(message):
        return message[:8] + packed + [0xF7]
    raise Exception("Neither edit buffer nor program dump. Can't replace program data.")


def _encode_name(name_to_encode):
    return [ord(char) if 32 <= ord(char) <= 126 else 0x20 for char in name_to_encode[:PROGRAM_NAME_LENGTH].ljust(PROGRAM_NAME_LENGTH)]


def _split_unescaped_programs(unescaped_data: List[int]):
    if len(unescaped_data) < PROGRAM_DATA_SIZE:
        raise Exception("Program dump did not contain enough data for a single program")
    if len(unescaped_data) % PROGRAM_DATA_SIZE != 0:
        raise Exception(f"Program dump payload is not divisible by {PROGRAM_DATA_SIZE} bytes")
    return [
        unescaped_data[index:index + PROGRAM_DATA_SIZE]
        for index in range(0, len(unescaped_data), PROGRAM_DATA_SIZE)
    ]


def _build_single_program_dump(channel: int, available_banks: int, bank: int, patch: int, program_data: List[int]):
    return [
        0xF0,
        KORG,
        0x30 | (channel & 0x0F),
        TRITON_SERIES,
        PROGRAM_PARAMETER_DUMP,
        available_banks & 0x7F,
        _selector_byte(0x02, bank),
        patch & 0x7F,
    ] + escapeSysex(program_data) + [0xF7]


def _looks_like_program_name(data: List[int], offset: int):
    name_bytes = data[offset:offset + PROGRAM_NAME_LENGTH]
    if len(name_bytes) < PROGRAM_NAME_LENGTH:
        return False
    return all(32 <= value <= 126 for value in name_bytes) and any(value != 0x20 for value in name_bytes)


def _find_all_data_program_run(data: List[int]) -> Tuple[Optional[int], int]:
    best_start = None
    best_count = 0

    for residue in range(PROGRAM_DATA_SIZE):
        current_start = None
        current_count = 0
        for offset in range(residue, len(data) - PROGRAM_NAME_LENGTH + 1, PROGRAM_DATA_SIZE):
            if _looks_like_program_name(data, offset):
                if current_start is None:
                    current_start = offset
                current_count += 1
                if current_count > best_count:
                    best_start = current_start
                    best_count = current_count
            else:
                current_start = None
                current_count = 0

    return best_start, best_count


def _extract_patches_from_all_data_dump(message):
    if not _has_triton_header(message, ALL_DATA_DUMP):
        raise Exception("Not a Triton all-data dump")

    channel = _message_channel(message)
    data = unescapeSysex(message[11:-1])
    start, count = _find_all_data_program_run(data)
    if start is None or count < MIN_ALL_DATA_PROGRAM_RUN:
        raise Exception("Could not locate a Triton program block inside the all-data dump")

    programs = []
    for index in range(count):
        offset = start + index * PROGRAM_DATA_SIZE
        program_data = data[offset:offset + PROGRAM_DATA_SIZE]
        if len(program_data) < PROGRAM_DATA_SIZE:
            break
        bank = index // PATCHES_PER_BANK
        patch = index % PATCHES_PER_BANK
        programs.append(_build_single_program_dump(channel, _available_banks_byte(), bank, patch, program_data))
    return programs
