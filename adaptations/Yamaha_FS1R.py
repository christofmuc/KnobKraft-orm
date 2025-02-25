#
#   Copyright (c) 2021-2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import dataclasses
import hashlib
from copy import copy
from typing import List, Set, Dict

import knobkraft
import testing

YAMAHA_ID=0x43
YAMAHA_FS1R=0x5e
SYSTEM_SETTINGS_ADDRESS=[0x00, 0x00, 0x00]
PERFORMANCE_EDIT_BUFFER_ADDRESS=[0x10, 0x00, 0x00]
PERFORMANCE_ADDRESS=[0x11, 0x00]  # Internal PERFORMANCE. Hi, med fixed, Lo is the number of the performance (0-127)
VOICE_EDIT_BUFFER_PART1=[0x40, 0x00, 0x00]
VOICE_EDIT_BUFFER_PART2=[0x41, 0x00, 0x00]
VOICE_EDIT_BUFFER_PART3=[0x42, 0x00, 0x00]
VOICE_EDIT_BUFFER_PART4=[0x43, 0x00, 0x00]
VOICE_ADDRESS=[0x51, 0x00]  # Internal VOICE

DEVICE_ID_DETECTED=0x00

# Build an index structure to detect VOICE EDIT BUFFERS
EDIT_BUFFER_ADDRESSES=set()
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFER_PART1))
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFER_PART2))
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFER_PART3))
EDIT_BUFFER_ADDRESSES.add(tuple(VOICE_EDIT_BUFFER_PART4))
EDIT_BUFFER_VOICE_ADDRESSES=copy(EDIT_BUFFER_ADDRESSES)
EDIT_BUFFER_ADDRESSES.add(tuple(PERFORMANCE_EDIT_BUFFER_ADDRESS))


@dataclasses.dataclass
class VoiceReference:
    part: int
    bank: int
    program: int
    user: bool


def name():
    return "Yamaha FS1R"


def createDeviceDetectMessage(device_id):
    # Just send a request for the system settings address
    # We could also use 0x30 as device_id base, just like SoundDiver
    return buildRequest(device_id, 0x20, SYSTEM_SETTINGS_ADDRESS)


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    return 700


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
    # Just send a request for the performance, the voice parts will be requested on reception of the performance message
    return buildRequest(channel, 0x20, PERFORMANCE_EDIT_BUFFER_ADDRESS)


def isPartOfEditBufferDump(message):
    if isOwnSysex(message) and len(message) > 8:
        address = addressFromMessage(message)
        if address == PERFORMANCE_EDIT_BUFFER_ADDRESS:
            user_voices = _listOfUserParts(_extractReferencedUserVoices(message))
            device_id = message[2] & 0x0f
            requests = [buildRequest(device_id, 0x20, [VOICE_EDIT_BUFFER_PART1[0] + part] + VOICE_EDIT_BUFFER_PART1[1:]) for part in user_voices]
            if len(requests) > 0:
                return True, [x for message in requests for x in message]
            else:
                return True
        else:
            # Must be a VOICE message to be included in the edit buffer
            return tuple(address) in EDIT_BUFFER_ADDRESSES
    return False


def isEditBufferDump(data):
    messages = knobkraft.findSysexDelimiters(data)
    if len(messages) > 0:
        messages_found = set()
        user_voices = set()
        for start, end in messages:
            sub_message = data[start:end]
            if isOwnSysex(sub_message) and len(sub_message) > 8:
                address = addressFromMessage(sub_message)
                if address == PERFORMANCE_EDIT_BUFFER_ADDRESS:
                    messages_found.add(tuple(address))
                    user_voices = set(_listOfUserParts(_extractReferencedUserVoices(sub_message)))
                elif tuple(address) in EDIT_BUFFER_VOICE_ADDRESSES:
                    part = address[0] - VOICE_EDIT_BUFFER_PART1[0]
                    user_voices.remove(part)
        return len(messages_found) > 0 and len(user_voices) == 0
    return False


