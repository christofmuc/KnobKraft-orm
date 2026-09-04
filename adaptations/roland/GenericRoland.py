#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import copy
from typing import List, Tuple, Optional, Dict, Union
import knobkraft

roland_id = 0x41  # Roland
command_rq1 = 0x11
command_dt1 = 0x12

# Construct the Roland character set as specified in the MIDI implementation
# This is only used by very old Synths like the Roland D-50/D-550
character_set = [' '] + [chr(x) for x in range(ord('A'), ord('Z') + 1)] + \
                [chr(x) for x in range(ord('a'), ord('z') + 1)] + \
                [chr(x) for x in range(ord('1'), ord('9') + 1)] + ['0', '-']

# Roland categories (first defined for XV-2080)
categories = {
    0: ("-", "NO ASSIGN", "No assign"),
    1: ("PNO", "AC. PIANO", "Acoustic Piano"),
    2: ("EP", "EL. PIANO", "Electric Piano"),
    3: ("KEY", "KEYBOARDS", "Other Keyboards"),
    4: ("BEL", "BELL", "Bell"),
    5: ("MLT", "MALLET", "Mallet"),
    6: ("ORG", "ORGAN", "Electric and Church Organ"),
    7: ("ACD", "ACCORDION", "Accordion"),
    8: ("HRM", "HARMONICA", "Harmonica, Blues Harp"),
    9: ("AGT", "AC.GUITAR", "Acoustic Guitar"),
    10: ("EGT", "EL.GUITAR", "Electric Guitar"),
    11: ("DGT", "DIST.GUITAR", "Distortion Guitar"),
    12: ("BS", "BASS", "Acoustic & Electric Bass"),
    13: ("SBS", "SYNTH BASS", "Synth Bass"),
    14: ("STR", "STRINGS", "Strings"),
    15: ("ORC", "ORCHESTRA", "Orchestra Ensemble"),
    16: ("HIT", "HIT&STAB", "Orchestra Hit, Hit"),
    17: ("WND", "WIND", "Winds (Oboe, Clarinet etc.)"),
    18: ("FLT", "FLUTE", "Flute, Piccolo"),
    19: ("BRS", "AC.BRASS", "Acoustic Brass"),
    20: ("SBR", "SYNTH BRASS", "Synth Brass"),
    21: ("SAX", "SAX", "Sax"),
    22: ("HLD", "HARD LEAD", "Hard Synth Lead"),
    23: ("SLD", "SOFT LEAD", "Soft Synth Lead"),
    24: ("TEK", "TECHNO SYNTH", "Techno Synth"),
    25: ("PLS", "PULSATING", "Pulsating Synth"),
    26: ("FX", "SYNTH FX", "Synth FX (Noise etc.)"),
    27: ("SYN", "OTHER SYNTH", "Poly Synth"),
    28: ("BPD", "BRIGHT PAD", "Bright Pad Synth"),
    29: ("SPD", "SOFT PAD", "Soft Pad Synth"),
    30: ("VOX", "VOX", "Vox, Choir"),
    31: ("PLK", "PLUCKED", "Plucked (Harp etc.)"),
    32: ("ETH", "ETHNIC", "Other Ethnic"),
    33: ("FRT", "FRETTED", "Fretted Inst (Mandolin etc.)"),
    34: ("PRC", "PERCUSSION", "Percussion"),
    35: ("SFX", "SOUND FX", "Sound FX"),
    36: ("BTS", "BEAT&GROOVE", "Beat and Groove"),
    37: ("DRM", "DRUMS", "Drum Set"),
    38: ("CMB", "COMBINATION", "Other Patches which use Split and Layer"),
}


class DataBlock:
    def __init__(self, address: tuple, size, block_name: str):
        self.address = address
        self.block_name = block_name
        self.size = DataBlock.size_to_number(size)

    @staticmethod
    def size_as_7bit_list(size, number_of_values) -> List[int]:
        return [(size >> ((number_of_values - 1 - i) * 7)) & 0x7f for i in range(number_of_values)]

    @staticmethod
    def size_to_number(size) -> int:
        if isinstance(size, tuple):
            num_values = len(size)
            result = 0
            for i in range(num_values):
                result += size[i] << (7 * (num_values - 1 - i))
            return result
        else:
            return size


