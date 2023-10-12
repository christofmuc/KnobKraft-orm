#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
from typing import List, Tuple, Optional, Dict, Union
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
    def size_to_number(size: Union[List, Tuple, int]) -> int:
        if isinstance(size, tuple) or isinstance(size, list):
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
        self._base_address = base_address
        self.base_address_int = DataBlock.size_to_number(base_address)
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
        return DataBlock.size_as_7bit_list(self.size * 8, self.num_size_bytes)  # Why times 8?. You can't cross border from one data set into the next

    def absolute_address(self, address: Tuple[int]) -> Tuple:
        address_int = DataBlock.size_to_number(address)
        return tuple(DataBlock.size_as_7bit_list(address_int + self.base_address_int, self.num_address_bytes))

    def address_and_size_for_sub_request(self, sub_request, sub_address) -> Tuple[List[int], List[int]]:
        # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
        sub_address_int = (sub_address << 14) if self.num_address_bytes == 4 else (sub_address << 7)
        concrete_address = DataBlock.size_as_7bit_list(DataBlock.size_to_number(self.data_blocks[sub_request].address) + sub_address_int + self.base_address_int, self.num_address_bytes)
        return concrete_address, DataBlock.size_as_7bit_list(self.data_blocks[sub_request].size, self.num_size_bytes)

    def subaddress_from_address(self, address: List[int]) -> int:
        offset = DataBlock.size_to_number(tuple(address)) - self.base_address_int
        return offset >> ((self.num_address_bytes - 2) * 7)

    def find_base_address(self, address: tuple) -> Tuple:
        int_value = DataBlock.size_to_number(address)
        sub_address = self.subaddress_from_address(list(address)) << 14
        return tuple(DataBlock.size_as_7bit_list(int_value - sub_address, self.num_address_bytes))


def knobkraft_api(func):
    func._is_knobkraft = True
    return func


class GenericRoland:
    def __init__(self, name: str, model_id: List[int], address_size: int, edit_buffer: RolandData, program_dump: RolandData,
                 category_index: Optional[int] = None,
                 device_family: Optional[List[int]] = None,
                 device_detect_message: Optional[RolandData] = None,
                 device_detect_ids: Optional[List[int]] = None):
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


class GenericRolandWithBackwardCompatibility:
    def __init__(self, main_model: GenericRoland, compatible_models: List[GenericRoland]):
        self.main_model = GenericRoland(main_model.name(), main_model.model_id, main_model.address_size, main_model.edit_buffer,
                                        main_model.program_dump,
                                        category_index=main_model.category_index,
                                        device_family=main_model.device_family,
                                        device_detect_message=main_model.device_detect_message,
                                        device_detect_ids=main_model.device_detect_ids)
        self.models_supported = [main_model] + compatible_models

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
        return self.main_model.channelIfValidDeviceResponse(message)

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
        model = self.model_from_message(message)
        return model.convertToEditBuffer(model.device_id, message)

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
        model = self.model_from_message(message)
        if model is not None:
            return model.convertToProgramDump(self.main_model.device_id, message, program_number)
        raise Exception("Can only convert edit buffers and program dumps of one of the compatible synths!")

    @knobkraft_api
    def numberFromDump(self, message) -> int:
        model = self.model_from_message(message)
        if model is not None:
            return model.numberFromDump(message)
        return 0

    @knobkraft_api
    def nameFromDump(self, message) -> str:
        model = self.model_from_message(message)
        if model is not None:
            return model.nameFromDump(message)
        return 'Invalid'

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
