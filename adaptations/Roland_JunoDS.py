#
#   Copyright (c) 2022-2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import hashlib
import sys
from typing import List, Union, Tuple, Optional, Dict

import knobkraft.sysex
import testing.test_data

this_module = sys.modules[__name__]

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


class DataBlock_:
    def __init__(self, address: tuple, size, block_name: str):
        self.address = address
        self.block_name = block_name
        self.size = DataBlock_.size_to_number(size)

    @staticmethod
    def size_as_7bit_list(size, number_of_values) -> List[int]:
        return [(size >> ((number_of_values - 1 - i) * 7)) & 0x7f for i in range(number_of_values)]

    @staticmethod
    def size_to_number(size: Union[List, Tuple, int]) -> int:
        if isinstance(size, tuple) or isinstance(size, list):
            num_values = len(size)
            result = 0
            for i in range(num_values):
                result += size[i] << (7 * (num_values - 1 - i))
            return result
        else:
            return size


class RolandData_:
    def __init__(self, data_name: str, num_items: int, num_address_bytes: int, num_size_bytes: int, base_address: Tuple, blocks: List[DataBlock_]):
        self.data_name = data_name
        self.num_items = num_items  # This is the "bank size" of that data type
        self.num_address_bytes = num_address_bytes
        self.num_size_bytes = num_size_bytes
        self._base_address = base_address
        self.base_address_int = DataBlock_.size_to_number(base_address)
        self.data_blocks = blocks
        self.size = self.total_size()
        self.allowed_addresses = set([self.absolute_address(x.address) for x in self.data_blocks])
        self.blank_out_zones = None

    def make_black_out_zones(self, model_id_length: int, program_position: int = None, name_blankout: Tuple[int, int, int] = None):
        # Calculate the additional bytes each data block takes. This is sysex header, checksum and sysex end, plus model ID and device ID
        # message = [0xf0, roland_id, device & 0x1f] + self.model_id + [command_id] + address + data + [0, 0xf7]
        data_block_overhead = 3 + model_id_length + 1 + 2 + self.num_address_bytes
        self.blank_out_zones = []
        # Ignore checksums, because they might include the program position and will be different for an edit buffer and a program dump
        #self.blank_out_zones += [(self._end_index_of_block(x, data_block_overhead) - 1, 1) for x in range(len(self.data_blocks))]
        if program_position is not None:
            # We want the fingerprint to ignore the program position
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
        return DataBlock_.size_as_7bit_list(self.size * 8, self.num_size_bytes)  # Why times 8?. You can't cross border from one data set into the next

    def absolute_address(self, address: Tuple[int]) -> Tuple:
        address_int = DataBlock_.size_to_number(address)
        return tuple(DataBlock_.size_as_7bit_list(address_int + self.base_address_int, self.num_address_bytes))

    def address_and_size_for_sub_request(self, sub_request, sub_address) -> Tuple[List[int], List[int]]:
        # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
        sub_address_int = (sub_address << 14) if self.num_address_bytes == 4 else (sub_address << 7)
        concrete_address = DataBlock_.size_as_7bit_list(DataBlock_.size_to_number(self.data_blocks[sub_request].address) + sub_address_int + self.base_address_int, self.num_address_bytes)
        return concrete_address, DataBlock_.size_as_7bit_list(self.data_blocks[sub_request].size, self.num_size_bytes)

    def subaddress_from_address(self, address: List[int]) -> int:
        offset = DataBlock_.size_to_number(tuple(address)) - self.base_address_int
        return offset >> ((self.num_address_bytes - 2) * 7)

    def find_base_address(self, address: tuple) -> Tuple:
        int_value = DataBlock_.size_to_number(address)
        sub_address = self.subaddress_from_address(list(address)) << 14
        return tuple(DataBlock_.size_as_7bit_list(int_value - sub_address, self.num_address_bytes))


def knobkraft_api(func):
    func._is_knobkraft = True
    return func


