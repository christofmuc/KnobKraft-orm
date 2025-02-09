#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys
from typing import List

from knobkraft.template import SynthBase, BankDescriptor, EditBufferCapability

SYX_KORG = 0x42
SYX_KORG_DW6000 = 0x04
SYX_KORG_MESSAGE_DATA_SAVE_REQUEST = 0x10
SYX_KORG_MESSAGE_DATA_DUMP = 0x40


class Korg_DW6000(SynthBase, EditBufferCapability):

    def __init__(self):
        super().__init__("Korg DW-6000",
                         banks=[BankDescriptor(bank=0, name="Internal", size=64, isROM=False, type="Patch")],
                         needs_channel_specific_detection=False
                         )

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        # Page 5 of the service manual - Device ID Request
        # Different from the DW-8000, the DW-6000 does not differentiate which channel it is on via sysex
        return [0xf0, SYX_KORG, SYX_KORG_MESSAGE_DATA_DUMP, 0xf7]

    def channelIfValidDeviceResponse(self, message: List[int]) -> int:
        # Page 3 of the service manual - Device ID
        if (len(message) > 3
                and message[0] == 0xf0  # Sysex
                and message[1] == SYX_KORG  # Korg
                and (message[2] & 0xf0) == 0x30  # Device ID
                and message[3] == SYX_KORG_DW6000):
            # Sadly, there is no way to figure out which channel the Korg DW-6000 is set to (isn't there?)
            return message[2] & 0x0f
        return -1

    def createEditBufferRequest(self, channel: int) -> List[int]:
        # Page 5 - Data Save Request
        return [0xf0, SYX_KORG, 0x30, SYX_KORG_DW6000, SYX_KORG_MESSAGE_DATA_SAVE_REQUEST, 0xf7]

    def isEditBufferDump(self, message: List[int]) -> bool:
        # Page 3 - Data Dump
        return (len(message) > 4
                and message[0] == 0xf0
                and message[1] == SYX_KORG
                and (message[2] & 0xf0) == 0x30  # Format, ignore MIDI Channel in lower 4 bits
                and message[3] == SYX_KORG_DW6000
                and message[4] == SYX_KORG_MESSAGE_DATA_DUMP
                )

    def convertToEditBuffer(self, channel: int, message: List[int]) -> List[int]:
        if self.isEditBufferDump(message):
            return message[0:2] + [0x30 | channel] + message[3:]
        raise Exception("This is not a Korg DW-6000 edit buffer - can't be converted")


korg_dw6000 = Korg_DW6000()
korg_dw6000.install(sys.modules[__name__])
