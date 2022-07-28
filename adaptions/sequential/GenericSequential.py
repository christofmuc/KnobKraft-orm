#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib


# Documenting the Sequential/DSI device_IDs here for all sequential modules
#
# Evolver    - 0b00100000 0x20 (same as Poly Evolver and all other Evolvers)
# Prophet 08 - 0b00100011 0x23 or 0b00100100 0x24 for special edition
# Mopho      - 0x25 (used in f0f7)
# Tetra      - 0b00100110 0x26
# Mopho KB   - 0b00100111 0x27 (Mopho Keyboard and Mopho SE) or 0b00101001 0x29 (Mopho X4)
# Tempest    - 0x28 (according to Sequential forum)
# Prophet 12 - 0b00101010 0x2a or 0b00101011 0x2b for module/special edition?
# Pro 2      - 0b00101100 0x2c
# Prophet 6  - 0b00101101 0x2d
# OB 6       - 0b00101110 0x2e
# Rev 2      - 0b00101111 0x2f
# Prophet X  - 0b00110000 0x30
# Pro 3      - 0b00110001 0x31
# Prophet 5  - 0b00110010 0x32 (this is the Rev 4 of course)
# Take 5     -            0x35 (they left 2 empty - maybe the desktop Prophet 5 and...?)

class GenericSequential:

    def __init__(self, name, device_id, banks, patches_per_bank,
                 name_len=0,
                 name_position=0,
                 file_version=None,
                 id_list=None,
                 blank_out_zones=None,
                 friendlyBankName=None,
                 friendlyProgramName=None):
        self.__id = device_id
        self.__name = name
        if id_list is None:
            self.__id_list = [device_id]
        else:
            self.__id_list = id_list
        self.__banks = banks
        self.__patches_per_bank = patches_per_bank
        self.__name_len = name_len
        self.__name_position = name_position
        self.__file_version = file_version
        if blank_out_zones is None:
            self.__blank_out_zones = [(name_position, name_len)]
        else:
            self.__blank_out_zones = blank_out_zones + [(name_position, name_len)]
        self.friendly_bank_name = friendlyBankName
        self.friendly_program_name = friendlyProgramName

    def name(self):
        return self.__name

    def createDeviceDetectMessage(self, channel):
        # This is a sysex generic device detect message
        return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]

    def deviceDetectWaitMilliseconds(self):
        return 100

    def needsChannelSpecificDetection(self):
        return False

    def channelIfValidDeviceResponse(self, message):
        if (len(message) > 9
                and message[0] == 0xf0  # Sysex
                and message[1] == 0x7e  # Non-realtime
                and message[3] == 0x06  # Device request
                and message[4] == 0x02  # Device request reply
                and message[5] == 0x01  # Sequential / Dave Smith Instruments
                and message[6] in self.__id_list):
            # Family seems to be different, the Prophet 12 has (0x01, 0x00, 0x00) while the Evolver has (0, 0, 0)
            # and message[7] == 0x01  # Family MS is 1
            # and message[8] == 0x00  # Family member
            # and message[9] == 0x00):  # Family member
            # Extract the current MIDI channel from index 2 of the message
            if message[2] == 0x7f:
                # If the device is set to OMNI it will return 0x7f as MIDI channel - we use 1 for now which will work
                return 1
            elif message[2] == 0x10:
                # This indicates the device is in MPE mode (currently Prophet-6, OB-6),
                # and allocates channel 0-5 to the six voices
                return 0
            return message[2]
        return -1

    def createEditBufferRequest(self, channel):
        if self.__file_version is None:
            # Modern style
            return [0xf0, 0x01, self.__id, 0b00000110, 0xf7]
        else:
            # Evolver style
            return [0xf0, 0x01, self.__id, self.__file_version, 0b00000110, 0xf7]

    def isEditBufferDump(self, message):
        return (len(message) > 3
                and message[0] == 0xf0
                and message[1] == 0x01  # Sequential
                and message[2] in self.__id_list
                and (self.__file_version is None and message[3] == 0b00000011  # Edit Buffer Data
                     or message[3] == self.__file_version and message[4] == 0b00000011))  # Edit Buffer Data

    def numberOfBanks(self):
        return self.__banks

    def numberOfPatchesPerBank(self):
        return self.__patches_per_bank

    def createProgramDumpRequest(self, channel, patchNo):
        bank = patchNo // self.numberOfPatchesPerBank()
        program = patchNo % self.numberOfPatchesPerBank()
        if self.__file_version is None:
            # Modern style
            return [0xf0, 0x01, self.__id, 0b00000101, bank, program, 0xf7]
        else:
            # Evolver style
            return [0xf0, 0x01, self.__id, self.__file_version, 0b00000101, bank, program, 0xf7]

    def isSingleProgramDump(self, message):
        return (len(message) > 3
                and message[0] == 0xf0
                and message[1] == 0x01  # Sequential
                and message[2] in self.__id_list
                and (self.__file_version is None and message[3] == 0b00000010  # Program Data
                     or message[3] == self.__file_version and message[4] == 0b00000010))  # Program Data

    def nameFromDump(self, message):
        dataBlock = self.getDataBlock(message)
        if len(dataBlock) > 0:
            patchData = self.unescapeSysex(dataBlock)
            layer_a_name = ''.join(
                [chr(x) for x in patchData[self.__name_position:self.__name_position + self.__name_len]]).strip()
            return layer_a_name
        return "Invalid"

    def numberFromDump(self, message):
        if self.isEditBufferDump(message):
            return 0
        elif self.isSingleProgramDump(message):
            return message[4 + self.extraOffset()] * self.numberOfPatchesPerBank() + message[5 + self.extraOffset()]
        raise "Data is neither edit buffer nor program dump, can't extract number"

    def convertToEditBuffer(self, channel, message):
        if self.isEditBufferDump(message):
            return message
        elif self.isSingleProgramDump(message):
            # Have to strip out bank and program, and set command to edit buffer dump
            return message[0:3 + self.extraOffset()] + [0b00000011] + message[6 + self.extraOffset():]
        raise Exception("Neither edit buffer nor program dump - can't be converted")

    def convertToProgramDump(self, channel, message, program_number):
        bank = program_number // self.numberOfPatchesPerBank()
        program = program_number % self.numberOfPatchesPerBank()
        if self.isEditBufferDump(message):
            return message[0:3 + self.extraOffset()] + [0b00000010] + [bank, program] + message[4 + self.extraOffset():]
        elif self.isSingleProgramDump(message):
            return message[0:3 + self.extraOffset()] + [0b00000010] + [bank, program] + message[6 + self.extraOffset():]
        raise Exception("Neither edit buffer nor program dump - can't be converted")

    def friendlyBankName(self, bank):
        if self.friendly_bank_name is not None:
            return self.friendly_bank_name(bank)
        raise Exception("Program error - friendlyBankName not defined but code reached in GenericSequential module!")

    def calculateFingerprint(self, message):
        raw = self.getDataBlock(message)
        data = self.unescapeSysex(raw)
        # Blank out all blank out zones, normally this is the name (or layer names)
        for zone in self.__blank_out_zones:
            data[zone[0]:zone[0] + zone[1]] = [0] * zone[1]
        return hashlib.md5(bytearray(data)).hexdigest()  # Calculate the fingerprint from the cleaned payload data

    def renamePatch(self, message, new_name):
        header_len = self.headerLen(message)
        data = self.unescapeSysex(message[header_len:-1])
        for i in range(self.__name_len):
            data[self.__name_position + i] = ord(new_name[i]) if i < len(new_name) else ord(' ')
        return message[:header_len] + self.escapeSysex(data) + [0xf7]

    def getDataBlock(self, message):
        return message[self.headerLen(message):-1]

    def headerLen(self, message):
        if self.isEditBufferDump(message):
            return 4 + self.extraOffset()
        elif self.isSingleProgramDump(message):
            return 6 + self.extraOffset()
        else:
            raise Exception("Can only work on edit buffer or single program dumps")

    def extraOffset(self):
        return 0 if self.__file_version is None else 1

    @staticmethod
    def unescapeSysex(sysex):
        result = []
        dataIndex = 0
        while dataIndex < len(sysex):
            msbits = sysex[dataIndex]
            dataIndex += 1
            for i in range(7):
                if dataIndex < len(sysex):
                    result.append(sysex[dataIndex] | ((msbits & (1 << i)) << (7 - i)))
                dataIndex += 1
        return result

    @staticmethod
    def escapeSysex(data):
        result = []
        dataIndex = 0
        while dataIndex < len(data):
            ms_bits = 0
            for i in range(7):
                if dataIndex + i < len(data):
                    ms_bits = ms_bits | ((data[dataIndex + i] & 0x80) >> (7 - i))
            result.append(ms_bits)
            for i in range(7):
                if dataIndex + i < len(data):
                    result.append(data[dataIndex + i] & 0x7f)
            dataIndex += 7
        return result

    def install(self, module):
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        # TODO Make this a loop
        setattr(module, 'name', self.name)
        setattr(module, 'createDeviceDetectMessage', self.createDeviceDetectMessage)
        setattr(module, 'deviceDetectWaitMilliseconds', self.deviceDetectWaitMilliseconds)
        setattr(module, 'needsChannelSpecificDetection', self.needsChannelSpecificDetection)
        setattr(module, 'channelIfValidDeviceResponse', self.channelIfValidDeviceResponse)
        setattr(module, 'createEditBufferRequest', self.createEditBufferRequest)
        setattr(module, 'isEditBufferDump', self.isEditBufferDump)
        setattr(module, 'numberOfBanks', self.numberOfBanks)
        setattr(module, 'numberOfPatchesPerBank', self.numberOfPatchesPerBank)
        setattr(module, 'createProgramDumpRequest', self.createProgramDumpRequest)
        setattr(module, 'isSingleProgramDump', self.isSingleProgramDump)
        setattr(module, 'nameFromDump', self.nameFromDump)
        setattr(module, 'numberFromDump', self.numberFromDump)
        setattr(module, 'convertToEditBuffer', self.convertToEditBuffer)
        setattr(module, 'convertToProgramDump', self.convertToProgramDump)
        setattr(module, 'calculateFingerprint', self.calculateFingerprint)
        setattr(module, 'renamePatch', self.renamePatch)
        if self.friendly_bank_name is not None:
            setattr(module, 'friendlyBankName', self.friendlyBankName)
        if self.friendly_program_name is not None:
            setattr(module, 'friendlyProgramName', self.friendly_program_name)
