#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

from typing import Dict, List, Optional, Tuple

import knobkraft
import testing
from roland import DataBlock, RolandData, GenericRoland, command_dt1, command_rq1


_PATCH_BLOCKS = [
    DataBlock((0x00, 0x00, 0x00, 0x00), 0x56, "Patch common"),
    DataBlock((0x00, 0x00, 0x02, 0x00), 0x34, "Patch common chorus"),
    DataBlock((0x00, 0x00, 0x04, 0x00), 0x53, "Patch common reverb"),
    DataBlock((0x00, 0x00, 0x06, 0x00), (0x01, 0x11), "Patch common MFX"),
    DataBlock((0x00, 0x00, 0x10, 0x00), 0x29, "Patch TMT"),
    DataBlock((0x00, 0x00, 0x20, 0x00), (0x01, 0x09), "Patch tone 1"),
    DataBlock((0x00, 0x00, 0x22, 0x00), (0x01, 0x09), "Patch tone 2"),
    DataBlock((0x00, 0x00, 0x24, 0x00), (0x01, 0x09), "Patch tone 3"),
    DataBlock((0x00, 0x00, 0x26, 0x00), (0x01, 0x09), "Patch tone 4"),
]

_PART_STRIDE = (0x00, 0x20, 0x00, 0x00)

_EDIT_BUFFER = RolandData(
    "SD-90 temporary patch, multitimbre part 1",
    1,
    4,
    4,
    (0x11, 0x00, 0x00, 0x00),
    _PATCH_BLOCKS,
)

_TEMPORARY_PART_PATCHES = RolandData(
    "SD-90 temporary patches, multitimbre parts 1-32",
    32,
    4,
    4,
    (0x11, 0x00, 0x00, 0x00),
    _PATCH_BLOCKS,
)


class SD90TemporaryPatchAdaptation(GenericRoland):
    _identity_family = [0x48, 0x01]

    def __init__(self):
        super().__init__(
            "Edirol SD-90",
            model_id=[0x00, 0x48],
            address_size=4,
            edit_buffer=_EDIT_BUFFER,
            program_dump=_TEMPORARY_PART_PATCHES,
            patch_name_length=16,
        )
        self.program_dump.make_black_out_zones(
            self._model_id_len,
            program_position=(6, 2),
            device_id_position=2,
            name_blankout=(0, 0, 16),
        )
        self.edit_buffer.make_black_out_zones(
            self._model_id_len,
            device_id_position=2,
            name_blankout=(0, 0, 16),
        )
        self._program_base_value = DataBlock.size_to_number(tuple(self.program_dump.base_address))
        self._part_stride_value = DataBlock.size_to_number(_PART_STRIDE)

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        return [0xF0, 0x7E, channel & 0x1F, 0x06, 0x01, 0xF7]

    def channelIfValidDeviceResponse(self, message: List[int]) -> int:
        if (
            len(message) >= 12
            and message[0] == 0xF0
            and message[1] == 0x7E
            and message[3] == 0x06
            and message[4] == 0x02
            and message[5] == 0x41
            and message[6:8] == self._identity_family
        ):
            self.device_id = message[2]
            return message[2] & 0x0F
        return -1

    def needsChannelSpecificDetection(self) -> bool:
        return False

    def bankDescriptors(self) -> List[Dict]:
        return [{"bank": 0, "name": "Temporary Parts", "size": 32, "type": "Temporary Patch"}]

    def _part_base_address(self, part_number: int) -> List[int]:
        if not 0 <= part_number < self.program_dump.num_items:
            raise Exception(f"Invalid SD-90 temporary part {part_number}")
        return DataBlock.size_as_7bit_list(
            self._program_base_value + (part_number * self._part_stride_value),
            self.address_size,
        )

    def _address_for_sub_request(self, layout: RolandData, sub_request: int, part_number: int) -> Tuple[List[int], List[int]]:
        part_base = DataBlock.size_to_number(tuple(self._part_base_address(part_number)))
        block_offset = DataBlock.size_to_number(tuple(layout.data_blocks[sub_request].address))
        concrete_address = DataBlock.size_as_7bit_list(part_base + block_offset, self.address_size)
        block_size = DataBlock.size_as_7bit_list(layout.data_blocks[sub_request].size, layout.num_size_bytes)
        return concrete_address, block_size

    def _part_number_from_address(self, address: List[int]) -> Optional[int]:
        address_value = DataBlock.size_to_number(tuple(address))
        if address_value < self._program_base_value:
            return None
        part_number = (address_value - self._program_base_value) // self._part_stride_value
        if 0 <= part_number < self.program_dump.num_items:
            return int(part_number)
        return None

    def _normalize_address(self, address: List[int]) -> Tuple[Optional[int], Optional[Tuple[int, int, int, int]]]:
        part_number = self._part_number_from_address(address)
        if part_number is None:
            return None, None
        address_value = DataBlock.size_to_number(tuple(address))
        normalized_value = address_value - (part_number * self._part_stride_value)
        normalized = tuple(DataBlock.size_as_7bit_list(normalized_value, self.address_size))
        return part_number, normalized

    def _matching_block(
        self, message: List[int], layout: RolandData
    ) -> Tuple[Optional[int], Optional[Tuple[int, int, int, int]], Optional[int]]:
        if not self.isOwnSysex(message):
            return None, None, None
        command, address, data = self.parseRolandMessage(message)
        if command != command_dt1:
            return None, None, None
        part_number, normalized = self._normalize_address(address)
        if part_number is None or normalized is None:
            return None, None, None
        for sub_request, block in enumerate(layout.data_blocks):
            if normalized == layout.absolute_address(block.address) and len(data) == block.size:
                return sub_request, normalized, part_number
        return None, None, None

    def createProgramDumpRequest(self, channel, patch_no):
        address, size = self._address_for_sub_request(self.program_dump, 0, patch_no % self.program_dump.num_items)
        return self.buildRolandMessage(self.device_id, command_rq1, address, size)

    def _createFollowUpProgramDumpRequest(self, patch_no: int, previous_request_no: int) -> List[int]:
        if previous_request_no + 1 < len(self.program_dump.data_blocks):
            address, size = self._address_for_sub_request(
                self.program_dump,
                previous_request_no + 1,
                patch_no % self.program_dump.num_items,
            )
            return self.buildRolandMessage(self.device_id, command_rq1, address, size)
        return []

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

    def convertToProgramDump(self, channel, message, program_number):
        if not (self.isSingleProgramDump(message) or self.isEditBufferDump(message)):
            raise Exception("Can only convert SD-90 temporary patches or edit buffers to program dumps")
        result: List[int] = []
        for block_no, sub_message in enumerate(knobkraft.splitSysex(message)):
            _, _, data = self.parseRolandMessage(sub_message)
            address, _ = self._address_for_sub_request(self.program_dump, block_no, program_number % self.program_dump.num_items)
            result += self.buildRolandMessage(self.device_id, command_dt1, address, data)
        return result

    def numberFromDump(self, message) -> int:
        if not self.isSingleProgramDump(message):
            return 0
        first = knobkraft.sysex.findSysexDelimiters(message, 1)[0]
        _, address = self.getCommandAndAddressFromRolandMessage(message[first[0]:first[1]])
        part_number = self._part_number_from_address(address)
        return 0 if part_number is None else part_number

    def blankedOut(self, message):
        # Part 1 program dumps share the same address region as the chosen edit buffer, so
        # prefer program semantics when a message can be interpreted both ways.
        if self.isSingleProgramDump(message):
            return self._apply_blankout(message.copy(), self.program_dump.blank_out_zones)
        if self.isEditBufferDump(message):
            return self._apply_blankout(message.copy(), self.edit_buffer.blank_out_zones)
        raise Exception("Only works with edit buffers and program dumps")


