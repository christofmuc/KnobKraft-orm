#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib
import sys
from pathlib import Path
from typing import List, Optional, Tuple

import knobkraft
import testing
from roland import DataBlock, RolandData, GenericRoland, command_dt1, command_rq1


this_module = sys.modules[__name__]


# Canonical JD-800 program dumps observed in Internal_Demo_Patches.syx are 384 bytes,
# transported as a 256-byte DT1 block followed by a 128-byte DT1 block.
_jd800_patch_blocks = [
    DataBlock((0x00, 0x00, 0x00), 0x100, "Patch block A"),
    DataBlock((0x00, 0x02, 0x00), 0x80, "Patch block B"),
]

_jd800_edit_buffer = RolandData(
    "JD-800 temporary patch",
    1,
    3,
    3,
    (0x00, 0x00, 0x00),
    _jd800_patch_blocks,
    uses_consecutive_addresses=True,
)

_jd800_program_dump = RolandData(
    "JD-800 internal patch memory",
    64,
    3,
    3,
    (0x05, 0x00, 0x00),
    _jd800_patch_blocks,
    uses_consecutive_addresses=True,
)


class JD800Adaptation(GenericRoland):
    def _matching_block(self, message: List[int], layout: RolandData) -> Tuple[Optional[int], Optional[Tuple[int, int, int]], Optional[int]]:
        if not self.isOwnSysex(message):
            return None, None, None
        command, address, data = self.parseRolandMessage(message)
        if command != command_dt1:
            return None, None, None
        normalized = tuple(layout.reset_to_base_address(address))
        for sub_request, block in enumerate(layout.data_blocks):
            if normalized == layout.absolute_address(block.address) and len(data) == block.size:
                return sub_request, normalized, self._patch_number_from_address(address)
        return None, None, None

    def isPartOfEditBufferDump(self, message):
        sub_request, _, _ = self._matching_block(message, self.edit_buffer)
        if sub_request is not None:
            return True, self._createFollowUpEditBufferDumpRequest(sub_request)
        return False

    def isEditBufferDump(self, messages):
        addresses = set()
        for start, end in knobkraft.sysex.findSysexDelimiters(messages):
            sub_request, normalized, _ = self._matching_block(messages[start:end], self.edit_buffer)
            if sub_request is not None:
                addresses.add(normalized)
        return all(address in addresses for address in self.edit_buffer.allowed_addresses)

    def isPartOfSingleProgramDump(self, message):
        sub_request, _, patch_no = self._matching_block(message, self.program_dump)
        if sub_request is not None:
            return True, self._createFollowUpProgramDumpRequest(patch_no, sub_request)
        return False

    def isSingleProgramDump(self, messages):
        addresses = set()
        programs = set()
        for start, end in knobkraft.sysex.findSysexDelimiters(messages):
            sub_request, normalized, patch_no = self._matching_block(messages[start:end], self.program_dump)
            if sub_request is not None:
                addresses.add(normalized)
                programs.add(patch_no)
        return len(programs) == 1 and all(address in addresses for address in self.program_dump.allowed_addresses)


jd_800 = JD800Adaptation(
    "Roland JD-800",
    model_id=[0x3D],
    address_size=3,
    edit_buffer=_jd800_edit_buffer,
    program_dump=_jd800_program_dump,
    device_detect_message=_jd800_edit_buffer,
    patch_name_length=16,
    uses_consecutive_addresses=True,
)

# GenericRoland assumes modern Roland naming and a one-byte program slot.
# The JD-800 uses 16-character names and a two-byte consecutive program address.
jd_800.edit_buffer.make_black_out_zones(
    jd_800._model_id_len,
    device_id_position=2,
    name_blankout=(0, 0, 16),
)

jd_800.program_dump.make_black_out_zones(
    jd_800._model_id_len,
    program_position=(4 + jd_800._model_id_len, 2),
    device_id_position=2,
    name_blankout=(0, 0, 16),
)

jd_800.install(this_module)


_BANK_PAGE_SIZE = 0x100
_BANK_PAGE_COUNT = (_jd800_program_dump.num_items * _jd800_program_dump.size) // _BANK_PAGE_SIZE
_BANK_PAGE_RANGE_END = DataBlock.size_to_number(tuple(_jd800_program_dump.base_address)) + _BANK_PAGE_COUNT * _BANK_PAGE_SIZE


def _testdata_path(filename: str) -> str:
    return str(Path(__file__).resolve().parent / "testData" / "Roland_JD800" / filename)


def _raw_bank_path(filename: str) -> List[List[int]]:
    return knobkraft.load_sysex(_testdata_path(filename))


def _address_as_number(address: List[int]) -> int:
    return DataBlock.size_to_number(tuple(address))


def _is_jd800_dt1_message(message: List[int]) -> bool:
    if not jd_800.isOwnSysex(message):
        return False
    command, _, _ = jd_800.parseRolandMessage(message)
    return command == command_dt1


def _is_bank_page_message(message: List[int]) -> bool:
    if not _is_jd800_dt1_message(message):
        return False
    _, address, data = jd_800.parseRolandMessage(message)
    address_number = _address_as_number(address)
    return len(data) == _BANK_PAGE_SIZE and _address_as_number(list(_jd800_program_dump.base_address)) <= address_number < _BANK_PAGE_RANGE_END


def _sorted_bank_pages(messages: List[List[int]]) -> List[Tuple[int, List[int]]]:
    pages = {}
    for message in messages:
        if _is_bank_page_message(message):
            _, address, data = jd_800.parseRolandMessage(message)
            pages[_address_as_number(address)] = data
    return sorted(pages.items())


def _bank_payload(messages: List[List[int]]) -> List[int]:
    pages = _sorted_bank_pages(messages)
    return [item for _, data in pages for item in data]