class GenericRoland_:
    def __init__(self, name: str, model_id: List[int], address_size: int, edit_buffer: RolandData_, program_dump: RolandData_,
                 bank_descriptors: Optional[List[Dict]] = None,
                 category_index: Optional[int] = None,
                 device_family: Optional[List[int]] = None,
                 device_detect_message: Optional[RolandData_] = None,
                 device_detect_ids: Optional[List[int]] = None):
        self._name = name
        self.model_id = model_id
        self.bank_descriptors = bank_descriptors
        self.device_family = device_family  # This is only used in the Identity Reply Message.
        self.device_detect_message = device_detect_message
        self.device_detect_ids = None if device_detect_ids is None else set(device_detect_ids)
        self.device_id = 0x10  # The Roland can have a device ID from 0x00 to 0x1f
        self._model_id_len = len(model_id)
        self.address_size = address_size
        self.edit_buffer = edit_buffer
        self.program_dump = program_dump
        self.category_index = category_index
        # Calculate the fingerprint blank out zones for edit buffer (just the name) and program dump (program position and name)
        edit_buffer.make_black_out_zones(self._model_id_len, program_position=5 + self._model_id_len)
        program_dump.make_black_out_zones(self._model_id_len, program_position=5 + self._model_id_len,
                                          name_blankout=(0, 0, 12))  # name always is in block 0 with index 0 and length 12

    @knobkraft_api
    def name(self):
        return self._name

    @knobkraft_api
    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        if self.device_family is not None:
            # Detecting the Roland via an Identity Request message
            # This is a sysex generic device detect message
            # Do not use the channel given, as this might be a MIDI channel but what we need is actually the sysex device ID
            # For the Juno DS, this would be 0x10, and that is not a MIDI channel, so we are in a bit of a bug situation here
            return [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]
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
            if (len(message) > 6 + self._model_id_len
                    and message[0] == 0xf0  # Sysex
                    and message[1] == 0x7e  # Non-realtime
                    and message[3] == 0x06  # Device request
                    and message[4] == 0x02  # Device request reply
                    and message[5] == 0x41  # Roland
                    and message[6:6 + self._model_id_len] == self.device_family):  # Family code expected, this is *not* the model ID
                # and message[8:10] == [0x00, 0x00]):  # Family code
                self.device_id = message[2]  # Store the device ID for later, we'll need it
                return message[2] & 0x0f  # Simulate MIDI channel, but of course this is stupid
        elif self.device_detect_message is not None:
            # Check if the message is our own, and at the address we were expecting
            if self.isOwnSysex(message):
                command, address, reply = self.parseRolandMessage(message)
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
        if self.bank_descriptors is not None:
            return self.bank_descriptors
        else:
            return [{"bank": 0, "name": "User Patches", "size": self.program_dump.num_items, "type": "User Patch"}]

    @knobkraft_api
    def bankSelect(self, channel, bank):
        if self.bank_descriptors is not None:
            if 0 <= bank < len(self.bank_descriptors):
                bank_info = self.bank_descriptors[bank]
                return [0xb0 | (channel & 0x0f), 0x00, bank_info["bank_msb"]] + [0xb0 | (channel & 0x0f), 0x20, bank_info["bank_lsb"]]
            else:
                raise RuntimeError(f"Bank {bank} given is out of range for bank descriptors!")
        else:
            # Hard to guess, let's just switch to bank 0?
            return [0xb0 | (channel & 0x0f), 0x00, 0]

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

    @knobkraft_api
    def createEditBufferRequest(self, channel) -> List[int]:
        # The edit buffer is called Patch mode temporary patch
        address, size = self.edit_buffer.address_and_size_for_sub_request(0, 0)
        return self.buildRolandMessage(self.device_id, command_rq1, address, size)

    def _createFollowUpEditBufferDumpRequest(self, previousRequestNo):
        # Check if there is a follow up data block
        if previousRequestNo + 1 < len(self.edit_buffer.data_blocks):
            address, size = self.edit_buffer.address_and_size_for_sub_request(previousRequestNo + 1, 0)
            return self.buildRolandMessage(self.device_id, command_rq1, address, size)
        else:
            return []

    @knobkraft_api
    def isPartOfEditBufferDump(self, message):
        # Accept a certain set of addresses. This does not verify the checksum, for speed reasons, or check the size
        if self.isOwnSysex(message):
            command, address = self.getCommandAndAddressFromRolandMessage(message)
            if command == command_dt1:
                normalized_address = tuple(self.edit_buffer.find_base_address(address))
                # Find out which data block we got
                for sub_request in range(len(self.edit_buffer.data_blocks)):
                    if normalized_address == self.edit_buffer.absolute_address(self.edit_buffer.data_blocks[sub_request].address):
                        return True, self._createFollowUpEditBufferDumpRequest(sub_request)
        return False

    @knobkraft_api
    def isEditBufferDump(self, messages):
        addresses = set()
        for message in knobkraft.sysex.findSysexDelimiters(messages):
            if self.isOwnSysex(messages[message[0]:message[1]]):
                _, address = self.getCommandAndAddressFromRolandMessage(messages[message[0]:message[1]])
                addresses.add(tuple(self.edit_buffer.find_base_address(address)))
        return all(a in addresses for a in self.edit_buffer.allowed_addresses)

    @knobkraft_api
    def convertToEditBuffer(self, channel, message):
        editBuffer = []
        if self.isEditBufferDump(message) or self.isSingleProgramDump(message):
            # We need to poke the device ID and the edit buffer address into the messages
            msg_no = 0
            for message in knobkraft.sysex.splitSysexMessage(message):
                command, address, data = self.parseRolandMessage(message)
                edit_buffer_address, _ = self.edit_buffer.address_and_size_for_sub_request(msg_no, 0x00)
                editBuffer = editBuffer + self.buildRolandMessage(self.device_id, command_dt1, edit_buffer_address, data)
                msg_no += 1
            return editBuffer
        raise Exception("Invalid argument given, can only convert edit buffers and program dumps to edit buffers")

    @knobkraft_api
    def createProgramDumpRequest(self, channel, patchNo):
        # Only RAM patches can be addressed directly ROM patches have to be loaded into the edit buffer via bank select program change
        # Determine the bank number for this patch
        bankNo = 0
        banks = self.bankDescriptors()
        sum = 0
        while bankNo < len(banks) and banks[bankNo]["size"] + sum <= patchNo:
            sum += banks[bankNo]["size"]
            bankNo += 1
        if "isROM" in banks[bankNo] and banks[bankNo]["isROM"]:
            # This is a ROM bank. We cannot address it via address data, we need to load the patch into the edit buffer
            programNo = patchNo - sum
            messages = self.bankSelect(channel, bankNo) + [0xc0 | (channel & 0x0f), programNo]
            return messages + self.createEditBufferRequest(channel)
        else:
            # This can be addressed directly
            address, size = self.program_dump.address_and_size_for_sub_request(0, patchNo % self.program_dump.num_items)
            return self.buildRolandMessage(self.device_id, command_rq1, address, size)

    def _createFollowUpProgramDumpRequest(self, patchNo, previousRequestNo):
        # Check if there is a follow up data block
        if previousRequestNo + 1 < len(self.program_dump.data_blocks):
            address, size = self.program_dump.address_and_size_for_sub_request(previousRequestNo + 1, patchNo % self.program_dump.num_items)
            return self.buildRolandMessage(self.device_id, command_rq1, address, size)
        else:
            return []

    @knobkraft_api
    def isPartOfSingleProgramDump(self, message):
        # Accept a certain set of addresses
        if self.isOwnSysex(message):
            command, address = self.getCommandAndAddressFromRolandMessage(message)
            if command == command_dt1:
                patchNo = self.program_dump.subaddress_from_address(address)
                normalized_address = tuple(self.program_dump.find_base_address(address))
                # Find out which data block we got
                for sub_request in range(len(self.program_dump.data_blocks)):
                    if normalized_address == self.program_dump.absolute_address(self.program_dump.data_blocks[sub_request].address):
                        return True, self._createFollowUpProgramDumpRequest(patchNo, sub_request)
        return False

    @knobkraft_api
    def isSingleProgramDump(self, messages):
        addresses = set()
        programs = set()
        for message in knobkraft.sysex.findSysexDelimiters(messages):
            _, address = self.getCommandAndAddressFromRolandMessage(messages[message[0]:message[1]])
            addresses.add(self.program_dump.find_base_address(address))
            programs.add(address[1])
        return len(programs) == 1 and all(a in addresses for a in self.program_dump.allowed_addresses)

    @knobkraft_api
    def convertToProgramDump(self, channel, message, program_number):
        programDump = []
        if self.isSingleProgramDump(message) or self.isEditBufferDump(message):
            # We need to poke the device ID and the program number into the messages
            msg_no = 0
            for message in knobkraft.sysex.splitSysexMessage(message):
                _, _, data = self.parseRolandMessage(message)
                program_buffer_address, _ = self.program_dump.address_and_size_for_sub_request(msg_no, program_number % self.program_dump.num_items)
                programDump = programDump + self.buildRolandMessage(self.device_id, command_dt1, program_buffer_address, data)
                msg_no += 1
            return programDump
        raise Exception("Can only convert single program dumps to program dumps!")

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
        # Use the prepared blank out zones to clear out a) program place and b) patch name
        if self.isEditBufferDump(message):
            return self._apply_blankout(message.copy(), self.edit_buffer.blank_out_zones)
        elif self.isSingleProgramDump(message):
            return self._apply_blankout(message.copy(), self.program_dump.blank_out_zones)
        raise "Only works with edit buffers and program dumps"

    @knobkraft_api
    def calculateFingerprint(self, message):
        return hashlib.md5(bytearray(self.blankedOut(message))).hexdigest()

    @knobkraft_api
    def numberFromDump(self, message) -> int:
        if not self.isSingleProgramDump(message):
            return 0
        messages = knobkraft.sysex.findSysexDelimiters(message, 1)
        _, address = self.getCommandAndAddressFromRolandMessage(message[messages[0][0]:messages[0][1]])
        return self.program_dump.subaddress_from_address(address)

    @knobkraft_api
    def nameFromDump(self, message) -> str:
        if self.isSingleProgramDump(message) or self.isEditBufferDump(message):
            messages = knobkraft.sysex.findSysexDelimiters(message, 1)
            _, _, data = self.parseRolandMessage(message[messages[0][0]:messages[0][1]])
            patch_name = ''.join([chr(x) for x in data[0:12]])
            return patch_name.strip()
        return 'Invalid'

    @knobkraft_api
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

    def install(self, module):
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        for a in dir(self):
            if callable(getattr(self, a)) and hasattr(getattr(self, a), "_is_knobkraft"):
                # this was helpful: http://stupidpythonideas.blogspot.com/2013/06/how-methods-work.html
                setattr(module, a, getattr(self, a))




