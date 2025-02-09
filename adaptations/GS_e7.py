from typing import List, Union

from knobkraft.template import SynthBase, BankDescriptor, EditBufferCapability

GS_MANUFACTURER_ID = [0x00, 0x21, 0x62]
GS_E7_ID = 0x01
GS_E7_MODEL_ID = 0x10

# Commands
READ_CONFIGURATION = 0x0c
READ_MEMORY = 0x0e
READ_SERIAL_NO = 0x20


class GSe7(SynthBase, ):

    def __init__(self):
        super().__init__("GS-e7", [BankDescriptor(bank=i, name=f"Bank {i+1}", size=128, type="Preset", isROM=False) for i in range(8)])
        self.last_read_address = -1

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        return GSe7._create_gs_message(READ_CONFIGURATION, [])

    def channelIfValidDeviceResponse(self, message: List[int]) -> int:
        if GSe7._is_sysex_of_size(message, 6):
            return message[1]
        return -1

    def bankSelect(self, channel: int, bank: int):
        pass

    def createEditBufferRequest(self, channel: int) -> List[int]:
        # The base address of the currently edited preset is 0x30800
        self.last_read_address = 0x30800
        return GSe7._read_memory(self.last_read_address)

    def isPartOfEditBufferDump(self, message: List[int]) -> Union[bool,(bool, List[int])]:
        # No chance to check if the message is really what we asked for
        if GSe7._is_sysex_of_size(message, 2*16+2):
            # No chance to check to generate the next message based on this one, we sadly need program state
            if 0x30800 <= self.last_read_address < 0x30800 + 128:
                self.last_read_address += 16
                if self.last_read_address < 0x30800 + 128:
                    # Need more data, we can only read 16 bytes at a time
                    next_edit_buffer_request = GSe7._read_memory(self.last_read_address)
                    return True, next_edit_buffer_request
                else:
                    # Done, this was the last message of the Preset
                    return True
            else:
                raise Exception(f"Invalid program state, last read address was not one of the edit buffer addresses: {self.last_read_address}")
        else:
            return False

    def isEditBufferDump(self, message: List[int]) -> bool:
        pass

    def convertToEditBuffer(self, channel: int, message: List[int]) -> List[int]:
        # Quote the manual: "To avoid any malfunction, the volatile memory cannot be
        # modified by system exclusive messages."
        # This makes the implementation of the edit buffer capability a bit useless.
        pass

    @staticmethod
    def _read_memory(address: int):
        return GSe7._create_gs_message(READ_MEMORY, [address & 0x7f, address >> 7 & 0x7f, address >> 14])

    @staticmethod
    def _create_gs_message(command: int, data: List[int]):
        return [0xf0] + GS_MANUFACTURER_ID + [GS_E7_ID, GS_E7_MODEL_ID, command] + data + [0xf7]

    @staticmethod
    def _is_sysex_of_size(message: List[int], size: int) -> bool:
        # The GS-e7 is peculiar in that it replies with non-marked sysex data. That is dangerous in a MIDI network.
        return len(message) == size and message[0] == 0xf0 and message[-1] == 0xf7

    #@staticmethod
    #def _is_own_sysex(message: List[int]) -> bool:
    #    return len(message) > 5 and message[1:6] == GS_MANUFACTURER_ID + [GS_E7_ID, GS_E7_MODEL_ID]