def convertToEditBuffer(channel, data):
    if isEditBufferDump(data):
        return data
    elif isSingleProgramDump(data):
        voice_addresses = [VOICE_EDIT_BUFFER_PART1, VOICE_EDIT_BUFFER_PART2, VOICE_EDIT_BUFFER_PART3, VOICE_EDIT_BUFFER_PART4]
        result = []
        voice_structure = None
        user_voices: Dict[int, List[int]] = {}
        for sub in knobkraft.findSysexDelimiters(data):
            sub_message = copy(data[sub[0]:sub[1]])
            if isPerformance(sub_message):
                voice_structure = _extractReferencedUserVoices(sub_message)
                setAddress(sub_message, PERFORMANCE_EDIT_BUFFER_ADDRESS)
                result.extend(recalculateChecksum(sub_message))
            elif isVoice(sub_message):
                voice_address = addressFromMessage(sub_message)
                assert voice_address[:2] == VOICE_ADDRESS
                user_voices[voice_address[2]] = sub_message

        for voice in voice_structure:
            if voice.user:
                if voice.program in user_voices:
                    voice_message = copy(user_voices[voice.program])
                    setAddress(voice_message, voice_addresses[voice.part])
                    result.extend(recalculateChecksum(voice_message))

        assert isEditBufferDump(result)
        return result
    raise Exception("Can't convert to edit buffer dump!")


def createProgramDumpRequest(channel, patch_no):
    global CURRENT_PERFORMANCE_STRUCTURE, CURRENT_OPEN_VOICE_REQUESTS
    if 0 <= patch_no < 128:
        # Request performance
        CURRENT_PERFORMANCE_STRUCTURE = None
        CURRENT_OPEN_VOICE_REQUESTS = None
        return buildRequest(DEVICE_ID_DETECTED, 0x20, PERFORMANCE_ADDRESS + [patch_no])
    raise Exception("Can only request 128 performances")


def _extractReferencedUserVoices(performance_message) -> List[VoiceReference]:
    result = []
    performance_data = dataBlockFromMessage(performance_message)
    if len(performance_data) == 400:
        # Got a single data block with all messages
        part_start = 80 + 112
        part = 0
        for part_index in range(part_start, part_start + 4 * 52, 52):
            part_bank = performance_data[part_index + 1]
            part_program = performance_data[part_index + 2]
            # Assume this part_bank == 1 is a user part, not a preset
            result.append(VoiceReference(part=part, bank=part_bank, program=part_program, user=True if part_bank==1 else False))
            part += 1
        return result
    else:
        raise Exception(f"Got performance data of unknown size: {len(performance_data)}")


def _setOfUserVoices(voices: List[VoiceReference]) -> Set[int]:
    return set([ref.program for ref in filter(lambda x: x.user, voices)])


def _listOfUserParts(voices: List[VoiceReference]) -> Set[int]:
    return set([ref.part for ref in filter(lambda x: x.user, voices)])


def isPartOfSingleProgramDump(message):
    if isOwnSysex(message) and len(message) > 8:
        address = addressFromMessage(message)
        if address[:2] == PERFORMANCE_ADDRESS:
            user_voices = _setOfUserVoices(_extractReferencedUserVoices(message))
            device_id = message[2] & 0x0f
            requests = [buildRequest(device_id, 0x20, VOICE_ADDRESS + [program]) for program in user_voices]
            if len(requests) > 0:
                return True, [x for message in requests for x in message]
            else:
                return True
        else:
            # Must be a VOICE message
            return address[:2] == VOICE_ADDRESS
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
                    voices_required = _setOfUserVoices(_extractReferencedUserVoices(sub_message))
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
    program_buffer = False
    edit_buffer = isEditBufferDump(message)
    if not edit_buffer:
        program_buffer = isSingleProgramDump(message)
    if program_buffer or edit_buffer:
        voices = None
        result = []
        for sub in knobkraft.findSysexDelimiters(message):
            sub_message = copy(message[sub[0]:sub[1]])
            if isPerformance(sub_message):
                voices = _extractReferencedUserVoices(sub_message)
                setAddress(sub_message, PERFORMANCE_ADDRESS + [program_number & 0x7f])
            elif isVoice(sub_message):
                if voices is None:
                    raise Exception("Need performance message before Voice message!")
                if edit_buffer:
                    part_no = addressFromMessage(sub_message)[0] - VOICE_EDIT_BUFFER_PART1[0]
                    setAddress(sub_message, VOICE_ADDRESS + [voices[part_no].program])
                    if not voices[part_no].user:
                        raise Exception("Got Voice edit buffer for voice which is not a user voice. Where to store?")
            else:
                raise Exception("Got unexpected message which is neither voice nor performance!")
            setDeviceId(sub_message, DEVICE_ID_DETECTED)
            result.extend(recalculateChecksum(sub_message))
        return result
    raise Exception("Can only convert single program dumps to program dumps")


