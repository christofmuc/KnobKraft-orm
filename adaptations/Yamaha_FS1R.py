#
#   Copyright (c) 2021-2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from copy import copy
from typing import List, Optional

import knobkraft
import testing

YAMAHA_ID=0x43
YAMAHA_FS1R=0x5e
SYSTEM_SETTINGS_ADDRESS=[0x00, 0x00, 0x00]
PERFORMANCE_EDIT_BUFFER_ADDRESS=[0x10, 0x00, 0x00]
PERFORMANCE_ADDRESS=[0x11, 0x00]  # Internal PERFORMANCE. Hi, med fixed, Lo is the number of the performance (0-127)
VOICE_EDIT_BUFFFER_PART1=[0x40, 0x00, 0x00]
VOICE_EDIT_BUFFFER_PART2=[0x41, 0x00, 0x00]
VOICE_EDIT_BUFFFER_PART3=[0x42, 0x00, 0x00]
VOICE_EDIT_BUFFFER_PART4=[0x43, 0x00, 0x00]
VOICE_ADDRESS=[0x51, 0x00]  # Internal VOICE

DEVICE_ID_DETECTED=0x00

# Build an index structure to detect VOICE EDIT BUFFERS
EDIT_BUFFER_ADDRESSES=set()
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFFER_PART1))
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFFER_PART2))
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFFER_PART3))
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFFER_PART4))
EDIT_BUFFER_VOICE_ADDRESSES=copy(EDIT_BUFFER_ADDRESSES)
EDIT_BUFFER_ADDRESSES.add(tuple(PERFORMANCE_EDIT_BUFFER_ADDRESS))

CURRENT_PERFORMANCE_STRUCTURE: Optional[set] =None
CURRENT_OPEN_VOICE_REQUESTS: Optional[set] =None

# class DataBlock:
#     def __init__(self, address, size, name):
#         pass
#
# fs1r_performance_data = [DataBlock((0x10, 0x00, 0x00), 80, "Performance Common"), # The first 12 bytes are the name
#                          DataBlock((0x10, 0x00, 0x50), 112, "Effect"),
#                          DataBlock((0x30, 0x00, 0x00), 52, "Part 1"),
#                          DataBlock((0x31, 0x00, 0x00), 52, "Part 2"),
#                          DataBlock((0x32, 0x00, 0x00), 52, "Part 3"),
#                          DataBlock((0x33, 0x00, 0x00), 52, "Part 4")]
#
# fs1_all_performance_data = [DataBlock((0x11, 0x00, 0x00), 400, "Performance")]
#
#
# fs1r_voice_data = [DataBlock((0x40, 0x00, 0x00), 112, "Voice Common")] + \
#                   [DataBlock((0x60, op, 0x00), 35, "Operator 1 Voiced") for op in range(8)] + \
#                   [DataBlock((0x60, op, 0x23), 27, "Operator 1 Non-Voiced") for op in range(8)] + \
#                   [DataBlock((0x61, op, 0x00), 35, "Operator 2 Voiced") for op in range(8)] + \
#                   [DataBlock((0x61, op, 0x23), 27, "Operator 2 Non-Voiced") for op in range(8)] + \
#                   [DataBlock((0x62, op, 0x00), 35, "Operator 3 Voiced") for op in range(8)] + \
#                   [DataBlock((0x62, op, 0x23), 27, "Operator 3 Non-Voiced") for op in range(8)] + \
#                   [DataBlock((0x63, op, 0x00), 35, "Operator 4 Voiced") for op in range(8)] + \
#                   [DataBlock((0x64, op, 0x23), 27, "Operator 4 Non-Voiced") for op in range(8)]
#
# fs1r_fseq_data = [DataBlock((0x70, 0x00, 0x00), 0x00, "Fseq parameter")]  # 8 bytes of name at the start, byte count not used in here
#
# fs1r_system_data = [DataBlock((0x00, 0x00, 0x00), 0x4c, "System Parameter")]


def name():
    return "Yamaha FS1R"


def createDeviceDetectMessage(device_id):
    # Just send a request for the system settings address
    # Use 0x30 as device_id base, just like SoundDiver
    return buildRequest(device_id, 0x20, SYSTEM_SETTINGS_ADDRESS)


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    return 500


def channelIfValidDeviceResponse(message):
    global DEVICE_ID_DETECTED
    if isOwnSysex(message) and len(message) > 8:
        if addressFromMessage(message) == SYSTEM_SETTINGS_ADDRESS:
            DEVICE_ID_DETECTED=message[2] & 0x0f
            print(f"Detected Yamaha FS1R set to sysex device ID '{DEVICE_ID_DETECTED}'")
            data_block = dataBlockFromMessage(message)
            channel = data_block[9]  # data_block[9] is the performance channel
            if channel == 0x10:
                print("Warning: Your FS1R is set to OMNI MIDI channel. Surprises await!")
                return 16
            return channel
    return -1


