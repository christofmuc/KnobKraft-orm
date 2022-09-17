#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

from typing import List


class DataBlock:
    def __init__(self, address: tuple, size, block_name: str):
        self.address = address
        self.block_name = block_name
        self.size = DataBlock.size_to_number(size)

    @staticmethod
    def size_as_7bit_list(size, number_of_values):
        return [(size >> ((number_of_values - 1 - i) * 7)) & 0x7f for i in range(number_of_values)]

    @staticmethod
    def size_to_number(size):
        if isinstance(size, tuple):
            num_values = len(size)
            result = 0
            for i in range(num_values):
                result += size[i] << (7 * (num_values - 1 - i))
            return result
        else:
            return size


class RolandData:
    def __init__(self, data_name: str, num_items: int, num_address_bytes: int, num_size_bytes: int, blocks: List[DataBlock]):
        self.data_name = data_name
        self.num_items = num_items  # This is the "bank size" of that data type
        self.num_address_bytes = num_address_bytes
        self.num_size_bytes = num_size_bytes
        self.data_blocks = blocks
        self.size = self.total_size()

    def total_size(self):
        return sum([f.size for f in self.data_blocks])

    def total_size_as_list(self):
        return DataBlock.size_as_7bit_list(self.size * 8, self.num_size_bytes)  # Why times 8?. You can't cross border from one data set into the next

    def address_and_size_for_sub_request(self, sub_request, sub_address):
        # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
        concrete_address = [self.data_blocks[sub_request].address[i] if i != 1 else sub_address for i in range(len(self.data_blocks[sub_request].address))]
        return concrete_address, self.total_size_as_list()

    def address_and_size_for_all_request(self, sub_address):
        # The idea is that if we request the first block, but with the total size of all blocks, the device will send us all messages back.
        # Somehow that does work, but not as expected. To get all messages from a single patch on an XV-3080, I need to multiply the size by 8???

        # Patch in the sub_address (i.e. the item in the bank). Assume the sub-item is always at position #1 in the tuple
        concrete_address = [self.data_blocks[0].address[i] if i != 1 else sub_address for i in range(len(self.data_blocks[0].address))]
        return concrete_address, self.total_size_as_list()