def calculateFingerprint(message):
    if isEditBufferDump(message) or isSingleProgramDump(message):
        data = []
        for sub in knobkraft.findSysexDelimiters(message):
            sub_message = message[sub[0]:sub[1]]
            if isPerformance(sub_message):
                # Erase the name
                name_len = _nameLength(sub_message)
                cleared = copy(dataBlockFromMessage(sub_message))
                cleared[:name_len] = [0x00] * name_len
                data.extend(cleared)
            else:
                data.extend(dataBlockFromMessage(sub_message))
        return hashlib.md5(bytearray(data)).hexdigest()
    raise Exception("Can fingerprint only edit buffer dumps or single program dumps")


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


def setDeviceId(message, deviceId):
    message[2] = deviceId


def isPerformance(message):
    return isOwnSysex(message) and len(message) > 8 and (addressFromMessage(message)[:2] == PERFORMANCE_ADDRESS or addressFromMessage(message) == PERFORMANCE_EDIT_BUFFER_ADDRESS)


def isVoice(message):
    return isOwnSysex(message) and len(message) > 8 and (addressFromMessage(message)[:2] == VOICE_ADDRESS or tuple(addressFromMessage(message)) in EDIT_BUFFER_VOICE_ADDRESSES)


def dataBlockFromMessage(message):
    if isOwnSysex(message):
        end_of_message = knobkraft.findSysexDelimiters(message, 1)[0]
        sub_message = message[end_of_message[0]:end_of_message[1]]
        data_len = message[4] << 7 | message[5]
        if len(sub_message) == data_len + 11:
            # The Check-sum is the value that results in a value of 0 for the
            # lower 7 bits when the Model ID, Start Address, Data and Check sum itself are added.
            checksum_block = sub_message[0x04:-1]
            if (sum(checksum_block) & 0x7f) == 0:
                # return "Data" block
                data_block =  sub_message[0x09:-2]
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


def isPartOfBankDump(message):
    return isOwnSysex(message) and len(message) > 8 and (isPerformance(message) or isVoice(message))


def isBankDumpFinished(messages: List[List[int]]):
    performances = 0
    voices = 0
    for message in messages:
        if isPartOfBankDump(message):
            if isPerformance(message):
                performances += 1
            elif isVoice(message):
                voices += 1
    return performances == 128 and voices == 128


def extractPatchesFromAllBankMessages(messages):
    performances = []
    voices:Dict[int, List[int]] = {}  # Map voice no to message representing that voice
    for message in messages:
        if isPerformance(message):
            performances.append(copy(message))
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
                    voices[voice_no] = copy(message)

    # Now assemble the performances with their voices
    used_voices = set()
    all_voices = set(range(128))
    for performance in performances:
        referenced_voices = _extractReferencedUserVoices(performance)
        voices_added = set()  # Use this to make sure we're not adding the same voice twice
        for ref_voice in referenced_voices:
            if ref_voice.user:
                if ref_voice.program in voices:
                    if ref_voice.program not in voices_added:
                        performance.extend(voices[ref_voice.program])
                        voices_added.add(ref_voice.program)
                else:
                    print(f"Warning: Voice no {ref_voice.program} referenced by patch '{nameFromDump(performance)}' not found in bank, performance incomplete")
        used_voices = used_voices.union(voices_added)

    if len(all_voices.symmetric_difference(used_voices)) > 0:
        print(f"Unused voices in this bank ignored. Voices [{all_voices.symmetric_difference(used_voices)}]")
    return performances


