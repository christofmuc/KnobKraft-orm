#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from typing import List, Tuple, Optional
import knobkraft

roland_id = 0x41  # Roland
command_rq1 = 0x11
command_dt1 = 0x12

# Construct the Roland character set as specified in the MIDI implementation
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
    31: ("PLK" "PLUCKED", "Plucked (Harp etc.)"),
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
    def __init__(self, data_name: str, num_items: int, num_address_bytes: int, num_size_bytes: int, base_address: Tuple, blocks: List[DataBlock]):
        self.data_name = data_name
        self.num_items = num_items  # This is the "bank size" of that data type
        self.num_address_bytes = num_address_bytes
        self.num_size_bytes = num_size_bytes
        self.base_address = base_address
        self.data_blocks = blocks
        self.size = self.total_size()
        self.allowed_addresses = set([self.absolute_address(x.address) for x in self.data_blocks])
        self.blank_out_zones = None

    def make_black_out_zones(self, model_id_length: int, program_position: int = None, name_blankout: Tuple[int, int, int] = None):
        # Calculate the additional bytes each data block takes. This is sysex header, checksum and sysex end, plus model ID and device ID
        # message = [0xf0, roland_id, device & 0x1f] + self.model_id + [command_id] + address + data + [0, 0xf7]
        data_block_overhead = 3 + model_id_length + 1 + 2
        self.blank_out_zones = []
        if program_position is not None:
            # We want the fingerprint to ignore the program position
            self.blank_out_zones += [(self._start_index_of_block(x, data_block_overhead) + program_position, 1) for x in range(len(self.data_blocks))]
        if name_blankout is not None:
            self.blank_out_zones += [(self._start_index_of_block(name_blankout[0], data_block_overhead) + name_blankout[1], name_blankout[2])]

    def _start_index_of_block(self, block_no, data_block_overhead):
        return sum((self.data_blocks[i].size + data_block_overhead) for i in range(block_no + 1))

    def total_size(self) -> int:
        return sum([f.size for f in self.data_blocks])

    def total_size_as_list(self) -> List[int]:
        return DataBlock.size_as_7bit_list(self.size * 8, self.num_size_bytes)  # Why times 8?. You can't cross border from one data set into the next

    def absolute_address(self, address: Tuple[int]) -> Tuple:
        return tuple([(address[i] + self.base_address[i]) for i in range(self.num_address_bytes)])

    def address_and_size_for_sub_request(self, sub_request, sub_address) -> Tuple[List[int], List[int]]:
        # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
        concrete_address = [(self.data_blocks[sub_request].address[i] + self.base_address[i]) if i != 1 else sub_address
                            for i in range(len(self.data_blocks[sub_request].address))]
        return concrete_address, self.total_size_as_list()

    def address_and_size_for_all_request(self, sub_address) -> Tuple[List[int], List[int]]:
        # The idea is that if we request the first block, but with the total size of all blocks, the device will send us all messages back.
        # Somehow that does work, but not as expected. To get all messages from a single patch on an XV-3080, I need to multiply the size by 8???

        # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
        concrete_address = [self.data_blocks[0].address[i] if i != 1 else sub_address for i in range(len(self.data_blocks[0].address))]
        return concrete_address, self.total_size_as_list()


