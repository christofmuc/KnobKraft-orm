#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib
import sys
from typing import Dict, List, Optional, Tuple

import knobkraft
import testing
from roland import DataBlock, RolandData, GenericRoland, command_dt1


this_module = sys.modules[__name__]


_PATCH_BLOCKS = [
    DataBlock((0x00, 0x00, 0x00), 0x38, "User Patch common A"),
    DataBlock((0x00, 0x00, 0x40), 0x20, "User Patch common B"),
    DataBlock((0x01, 0x00, 0x00), 0x7B, "User Patch part 1 A"),
    DataBlock((0x02, 0x00, 0x00), 0x36, "User Patch part 1 B"),
    DataBlock((0x03, 0x00, 0x00), 0x7B, "User Patch part 2 A"),
    DataBlock((0x04, 0x00, 0x00), 0x36, "User Patch part 2 B"),
]

_EDIT_BUFFER_PLACEHOLDER = RolandData(
    "SC-88 Pro/ST Pro User Patch placeholder",
    1,
    3,
    3,
    (0x23, 0x00, 0x00),
    _PATCH_BLOCKS,
)

_USER_PATCHES = RolandData(
    "SC-88 Pro/ST Pro User Patches",
    16,
    3,
    3,
    (0x23, 0x00, 0x00),
    _PATCH_BLOCKS,
)


