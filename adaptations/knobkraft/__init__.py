#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys as _sys

from .sysex import *
from .test_helper import *


_HOST_API_VERSION_ATTRIBUTE = "_knobkraft_adaptation_api_version"


def host_api_version() -> int:
    """Return the API level advertised by the embedding KnobKraft host."""
    return int(getattr(_sys, _HOST_API_VERSION_ATTRIBUTE, 0))


def require_host_api_version(minimum_version: int, adaptation_name: str) -> None:
    """Refuse to load an adaptation when its host bridge is too old."""
    actual_version = host_api_version()
    if actual_version < minimum_version:
        raise RuntimeError(
            f"{adaptation_name} requires KnobKraft adaptation API level {minimum_version}, "
            f"but this host provides level {actual_version}. Please update KnobKraft Orm."
        )
