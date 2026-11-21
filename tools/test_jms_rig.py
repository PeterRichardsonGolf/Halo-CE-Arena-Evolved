"""tools/weapon_pack/jms_rig.py: a JMS model moved by a uniform scale about a point and the named nodes given new
positions, vertices and markers moved alike, normals, bindings and triangles untouched."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent / "weapon_pack"))

import jms_rig  # noqa: E402

JMS = "\n".join([
    "8200", "1074697263",
    "2", "handle", "1", "-1", "0\t0\t0\t1", "0\t0\t0", "blades", "-1", "-1", "0\t0\t0\t1", "0\t0\t0.8",
    "1", "mat", "<none>",
    "1", "flare", "0", "1", "0\t0\t0\t1", "30\t0\t0.05", "0",
    "1", "__unnamed",
    "2", "0", "10\t0\t0.8", "0\t0\t1", "-1", "0", "0\t0",
    "1", "10\t0\t-0.8", "0\t0\t-1", "-1", "0", "1\t1",
    "1", "0", "0", "0", "1", "0", ""])


def test_points_move_and_nodes_are_set():
    out, moved = jms_rig.rig(JMS, 2.0, (0.0, 0.0, 0.8), (1.0, 0.0, 0.0), {"blades": (8.0, 0.0, 0.0)})
    lines = out.split("\n")
    assert moved == 3
    assert lines[12] == "8\t0\t0"                       # the node's position, as given
    assert lines[7] == "0\t0\t0"                        # the other node untouched
    assert "61\t0\t-1.5" in out                          # the marker: 2 * (30, 0, 0.05 - 0.8) + (1, 0, 0)
    assert "21\t0\t0" in out and "21\t0\t-3.2" in out   # the vertices: 2 * (10 - 0) + 1 = 21, z 2 * (0.8 - 0.8), 2 * (-0.8 - 0.8)
    assert "0\t0\t1" in out and "0\t0\t-1" in out       # normals unchanged
    assert out.count("\n") == JMS.count("\n")


def test_a_node_that_is_not_there_is_refused():
    with pytest.raises(jms_rig.Refused):
        jms_rig.rig(JMS, 1.0, (0, 0, 0), (0, 0, 0), {"no such": (0.0, 0.0, 0.0)})


def test_a_short_file_is_refused():
    with pytest.raises(jms_rig.Refused):
        jms_rig.rig("8200\n1\n3\nonly", 1.0, (0, 0, 0), (0, 0, 0), {})
