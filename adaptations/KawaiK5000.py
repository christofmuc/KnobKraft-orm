# W, S and R have Bank A
# S and R have Bank D
# If expansion installed also Banks E and F
# W additionally for the Rom parts Bank B

# https://github.com/coniferprod/KSynthLib/blob/master/KSynthLib/K5000/SystemExclusive.cs

from typing import List, Dict

import testing

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


def extractPatchesFromAllBankMessages(messages):
    if not messages:
        raise ValueError("No messages received for bank dump.")

    bank_byte = messages[0][7] if len(messages[0]) > 7 else 0x00  # Ensure it's a valid integer

    # Flatten all messages into a single data array (excluding SysEx delimiters)
    all_data = []
    for message in messages:
        if not isPartOfBankDump(message):
            continue
        all_data.extend(message[8:-1])  # Remove SysEx header/footer

    # Extract tone map from the first 19 bytes
    tone_map_data = all_data[:19]
    tone_map = getToneMap(tone_map_data)

    # Extract available patch numbers from the tone map
    patch_numbers = [i for i, present in enumerate(tone_map) if present]

    patch_count = len(patch_numbers)  # Number of patches present in the dump
    print(f"Contains {patch_count} patches.")

    # Remaining patch data (skip tone map and padding)
    patch_data_start = 19
    patch_data = all_data[patch_data_start:]

    offset = 0
    patches = []

    for i, patch_number in enumerate(patch_numbers):
        if offset >= len(patch_data):
            print(f"Warning: Reached end of patch data unexpectedly at patch {i}.")
            break

        # Extract checksum (first byte of patch)
        checksum = patch_data[offset]
        offset += 1
        print(f"Checksum for patch {i+1} (Patch Number {patch_number}) = {checksum:02X}")

        # Remaining bytes to process
        bytes_left = len(patch_data) - offset

        # Extract patch data dynamically
        patch_body = patch_data[offset:offset + bytes_left]
        print(f"Patch data is from {offset} to {offset + bytes_left} ({len(patch_body)} bytes)")

        if len(patch_body) < 60:  # Ensure it's large enough to contain source info
            print(f"Error: Patch {i+1} is too small to be valid. Skipping.")
            continue

        # **Print the first 20 bytes of patch data for debugging**
        print(f"Patch {i+1} raw first 20 bytes: {' '.join(f'{b:02X}' for b in patch_body[:20])}")

        # **Improved Source Count Extraction**
        detected_source_offset = None
        for possible_offset in range(40, 80):  # Expand the search range
            if patch_body[possible_offset] <= 6:  # Valid source count should be 0-6
                detected_source_offset = possible_offset
                break

        if detected_source_offset is None:
            print(f"Error: Could not detect a valid source count offset for patch {i+1}. Skipping.")
            continue

        source_count = patch_body[detected_source_offset]  # Extract source count
        source_count = min(source_count, 6)  # Max sources should be 6
        print(f"Patch {i+1} detected {source_count} sources at offset {detected_source_offset}.")

        # **Check if it's PCM or ADD sources**
        is_additive = False  # Default to PCM synthesis

        if source_count > 0:
            # Ensure offset is valid before reading wave data
            if detected_source_offset + source_count < len(patch_body):
                wave_data = patch_body[detected_source_offset + 1: detected_source_offset + 1 + source_count]
                is_additive = any(wave_data)  # If any wave data is non-zero, it's additive

        # **Determine correct patch size**
        if is_additive:
            patch_size = TONE_COMMON_DATA_SIZE + (source_count * ADD_KIT_SIZE)
            print(f"Patch {i+1} is ADDITIVE with {source_count} sources. Calculated size: {patch_size}")
        else:
            patch_size = TONE_COMMON_DATA_SIZE + (source_count * SOURCE_DATA_SIZE)
            print(f"Patch {i+1} is PCM with {source_count} sources. Calculated size: {patch_size}")

        if patch_size <= 0 or patch_size > bytes_left:
            print(f"Error: Invalid patch size detected for patch {i+1}. Skipping.")
            continue

        # Extract only the correct patch size
        current_patch = patch_body[:patch_size]

        # Restore SysEx header/footer and insert the patch number
        formatted_patch = [
            0xF0, KawaiSysexID, 0x00, OneBlockDump, 0x00, 0x0A, 0x00, bank_byte, patch_number, checksum
        ] + list(current_patch) + [0xF7]

        # Debug: Print first bytes of patch
        print(f"Patch {i+1} first bytes: {' '.join(f'{byte:02X}' for byte in formatted_patch[:20])}")

        # Validate patch
        if not isSingleProgramDump(formatted_patch):
            print(f"Error: Extracted patch {i+1} is NOT a valid program dump. Skipping.")
            continue

        # Append the valid patch
        patches.append(formatted_patch)

        # Move offset forward correctly
        offset += patch_size
        print(f"Moving offset to {offset}")

    print(f"Extracted {len(patches)} valid patches.")
    return patches  # Return a list of lists (KnobKraft format)




def determinePatchSize(patch_data):
    """
    Determines the patch size dynamically by analyzing the structure.
    - Extracts the number of PCM and ADD sources.
    - Calculates the correct patch size.

    Returns (pcm_count, add_count, patch_size)
    """
    if len(patch_data) < 10:
        return 0, 0, 0  # Invalid patch

    # Extract the number of sources
    pcm_count = 0
    add_count = 0
    patch_size = 0

    # Extract source count offset (based on C# logic)
    source_count_offset = 51
    if len(patch_data) > source_count_offset:
        source_count = patch_data[source_count_offset]
        print(f"Detected {source_count} sources in patch.")

        # Determine PCM vs ADD sources
        for i in range(source_count):
            source_type_offset = source_count_offset + 1 + (i * 86)  # Each source is 86 bytes
            if len(patch_data) > source_type_offset:
                is_additive = patch_data[source_type_offset]  # Check if ADD type
                if is_additive:
                    add_count += 1
                else:
                    pcm_count += 1

    # Compute patch size based on extracted data
    if add_count > 0:
        patch_size = TONE_COMMON_DATA_SIZE + (add_count * ADD_KIT_SIZE)
    else:
        patch_size = TONE_COMMON_DATA_SIZE + (pcm_count * SOURCE_DATA_SIZE)

    return pcm_count, add_count, patch_size




def getToneMap(data: bytes) -> List[bool]:  # ✅    !!!PCM bank has no tone map
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


def make_test_data():
    def bankGenerator(test_data: testing.TestData) -> List[int]:
        yield test_data.all_messages

    return testing.TestData(sysex=R"testData/Kawai_K5000/full bank A midiOX K5000r.syx",
                            bank_generator=bankGenerator)

    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], number=1)

        return testing.TestData(sysex=R"testData/Kawai_K5000/single sound bank A patch 1.syx", program_generator=programs)
