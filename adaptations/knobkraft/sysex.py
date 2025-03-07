#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import List, Tuple
import binascii


def load_sysex(filename, as_single_list=False) -> List[List[int]]:
    with open(filename, mode="rb") as midi_messages:
        content = midi_messages.read()
        if as_single_list:
            return list(content)
        messages = []
        start_index = 0
        for index, byte in enumerate(content):
            if byte == 0xf7:
                messages.append(list(content[start_index:index + 1]))
                start_index = index + 1
        return messages


def splitSysexMessage(messages):
    result = []
    start = 0
    for read in range(len(messages)):
        if messages[read] == 0xf0:
            start = read
        elif messages[read] == 0xf7:
            result.append(messages[start:read + 1])
    return result


def stringToSyx(string):
    return list(binascii.unhexlify(string.replace(' ', '')))


def findSysexDelimiters(messages, max_no=None) -> List[Tuple[int, int]]:
    result = []
    start = 0
    for read in range(len(messages)):
        if messages[read] == 0xf0:
            start = read
        elif messages[read] == 0xf7:
            result.append((start, read + 1))
            if max_no is not None and len(result) >= max_no:
                # Early abort when we only want the first max_no messages
                return result
    return result


def splitSysex(byte_list):
    result = []
    index = 0
    while index < len(byte_list):
        sysex = []
        if byte_list[index] == 0xf0:
            # Sysex start
            while byte_list[index] != 0xf7 and index < len(byte_list):
                sysex.append(byte_list[index])
                index += 1
            sysex.append(0xf7)
            index += 1
            result.append(sysex)
        else:
            #print("Skipping invalid byte", )
            result.append([byte_list[index]])
            index += 1
    return result


def unescapeSysex_deepmind(sysex):
    # This implements the algorithm defined on page 141 of the Deepmind user manual. I think it is the same as DSI uses
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


def denibble_hi_then_lo(message):
    return [message[x+1] | (message[x] << 4) for x in range(0, len(message), 2)]


def denibble_lo_then_hi(message):
    return [message[x] | (message[x + 1] << 4) for x in range(0, len(message), 2)]


def nibble(message):
    result = []
    for b in message:
        result.append(b & 0x0f)
        result.append((b & 0xf0) >> 4)
    return result


def to_hex_str(data: List[int]):
    return ' '.join([f'{i:02X}' for i in data])