def bankDescriptors():
    return [{"bank": 0, "name": "Patches", "size": 128, "type": "Performance", "isROM": False, }]
            #{"bank": 1, "name": "Voices", "size": 128, "type": "Voice", "isROM": False, }]


def _nameLength(message):
    if isPerformance(message):
        return 12
    elif isVoice(message):
        return 10
    else:
        raise Exception("neither a performance nor a voice")


def nameFromDump(message):
    for sub in knobkraft.findSysexDelimiters(message):
        sub_message = message[sub[0]:sub[1]]
        if isPerformance(sub_message):
            name_len = _nameLength(sub_message)
            return "".join([chr(x) for x in dataBlockFromMessage(sub_message)[:name_len]])
    return "Invalid"


def renamePatch(message, new_name):
    result = []
    worked = False
    for sub in knobkraft.findSysexDelimiters(message):
        sub_message = copy(message[sub[0]:sub[1]])
        if isPerformance(sub_message):
            name_len = _nameLength(sub_message)
            name_list = [ord(x) for x in new_name.ljust(name_len, " ")]
            sub_message[9:9+name_len] = name_list
            result.extend(recalculateChecksum(sub_message))
            worked = True
        else:
            result.extend(sub_message)
    if worked:
        return result
    else:
        raise Exception("can only rename single program dumps!")


def createEditBufferRequest(channel):
    # How do we request the Voices? Just send all requests at once?
    return buildRequest(channel, 0x20, PERFORMANCE_EDIT_BUFFER_ADDRESS) + \
           buildRequest(channel, 0x20, VOICE_EDIT_BUFFFER_PART1) + \
           buildRequest(channel, 0x20, VOICE_EDIT_BUFFFER_PART2) + \
           buildRequest(channel, 0x20, VOICE_EDIT_BUFFFER_PART3) + \
           buildRequest(channel, 0x20, VOICE_EDIT_BUFFFER_PART4)


def isPartOfEditBufferDump(message):
    if isOwnSysex(message) and len(message) > 8:
        address = addressFromMessage(message)
        return tuple(address) in EDIT_BUFFER_ADDRESSES
    return False


def isEditBufferDump(data):
    messages = knobkraft.findSysexDelimiters(data)
    if len(messages) >= 5:
        messages_found = set()
        for message in messages:
            sub_message = data[message[0], message[1]]
            if isOwnSysex(sub_message) and len(sub_message) > 8:
                address = addressFromMessage(sub_message)
                if tuple(address) in EDIT_BUFFER_ADDRESSES:
                    messages_found.add(tuple(address))
        return len(messages_found) == 5
    return False


def convertToEditBuffer(channel, data):
    if isEditBufferDump(data):
        return data
    elif isSingleProgramDump(data):
        program = copy(data)
        if isVoice(data):
            setAddress(program, VOICE_EDIT_BUFFFER_PART1)
        elif isPerformance(data):
            setAddress(program, PERFORMANCE_EDIT_BUFFER_ADDRESS)
        return recalculateChecksum(program)
    raise Exception("Can't convert to edit buffer dump!")


def createProgramDumpRequest(channel, patch_no):
    global CURRENT_PERFORMANCE_STRUCTURE, CURRENT_OPEN_VOICE_REQUESTS
    if 0 <= patch_no < 128:
        # Request performance
        CURRENT_PERFORMANCE_STRUCTURE = None
        CURRENT_OPEN_VOICE_REQUESTS = None
        return buildRequest(DEVICE_ID_DETECTED, 0x20, PERFORMANCE_ADDRESS + [patch_no])
    raise Exception("Can only request 128 performances")


def _extractReferencedUserVoices(performance_message):
    result = set()
    performance_data = dataBlockFromMessage(performance_message)
    if len(performance_data) == 400:
        # Got a single data block with all messages
        part_start = 80 + 112
        for part_index in range(part_start, part_start + 4 * 52, 52):
            part_bank = performance_data[part_index + 1]
            part_program = performance_data[part_index + 2]
            if part_bank == 1:
                # Assume this is a user part, not a preset
                result.add(part_program)
        return result
    else:
        raise Exception(f"Got performance data of unknown size: {len(performance_data)}")


