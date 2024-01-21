import knobkraft
from YamahaRefaceDX import *


def test_legacy_convert():
    messages = knobkraft.sysex.load_sysex("testData/refaceDX-00-Piano_1___.syx")
    raw_data = list(itertools.chain.from_iterable(messages))
    legacy_format = convertToLegacyFormat(raw_data)
    assert nameFromDump(legacy_format) == nameFromDump(raw_data)
    back_to_normal = convertFromLegacyFormat(0, legacy_format)
    assert isEditBufferDump(back_to_normal)
    assert calculateFingerprint(back_to_normal) == calculateFingerprint(raw_data)
    assert back_to_normal == raw_data
    assert convertToEditBuffer(0, legacy_format) == raw_data


def test_program_dumps():
    editbuffer =knobkraft.sysex.load_sysex("testData/refaceDX-00-Piano_1___.syx", as_single_list=True)
    assert isEditBufferDump(editbuffer)
    program = convertToProgramDump(0, editbuffer, 1)
    assert isSingleProgramDump(program)
    legacy_format = convertToLegacyFormat(program)
    assert isEditBufferDump(legacy_format)
    edit_buffer_back = convertFromLegacyFormat(0, legacy_format)
    assert editbuffer == edit_buffer_back
