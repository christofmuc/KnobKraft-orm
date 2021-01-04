#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import re

from knobdiver.adaptation import Adaptation

splitter = re.compile("[ ,]")


def lazyHex(stringInput):
    if type(stringInput) is list:
        return stringInput
    elif type(stringInput) is str:
        items = splitter.split(stringInput)
        result = []
        for item in items:
            try:
                value = int(item, 16)
                result.append(value)
            except ValueError:
                if item is not None and item != "":
                    result.append(item)
        return result
    else:
        raise Exception("lazyHex takes a String as parameter that is converted into a list of bytes or pseudo-bytes")

