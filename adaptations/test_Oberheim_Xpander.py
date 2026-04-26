import Oberheim_Xpander as adaptation


def _bank_messages():
    return [adaptation.convertToProgramDump(0, adaptation.SAMPLE_PROGRAM, patch_no) for patch_no in range(adaptation.PATCH_COUNT)]


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


def test_request_opcodes_and_scratch_slot_follow_spec():
    assert adaptation.createProgramDumpRequest(0, 9) == [0xF0, 0x10, 0x02, 0x00, 0x00, 0x09, 0xF7]
    assert adaptation.createBankDumpRequest(0, 0) == [0xF0, 0x10, 0x02, 0x02, 0x00, 0xF7]
    assert adaptation.defaultProgramPlace() == 99


def test_bank_dump_extraction_waits_for_all_100_programs():
    partial = _bank_messages()[:10]
    complete = _bank_messages()

    assert not adaptation.isBankDumpFinished(partial)
    assert adaptation.isBankDumpFinished(complete)

    patches = adaptation.extractPatchesFromAllBankMessages(complete)
    assert len(patches) == adaptation.PATCH_COUNT
    assert adaptation.numberFromDump(patches[0]) == 0
    assert adaptation.numberFromDump(patches[-1]) == adaptation.PATCH_COUNT - 1
    assert all(adaptation.isSingleProgramDump(patch) for patch in patches)