sd_90 = SD90TemporaryPatchAdaptation()


def name():
    return sd_90.name()


def createDeviceDetectMessage(channel: int) -> List[int]:
    return sd_90.createDeviceDetectMessage(channel)


def channelIfValidDeviceResponse(message: List[int]) -> int:
    return sd_90.channelIfValidDeviceResponse(message)


def needsChannelSpecificDetection() -> bool:
    return sd_90.needsChannelSpecificDetection()


def bankDescriptors() -> List[Dict]:
    return sd_90.bankDescriptors()


def createEditBufferRequest(channel) -> List[int]:
    return sd_90.createEditBufferRequest(channel)


def isPartOfEditBufferDump(message):
    return sd_90.isPartOfEditBufferDump(message)


def isEditBufferDump(messages):
    return sd_90.isEditBufferDump(messages)


def convertToEditBuffer(channel, message):
    return sd_90.convertToEditBuffer(channel, message)


def createProgramDumpRequest(channel, patch_no):
    return sd_90.createProgramDumpRequest(channel, patch_no)


def isPartOfSingleProgramDump(message):
    return sd_90.isPartOfSingleProgramDump(message)


def isSingleProgramDump(messages):
    return sd_90.isSingleProgramDump(messages)


def convertToProgramDump(channel, message, program_number):
    return sd_90.convertToProgramDump(channel, message, program_number)


def numberFromDump(message) -> int:
    return sd_90.numberFromDump(message)


def nameFromDump(message) -> str:
    return sd_90.nameFromDump(message)


def renamePatch(message: List[int], new_name: str) -> List[int]:
    return sd_90.renamePatch(message, new_name)


def blankedOut(message):
    return sd_90.blankedOut(message)


def calculateFingerprint(message):
    return sd_90.calculateFingerprint(message)


