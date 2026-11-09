#
#   Roland D-110 adaptation, contributed to KnobKraft Orm under its dual licensing
#   (Affero GPL by default, an MIT license is available for purchase from Christof Ruch).
#
#   Tones only. The SysEx layout follows Sean Luke's Edisyn D-110 Tone editor
#   (https://github.com/eclab/edisyn, edisyn/synth/rolandd110, Apache 2.0), which is marked librarian-tested:
#   - a tone is one block of 246 bytes: tone common (name in the first 10 bytes) and four partials;
#   - the temporary tone of part 1 is at 04 00 00;
#   - the 64 internal tones are at 08 00 00, one every 0x100 bytes in the 7-bit address space, so the
#     middle address byte steps by 2 (08 02 00 is tone 2). A tone read from memory in a bulk dump
#     carries 256 bytes, 10 more than the temporary tone; they are not part of the sound.
#
import sys
from typing import List

import testing
from roland import DataBlock, RolandData, GenericRoland
from roland.GenericRoland import knobkraft_api, command_dt1

this_module = sys.modules[__name__]

_tone_size = 246
_memory_tone_size = 256

_tone = [DataBlock((0x00, 0x00, 0x00), (0x01, 0x76), "Tone common and partials")]
_temporary_tone = RolandData("D-110 Temporary Tone (part 1)", num_items=1, num_address_bytes=3, num_size_bytes=3,
                             base_address=(0x04, 0x00, 0x00), blocks=_tone)
_internal_tones = RolandData("D-110 Internal Tone", num_items=64, num_address_bytes=3, num_size_bytes=3,
                             base_address=(0x08, 0x00, 0x00), blocks=_tone,
                             supported_layouts=[(_memory_tone_size,)])
# A small read of the system area, which answers on the D-110's unit number
_system = RolandData("D-110 System", 1, 3, 3, (0x10, 0x00, 0x00), [DataBlock((0x00, 0x00, 0x00), 0x01, "Master tune")])


class RolandD110(GenericRoland):

    def _address_for_sub_request(self, layout, block_no, item):
        if layout is self.program_dump:
            return [0x08, item * 2, 0x00], DataBlock.size_as_7bit_list(_tone_size, 3)
        return super()._address_for_sub_request(layout, block_no, item)

    def _tone_data(self, message):
        _, blocks, context = self._validated_dump(message)
        return blocks[0][1][:_tone_size], context

    @knobkraft_api
    def convertToEditBuffer(self, channel, message):
        # A memory tone's 10 extra bytes would spill into part 2's temporary tone
        data, _ = self._tone_data(message)
        address, _ = self._address_for_sub_request(self.edit_buffer, 0, 0)
        return self.buildRolandMessage(self.device_id, command_dt1, address, data)

    @knobkraft_api
    def convertToProgramDump(self, channel, message, program_number):
        data, _ = self._tone_data(message)
        address, _ = self._address_for_sub_request(self.program_dump, 0, program_number % self.program_dump.num_items)
        return self.buildRolandMessage(self.device_id, command_dt1, address, data)

    @knobkraft_api
    def nameFromDump(self, message) -> str:
        # Names are padded with spaces, sometimes with zeros
        name = super().nameFromDump(message)
        return ''.join(c if 32 <= ord(c) < 127 else ' ' for c in name)

    def blankedOut(self, message):
        # The fingerprint of the 246 bytes of the sound, wherever it is stored and however long the stored block
        data, _ = self._tone_data(message)
        data = data.copy()
        data[0:self.patch_name_length] = [0] * self.patch_name_length
        return self.buildRolandMessage(0x10, command_dt1, [0x08, 0x00, 0x00], data)

    @knobkraft_api
    def bankDescriptors(self):
        return [{"bank": 0, "name": "Internal Tones", "size": 64, "type": "Tone"}]


d110 = RolandD110("Roland D-110",
                  model_id=[0x16],
                  address_size=3,
                  edit_buffer=_temporary_tone,
                  program_dump=_internal_tones,
                  device_detect_message=_system,
                  patch_name_length=10)
d110.install(this_module)


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        # Edisyn's initial D-110 tone (edisyn/synth/rolandd110/RolandD110Tone.init, Apache 2.0), a memory tone
        # as a bulk dump writes it: internal tone 1, 256 bytes.
        init_tone = ("F0 41 10 16 12 08 00 00 44 31 31 30 20 49 6E 69 74 00 00 00 01 00 18 32 0B 01 00 49 3F 07 00 00 "
                     "00 0A 00 00 00 64 32 32 32 32 00 00 00 58 00 0B 40 07 40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 "
                     "39 40 0C 40 0C 00 00 00 2C 3E 51 19 64 47 42 3E 18 32 0B 01 00 49 3F 07 00 00 00 0A 00 00 00 64 "
                     "32 32 32 32 00 00 00 64 00 0B 40 07 40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 39 40 0C 40 0C 00 "
                     "00 00 2C 3E 51 19 64 47 42 3E 18 32 0B 01 00 49 3F 07 00 00 00 00 00 00 00 64 32 32 32 32 00 00 "
                     "00 64 00 0B 40 07 40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 39 40 0C 40 0C 00 00 00 2C 3E 51 19 "
                     "64 47 42 3E 18 32 0B 01 00 49 3F 07 00 00 00 00 00 00 00 64 32 32 32 32 00 00 00 64 00 0B 40 07 "
                     "40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 39 40 0C 40 0C 00 00 00 2C 3E 51 19 64 47 42 3E 00 00 "
                     "00 00 00 00 00 00 00 00 2D F7")
        # Edisyn pads its name with a zero; the D-110 pads with spaces, as renamePatch does
        edisyn_init = [int(x, 16) for x in init_tone.split()]
        assert d110.nameFromDump(edisyn_init) == "D110 Init "
        message = d110.renamePatch(edisyn_init, "D110 Init")
        # The same tone as the D-110 sends it when asked for internal tone 5
        fifth = d110.convertToProgramDump(0, message, 4)
        assert fifth[5:8] == [0x08, 0x08, 0x00] and len(fifth) == 8 + _tone_size + 2
        assert d110.numberFromDump(fifth) == 4
        assert d110.createProgramDumpRequest(0, 4) == [0xF0, 0x41, 0x10, 0x16, 0x11, 0x08, 0x08, 0x00, 0x00, 0x01, 0x76, 0x79, 0xF7]
        edit_buffer = d110.convertToEditBuffer(0, message)
        assert edit_buffer[5:8] == [0x04, 0x00, 0x00] and len(edit_buffer) == 8 + _tone_size + 2
        assert d110.createEditBufferRequest(0) == [0xF0, 0x41, 0x10, 0x16, 0x11, 0x04, 0x00, 0x00, 0x00, 0x01, 0x76, 0x05, 0xF7]
        yield testing.ProgramTestData(message=message, name="D110 Init ", number=0, rename_name="Warm Pad  ")
        yield testing.ProgramTestData(message=fifth, name="D110 Init ", number=4)

    def edit_buffers(data: testing.TestData) -> List[testing.ProgramTestData]:
        for program in data.programs:
            yield testing.ProgramTestData(message=d110.convertToEditBuffer(0, program.message.byte_list), name=program.name)

    return testing.TestData(program_generator=programs,
                            edit_buffer_generator=edit_buffers,
                            device_detect_call="F0 41 10 16 11 10 00 00 00 00 01 6F F7",
                            device_detect_reply=("F0 41 10 16 12 10 00 00 4A 26 F7", 0))