_juno_ds_patch_data = [DataBlock_((0x00, 0x00, 0x00, 0x00), 0x50, "Patch common"),
                      DataBlock_((0x00, 0x00, 0x02, 0x00), (0x01, 0x11), "Patch common MFX"),
                      DataBlock_((0x00, 0x00, 0x04, 0x00), 0x54, "Patch common Chorus"),
                      DataBlock_((0x00, 0x00, 0x06, 0x00), 0x53, "Patch common Reverb"),
                      DataBlock_((0x00, 0x00, 0x10, 0x00), 0x29, "Patch common Tone Mix Table"),
                      DataBlock_((0x00, 0x00, 0x20, 0x00), (0x01, 0x1a), "Tone 1"),
                      DataBlock_((0x00, 0x00, 0x22, 0x00), (0x01, 0x1a), "Tone 2"),
                      DataBlock_((0x00, 0x00, 0x24, 0x00), (0x01, 0x1a), "Tone 3"),
                      DataBlock_((0x00, 0x00, 0x26, 0x00), (0x01, 0x1a), "Tone 4")]
_juno_ds_edit_buffer_addresses = RolandData_("Juno-DS Temporary Patch/Drum (patch mode part 1)", 1, 4, 4,
                                           (0x1f, 0x00, 0x00, 0x00),
                                           _juno_ds_patch_data)
