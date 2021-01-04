#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#


class Adaptation(object):

    def __init__(self, adaptation_data):
        self.adaptation = adaptation_data

    def parseMessage(self, message, reply_expected, bank_driver=None):
        reply = self.ignoreDeviceID(message)
        prg_result = -1
        result = []
        nested = False
        reply_ptr = 0
        expected_ptr = 0
        loop_start = -1
        while reply_ptr < len(reply):
            if type(reply_expected[expected_ptr]) is str:
                if reply_expected[expected_ptr] == "[":
                    nested = True
                    if type(prg_result) is not list:
                        prg_result = []
                    loop_start = expected_ptr
                    expected_ptr = expected_ptr + 1
                elif reply_expected[expected_ptr] == "]":
                    # Check if we have enough
                    if len(result) < bank_driver["# of Entries"]:
                        expected_ptr = loop_start
                    else:
                        expected_ptr = expected_ptr + 1
                elif reply_expected[expected_ptr] == "SUM":
                    summation_start = reply_ptr
                    expected_ptr = expected_ptr + 1
                elif reply_expected[expected_ptr] == "EN#":
                    if nested:
                        prg_result.append(reply[reply_ptr])
                    else:
                        prg_result = reply[reply_ptr]
                    expected_ptr = expected_ptr + 1
                    reply_ptr = reply_ptr + 1
                elif reply_expected[expected_ptr] == "SIN":
                    data = []
                    while len(data) < self.adaptation["Data Types"]["Single"]["Size"]:
                        data.append(reply[reply_ptr])
                        reply_ptr = reply_ptr + 1
                    expected_ptr = expected_ptr + 1
                    if nested:
                        result.append(data)
                    else:
                        result = data
                elif reply_expected[expected_ptr] == "CHK":
                    # Ignore checksum for now
                    expected_ptr = expected_ptr + 1
                    reply_ptr = reply_ptr + 1
                else:
                    raise Exception("Unknown pseudo byte" + reply_expected[expected_ptr])
            else:
                # Just check that all bytes are as expected
                if reply[reply_ptr] != reply_expected[expected_ptr]:
                    return False, -1, []
                reply_ptr = reply_ptr + 1
                expected_ptr = expected_ptr + 1
        return True, prg_result, result

    @staticmethod
    def createMessage(expected_message, program_no, data_block):
        data = []
        ptr = 0
        while ptr < len(expected_message):
            if type(expected_message[ptr]) is str:
                if expected_message[ptr] == "SUM":
                    summation_start = ptr
                    ptr = ptr + 1
                elif expected_message[ptr] == "EN#":
                    data.append(program_no)
                    ptr = ptr + 1
                elif expected_message[ptr] == "SIN":
                    data = data + data_block
                    ptr = ptr + 1
                elif expected_message[ptr] == "CHK":
                    # Ignore checksum for now
                    data.append(0)
                    ptr = ptr + 1
                else:
                    raise Exception("Unknown pseudo byte" + expected_message[ptr])
            else:
                # Just copy byte verbatim
                data.append(expected_message[ptr])
                ptr = ptr + 1
        return data

    def bankNoForProgramNo(self, program_number):
        bank = 0
        count = 0
        while self.adaptation["Bank Drivers"][bank]["# of Entries"] + count < program_number + 1:
            count = count + self.adaptation["Bank Drivers"][bank]["# of Entries"]
            bank = bank + 1
        return bank

    def insertDeviceID(self, channel, message):
        # TODO not sure what to do with the device ID min and max values. Is this for display?
        device_id = (channel + self.adaptation["Device ID"][1]) % self.adaptation["Device ID"][2]
        return message[0:self.adaptation["Device ID"][0]] + [channel] + message[self.adaptation["Device ID"][0] + 1:]

    def ignoreDeviceID(self, message):
        return message[0:self.adaptation["Device ID"][0]] + [0] + message[self.adaptation["Device ID"][0] + 1:]

    @staticmethod
    def kawaiK1K4Checksum(data):
        return (0xA4 + sum(data)) & 0x7f

