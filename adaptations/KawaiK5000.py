# W, S and R have Bank A
# S and R have Bank D
# If expansion installed also Banks E and F
# W additionally for the Rom parts Bank B
# lots of inspiration from:
# https://github.com/coniferprod/KSynthLib/blob/master/KSynthLib/K5000/SystemExclusive.cs
# and https://github.com/coniferprod/KSynthLib/blob/master/Driver/Program.cs#L137

# Adaptation written by Markus Schlösser
from copy import copy
from typing import List, Dict

import testing
import hashlib

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
    """Returns a list of available banks based on the detected K5000 model."""
    global K5000_SPECIFIC_DEVICE

    if K5000_SPECIFIC_DEVICE is None:
        return []  # Prevent errors if called before detection

    # Base bank configurations
    base_banks = [
        {"id": 0x00, "name": "A", "size": 128}  # Adjust to 128 once skipping is implemented
    ]

    if K5000_SPECIFIC_DEVICE == "K5000W":
        base_banks.append({"id": 0x01, "name": "B", "size": 128})  # ROMpler Bank for W

    if K5000_SPECIFIC_DEVICE in ["K5000S", "K5000R"]:
        base_banks.append({"id": 0x02, "name": "D", "size": 128})  # Adjust to 128 once skipping is implemented
        base_banks.extend([
            {"id": 0x03, "name": "E", "size": 128},
            {"id": 0x04, "name": "F", "size": 128}
        ])  # Expansion banks

    return [
        {
            "bank": bank["id"],
            "name": f"Bank {bank['name']}",
            "size": bank["size"],
            "type": "Patch",
            "isROM": bank.get("isROM", False)
        }
        for bank in base_banks
    ]


def createEditBufferRequest(channel):
    # Not implemented - the Kawai K5000 can not be requested to send its edit buffer
    return []


def createProgramDumpRequest(channel: int, patchNo: int) -> List[int]:
    """
    Creates a SysEx message to request a program dump for a given patch number.

    Parameters:
    - channel (int): MIDI channel (0-15)
    - patchNo (int): Patch number (0-based index across all banks)

    Returns:
    - List[int]: SysEx message bytes
    """
    banks = bankDescriptors()
    patch_number = patchNo
    selected_bank = None

    # Identify the correct bank and adjust the patch number accordingly
    for bank in banks:
        if patch_number < bank["size"]:
            selected_bank = bank
            break
        patch_number -= bank["size"]

    if selected_bank is None:
        raise ValueError(f"Invalid patch number {patchNo}. Exceeds total patch count.")

    # Extract the correct SysEx bank byte
    bank_byte = selected_bank["bank"]

    # Construct SysEx message
    return [
        0xF0, KawaiSysexID, channel, OneBlockDumpRequest, 0x00, 0x0A, 0x00, bank_byte, patch_number, 0xF7
    ]


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


def convertToProgramDump(channel: int, message: List[int], program_number: int) -> List[int]:
    """
    Converts a received program dump into a properly formatted SysEx message,
    ensuring the correct bank and patch number assignment based on `bankDescriptors`.

    Parameters:
    - channel (int): MIDI channel (0-15)
    - message (List[int]): Incoming SysEx message bytes.
    - program_number (int): Global patch number across all banks.

    Returns:
    - List[int]: Modified SysEx message with updated bank and patch number.

    Raises:
    - Exception: If the message is not a valid single program dump.
    - ValueError: If the calculated bank number is out of range.
    """
    if not isSingleProgramDump(message):
        raise Exception("Invalid message format - can't be converted")

    # Get dynamic bank information from bankDescriptors()
    banks = bankDescriptors()

    # Determine correct bank and patch number
    patch_number = program_number
    selected_bank = None

    for bank in banks:
        if patch_number < bank["size"]:
            selected_bank = bank
            break
        patch_number -= bank["size"]

    if selected_bank is None:
        raise ValueError(f"Invalid program number {program_number}. Exceeds available patches.")

    # Extract bank byte dynamically
    bank_byte = selected_bank["bank"]

    # Construct the modified SysEx message
    modified_message = message[:7] + [bank_byte, patch_number] + message[9:]

    return modified_message


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


