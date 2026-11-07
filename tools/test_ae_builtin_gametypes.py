"""AE's gametypes built in (0.7.1, arena_gametypes.c, playlist_profile.c) in real games, headless, through the ae_test
harness: a new save has none on disk but the built-in folders, and a save of an earlier build has its files moved once:

- as written by that build: backed up and removed (the built-in has it);
- edited: kept under a free stored name, "<its display name> (MINE)";
- deleted: nothing to do, and no file comes back;
- a player's own: left alone;
- a second start does nothing.

Runs only when asked ($AE_MENUS_BUILD, the harness's config, and $AE_OLD_BUILD, a build of 0.7.0 to write the old save):

    AE_MENUS_BUILD=$PWD/build/linux64 AE_OLD_BUILD=<0.7.0>/build/linux64 python3 -m pytest -q tools/test_ae_builtin_gametypes.py
"""
import importlib.util
import os
import shutil
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_builtin_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)
sys.path.insert(0, str(HERE / "ae_test"))
import gametype_file  # noqa: E402

BUILT_IN = 26 + 3 + 12 + 82  # (82 modes since the AE BTB+ set, revision 28)
EDITED, DELETED, UNTOUCHED, OWN = "AE CTF", "AE KING", "AE SND", "AE TEAM SLY"


def names(root):
    return sorted(gametype_file.name_of(f.read_bytes()) for f in (Path(root) / "u" / "UDATA").glob("*/blam.lst")
                  if f.stat().st_size >= gametype_file.VARIANT_SIZE + gametype_file.SIGNATURE_SIZE)


def run(cfg, out, name, root=None, build=None):
    """one game: (its save root, kept under the work folder, and its debug.txt)"""
    spec = {"name": name, "build": build or menus.build(), "window": "1920x1080", "exit_after": 20, "screenshots": 0,
            "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "0"}}
    if root:
        spec["save_from"] = str(root)
    run_id = f"builtin-{os.getpid()}"
    menus.harness.play(cfg, spec, out / name, keep_work=True, run_id=run_id)
    debug = out / name / "debug.txt"
    save = menus.harness.expand(cfg["work_dir"]) / run_id / menus.harness.safe_name(name) / "save"
    return save, debug.read_text(errors="replace") if debug.exists() else ""


@pytest.fixture(scope="module")
def setup():
    cfg = menus.config()
    if not cfg or not os.environ.get("AE_OLD_BUILD"):
        pytest.skip("$AE_MENUS_BUILD, $AE_OLD_BUILD and the harness's config")
    out = menus.out_dir(cfg, "builtin-gametypes")
    return cfg, out


def test_a_new_save(setup):
    cfg, out = setup
    root, text = run(cfg, out, "new")
    assert "is built in now" not in text
    assert names(root) == []
    assert len(list((root / "z" / "saved" / "playlists" / "default_playlist").glob("*/blam.lst"))) == BUILT_IN


def test_the_move(setup):
    cfg, out = setup
    save, _ = run(cfg, out, "old", build=os.environ["AE_OLD_BUILD"])
    root = menus.harness.expand(cfg["work_dir"]) / "seeds" / "builtin-old"
    shutil.rmtree(root, ignore_errors=True)
    shutil.copytree(save, root, symlinks=True)
    assert len(names(root)) == 115

    # the player's changes
    edited = gametype_file.find(root, EDITED)
    block = bytearray(edited.read_bytes())
    block[0x20] ^= 1
    edited.write_bytes(gametype_file.sign(block))
    shutil.rmtree(gametype_file.find(root, DELETED).parent)
    gametype_file.add_own_copy(root, UNTOUCHED, "MY OWN", {})
    before = names(root)
    assert "MY OWN" in before and DELETED not in before

    moved, text = run(cfg, out, "moved", root=root)
    assert sorted(names(moved)) == ["MY CTF", "MY OWN"]
    assert (moved / "z" / "saved" / "playlists" / "arena_gametypes.moved.txt").exists()
    assert not (moved / "z" / "saved" / "playlists" / "arena_gametypes.txt").exists()
    backups = list((moved / "z" / "saved" / "playlists_backup" / "revision_29").glob("*/blam.lst"))
    assert len(backups) == 113
    assert "'AE CTF' was edited, kept as 'MY CTF'" in text

    # a second start: nothing
    again = menus.harness.expand(cfg["work_dir"]) / "seeds" / "builtin-again"
    shutil.rmtree(again, ignore_errors=True)
    shutil.copytree(moved, again, symlinks=True)
    final, text = run(cfg, out, "again", root=again)
    assert sorted(names(final)) == ["MY CTF", "MY OWN"]
    assert "is built in now" not in text
    for folder in (root, again, final.parent.parent):
        shutil.rmtree(folder, ignore_errors=True)