class RolandData:
    def __init__(self, data_name: str, num_items: int, num_address_bytes: int, num_size_bytes: int, base_address: Tuple, blocks: List[DataBlock], uses_consecutive_addresses: Optional[bool] = False,
                 supported_layouts: Optional[List[Tuple[int, ...]]] = None):
        self.data_name = data_name
        self.num_items = num_items  # This is the "bank size" of that data type
        self.num_address_bytes = num_address_bytes
        self.num_size_bytes = num_size_bytes
        self.base_address = base_address
        self.data_blocks = blocks
        self.size = self.total_size()
        self.allowed_addresses = set([self.absolute_address(x.address) for x in self.data_blocks])
        self.blank_out_zones = None
        self.uses_consecutive_addresses = uses_consecutive_addresses
        # Each tuple describes a complete supported model variant, in block order.
        self.supported_layouts = {tuple(block.size for block in blocks)}
        self.supported_layouts.update(supported_layouts or [])

    def make_black_out_zones(self, model_id_length: int, program_position: Union[int, Tuple[int, int]] = None, device_id_position: int = None, name_blankout: Tuple[int, int, int] = None):
        # Calculate the additional bytes each data block takes. This is sysex header, checksum and sysex end, plus model ID and device ID
        # message = [0xf0, roland_id, device & 0x1f] + self.model_id + [command_id] + address + data + [0, 0xf7]
        data_block_overhead = 3 + model_id_length + 1 + 2 + self.num_address_bytes
        self.blank_out_zones = []
        # Ignore checksums, because they might include the program position and name and will be different for an edit buffer and a program dump
        self.blank_out_zones += [(self._end_index_of_block(x, data_block_overhead) - 1, 1) for x in range(len(self.data_blocks))]
        if device_id_position is not None:
            # We want the fingerprint to ignore the device ID
            self.blank_out_zones += [(self._start_index_of_block(x, data_block_overhead) + device_id_position, 1) for x in range(len(self.data_blocks))]
        if program_position is not None:
            # We want the fingerprint to ignore the program position
            if isinstance(program_position, tuple):
                self.blank_out_zones += [(self._start_index_of_block(x, data_block_overhead) + program_position[0], program_position[1]) for x in range(len(self.data_blocks))]
            else:
                self.blank_out_zones += [(self._start_index_of_block(x, data_block_overhead) + program_position, 1) for x in range(len(self.data_blocks))]
        if name_blankout is not None:
            self.blank_out_zones += [(self._start_index_of_block(name_blankout[0], data_block_overhead) + data_block_overhead - 2
                                      + name_blankout[1], name_blankout[2])]

    def _start_index_of_block(self, block_no, data_block_overhead):
        return sum([(0 if i == 0 else (self.data_blocks[i-1].size + data_block_overhead)) for i in range(block_no + 1)])

    def _end_index_of_block(self, block_no, data_block_overhead):
        return self._start_index_of_block(block_no, data_block_overhead) + self.data_blocks[block_no].size + data_block_overhead - 1

    def total_size(self) -> int:
        return sum([f.size for f in self.data_blocks])

    def total_size_as_list(self) -> List[int]:
        return DataBlock.size_as_7bit_list(self.size * 8, self.num_size_bytes)  # Why times 8?. You can't cross border from one data set into the next

    def absolute_address(self, address: Tuple[int]) -> Tuple:
        return tuple([(address[i] + self.base_address[i]) for i in range(self.num_address_bytes)])

    def address_and_size_for_sub_request(self, sub_request, sub_address) -> Tuple[List[int], List[int]]:
        if self.uses_consecutive_addresses:
            base_number = DataBlock.size_to_number(tuple(self.base_address))
            address = DataBlock.size_to_number(tuple(self.data_blocks[sub_request].address))
            multiplier = sub_address * self.size
            target_address = base_number + address + multiplier
            concrete_address = DataBlock.size_as_7bit_list(target_address, self.num_address_bytes)
        else:
            # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
            concrete_address = [(self.data_blocks[sub_request].address[i] + self.base_address[i]) if i != 1
                                else (sub_address + self.base_address[i])
                                for i in range(len(self.data_blocks[sub_request].address))]
        return concrete_address, DataBlock.size_as_7bit_list(self.data_blocks[sub_request].size, self.num_size_bytes)

    def reset_to_base_address(self, address) -> Tuple:
        if self.uses_consecutive_addresses:
            address_as_number = DataBlock.size_to_number(tuple(address))
            base_number = DataBlock.size_to_number(tuple(self.base_address))
            if address_as_number >= base_number:
                normalized_address = address_as_number - base_number
                # Calculate the normalized addressed modulo the size of the data blocks
                normalized_address = normalized_address % self.total_size()
                readjusted_base = base_number + normalized_address
                return tuple(DataBlock.size_as_7bit_list(readjusted_base, self.num_address_bytes))
            else:
                return tuple(address)
        else:
            # The address[1] part is where the program number is stored. To compare addresses we reset it to the base address
            return tuple([address[i] if i != 1 else self.base_address[i] for i in range(self.num_address_bytes)])

    def address_and_size_for_all_request(self, sub_address) -> Tuple[List[int], List[int]]:
        # The idea is that if we request the first block, but with the total size of all blocks, the device will send us all messages back.
        # Somehow that does work, but not as expected. To get all messages from a single patch on an XV-3080, I need to multiply the size by 8???

        # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
        concrete_address = [(self.data_blocks[0].address[i] + self.base_address[i]) if i != 1
                            else (sub_address + self.base_address[i])
                            for i in range(len(self.data_blocks[0].address))]
        return concrete_address, self.total_size_as_list()


