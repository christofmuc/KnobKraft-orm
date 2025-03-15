from functools import partial
from typing import List, Callable, Optional
from enum import Enum
import logging


def adaptation_has_implemented(adaptation, function_implemented: str):
    return hasattr(adaptation, function_implemented)


def adaptation_has_all_implemented(adaptation, methods: List[str]):
    return all(hasattr(adaptation, function_implemented) for function_implemented in methods)


def adaptation_has_bank_dump_capability(adaptation):
    return adaptation_has_all_implemented(adaptation, ["createBankDumpRequest", "isBankDumpFinished"])


def adaptation_has_program_dump_capability(adaptation):
    return adaptation_has_all_implemented(adaptation, ["createProgramDumpRequest", "isSingleProgramDump"])


def adaptation_has_edit_buffer_capability(adaptation):
    return adaptation_has_all_implemented(adaptation, ["createEditBufferRequest", "isEditBufferDump"])


MidiMessageHandler = Callable[[List[int]], None]


class MidiController:
    def __init__(self):
        self.handlers: List[MidiMessageHandler] = []

    def add_message_handler(self, handler: MidiMessageHandler) -> None:
        self.handlers.append(handler)

    def send(self, message: List[int]):
        pass

    def receive(self, message: List[int]):
        for handler in self.handlers:
            handler(message)


class BankDownloadMethod(Enum):
    BANKS = 1
    PROGRAM_BUFFERS = 2
    EDIT_BUFFERS = 3


