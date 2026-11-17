"""Shared SY77/TG77 voice support by hmmbug and Giovanni Candiago."""

import hashlib

YAMAHA = 0x43
FORMAT = 0x7A
VOICE = b"LM  8101VC"
SYSTEM = b"LM  8101SY"

INTERNAL, PRESET_1, PRESET_2, EDIT = 0x00, 0x02, 0x03, 0x7F
BANKS = (INTERNAL, PRESET_1, PRESET_2)
BANK_NAMES = ("Internal", "Preset 1", "Preset 2")
BANK_LABELS = ("IN", "P1", "P2")
VOICE_MODE_PROGRAM_CHANGES = (122, 124, 117)
BANK_SIZE = 64
BANK_D_START = 48
BANK_D_ONLY = {0x02: "4AFM mono", 0x07: "4AWM", 0x09: "2AFM+2AWM", 0x0A: "Drum Set"}

MEMORY_TYPE = 30
MEMORY_NUMBER = 31
VOICE_TYPE = 32
VOICE_NAME = slice(33, 43)
VOICE_RECEIVE_CHANNEL = 80


def _device_number(channel):
    return channel if channel is not None and 0 <= channel < 16 else 0


def _checksum(data):
    return (-sum(data)) & 0x7F


def _is_bulk_dump(message, identifier, minimum_size):
    return (
        len(message) >= minimum_size
        and message[0] == 0xF0
        and message[1] == YAMAHA
        and 0 <= message[2] < 16
        and message[3] == FORMAT
        and message[-1] == 0xF7
        and (message[4] << 7) + message[5] == len(message) - 8
        and all(0 <= byte < 0x80 for byte in message[1:-1])
        and bytes(message[6:16]) == identifier
        and message[-2] == _checksum(message[6:-2])
    )


def _voice_address(message):
    if not _is_bulk_dump(message, VOICE, 45):
        return None
    memory, slot = message[MEMORY_TYPE:MEMORY_NUMBER + 1]
    if memory == INTERNAL:
        return memory, slot & 0x3F
    if memory in (PRESET_1, PRESET_2, EDIT) and slot < BANK_SIZE:
        return memory, slot
    return None


def _dump_request(channel, identifier, memory=0, slot=0):
    return [
        0xF0, YAMAHA, 0x20 | _device_number(channel), FORMAT,
        *identifier, *([0] * 14), memory, slot, 0xF7,
    ]


def _with_address(channel, message, memory, slot):
    result = list(message)
    result[2] = _device_number(channel)
    result[MEMORY_TYPE:MEMORY_NUMBER + 1] = [memory, slot]
    result[-2] = _checksum(result[6:-2])
    return result


def bankDescriptors():
    return [
        {"bank": bank, "name": name, "size": BANK_SIZE, "type": "Voice", "isROM": bank != 0}
        for bank, name in enumerate(BANK_NAMES)
    ]


def friendlyBankName(bank):
    return BANK_NAMES[bank]


def friendlyProgramName(program):
    if not 0 <= program < BANK_SIZE * len(BANKS):
        return "Edit voice"
    bank, slot = divmod(program, BANK_SIZE)
    group, position = divmod(slot, 16)
    return f"{BANK_LABELS[bank]}-{'ABCD'[group]}{position + 1:02d}"


def createProgramDumpRequest(channel, program):
    if not 0 <= program < BANK_SIZE * len(BANKS):
        return []
    bank, slot = divmod(program, BANK_SIZE)
    return _dump_request(channel, VOICE, BANKS[bank], slot)


def numberFromDump(message):
    address = _voice_address(message)
    if address is None or address == (EDIT, 0):
        return -1
    memory, slot = address
    # A TG77 front-panel single-voice dump can carry EDIT and its source slot.
    return slot if memory == EDIT else BANKS.index(memory) * BANK_SIZE + slot


def isSingleProgramDump(message):
    return numberFromDump(message) >= 0


def isEditBufferDump(message):
    return _voice_address(message) == (EDIT, 0)


def convertToEditBuffer(channel, message):
    if _voice_address(message) is None:
        return []
    return _with_address(channel, message, EDIT, 0)


def convertToProgramDump(channel, message, program):
    if _voice_address(message) is None or not 0 <= program < BANK_SIZE:
        print("Yamaha SY77/TG77: choose a valid voice and an internal slot from A01 to D16.")
        return []

    voice_type = message[VOICE_TYPE]
    if voice_type in BANK_D_ONLY and program < BANK_D_START:
        print(f"Yamaha SY77/TG77: {BANK_D_ONLY[voice_type]} cannot be stored at "
              f"{friendlyProgramName(program)}. Choose an internal slot from D01 to D16.")
        return []
    return _with_address(channel, message, INTERNAL, program)


def nameFromDump(message):
    return "".join(chr(byte) if 32 <= byte <= 126 else " " for byte in message[VOICE_NAME]).strip()


def isDefaultName(patch_name):
    return patch_name.strip().upper() in ("", "INIT VOICE")


def renamePatch(message, new_name):
    if _voice_address(message) is None:
        return list(message)
    result = list(message)
    result[VOICE_NAME] = [
        ord(character) if " " <= character <= "~" else 0x20
        for character in new_name[:10].ljust(10)
    ]
    result[-2] = _checksum(result[6:-2])
    return result


def calculateFingerprint(message):
    # Ignore addressing, name, framing and checksum. Keep the voice type and
    # effect mode along with the other sound parameters, as in Giovanni's PR.
    return hashlib.md5(bytes(message[VOICE_TYPE:VOICE_TYPE + 1]) + bytes(message[43:-2])).hexdigest()


