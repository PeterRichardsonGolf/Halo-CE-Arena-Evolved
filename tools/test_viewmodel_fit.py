"""tools/viewmodel_fit.py: the place numbers found again from probes made with known ones, the log reading and the refusals."""
import math
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viewmodel_fit as fit  # noqa: E402

MARKERS = {
    "primary trigger": (0.62, -0.05, -0.08),
    "primary ejection port": (0.38, -0.07, -0.05),
    "magazine": (0.33, -0.08, -0.14),
    "left hand": (0.45, -0.02, -0.12),
}
PIVOT = (0.30, -0.08, -0.10)


def placed(point, numbers, pivot=PIVOT):
    """a point moved as the engine places it: turned and scaled about the pivot, then moved (camera frame)"""
    right, up, forward, yaw, pitch, roll, scale = numbers
    turn = fit.local_turn(yaw, pitch, roll)
    rel = [point[i] - pivot[i] for i in range(3)]
    turned = fit.mat_vec(turn, rel)
    move = [forward * fit.UNIT, -right * fit.UNIT, up * fit.UNIT]
    return tuple(pivot[i] + scale * turned[i] + move[i] for i in range(3))


def probe_text(key, pivot, markers):
    lines = ["arena-evolved: viewmodel probe: %s pivot %.5f %.5f %.5f" % ((key,) + tuple(pivot))]
    for name, point in markers.items():
        lines.append('arena-evolved: viewmodel probe: %s marker "%s" %.5f %.5f %.5f' % ((key, name) + tuple(point)))
    return "\n".join(lines)


@pytest.mark.parametrize("numbers", [
    [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0],
    [3.0, -1.5, 4.0, 0.0, 0.0, 0.0, 1.0],
    [1.0, 2.0, -3.0, 5.0, -4.0, 3.0, 0.85],
    [-2.5, 0.5, 6.0, -12.0, 8.0, -20.0, 1.2],
])
def test_fit_finds_the_numbers_again(numbers):
    target = {name: placed(point, numbers) for name, point in MARKERS.items()}
    log = probe_text("assault rifle", (0.0, 0.0, 0.0), target) + "\n" + probe_text("ar_modern", PIVOT, MARKERS)
    found, names, miss = fit.fit(fit.read_probes(log), "assault rifle", "ar_modern")
    assert len(names) == 4 and miss < 1e-4
    for want, got in zip(numbers, found):
        assert got == pytest.approx(want, abs=2e-3)


def test_turn_angles_round_trip():
    for yaw, pitch, roll in [(0, 0, 0), (10, -5, 7), (-30, 20, -40), (44, 44, 44)]:
        assert fit.turn_angles(fit.local_turn(yaw, pitch, roll)) == pytest.approx((yaw, pitch, roll), abs=1e-6)


def test_no_scale_keeps_one():
    numbers = [1.0, 0.0, 2.0, 3.0, 0.0, 0.0, 1.0]
    target = {name: placed(point, numbers) for name, point in MARKERS.items()}
    log = probe_text("a", (0, 0, 0), target) + "\n" + probe_text("b", PIVOT, MARKERS)
    found, _, _ = fit.fit(fit.read_probes(log), "a", "b", allow_scale=False)
    assert found[6] == 1.0


def test_last_probe_of_a_key_wins_and_names_must_match():
    first = probe_text("b", PIVOT, {"x": (1, 0, 0)})
    second = probe_text("b", PIVOT, MARKERS)
    probes = fit.read_probes(first + "\n" + second)
    assert set(probes["b"]["markers"]) == set(MARKERS)
    log = probe_text("a", (0, 0, 0), {"other": (1, 1, 1), "magazine": (0, 0, 0)}) + "\n" + second
    with pytest.raises(fit.Refused, match="markers both weapons have"):
        fit.fit(fit.read_probes(log), "a", "b")


def test_missing_probe_and_too_big_a_turn_refused():
    log = probe_text("b", PIVOT, MARKERS)
    with pytest.raises(fit.Refused, match="no probe of a"):
        fit.fit(fit.read_probes(log), "a", "b")
    numbers = [0.0, 0.0, 0.0, 80.0, 0.0, 0.0, 1.0]
    target = {name: placed(point, numbers) for name, point in MARKERS.items()}
    log = probe_text("a", (0, 0, 0), target) + "\n" + probe_text("b", PIVOT, MARKERS)
    with pytest.raises(fit.Refused, match="turn needed"):
        fit.fit(fit.read_probes(log), "a", "b")


def test_cli(tmp_path, capsys):
    numbers = [1.0, -0.5, 2.0, 4.0, -3.0, 2.0, 0.9]
    target = {name: placed(point, numbers) for name, point in MARKERS.items()}
    path = tmp_path / "debug.txt"
    path.write_text(probe_text("assault_rifle", (0, 0, 0), target) + "\n" + probe_text("ar_modern", PIVOT, MARKERS))
    assert fit.main([str(path), "--target", "assault_rifle", "--source", "ar_modern"]) == 0
    out = capsys.readouterr().out
    line = [l for l in out.splitlines() if l.startswith("viewmodel place ")][0]
    got = [float(x) for x in line.split()[2:]]
    assert got == pytest.approx(numbers, abs=5e-3)
    assert not math.isnan(sum(got))
