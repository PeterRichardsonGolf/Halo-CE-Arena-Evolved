"""jms_cut.py: the triangles of one node inside a box are dropped, the rest of a JMS file kept as it was."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent / "weapon_pack"))

import jms_cut  # noqa: E402

BOX = [0.5, 3.9, -0.25, 0.25, -1.15, 2.6]
VERTICES = [(0, 1.0, 0.0, 0.0), (0, 2.0, 0.1, 0.5), (0, 3.0, -0.1, 1.0),     # inside the box, on node 0
            (0, 5.0, 0.0, 0.0),                                               # on node 0, outside the box
            (1, 1.0, 0.0, 0.0)]                                               # inside the box, on node 1


def jms(triangles):
    """a JMS (8200) with two nodes, one material, no markers or regions, and VERTICES; triangles: index triples"""
    lines = ["8200", "3251", "2", "root", "1", "-1", "0.0 0.0 0.0 1.0", "0.0 0.0 0.0", "other", "-1", "-1",
             "0.0 0.0 0.0 1.0", "0.0 0.0 0.0", "1", "mat", "<none>", "0", "0", str(len(VERTICES))]
    for node, x, y, z in VERTICES:
        lines += [str(node), "%f %f %f" % (x, y, z), "0.0 0.0 1.0", "-1", "0.0", "0.5 0.5 0.0"]
    lines.append(str(len(triangles)))
    for a, b, c in triangles:
        lines += ["0", "0", "%d %d %d" % (a, b, c)]
    return "\n".join(lines) + "\n"


def test_a_triangle_wholly_inside_the_box_and_on_the_node_is_dropped():
    result, dropped, total = jms_cut.cut(jms([(0, 1, 2), (0, 1, 3), (2, 3, 4), (0, 1, 4)]), 0, BOX)
    assert (dropped, total) == (1, 4)
    # (the one on node 0 wholly inside is gone; one vertex outside, or on another node, keeps a triangle)
    assert result == jms([(0, 1, 3), (2, 3, 4), (0, 1, 4)])


def test_a_cut_that_hits_nothing_changes_nothing():
    text = jms([(0, 1, 3), (2, 3, 4)])
    assert jms_cut.cut(text, 0, BOX) == (text, 0, 2)
    assert jms_cut.cut(jms([(0, 1, 2)]), 1, BOX)[1] == 0


def test_a_file_that_ends_early_is_refused():
    with pytest.raises(jms_cut.Refused):
        jms_cut.cut("8200\n3251\n2\nroot\n", 0, BOX)