def isPartOfSingleProgramDump(message):
    global CURRENT_PERFORMANCE_STRUCTURE, CURRENT_OPEN_VOICE_REQUESTS
    if isOwnSysex(message) and len(message) > 8:
        address = addressFromMessage(message)
        if address[:2] == PERFORMANCE_ADDRESS:
            if CURRENT_PERFORMANCE_STRUCTURE is not None:
                raise Exception("Program logic error, second performance found before the voices of the first!")
            CURRENT_PERFORMANCE_STRUCTURE = _extractReferencedUserVoices(message)
            CURRENT_OPEN_VOICE_REQUESTS = copy(CURRENT_PERFORMANCE_STRUCTURE)
        elif address[:2] == VOICE_ADDRESS:
            voice_no = address[2]
            if voice_no not in CURRENT_PERFORMANCE_STRUCTURE:
                # This is a voice, but not one of those we requested
                return False

        # Now request the next voice
        if len(CURRENT_PERFORMANCE_STRUCTURE) > 0:
            # Request the first of the user voices
            return True, buildRequest(DEVICE_ID_DETECTED, 0x20, VOICE_ADDRESS + [CURRENT_OPEN_VOICE_REQUESTS.pop()])
        else:
            return True

    return False


def isSingleProgramDump(data):
    messages = knobkraft.findSysexDelimiters(data)
    if len(messages) > 0:
        voices_found = set()
        voices_required = None
        for message in messages:
            sub_message = data[message[0]:message[1]]
            if len(sub_message) > 8:
                address = addressFromMessage(sub_message)
                if address[:2] == PERFORMANCE_ADDRESS:
                    voices_required = _extractReferencedUserVoices(sub_message)
                elif address[:2] == VOICE_ADDRESS:
                    voices_found.add(address[2])

        if voices_required is None:
            # No performance found
            return False

        if len(voices_required) > 0:
            return voices_found == voices_required
        else:
            return True
    return False


def convertToProgramDump(channel, message, program_number):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        new_message = message[:2] + [DEVICE_ID_DETECTED & 0x0f] + message[3:8] + [program_number % 128] + message[9:]
        if isVoice(message):
            setAddress(new_message, VOICE_ADDRESS + [program_number & 0x7f])
        elif isPerformance(message):
            setAddress(new_message, PERFORMANCE_ADDRESS + [program_number & 0x7f])
        return recalculateChecksum(new_message)
    raise Exception("Can only convert single program dumps to program dumps")




#def calculateFingerprint(message):
#    for i in range(10):
#        message[i] = 0
#    return hashlib.md5(bytearray(message)).hexdigest()


def isOwnSysex(message):
    return (len(message) > 7
            and message[0] == 0xf0  # Sysex
            and message[1] == YAMAHA_ID
            and message[3] == YAMAHA_FS1R)


def numberFromDump(message):
    if isSingleProgramDump(message):
        if isPerformance(message):
            return addressFromMessage(message)[2]  # The lo address is the performance number
        elif isVoice(message):
            return addressFromMessage(message)[2] + 128
    raise Exception("Can only extract program number from Performances and Voices!")


def addressFromMessage(message):
    if isOwnSysex(message) and len(message) > 8:
        return [message[6], message[7], message[8]]
    raise Exception("Got invalid data block")


def setAddress(message, address):
    message[6:9] = address


def isPerformance(message):
    return isOwnSysex(message) and len(message) > 8 and (addressFromMessage(message)[:2] == PERFORMANCE_ADDRESS or addressFromMessage(message) == PERFORMANCE_EDIT_BUFFER_ADDRESS)


def isVoice(message):
    return isOwnSysex(message) and len(message) > 8 and (addressFromMessage(message)[:2] == VOICE_ADDRESS or tuple(addressFromMessage(message)) in EDIT_BUFFER_VOICE_ADDRESSES)


def dataBlockFromMessage(message):
    if isOwnSysex(message):
        data_len = message[4] << 7 | message[5]
        if len(message) == data_len + 11:
            # The Check-sum is the value that results in a value of 0 for the
            # lower 7 bits when the Model ID, Start Address, Data and Check sum itself are added.
            checksum_block = message[0x04:-1]
            if (sum(checksum_block) & 0x7f) == 0:
                # return "Data" block
                data_block =  message[0x09:-2]
                assert len(data_block) == data_len
                return data_block
    raise Exception("Got corrupt data block")


def recalculateChecksum(message):
    if isOwnSysex(message):
        data_len = message[4] << 7 | message[5]
        if len(message) == data_len + 11:
            changed = copy(message)
            checksum_block = message[0x04:-2]
            checksum = sum([-x for x in checksum_block]) & 0x7f
            changed[-2] = checksum
            return changed
    raise Exception("Got corrupt data block")