def friendlyBankName(bank_number: int) -> str:
    if bank_number != 0:
        raise Exception(f"Invalid bank {bank_number} for {name()}")
    return "Temporary Parts"


def friendlyProgramName(program_number: int) -> str:
    return f"Part {program_number + 1:02d}"


def generalMessageDelay() -> int:
    return 40


def setupHelp():
    return (
        "This first-pass SD-90 adaptation targets the documented Temporary Patch format in Native mode. "
        "Edit-buffer download uses Multitimbre Part 1, while program requests address the 32 temporary part patches."
    )


def _identity_reply(device_id: int = 0x10) -> List[int]:
    return [0xF0, 0x7E, device_id & 0x1F, 0x06, 0x02, 0x41, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF7]


def _block_payloads(part_number: int, name_text: str) -> List[List[int]]:
    name_bytes = [ord(ch) & 0x7F for ch in name_text[:16].ljust(16, " ")]
    payloads = [
        [((part_number * 3) + index) & 0x7F for index in range(0x56)],
        [((part_number * 5) + index) & 0x7F for index in range(0x34)],
        [((part_number * 7) + index) & 0x7F for index in range(0x53)],
        [((part_number * 11) + index) & 0x7F for index in range(DataBlock.size_to_number((0x01, 0x11)))],
        [((part_number * 13) + index) & 0x7F for index in range(0x29)],
        [((part_number * 17) + index) & 0x7F for index in range(DataBlock.size_to_number((0x01, 0x09)))],
        [((part_number * 19) + index) & 0x7F for index in range(DataBlock.size_to_number((0x01, 0x09)))],
        [((part_number * 23) + index) & 0x7F for index in range(DataBlock.size_to_number((0x01, 0x09)))],
        [((part_number * 29) + index) & 0x7F for index in range(DataBlock.size_to_number((0x01, 0x09)))],
    ]
    payloads[0][0:16] = name_bytes
    return payloads


def _build_program_dump(part_number: int, name_text: str) -> List[int]:
    result: List[int] = []
    for block_no, block_data in enumerate(_block_payloads(part_number, name_text)):
        address, _ = sd_90._address_for_sub_request(sd_90.program_dump, block_no, part_number)
        result += sd_90.buildRolandMessage(sd_90.device_id, command_dt1, address, block_data)
    return result


def _build_edit_buffer(name_text: str) -> List[int]:
    result: List[int] = []
    for block_no, block_data in enumerate(_block_payloads(0, name_text)):
        address, _ = sd_90.edit_buffer.address_and_size_for_sub_request(block_no, 0)
        result += sd_90.buildRolandMessage(sd_90.device_id, command_dt1, address, block_data)
    return result


def _responses_for_edit_buffer(adaptation, edit_buffer):
    responses = {}
    request = adaptation.createEditBufferRequest(0)
    for message in knobkraft.splitSysex(edit_buffer):
        responses[tuple(request)] = [message]
        handshake = adaptation.isPartOfEditBufferDump(message)
        request = handshake[1] if isinstance(handshake, tuple) else []
    return responses


def make_test_data():
    from testing.mock_midi import ScriptedMockDevice

    program_numbers = [0, 1, 10, 31]
    programs = [
        testing.ProgramTestData(
            message=_build_program_dump(program_number, f"SD90 PART {program_number + 1:02d}"),
            name=f"SD90 PART {program_number + 1:02d}".ljust(16, " "),
            number=program_number,
            friendly_number=f"Part {program_number + 1:02d}",
            rename_name=f"RENAMED PART {program_number + 1:02d}"[:16].ljust(16, " "),
            target_no=(program_number + 3) % 32,
        )
        for program_number in program_numbers
    ]
    edit_buffer = testing.ProgramTestData(
        message=_build_edit_buffer("SD90 EDIT PART1"),
        name="SD90 EDIT PART1 ".ljust(16, " "),
        rename_name="EDIT BUFFER REN ".ljust(16, " "),
        target_no=7,
    )

    return testing.TestData(
        program_generator=lambda _test_data: programs,
        edit_buffer_generator=lambda _test_data: [edit_buffer],
        program_dump_request=(0, 11, createProgramDumpRequest(0, 11)),
        device_detect_call=createDeviceDetectMessage(0),
        device_detect_reply=(_identity_reply(), 0),
        friendly_bank_name=(0, "Temporary Parts"),
        expected_patch_count=len(programs),
        single_edit_buffer_mock_device_factory=lambda _test_data, adaptation: ScriptedMockDevice(
            _responses_for_edit_buffer(adaptation, edit_buffer.message.byte_list)
        ),
        send_to_synth_patch=lambda test_data: test_data.programs[0].message.byte_list,
        expected_send_to_synth_messages=lambda test_data, adaptation: knobkraft.splitSysex(
            adaptation.convertToEditBuffer(0, test_data.programs[0].message.byte_list)
        ),
    )