def _program_dump_from_payload(payload: List[int], program_no: int) -> List[int]:
    result = []
    offset = 0
    for block_no, block in enumerate(_jd800_patch_blocks):
        address, _ = _jd800_program_dump.address_and_size_for_sub_request(block_no, program_no)
        block_data = payload[offset:offset + block.size]
        result += jd_800.buildRolandMessage(jd_800.device_id, command_dt1, address, block_data)
        offset += block.size
    return result


def _programs_from_bank_messages(messages: List[List[int]]) -> List[testing.ProgramTestData]:
    payload = _bank_payload(messages)
    patch_size = _jd800_program_dump.size
    programs = []
    if len(payload) % patch_size != 0:
        raise Exception(f"Unexpected JD-800 bank payload size {len(payload)}, not divisible by patch size {patch_size}")
    for program_no in range(len(payload) // patch_size):
        patch_payload = payload[program_no * patch_size:(program_no + 1) * patch_size]
        program_dump = _program_dump_from_payload(patch_payload, program_no)
        programs.append(
            testing.ProgramTestData(
                message=program_dump,
                name=jd_800.nameFromDump(program_dump),
                number=jd_800.numberFromDump(program_dump),
                rename_name="Renamed JD-800 ",
            )
        )
    return programs



def isPartOfEditBufferDump(message):
    return jd_800.isPartOfEditBufferDump(message)


def isEditBufferDump(messages):
    return jd_800.isEditBufferDump(messages)


def isPartOfSingleProgramDump(message):
    return jd_800.isPartOfSingleProgramDump(message)


def isSingleProgramDump(messages):
    return jd_800.isSingleProgramDump(messages)
def blankedOut(message):
    # Program dumps also match isEditBufferDump() under consecutive addressing,
    # so fingerprint them with the program-dump blankout table first.
    if jd_800.isSingleProgramDump(message):
        return jd_800._apply_blankout(message.copy(), jd_800.program_dump.blank_out_zones)
    if jd_800.isEditBufferDump(message):
        return jd_800._apply_blankout(message.copy(), jd_800.edit_buffer.blank_out_zones)
    raise Exception("Only works with edit buffers and program dumps")


def calculateFingerprint(message):
    return hashlib.md5(bytearray(blankedOut(message))).hexdigest()


def isPartOfBankDump(message):
    return _is_bank_page_message(message)


def isBankDumpFinished(messages):
    pages = _sorted_bank_pages(messages)
    if len(pages) != _BANK_PAGE_COUNT:
        return False
    first_address = _address_as_number(list(_jd800_program_dump.base_address))
    return all(address == first_address + index * _BANK_PAGE_SIZE for index, (address, _) in enumerate(pages))


def extractPatchesFromAllBankMessages(messages):
    return [program.message.byte_list for program in _programs_from_bank_messages(messages)]


def createBankDumpRequest(channel, bank):
    return jd_800.buildRolandMessage(jd_800.device_id, command_rq1, [0x05, 0x00, 0x00], [0x01, 0x40, 0x00])


def setupHelp():
    return (
        "Set JD-800 MIDI Unit Number to 17 and enable Rx Exclusive ON-1, "
        "or use Rx Exclusive ON-2 to ignore the Unit Number. "
        "Canonical single programs are 384 bytes as 256+128 DT1 blocks. "
        "Large 25536-byte .syx files are treated as full 64-patch memory dumps chunked into 256-byte pages."
    )


def _responses_for_edit_buffer(adaptation, edit_buffer):
    responses = {}
    request = adaptation.createEditBufferRequest(0)
    for message in knobkraft.splitSysex(edit_buffer):
        responses[tuple(request)] = [message]
        handshake = adaptation.isPartOfEditBufferDump(message)
        request = handshake[1] if isinstance(handshake, tuple) else []
    return responses


def make_test_data():
    from testing.mock_midi import BankDumpMockDevice, ScriptedMockDevice

    bank_messages = _raw_bank_path("JDBOOK.SYX")

    def programs(_test_data: testing.TestData) -> List[testing.ProgramTestData]:
        return _programs_from_bank_messages(bank_messages)

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        source_program = test_data.programs[0]
        edit_buffer = jd_800.convertToEditBuffer(0, source_program.message.byte_list)
        return [
            testing.ProgramTestData(
                message=edit_buffer,
                name=jd_800.nameFromDump(edit_buffer),
                rename_name="Edit JD-800    ",
            )
        ]

    def banks(_test_data: testing.TestData) -> List[List[List[int]]]:
        return [bank_messages]

    def single_edit_buffer_mock_device(test_data: testing.TestData, adaptation):
        first_edit_buffer = test_data.edit_buffers[0].message.byte_list
        return ScriptedMockDevice(_responses_for_edit_buffer(adaptation, first_edit_buffer))

    def bank_mock_device(_test_data: testing.TestData, adaptation):
        return BankDumpMockDevice(adaptation, bank_messages)

    return testing.TestData(
        sysex=_testdata_path("JDBOOK.SYX"),
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,
        expected_patch_count=64,
        program_dump_request=(0, 11, "F0 41 10 3D 11 05 21 00 00 02 00 58 F7"),
        mock_device_factory=bank_mock_device,
        expected_wire_patch_count=64,
        expected_sent_messages=lambda _test_data, adaptation: [adaptation.createBankDumpRequest(0, 0)],
        single_edit_buffer_mock_device_factory=single_edit_buffer_mock_device,
        send_to_synth_patch=lambda test_data: test_data.edit_buffers[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: knobkraft.splitSysex(
            adaptation.convertToEditBuffer(0, test_data.edit_buffers[0].message.byte_list)
        ),
    )