def buildRequest(device_id, device_id_base, address):
    return [0xf0, YAMAHA_ID, device_id_base | (device_id & 0x0f), YAMAHA_FS1R, address[0], address[1], address[2], 0xf7]


def extractPatchesFromAllBankMessages(messages):
    performances = []
    voices = {}
    for message in messages:
        if isPerformance(message):
            performances.append(message)
        elif isVoice(message):
            address = addressFromMessage(message)
            if tuple(address[:2]) in EDIT_BUFFER_VOICE_ADDRESSES:
                # Scratching head
                pass
            elif address[:2] == VOICE_ADDRESS:
                voice_no = address[2]
                if voice_no in voices:
                    print(f"Warning, found voice no {voice_no} more than once in bank dump, no way to import, ignoring second message")
                else:
                    voices[voice_no] = message

    # Now assemble the performances with their voices
    for performance in performances:
        referenced_voices = _extractReferencedUserVoices(performance)
        for voice in referenced_voices:
            if voice in voices:
                performance.extend(voices[voice])
            else:
                print(f"Warning: Voice no {voice} referenced by patch '{nameFromDump(performance)}' not found in bank, performance incomplete")
    return performances


def make_test_data():
    from mido import MidiFile
    mid = MidiFile("testData/Yamaha_FS1R/Vdfs1r01.mid")
    program_data = []
    for i, track in enumerate(mid.tracks):
        print('Track {}: {}'.format(i, track.name))
        for msg in track:
            if msg.type == "sysex":
                program_data.append(msg.bytes())
    #message = knobkraft.stringToSyx("f0 43 00 5e 03 10 11 00 01 43 50 2d 37 30 20 50 47 20 20 20 20 00 00 01 00 75 40 18 00 00 00 01 00 07 68 00 00 00 00 00 00 00 02 00 00 00 00 00 40 01 01 01 01 01 01 00 00 00 01 00 02 00 04 00 08 20 00 20 00 00 00 00 00 19 1a 13 14 13 14 27 28 38 38 50 50 3c 50 40 40 00 0f 00 0a 00 09 00 18 00 2a 00 00 00 00 00 00 00 00 22 04 45 0a 40 00 00 05 00 2e 00 5c 00 0a 00 4a 00 14 00 40 00 32 00 40 00 00 00 00 00 00 00 1a 00 40 00 01 00 40 00 40 00 22 00 40 00 32 00 40 00 13 00 34 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 40 40 01 40 40 40 15 40 00 00 7f 46 0c 07 00 42 22 07 46 36 07 00 00 04 01 01 00 10 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 0d 08 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 00 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 04 01 00 11 7f 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 00 28 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 32 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 04 01 00 11 7f 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 00 28 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 32 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 04 01 00 11 7f 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 00 28 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 32 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 03 f7")
    programs = extractPatchesFromAllBankMessages(program_data)

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        pass
        #edit_buffer = []
        #for msg in program_data:
        #    if isPartOfEditBufferDump(msg):
        #        edit_buffer.append(msg)
        #    if isEditBufferDump(edit_buffer):
        #        yield testing.ProgramTestData(message=edit_buffer)
        #        edit_buffer = []
        #edit1 = convertToEditBuffer(0x01, message)
        #edit1 = convertToEditBuffer(0x01, program_data[0])
        #yield testing.ProgramTestData(message=edit1, name="CP-70 PG    ", rename_name="Piano 2   ")
        #edit2 = convertToEditBuffer(0x01, program_data[255])
        #yield testing.ProgramTestData(message=edit2, name="Sitar     ", target_no=244, rename_name="Piano 2   ")

    def program_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        #yield testing.ProgramTestData(programs[0], name="nknonwsfsdf")
        #program_buffer = []
        #for msg in programs:
        #    if isPartOfSingleProgramDump(msg):
        #        program_buffer.append(msg)
        #    if isSingleProgramDump(program_buffer):
        #        yield testing.ProgramTestData(message=program_buffer)
        #        program_buffer = []
        yield testing.ProgramTestData(message=programs[0], name="HARDPNO PF  ", rename_name="Piano 2   ", number=0, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=programs[127], name="Sitar       ", rename_name="Piano 2   ", number=127, friendly_number="Bank3-2")

    return testing.TestData(program_generator=program_buffers, edit_buffer_generator=edit_buffers,
                            device_detect_call="F0 43 20 5E 00 00 00 F7",
                            device_detect_reply=("f0 43 00 5e 00 4c 00 00 00 40 00 00 00 00 00 40 02 01 00 00 00 00 00 00 00 01 00 01 01 01 01 10 11 12 13 14 15 16 0d 04 02 50 51 3c 7f 3c 7f 3c 7f 3c 7f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 06 f7", 0x00))