class Librarian:

    def __init__(self):
        self.download_number = 0  # The current patch number we try to download
        self.start_download_number = 0  # The number of the first patch we download
        self.end_download_number = 0  # The number of the last patch we download to get a whole bank
        self.current_download_messages: List[List[int]] = []  # The list of patches already successfully downloaded
        self.current_edit_buffer: List[int] = []  # Messages collected for the current incoming edit buffer
        self.current_download_program_dump: List[int] = []  # Messages collected for the current incoming program buffer
        self.on_finished: Optional[Callable[[List[List[int]]], None]] = None  # This function is called with the result of load operation

    def determine_bank_download_method(self, adaptation) -> BankDownloadMethod:
        if adaptation_has_implemented(adaptation, "bankDownloadMethodOverride"):
            download_method = adaptation.bankDownloadMethodOverride()
            if download_method == "EDITBUFFERS":
                return BankDownloadMethod.EDIT_BUFFERS
            elif download_method == "PROGRAMS":
                return BankDownloadMethod.PROGRAM_BUFFERS
            else:
                raise Exception("bankDownloadMethodOverride must return either EDITBUFFERS or PROGRAMS as string. BankDumps would be default anyway.")

        # Default behavior - use the "best possible"
        if adaptation_has_bank_dump_capability(adaptation):
            return BankDownloadMethod.BANKS
        elif adaptation_has_program_dump_capability(adaptation):
            return BankDownloadMethod.PROGRAM_BUFFERS
        elif adaptation_has_edit_buffer_capability(adaptation):
            return BankDownloadMethod.EDIT_BUFFERS
        else:
            raise Exception("Adaptation has implemented none of BankDumpCapbility, ProgramDumpCapability, or EditBufferCapability. Can't download patches")

    def start_downloading_all_patches(
            self,
            midi_controller,
            channel: int,
            adaptation,
            bank_no: int,
            on_finished: Callable[[List[List[int]]], None]
    ) -> None:
        # Initialize state
        self.download_number = 0
        self.on_finished = on_finished
        self.current_download_messages.clear()

        # Determine the download method
        method = self.determine_bank_download_method(adaptation)

        if method == BankDownloadMethod.BANKS:
            buffer = adaptation.createBankDumpRequest(channel, bank_no)
            midi_controller.send(buffer)
            midi_controller.add_message_handler(partial(self._handle_next_bank_dump, midi_controller=midi_controller, adaptation=adaptation))

        elif method == BankDownloadMethod.PROGRAM_BUFFERS:
            midi_controller.add_message_handler(partial(self._handle_next_program_buffer, midi_controller=midi_controller, adaptation=adaptation, bank_no=bank_no))
            self.download_number = SynthBank.start_index_in_bank(adaptation, bank_no)
            self.start_download_number = self.download_number
            self.end_download_number = self.download_number + SynthBank.number_of_patches_in_bank(adaptation, bank_no)
            self._start_download_next_program_buffer(midi_controller, adaptation, channel)

        elif method == BankDownloadMethod.EDIT_BUFFERS:
            midi_controller.add_message_handler(partial(self._handle_next_edit_buffer, midi_controller=midi_controller, adaptation=adaptation, bank_no=bank_no))
            self.download_number = SynthBank.start_index_in_bank(adaptation, bank_no)
            self.start_download_number = self.download_number
            self.end_download_number = self.download_number + SynthBank.number_of_patches_in_bank(adaptation, bank_no)
            self._start_download_next_edit_buffer(midi_controller, adaptation, channel, True)

        else:
            logging.error("Error: This synth has not implemented a single method to retrieve a bank. Please consult the documentation!")

    def _handle_next_bank_dump(self, message: List[int], midi_controller: MidiController, adaptation) -> None:
        """
        Handle the next message incoming during a bank dump download
        """
        if adaptation.adaptation_has_implemented("isPartOfBankDump") and adaptation.isPartOfBankDump(message):
            # This is part of the bank dump. Store in self.current_download_message and check if we're done
            self.current_download_messages.append(message)
            if adaptation.isBankDumpFinished(self.current_download_messages):
                patches = self.load_sysex(self.current_download_messages)
                self.on_finished(patches)
        elif adaptation.isBankDumpFinished(message):
            # Simple case - the bank dump is only a single message and no isPartOfBankDump() has been implemented
            patches = self.load_sysex(self.current_download_messages)
            self.on_finished(patches)

    def _start_download_next_program_buffer(self, midi_controller: MidiController, adaptation, channel: int) -> None:
        """
        Start the download of the next program buffer
        """
        self.current_download_program_dump.clear()
        messages = adaptation.createProgramDumpRequest(channel, self.download_number)
        midi_controller.send(messages)

    def _handle_next_program_buffer(self, message: List[int], midi_controller: MidiController, adaptation, channel: int) -> None:
        """
        Check if the message received is part of an edit buffer, if yes check if the program dump is complete or even the whole bank
        is complete.
        """
        next_message = None
        if adaptation_has_implemented(adaptation, "isPartOfSingleProgramDump"):
            handshake = adaptation.isPartOfSingleProgramDump(message)
            if isinstance(handshake, tuple):
                is_part_of_program_buffer, next_message = handshake
            elif isinstance(handshake, bool):
                is_part_of_program_buffer = handshake
            else:
                raise Exception("Expected tuple or bool from isPartOfSingleProgramDump")
        else:
            # Simple case: program buffers are single messages
            is_part_of_program_buffer = adaptation.isSingleProgramDump(message)

        if is_part_of_program_buffer:
            self.current_download_program_dump.extend(message)

            # See if we should send a reply (ACK)
            if next_message:
                midi_controller.send(next_message)

            # Do we have a complete program dump?
            if adaptation.isSingleProgramDump(self.current_download_program_dump):
                self.current_download_messages.append(self.current_download_program_dump)
                # Finished?
                if self.download_number >= self.end_download_number - 1:
                    patches = self.load_sysex(self.current_download_messages)
                    self.on_finished(patches)
                else:
                    self.download_number += 1
                    self._start_download_next_program_buffer(midi_controller, adaptation, channel)

    def _start_download_next_edit_buffer(self, midi_controller: MidiController, adaptation, channel: int, send_program_change: bool) -> None:
        """
        Start the download of the next edit buffer. An edit buffer can consist of multiple MIDI messages which we'll going to collect in the
        current_edit_buffer member.

        This creates a program change message and a subsequent get edit buffer message
        """
        self.current_edit_buffer.clear()
        messages = []
        if send_program_change:
            messages.extend([0xc0 | (channel & 0x0f), self.download_number & 0x7f])
        messages.extend(adaptation.createEditBufferRequest(channel))
        self.download_number = self.end_download_number
        midi_controller.send(messages)

    def _handle_next_edit_buffer(self, message: List[int], midi_controller: MidiController, adaptation, channel: int) -> None:
        """
        This is called when the next MIDI message is received from the synth. Check if this is part of the edit buffer, if we need to answer
        with a handshake message and whether the bank download is done.
        """
        next_message = None
        if adaptation_has_implemented(adaptation, "isPartOfEditBufferDump"):
            handshake = adaptation.isPartOfEditBufferDump(message)
            if isinstance(handshake, tuple):
                is_part_of_edit_buffer, next_message = handshake
            elif isinstance(handshake, bool):
                is_part_of_edit_buffer = handshake
            else:
                raise Exception("Expected tuple or bool from isPartOfEditBufferDump")
        else:
            # Simple case, single message edit buffers
            is_part_of_edit_buffer = adaptation.isEditBufferDump(message)

        if is_part_of_edit_buffer:
            if next_message:
                midi_controller.send(next_message)
            self.current_edit_buffer.extend(message)

            if adaptation.isEditBufferDump(self.current_edit_buffer):
                # Got one complete edit buffer!
                self.current_download_messages.append(self.current_edit_buffer)

                # Are we done?
                if self.download_number >= self.end_download_number - 1:
                    patches = self.load_sysex(self.current_download_messages)
                    self.on_finished(patches)
                else:
                    self.download_number += 1
                    # Continue with the next edit buffer
                    self._start_download_next_edit_buffer(midi_controller, adaptation, channel, True)

    def load_sysex(self, messages) -> List[List[int]]:
        return []
