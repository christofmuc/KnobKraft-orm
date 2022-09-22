#
#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import importlib.util
import os
import sys


def load_adaptation(adaptation_file):
    # Dynamically load the adaptation and create the generic test suite all adaptations must undergo
    spec = importlib.util.spec_from_file_location(adaptation_file, adaptation_file)
    synth_under_test = importlib.util.module_from_spec(spec)
    sys.modules[adaptation_file] = synth_under_test
    spec.loader.exec_module(synth_under_test)
    return synth_under_test


def pytest_addoption(parser):
    parser.addoption("--all", action="store_true", help="run all combinations")
    parser.addoption("--adaptation", help="specify adaptation to test")


def pytest_generate_tests(metafunc):
    if "adaptation" in metafunc.fixturenames:
        if metafunc.config.getoption("all"):
            adaptations_to_test = []
            for file in os.listdir(os.path.dirname(os.path.realpath(__file__))):
                if os.path.isfile(file) and file != "conftest.py" and file != "test_adaptations.py" and file.lower().endswith(".py") and not file.lower().startswith("test_"):
                    adaptations_to_test += [file]
            metafunc.parametrize("adaptation", [load_adaptation(a) for a in adaptations_to_test])
        else:
            metafunc.parametrize("adaptation", [load_adaptation(metafunc.config.getoption("adaptation"))])