class GenericRoland:
    def __init__(self, name: str, model_id: List[int], address_size: int, edit_buffer: RolandData, program_dump: RolandData,
                 category_index: Optional[int] = None):
        self.name = name
        self.model_id = model_id
        self._model_id_len = len(model_id)
        self.address_size = address_size
        self.edit_buffer = edit_buffer
        self.program_dump = program_dump
        self.category_index = category_index
        # Calculate the fingerprint blank out zones for edit buffer (just the name) and program dump (program position and name)
        edit_buffer.make_black_out_zones(self._model_id_len, 5 + self._model_id_len)
        program_dump.make_black_out_zones(self._model_id_len, 5 + self._model_id_len,
                                          (0, 0, 12))  # name always is in block 0 with index 0 and length 12

    def isOwnSysex(self, message) -> bool:
        if len(message) > (2 + self._model_id_len):
            if message[0] == 0xf0 and message[1] == roland_id and message[3:3 + self._model_id_len] == self.model_id:
                return True
        return False

    def _checksum_start(self) -> int:
        return 4 + self._model_id_len

    def _ignoreProgramPosition(self, address) -> Tuple:
        # The address[1] part is where the program number is stored. To compare addresses we set it to 0
        return tuple([address[i] if i != 1 else 0x00 for i in range(self.address_size)])

    def buildRolandMessage(self, device, command_id, address, data) -> List[int]:
        message = [0xf0, roland_id, device & 0x1f] + self.model_id + [command_id] + address + data + [0, 0xf7]
        message[-2] = self.roland_checksum(message[self._checksum_start():-2])
        return message

    def parseRolandMessage(self, message: list) -> Tuple[int, List[int], List[int]]:
        checksum_start = self._checksum_start()
        checksum = self.roland_checksum(message[checksum_start:-2])
        if checksum == message[-2]:
            command = message[3 + self._model_id_len]
            address = message[checksum_start:checksum_start + self.address_size]
            return command, address, message[checksum_start + self.address_size:-2]
        raise Exception("Checksum error in Roland message parsing, expected", message[-2], "but got", checksum)

    def getCommandAndAddressFromRolandMessage(self, message: list) -> Tuple[int, List[int]]:
        checksum_start = self._checksum_start()
        return message[3 + self._model_id_len], message[checksum_start:checksum_start + self.address_size]

    @staticmethod
    def roland_checksum(data_block) -> int:
        return sum([-x for x in data_block]) & 0x7f

    def createEditBufferRequest(self, device_id) -> List[int]:
        # The edit buffer is called Patch mode temporary patch
        result = []
        for i in range(len(self.edit_buffer.data_blocks)):
            address, size = self.edit_buffer.address_and_size_for_sub_request(i, 0)
            result += self.buildRolandMessage(device_id, command_rq1, address, size)
        return result

    def isPartOfEditBufferDump(self, message):
        # Accept a certain set of addresses. This does not verify the checksum, for speed reasons, or check the size
        if self.isOwnSysex(message):
            command, address = self.getCommandAndAddressFromRolandMessage(message)
            return command == command_dt1 and tuple(self._ignoreProgramPosition(address)) in self.edit_buffer.allowed_addresses
        else:
            return False

    def isEditBufferDump(self, messages):
        addresses = set()
        for message in knobkraft.sysex.findSysexDelimiters(messages):
            if self.isOwnSysex(messages[message[0]:message[1]]):
                _, address = self.getCommandAndAddressFromRolandMessage(messages[message[0]:message[1]])
                addresses.add(tuple(self._ignoreProgramPosition(address)))
        return all(a in addresses for a in self.edit_buffer.allowed_addresses)

    def convertToEditBuffer(self, device_id, message):
        editBuffer = []
        if self.isEditBufferDump(message) or self.isSingleProgramDump(message):
            # We need to poke the device ID and the edit buffer address into the messages
            msg_no = 0
            for message in knobkraft.sysex.splitSysexMessage(message):
                command, address, data = self.parseRolandMessage(message)
                edit_buffer_address, _ = self.edit_buffer.address_and_size_for_sub_request(msg_no, 0x00)
                editBuffer = editBuffer + self.buildRolandMessage(device_id, command_dt1, edit_buffer_address, data)
                msg_no += 1
            return editBuffer
        raise Exception("Invalid argument given, can only convert edit buffers and program dumps to edit buffers")

    def createProgramDumpRequest(self, device_id, patchNo):
        address, size = self.program_dump.address_and_size_for_all_request(patchNo % self.program_dump.num_items)
        return self.buildRolandMessage(device_id, command_rq1, address, size)

    def isPartOfSingleProgramDump(self, message):
        # Accept a certain set of addresses
        if self.isOwnSysex(message):
            command, address = self.getCommandAndAddressFromRolandMessage(message)
            matches = tuple(self._ignoreProgramPosition(address)) in self.program_dump.allowed_addresses
            return command == command_dt1 and matches
        else:
            return False

    def isSingleProgramDump(self, messages):
        addresses = set()
        programs = set()
        for message in knobkraft.sysex.findSysexDelimiters(messages):
            _, address = self.getCommandAndAddressFromRolandMessage(messages[message[0]:message[1]])
            addresses.add(self._ignoreProgramPosition(address))
            programs.add(address[1])
        return len(programs) == 1 and all(a in addresses for a in self.program_dump.allowed_addresses)

    def convertToProgramDump(self, device_id, message, program_number):
        programDump = []
        if self.isSingleProgramDump(message) or self.isEditBufferDump(message):
            # We need to poke the device ID and the program number into the messages
            msg_no = 0
            for message in knobkraft.sysex.splitSysexMessage(message):
                _, _, data = self.parseRolandMessage(message)
                program_buffer_address, _ = self.program_dump.address_and_size_for_sub_request(msg_no, program_number % self.program_dump.num_items)
                programDump = programDump + self.buildRolandMessage(device_id, command_dt1, program_buffer_address, data)
                msg_no += 1
            return programDump
        raise Exception("Can only convert single program dumps to program dumps!")

    @staticmethod
    def _apply_blankout(data: List[int], blankout: List[Tuple[int, int]]):
        result = data
        for blank in blankout:
            for i in range(blank[1]):
                result[blank[0] + i] = 0
        return result

    def calculateFingerprint(self, message):
        # Use the prepared blank out zones to clear out a) program place and b) patch name
        if self.isEditBufferDump(message):
            return hashlib.md5(bytearray(self._apply_blankout(message, self.edit_buffer.blank_out_zones))).hexdigest()
        elif self.isSingleProgramDump(message):
            return hashlib.md5(bytearray(self._apply_blankout(message, self.program_dump.blank_out_zones))).hexdigest()
        else:
            return hashlib.md5(bytearray(message)).hexdigest()

    def numberFromDump(self, message) -> int:
        if not self.isSingleProgramDump(message):
            return 0
        messages = knobkraft.sysex.findSysexDelimiters(message, 1)
        _, address = self.getCommandAndAddressFromRolandMessage(message[messages[0][0]:messages[0][1]])
        return address[1]

    def nameFromDump(self, message) -> str:
        if self.isSingleProgramDump(message) or self.isEditBufferDump(message):
            messages = knobkraft.sysex.findSysexDelimiters(message, 1)
            _, _, data = self.parseRolandMessage(message[messages[0][0]:messages[0][1]])
            patch_name = ''.join([chr(x) for x in data[0:12]])
            return patch_name.strip()
        return 'Invalid'

    def storedTags(self, message) -> List[str]:
        if self.category_index is not None:
            if self.isSingleProgramDump(message) or self.isEditBufferDump(message):
                messages = knobkraft.sysex.findSysexDelimiters(message, 1)
                _, _, data = self.parseRolandMessage(message[messages[0][0]:messages[0][1]])
                category = data[self.category_index]
                if 0 <= category < len(categories):
                    return [categories[category][1]]
                print(f"Warning - encountered invalid category number {category} for which no text is defined, ignoring")
        return []


