# W, S and R have Bank A
# S and R have Bank D
# If expansion installed also Banks E and F
# W additionally for the Rom parts Bank B

# https://github.com/coniferprod/KSynthLib/blob/master/KSynthLib/K5000/SystemExclusive.cs

import struct
from typing import List, Dict, Any
from knobkraft.sysex import findSysexDelimiters

K5000_SPECIFIC_DEVICE = None

KawaiSysexID = 0x40
OneBlockDumpRequest = 0x00  # get one patch
AllBlockDumpRequest = 0x01
OneBlockDump = 0x20
AllBlockDump = 0x21

# Handshake stuff
WriteComplete = 0x40,
WriteError = 0x41,
WriteErrorByProtect = 0x42,
WriteErrorByMemoryFull = 0x44,
WriteErrorByNoExpandMemory = 0x45


def name():
    return "Kawai K5000"


def createDeviceDetectMessage(channel):  # ✅
    return [0xF0, KawaiSysexID, channel, 0x60, 0xF7]


def channelIfValidDeviceResponse(message):  # ✅
    global K5000_SPECIFIC_DEVICE
    # Check minimum length to avoid out-of-bounds errors
    if len(message) != 8:
        return -1

    # Verify Sysex header for Kawai K5000
    if (message[0] == 0xF0  # Start of SysEx
            and message[1] == KawaiSysexID  # Kawai manufacturer ID
            and 0x00 <= message[2] <= 0x0F  # Unit channel (0-F for ch 1-16)
            and message[3] == 0x61  # Fixed ID for this SysEx type
            and message[4] == 0x00  # Reserved, always 00
            and message[5] == 0x0A  # Specific function ID for ID request reply
            and message[7] == 0xF7):  # End of SysEx

        # Extract channel number (0-F, corresponding to 1-16 MIDI channels)
        channel = message[2]

        # Device ID mapping
        device_map = {
            0x01: "K5000W",
            0x02: "K5000S",
            0x03: "K5000R"
        }

        # Extract device type
        K5000_SPECIFIC_DEVICE = device_map.get(message[6], "Unknown Device")

        return channel

    return -1


def needsChannelSpecificDetection():  # ✅
    return True


def bankDescriptors() -> List[Dict]:
    global K5000_SPECIFIC_DEVICE

    if K5000_SPECIFIC_DEVICE is None:
        return []  # Prevent errors if called before detection

    base_banks = [(0x00, "A", 100)]  # "100" needs to be changed back to 128 once kk can do skipping

    if K5000_SPECIFIC_DEVICE == "K5000W":
        base_banks.append((0x01, "B", 128))  # ROMpler Bank for W

    if K5000_SPECIFIC_DEVICE in ["K5000S", "K5000R"]:
        base_banks.append((0x02, "D", 40))  # needs to be changed back to 128 once kk can do skipping
        base_banks.extend([(0x03, "E", 51), (0x04, "F", 128)])  # Expansion banks # needs to be changed back to 128 once kk can do skipping

    return [{
        "bank": b[0],
        "name": f"Bank {b[1]}",
        "size": b[2],
        "type": "Patch",
        "isROM": (b[1] == "B")  # Only ROM for K5000W Bank B
    } for b in base_banks]


def createEditBufferRequest(channel):
    # Not implemented - the Kawai K5000 can not be requested to send its edit buffer
    return []


def createProgramDumpRequest(channel, patchNo):  # ✅
    global K5000_SPECIFIC_DEVICE

    banks = bankDescriptors()
    total_patches = 0
    selected_bank = None
    patch_number = patchNo

    # Correct mapping of bank indexes to SysEx bank bytes
    bank_byte_map = {
        "A": 0x00,
        "D": 0x02,
        "E": 0x03,
        "F": 0x04
    }

    for bank in banks:
        if patch_number < bank["size"]:
            selected_bank = bank
            break
        patch_number -= bank["size"]

    if selected_bank is None:
        raise ValueError(f"Invalid patch number {patchNo}. Exceeds total patch count.")

    bank_name = selected_bank["name"].split()[-1]  # Extract the letter (A, D, E, F)
    bank_byte = bank_byte_map.get(bank_name, 0x00)  # Default to A if something goes wrong

    sysex_message = [
        0xF0, KawaiSysexID, channel, OneBlockDumpRequest, 0x00, 0x0A, 0x00, bank_byte, patch_number, 0xF7
    ]
    return sysex_message


def isSingleProgramDump(message):  # ✅
    # Check minimum length to avoid out-of-bounds errors
    if len(message) < 10:
        return False

    # Verify Sysex header for a Kawai K5000 program dump
    return (message[0] == 0xF0  # Start of SysEx
            and message[1] == KawaiSysexID  # Kawai manufacturer ID
            and 0x00 <= message[2] <= 0x0F  # MIDI channel (0-F for ch 1-16)
            and message[3] == OneBlockDump  # Function ID for single dump
            and message[4] == 0x00  # Reserved
            and message[5] == 0x0A
            and message[-1] == 0xF7)  # End of SysEx


