#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from .sysex import *
from .test_helper import *


def knobkraft_api(func):
    func._is_knobkraft = True
    return func

