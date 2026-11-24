"""Every top-level tools/test_*.py is run by the CI workflow or listed, with a reason, in tools/ci_tests_not_run.txt;
every C unit in port/linux/tests is in test_ae_units.py's UNITS table (or run by a tools/test_*.py wrapper that names it).
A new test that nothing runs fails here instead of rotting."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"
NOT_RUN = TOOLS / "ci_tests_not_run.txt"
UNITS_TEST = TOOLS / "test_ae_units.py"


def top_level_tests():
    return sorted(p.name for p in TOOLS.glob("test_*.py"))


def workflow_tests():
    """Test files the workflow names, as a path (tools/test_x.py) or a module (tools.test_x)."""
    text = WORKFLOW.read_text()
    named = set(re.findall(r"tools/(test_\w+\.py)", text))
    named |= {m + ".py" for m in re.findall(r"tools\.(test_\w+)", text)}
    return named


def not_run_entries():
    """The list file: one 'name reason' per line, '#' lines and blanks skipped."""
    entries = {}
    for line in NOT_RUN.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, reason = line.partition(" ")
        entries[name] = reason.strip()
    return entries


def test_every_python_test_has_a_runner():
    listed = not_run_entries()
    named = workflow_tests()
    loose = [name for name in top_level_tests() if name not in named and name not in listed]
    assert not loose, ("not run by .github/workflows/build.yml and not in tools/ci_tests_not_run.txt "
                       "(name it in the workflow's test line, or list it with a reason): " + ", ".join(loose))


def test_not_run_list_is_current():
    listed = not_run_entries()
    named = workflow_tests()
    existing = set(top_level_tests())
    gone = [name for name in listed if name not in existing]
    assert not gone, "listed in tools/ci_tests_not_run.txt but not in tools/: " + ", ".join(gone)
    now_run = [name for name in listed if name in named]
    assert not now_run, "listed as not run, but the workflow names them (drop the list line): " + ", ".join(now_run)
    reasonless = [name for name, reason in listed.items() if not reason]
    assert not reasonless, "listed without a reason: " + ", ".join(reasonless)


def test_every_c_unit_is_in_the_units_table():
    table = set(re.findall(r'^    "(\w+_test\.c)":', UNITS_TEST.read_text(), re.M))
    wrappers = "\n".join(p.read_text(errors="replace") for p in TOOLS.glob("test_*.py") if p != UNITS_TEST)
    loose = [p.name for p in sorted((ROOT / "port" / "linux" / "tests").glob("*_test.c"))
             if p.name not in table and p.name not in wrappers]
    assert not loose, "not in test_ae_units.py's UNITS and not run by a tools/test_*.py: " + ", ".join(loose)