# JV-80. The JV-90/JV-1000 share the model ID of the JV-80, but have two bytes more size than the JV-80. How do I handle that?
_jv80_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x22, "Patch common"),
                    DataBlock((0x00, 0x00, 0x08, 0x00), 0x73, "Patch tone 1"),  # 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x09, 0x00), 0x73, "Patch tone 2"),  # 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x0A, 0x00), 0x73, "Patch tone 3"),  # 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x0B, 0x00), 0x73, "Patch tone 4")]  # 0x75 size for the JV-90/JV-1000
_jv80_edit_buffer_addresses = RolandData("JV-80 Temporary Patch", 1, 4, 4,
                                         (0x00, 0x08, 0x20, 0x00),  # This is start address, needs offset address added!
                                         _jv80_patch_data)
_jv80_program_buffer_addresses = RolandData("JV-80 Internal Patch", 0x40, 4, 4,
                                            (0x01, 0x40, 0x20, 0x00),  # This is start address, needs offset address added!
                                            _jv80_patch_data)
jv_80 = GenericRoland("JV-80", model_id=[0x46], address_size=4, edit_buffer=_jv80_edit_buffer_addresses, program_dump=_jv80_program_buffer_addresses)

# JV-1080/JV-2080 are identical, only that the JV-2080 has 2 more bytes in the patch common section
_jv1080_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x48, "Patch common"),
                      DataBlock((0x00, 0x00, 0x10, 0x00), (0x01, 0x01), "Patch tone 1"),
                      DataBlock((0x00, 0x00, 0x12, 0x00), (0x01, 0x01), "Patch tone 2"),
                      DataBlock((0x00, 0x00, 0x14, 0x00), (0x01, 0x01), "Patch tone 3"),
                      DataBlock((0x00, 0x00, 0x16, 0x00), (0x01, 0x01), "Patch tone 4")]