_juno_ds_program_buffer_addresses = RolandData_("Juno-DS User Patches", 256, 4, 4,
                                              (0x30, 0x00, 0x00, 0x00),
                                              _juno_ds_patch_data)

juno_ds = GenericRoland_("Roland Juno-DS",
                        model_id=[0x00, 0x00, 0x3a],
                        address_size=4,
                        bank_descriptors=[{"bank": 0, "name": "User Patches 1", "size": 128, "type": "User Patch", "bank_msb": 87, "bank_lsb": 0},
                                          {"bank": 1, "name": "User Patches 2", "size": 128, "type": "User Patch", "bank_msb": 87, "bank_lsb": 1},
                                          {"bank": 2, "name": "Preset Patches 1", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 64, "isROM": True},
                                          {"bank": 3, "name": "Preset Patches 2", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 65, "isROM": True},
                                          {"bank": 4, "name": "Preset Patches 3", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 66, "isROM": True},
                                          {"bank": 5, "name": "Preset Patches 4", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 67, "isROM": True},
                                          {"bank": 6, "name": "Preset Patches 5", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 68, "isROM": True},
                                          {"bank": 7, "name": "Preset Patches 6", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 69, "isROM": True},
                                          {"bank": 8, "name": "Preset Patches 7", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 70, "isROM": True},
                                          {"bank": 9, "name": "Preset Patches 7", "size": 128, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 71, "isROM": True},
                                          {"bank": 10, "name": "Preset Patches 7", "size": 64, "type": "Preset Patch", "bank_msb": 87, "bank_lsb": 72, "isROM": True},
                                          {"bank": 11, "name": "DS Patches 1", "size": 128, "type": "DS Patch", "bank_msb": 87, "bank_lsb": 73, "isROM": True},
                                          {"bank": 12, "name": "DS Patches 2", "size": 56, "type": "DS Patch", "bank_msb": 87, "bank_lsb": 74, "isROM": True},
                                          {"bank": 13, "name": "EXP 04", "size": 50, "type": "Expansion Patch", "bank_msb": 93, "bank_lsb": 1, "isROM": True},
                                          {"bank": 14, "name": "EXP 06", "size": 128, "type": "Expansion Patch", "bank_msb": 93, "bank_lsb": 2, "isROM": True},
                                          ],
                        edit_buffer=_juno_ds_edit_buffer_addresses,
                        program_dump=_juno_ds_program_buffer_addresses,
                        category_index=0x0c,
                        device_family=[0x3a, 0x02, 0x02])