class SC88ProFamilyAdaptation(GenericRoland):
    _identity_family = [0x42, 0x00]
    _name_message_number = 0
    _name_offset = 8
    _name_length = 16

    def __init__(self):
        super().__init__(
            "Roland SC-88ST Pro",
            model_id=[0x42],
            address_size=3,
            edit_buffer=_EDIT_BUFFER_PLACEHOLDER,
            program_dump=_USER_PATCHES,
            patch_name_message_number=self._name_message_number,
            patch_name_length=self._name_length,
        )
        self.program_dump.make_black_out_zones(
            self._model_id_len,
            program_position=5 + self._model_id_len,
            device_id_position=2,
            name_blankout=(self._name_message_number, self._name_offset, self._name_length),
        )
        self.edit_buffer.make_black_out_zones(
            self._model_id_len,
            device_id_position=2,
            name_blankout=(self._name_message_number, self._name_offset, self._name_length),
        )

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        return [0xF0, 0x7E, channel & 0x1F, 0x06, 0x01, 0xF7]

    def channelIfValidDeviceResponse(self, message: List[int]) -> int:
        if (
            len(message) >= 13
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

    def _matching_block(
        self, message: List[int], layout: RolandData
    ) -> Tuple[Optional[int], Optional[Tuple[int, int, int]], Optional[int]]:
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

    def _name_bytes(self, message: List[int]) -> List[int]:
        messages = knobkraft.sysex.findSysexDelimiters(message, self._name_message_number + 1)
        _, _, data = self.parseRolandMessage(message[messages[self._name_message_number][0]:messages[self._name_message_number][1]])
        return data[self._name_offset:self._name_offset + self._name_length]

    def nameFromDump(self, message) -> str:
        if self.isSingleProgramDump(message):
            return "".join(chr(x) for x in self._name_bytes(message))
        return "Invalid"

    def renamePatch(self, message: List[int], new_name: str) -> List[int]:
        if not self.isSingleProgramDump(message):
            raise Exception("renamePatch: only supports single program dumps")

        name = (new_name or "")[:self._name_length].ljust(self._name_length, " ")
        name_bytes = [ord(ch) & 0x7F for ch in name]

        rebuilt: List[int] = []
        msg_no = 0
        for start, end in knobkraft.sysex.findSysexDelimiters(message):
            sub = message[start:end]
            device_id_in_msg = sub[2]
            _, address, data = self.parseRolandMessage(sub)
            if msg_no == self._name_message_number:
                data = data.copy()
                data[self._name_offset:self._name_offset + self._name_length] = name_bytes
            rebuilt += self.buildRolandMessage(device_id_in_msg, command_dt1, address, data)
            msg_no += 1
        return rebuilt


sc_88st_pro = SC88ProFamilyAdaptation()


def name():
    return sc_88st_pro.name()


def createDeviceDetectMessage(channel: int) -> List[int]:
    return sc_88st_pro.createDeviceDetectMessage(channel)


def channelIfValidDeviceResponse(message: List[int]) -> int:
    return sc_88st_pro.channelIfValidDeviceResponse(message)


def needsChannelSpecificDetection() -> bool:
    return sc_88st_pro.needsChannelSpecificDetection()


def bankDescriptors() -> List[Dict]:
    return [{"bank": 0, "name": "User Patches", "size": 16, "type": "User Patch"}]


def createProgramDumpRequest(channel, patch_no):
    return sc_88st_pro.createProgramDumpRequest(channel, patch_no)


def isPartOfSingleProgramDump(message):
    return sc_88st_pro.isPartOfSingleProgramDump(message)


def isSingleProgramDump(messages):
    return sc_88st_pro.isSingleProgramDump(messages)


def convertToProgramDump(channel, message, program_number):
    return sc_88st_pro.convertToProgramDump(channel, message, program_number)


def numberFromDump(message) -> int:
    return sc_88st_pro.numberFromDump(message)


def nameFromDump(message) -> str:
    return sc_88st_pro.nameFromDump(message)


def renamePatch(message: List[int], new_name: str) -> List[int]:
    return sc_88st_pro.renamePatch(message, new_name)


def blankedOut(message):
    if sc_88st_pro.isSingleProgramDump(message):
        return sc_88st_pro._apply_blankout(message.copy(), sc_88st_pro.program_dump.blank_out_zones)
    raise Exception("Only works with single program dumps")


def calculateFingerprint(message):
    return hashlib.md5(bytearray(blankedOut(message))).hexdigest()


def friendlyBankName(bank_number: int) -> str:
    if bank_number != 0:
        raise Exception(f"Invalid bank {bank_number} for {name()}")
    return "User Patch Bank"


def friendlyProgramName(program_number: int) -> str:
    return f"U{program_number + 1:02d}"


def generalMessageDelay() -> int:
    return 40


def _program_number_from_address(address: List[int]) -> Optional[int]:
    if len(address) != 3:
        return None
    normalized = sc_88st_pro.program_dump.reset_to_base_address(address)
    if tuple(normalized) not in sc_88st_pro.program_dump.allowed_addresses:
        return None
    program_number = address[1] - sc_88st_pro.program_dump.base_address[1]
    if 0 <= program_number < sc_88st_pro.program_dump.num_items:
        return program_number
    return None


def _bank_block_index(message: List[int]) -> Optional[Tuple[int, int]]:
    if not sc_88st_pro.isOwnSysex(message):
        return None
    command, address, data = sc_88st_pro.parseRolandMessage(message)
    if command != command_dt1 or len(address) != 3:
        return None
    if address[0] != 0x2B or address[2] != 0x00:
        return None
    packet_number = address[1]
    total_packets = sc_88st_pro.program_dump.num_items * len(sc_88st_pro.program_dump.data_blocks)
    if not (0 <= packet_number < total_packets):
        return None
    patch_no = packet_number // len(sc_88st_pro.program_dump.data_blocks)
    sub_request = packet_number % len(sc_88st_pro.program_dump.data_blocks)
    expected_size = sc_88st_pro.program_dump.data_blocks[sub_request].size
    if len(data) != expected_size:
        return None
    return patch_no, sub_request


def createBankDumpRequest(channel, bank):
    if bank != 0:
        raise Exception(f"{name()} only exposes one user patch bank")
    return sc_88st_pro.buildRolandMessage(sc_88st_pro.device_id, 0x11, [0x0C, 0x00, 0x00], [0x00, 0x04, 0x00])


def isPartOfBankDump(message):
    if len(message) > 0 and isinstance(message[0], list):
        return any(isPartOfBankDump(part) for part in message)
    return _bank_block_index(message) is not None


def isBankDumpFinished(messages):
    found = set()
    for message in messages:
        block = _bank_block_index(message)
        if block is not None:
            found.add(block)
    return len(found) == sc_88st_pro.program_dump.num_items * len(sc_88st_pro.program_dump.data_blocks)


def extractPatchesFromAllBankMessages(messages: List[List[int]]) -> List[List[int]]:
    grouped: Dict[int, Dict[int, List[int]]] = {}
    for message in messages:
        block = _bank_block_index(message)
        if block is None:
            continue
        patch_no, sub_request = block
        grouped.setdefault(patch_no, {})[sub_request] = message

    patches = []
    for patch_no in range(sc_88st_pro.program_dump.num_items):
        blocks = grouped.get(patch_no, {})
        if len(blocks) != len(sc_88st_pro.program_dump.data_blocks):
            continue
        patch = []
        for sub_request in range(len(sc_88st_pro.program_dump.data_blocks)):
            _, _, data = sc_88st_pro.parseRolandMessage(blocks[sub_request])
            address, _ = sc_88st_pro.program_dump.address_and_size_for_sub_request(sub_request, patch_no)
            patch += sc_88st_pro.buildRolandMessage(sc_88st_pro.device_id, command_dt1, address, data)
        if sc_88st_pro.isSingleProgramDump(patch):
            patches.append(patch)
    return patches


def setupHelp():
    return (
        "This adaptation targets the documented SC-88 Pro/ST Pro User Patch format. "
        "The SC-88ST Pro relies on external SysEx editing, so KnobKraft currently supports "
        "named User Patch dumps and the full 16-patch User Patch bank."
    )


def _identity_reply(device_id: int = 0x10) -> List[int]:
    return [0xF0, 0x7E, device_id & 0x1F, 0x06, 0x02, 0x41, 0x42, 0x00, 0x00, 0x00, 0x01, 0x00, 0xF7]


def _build_patch(program_number: int, name_text: str) -> List[int]:
    name_bytes = [ord(ch) & 0x7F for ch in name_text[:16].ljust(16, " ")]
    block_payloads = [
        [((program_number * 7) + index) & 0x7F for index in range(0x38)],
        [((program_number * 11) + index) & 0x7F for index in range(0x20)],
        [((program_number * 13) + index) & 0x7F for index in range(0x7B)],
        [((program_number * 17) + index) & 0x7F for index in range(0x36)],
        [((program_number * 19) + index) & 0x7F for index in range(0x7B)],
        [((program_number * 23) + index) & 0x7F for index in range(0x36)],
    ]
    block_payloads[0][8:8 + len(name_bytes)] = name_bytes

    result: List[int] = []
    for block_no, block_data in enumerate(block_payloads):
        address, _ = sc_88st_pro.program_dump.address_and_size_for_sub_request(block_no, program_number)
        result += sc_88st_pro.buildRolandMessage(sc_88st_pro.device_id, command_dt1, address, block_data)
    return result


def _synthetic_programs() -> List[testing.ProgramTestData]:
    programs = []
    for program_number in range(sc_88st_pro.program_dump.num_items):
        patch_name = f"USERPATCH{program_number + 1:02d}".ljust(16, " ")
        program = _build_patch(program_number, patch_name)
        programs.append(
            testing.ProgramTestData(
                message=program,
                name=patch_name,
                number=program_number,
                friendly_number=f"U{program_number + 1:02d}",
                rename_name=f"RENAMED{program_number + 1:02d}".ljust(16, " "),
            )
        )
    return programs


def _synthetic_bank_messages() -> List[List[int]]:
    messages: List[List[int]] = []
    for program_number, program in enumerate(_synthetic_programs()):
        for block_number, program_message in enumerate(knobkraft.splitSysex(program.message.byte_list)):
            _, _, data = sc_88st_pro.parseRolandMessage(program_message)
            packet_number = program_number * len(sc_88st_pro.program_dump.data_blocks) + block_number
            messages.append(
                sc_88st_pro.buildRolandMessage(
                    sc_88st_pro.device_id,
                    command_dt1,
                    [0x2B, packet_number, 0x00],
                    data,
                )
            )
    return messages


def make_test_data():
    from testing.mock_midi import BankDumpMockDevice

    programs = _synthetic_programs()
    bank_messages = _synthetic_bank_messages()

    return testing.TestData(
        program_generator=lambda _test_data: programs,
        bank_generator=lambda _test_data: [bank_messages],
        program_dump_request=(0, 11, createProgramDumpRequest(0, 11)),
        device_detect_call=createDeviceDetectMessage(0),
        device_detect_reply=(_identity_reply(), 0),
        friendly_bank_name=(0, "User Patch Bank"),
        expected_patch_count=len(programs),
        mock_device_factory=lambda _test_data, adaptation: BankDumpMockDevice(adaptation, bank_messages),
        expected_wire_patch_count=len(programs),
        expected_sent_messages=lambda _test_data, adaptation: [adaptation.createBankDumpRequest(0, 0)],
    )
