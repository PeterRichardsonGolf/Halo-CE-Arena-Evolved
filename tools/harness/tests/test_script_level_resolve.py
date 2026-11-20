"""Scripted campaign destinations stay within their map family and MCC package."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, read, run


# (Arena Evolved has no Custom Edition campaign maps: these two cases expect upstream's CE destinations)
_NO_CE = pytest.mark.skip(reason="Arena Evolved: no Custom Edition campaign maps")


@pytest.mark.parametrize("case", ["mcc", "mcc-path", "mcc-foreign-path", "mcc-missing", "mcc-multiplayer",
                                  pytest.param("ce", marks=_NO_CE), "ce-missing", "xbox", "empty", "long",
                                  "small-buffer", pytest.param("transitions", marks=_NO_CE)])
def test_destination(case):
    code = function(read("port/linux/game/mcc_main.c"), "mcc_main_script_level_resolve")
    code += "\n" + function(read("source/main/main.c"), "main_script_level_resolve")
    status, output = run(build("script_level_resolve", (("under_test.inc", code),)), case)
    assert status == 0, output
