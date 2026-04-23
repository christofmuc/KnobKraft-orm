from __future__ import annotations

from collections import deque
from typing import Callable, Deque, Dict, Iterable, List, Optional, Protocol, Tuple

import knobkraft

from testing.librarian import MidiController


MidiMessage = List[int]


class MockDevice(Protocol):
    def respond(self, message: MidiMessage) -> List[MidiMessage]:
        ...


class MockMidiController(MidiController):
    def __init__(self, device: MockDevice):
        super().__init__()
        self.device = device
        self.sent_messages: List[MidiMessage] = []
        self.sent_message_delays: List[int] = []
        self.pending_replies: Deque[MidiMessage] = deque()
        self.finished = False
        self.send_delay = 0
        if hasattr(device, "initial_messages"):
            self.pending_replies.extend([message.copy() for message in device.initial_messages()])

    def send(self, message: MidiMessage):
        for outbound in _split_outbound_messages(message):
            self.sent_messages.append(outbound)
            self.sent_message_delays.append(self.send_delay)
            self.pending_replies.extend(self.device.respond(outbound))

    def receive(self, message: MidiMessage):
        super().receive(message)

    def drain(self, max_steps: int = 10000):
        steps = 0
        while self.pending_replies and not self.finished:
            if steps >= max_steps:
                raise AssertionError(
                    f"Mock MIDI drain exceeded {max_steps} steps; "
                    f"sent={len(self.sent_messages)} pending={len(self.pending_replies)}"
                )
            steps += 1
            self.receive(self.pending_replies.popleft())
        return steps

    def mark_finished(self):
        self.finished = True

    def set_send_delay(self, delay_ms: int):
        self.send_delay = delay_ms


class ScriptedMockDevice:
    def __init__(self, responses: Dict[Tuple[int, ...], Iterable[MidiMessage]], *, ignore_unmatched: bool = False):
        self.responses = {tuple(request): [list(reply) for reply in replies] for request, replies in responses.items()}
        self.ignore_unmatched = ignore_unmatched
        self.unmatched_requests: List[MidiMessage] = []

    def respond(self, message: MidiMessage) -> List[MidiMessage]:
        key = tuple(message)
        if key in self.responses:
            return [reply.copy() for reply in self.responses[key]]
        self.unmatched_requests.append(message.copy())
        if self.ignore_unmatched:
            return []
        raise AssertionError(f"Unexpected MIDI request: {knobkraft.syxToString(message)}")


class BankDumpMockDevice:
    def __init__(self, adaptation, bank_messages: Iterable[MidiMessage], bank: int = 0, channel: int = 0):
        self.expected_request = _first_message(adaptation.createBankDumpRequest(channel, bank))
        self.bank_messages = [message.copy() for message in bank_messages]

    def respond(self, message: MidiMessage) -> List[MidiMessage]:
        if message == self.expected_request:
            return [reply.copy() for reply in self.bank_messages]
        raise AssertionError(f"Unexpected bank request: {knobkraft.syxToString(message)}")


class PassiveBankDumpMockDevice:
    def __init__(
            self,
            bank_messages: Iterable[MidiMessage],
            *,
            accepted_replies: Optional[Iterable[MidiMessage]] = None,
            ignore_unmatched: bool = False):
        self.bank_messages = [message.copy() for message in bank_messages]
        self.accepted_replies = {tuple(reply) for reply in accepted_replies or []}
        self.ignore_unmatched = ignore_unmatched
        self.unmatched_requests: List[MidiMessage] = []

    def initial_messages(self) -> List[MidiMessage]:
        return [message.copy() for message in self.bank_messages]

    def respond(self, message: MidiMessage) -> List[MidiMessage]:
        if tuple(message) in self.accepted_replies:
            return []
        self.unmatched_requests.append(message.copy())
        if self.ignore_unmatched:
            return []
        raise AssertionError(f"Unexpected passive bank reply: {knobkraft.syxToString(message)}")


class ProgramDumpMockDevice:
    def __init__(
            self,
            adaptation,
            programs: Iterable[MidiMessage],
            *,
            channel: int = 0,
            response_for_request: Optional[Callable[[int, MidiMessage], Iterable[MidiMessage]]] = None):
        self.adaptation = adaptation
        self.channel = channel
        self.programs = [program.copy() for program in programs]
        self.response_for_request = response_for_request

    def respond(self, message: MidiMessage) -> List[MidiMessage]:
        for program_no, program in enumerate(self.programs):
            if message == _first_message(self.adaptation.createProgramDumpRequest(self.channel, program_no)):
                if self.response_for_request:
                    return [reply.copy() for reply in self.response_for_request(program_no, message)]
                return knobkraft.splitSysex(program)
        raise AssertionError(f"Unexpected program request: {knobkraft.syxToString(message)}")


class EditBufferMockDevice:
    def __init__(self, adaptation, edit_buffers: Iterable[MidiMessage], *, channel: int = 0):
        self.adaptation = adaptation
        self.channel = channel
        self.edit_buffers = [edit_buffer.copy() for edit_buffer in edit_buffers]
        self.current_program = 0

    def respond(self, message: MidiMessage) -> List[MidiMessage]:
        if _is_program_change(message, self.channel):
            self.current_program = message[1]
            return []
        if message == _first_message(self.adaptation.createEditBufferRequest(self.channel)):
            if not (0 <= self.current_program < len(self.edit_buffers)):
                raise AssertionError(f"Program change selected unavailable edit buffer {self.current_program}")
            return knobkraft.splitSysex(self.edit_buffers[self.current_program])
        raise AssertionError(f"Unexpected edit-buffer request: {knobkraft.syxToString(message)}")


def _split_outbound_messages(message: MidiMessage) -> List[MidiMessage]:
    if not message:
        return []
    result: List[MidiMessage] = []
    index = 0
    while index < len(message):
        status = message[index]
        if status == 0xF0:
            end = index
            while end < len(message) and message[end] != 0xF7:
                end += 1
            if end >= len(message):
                raise ValueError(f"Unterminated SysEx request: {message[index:]}")
            result.append(message[index:end + 1])
            index = end + 1
        elif 0x80 <= status <= 0xBF or 0xE0 <= status <= 0xEF:
            index = _append_midi_message_slice(message, index, 3, result)
        elif 0xC0 <= status <= 0xDF:
            index = _append_midi_message_slice(message, index, 2, result)
        elif 0xF8 <= status <= 0xFF:
            result.append([status])
            index += 1
        else:
            result.append([status])
            index += 1
    return result


def _append_midi_message_slice(message: MidiMessage, index: int, length: int, result: List[MidiMessage]) -> int:
    if index + length > len(message):
        raise ValueError(f"Unterminated MIDI message: {message[index:]}")
    result.append(message[index:index + length])
    return index + length


def _first_message(message: MidiMessage) -> MidiMessage:
    messages = _split_outbound_messages(message)
    if len(messages) != 1:
        raise AssertionError(f"Expected exactly one outbound message, got {len(messages)}")
    return messages[0]


def _is_program_change(message: MidiMessage, channel: int) -> bool:
    return len(message) == 2 and message[0] == (0xC0 | (channel & 0x0F))
