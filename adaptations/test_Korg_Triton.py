import knobkraft

import Korg_Triton as adaptation


def test_extract_programs_from_full_dump():
    full_dump = knobkraft.load_sysex("testData/Korg_Triton/full-korgtriton-midiox.syx")
    bank_dump = knobkraft.load_sysex("testData/Korg_Triton/bank1-korgtriton-midiox2.syx")

    full_dump_programs = adaptation.extractPatchesFromAllBankMessages(full_dump)
    bank_programs = knobkraft.splitSysex(adaptation.extractPatchesFromBank(bank_dump[0]))
    rebuilt_bank_dump = adaptation.convertPatchesToBankDump(bank_programs)

    assert len(full_dump_programs) >= 256
    assert full_dump_programs[:len(bank_programs)] == bank_programs
    assert rebuilt_bank_dump == bank_dump
    assert adaptation.isSingleProgramDump(full_dump_programs[0])
    assert adaptation.nameFromDump(full_dump_programs[0]) == "Noisy Stabber"
    assert adaptation.nameFromDump(full_dump_programs[127]) == "Krystal Bells"
    assert adaptation.nameFromDump(full_dump_programs[128]) == "Synth Sweeper"