def knobkraft_api(func):
    func._is_knobkraft = True
    return func


class GenericRoland:
    def __init__(self, name: str, model_id: List[int], address_size: int, edit_buffer: RolandData, program_dump: RolandData,
                 category_index: Optional[int] = None,
                 device_family: Optional[List[int]] = None,
                 device_detect_message: Optional[RolandData] = None,
                 device_detect_ids: Optional[List[int]] = None,
                 patch_name_message_number: Optional[int] = 0,
                 patch_name_length: Optional[int] = 12,
                 use_roland_character_set: Optional[bool] = False,
                 uses_consecutive_addresses: Optional[bool] = False,
                 patch_name_offset: int = 0):
        self._name = name
        self.model_id = model_id
        self.device_family = device_family  # This is only used in the Identity Reply Message.
        self.device_detect_message = device_detect_message
        self.device_detect_ids = None if device_detect_ids is None else set(device_detect_ids)
        self.device_id = 0x10  # The Roland can have a device ID from 0x00 to 0x1f
        self._model_id_len = len(model_id)
        self.address_size = address_size
        self.edit_buffer = edit_buffer
        self.program_dump = program_dump
        self.category_index = category_index
        self.patch_name_message_number = patch_name_message_number
        self.patch_name_length = patch_name_length
        self.patch_name_offset = patch_name_offset
        self.use_roland_character_set = use_roland_character_set
        self.uses_consecutive_addresses = uses_consecutive_addresses
        self._address_maps = {}

    @knobkraft_api
    def name(self):
        return self._name

    @knobkraft_api
    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        if self.device_family is not None:
            # Detecting the Roland via an Identity Request message
            # This is a sysex generic device detect message
            return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]
        elif self.device_detect_message is not None:
            # Might be an older (pre XV-3080) Roland, try to query for the system common first data block and see if it answers
            address, size = self.device_detect_message.address_and_size_for_sub_request(0, 0)
            return self.buildRolandMessage((channel + 0x10) & 0x1f, command_rq1, address, size)
        else:
            print(f"{self._name} adaptation: No auto detection implemented. Specify either device family for identity reply, or data block")
            return []

    @knobkraft_api
    def channelIfValidDeviceResponse(self, message: List[int]) -> int:
        if self.device_family is not None:
            # The Roland usually will reply on a Universal Device Identity Reply message
            if (len(message) >= 15
                    and message[0] == 0xf0  # Sysex
                    and message[1] == 0x7e  # Non-realtime
                    and message[3] == 0x06  # Device request
                    and message[4] == 0x02  # Device request reply
                    and message[5] == 0x41  # Roland
                    and message[6:6 + len(self.device_family)] == self.device_family
                    and message[-1] == 0xf7
                    and all(0 <= x < 0x80 for x in message[1:-1])
                    and message[2] <= 0x1f):
                # and message[8:10] == [0x00, 0x00]):  # Family code
                self.device_id = message[2]  # Store the device ID for later, we'll need it
                return message[2] & 0x0f  # Simulate MIDI channel, but of course this is stupid
        elif self.device_detect_message is not None:
            # Check if the message is our own, and at the address we were expecting
            if self.isOwnSysex(message):
                try:
                    command, address, reply = self.parseRolandMessage(message)
                except ValueError:
                    return -1
                if command == command_dt1 and address == list(self.device_detect_message.absolute_address(self.device_detect_message.data_blocks[0].address)):
                    self.device_id = message[2]
                    return message[2] & 0x0f
        return -1

    @knobkraft_api
    def needsChannelSpecificDetection(self) -> bool:
        # When using a standard message, we actually need to iterate over all device IDs (not implemented yet, only channels 0 to 15)
        return self.device_family is None

    @knobkraft_api
    def bankDescriptors(self) -> List[Dict]:
        return [{"bank": 0, "name": "User Patches", "size": self.program_dump.num_items, "type": "User Patch"}]

    def isOwnSysex(self, message) -> bool:
        if len(message) > (2 + self._model_id_len):
            if message[0] == 0xf0 and message[1] == roland_id and message[3:3 + self._model_id_len] == self.model_id:
                return True
        return False

    def _checksum_start(self) -> int:
        return 4 + self._model_id_len

    def buildRolandMessage(self, device, command_id, address, data) -> List[int]:
        message = [0xf0, roland_id, device & 0x1f] + self.model_id + [command_id] + address + data + [0, 0xf7]
        message[-2] = self.roland_checksum(message[self._checksum_start():-2])
        return message

    def parseRolandMessage(self, message: list) -> Tuple[int, List[int], List[int]]:
        if (len(message) < self._checksum_start() + self.address_size + 2
                or not self.isOwnSysex(message) or message[-1] != 0xf7
                or not all(isinstance(x, int) and 0 <= x < 0x80 for x in message[1:-1])
                or message[2] > 0x1f):
            raise ValueError("Invalid Roland message framing, model or device ID")
        checksum_start = self._checksum_start()
        checksum = self.roland_checksum(message[checksum_start:-2])
        if checksum == message[-2]:
            command = message[3 + self._model_id_len]
            address = message[checksum_start:checksum_start + self.address_size]
            return command, address, message[checksum_start + self.address_size:-2]
        raise ValueError("Checksum error in Roland message parsing", message[-2], checksum)

    def getCommandAndAddressFromRolandMessage(self, message: list) -> Tuple[int, List[int]]:
        checksum_start = self._checksum_start()
        return message[3 + self._model_id_len], message[checksum_start:checksum_start + self.address_size]

    @staticmethod
    def roland_checksum(data_block) -> int:
        return sum([-x for x in data_block]) & 0x7f

    @knobkraft_api
    def createEditBufferRequest(self, channel) -> List[int]:
        # The edit buffer is called Patch mode temporary patch
        address, size = self._address_for_sub_request(self.edit_buffer, 0, 0)
        return self.buildRolandMessage(self.device_id, command_rq1, address, size)

    def _createFollowUpEditBufferDumpRequest(self, previousRequestNo):
        # Check if there is a follow up data block
        if previousRequestNo + 1 < len(self.edit_buffer.data_blocks):
            address, size = self._address_for_sub_request(self.edit_buffer, previousRequestNo + 1, 0)
            return self.buildRolandMessage(self.device_id, command_rq1, address, size)
        else:
            return []

    def _address_for_sub_request(self, layout, block_no, item):
        # Models with a different part stride can override this one address hook.
        return layout.address_and_size_for_sub_request(block_no, item)

    def _block_address_map(self, layout):
        # Use concrete addresses, including program/part context, rather than
        # dropping an address byte. Cache per instance, never on shared layouts.
        if layout not in self._address_maps:
            addresses = {}
            for item in range(layout.num_items):
                for block_no in range(len(layout.data_blocks)):
                    address, _ = self._address_for_sub_request(layout, block_no, item)
                    key = tuple(address)
                    if key in addresses:
                        raise ValueError("Ambiguous Roland block layout")
                    addresses[key] = (block_no, item)
            self._address_maps[layout] = addresses
        return self._address_maps[layout]

    def _parse_block(self, message, layout):
        command, address, data = self.parseRolandMessage(message)
        if command != command_dt1:
            raise ValueError("Expected a Roland DT1 block")
        match = self._block_address_map(layout).get(tuple(address))
        if match is None:
            raise ValueError("Unknown Roland block address or program")
        block_no, item = match
        if not any(len(data) == sizes[block_no] for sizes in layout.supported_layouts):
            raise ValueError("Unsupported Roland block size")
        return block_no, item, address, data

    def _parse_dump(self, message, layout):
        blocks = {}
        context = None
        end_of_previous = 0
        for start, end in knobkraft.sysex.findSysexDelimiters(message):
            if start != end_of_previous:
                raise ValueError("Unexpected data between Roland blocks")
            sub = message[start:end]
            block_no, item, address, data = self._parse_block(sub, layout)
            if block_no in blocks:
                raise ValueError("Duplicate Roland block")
            if context is not None and context != (sub[2], item):
                raise ValueError("Mixed Roland devices or programs")
            context = (sub[2], item)
            blocks[block_no] = (address, data)
            end_of_previous = end
        if end_of_previous != len(message) or len(blocks) != len(layout.data_blocks):
            raise ValueError("Incomplete Roland dump")
        if tuple(len(blocks[i][1]) for i in range(len(blocks))) not in layout.supported_layouts:
            raise ValueError("Mixed or unsupported Roland layout variants")
        return blocks, context

    def _validated_dump(self, message):
        for layout in (self.program_dump, self.edit_buffer):
            try:
                blocks, context = self._parse_dump(message, layout)
                return layout, blocks, context
            except ValueError:
                pass
        raise ValueError("Expected a complete, valid Roland edit buffer or program dump")

    def _is_dump(self, messages, layout):
        try:
            self._parse_dump(messages, layout)
            return True
        except ValueError:
            return False

    @knobkraft_api
    def isPartOfEditBufferDump(self, message):
        try:
            block_no, _, _, _ = self._parse_block(message, self.edit_buffer)
            return True, self._createFollowUpEditBufferDumpRequest(block_no)
        except ValueError:
            return False

    @knobkraft_api
    def isEditBufferDump(self, messages):
        return self._is_dump(messages, self.edit_buffer)

    def _convert_dump(self, message, destination, item, device_id=None):
        source, blocks, _ = self._validated_dump(message)
        by_identity = {source.data_blocks[i].address: data for i, (_, data) in blocks.items()}
        if set(by_identity) != {block.address for block in destination.data_blocks}:
            raise ValueError("Incompatible Roland source and destination layouts")
        payloads = [by_identity[block.address] for block in destination.data_blocks]
        if tuple(map(len, payloads)) not in destination.supported_layouts:
            raise ValueError("Unsupported Roland destination layout variant")
        result = []
        for block_no, data in enumerate(payloads):
            address, _ = self._address_for_sub_request(destination, block_no, item)
            result += self.buildRolandMessage(self.device_id if device_id is None else device_id,
                                              command_dt1, address, data)
        return result

    @knobkraft_api
    def convertToEditBuffer(self, channel, message):
        return self._convert_dump(message, self.edit_buffer, 0)

    @knobkraft_api
    def createProgramDumpRequest(self, channel, patchNo):
        address, size = self._address_for_sub_request(self.program_dump, 0, patchNo % self.program_dump.num_items)
        return self.buildRolandMessage(self.device_id, command_rq1, address, size)

    def _createFollowUpProgramDumpRequest(self, patchNo, previousRequestNo):
        # Check if there is a follow up data block
        if previousRequestNo + 1 < len(self.program_dump.data_blocks):
            address, size = self._address_for_sub_request(self.program_dump, previousRequestNo + 1, patchNo % self.program_dump.num_items)
            return self.buildRolandMessage(self.device_id, command_rq1, address, size)
        else:
            return []

    @knobkraft_api
    def isPartOfSingleProgramDump(self, message):
        try:
            block_no, item, _, _ = self._parse_block(message, self.program_dump)
            return True, self._createFollowUpProgramDumpRequest(item, block_no)
        except ValueError:
            return False

    @knobkraft_api
    def isSingleProgramDump(self, messages):
        return self._is_dump(messages, self.program_dump)

    @knobkraft_api
    def convertToProgramDump(self, channel, message, program_number):
        return self._convert_dump(message, self.program_dump, program_number % self.program_dump.num_items)

    @staticmethod
    def _apply_blankout(data: List[int], blankout: List[Tuple[int, int]]):
        result = data
        for blank in blankout:
            for i in range(blank[1]):
                result[blank[0] + i] = 0
        # Additionnal blankout: The checkums can't be precalculated, as we have messages with varying length
        # Instead just blank every byte before the 0xf7
        for i in range(len(data)):
            if i+1 < len(data):
                if data[i+1] == 0xf7:
                    data[i] = 0x00
        return result

    def blankedOut(self, message):
        # Canonical program-slot-zero DT1 serialization preserves the legacy hash
        # of ordered, nominal-size program dumps at the default device ID.
        # Construct each block separately so variant lengths cannot shift masks
        # into sound data. See docs/roland-fingerprints.md for database migration.
        canonical = self._convert_dump(message, self.program_dump, 0, device_id=0x10)
        result = []
        for block_no, sub in enumerate(knobkraft.splitSysex(canonical)):
            sub[self._checksum_start() + 1] = 0  # legacy program-position mask
            sub[-2] = 0
            if block_no == self.patch_name_message_number:
                name_start = self._checksum_start() + self.address_size + self.patch_name_offset
                sub[name_start:name_start + self.patch_name_length] = [0] * self.patch_name_length
            result.extend(sub)
        return result

    @knobkraft_api
    def calculateFingerprint(self, message):
        return hashlib.md5(bytearray(self.blankedOut(message))).hexdigest()

    def _patch_number_from_address(self, address):
        if self.uses_consecutive_addresses:
            address_as_number = DataBlock.size_to_number(tuple(address))
            base_as_number = DataBlock.size_to_number(tuple(self.program_dump.base_address))
            return (address_as_number - base_as_number) // self.program_dump.size
        else:
            return address[1] - self.program_dump.base_address[1]

    @knobkraft_api
    def numberFromDump(self, message) -> int:
        try:
            _, context = self._parse_dump(message, self.program_dump)
            return context[1]
        except ValueError:
            return 0

    @knobkraft_api
    def nameFromDump(self, message) -> str:
        try:
            _, blocks, _ = self._validated_dump(message)
        except ValueError:
            return 'Invalid'
        data = blocks[self.patch_name_message_number][1]
        name = data[self.patch_name_offset:self.patch_name_offset + self.patch_name_length]
        if self.use_roland_character_set:
            return ''.join(character_set[x] if x < len(character_set) else ' ' for x in name)
        return ''.join(chr(x) for x in name)

    @knobkraft_api
    def renamePatch(self, message: List[int], new_name: str) -> List[int]:
        """
        Return a new dump with the patch name changed to `new_name`.
        Works for both single program dumps and edit buffer dumps.
        """
        layout, blocks, context = self._validated_dump(message)

        # Prepare name bytes
        name = (new_name or "").strip()
        # Truncate and pad to exact length
        name = (name[:self.patch_name_length]).ljust(self.patch_name_length, " ")

        if self.use_roland_character_set:
            # Map characters to legacy Roland indices; fallback to space when unsupported
            charset_index: Dict[str, int] = {ch: i for i, ch in enumerate(character_set)}
            name_bytes = [charset_index.get(ch, charset_index[" "]) for ch in name]
        else:
            # Standard 7-bit ASCII (Roland SysEx is 7-bit clean)
            name_bytes = [ord(ch) & 0x7F for ch in name]

        rebuilt: List[int] = []
        for block_no in range(len(layout.data_blocks)):
            address, data = blocks[block_no]
            data = data.copy()
            if block_no == self.patch_name_message_number:
                data[self.patch_name_offset:self.patch_name_offset + self.patch_name_length] = name_bytes
            rebuilt += self.buildRolandMessage(context[0], command_dt1, address, data)
        return rebuilt

    @knobkraft_api
    def storedTags(self, message) -> List[str]:
        if self.category_index is not None:
            try:
                _, blocks, _ = self._validated_dump(message)
            except ValueError:
                return []
            data = blocks[0][1]
            if self.category_index < len(data) and data[self.category_index] in categories:
                return [categories[data[self.category_index]][1]]
        return []

    def install(self, module):
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        for a in dir(self):
            if callable(getattr(self, a)) and hasattr(getattr(self, a), "_is_knobkraft"):
                # this was helpful: http://stupidpythonideas.blogspot.com/2013/06/how-methods-work.html
                setattr(module, a, getattr(self, a))


