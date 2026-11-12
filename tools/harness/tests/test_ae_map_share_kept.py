"""Arena Evolved: map sharing's catalogue of hashed files keeps the place of a file in use (map_share_files.c,
map_share_file_hash, the AE hook that asks ae_map_share_file_kept): the game's own map and a map being sent to a
machine are not replaced by the files another machine asks for, however many (item 90's security review, M2).
The real function is compiled; the negative controls take the rule out and must fail."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, function, mutated, read, run  # noqa: E402

CASES = ["kept-files-keep-their-places", "let-go-is-replaced", "hashing-file-passed-over"]

# a fault in map_share_file_hash and the case that must catch it
NEGATIVE_CONTROLS = {
    "nothing-kept": (("ae_map_share_file_kept(globals->files[index].path)", "0"), "kept-files-keep-their-places"),
    "first-place-kept-for-ever": (("ae_map_share_file_kept(globals->files[index].path)", "(index == 0)"),
                                  "let-go-is-replaced"),
    "hashing-file-replaced": ((" || (globals->job_type == _hash_job_file && globals->job_owner == index)); ae_turn++)",
                              "); ae_turn++)"), "hashing-file-passed-over"),
}


def generated(fault=None):
    source = read("port/linux/game/map_share_files.c")
    config = f"#define MAXIMUM_SHARE_FILES {constant(source, 'MAXIMUM_SHARE_FILES')}\n"
    code = function(source, "map_share_file_hash")
    if fault:
        code = mutated(code, *fault)
    return (("config.inc", config), ("under_test.inc", code + "\n"))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("ae_map_share_kept", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("ae_map_share_kept", generated(fault)), case)
    assert status == CHECK_FAILED, f"map_share_file_hash with a fault ({control}) passed '{case}': the test cannot see it"
