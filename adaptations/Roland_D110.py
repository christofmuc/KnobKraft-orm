#
#   Roland D-110 adaptation, contributed to KnobKraft Orm under its dual licensing
#   (Affero GPL by default, an MIT license is available for purchase from Christof Ruch).
#
#   Tones only. Page numbers are those of the MIDI implementation in the Roland D-110 Owner's Manual:
#   - p. 116, "Exclusive communication": model ID 16H, Request data 1 (RQ1, 11H) and Data set 1 (DT1, 12H). The
#     Device ID is the Unit # less 1, and the D-110 recognizes Unit # 17 to 32: 10H to 1FH;
#   - p. 117, "Parameter base address", areas accessible on the Unit #: Tone Temporary Area (part 1) at 04 00 00,
#     Tone Memory #1 to #64 at 08 00 00 to 08 7E 00 (08 02 00 is Tone Memory #2: the middle address byte steps
#     by 2), System Area at 10 00 00;
#   - p. 117 and 118, tables 5-1, 5-1-1 and 5-1-2: a tone is the Common parameter (00 00 0E bytes, TONE NAME 1 to
#     10 first) and four Partial parameters (00 00 3A bytes each), 246 bytes (00 01 76) in all;
#   - p. 110, "One-way transfer procedure": intervals of at least 20 milliseconds between messages.
#   Not in the manual, but in Sean Luke's Edisyn D-110 Tone editor (https://github.com/eclab/edisyn,
#   edisyn/synth/rolandd110, Apache 2.0), which is marked librarian-tested: a Tone Memory in a bulk dump carries
#   256 bytes, the whole of its place in memory. The last 10 are not part of the sound.
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
# A one-byte read of the System Area (p. 118, table 5-6), which answers on the D-110's Unit #
_system = RolandData("D-110 System", 1, 3, 3, (0x10, 0x00, 0x00), [DataBlock((0x00, 0x00, 0x00), 0x01, "Master tune")])


class RolandD110(GenericRoland):

    def _address_for_sub_request(self, layout, block_no, item):
        if layout is self.program_dump:
            return [0x08, item * 2, 0x00], DataBlock.size_as_7bit_list(_tone_size, 3)
        return super()._address_for_sub_request(layout, block_no, item)

    def _tone_data(self, message):
        _, blocks, _ = self._validated_dump(message)
        return blocks[0][1][:_tone_size]

    def _tone_to_send(self, message):
        # TONE NAME 1 to 10 are ASCII 32 to 127 (p. 117, table 5-1-1). Files of other editors pad the name with
        # zeros: they are accepted, and the D-110 is sent spaces.
        data = self._tone_data(message)
        name = slice(self.patch_name_offset, self.patch_name_offset + self.patch_name_length)
        data[name] = [c if c >= 0x20 else 0x20 for c in data[name]]
        return data

    @knobkraft_api
    def convertToEditBuffer(self, channel, message):
        # Data set 1 (DT1) to the Tone Temporary Area of part 1 (p. 116 and 117). A Tone Memory's 10 extra bytes
        # would spill into the Tone Temporary Area of part 2 at 04 01 76.
        address, _ = self._address_for_sub_request(self.edit_buffer, 0, 0)
        return self.buildRolandMessage(self.device_id, command_dt1, address, self._tone_to_send(message))

    @knobkraft_api
    def convertToProgramDump(self, channel, message, program_number):
        # Data set 1 (DT1) to a Tone Memory (p. 116 and 117)
        address, _ = self._address_for_sub_request(self.program_dump, 0, program_number % self.program_dump.num_items)
        return self.buildRolandMessage(self.device_id, command_dt1, address, self._tone_to_send(message))

    @knobkraft_api
    def generalMessageDelay(self) -> int:
        # One-way transfer procedure (p. 110): "intervals of at least 20 milliseconds" between messages
        return 25

    @knobkraft_api
    def nameFromDump(self, message) -> str:
        # Names are padded with spaces, sometimes with zeros
        name = super().nameFromDump(message)
        return ''.join(c if 32 <= ord(c) < 127 else ' ' for c in name)

    def blankedOut(self, message):
        # The fingerprint of the 246 bytes of the sound, wherever it is stored and however long the stored block
        data = self._tone_data(message)
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


