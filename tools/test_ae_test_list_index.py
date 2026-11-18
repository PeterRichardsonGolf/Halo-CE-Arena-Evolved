"""tools/ae_test/list_index.py: INDEX.md is current, a list's header and tests are read, --sync copies new lists and
leaves a differing one unless forced."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))

import list_index  # noqa: E402


def test_index_is_current():
    assert list_index.main(["--check"]) == 0


def test_every_list_is_indexed():
    text = list_index.INDEX.read_text(encoding="utf-8")
    for path in list_index.LISTS.glob("*.txt"):
        assert f"File `{path.name}`" in text


def test_read_list():
    info = list_index.read_list(list_index.LISTS / "vehicle-balance-1.txt")
    assert info["id"] == "vehicle-balance-1"
    assert info["title"] == "VEHICLE BALANCE"
    assert info["tests"][0]["id"] == "bullets-hurt"
    assert info["header"][0].startswith("Vehicle balance")


def test_find():
    assert any(info["id"] == "vehicle-balance-1" for info in list_index.find(["vehicle", "rockets"]))


def test_sync(tmp_path):
    copied, same, differing = list_index.sync(tmp_path)
    assert copied and not same and not differing
    (tmp_path / "sample.txt").write_text("changed by the owner\n")
    (tmp_path / "sample.results.jsonl").write_text("{}\n")
    copied, same, differing = list_index.sync(tmp_path)
    assert not copied and differing == ["sample.txt"]
    assert (tmp_path / "sample.txt").read_text() == "changed by the owner\n"
    copied, _, differing = list_index.sync(tmp_path, force=True)
    assert copied == ["sample.txt"] and not differing
    assert (tmp_path / "sample.results.jsonl").read_text() == "{}\n"
