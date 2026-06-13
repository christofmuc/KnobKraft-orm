"""
Yamaha SY/TG-series synth adaptation base class.

Created by github.com/hmmbug.

This base class provides a foundation for Knobcraft ORM adaptations for the
Yamaha SY/TG synth series. To create a synth-specific implementation:

Methods to add:
    convertPatchesToBankDump(self, patches: List[List[int]]) -> List[int]
    createCustomProgramChange(self, channel: int, patchNo: int) -> List[int]

Methods to override:
    friendlyProgramName(self, patchNo: int) -> str
    bankSlotToPatchNo(self, memory_type:int, memory_number: int) -> int
    _mapPatchNumToSynthMemory(self, patchno: int) -> tuple[int, int]

Data structures to define:
    - a MemoryType(IntEnum) class representing the INTERNAL/PRESET banks.
      This varies for each synth (example below)
    - a 'banks' dictionary. See the Knobcraft Adaption Programming Guide or
      the SY77 adaptation for examples.

Device detection may require overrides to other methods such as
`createDeviceDetectMessage()` and/or `channelIfValidDeviceResponse`.


## BULK DATA DUMP OVERVIEW

This series of synths have a (mostly) common SysEx format. Each message header
is of the format (hex):

    HEADER:
        f0 4d <device id> <synth id>
    where:
        f0: start of sysex
        4d: manufacturer id for Yamaha
        device id: the MIDI device id for SysEx messages. This is not the
            same as MIDI channel.
        synth id: a marker for (I guess) a hardware series or protocol
            version. For example 0x7a is used for SY55/77/85/99. It's unnamed
            in most of the MIDI Data Format manuals, but the SY55 manual
            refers to it as a "format number".

Next is a 10 character string in 3 parts:

    SUBHEADER (msg_id)
        xxxxyyyyzz
    where:
        xxxx is always 2 upper case characters plus spaces, usually "LM  "
            ("LM" + 2x spaces)
            Referred to as a "Classification Number" in some manuals.
        yyyy is a 4 digit decimal number
            Referred to as a "Data Format Name" in some manuals, but seemingly
            unrelated to the above "format number"
        zz is a 2 digit code for the data type (eg. voice, multi etc)

    Examples:
        SY22/SY35:  "PK  2203AE"
        TG33:       "LM  0012VE"
        SY55/TG55:  "LM  8103VC"
        SY77/TG77:  "LM  8101VC"
        SY85/TG500: "LM  0065VC"
        SY99:       "LM  8101VC" and "LM  0040VC"

Some synths also then have a 16 byte value, mostly null padding, sometimes with
memory type (Internal, Preset etc) and memory slot.

A message tail comprises:

    TAIL
    checksum:   a 1 byte checksum value based on a simple algorithm
                Only used for dump records, not for dump requests
    f0:         end of SysEx


## DEVICE DETECTION OVERVIEW

Mostly this can be accomplished with the SUBHEADER value in conjunction with
the name of PRESET sound. For example, the message formats of the SY77 and
TG77 are identical but the PRESET sounds differ.

There are some exceptions to this. The SY85 has no (read-only) PRESET banks
while the rack version of it (the TG500) does have PRESET banks.
"""

import hashlib
import logging
from enum import IntEnum
from types import ModuleType
from typing import List, Optional, Type

import knobkraft.sysex

# SysEx constants
SYSEX_START = 0xF0
SYSEX_END = 0xF7
YAMAHA_ID = 0x43
OFFSET_DEVICE_ID = 2  # offsets into SysEx msgs
OFFSET_CHECKSUM = -2  # penultimate byte
OFFSET_SIZE_BYTE1 = 4
OFFSET_SIZE_BYTE2 = OFFSET_SIZE_BYTE1 + 1
MESSAGE_TYPE_LENGTH = 10
OFFSET_MESSAGE_TYPE = 6
OFFSET_MESSAGE_TYPE_END = OFFSET_MESSAGE_TYPE + MESSAGE_TYPE_LENGTH

