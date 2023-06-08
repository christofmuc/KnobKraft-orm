from dataclasses import dataclass
from typing import Optional, Union, List, Callable, Tuple, Any
import knobkraft


class MidiMessage:
    def __init__(self, message: Union[List[int], str]):
        if isinstance(message, str):
            self.byte_list = knobkraft.stringToSyx(message)
        elif isinstance(message, list):
            if not all(0 <= x < 256 for x in message):
                raise ValueError("All MIDI message values must be in the range 0 to 255")
            self.byte_list = message
        else:
            raise ValueError("Please specify MIDI messages either as string of hex values or as a list of bytes!")

    def __eq__(self, other):
        if isinstance(other, MidiMessage):
            return self.byte_list == other.byte_list
        else:
            return self.byte_list == other


def make_midi_message(message: Optional[Union[MidiMessage, List[int], str]]) -> Optional[MidiMessage]:
    if message is None:
        return None
    elif isinstance(message, MidiMessage):
        return message
    else:
        return MidiMessage(message)


@dataclass
class ProgramTestData:
    message: Union[MidiMessage, List[int], str]
    name: Optional[str] = None
    number: Optional[int] = None
    rename_name: Optional[str] = None
    target_no: Optional[int] = None
    second_layer_name: Optional[str] = None

    def __post_init__(self):
        self.message = make_midi_message(self.message)


@dataclass
class TestData:
    sysex: Optional[str] = None
    program_generator: Optional[Callable[[Any], List[ProgramTestData]]] = None
    edit_buffer_generator: Optional[Callable[[Any], List[ProgramTestData]]] = None
    program_dump_request: Optional[Union[MidiMessage, List[int], str]] = None
    device_detect_call: Optional[Union[MidiMessage, List[int], str]] = None
    device_detect_reply: Optional[Tuple[Union[MidiMessage, List[int], str], int]] = None
    friendly_bank_name: Optional[Tuple[int, str]] = None
    convert_to_edit_buffer_produces_program_dump: bool = False
    rename_name: Optional[str] = None
    not_idempotent: bool = False

    def __post_init__(self):
        self.all_messages = []
        if self.sysex:
            self.all_messages = knobkraft.load_sysex(self.sysex)
        if self.program_generator:
            self.programs = self.program_generator(self)
        else:
            self.programs = []
        if self.edit_buffer_generator:
            self.edit_buffers = self.edit_buffer_generator(self)
        else:
            self.edit_buffers = []
        self.program_dump_request = make_midi_message(self.program_dump_request)
        self.device_detect_call = make_midi_message(self.device_detect_call)
        self.device_detect_reply = None if self.device_detect_reply is None else (make_midi_message(self.device_detect_reply[0]), self.device_detect_reply[1])