_jv1080_edit_buffer_addresses = RolandData("JV-1080 Temporary Patch", 1, 4, 4,
                                           (0x03, 0x00, 0x00, 0x00),
                                           _jv1080_patch_data)
_jv1080_program_buffer_addresses = RolandData("JV-1080 User Patches", 128, 4, 4,
                                              (0x11, 0x00, 0x00, 0x00),
                                              _jv1080_patch_data)
jv_1080 = GenericRoland("JV-1080", model_id=[0x6a], address_size=4, edit_buffer=_jv1080_edit_buffer_addresses,
                        program_dump=_jv1080_program_buffer_addresses)

# JV-2080
_jv2080_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x4a, "Patch common"),  # 0x4a, 2 bytes more than the JV-1080 for the JV-2080
                      DataBlock((0x00, 0x00, 0x10, 0x00), (0x01, 0x01), "Patch tone 1"),
                      DataBlock((0x00, 0x00, 0x12, 0x00), (0x01, 0x01), "Patch tone 2"),
                      DataBlock((0x00, 0x00, 0x14, 0x00), (0x01, 0x01), "Patch tone 3"),
                      DataBlock((0x00, 0x00, 0x16, 0x00), (0x01, 0x01), "Patch tone 4")]
_jv2080_edit_buffer_addresses = RolandData("JV-2080 Temporary Patch", 1, 4, 4,
                                           (0x03, 0x00, 0x00, 0x00),
                                           _jv2080_patch_data)
_jv2080_program_buffer_addresses = RolandData("JV-2080 User Patches", 128, 4, 4,
                                              (0x11, 0x00, 0x00, 0x00),
                                              _jv2080_patch_data)
jv_2080 = GenericRoland("JV-2080", model_id=[0x6a], address_size=4, edit_buffer=_jv2080_edit_buffer_addresses,
                        program_dump=_jv2080_program_buffer_addresses,
                        category_index=0x49)

# XV-3080 and XV-5080. But the XV-5080 has these Patch Split Key messages as well!? We can ignore them?
_xv3080_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x4f, "Patch common"),
                      DataBlock((0x00, 0x00, 0x02, 0x00), (0x01, 0x11), "Patch common MFX"),
                      DataBlock((0x00, 0x00, 0x04, 0x00), 0x34, "Patch common Chorus"),
                      DataBlock((0x00, 0x00, 0x06, 0x00), 0x53, "Patch common Reverb"),
                      DataBlock((0x00, 0x00, 0x10, 0x00), 0x29, "Patch common Tone Mix Table"),
                      DataBlock((0x00, 0x00, 0x20, 0x00), (0x01, 0x09), "Tone 1"),
                      DataBlock((0x00, 0x00, 0x22, 0x00), (0x01, 0x09), "Tone 2"),
                      DataBlock((0x00, 0x00, 0x24, 0x00), (0x01, 0x09), "Tone 3"),
                      DataBlock((0x00, 0x00, 0x26, 0x00), (0x01, 0x09), "Tone 4")]
_xv3080_edit_buffer_addresses = RolandData("XV-3080 Temporary Patch", 1, 4, 4,
                                           (0x1f, 0x00, 0x00, 0x00),
                                           _xv3080_patch_data)
_xv3080_program_buffer_addresses = RolandData("XV-3080 User Patches", 128, 4, 4,
                                              (0x30, 0x00, 0x00, 0x00),
                                              _xv3080_patch_data)
xv_3080 = GenericRoland("XV-3080", model_id=[0x00, 0x10], address_size=4, edit_buffer=_xv3080_edit_buffer_addresses,
                        program_dump=_xv3080_program_buffer_addresses,
                        category_index=0x0c)
#  and XV-5080 and XV-5050?


# roland_gs = RolandSynth("GS", model_id=[0x42], address_size=3)
