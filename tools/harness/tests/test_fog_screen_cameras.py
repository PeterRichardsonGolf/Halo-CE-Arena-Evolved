"""The real previous camera bookkeeping of _rasterizer_environment_fog_screen_begin (rasterizer_xbox_environment_fog.c):
each split-screen window keeps its own slot of previous_camera_matrix, and no window's fog screen frame writes past the
array. Upstream's "&previous_camera_matrix[i]" inside the scope of the local pointer of the same name indexed the
pointer: the first fog screen frame drawn in window 2 overwrote the next global, the model shader's ambient reflection
tint pointer, and the next model draw crashed (split screen, a player falling into Waterworks' pit, 0.3.9)."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, desktop_source, function, mutated, read, run, structure  # noqa: E402

CASES = ["first-frame-in-window-2", "each-window-its-own"]

FIRST_FIX = "csmemcpy(previous_camera_matrix_for_window(window_index), "
STORE_FIX = "csmemcpy(previous_camera_matrix, &global_window_parameters.frustum.world_to_view, "

# upstream's two writes (the local pointer indexed again), and the case that must catch each
NEGATIVE_CONTROLS = {
    "first-frame-indexes-the-pointer": ((FIRST_FIX, "csmemcpy(&previous_camera_matrix[window_index], "),
                                        "first-frame-in-window-2"),
    "store-indexes-the-pointer": ((STORE_FIX, "csmemcpy(&previous_camera_matrix[global_window_parameters."
                                              "window_index], &global_window_parameters.frustum.world_to_view, "),
                                  "each-window-its-own"),
}


def bookkeeping(source):
    """The local pointer's declaration and the block after the declarations that keeps the previous cameras, from
    the real function, as a function of the frame's fog screen and window."""
    begin = function(source, "_rasterizer_environment_fog_screen_begin")
    at = begin.index("real_matrix4x3 *previous_camera_matrix =")
    declaration = begin[at:begin.index(";", at) + 1]
    first = begin.index("if (local_fog_screen_first_time)")
    start = begin.rindex("{", 0, first)
    end, depth = start + 1, 1
    while depth:
        depth += (begin[end] == "{") - (begin[end] == "}")
        end += 1
    block = begin[start:end]
    assert "global_window_parameters.frustum.world_to_view" in block, "the block no longer stores the camera"
    return ("static void fog_screen_cameras(struct fog_screen const *screen,\n"
            "\tstruct rasterizer_environment_fog_screen_window *window)\n{\n"
            "\t" + declaration + "\n"
            "\treal_matrix4x3 wind_matrix = *global_identity4x3;\n"
            "\treal_matrix4x3 matrix;\n\treal_vector3d vector;\n\tshort layer;\n"
            "\t" + block + "\n}\n")


def generated(fault=None):
    # (the hooks' #ifndef HALO_XBOX_CONSOLE carries the AE hook mark, a comment desktop_source would read as a name)
    fog = desktop_source(read("source/rasterizer/xbox/rasterizer_xbox_environment_fog.c").replace("/* AE hook */", ""))
    code = function(fog, "previous_camera_matrix_for_window") + "\n" + bookkeeping(fog)
    if fault:
        code = mutated(code, *fault)
    types = structure(read("source/math/real_math.h"), "real_matrix4x3") + "\n" \
        "typedef struct real_matrix4x3 real_matrix4x3;\n"
    return (("under_test_types.inc", types), ("under_test.inc", code))


def test_fixes_in_the_sources():
    """the two writes are the desktop build's (the controls below put upstream's back)"""
    code = generated()[1][1]
    assert FIRST_FIX in code and STORE_FIX in code


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("fog_screen_cameras", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("fog_screen_cameras", generated(fault)), case)
    assert status == CHECK_FAILED, f"the bookkeeping with a fault ({control}) passed '{case}': the test cannot see it"