class Yamaha77Voice:
    def __init__(self, model, first_preset_name):
        self.model = model
        self.first_preset_name = first_preset_name
        self._probe_replies = {}
        self._voice_channels = {}

    def name(self):
        return f"Yamaha {self.model}"

    def setupHelp(self):
        if self.model == "TG77":
            program_change = (
                "For Program change send mode, use Normal mode with Internal selected in Voice Play. "
                "Audition presets using the edit buffer."
            )
        else:
            program_change = "For Program change send mode, set Program Change to Direct."
        return (
            f"This adaptation supports voices. {program_change} "
            "Run detection again after changing the Voice Receive Channel. If the system reply is "
            "unavailable, Program Change uses the SysEx device number's corresponding MIDI channel. "
            "Turn Bulk Protect off on the instrument before writing to internal memory. "
            "4AFM mono, 4AWM, 2AFM+2AWM and Drum Set voices require an internal slot from D01 to D16."
        )

    def needsChannelSpecificDetection(self):
        return True

    def createDeviceDetectMessage(self, channel):
        device = _device_number(channel)
        self._probe_replies[device] = (False, None)
        # The first factory preset identifies the model. The separate system
        # reply provides the MIDI voice channel without changing any settings.
        return _dump_request(device, VOICE, PRESET_1, 0) + _dump_request(device, SYSTEM)

    def channelIfValidDeviceResponse(self, message):
        if _is_bulk_dump(message, SYSTEM, VOICE_RECEIVE_CHANNEL + 3):
            device = message[2]
            matched, _ = self._probe_replies.get(device, (False, None))
            voice_channel = message[VOICE_RECEIVE_CHANNEL]
            voice_channel = voice_channel if voice_channel < 16 else device
            self._probe_replies[device] = (matched, voice_channel)
            if matched:
                self._voice_channels[device] = voice_channel
            return -1

        if _voice_address(message) not in ((PRESET_1, 0), (EDIT, 0)):
            return -1
        if nameFromDump(message) != self.first_preset_name:
            return -1

        device = message[2]
        _, voice_channel = self._probe_replies.get(device, (False, None))
        self._probe_replies[device] = (True, voice_channel)
        self._voice_channels[device] = device if voice_channel is None else voice_channel
        return device

    def createEditBufferRequest(self, channel):
        request = _dump_request(channel, VOICE, EDIT, 0)
        if self.model == "SY77":
            return request

        # The TG77 requires entering Edit before requesting its current voice.
        device = 0x10 | _device_number(channel)
        enter_edit = [0xF0, YAMAHA, device, 0x34, 0x0D, 0, 0, 5, 0, 0x7F, 0xF7]
        enter_voice = [0xF0, YAMAHA, device, 0x34, 0x0D, 0, 0, 0, 0, 0x7F, 0xF7]
        return enter_edit + request + enter_voice

    def createCustomProgramChange(self, channel, program):
        if not 0 <= program < BANK_SIZE * len(BANKS):
            return []
        device = _device_number(channel)
        status = 0xC0 | self._voice_channels.get(device, device)
        bank, slot = divmod(program, BANK_SIZE)
        if self.model == "SY77":
            # Yamaha's Direct mode uses PC 124 for Voice Preset 1.
            return [status, VOICE_MODE_PROGRAM_CHANGES[bank], status, slot]
        if bank == 0:
            return [status, slot]
        print("Yamaha TG77: use edit-buffer send mode to audition preset voices.")
        return []

    def install(self, namespace):
        for name in (
            "name", "setupHelp", "needsChannelSpecificDetection",
            "createDeviceDetectMessage", "channelIfValidDeviceResponse",
            "createEditBufferRequest", "createCustomProgramChange",
        ):
            namespace[name] = getattr(self, name)

        for function in (
            bankDescriptors, friendlyBankName, friendlyProgramName,
            createProgramDumpRequest, numberFromDump, isSingleProgramDump,
            isEditBufferDump, convertToEditBuffer, convertToProgramDump,
            nameFromDump, isDefaultName, renamePatch, calculateFingerprint,
        ):
            namespace[function.__name__] = function
        namespace["isPartOfSingleProgramDump"] = isSingleProgramDump


def voice_test_data(fixture, count, samples, mode_samples=()):
    """Build generic tests from captures of internal voices."""
    import testing

    def programs(data):
        regular_voices = [
            (index, index, name, label) for index, name, label in samples
        ]
        # Giovanni placed the 11 additional modes at D01 to D11.
        mode_voices = [
            (fixture_index, 48 + mode, name, f"IN-D{mode + 1:02d}")
            for mode, (fixture_index, name) in enumerate(mode_samples)
        ]
        return [
            testing.ProgramTestData(
                message=data.all_messages[index], number=slot, name=name,
                friendly_number=label, rename_name="New Patch", target_no=slot,
            )
            for index, slot, name, label in regular_voices + mode_voices
        ]

    def edit_buffers(data):
        return [
            testing.ProgramTestData(
                message=convertToEditBuffer(0, program.message.byte_list),
                name=program.name, rename_name=program.rename_name, target_no=program.target_no,
            )
            for program in programs(data)
        ]

    return testing.TestData(
        sysex=fixture, expected_patch_count=count, program_generator=programs,
        edit_buffer_generator=edit_buffers, friendly_bank_name=(0, "Internal"),
    )
