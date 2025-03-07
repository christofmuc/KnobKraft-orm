import hashlib
import sys
from typing import List, Tuple, Optional, Union
from base64 import b64decode, b64encode
from zlib import crc32

import knobkraft
import testing
from knobkraft import knobkraft_api

MANUFACTURER_ID = [0x00, 0x20, 0x2B]  # ASM
MODEL_ID = 0x6F  # Hydrasynth
SYSEX_PREAMBLE = [0xf0] + MANUFACTURER_ID + [0x00] + [MODEL_ID]
ENDIANNESS = 'little'
MINIMUM_MESSAGE_SIZE = 4 + 4  # Excluding preamble and SYSEX_END. 4 bytes checksum + 1 byte command + 1 byte payload in b64.
#MESSAGES_PER_PATCH = 22

# Wait this amount of milliseconds between sending messages.
GENERAL_MESSAGE_DELAY_MILLIS = 30  # Seems like a safe value.


# Commands are one byte.
COMMANDS = {
    'CONNECT': 0x00,
    'CONNECT_ACK': 0x01,
    'PATCH_NAME_REQUEST': 0x02,  # Payload: [0, 0]. Triggers DATA responses with all the patch names for the current bank.
    'PATCH_REQUEST': 0x04,  # Payload: [0, bank#, patch#]. Triggers the first DATA response with the initial page of patch data, including the name.
    'PATCH_WRITE_REQUEST_RECEIVED': 0x07, # Payload: [0, bank#, patch#]. Sent from Hydrasynth after completing a DATA transfer that contains a patch write request.
    'UPDATE_FLASH': 0x14, # Payload: [0]. Sent to Hydrasynth to write data into flash after receiving.
    'UPDATE_FLASH_COMPLETE': 0x15, # Payload: [0]. Sent from Hydrasynth after completing Flash update.
    'DATA': 0x16,  # Payload [0, page#, total_pages, data...]. page# starts with 0.
    'DATA_ACK': 0x17,  # Payload: [0, page#, total_pages]. Triggers next page after acknowledging.
    'BANK_SWITCH': 0x18,  # Payload: [bank#]. bank# starts at 0 and maps to banks A .. F
    'BANK_SWITCH_ACK': 0x19,
    'PATCH_REQUEST_COMPLETE': 0x1A,  # Payload: [0]. Guessing meaning here, comes after a completed patch transfer from HS->PC.
    'PATCH_REQUEST_COMPLETE_ACK': 0x1B,  # Payload: [0]. Guessing.
    'FIRMWARE_VERSION_REQUEST': 0x28,  # Payload [0].
    'FIRMWARE_VERSION_RESPONSE': 0x29,  # Payload [0, v_major, 0, v_minor]. v_major = 155 -> V1.55. v_minor is a guess.
}
DATA_TYPES = {
    'PATCH_DUMP': 5,
    'PATCH_WRITE_DATA': 6
}
DATA_TYPES_REVERSE = {
    value: key for key, value in DATA_TYPES.items()
}


def to_hex_str(byte_list: List[int]) -> str:
    return " ".join(f"{byte:02X}" for byte in byte_list)


