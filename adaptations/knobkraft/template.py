import abc
from dataclasses import dataclass
from typing import List, Union, Tuple


def knobkraft_api(func):
    func._is_knobkraft = True
    return func


@dataclass
class BankDescriptor:
    bank: int  # The number of the bank. Should be zero-based
    name: str  # The friendly name of the bank
    size: int  # The number of items in this bank. This allows for banks of different sizes for one synth
    type: str  # A text describing the type of data in this bank. Could be "Patch", "Tone", ... Will be displayed in the metadata.
    isROM: bool # Use this to indicate for later bank management functionality that the bank can be read, but not written to


class SynthBase(abc.ABC):

    def __init__(self, name, banks: List[BankDescriptor], needs_channel_specific_detection=True, device_detect_ms=200):
        self._name = name
        self._banks = banks
        self._needs_channel_specific_detection = needs_channel_specific_detection
        self._device_detect_ms = device_detect_ms

    @knobkraft_api
    def name(self):
        return self._name

    @knobkraft_api
    def bankDescriptors(self) -> List[BankDescriptor]:
        return self._banks

    @knobkraft_api
    @abc.abstractmethod
    def bankSelect(self, channel: int, bank: int): ...

    @knobkraft_api
    @abc.abstractmethod
    def createDeviceDetectMessage(self, channel: int) -> List[int]: ...

    @knobkraft_api
    @abc.abstractmethod
    def channelIfValidDeviceResponse(self, message: List[int]) -> bool: ...

    @knobkraft_api
    def needsChannelSpecificDetection(self) -> bool:
        return self._needs_channel_specific_detection

    @knobkraft_api
    def deviceDetectWaitMilliseconds(self) -> int:
        return self._device_detect_ms

    @knobkraft_api
    def install(self, module):
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        for a in dir(self):
            if callable(getattr(self, a)) and hasattr(getattr(self, a), "_is_knobkraft"):
                # this was helpful: http://stupidpythonideas.blogspot.com/2013/06/how-methods-work.html
                setattr(module, a, getattr(self, a))


class EditBufferCapability(abc.ABC):

    @abc.abstractmethod
    @knobkraft_api
    def createEditBufferRequest(self, channel: int) -> List[int]: ...

    @abc.abstractmethod
    @knobkraft_api
    def isEditBufferDump(self, message: List[int]) -> bool: ...

    @abc.abstractmethod
    @knobkraft_api
    def convertToEditBuffer(self, channel: int, message: List[int]) -> List[int]: ...

    @abc.abstractmethod
    @knobkraft_api
    def isPartOfEditBufferDump(self, message: List[int]) -> Union[bool, Tuple[bool, List[int]]]: ...


class ProgramDumpCapability(abc.ABC):

    @abc.abstractmethod
    def createProgramDumpRequest(self, channel: int, patchNo: int) -> List[int]: ...

    @abc.abstractmethod
    def isSingleProgramDump(self, message: List[int]) -> bool: ...

    @abc.abstractmethod
    def convertToProgramDump(self, channel: int, message: List[int], program_number: int) -> List[int]: ...

    @abc.abstractmethod
    def numberFromDump(self, message) -> int: ...

    @abc.abstractmethod
    def isPartOfSingleProgramDump(self, message: list[int]) -> Union[bool, Tuple[bool, List[int]]]: ...


class BankDumpCapability(abc.ABC):

    @abc.abstractmethod
    def createBankDumpRequest(self, channel: int, bank: int) -> List[int]: ...

    @abc.abstractmethod
    def isPartOfBankDump(self, message: List[int]) -> bool: ...

    @abc.abstractmethod
    def isBankDumpFinished(self, messages: List[int]) -> bool: ...

    @abc.abstractmethod
    def extractPatchesFromBank(self, messages: List[int]) -> List[List[int]]: ...

    @abc.abstractmethod
    def extractPatchesFromAllBankMessages(self, messages: List[List[int]]) -> List[List[int]]: ...

