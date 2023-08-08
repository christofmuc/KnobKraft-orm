from Roland_JV1080 import *
import knobkraft


def test_fingerprint():
    a1 = knobkraft.load_sysex("testData/RolandJV1080/Super JV Pad.syx", as_single_list=True)
    a2 = knobkraft.load_sysex("testData/RolandJV1080/Super JV Pad2.syx", as_single_list=True)
    assert jv_1080.isSingleProgramDump(a1)
    assert jv_1080.isSingleProgramDump(a2)
    fingerprint1 = jv_1080.calculateFingerprint(a1)
    fingerprint2 = jv_1080.calculateFingerprint(a2)
    assert fingerprint1 == fingerprint2
