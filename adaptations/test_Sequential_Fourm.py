from pathlib import Path

import knobkraft
import sequential

import Sequential_Fourm as adaptation

SAMPLE_FILE = Path(__file__).resolve().parent / "testData" / "Sequential_Fourm" / "a_new_legend.syx"


def test_fixture_round_trips_through_dsi_packing():
    message = knobkraft.load_sysex(str(SAMPLE_FILE), as_single_list=True)

    assert adaptation.isSingleProgramDump(message)
    assert adaptation.nameFromDump(message) == "A New Legend"

    unpacked = sequential.GenericSequential.unescapeSysex(message[6:-1])
    repacked = sequential.GenericSequential.escapeSysex(unpacked)

    assert len(unpacked) == 4102
    assert len(repacked) == 4688
    assert message[:6] + repacked + [0xF7] == message
