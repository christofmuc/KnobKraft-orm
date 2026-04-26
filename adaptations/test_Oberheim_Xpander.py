import Oberheim_Xpander as adaptation
def test_sample_program_shape_and_name_round_trip():
    message = adaptation.SAMPLE_PROGRAM
    assert adaptation.isSingleProgramDump(message)
    assert len(message) == adaptation.PROGRAM_LENGTH
    assert len(message[adaptation.RAW_DATA_START:adaptation.RAW_DATA_END]) == 392
    assert len(adaptation._drop_odd_bytes(message[adaptation.RAW_DATA_START:adaptation.RAW_DATA_END])) == 196
    assert adaptation.nameFromDump(message) == "WEIRDLFO"
def test_rename_and_fingerprint_ignore_name_and_program_slot():
    original = adaptation.SAMPLE_PROGRAM
    renamed = adaptation.renamePatch(original, "weirdlfp")
    moved = adaptation.convertToProgramDump(0, renamed, 42)
    assert adaptation.nameFromDump(renamed) == "WEIRDLFP"
    assert adaptation.numberFromDump(moved) == 42
    assert adaptation.calculateFingerprint(original) == adaptation.calculateFingerprint(renamed)
    assert adaptation.calculateFingerprint(original) == adaptation.calculateFingerprint(moved)
