"""Yamaha SY77 voices. Initial adaptation and ROM 1.5 hardware testing by hmmbug."""

from yamaha.Yamaha77Voice import Yamaha77Voice, voice_test_data

Yamaha77Voice("SY77", "GrandPiano").install(globals())


def make_test_data():
    return voice_test_data("testData/Yamaha_SY77/bank.syx", 64, [
        (0, "GrandPiano", "IN-A01"),
        (6, "Triton", "IN-A07"),
        (7, "FrenchHorn", "IN-A08"),
        (16, "Dyna Grand", "IN-B01"),
    ])
