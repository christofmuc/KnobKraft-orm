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