class GenericRolandWithBackwardCompatibility:
    def __init__(self, main_model: GenericRoland, compatible_models: List[GenericRoland]):
        # Own the destination and protocol instances. Detection must not mutate
        # imported JV adaptations or another wrapper constructed from these models.
        self.models_supported = copy.deepcopy([main_model] + compatible_models)
        self.main_model = self.models_supported[0]

    def _destination_model(self, message):
        model = self.model_from_message(message)
        if model is None:
            raise ValueError("Unsupported Roland source model")
        # Keep protocol/layout selection independent of the destination device ID.
        destination = copy.copy(model)
        destination.device_id = self.main_model.device_id
        return destination

    def model_from_message(self, message) -> Optional[GenericRoland]:
        for synth in self.models_supported:
            if synth.isOwnSysex(message):
                return synth
        return None

    @knobkraft_api
    def name(self):
        return self.main_model.name()

    @knobkraft_api
    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        return self.main_model.createDeviceDetectMessage(channel)

    @knobkraft_api
    def channelIfValidDeviceResponse(self, message: List[int]) -> int:
        # The Roland usually will reply on a Universal Device Identity Reply message
        channel = self.main_model.channelIfValidDeviceResponse(message)
        if channel >= 0:
            for model in self.models_supported:
                model.device_id = self.main_model.device_id
        return channel

    @knobkraft_api
    def needsChannelSpecificDetection(self) -> bool:
        return self.main_model.needsChannelSpecificDetection()

    @knobkraft_api
    def bankDescriptors(self):
        return self.main_model.bankDescriptors()

    @knobkraft_api
    def createEditBufferRequest(self, _channel) -> List[int]:
        return self.main_model.createEditBufferRequest(self.main_model.device_id)

    @knobkraft_api
    def isPartOfEditBufferDump(self, message) -> bool:
        model = self.model_from_message(message)
        # Accept a certain set of addresses
        if model is not None:
            return model.isPartOfEditBufferDump(message)
        return False

    @knobkraft_api
    def isEditBufferDump(self, data) -> bool:
        model = self.model_from_message(data)
        if model is not None:
            return model.isEditBufferDump(data)
        return False

    @knobkraft_api
    def convertToEditBuffer(self, _channel, message):
        return self._destination_model(message).convertToEditBuffer(_channel, message)

    @knobkraft_api
    def createProgramDumpRequest(self, _channel, patchNo):
        return self.main_model.createProgramDumpRequest(self.main_model.device_id, patchNo)

    @knobkraft_api
    def isPartOfSingleProgramDump(self, message):
        # Accept a certain set of addresses
        model = self.model_from_message(message)
        if model is not None:
            return model.isPartOfSingleProgramDump(message)
        return False

    @knobkraft_api
    def isSingleProgramDump(self, data):
        model = self.model_from_message(data)
        if model is not None:
            return model.isSingleProgramDump(data)
        return False

    @knobkraft_api
    def convertToProgramDump(self, _channel, message, program_number):
        return self._destination_model(message).convertToProgramDump(_channel, message, program_number)

    @knobkraft_api
    def numberFromDump(self, message) -> int:
        model = self.model_from_message(message)
        if model is not None:
            return model.numberFromDump(message)
        return -1

    @knobkraft_api
    def nameFromDump(self, message) -> str:
        model = self.model_from_message(message)
        if model is not None:
            return model.nameFromDump(message)
        return 'Invalid'

    @knobkraft_api
    def renamePatch(self, message: List[int], new_name: str) -> List[int]:
        model = self.model_from_message(message)
        if model is not None:
            return model.renamePatch(message, new_name)
        print("Can't rename patch as model cannot be detected!")
        return message

    @knobkraft_api
    def calculateFingerprint(self, message) -> int:
        model = self.model_from_message(message)
        if model is not None:
            return model.calculateFingerprint(message)
        raise Exception("Can't fingerprint data that is not of one of the defined Roland Synths")

    @knobkraft_api
    def storedTags(self, message) -> List[str]:
        model = self.model_from_message(message)
        if model is not None:
            return model.storedTags(message)
        return []

    def install(self, module):
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        for a in dir(self):
            if callable(getattr(self, a)) and hasattr(getattr(self, a), "_is_knobkraft"):
                # this was helpful: http://stupidpythonideas.blogspot.com/2013/06/how-methods-work.html
                setattr(module, a, getattr(self, a))
