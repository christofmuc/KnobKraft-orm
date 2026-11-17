"""Yamaha TG77 voices. Initial adaptation by hmmbug and Giovanni Candiago.
TG77 hardware testing and all-voice-mode fixture by Giovanni Candiago.
"""

from yamaha.Yamaha77Voice import Yamaha77Voice, voice_test_data

Yamaha77Voice("TG77", "SP|Cosmo").install(globals())


def generalMessageDelay():
    # Giovanni measured up to 800 ms for the 1564-byte 4AFM mono dump.
    return 800


def make_test_data():
    return voice_test_data("testData/Yamaha_SY77-TG77/TG77_test_voices.syx", 75, [
        (0, "PD:PUFF AT", "IN-A01"),
        (2, "PD:WARM", "IN-A03"),
        (4, "PD:CHRSTRG", "IN-A05"),
        (6, "WN:PICCOLO", "IN-A07"),
        (12, "PD:GB AGE", "IN-A13"),
        (19, "PD:PUFF AT", "IN-B04"),
        (48, "PD:OPET", "IN-D01"),
    ], mode_samples=[
        (64, "1AFM mono"), (65, "2AFM mono"), (66, "4AFM mono"),
        (67, "1AFM poly"), (68, "2AFM poly"), (69, "1AWM poly"),
        (70, "2AWM poly"), (71, "4AWM poly"), (72, "1AFM&1AWM"),
        (73, "2AFM&2AWM"), (74, "Drum Set"),
    ])