# Example MemoryType enum:
#
# class MemoryType(IntEnum):
#     INTERNAL = 0x00
#     PRESET1  = 0x02
#     PRESET2  = 0x03
#     EDIT     = 0x7F


class YamahaSYTGBase:
    def __init__(
        self,
        synth_name: str,
        synth_id: int,
        first_preset_name: str,  # Verbatim preset name. Used for device detection.
        # voice dump parameters
        msg_id_voice_dump: str,  # Used for device detection.
        voice_default_name: str,
        voice_name_length: int,
        offset_voice_name: int,
        # memory banks
        memory_types: Type[IntEnum],
        banks: List[dict],
        # offsets in sysex messages
        # these are the offsets in a bulk dump to where the memory_type
        # (INTERNAL, PRESET1 etc) and voice slot is specified
        offset_memory_type: int,
        offset_memory_number: int,
        # ...and the same but for a bulk dump request. The request messages
        # are slightly different because there's no byte count field in
        # requests
        offset_req_memory_type: int,
        offset_req_memory_number: int,
        # voice dump keyword params
        msg_id_all_voice_dump: Optional[str] = None,
        # misc
        help_string: str = "",
        needs_channel_specific_detection: bool = True,
    ) -> None:
        self.synth_name = synth_name
        self.synth_id = synth_id
        self.first_preset_name = first_preset_name

        # voice
        self.msg_id_voice_dump = msg_id_voice_dump  # request ID for single voice
        self.msg_id_all_voice_dump = msg_id_all_voice_dump  # request ID for voice bank
        self.voice_default_name = voice_default_name
        self.voice_name_length = voice_name_length
        self.offset_voice_name = offset_voice_name

        # banks
        self.memory_types = memory_types
        if banks is None:
            raise ValueError("'banks' is undefined.")
        self.banks = banks

        self.offset_memory_type = offset_memory_type
        self.offset_memory_number = offset_memory_number
        self.offset_req_memory_type = offset_req_memory_type
        self.offset_req_memory_number = offset_req_memory_number

        # misc
        self.help_string = help_string
        self.needs_channel_specific_detection = needs_channel_specific_detection

        # not user-configurable beyond here
        self.detected_device_id = None

    def name(self) -> str:
        return self.synth_name

    def setupHelp(self) -> str:
        return self.help_string

    # ##### DEVICE DETECTION CAPABILITIES

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        device_id = 0x20 | (channel & 0x0F)
        # Request PRESET1, voice 1 by default. Override this method if something different is needed.
        bank_name = (
            "PRESET1" if "PRESET1" in self.memory_types.__members__ else "PRESET"
        )
        msg = (
            str2bytes(self.msg_id_voice_dump)
            + [0x00] * 14
            + [int(self.memory_types[bank_name]), 0]
        )
        buf = self._makeMessage(device_id, msg, add_checksum=False)
        logging.debug(
            f"createDeviceDetectMessage({channel}: {bytes2str(buf[:32])} ... {bytes2str(buf[-2:])}"
        )
        return buf

    def channelIfValidDeviceResponse(self, buf: List[int]) -> int:
        if len(buf) < 32:
            return -1  # filter out irrelevant messages like active sensing
        if self._validateVoiceMessage(buf):
            if (
                self.nameFromDump(buf).strip() == self.first_preset_name.strip()
            ):  # ignore whitespace
                rtn = buf[OFFSET_DEVICE_ID]
                self.detected_device_id = rtn
            else:
                rtn = -1
        else:
            rtn = -1
        return rtn

    def needsChannelSpecificDetection(self) -> bool:
        return self.needs_channel_specific_detection

    # ##### PROGRAM DUMP CAPABILITY

    def createProgramDumpRequest(self, channel: int, patchNo: int) -> List[int]:
        device_id = 0x20 | (channel & 0x0F)
        memory_type, memory_number = self._mapPatchNumToSynthMemory(patchNo)
        msg = [
            *str2bytes(self.msg_id_voice_dump),
            *[0] * 14,
            int(memory_type),
            memory_number,
        ]
        # print(f"DEBUG ch:{channel} pN:{patchNo}", msg)
        buf = self._makeMessage(device_id, msg, add_checksum=False)
        logging.debug(
            f"createProgramDumpRequest({channel}, {patchNo}): {bytes2str(buf[:32])}"
        )
        return buf

    def isSingleProgramDump(self, buf: List[int]) -> bool:
        if self._validateVoiceMessage(buf):
            msg_size = self._getMessageSize(buf)
            rtn = msg_size == len(buf)
            logging.debug(
                f"isSingleProgramDump: msgSize:{msg_size} totalSize:{len(buf)} -> {rtn}"
            )
        else:
            rtn = False
            logging.debug(f"isSingleProgramDump: invalid msg: {buf}")
        return rtn

    def convertToProgramDump(
        self, channel: int, message: List[int], patchNo: int
    ) -> List[int]:
        buf = message.copy()
        buf[OFFSET_DEVICE_ID] = channel & 0x0F
        buf[self.offset_memory_type], buf[self.offset_memory_number] = (
            self._mapPatchNumToSynthMemory(patchNo)
        )
        buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
        logging.debug(
            f"convertToProgramDump({channel}, msg): "
            f"MemType/Num:{message[self.offset_memory_type]:02x}{message[self.offset_memory_number]:02x}"
            f"->{buf[self.offset_memory_type]:02x}{buf[self.offset_memory_number]:02x}"
        )
        return buf

    # ##### EDIT BUFFER CAPABILITIES

    def convertToEditBuffer(self, channel: int, msg: List[int]) -> List[int]:
        buf = msg.copy()
        buf[OFFSET_DEVICE_ID] = channel & 0x0F
        buf[self.offset_memory_type] = self.memory_types["EDIT"]
        buf[self.offset_memory_number] = 0
        buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
        logging.debug(
            f"convertToEditBuffer({channel}, msg): "
            f"MemType/Num:{msg[self.offset_memory_type]:02x}{msg[self.offset_memory_number]:02x}"
            f"->{buf[self.offset_memory_type]:02x}{buf[self.offset_memory_number]:02x}"
        )
        return buf

    def createEditBufferRequest(self, channel: int) -> List[int]:
        device_id = 0x20 | (channel & 0x0F)
        msg = [
            *str2bytes(self.msg_id_voice_dump),
            *[0] * 14,
            int(self.memory_types["EDIT"]),
            0,
        ]
        buf = self._makeMessage(device_id, msg, add_checksum=False)
        logging.debug(
            f"createEditBufferRequest({channel}: {bytes2str(buf[:32])} ... {bytes2str(buf[-2:])}"
        )
        return buf

    def isEditBufferDump(self, buf: List[int]) -> bool:
        rtn = self._validateVoiceMessage(buf) and (
            buf[self.offset_memory_type] == self.memory_types["EDIT"]
        )
        logging.debug(
            f"isEditBufferDump: MemType:{buf[self.offset_memory_type]:02x} "
            f"{buf[self.offset_memory_type] == self.memory_types['EDIT']} -> {rtn}"
        )
        return rtn

    # ##### BANK CAPABILITIES

    def bankDescriptors(self) -> List[dict]:
        return self.banks

    def isPartOfBankDump(self, first_message: List[int]) -> bool:
        return self._getMessageSize(first_message) < len(first_message)

    def isBankDumpFinished(self, message: List[List[int]]) -> bool:
        return False

    def friendlyProgramName(self, patchNo: int) -> str:
        raise NotImplementedError

    def friendlyBankName(self, bank_number: int) -> str:
        rtn = self.banks[bank_number]["name"]
        logging.debug(f"friendlyBankName(bank_number) -> {rtn}")
        return rtn

    # ##### SYSEX MESSAGE HELPERS

    def _makeMessage(
        self, device_id: int, msg: List[int], add_checksum: bool = True
    ) -> List[int]:
        buf = [SYSEX_START, YAMAHA_ID, device_id, self.synth_id] + msg
        if add_checksum:
            buf += [0, SYSEX_END]
            buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
        else:
            buf += [SYSEX_END]
        logging.debug(
            f"make_message({device_id}, msg={msg}, add_checksum={add_checksum}): {bytes2str(buf)}"
        )
        return buf

    def _calculateRawChecksum(self, data: List[int]) -> int:
        """
        Checksum the whole data array.
        """
        data_sum = sum(data) & 0x7F  # sum & mask to 7 bit values
        checksum = (
            0x80 - data_sum
        ) & 0x7F  # subtract from 128 (0x80), mask again for 7-bit value
        return checksum

    def _calculateChecksum(self, data: List[int]) -> int:
        """
        Checksum the SysEx array. This excludes the first 6 bytes (f0 43 0n, 7a + 2 byte data length).
        Returns the checksum byte to be inserted at OFFSET_CHECKSUM.
        """
        return self._calculateRawChecksum(data[6:-2])

    def _validateMessage(self, buf: List[int]) -> bool:
        if len(buf) < 32:
            return False
        csum = self._calculateChecksum(buf)
        rtn = (
            buf[0] == SYSEX_START
            and buf[1] == YAMAHA_ID
            and buf[3] == self.synth_id
            and buf[OFFSET_CHECKSUM] == csum
            and buf[-1] == SYSEX_END
        )
        logging.debug(
            f"validate_message: {bytes2str(buf[:32])} ... {bytes2str(buf[-2:])}"
            f" chk:{buf[-2]:02x} ?? ver:{csum:02x} --> {rtn}"
        )
        return rtn

    def _validateVoiceMessage(self, buf: List[int]) -> bool:
        rtn = self._validateMessage(buf) and self._getMessageType(buf) == str2bytes(
            self.msg_id_voice_dump
        )
        logging.debug(f"validate_voice_message: rtn:{rtn}")
        return rtn

    def _getMemoryType(self, buf: List[int]) -> int:
        if buf[self.offset_memory_type] not in self.memory_types:
            raise ValueError(f"Invalid Enum value '{buf[self.offset_memory_type]:02x}'")
        return self.memory_types(buf[self.offset_memory_type])

    def _getMemoryNumber(self, buf: List[int]) -> int:
        return buf[self.offset_memory_number]

    def _getMessageType(self, buf: List[int]) -> List[int]:
        return buf[OFFSET_MESSAGE_TYPE:OFFSET_MESSAGE_TYPE_END]

    def _nameFromDump(self, buf: List[int]) -> str:
        return "".join(
            chr(c)
            for c in buf[
                self.offset_voice_name : self.offset_voice_name + self.voice_name_length
            ]
        )

    def nameFromDump(self, buf: List[int]) -> str:
        rtn = self._nameFromDump(buf)
        logging.debug(f"nameFromDump: rtn:{rtn}")
        return rtn

    def numberFromDump(self, buf: List[int]) -> int:
        rtn = self.bankSlotToPatchNo(
            buf[self.offset_memory_type], buf[self.offset_memory_number]
        )
        logging.debug(
            f"numberFromDump: MemType/Num: {buf[self.offset_memory_type]}/{buf[self.offset_memory_number]} -> rtn:{rtn}"
        )
        return rtn

    def _getPayloadSize(self, buf: List[int]) -> int:
        "Returns payload size - the size field"
        if len(buf) > OFFSET_SIZE_BYTE2:
            return (buf[OFFSET_SIZE_BYTE1] << 7) + buf[OFFSET_SIZE_BYTE2]
        else:
            return 0

    def _getMessageSize(self, buf: List[int]) -> int:
        """
        Returns message size (inc. headers/tail):
        - header (4 bytes)
        - payload size field: 2 bytes
        - payload (variable)
        - tail (2 bytes: checksum and End of SysEx byte)
        For a single voice a simple `len(buf)` would work fine, but for banks of voices
        we need to use the size supplied in the message.
        """
        return 4 + 2 + self._getPayloadSize(buf) + 2

    def extractPatchesFromBank(self, bank: List[int]) -> List[int]:
        count = 0
        patches = []
        for patch in knobkraft.sysex.splitSysexMessage(bank):
            patches += patch
            count += 1
        logging.debug(f"extractPatchesFromBank(): {count} patches extracted")
        return patches

    def _mapPatchNumToSynthMemory(self, patchno: int) -> tuple[int, int]:
        "Maps Knobcraft flat patch numbers to synth memory types and banks"
        raise NotImplementedError

    def bankSlotToPatchNo(self, memory_type: int, memory_number: int) -> int:
        return -1  # Override in child class

    def _memoryTypeNumberChecks(self, memory_type: int, memory_number: int):
        if memory_type not in self.memory_types:
            raise ValueError(
                f"Invalid memory_type for dump request given: {memory_type}"
            )
        if not (0 <= memory_number <= 63):
            raise ValueError(
                f"Invalid memory_number for dump request given: {memory_number} (out of range)"
            )

    def isDefaultName(self, patchName: str) -> bool:
        rtn = patchName == self.voice_default_name
        logging.debug(f"isDefaultName({patchName}) -> {rtn}")
        return rtn

    def renamePatch(self, message, new_name) -> List[int]:
        if not self._validateVoiceMessage(message):
            raise ValueError("Not a Voice Dump")

        # truncate / space pad new_name
        fmt_name = [0x20] * self.voice_name_length
        for i, c in enumerate(new_name[: self.voice_name_length]):
            fmt_name[i] = ord(c)

        # build new message
        rtn = message.copy()
        for i in range(0, self.voice_name_length):
            rtn[self.offset_voice_name + i] = fmt_name[i]
        rtn[OFFSET_CHECKSUM] = self._calculateChecksum(rtn)
        logging.debug(f"renamePatch({new_name}): {self._nameFromDump(rtn)}")
        return rtn

    # ##### KNOBCRAFT FUNCTIONS

    def blankedOut(self, buf: List[int]) -> List[int]:
        buf[OFFSET_DEVICE_ID] = 0
        buf[self.offset_memory_type] = 0
        buf[self.offset_memory_number] = 0
        buf[OFFSET_CHECKSUM] = 0
        buf[
            self.offset_voice_name : self.offset_voice_name + self.voice_name_length
        ] = [0] * self.voice_name_length
        return buf

    def calculateFingerprint(self, buf: List[int]) -> str:
        rtn = buf.copy()
        rtn = self.blankedOut(rtn)
        return hashlib.md5(bytearray(rtn)).hexdigest()

    # ##### CLASS OPERATIONS METHODS

    def install(self, module: ModuleType) -> None:
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        # TODO Make this a loop
        for m in [
            "name",
            "setupHelp",
            # ##### DEVICE DETECTION CAPABILITIES
            "createDeviceDetectMessage",
            "channelIfValidDeviceResponse",
            "needsChannelSpecificDetection",
            # ##### PROGRAM DUMP CAPABILITY
            "createProgramDumpRequest",
            "isSingleProgramDump",
            "convertToProgramDump",
            # ##### EDIT BUFFER CAPABILITIES
            "convertToEditBuffer",
            "createEditBufferRequest",
            "isEditBufferDump",
            # ##### BANK CAPABILITIES
            "bankDescriptors",
            "isPartOfBankDump",
            "isBankDumpFinished",
            "friendlyBankName",
            # ##### SYSEX MESSAGE HELPERS
            "nameFromDump",
            "numberFromDump",
            "extractPatchesFromBank",
            "bankSlotToPatchNo",
            "isDefaultName",
            "renamePatch",
            # ##### KNOBCRAFT FUNCTIONS
            "blankedOut",
            "calculateFingerprint",
        ]:
            setattr(module, m, getattr(self, m))


def str2bytes(s: str) -> List[int]:
    return [ord(x) for x in s]


def bytes2str(b: List[int], delimiter: str = " ") -> str:
    return delimiter.join(f"{x:02x}" for x in b)