def createBankDumpRequest(channel: int, bank_identifier) -> List[int]:
    """
    Creates a SysEx message to request a full bank dump.

    Parameters:
    - channel (int): MIDI channel (0-15)/Sysex ID.
    - bank_identifier (Union[str, int]): Either the bank name (e.g., "A", "D") or the bank ID (e.g., 0, 2).

    Returns:
    - List[int]: SysEx message bytes.

    Raises:
    - ValueError: If the bank name or ID is not found in `bankDescriptors()`.
    """
    banks = bankDescriptors()

    # Determine whether we received a name ("A", "D") or an ID (0x00, 0x02)
    if isinstance(bank_identifier, int):  # If an integer is passed, it's an ID
        selected_bank = next((bank for bank in banks if bank["bank"] == bank_identifier), None)
    else:  # If a string is passed, it's a bank name
        selected_bank = next((bank for bank in banks if bank["name"].split()[-1] == str(bank_identifier)), None)

    if selected_bank is None:
        raise ValueError(f"Invalid bank identifier '{bank_identifier}'. Available banks: {[bank['name'] for bank in banks]}")

    bank_id = selected_bank["bank"]  # Get the correct bank ID

    return [0xF0, KawaiSysexID, channel, AllBlockDumpRequest, 0x00, 0x0A, 0x00, bank_id, 0x00, 0xF7]


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
    """
    Extracts individual patches from a bank dump.

    Parameters:
    - messages (List[List[int]]): List of SysEx messages forming a bank dump.

    Returns:
    - List[List[int]]: Extracted patch SysEx messages.
    """
    if not messages:
        raise ValueError("No messages received for bank dump.")

    # Get dynamic bank information
    banks = bankDescriptors()

    # Determine bank ID from the first message (ensuring it is valid)
    received_bank_byte = messages[0][7] if len(messages[0]) > 7 else None
    selected_bank = next((b for b in banks if b["bank"] == received_bank_byte), None)

    if selected_bank is None:
        raise ValueError(f"Invalid bank byte {received_bank_byte}. No matching bank found.")

    bank_byte = selected_bank["bank"]

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

    patch_count = len(patch_numbers)
    print(f"Contains {patch_count} patches in bank {selected_bank['name']}.")

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
        print(f"Checksum for patch {i+1} (Patch Number {patch_number}) = {checksum:02X}")
        offset += 1  # ✅ Move past checksum

        # Remaining bytes to process
        bytes_left = len(patch_data) - offset

        # Extract patch data dynamically
        patch_body = patch_data[offset:offset + bytes_left]

        if len(patch_body) < 60:  # Ensure it's large enough to contain source info
            print(f"Error: Patch {i+1} is too small to be valid. Skipping.")
            continue

        # Extract source count (from Common Data byte 50)
        source_count_offset = 50
        source_count = min(patch_body[source_count_offset], 6)  # Max 6 sources

        # ---- PCM/ADD Classification Per Source ----
        add_count = 0
        pcm_count = 0
        source_type_offset = TONE_COMMON_DATA_SIZE + 27  # First wave type

        for s in range(source_count):
            source_offset = source_type_offset + (s * SOURCE_DATA_SIZE)

            if source_offset + 1 >= len(patch_body):  # Avoid out-of-bounds access
                pcm_count += 1
                continue

            # Extract MSB and LSB
            wave_msb = patch_body[source_offset]
            wave_lsb = patch_body[source_offset + 1]
            wave_type = ((wave_msb & 0b111) << 7) | wave_lsb  # Correctly extract wave type

            is_add = (wave_type == 512)  # ✅ Only wave_type 512 is ADD

            if is_add:
                add_count += 1
            else:
                pcm_count += 1

            print(f"Patch {i+1} Source {s+1}: Wave MSB: {wave_msb:02X}, LSB: {wave_lsb:02X} -> Type: {wave_type} -> {'ADD' if is_add else 'PCM'}")

        # Debug: Total ADD vs PCM count
        print(f"Patch {i + 1}: {add_count} ADD, {pcm_count} PCM")

        # ---- Get Patch Size from SINGLE_INFO ----
        patch_size = None
        for size, (pcm, add) in SINGLE_INFO.items():
            if pcm == pcm_count and add == add_count:
                patch_size = size
                break

        if patch_size is None:
            print(
                f"Error: Could not determine patch size for {pcm_count} PCM, {add_count} ADD sources in Patch {i + 1}. Skipping.")
            continue

        if patch_size <= 0 or patch_size > bytes_left + 1:
            print(
                f"Error: Invalid patch size detected for patch {i + 1}. Skipping. Expected {patch_size}, but only {bytes_left} remain.")
            continue

        # ---- Extract only the correct patch size ----
        current_patch = patch_body[:patch_size - 1]  # ✅ Exclude last byte (extra checksum)

        # Restore SysEx header/footer and insert the patch number
        formatted_patch = [
            0xF0, KawaiSysexID, 0x00, OneBlockDump, 0x00, 0x0A, 0x00, bank_byte, patch_number, int(checksum)
        ] + list(current_patch) + [0xF7]

        if not isSingleProgramDump(formatted_patch):
            print(f"Error: Extracted patch {i+1} is NOT a valid program dump. Skipping.")
            continue

        patches.append(formatted_patch)

        # Move offset forward correctly
        offset += patch_size - 1  # ✅ Ensure correct alignment for next patch

    print(f"Extracted {len(patches)} valid patches from bank {selected_bank['name']}.")
    return patches


def getToneMap(data: bytes) -> List[bool]:  # ✅    !!!PCM bank has no tone map, don't yet, what to do with it
    TONE_COUNT = 128
    DATA_SIZE = 19
    if len(data) != DATA_SIZE:
        raise ValueError("Invalid tone map size")

    bit_string = "".join(f"{byte:07b}"[::-1] for byte in data)
    return [bit == '1' for bit in bit_string[:TONE_COUNT]]


def calculateFingerprint(message: List[int]):
    if isSingleProgramDump(message):
        patch_name_start = 49  # Adjusted offset to skip the leading zero
        patch_name_length = 8  # Names are exactly 8 characters long
        blanked_out = copy(message)
        blanked_out[patch_name_start:patch_name_start + patch_name_length] = [ord(" ")] * patch_name_length
        return hashlib.md5(bytearray(blanked_out[10:-1])).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint Presets")





def make_test_data():
    global K5000_SPECIFIC_DEVICE

    def bankGenerator(test_data: testing.TestData) -> List[int]:
        yield test_data.all_messages

    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        program_buffers = extractPatchesFromAllBankMessages(data.all_messages)
        yield testing.ProgramTestData(program_buffers[0], number=0, name="PowerK5K")
        yield testing.ProgramTestData(program_buffers[1], number=1, name="PowerBas")
        yield testing.ProgramTestData(program_buffers[-1], number=97, name="Boreal")

    K5000_SPECIFIC_DEVICE = 0x01
    return testing.TestData(sysex=R"testData/Kawai_K5000/full bank A midiOX K5000r.syx",
                            bank_generator=bankGenerator,
                            program_generator=programs,
                            device_detect_call=[0xF0, KawaiSysexID, 0, 0x60, 0xF7])
