from collections import deque
from functools import partial
from typing import List, Callable, Optional, Dict, Deque, Any
from enum import Enum
import logging

import knobkraft
from testing.test_data import MidiController


def adaptation_has_implemented(adaptation, function_implemented: str):
    return hasattr(adaptation, function_implemented)


def adaptation_has_all_implemented(adaptation, methods: List[str]):
    return all(hasattr(adaptation, function_implemented) for function_implemented in methods)


def adaptation_has_bank_dump_capability(adaptation):
    return adaptation_has_all_implemented(adaptation, ["isBankDumpFinished"])


def adaptation_has_program_dump_capability(adaptation):
    return adaptation_has_all_implemented(adaptation, ["createProgramDumpRequest", "isSingleProgramDump"])


def adaptation_has_edit_buffer_capability(adaptation):
    return adaptation_has_all_implemented(adaptation, ["createEditBufferRequest", "isEditBufferDump"])


def flatten(xss: List[List[Any]]) -> List[Any]:
    return [x for xs in xss for x in xs]


class SynthBank:

    @staticmethod
    def start_index_in_bank(adaptation, bank_no):
        if adaptation_has_implemented(adaptation, "bankDescriptors"):
            banks = adaptation.bankDescriptors()
            index = 0
            for b in range(bank_no):
                index += banks[b]["size"]
            return index
        else:
            return bank_no * adaptation.numberOfPatchesPerBank()

    @staticmethod
    def number_of_patches_in_bank(adaptation, bank_no):
        if adaptation_has_implemented(adaptation, "bankDescriptors"):
            banks = adaptation.bankDescriptors()
            return banks[bank_no]["size"]
        else:
            return adaptation.numberOfPatchesPerBank()


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
        self.max_number_messages_per_patch = 14  # This is to limit memory and computation time when loading large files
        self.max_number_messages_per_bank = 256  # Again, we need to limit miscalculations. But banks might have many more messages

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
            midi_controller.add_message_handler(partial(self._handle_next_bank_dump, midi_controller=midi_controller, adaptation=adaptation))
            midi_controller.send(buffer)

        elif method == BankDownloadMethod.PROGRAM_BUFFERS:
            midi_controller.add_message_handler(partial(self._handle_next_program_buffer, midi_controller=midi_controller, adaptation=adaptation, channel=channel))
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
        if adaptation_has_implemented(adaptation, "isPartOfBankDump") and adaptation.isPartOfBankDump(message):
            # This is part of the bank dump. Store in self.current_download_message and check if we're done
            self.current_download_messages.append(message)
            if adaptation.isBankDumpFinished(self.current_download_messages):
                patches = self.load_sysex(adaptation, self.current_download_messages)
                self.on_finished(patches)
        elif adaptation.isBankDumpFinished([message]):
            # Simple case - the bank dump is only a single message and no isPartOfBankDump() has been implemented
            patches = self.load_sysex(adaptation, self.current_download_messages)
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
                    patches = self.load_sysex(adaptation, self.current_download_messages)
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

    def load_sysex(self, adaptation, sysex_messages: List[List[int]]) -> List[List[int]]:
        results: List[List[int]] = []
        program_dumps_by_id: Dict[str, List[int]] = {}

        if adaptation_has_program_dump_capability(adaptation):
            current_program_dumps: Deque[List[int]] = deque()
            for message in sysex_messages:
                # Try to parse and load these messages as program dumps
                if adaptation_has_implemented(adaptation, "isPartOfSingleProgramDump") and adaptation.isPartOfSingleProgramDump(message) or adaptation.isSingleProgramDump(message):
                    current_program_dumps.append(message)
                    while len(current_program_dumps) > self.max_number_messages_per_patch:
                        logging.debug(f"Dropping message during parsing as potential number of MIDI messages per patch is larger than {self.max_number_messages_per_patch}")
                        current_program_dumps.popleft()
                    sliding_window: List[int] = flatten(list(current_program_dumps))
                    if adaptation.isSingleProgramDump(sliding_window):
                        patch = sliding_window
                        results.append(patch)
                        if adaptation_has_implemented(adaptation, "calculateFingerprint"):
                            program_dumps_by_id[adaptation.calculateFingerprint(patch)] = patch
                        current_program_dumps.clear()

        if adaptation_has_edit_buffer_capability(adaptation):
            current_edit_buffers: Deque[List[int]] = deque()
            patch_no = 0
            for message in sysex_messages:
                # Try to parse and load these messages as edit buffers
                if adaptation_has_implemented(adaptation, "isPartOfEditBufferDump") and adaptation.isPartOfEditBufferDump(message) or adaptation.isEditBufferDump(message):
                    current_edit_buffers.append(message)
                    if len(current_edit_buffers) > self.max_number_messages_per_patch:
                        logging.debug(f"Dropping message during parsing as potential number of MIDI messages per patch is larger than {self.max_number_messages_per_patch}")
                        current_edit_buffers.popleft()
                    sliding_window = flatten(list(current_edit_buffers))
                    if adaptation.isEditBufferDump(sliding_window):
                        patch = sliding_window
                        if adaptation_has_implemented(adaptation, "calculateFingerprint"):
                            patch_id = adaptation.calculateFingerprint(patch)
                            if patch_id not in program_dumps_by_id:
                                results.append(patch)
                            else:
                                # Ignore edit buffer, as we already loaded a program dump with the same ID
                                pass
                        else:
                            results.append(patch)
                        current_edit_buffers.clear()

        if adaptation_has_bank_dump_capability(adaptation):
            current_bank: Deque[List[int]] = deque()
            for message in sysex_messages:
                # Try to parse and load these messages as a bank dump
                if adaptation_has_implemented(adaptation, "isPartOfBankDump") and adaptation.isPartOfBankDump(message) or adaptation.isBankDumpFinished([message]):
                    current_bank.append(message)
                    if len(current_bank) > self.max_number_messages_per_bank:
                        logging.debug(f"Dropping message during parsing as potential number of MIDI messages per bank is larger than {self.max_number_messages_per_bank}")
                        current_bank.popleft()
                    sliding_window: List[List[int]] = list(current_bank)
                    if adaptation.isBankDumpFinished(sliding_window):
                        if adaptation_has_implemented(adaptation, "extractPatchesFromAllBankMessages"):
                            more_patches = adaptation.extractPatchesFromAllBankMessages(sliding_window)
                            logging.info(f"Loaded bank dump with {len(more_patches)} patches")
                            results.extend(more_patches)
                            current_bank.clear()
                        else:
                            more_patches = adaptation.extractPatchesFromBank(sliding_window[0])
                            if more_patches:
                                logging.info(f"Loaded bank dump from single message with {len(more_patches)} patches")
                                split_patches = knobkraft.splitSysex(more_patches)
                                results.extend(split_patches)
                            current_bank.clear()


        return results
