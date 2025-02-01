import hashlib
from typing import List, Dict

from knobkraft import knobkraft_api

EMU_ID = 0x18
PROTEUS_IDS = {
    0x04: "Proteus 1", 0x05: "Proteus 2", 0x06: "Proteus 3",
    0x07: "Proteus 1 XR", 0x08: "Proteus 2 XR", 0x09: "Proteus 3 XR",
    0x0A: "1+Orchestral"
}
COMMAND_PRESET_REQUEST = 0x00
COMMAND_PRESET_DATA = 0x01
COMMAND_VERSION_REQUEST = 0x0a
COMMAND_VERSION_DATA = 0x0b


class GenericEmu:

    def __init__(self, name: str,  model_id: int, banks: List[Dict]):
        self._name = name
        self._model_id = model_id
        self._banks = banks

    @knobkraft_api
    def name(self):
        return self._name

    @knobkraft_api
    def createDeviceDetectMessage(self, device_id):
        return [0xf0, EMU_ID, self._model_id, device_id & 0x0f, COMMAND_VERSION_REQUEST, 0xf7]

    @knobkraft_api
    def needsChannelSpecificDetection(self):
        return True

    @knobkraft_api
    def deviceDetectWaitMilliseconds(self):
        return 300

    @knobkraft_api
    def channelIfValidDeviceResponse(self, message):
        if (len(message) > 5
                and message[0] == 0xf0
                and message[1] == EMU_ID
                and message[4] == COMMAND_VERSION_DATA):
            if message[2] in PROTEUS_IDS:
                return message[3]  # Device ID is at index 3
        return -1

    @knobkraft_api
    def bankDescriptors(self):
        return self._banks

    @knobkraft_api
    def createProgramDumpRequest(self, device_id, patchNo):
        if not 0 <= patchNo < 320:
            raise ValueError(f"Patch number out of range: {patchNo} should be from 0 to 319")
        return [[0xf0, EMU_ID, model_id, device_id & 0x0f, COMMAND_PRESET_REQUEST, patchNo & 0x7f, (patchNo >> 7) & 0x7f, 0xf7] for model_id in PROTEUS_IDS]

    @knobkraft_api
    def isSingleProgramDump(self, message: List[int]) -> bool:
        return (len(message) > 5
                and message[0] == 0xf0
                and message[1] == EMU_ID
                and message[2] in PROTEUS_IDS
                and message[4] == COMMAND_PRESET_DATA)

    @knobkraft_api
    def nameFromDump(self, message: List[int]) -> str:
        if self.isSingleProgramDump(message):
            offset = 7
            name_chars = []
            for _ in range(12):
                val = message[offset] + (message[offset + 1] << 7)
                name_chars.append(chr(val))
                offset += 2
            return ''.join(name_chars)
        return 'invalid'

    @knobkraft_api
    def numberFromDump(self, message: List[int]) -> int:
        if self.isSingleProgramDump(message):
            return message[5] + (message[6] << 7)
        return -1

    @knobkraft_api
    def convertToProgramDump(self, device_id, message, program_number):
        if self.isSingleProgramDump(message):
            return message[0:3] + [device_id & 0x0f] + message[4:5] + [program_number & 0x7f, (program_number >> 7) & 0x7f] + message[7:]
        raise Exception("Can only convert program dumps")

    @knobkraft_api
    def renamePatch(self, message: List[int], new_name: str) -> List[int]:
        if self.isSingleProgramDump(message):
            name_params = [(ord(c) & 0x7f, (ord(c) >> 7) & 0x7f) for c in new_name.ljust(12, " ")]
            return message[:7] + [item for sublist in name_params for item in sublist] + message[31:]
        raise Exception("Can only rename Presets!")

    @knobkraft_api
    def calculateFingerprint(self, message: List[int]):
        if self.isSingleProgramDump(message):
            data = message[8:-1]
            data[0:24] = [0] * 24
            return hashlib.md5(bytearray(data)).hexdigest()
        raise Exception("Can only fingerprint Presets")

    def install(self, module):
        # This is required because the original KnobKraft modules are not objects, but rather a module namespace with
        # methods declared. Expose our objects methods in the top level module namespace so the C++ code finds it
        for a in dir(self):
            if callable(getattr(self, a)) and hasattr(getattr(self, a), "_is_knobkraft"):
                # this was helpful: http://stupidpythonideas.blogspot.com/2013/06/how-methods-work.html
                setattr(module, a, getattr(self, a))