def convertToProgramDump(channel, message, program_number):  # ❌❌ K5000 replies with [f0 40 00 41 00 0a f7] when sending from kk, which is "write error"
    if not isSingleProgramDump(message):
        raise Exception("Invalid message format - can't be converted")

    bank_number = program_number // 128  # Determine bank index
    patch_number = program_number % 128  # Determine patch within the bank

    # Determine correct bank byte based on the bank index
    bank_byte_map = {0: 0x00, 1: 0x02, 2: 0x03, 3: 0x04}

    if bank_number not in bank_byte_map:
        raise ValueError(f"Invalid bank number {bank_number}. Must be 0-3.")

    bank_byte = bank_byte_map[bank_number]

    # Reconstruct SysEx message with updated bank and program number
    return message[:7] + [bank_byte, patch_number] + message[8:]


def numberFromDump(message) -> int:  # where can I see if successful?
    if isSingleProgramDump(message):
        return message[8]
    raise Exception("Can extract number only from single program dump messages")


def nameFromDump(message) -> str:  # ✅
    if not isSingleProgramDump(message):
        raise Exception("Not a program dump")

    patch_name_start = 49  # Adjusted offset to skip the leading zero
    patch_name_length = 8  # Names are exactly 8 characters long

    patch_data = message[patch_name_start:patch_name_start + patch_name_length]

    # Replace non-printable or padding characters with spaces
    clean_name = ''.join(chr(c) if 32 <= c <= 126 else ' ' for c in patch_data)

    return clean_name.rstrip("\x7f ")  # Removes trailing DEL (0x7F) and spaces


def renamePatch(message, new_name):  # ✅
    if not isSingleProgramDump(message):
        raise Exception("Not a program dump")

    patch_name_start = 49  # Adjusted offset to skip the leading zero
    patch_name_length = 8  # Names are exactly 8 characters long
    patch_name_end = patch_name_start + patch_name_length

    # Ensure new name is exactly 8 characters, padded with spaces if needed
    new_name_bytes = new_name.ljust(patch_name_length)[:patch_name_length].encode('ascii', errors='ignore')

    # Replace the name bytes in the message
    new_message = message[:patch_name_start] + list(new_name_bytes) + message[patch_name_end:]

    return new_message


def createBankDumpRequest(channel, bank):  # ✅, BUT sends 4 requests due to time out
    return [0xF0, KawaiSysexID, channel, AllBlockDumpRequest, 0x00, 0x0A, 0x00, bank, 0x00, 0xF7]


def isPartOfBankDump(message):
    return (
            len(message) > 4
            and message[0] == 0xF0
            and message[1] == KawaiSysexID
            and 0x00 <= message[2] <= 0x0F
            and message[3] == AllBlockDump
            and message[5] == 0x0A
    )


def isBankDumpFinished(messages):
    return any(isPartOfBankDump(message) for message in messages)


# https://github.com/coniferprod/k5ktools/blob/main/bank.py
# https://github.com/coniferprod/KSynthLib/blob/master/KSynthLib/K5000/ToneMap.cs#L27
MAX_PATCH_COUNT = 128
TONE_COMMON_DATA_SIZE = 82
SOURCE_COUNT_OFFSET = 51
SOURCE_DATA_SIZE = 86
ADD_KIT_SIZE = 806
POOL_SIZE = 0x20000
MAX_SOURCE_COUNT = 6

# possible amount of sources and resulting file sizes (not used currently)
SINGLE_INFO = {
    254: (2, 0),
    340: (3, 0),
    426: (4, 0),
    512: (5, 0),
    598: (6, 0),
    1060: (1, 1),
    1146: (2, 1),
    1232: (3, 1),
    1318: (4, 1),
    1404: (5, 1),
    1866: (0, 2),
    1952: (1, 2),
    2038: (2, 2),
    2124: (3, 2),
    2210: (4, 2),
    2758: (0, 3),
    2844: (1, 3),
    2930: (2, 3),
    3016: (3, 3),
    3650: (0, 4),
    3736: (1, 4),
    3822: (2, 4),
    4542: (0, 5),
    4628: (1, 5),
    5434: (0, 6),
}