def edisyn_init_tone() -> List[int]:
    # Edisyn's initial D-110 tone (edisyn/synth/rolandd110/RolandD110Tone.init, Apache 2.0), a Tone Memory as a
    # bulk dump writes it: Tone Memory #1, 256 bytes. Edisyn pads its name with a zero.
    init_tone = ("F0 41 10 16 12 08 00 00 44 31 31 30 20 49 6E 69 74 00 00 00 01 00 18 32 0B 01 00 49 3F 07 00 00 "
                 "00 0A 00 00 00 64 32 32 32 32 00 00 00 58 00 0B 40 07 40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 "
                 "39 40 0C 40 0C 00 00 00 2C 3E 51 19 64 47 42 3E 18 32 0B 01 00 49 3F 07 00 00 00 0A 00 00 00 64 "
                 "32 32 32 32 00 00 00 64 00 0B 40 07 40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 39 40 0C 40 0C 00 "
                 "00 00 2C 3E 51 19 64 47 42 3E 18 32 0B 01 00 49 3F 07 00 00 00 00 00 00 00 64 32 32 32 32 00 00 "
                 "00 64 00 0B 40 07 40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 39 40 0C 40 0C 00 00 00 2C 3E 51 19 "
                 "64 47 42 3E 18 32 0B 01 00 49 3F 07 00 00 00 00 00 00 00 64 32 32 32 32 00 00 00 64 00 0B 40 07 "
                 "40 00 00 00 00 1E 47 59 3F 64 3E 3B 39 64 39 40 0C 40 0C 00 00 00 2C 3E 51 19 64 47 42 3E 00 00 "
                 "00 00 00 00 00 00 00 00 2D F7")
    return [int(x, 16) for x in init_tone.split()]


def make_test_data():
    from testing.mock_midi import ScriptedMockDevice

    # The D-110 pads a name with spaces, as renamePatch does
    message = d110.renamePatch(edisyn_init_tone(), "D110 Init")
    tone = message[8:8 + _tone_size]
    # The same tone as the D-110 sends it when asked for Tone Memory #5, and for the Tone Temporary Area of part 1
    fifth = [0xF0, 0x41, 0x10, 0x16, 0x12, 0x08, 0x08, 0x00] + tone + [GenericRoland.roland_checksum([0x08, 0x08, 0x00] + tone), 0xF7]
    temporary = [0xF0, 0x41, 0x10, 0x16, 0x12, 0x04, 0x00, 0x00] + tone + [GenericRoland.roland_checksum([0x04, 0x00, 0x00] + tone), 0xF7]
    edit_buffer_request = [0xF0, 0x41, 0x10, 0x16, 0x11, 0x04, 0x00, 0x00, 0x00, 0x01, 0x76, 0x05, 0xF7]

    def tone_memory_request(number: int) -> List[int]:
        # Request data 1 (RQ1) for the 246 bytes of one Tone Memory
        address_and_size = [0x08, number * 2, 0x00, 0x00, 0x01, 0x76]
        return [0xF0, 0x41, 0x10, 0x16, 0x11] + address_and_size + [GenericRoland.roland_checksum(address_and_size), 0xF7]

    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        return [testing.ProgramTestData(message=message, name="D110 Init ", number=0, rename_name="Warm Pad  "),
                testing.ProgramTestData(message=fifth, name="D110 Init ", number=4)]

    def edit_buffers(data: testing.TestData) -> List[testing.ProgramTestData]:
        return [testing.ProgramTestData(message=temporary, name="D110 Init ", rename_name="Warm Pad  ")]

    def internal_tones(data: testing.TestData, adaptation):
        # A D-110 that answers each request with the Tone Memory asked for, and refuses any other message
        return ScriptedMockDevice({tuple(tone_memory_request(i)): [adaptation.convertToProgramDump(0, message, i)]
                                   for i in range(64)})

    return testing.TestData(program_generator=programs,
                            edit_buffer_generator=edit_buffers,
                            program_dump_request=(0, 4, "F0 41 10 16 11 08 08 00 00 01 76 79 F7"),
                            device_detect_call="F0 41 10 16 11 10 00 00 00 00 01 6F F7",
                            device_detect_reply=("F0 41 10 16 12 10 00 00 4A 26 F7", 0),
                            mock_device_factory=internal_tones,
                            expected_wire_patch_count=64,
                            expected_sent_messages=lambda data, adaptation: [tone_memory_request(i) for i in range(64)],
                            single_edit_buffer_mock_device_factory=lambda data, adaptation: ScriptedMockDevice(
                                {tuple(edit_buffer_request): [temporary]}),
                            expected_send_to_synth_messages=lambda data, adaptation: [temporary])
