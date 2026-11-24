"""perf_scorecard.py's reading and judging, on made-up benchmark lines (no game is run)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))

import perf_scorecard as ps  # noqa: E402


def line(name, avg, low, p99, median=4.10):
    return (f"arena-evolved: benchmark: {name}: {avg} frames a second on average, {low} the 1% low, {median} ms the "
            f"median frame, {p99} the 99th percentile, 9.80 the longest, 0 over 33 ms (3600 frames in 15.0 s)\n")


def scores(maps, peak_rss=600.0, peak_gpu=300.0):
    return {"maps": maps, "memory": {"peak_rss_mb": peak_rss, "peak_gpu_mb": peak_gpu}}


def test_parse_reads_each_map_and_ignores_other_lines():
    text = ("arena-evolved: benchmark: bloodgulch: measuring 15 s\n" + line("bloodgulch", 240.5, 150.2, 7.10) +
            line("chillout", 120.0, 80.0, 12.50))
    stages = ps.parse_benchmark(text)
    assert set(stages) == {"bloodgulch", "chillout"}
    assert stages["bloodgulch"]["avg_fps"] == 240.5
    assert stages["chillout"]["p99_ms"] == 12.5
    assert stages["chillout"]["frames"] == 3600


def test_combine_takes_the_median_and_the_spread():
    runs = [ps.parse_benchmark(line("a", fps, 100, 8, median)) for fps, median in ((90.0, 4.5), (100.0, 5.0), (300.0, 5.5))]
    combined = ps.combine(runs)["a"]
    assert combined["avg_fps"] == 100.0
    # (the spread is the median frame time's, not the average frame rate's)
    assert abs(combined["spread"] - 0.2) < 1e-9
    assert combined["repeats"] == 3


def test_compare_passes_small_changes_and_fails_a_slower_median():
    base = scores({"a": dict(ps.parse_benchmark(line("a", 100, 60, 10))["a"], spread=0.0)})
    same = scores({"a": dict(ps.parse_benchmark(line("a", 70, 59, 10.3, 4.2))["a"], spread=0.0)})
    slower = scores({"a": dict(ps.parse_benchmark(line("a", 100, 60, 10, 4.6))["a"], spread=0.0)})
    # (a swing of the average frame rate alone is not judged)
    assert not [t for level, t in ps.compare(same, base) if level == "FAIL"]
    fails = [t for level, t in ps.compare(slower, base) if level == "FAIL"]
    assert len(fails) == 1 and "median_ms" in fails[0]


def test_compare_widens_the_tolerance_with_noise():
    base = scores({"a": dict(ps.parse_benchmark(line("a", 100, 60, 10))["a"], spread=0.06)})
    noisy = scores({"a": dict(ps.parse_benchmark(line("a", 90, 60, 10, 4.5))["a"], spread=0.06)})
    assert not [t for level, t in ps.compare(noisy, base) if level == "FAIL"]


def test_compare_fails_a_rising_frame_time_memory_and_a_missing_map():
    base = scores({"a": dict(ps.parse_benchmark(line("a", 100, 60, 10))["a"], spread=0.0),
                   "b": dict(ps.parse_benchmark(line("b", 100, 60, 10))["b"], spread=0.0)})
    new = scores({"a": dict(ps.parse_benchmark(line("a", 100, 60, 12))["a"], spread=0.0)}, peak_rss=700.0)
    fails = [t for level, t in ps.compare(new, base) if level == "FAIL"]
    assert any("p99_ms" in t for t in fails)
    assert any("peak_rss_mb" in t for t in fails)
    assert any(t.startswith("b: not measured") for t in fails)


def test_growth_is_the_second_half_slope():
    flat_then_rising = [(t * 60.0, 500.0) for t in range(5)] + [(t * 60.0, 500.0 + 3.0 * (t - 5)) for t in range(5, 10)]
    assert abs(ps.growth_mb_minute(flat_then_rising) - 3.0) < 1e-6
    assert ps.growth_mb_minute([(0, 1), (1, 1)]) is None
    soak = {"maps": {}, "memory": {}, "soak": {"growth_mb_minute": 3.0}}
    assert [t for level, t in ps.compare(soak, scores({})) if level == "FAIL" and "soak" in t]
