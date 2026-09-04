#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import Iterable, List, Sequence


korg_id = 0x42


def createDeviceDetectMessage(channel: int = 0x7f) -> List[int]:
    return [0xf0, 0x7e, channel & 0x7f, 0x06, 0x01, 0xf7]


def korgChannelByte(channel: int) -> int:
    return 0x30 | (channel & 0x0f)


def split_14bit(value: int) -> List[int]:
    return [value & 0x7f, (value >> 7) & 0x7f]


def join_14bit(lsb: int, msb: int) -> int:
    return (lsb & 0x7f) | ((msb & 0x7f) << 7)


def buildMessage(channel: int, model_id: Sequence[int], command: int, data: Iterable[int] = ()) -> List[int]:
    return [0xf0, korg_id, korgChannelByte(channel)] + list(model_id) + [command] + list(data) + [0xf7]


def hasKorgHeader(message: List[int], model_id: Sequence[int], command: int = None) -> bool:
    header = [0xf0, korg_id]
    model = list(model_id)
    if len(message) < len(header) + 1 + len(model) + 2:
        return False
    if message[0:2] != header:
        return False
    if (message[2] & 0xf0) != 0x30:
        return False
    if message[3:3 + len(model)] != model:
        return False
    if command is not None and message[3 + len(model)] != command:
        return False
    return message[-1] == 0xf7


def channelIfValidDeviceResponse(
        message: List[int],
        family_id: Sequence[int],
        member_id: Sequence[int] = (0x00, 0x00)) -> int:
    if (len(message) >= 15
            and message[0] == 0xf0
            and message[1] == 0x7e
            and message[3] == 0x06
            and message[4] == 0x02
            and message[5] == korg_id
            and message[6:8] == list(family_id)
            and message[8:10] == list(member_id)
            and message[-1] == 0xf7):
        return message[2]
    return -1


def escapeSysex(data: Iterable[int]) -> List[int]:
    result = []
    data = list(data)
    data_index = 0
    while data_index < len(data):
        ms_bits = 0
        for i in range(7):
            if data_index + i < len(data):
                ms_bits |= (data[data_index + i] & 0x80) >> (7 - i)
        result.append(ms_bits)
        for i in range(7):
            if data_index + i < len(data):
                result.append(data[data_index + i] & 0x7f)
        data_index += 7
    return result


def unescapeSysex(sysex: Iterable[int]) -> List[int]:
    result = []
    sysex = list(sysex)
    data_index = 0
    while data_index < len(sysex):
        ms_bits = sysex[data_index]
        data_index += 1
        for i in range(7):
            if data_index < len(sysex):
                result.append(sysex[data_index] | ((ms_bits & (1 << i)) << (7 - i)))
            data_index += 1
    return result