def convert_mid_to_syx():
    from mido import MidiFile
    mid = MidiFile("testData/Yamaha_FS1R/Vdfs1r01.mid")
    program_data = []
    for i, track in enumerate(mid.tracks):
        print('Track {}: {}'.format(i, track.name))
        for msg in track:
            if msg.type == "sysex":
                program_data.append(msg.bytes())
    with open("testData/Yamaha_FS1R/Vdfs1r01.syx", "wb") as writeout:
        ex = bytearray()
        for data in program_data:
            ex.extend(data)
        writeout.write(ex)


def test_load_dump():
    all_messages = knobkraft.load_sysex("testData/Yamaha_FS1R/fs1r.syx")
    program = []
    createProgramDumpRequest(1, 0)
    count = 0
    for m in all_messages:
        valid = isPartOfSingleProgramDump(m)
        if isinstance(valid, tuple):
            print(f"Got request message generated!: {valid[1]}")
            program.extend(m)
        elif valid:
            program.extend(m)
        if isSingleProgramDump(program):
            print(f"Got full program: {nameFromDump(program)}")
            program = []
            count += 1
            createProgramDumpRequest(1, 0)
    assert count == 128


def make_test_data():
    #message = knobkraft.stringToSyx("f0 43 00 5e 03 10 11 00 01 43 50 2d 37 30 20 50 47 20 20 20 20 00 00 01 00 75 40 18 00 00 00 01 00 07 68 00 00 00 00 00 00 00 02 00 00 00 00 00 40 01 01 01 01 01 01 00 00 00 01 00 02 00 04 00 08 20 00 20 00 00 00 00 00 19 1a 13 14 13 14 27 28 38 38 50 50 3c 50 40 40 00 0f 00 0a 00 09 00 18 00 2a 00 00 00 00 00 00 00 00 22 04 45 0a 40 00 00 05 00 2e 00 5c 00 0a 00 4a 00 14 00 40 00 32 00 40 00 00 00 00 00 00 00 1a 00 40 00 01 00 40 00 40 00 22 00 40 00 32 00 40 00 13 00 34 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 40 40 01 40 40 40 15 40 00 00 7f 46 0c 07 00 42 22 07 46 36 07 00 00 04 01 01 00 10 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 0d 08 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 00 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 04 01 00 11 7f 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 00 28 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 32 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 04 01 00 11 7f 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 00 28 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 32 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 04 01 00 11 7f 01 00 00 18 40 40 7f 40 40 40 00 7f 7f 00 28 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 02 32 42 3e 32 00 01 7f 00 01 40 40 00 00 00 00 03 f7")

    def edit_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        programs = extractPatchesFromAllBankMessages(test_data.all_messages)
        yield testing.ProgramTestData(message=convertToEditBuffer(12, programs[0]), name="HARDPNO PF  ", rename_name="Piano 2   ", number=0, friendly_number="Bank3-2")

    def program_buffers(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        assert isBankDumpFinished(test_data.all_messages)
        programs = extractPatchesFromAllBankMessages(test_data.all_messages)
        assert len(programs) == 128
        yield testing.ProgramTestData(message=programs[0], name="HARDPNO PF  ", rename_name="Piano 2   ", number=0, friendly_number="Bank3-2")
        yield testing.ProgramTestData(message=programs[127], name="Sitar       ", rename_name="Piano 2   ", number=127, friendly_number="Bank3-2")

    return testing.TestData(sysex="testData/Yamaha_FS1R/Vdfs1r01.syx",
                            program_generator=program_buffers,
                            edit_buffer_generator=edit_buffers,
                            device_detect_call="F0 43 20 5E 00 00 00 F7",
                            device_detect_reply=("f0 43 00 5e 00 4c 00 00 00 40 00 00 00 00 00 40 02 01 00 00 00 00 00 00 00 01 00 01 01 01 01 10 11 12 13 14 15 16 0d 04 02 50 51 3c 7f 3c 7f 3c 7f 3c 7f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 06 f7", 0x00))