def parsePatchSizesFromBank(message: bytes) -> Dict[int, Dict[str, Any]]:  # ❌❌❌❌❌❌❌❌
    pointer_table = {}

    sysex_header_size = 8 + 19  # SysEx header (8 bytes) + Tone map (19 bytes)
    pointer_table_offset = sysex_header_size

    # Each pointer entry has 7 pointers, each 4 bytes (28 bytes per entry)
    for patch_index in range(MAX_PATCH_COUNT):
        entry_offset = pointer_table_offset + (patch_index * 28)
        entry = struct.unpack_from('>7I', message, entry_offset)

        tone_ptr = entry[0]
        source_ptrs = entry[1:]

        if tone_ptr != 0:
            pointer_table[patch_index] = {'tone': tone_ptr, 'sources': source_ptrs}

    # The high_ptr indicates the end of the patch data area
    high_ptr_offset = pointer_table_offset + MAX_PATCH_COUNT * 28
    high_ptr = struct.unpack_from('>I', message, high_ptr_offset)[0]

    if not pointer_table:
        raise ValueError("Pointer table is empty, invalid bank message")

    # Base pointer is the smallest tone pointer, used as offset reference
    base_ptr = min(entry['tone'] for entry in pointer_table.values())

    print(f"base_ptr calculated: {base_ptr}")

    for entry in pointer_table.values():
        entry['tone'] -= base_ptr
        entry['sources'] = tuple(src - base_ptr if src != 0 else 0 for src in entry['sources'])

    high_ptr -= base_ptr

    # Calculate the correct data region offset for actual patch data
    data_region_offset = high_ptr_offset + 4
    patch_data = message[data_region_offset:data_region_offset + POOL_SIZE]

    patch_info = {}
    sorted_pointers = sorted(pointer_table.items(), key=lambda item: item[1]['tone'])
    print(f"Pointer table has {len(sorted_pointers)} pointers")
    for idx, (index, entry) in enumerate(sorted_pointers):
        tone_ptr = entry['tone']

        print(f"Processing patch index {index}: tone_ptr={tone_ptr}")

        if tone_ptr + SOURCE_COUNT_OFFSET >= len(patch_data):
            print(f"Index out of range for patch {index}, tone_ptr={tone_ptr}, skipping")
            continue

        source_count = min(patch_data[tone_ptr + SOURCE_COUNT_OFFSET], MAX_SOURCE_COUNT)
        add_kit_count = sum(1 for src in entry['sources'] if src != 0)
        patch_size = TONE_COMMON_DATA_SIZE + (SOURCE_DATA_SIZE * source_count) + (ADD_KIT_SIZE * add_kit_count)

        # Confirm calculated patch size does not exceed data region bounds
        if tone_ptr + patch_size > len(patch_data):
            continue  # Skip invalid-sized patches

        patch_info[index] = {
            'offset': tone_ptr,
            'size': patch_size,
            'source_count': source_count,
            'add_kit_count': add_kit_count
        }

        print(f"Patch {index} info: offset={tone_ptr}, size={patch_size}, source_count={source_count}, add_kit_count={add_kit_count}")

    return patch_info


def extractPatchesFromAllBankMessages(messages, channel=None):  # ❌❌❌❌❌❌ does NOT work, problem either here or in parsePatchSizesFromBank, have tried at least 50 times
    patches = []
    message = messages[0] if isinstance(messages[0], list) else messages

    tone_map_data = message[8:27]
    tone_map = getToneMap(bytes(tone_map_data))

    # Pass the correct slice including pointer table and data
    bank_data = bytes(message[27:])  # Correct slice without re-adding header offset
    bank_patch_info = parsePatchSizesFromBank(bank_data)

    # Properly aligned data extraction without adding extra offset
    data_region_offset = (MAX_PATCH_COUNT * 7 * 4) + 4
    data = bank_data[data_region_offset:-1]

    for patch_index, included in enumerate(tone_map):
        if not included or patch_index not in bank_patch_info:
            continue

        info = bank_patch_info[patch_index]
        offset = info['offset']
        patch_size = info['size']

        if offset + patch_size > len(data):
            continue

        patch_data = data[offset: offset + patch_size]
        patch_sysex = [0xF0, 0x40, channel, 0x20, 0x00, 0x0A, 0x00, patch_index] + list(patch_data) + [0xF7]
        patches.append(patch_sysex)

    return patches





def getToneMap(data: bytes) -> List[bool]:  # ✅
    TONE_COUNT = 128
    DATA_SIZE = 19
    if len(data) != DATA_SIZE:
        raise ValueError("Invalid tone map size")

    bit_string = "".join(f"{byte:07b}"[::-1] for byte in data)
    return [bit == '1' for bit in bit_string[:TONE_COUNT]]




def toneMapToString(include: List[bool]) -> str:  # not used currently
    return ' '.join(str(i + 1) for i, included in enumerate(include) if included)


def toneMapCount(include: List[bool]) -> int:
    print(sum(include))
    return sum(include)


def toneMapEquals(map1: List[bool], map2: List[bool]) -> bool:  # not used currently
    return map1 == map2


def toneMapToData(include: List[bool]) -> bytes:  # not used currently
    bit_string = ''
    for i in range(len(include)):
        bit_string += '1' if include[i] else '0'
        if (i + 1) % 7 == 0:
            bit_string += '0'

    bit_string = bit_string[::-1]  # reverse string

    data = []
    for i in range(0, len(bit_string), 8):
        byte_str = bit_string[i:i + 8]
        data.append(int(byte_str, 2))

    return bytes(data)