class ASMHydrasynth:

    def __init__(self, name: str,  model_id: int):
        self._name = name
        self._model_id = model_id

    @knobkraft_api
    def name(self):
        return self._name

    @knobkraft_api
    def createDeviceDetectMessage(self, device_id):
        return self._to_hydrasynth('CONNECT', [0])

    @knobkraft_api
    def needsChannelSpecificDetection(self):
        return False

    @knobkraft_api
    def deviceDetectWaitMilliseconds(self):
        return 300

    @knobkraft_api
    def channelIfValidDeviceResponse(self, message):
        command, payload = self._from_hydrasynth(message)

        if command != 'CONNECT_ACK':
            print(f'Invalid command received during device detect: {command}')
            return -1

        # Hydrasynth Desktop returns: 0x00A:B:C:D:E0x00 listing available bank names.
        if payload[0] != 0 and payload[-1] != 0:
            print(f'Invalid payload received during device detect: {to_hex_str(payload)}')
            return -1

        try:
            bank_string = bytes(payload[1:-1]).decode('ascii', errors='strict')
        except UnicodeDecodeError:
            print('Error: Cannot decode bank names as ASCII.')
            return -1

        if ':' not in bank_string:
            print('Error: Bank names not delimited by ":"')
            return -1

        bank_names = bank_string.split(':')
        if len(bank_names) not in [5, 8]:
            print(f'Unusual number of banks detected: {len(bank_names)}. Expected 5 or 8.)')

        print(f'Got valid Hydrasynth device response message with bank names: {bank_names}. Message: {to_hex_str(message)}.')
        return 0  # We don't support channel detection, so return valid channel MIDI channel 1 (zero-based) if all ok.

    @knobkraft_api
    def bankDescriptors(self) -> List[dict]:
        return [{"bank": x, "name": f"Bank {chr(ord('A') + x)}", "size": 128, "type": "Patch"} for x in range(5)]

    @knobkraft_api
    def bankSwitch(self, bank: int) -> List[int]:
        return self._to_hydrasynth('BANK_SWITCH', [bank % len(self.bankDescriptors())])

    @knobkraft_api
    def createProgramDumpRequest(self, device_id, patchNo):
        bank, patch = ASMHydrasynth._programToBankAndPatch(patchNo)
        return self._to_hydrasynth('PATCH_REQUEST', [0, bank, patch])

    @knobkraft_api
    def isPartOfSingleProgramDump(self, message):
        status, last, total, _ = self._verifyBufferMessages(message)
        if not status:
            return False
        ack_message = self._createDataAckMessage(last, total)
        print(f'isPartOfSingleProgramDump(): Returning True, {to_hex_str(ack_message)} (ACK {last}/{total}, message valid).')
        return True, ack_message

    @knobkraft_api
    def isSingleProgramDump(self, message: List[int]) -> bool:
        if len(message) == 0:
            return False
        status, last, total, _ = self._verifyBufferMessages(message)
        return status and last == total - 1  # Message is valid and we got all parts (0-based, hence total - 1).

    @knobkraft_api
    def nameFromDump(self, message: List[int]) -> str:
        status, last, total, relevant = self._verifyBufferMessages(message)
        if not status:
            return "invalid"
        messages = knobkraft.splitSysex(relevant)
        first_message = self._from_hydrasynth(messages[0])
        data = first_message[1]
        name_buffer = data[12:28]  # Names are stores as 0-terminated strings here, max. 16 characters long.
        name = ''
        for c in name_buffer:
            if c == 0:
                break
            else:
                name += chr(c)

        return name

    @knobkraft_api
    def numberFromDump(self, message: List[int]) -> int:
        messages = knobkraft.findSysexDelimiters(message, 1)
        command, data = self._from_hydrasynth(message[messages[0][0]:messages[0][1]])
        if not command == "DATA":
            raise Exception("Invalid program buffer handed to numberFromDump")
        bank = data[5]
        patch = data[6]
        return bank * 128 + patch

    @knobkraft_api
    def convertToProgramDump(self, device_id, message, program_number) -> List[int]:
        bank, patch = ASMHydrasynth._programToBankAndPatch(program_number)

        # Double-check consistency.
        status, last, total, relevant_messages = self._verifyBufferMessages(message)
        if not status:
            raise Exception('Message did not pass isSingleProgramDump() consistency checking.')

        # Consistency checking done. Now assemble sequence of MIDI messages for writing our patch to the given bank/patch numbers.
        result = self._patchProgramDump(relevant_messages, 'PATCH_WRITE_DATA', bank, patch)

        # Add the patch request complete message to make the hydrasynth responsive again
        result += self._to_hydrasynth("PATCH_REQUEST_COMPLETE", [0])

        # Add message to trigger flash updates.
        # result += to_hydrasynth('UPDATE_FLASH', [0])
        return result

    # Hydrasynth expects a handshake protocol for sending patches to the synth.
    # We're lazy here: Instead of implementing the handshake, we wait a "safe" time
    # between messages, assuming that they have been received correctly.
    @knobkraft_api
    def generalMessageDelay(self):
        return GENERAL_MESSAGE_DELAY_MILLIS

    @knobkraft_api
    def renamePatch(self, message: List[int], new_name: str) -> List[int]:
        first = knobkraft.findSysexDelimiters(message, 1)
        command, data = self._from_hydrasynth(message[first[0][0]:first[0][1]])
        if not command == "DATA":
            raise Exception("Invalid program buffer handed to renamePatch")

        name_buffer = 16 * [32]  # Default is space
        i = 0
        for c in new_name[:16]:
            name_buffer[i] = ord(c)
            i += 1
        if i < 16:
            name_buffer[i] = 0x00

        assert len(name_buffer) == len(data[12:28])
        data[12:28] = name_buffer

        # Merge messages again into one.
        return self._to_hydrasynth(command, data) + message[first[0][1]:]

    @knobkraft_api
    def calculateFingerprint(self, message: List[int]):
        status, _, _, relevant_messages = self._verifyBufferMessages(message)
        if not status:
            raise Exception("Invalid program buffer handed to renamePatch")

        first = knobkraft.findSysexDelimiters(relevant_messages, 1)
        command, data = self._from_hydrasynth(relevant_messages[first[0][0]:first[0][1]])
        if not command == "DATA":
            raise Exception(f"Invalid program buffer handed to renamePatch, data type is {command}")

        # The data type is irrelevant
        data[3] = 0
        # Fill bank and patch numbers with 0.
        data[5] = 0
        data[6] = 0
        # Fill the name part with zeros.
        data[12:28] = 16 * [0]

        # Now calculate a checksum on a reassembled nameless patch
        fingerprinted_message = self._to_hydrasynth(command, data) + relevant_messages[first[0][1]:]
        return hashlib.md5(bytearray(fingerprinted_message)).hexdigest()

    # Create a DATA_ACK message with the given parameters.
    def _createDataAckMessage(self, page, total_pages):
        return self._to_hydrasynth('DATA_ACK', [0, page, total_pages])

    @staticmethod
    def _programToBankAndPatch(program):
        bank_number = program // 128
        patch_in_bank = program % 128
        return bank_number, patch_in_bank

    def _to_hydrasynth(self, command: str, payload: List[int]):
        """ Encode a command and its payload into a Hydrasynth specific SysEx message.

        :param command: Command is a string, used as key for the COMMANDS dict above.
        :param payload: Payload is either a list of ints (to be converted to bytes), a hex string, a bytearray or a bytes object.
        :return: Returns a list of ints (representing byte values) ready for sending back to KnobKraft ORM for sending as MIDI.
        """
        command_bytes = bytes([COMMANDS[command]])

        payload_bytes = None
        if isinstance(payload, bytes):
            payload_bytes = payload
        elif isinstance(payload, list):
            payload_bytes = bytes(payload)
        elif isinstance(payload, str):
            payload_bytes = bytes.fromhex(payload)
        else:
            raise(ValueError('Invalid payload format.'))

        message_bytes = command_bytes + payload_bytes
        checksum_bytes = ASMHydrasynth._crc32_jamcrc(message_bytes).to_bytes(4, byteorder=ENDIANNESS, signed=False)
        message_b64 = b64encode(checksum_bytes + message_bytes)

        return SYSEX_PREAMBLE + list(message_b64) + [0xf7]

    # Decode and verify a SysEx message coming from Hydrasynth into a command string and a payload.
    # The message is either a string with hex values (to allow for debugging MIDI dumps) or a list of values (interpreted as bytes).
    # We return the Hydrasynth command as a string (see COMMANDS keys) if known, or a hex dump if not, or None if the message was invalid.
    # We return the payload as a list of byte values as ints, or None if the message is invalid.
    # If the message is too short to be interpreted as a base 64 decoded, checksummed message, we return None for the command, and
    # the raw message body as the payload.
    def _from_hydrasynth(self, message: List[int]) -> Tuple[Optional[str], List[int]]:
        message_bytes = None
        if isinstance(message, list):
            message_bytes = bytes(message)
        elif isinstance(message, str):
            message_bytes = bytes.fromhex(message)
        else:
            raise(ValueError('Invalid message format.'))

        if message[0] != 0xf0 or message[-1] != 0xf7:
            print(f'Invalid SysEx message: {message[0]:02X} ... {message[-1]:02X}')
            return None, message

        if message[:len(SYSEX_PREAMBLE)] != SYSEX_PREAMBLE:
            print(f'Hydrasynth preamble missing. Returning raw message: {to_hex_str(message)}')
            return None, message

        message_b64 = message_bytes[len(SYSEX_PREAMBLE):-1]
        if len(message_b64) < MINIMUM_MESSAGE_SIZE:
            print(f'Hydrasynth message length ({len(message_b64)}) is smaller than minimum ({MINIMUM_MESSAGE_SIZE}).')
            print(f'Returning SysEx message content: {to_hex_str(list(message_b64))}')
            return None, list(message_b64)

        # We have a potential Hydrasynth message. Let’s verify its checksum.
        message_decoded = b64decode(message_b64)

        message_checksum = message_decoded[:4]
        message_bytes = message_decoded[4:]
        checksum_bytes = ASMHydrasynth._crc32_jamcrc(message_bytes).to_bytes(4, byteorder=ENDIANNESS, signed=False)
        if message_checksum != checksum_bytes:
            print(f'Message checksum ({message_checksum}) does not match computed checksum ({checksum_bytes}).')
            print('Returning None.')
            return None, []

        # We have a valid Hydrasynth message! Let’s return its content.
        command_str = ASMHydrasynth.decode_command(message_bytes[0])

        payload_bytes = message_bytes[1:]
        payload = list(payload_bytes)

        return command_str, payload

    # Analyze a message and determine if it is a valid set of program dump messages.
    # Returns status, last message received (0-based), total # of messages.
    # Status is either False (Invalid) or True (valid).
    # A message is only complete, if status is True and the last message received is one less than the total # of messages.
    def _verifyBufferMessages(self, buffer) -> Tuple[bool, Optional[int], Optional[int], List[int]]:
        messages = knobkraft.splitSysex(buffer)
        if len(messages) == 0:
            print('No messages found, expected at least one message.')
            return False, None, None, []

        # We got one or more messages. Parse them and figure out if they're complete, or which is next.
        parsed_messages = [self._from_hydrasynth(m) for m in messages]

        # Figure out total number of messages and verify that the given messages fit.
        current = None
        expected = None
        last = None
        relevant_messages = []
        for i, m in enumerate(parsed_messages):
            # We expect these to be all DATA chunks with information about the total # of DATA messages to expect.
            if m[0] not in ['DATA']:
                print(f'Message #{i} is not a DATA message.')
                continue

            data = m[1]
            if len(data) < 6:
                print(f'Message #{i} should contain at least 6 bytes, got: {len(data)} instead.')
                return False, None, None, []

            # Figure out how many messages we expect and verify the given messages.
            _, current, total = data[:3]
            if current == 0:
                current = 0
                last = None
                relevant_messages = []
            relevant_messages.extend(messages[i])
            if not expected:
                expected = total
                #if expected != MESSAGES_PER_PATCH:
                 #   print(f'Expected {MESSAGES_PER_PATCH} total messages per patch, but total is {expected} instead.')
                    # Not failing here, because this might be a different Hydrasynth or some result of an update.
            #elif len(parsed_messages) > expected:
            #    print(f'Expected {expected} messages, but got {len(parsed_messages)} instead.')
            #    return False, current, expected
            #elif expected != total:
            #    print(f'Expected {expected} messages based on initial message, but message {i} indicates a total of {total} messages.')
            #    return False, current, expected

            # Number of messages vs. total are within expectations.
            # Verify if message sequence is consistent.
            #if not last:
            #    last = current
            #elif current - last != 1:
            #    print(f'Unexpected message sequence: Found message {current} after {last}.')
            #    return False, current, expected, []
            last = current

        # Everything looking good so far!
        return last != None, current, expected, relevant_messages

    # Patch the given patch data messages to qualify as PATCH_WRITE_DATA requests and to specify the given
    # bank and patch numbers.
    def _patchProgramDump(self, message: List[int], dump_type: Union[int, str], bank: int, patch: int) -> List[int]:
        # Split and parse messages for further processing.
        messages = knobkraft.splitSysex(message)
        parsed_messages = [self._from_hydrasynth(m) for m in messages]

        # The first message should indicate the right data type and bank/patch numbers. This means we need to patch whatever the
        # original message said to match where we are supposed to write to.
        if isinstance(dump_type, str):
            dump_type_number = DATA_TYPES[dump_type]
        elif isinstance(dump_type, int):
            dump_type_number = dump_type
        else:
            raise ValueError('Invalid dump type.')
        parsed_messages[0][1][3] = dump_type_number  # 0 = first message, 1 = data bytes, 3 = position of data type.
        parsed_messages[0][1][5] = bank
        parsed_messages[0][1][6] = patch

        # Serialize all messages into a combined message to return to KnobKraft.
        result = []
        for m in parsed_messages:
            result += self._to_hydrasynth(m[0], m[1])

        return result

    # CRC32 JAMCRC is a bitwise NOT of the CRC32 checksum.
    # See: https://stackoverflow.com/questions/58861209/crc-python-how-to-calculate-jamcrc-decimal-number-from-string
    @staticmethod
    def _crc32_jamcrc(b):  # b should be a bytearray
        return int("0b"+"1"*32, 2) - crc32(b)

    @staticmethod
    def decode_command(command: int) -> str:
        for key in COMMANDS:
            if COMMANDS[key] == command:
                return key

    def install(self, module):
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        for a in dir(self):
            if callable(getattr(self, a)) and hasattr(getattr(self, a), "_is_knobkraft"):
                # this was helpful: http://stupidpythonideas.blogspot.com/2013/06/how-methods-work.html
                setattr(module, a, getattr(self, a))


this_module = sys.modules[__name__]
asm = ASMHydrasynth("ASM Hydrasynth Desktop", MODEL_ID)
asm.install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        program = []
        names = ["Etherpad RJ", "GX UltraPad PS", "Etherpad RJ", "VocoStringer  RA", "OminousScoringAR"]
        i = 0
        for message in data.all_messages:
            if asm.isPartOfSingleProgramDump(message):
                program.extend(message)
            if asm.isSingleProgramDump(program):
                yield testing.ProgramTestData(message=program, name=names[i], number=127)
                i = i + 1

    return testing.TestData(sysex="testData\ASM_Hydrasynth\programs2.syx",
                            program_generator=programs,
                            device_detect_reply=('F0 00 20 2B 00 6F 4B 4B 43 61 59 41 45 41 51 54 70 43 4F 6B 4D 36 52 44 70 46 41 41 3D 3D F7', 0))
