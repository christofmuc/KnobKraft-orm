#   Korg Triton Classic – Program mode adaption
from typing import List
import knobkraft
import testing

# ---------------- Constants ----------------

KORG = 0x42
TRITON_SERIES = 0x50
TRITON_CLASSIC = 0x05  # Only used for device identity, NOT in operational messages!

# Request functions
MODE_REQUEST = 0x12
CURRENT_PROGRAM_DUMP_REQUEST = 0x10
PROGRAM_BANK_DUMP_REQUEST = 0x1C

# Response functions
MODE_CHANGE = 0x4E
CURRENT_PROGRAM_DUMP = 0x40
PROGRAM_BANK_DUMP = 0x4C
PARAMETER_CHANGE = 0x41
DATA_DUMP_ERROR = 0x24

# Per TABLE 1 in MIDI implementation: PCM programs are 540 bytes (0-539)
PROGRAM_DATA_SIZE = 540


# ---------------- Metadata ----------------

def name():
    return "Korg Triton Classic"


# ---------------- Device detection ----------------

def createDeviceDetectMessage(channel):
    return [0xF0, 0x7E, channel & 0x0F, 0x06, 0x01, 0xF7]


def deviceDetectWaitMilliseconds():
    return 300


def generalMessageDelay():
    # Triton is slow - give it time
    return 1000


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    if (
        len(message) >= 15
        and message[0] == 0xF0
        and message[1] == 0x7E
        and message[3] == 0x06
        and message[4] == 0x02
        and message[5] == 0x42      # KORG
        and message[6] == 0x50      # Triton series
        and message[7] == 0x00
        and message[8] == 0x05      # Triton Classic (model ID only here!)
        and message[9] == 0x00
        and message[14] == 0xF7
    ):
        return message[2]   # IMPORTANT: unmasked
    return -1


# ---------------- Edit buffer ----------------

def createEditBufferRequest(channel):
    """
    Request current program parameter dump (edit buffer)
    
    Format: F0, 42, 3g, 50, 10, 00, F7
    """
    return [
        0xF0,
        KORG,                       # 0x42
        0x30 | (channel & 0x0F),    # 3g where g = channel
        TRITON_SERIES,              # 0x50
        CURRENT_PROGRAM_DUMP_REQUEST,  # 0x10 (5th byte)
        0x00,                       # Reserved
        0xF7
    ]


def isEditBufferDump(message):
    """
    Check if message is a current program dump
    Expected format: F0 42 3g 50 40 0t [data] F7
    """
    if not message or message[0] != 0xF0 or len(message) < 7:
        return False
    
    return (
        message[1] == KORG
        and (message[2] & 0xF0) == 0x30
        and message[3] == TRITON_SERIES
        and message[4] == CURRENT_PROGRAM_DUMP  # 0x40 in 5th byte position
    )


def nameFromDump(message):
    """
    Extract program name from dump
    Format: F0 42 3g 50 40 0t [escaped_data] F7
    """
    if not message or len(message) < 8:
        return "Invalid"
    
    if message[4] == CURRENT_PROGRAM_DUMP:  # 0x40
        try:
            # Skip: F0(1) 42(1) 3g(1) 50(1) 40(1) 0t(1) = 6 bytes
            data = unescapeSysex(message[6:-1])
            # Program names are 16 chars at the start
            if len(data) >= 16:
                return ''.join(chr(x) if 32 <= x < 127 else ' ' for x in data[0:16])
        except:
            pass
    
    return "Unknown"


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return (
            message[0:2]
            + [0x30 | (channel & 0x0F)]
            + message[3:]
        )
    raise Exception("Not an edit buffer dump")


# ---------------- Bank handling ----------------

def bankDescriptors():
    return [
        {"bank": 0x00, "name": "INT-A", "size": 128, "type": "Patch"},
        {"bank": 0x01, "name": "INT-B", "size": 128, "type": "Patch"},
        {"bank": 0x02, "name": "INT-C", "size": 128, "type": "Patch"},
        {"bank": 0x03, "name": "INT-D", "size": 128, "type": "Patch"},
    ]


def createBankDumpRequest(channel, bank):
    """
    Request program parameter dump for one bank (Program mode)

    Format: F0 42 3g 50 1C 00kk0bbb 0ppppppp 00 F7
    
    k = 0 : All Programs
        1 : 1 Bank Programs (Use b)
        2 : 1 Program (Use b & pp)
    b = 0-5 : Bank A-F
    """
    kind = 0x01            # 1 Bank Program
    kind_bits = (kind & 0x03) << 4   # bits 5–4
    bank_bits = bank & 0x07           # bits 2–0

    return [
        0xF0,
        KORG,
        0x30 | (channel & 0x0F),
        TRITON_SERIES,
        PROGRAM_BANK_DUMP_REQUEST,    # 0x1C
        kind_bits | bank_bits,         # 00kk0bbb
        0x00,                          # program number (ignored for bank)
        0x00,                          # reserved
        0xF7
    ]


def isPartOfBankDump(message):
    """
    Check if message is a bank dump
    The Triton sends ONE large 0x4C message containing all programs
    """
    if not message or message[0] != 0xF0 or len(message) < 8:
        return False
    
    return (
        message[1] == KORG and
        (message[2] & 0xF0) == 0x30 and
        message[3] == TRITON_SERIES and
        message[4] == PROGRAM_BANK_DUMP  # 0x4C
    )