juno_ds.install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    return testing.TestData(device_detect_call="f0 7e 7f 06 01 f7",
                            device_detect_reply=("f0 7e 10 06 02 41 3a 02 02 00 00 03 00 00 f7", 0))


def test_program_dump():
    message = knobkraft.sysex.stringToSyx("f0 41 10 00 00 3a 12 30 7f 26 00 7f 40 40 00 40 40 00 40 01 00 00 00 7f 00 00 7f 7f 00 01 01 01 00 00 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 00 00 00 00 01 00 00 00 00 00 00 00 00 01 00 00 00 00 4a 40 40 40 40 40 28 50 28 00 40 22 5e 40 40 00 7f 40 01 40 00 40 40 01 40 40 40 40 00 0a 0a 40 00 7f 7f 7f 00 40 3c 03 01 60 40 40 40 00 0a 0a 0a 7f 7f 7f 01 05 0c 02 00 00 40 00 00 00 40 40 40 40 01 05 0c 02 00 00 40 00 00 00 40 40 40 40 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 3a f7")
    assert juno_ds.isPartOfSingleProgramDump(message)


def test_create_program_dump():
    # A low program number is a direct address request, because the user banks can be retrieved via memory address
    request = juno_ds.createProgramDumpRequest(1, 70)
    command, address, data = juno_ds.parseRolandMessage(request)
    assert command == 0x11
    assert address == [0x30, 70, 0x00, 0x00]

    # A higher program number points into a ROM bank, and will issue bank select messages, a program select, and then an edit buffer request
    request = juno_ds.createProgramDumpRequest(1, 300)
    assert request[0:3] == [0xb1, 0x00, 87]
    assert request[3:6] == [0xb1, 0x20, 64]
    assert request[6:8] == [0xc1, 300 - 256]
    command, address, data = juno_ds.parseRolandMessage(request[8:])
    assert command == 0x11
    assert address == [0x1f, 0x00, 0x00, 0x00]

    request = juno_ds.createProgramDumpRequest(1, 512)
    assert request[0:3] == [0xb1, 0x00, 87]
    assert request[3:6] == [0xb1, 0x20, 66]
    assert request[6:8] == [0xc1, 0]
    command, address, data = juno_ds.parseRolandMessage(request[8:])
    assert command == 0x11
    assert address == [0x1f, 0x00, 0x00, 0x00]