def isBankDumpFinished(messages):
    """
    Check if we received a complete bank dump
    
    The Triton sends ONE large 0x4C message with all 128 programs,
    terminated by F7.
    """
    for msg in messages:
        if (len(msg) > 8 and 
            msg[0] == 0xF0 and
            msg[1] == KORG and 
            (msg[2] & 0xF0) == 0x30 and
            msg[3] == TRITON_SERIES and 
            msg[4] == PROGRAM_BANK_DUMP and
            msg[-1] == 0xF7):
            # Found a complete bank dump message
            return True
    
    return False


def extractPatchesFromBank(message):
    """
    Extract individual patches from bank dump message
    
    The Triton sends one 0x4C message for a bank:
    F0 42 3g 50 4C [000v] [00kk0bbb] [0ppp] [escaped_data_for_all_programs] F7
    
    According to TABLE 1 in the MIDI implementation, each PCM program is
    exactly 540 bytes (bytes 0-539). The programs are concatenated directly
    in the unescaped data with NO headers between them.
    
    Strategy:
    1. Unescape the entire data block
    2. Split into 540-byte chunks
    3. Re-escape each chunk and create individual 0x40 messages
    """
    if not isPartOfBankDump(message):
        raise Exception("Not a Triton program bank dump")
    
    channel = message[2] & 0x0F
    
    # The message structure is:
    # F0 42 3g 50 4C [byte5] [byte6] [byte7] [escaped_data...] F7
    # We need to unescape starting from byte 8
    escaped_data = message[8:-1]  # Skip header and F7
    unescaped_data = unescapeSysex(escaped_data)
    
    # Calculate how many programs we have
    num_programs = len(unescaped_data) // PROGRAM_DATA_SIZE
    
    programs = []
    
    for prog_num in range(num_programs):
        # Extract this program's data (540 bytes)
        start = prog_num * PROGRAM_DATA_SIZE
        end = start + PROGRAM_DATA_SIZE
        program_data = unescaped_data[start:end]
        
        # Create an edit buffer dump message for this program
        # F0 42 3g 50 40 0t [escaped_data] F7
        prog_type = 0x00  # PCM type (0x00), MOSS would be 0x01
        
        escaped_prog_data = escapeSysex(program_data)
        
        prog_dump = (
            [0xF0, KORG, 0x30 | channel, TRITON_SERIES, CURRENT_PROGRAM_DUMP, prog_type]
            + escaped_prog_data
            + [0xF7]
        )
        
        programs.append(prog_dump)
    
    # Return all programs concatenated
    result = []
    for prog in programs:
        result.extend(prog)
    
    return result


# ---------------- Sysex helpers ----------------

def unescapeSysex(sysex: List[int]) -> List[int]:
    """Unescape Korg's 7-bit MIDI encoding"""
    result = []
    i = 0
    while i < len(sysex):
        if i >= len(sysex):
            break
        msb = sysex[i]
        i += 1
        for bit in range(7):
            if i < len(sysex):
                result.append(sysex[i] | (((msb >> bit) & 1) << 7))
                i += 1
            else:
                break
    return result


def escapeSysex(data: List[int]) -> List[int]:
    """Escape data to Korg's 7-bit MIDI encoding"""
    result = []
    i = 0
    while i < len(data):
        msb = 0
        for bit in range(7):
            if i + bit < len(data):
                msb |= ((data[i + bit] >> 7) & 1) << bit
        result.append(msb)
        
        for bit in range(7):
            if i < len(data):
                result.append(data[i] & 0x7F)
                i += 1
            else:
                break
    return result


def make_test_data():
    def programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        single_bank = knobkraft.load_sysex("testData/Korg_Triton/bank1-patch1-korgtriton-noisystabber.syx", as_single_list=True)
        bank_as_list_of_messages = knobkraft.load_sysex("testData/Korg_Triton/bank1-patch1-korgtriton-noisystabber.syx", as_single_list=False)
        assert isBankDumpFinished(bank_as_list_of_messages)
        patches = extractPatchesFromBank(single_bank)
        individual_messages = knobkraft.splitSysex(patches)
        yield testing.ProgramTestData(message=individual_messages[0], name="Noisy Stabber   ")

        #full_dump = knobkraft.load_sysex("testData/Korg_Triton/full-korgtriton-midiox.syx")  # full dump has message code 0x50, which is not implemented yet
        #bank_extracted = []
        #for message in full_dump:
        #    if isPartOfBankDump(message):
        #        bank_extracted.append(bank_extracted)
        #assert isBankDumpFinished(bank_extracted)
        # individual_messages = knobkraft.splitSysex(patches)
        # yield testing.ProgramTestData(message=individual_messages[0], name="Noisy Stabber   ")
        # yield testing.ProgramTestData(message=individual_messages[3], name="Noisy Stabber   ")

    def banks(test_data: testing.TestData) -> List:
        yield test_data.all_messages[0]

    return testing.TestData(sysex="testData/Korg_Triton/bank1-korgtriton-midiox2.syx", edit_buffer_generator=programs, bank_generator=banks, expected_patch_count=128)
