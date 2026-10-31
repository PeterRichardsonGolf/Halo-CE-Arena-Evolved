"""The test harness itself (tools/ae_test): spec parsing, the games' environment, debug.txt counts, the
verdicts and baseline comparison, images (BMP to PNG, frame picking, reticle offsets), the handshake
comparison, the Windows runner contract and the remote argument rewriting. No game, box or network.
"""
import io
import json
import os
import struct
import sys
import tempfile
import time
import unittest
from contextlib import redirect_stdout
from pathlib import Path

HERE = Path(__file__).resolve().parent / "ae_test"
sys.path.insert(0, str(HERE))
# the seeds revision 16 added (the OCTAGON drills): not in the golden roots before it
OCTAGON_SEEDS = ("AE OCT BR", "AE OCT BRSN", "AE OCT MAG", "AE OCT MGSN")
# and revision 17's (their team versions)
TEAM_OCTAGON_SEEDS = ("AE OCT TBR", "AE OCT TBSN", "AE OCT TMAG", "AE OCT TMSN")
# and revision 22's (the AE BTB set, the AE COMP 2V2 set, their battle rifle versions)
BTB_SEEDS = ("AE BTB SLY", "AE BTB CTF", "AE BTB KING", "AE BTB BALL", "AE BTB HVY")
COMP_2V2_SEEDS = ("AE C2V2 TS", "AE C2V2 CTF", "AE C2V2 KOH", "AE C2V2 OB")
BR_22_SEEDS = ("AE BR BTB S", "AE BR BTB C", "AE BR BTB K", "AE BR BTB B", "AE BR BTB H", "AE BR C2 TS", "AE BR C2CTF",
               "AE BR C2KOH", "AE BR C2 OB")
REVISION_22_SEEDS = BTB_SEEDS + COMP_2V2_SEEDS + BR_22_SEEDS
# and revision 24's (AE GRIFBALL and AE BOMB: the Bomb game type, in the builds with it)
REVISION_24_SEEDS = ("AE GRIFBALL", "AE BOMB")
# and revision 26's (AE SND, AE SEARCH AND DESTROY: AE BOMB with the Bomb variant's SEARCH AND DESTROY bit)
REVISION_26_SEEDS = ("AE SND",)
ADDED_SEEDS = OCTAGON_SEEDS + TEAM_OCTAGON_SEEDS + REVISION_22_SEEDS + REVISION_24_SEEDS + REVISION_26_SEEDS
# (OpenCE's asset-free tests, tools/harness, are a package of the same name,
# imported by it too: ours while these tools are imported, then the name is
# theirs again, whichever tests one pytest run collects first)
_other_harness = sys.modules.pop("harness", None)

import harness  # noqa: E402
import handshake  # noqa: E402
import record  # noqa: E402
import run  # noqa: E402
import sheet  # noqa: E402
import smoke  # noqa: E402
import windows  # noqa: E402
import gametype_file  # noqa: E402
import seed_compare  # noqa: E402
import strip_checks  # noqa: E402
import panel_checks  # noqa: E402

del sys.modules["harness"]
if _other_harness is not None:
    sys.modules["harness"] = _other_harness

DEBUG_MP = """\
arena-evolved: Halo CE: Arena Evolved 0.1.0-beta-dev (dev, release config, commit 623d8f22) Linux x64
arena-evolved: OpenCE network version 20 (joins 11-20); Delta 1, wire ae-20a, legacy table: built in; protocol auto
10.07.26 04:23:01  joining a host of network version 20 (this machine's is 20)
arena-evolved: Delta Peer: machine 1 speaks Delta (build X, pc_linux, network version 20); capabilities 0x3, agreed 0x3
10.06.26 21:18:27  the map 'levels\\ui\\ui' is corrected: tag 'sky' (sky ):  has the wrong parent groups
10.06.26 21:18:30  scenario scripts: 12 scripts, 4 globals
10.06.26 21:18:30  the map's script x calls map_reset, which a map's scripts may not; 2 scripts won't run, 0 globals
10.06.26 21:37:51  EXCEPTION assert in c:\\halo\\SOURCE\\camera\\observer.c,#899: <message> (release build)
arena-evolved: network test: tick 562 player 0: (21.420 11.432 -0.217) h1.00/1.00 g4/0 k2 d1 f0 | items 30 (+34) |
arena-evolved: network test: tick 592 player 0: (21.5 11.4 -0.2) h1 k3 d1 f0 player 1: (1.0 2.0 3.0) h1 k1 d3 f0 | items 31 (+34) |
arena-evolved: exiting after debug.exit_after
"""


def bmp(width, height, pixels, top_down=True, bpp=32):
    """a BMP as the game writes it (32-bit BGRA, rows from the top) or 24-bit bottom-up"""
    n = bpp // 8
    stride = (width * n + 3) & ~3
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            r, g, b = pixels[y * width + x]
            row += bytes([b, g, r] + ([255] if n == 4 else []))
        row += b"\0" * (stride - len(row))
        rows.append(bytes(row))
    if not top_down:
        rows.reverse()
    data = b"".join(rows)
    header = b"BM" + struct.pack("<IHHI", 54 + len(data), 0, 0, 54) + struct.pack(
        "<IiiHHIIiiII", 40, width, -height if top_down else height, 1, bpp, 0, len(data), 0, 0, 0, 0)
    return header + data


class Config(unittest.TestCase):
    def test_defaults_and_file(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "c.json"
            p.write_text(json.dumps({"parallel": 2, "box": {"ssh": "box"}}))
            cfg = harness.load_config(str(p))
            self.assertEqual(cfg["parallel"], 2)
            self.assertEqual(cfg["box"]["ssh"], "box")
            self.assertEqual(cfg["box"]["harness_dir"], harness.DEFAULTS["box"]["harness_dir"])
            self.assertEqual(cfg["max_games_total"], 4)

    def test_sample_config_is_json_without_personal_paths(self):
        text = (HERE / "config.sample.json").read_text()
        json.loads(text)
        self.assertNotIn("/" + "home/", text)

    def test_tmp_refused(self):
        with self.assertRaises(SystemExit):
            harness.check_not_tmp("/tmp/x/save", "save root")
        harness.check_not_tmp(str(Path.home() / "halo-test"), "save root")

    def test_no_personal_paths_in_code(self):
        for f in list(HERE.glob("*.py")) + [HERE / "config.sample.json", Path(__file__)]:
            text = f.read_text()
            self.assertFalse("/" + "home/" in text, f.name)


class Specs(unittest.TestCase):
    def test_unknown_key(self):
        with self.assertRaises(ValueError):
            harness.parse_spec({"name": "x", "mapp": "bloodgulch"})

    def test_bad_values(self):
        with self.assertRaises(ValueError):
            harness.parse_spec({"local_players": 5})
        with self.assertRaises(ValueError):
            harness.parse_spec({"window": "big"})

    def test_kinds(self):
        self.assertEqual(harness.spec_kind(harness.parse_spec({"map": "bloodgulch"})), "mp")
        self.assertEqual(harness.spec_kind(harness.parse_spec({"map": "levels\\a10\\a10"})), "campaign")
        self.assertEqual(harness.spec_kind(harness.parse_spec({})), "menu")
        self.assertEqual(harness.spec_kind(harness.parse_spec({"network_test": "join"})), "join")

    def test_mp_env(self):
        s = harness.parse_spec({"map": "bloodgulch", "gametype": "ctf", "local_players": 3, "flags": 25100321,
                                "start": 25, "mod": "", "window": "1280x720", "screenshots": 100,
                                "env": {"HALO_MATCH_CLOCK": "both"}})
        e = harness.spec_env(s, "/r", "/s", "/shots")
        self.assertEqual(e["HALO_NETWORK_TEST"], "host:bloodgulch:ctf")
        self.assertEqual(e["HALO_NETWORK_TEST_LOCAL_PLAYERS"], "3")
        self.assertEqual(e["HALO_NETWORK_TEST_FLAGS"], "25100321")
        self.assertEqual(e["HALO_MOD"], "")
        self.assertEqual(e["HALO_DISPLAY_MODE"], "windowed")
        self.assertEqual(e["HALO_WINDOW_SIZE"], "1280x720")
        self.assertEqual(e["HALO_SCREENSHOT_EVERY"], "100")
        self.assertEqual(e["HALO_NET_ONLINE"], "false")
        self.assertEqual(e["SDL_AUDIO_DRIVER"], "dummy")
        self.assertEqual(e["HALO_MATCH_CLOCK"], "both")

    def test_campaign_init(self):
        s = harness.parse_spec({"map": "levels\\a10\\a10"})
        self.assertEqual(harness.spec_init(s), ["map_name levels\\a10\\a10"])
        self.assertNotIn("HALO_NETWORK_TEST", harness.spec_env(s, "/r", "/s"))

    def test_map_available(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "maps" / "ce").mkdir(parents=True)
            (Path(d) / "mods" / "NHE" / "maps").mkdir(parents=True)
            (Path(d) / "maps" / "bloodgulch.map").touch()
            (Path(d) / "mods" / "NHE" / "maps" / "badcreek.map").touch()
            cfg = dict(harness.DEFAULTS, data_dir=d)
            self.assertTrue(harness.map_available(cfg, {"map": "bloodgulch"})[0])
            self.assertTrue(harness.map_available(cfg, {"map": "badcreek", "mod": "NHE"})[0])
            self.assertFalse(harness.map_available(cfg, {"map": "badcreek"})[0])
            self.assertFalse(harness.map_available(cfg, {"map": "beavercreek_v2@ce"})[0])
            self.assertFalse(harness.map_available(cfg, {"mod": "CE+ X"})[0])

    def test_run_cli_spec(self):
        a = run.parser().parse_args(["--build", "abc", "--map", "levels\\a10\\a10", "--env", "A=1", "--players", "2"])
        s = run.spec_from_args(a)
        self.assertEqual(s["name"], "a10")
        self.assertEqual(s["env"], {"A": "1"})
        self.assertEqual(s["local_players"], 2)

    def test_saved_gametype_env(self):
        s = harness.parse_spec({"map": "bloodgulch", "saved_gametype": "AE PRO TS"})
        self.assertEqual(harness.spec_env(s, "/r", "/s")["HALO_NETWORK_TEST_GAMETYPE"], "AE PRO TS")
        self.assertNotIn("HALO_NETWORK_TEST_GAMETYPE", harness.spec_env(harness.parse_spec({"map": "x"}), "/r", "/s"))
        a = run.parser().parse_args(["--build", "abc", "--map", "bloodgulch", "--saved-gametype", "NHE 1V1",
                                     "--save-from", "~/saves"])
        s = run.spec_from_args(a)
        self.assertEqual(s["saved_gametype"], "NHE 1V1")
        self.assertEqual(s["save_from"], "~/saves")

    def test_pack_env(self):
        s = harness.parse_spec({"map": "bloodgulch", "pack_loadout": "both", "pack_map_spawns": True,
                                "pack_file": "~/packs/test.aepack"})
        e = harness.spec_env(s, "/r", "/s")
        self.assertEqual(e["HALO_NETWORK_TEST_PACK_LOADOUT"], "both")
        self.assertEqual(e["HALO_NETWORK_TEST_PACK_MAP_SPAWNS"], "1")
        # (the game opens it by Xbox path, as its own files: the harness links it into the data root)
        self.assertEqual(e["HALO_WEAPON_PACK_FILE"], "d:\\pack_file\\test.aepack")
        plain = harness.spec_env(harness.parse_spec({"map": "bloodgulch"}), "/r", "/s")
        for k in ("HALO_NETWORK_TEST_PACK_LOADOUT", "HALO_NETWORK_TEST_PACK_MAP_SPAWNS", "HALO_WEAPON_PACK_FILE"):
            self.assertNotIn(k, plain)
        off = harness.spec_env(harness.parse_spec({"map": "bloodgulch", "pack_map_spawns": False}), "/r", "/s")
        self.assertNotIn("HALO_NETWORK_TEST_PACK_MAP_SPAWNS", off)
        for text in ("false", "off", "0", 0):
            self.assertNotIn("HALO_NETWORK_TEST_PACK_MAP_SPAWNS", harness.spec_env(
                harness.parse_spec({"map": "bloodgulch", "pack_map_spawns": text}), "/r", "/s"))
        for text in ("true", "on", "1", 1):
            self.assertEqual(harness.spec_env(harness.parse_spec({"map": "bloodgulch", "pack_map_spawns": text}),
                                              "/r", "/s")["HALO_NETWORK_TEST_PACK_MAP_SPAWNS"], "1")
        with self.assertRaises(ValueError):
            harness.parse_spec({"map": "bloodgulch", "pack_map_spawns": "maybe"})
        for bad_expect in ({"lines": "weapon pack"}, {"no_lines": "x"}, {"lines": [3]}):
            with self.assertRaises(ValueError):
                harness.parse_spec({"map": "bloodgulch", "expect": bad_expect})
        for bad in ("secondry", "", 3):
            with self.assertRaises(ValueError):
                harness.parse_spec({"map": "bloodgulch", "pack_loadout": bad})
        a = run.parser().parse_args(["--build", "abc", "--map", "bloodgulch", "--pack-loadout", "primary",
                                     "--pack-map-spawns", "--packs-from", "~/p", "--expect-line", "a b",
                                     "--expect-no-line", "c"])
        s = run.spec_from_args(a)
        self.assertEqual((s["pack_loadout"], s["pack_map_spawns"], s["packs_from"]), ("primary", True, "~/p"))
        self.assertEqual(s["expect"], {"lines": ["a b"], "no_lines": ["c"]})

    def test_packs_linked(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            d = Path(d)
            (d / "data" / "maps").mkdir(parents=True)
            (d / "data" / "packs").mkdir()
            (d / "mine").mkdir()
            b = d / "b"
            b.mkdir()
            (b / "halo").write_bytes(b"x")
            build = {"dir": str(b), "binary": str(b / "halo"), "env": {}}
            cfg = dict(harness.DEFAULTS, data_dir=str(d / "data"))
            prep = harness.prepare_game(cfg, harness.parse_spec({"name": "a", "map": "x"}), build,
                                        d / "work" / "a", d / "out" / "a")
            self.assertEqual((Path(prep["root"]) / "packs").resolve(), (d / "data" / "packs").resolve())
            prep = harness.prepare_game(cfg, harness.parse_spec({"name": "b", "map": "x", "packs_from": str(d / "mine")}),
                                        build, d / "work" / "b", d / "out" / "b")
            self.assertEqual((Path(prep["root"]) / "packs").resolve(), (d / "mine").resolve())
            with self.assertRaises(SystemExit):
                harness.prepare_game(cfg, harness.parse_spec({"name": "c", "map": "x", "packs_from": str(d / "no")}),
                                     build, d / "work" / "c", d / "out" / "c")
            with self.assertRaises(SystemExit):
                harness.prepare_game(cfg, harness.parse_spec({"name": "t", "map": "x", "packs_from": "/tmp/p"}),
                                     build, d / "work" / "t", d / "out" / "t")
            # (a pack_file: linked as the data root's pack_file/<name>, beside any packs/)
            (d / "mine" / "flipped.aepack").write_bytes(b"AEWPACK")
            prep = harness.prepare_game(cfg, harness.parse_spec({"name": "f", "map": "x", "packs_from": str(d / "mine"),
                                                                 "pack_file": str(d / "mine" / "flipped.aepack")}),
                                        build, d / "work" / "f", d / "out" / "f")
            linked = Path(prep["root"]) / "pack_file" / "flipped.aepack"
            self.assertEqual(linked.resolve(), (d / "mine" / "flipped.aepack").resolve())
            self.assertEqual(prep["env"]["HALO_WEAPON_PACK_FILE"], "d:\\pack_file\\flipped.aepack")
            with self.assertRaises(SystemExit):
                harness.prepare_game(cfg, harness.parse_spec({"name": "g", "map": "x", "pack_file": "/tmp/p.aepack"}),
                                     build, d / "work" / "g", d / "out" / "g")
            # (packs_from "none": no packs/ at all, whatever data_dir holds; run.py --no-packs)
            prep = harness.prepare_game(cfg, harness.parse_spec({"name": "n", "map": "x", "packs_from": "none"}),
                                        build, d / "work" / "n", d / "out" / "n")
            self.assertTrue((d / "data" / "packs").is_dir())
            self.assertFalse((Path(prep["root"]) / "packs").exists())
            a = run.parser().parse_args(["--build", "abc", "--map", "x", "--no-packs"])
            self.assertEqual(run.spec_from_args(a)["packs_from"], "none")
            with self.assertRaises(SystemExit):
                run.parser().parse_args(["--build", "abc", "--map", "x", "--no-packs", "--packs-from", "~/p"])
            # (a pack_file that is not there: the harness says so, the game is not started)
            with self.assertRaises(SystemExit):
                harness.prepare_game(cfg, harness.parse_spec({"name": "h", "map": "x",
                                                              "pack_file": str(d / "mine" / "typo.aepack")}),
                                     build, d / "work" / "h", d / "out" / "h")
            # (no packs folder anywhere: none linked)
            (d / "data" / "packs").rmdir()
            prep = harness.prepare_game(cfg, harness.parse_spec({"name": "e", "map": "x"}), build,
                                        d / "work" / "e", d / "out" / "e")
            self.assertFalse((Path(prep["root"]) / "packs").exists())

    def test_save_from_copied(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            d = Path(d)
            (d / "data" / "maps").mkdir(parents=True)
            src = d / "from"
            (src / "UDATA" / "x").mkdir(parents=True)
            (src / "UDATA" / "x" / "blam.lst").write_text("seed")
            (src / "link.txt").symlink_to(src / "UDATA" / "x" / "blam.lst")
            b = d / "b"
            b.mkdir()
            (b / "halo").write_bytes(b"x")
            build = {"dir": str(b), "binary": str(b / "halo"), "env": {}}
            cfg = dict(harness.DEFAULTS, data_dir=str(d / "data"))
            spec = harness.parse_spec({"name": "g", "map": "bloodgulch", "save_from": str(src)})
            prep = harness.prepare_game(cfg, spec, build, d / "work" / "g", d / "out" / "g")
            self.assertEqual((Path(prep["save"]) / "UDATA" / "x" / "blam.lst").read_text(), "seed")
            # (links are copied as files: nothing in the copy points back into the source)
            self.assertFalse((Path(prep["save"]) / "link.txt").is_symlink())
            self.assertEqual((Path(prep["save"]) / "link.txt").read_text(), "seed")
            # (a shared save root is copied into once, when it is made: the next case keeps what the first left)
            spec = harness.parse_spec({"name": "h", "map": "bloodgulch", "save_from": str(src), "save": "s"})
            roots = {}
            prep = harness.prepare_game(cfg, spec, build, d / "work" / "h", d / "out" / "h", roots)
            (Path(prep["save"]) / "UDATA" / "x" / "blam.lst").write_text("played")
            prep = harness.prepare_game(cfg, spec, build, d / "work" / "i", d / "out" / "i", roots)
            self.assertEqual((Path(prep["save"]) / "UDATA" / "x" / "blam.lst").read_text(), "played")
            with self.assertRaises(SystemExit):
                harness.prepare_game(cfg, harness.parse_spec({"name": "t", "map": "x", "save_from": "/tmp/saves"}),
                                     build, d / "work" / "t", d / "out" / "t")
            with self.assertRaises(SystemExit):
                harness.prepare_game(cfg, harness.parse_spec({"name": "m", "map": "x", "save_from": str(d / "no")}),
                                     build, d / "work" / "m", d / "out" / "m")

    def test_run_dry_run(self):
        buf = io.StringIO()
        with redirect_stdout(buf):
            run.main(["--build", "abc", "--map", "damnation", "--dry-run"])
        self.assertEqual(json.loads(buf.getvalue())["env"]["HALO_NETWORK_TEST"], "host:damnation")


class GametypeFile(unittest.TestCase):
    """a saved gametype file's variant bytes patched and signed again (the signature over its first 0x68 bytes)"""

    def make(self, root, name, folder="ABCDEF012345"):
        block = bytearray(512)
        block[:24] = name.encode("utf-16-le").ljust(24, b"\0")
        block[0x1C] = 1
        f = Path(root) / "u" / "UDATA" / folder / "blam.lst"
        f.parent.mkdir(parents=True)
        f.write_bytes(gametype_file.sign(bytes(block)))
        return f

    def test_sign_and_patch(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            src = Path(d) / "from"
            f = self.make(src, "NHE 1V1")
            self.make(src, "AE PRO TS", "000000000001")
            original = f.read_bytes()
            self.assertTrue(gametype_file.signed(original))
            self.assertEqual(gametype_file.find(src, "nhe 1v1"), f)
            self.assertIsNone(gametype_file.find(src, "NHE 2V2"))
            # (copy first: the source is never written; the copy patched and signed again)
            to = Path(d) / "to"
            g = gametype_file.copy_and_patch(src, to, "NHE 1V1", {0x1D: 3})
            self.assertEqual(f.read_bytes(), original)
            self.assertTrue(str(g).startswith(str(to)))
            b = g.read_bytes()
            self.assertEqual((b[0x1C], b[0x1D], len(b)), (1, 3, 512))
            self.assertTrue(gametype_file.signed(b))
            self.assertTrue(gametype_file.find(to, "AE PRO TS"))
            # (a --to that exists, /tmp, real save roots and Quiver installs are refused)
            for frm, dst in ((src, to), (src, "/tmp/ae-to"), ("/tmp/ae-from", Path(d) / "t2"),
                             (Path.home() / ".local/share/halo-linux-x", Path(d) / "t3"),
                             (src, Path.home() / ".local/share/halo-linux-x/copy"),
                             (Path(d) / "Quiver Launcher" / "Apps" / "halo", Path(d) / "t4")):
                with self.assertRaises(SystemExit, msg=f"{frm} -> {dst}"):
                    gametype_file.copy_and_patch(frm, dst, "NHE 1V1", {0x1D: 3})
            # (only the variant's bytes, only byte values, and a signed file with a unique name)
            for values in ({0x68: 1}, {0x1D: 256}, {0x1D: -1}):
                with self.assertRaises(SystemExit):
                    gametype_file.copy_and_patch(src, Path(d) / "v", "NHE 1V1", values)
                self.assertFalse((Path(d) / "v").exists())
            self.make(src, "nhe 1v1", "000000000002")
            with self.assertRaises(SystemExit):
                gametype_file.find(src, "NHE 1V1")
            (src / "u" / "UDATA" / "000000000002" / "blam.lst").unlink()
            f.write_bytes(original[:0x30] + b"x" + original[0x31:])
            with self.assertRaises(SystemExit):
                gametype_file.copy_and_patch(src, Path(d) / "w", "NHE 1V1", {0x1D: 4})
            f.write_bytes(original)
            with redirect_stdout(io.StringIO()):
                self.assertEqual(gametype_file.main(["--from", str(src), "--to", str(Path(d) / "m"),
                                                     "--name", "AE PRO TS", "--set", "0x1d=5"]), 0)
            self.assertEqual(gametype_file.find(Path(d) / "m", "AE PRO TS").read_bytes()[0x1D], 5)

    @staticmethod
    def universal_variant_offsets():
        """struct universal_variant's fields as game_engine.h declares them: {name: offset in the file's variant}
        (its byte, boolean, long, unsigned long and real fields, in order, each at its natural alignment)"""
        import re
        header = (Path(__file__).resolve().parent.parent / "source" / "game" / "game_engine.h").read_text()
        body = header[header.index("struct universal_variant\n{"):]
        body = body[:body.index("};")]
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        sizes = {"boolean": 1, "byte": 1, "long": 4, "unsigned long": 4, "real": 4}
        offsets, at = {}, 0
        for kind, name in re.findall(r"^\s*(unsigned long|boolean|byte|long|real)\s+(\w+);", body, flags=re.M):
            size = sizes[kind]
            at = (at + size - 1) // size * size
            offsets[name] = gametype_file.UNIVERSAL_VARIANT_OFFSET + at
            at += size
        return offsets

    def test_named_offsets(self):
        # (every NAMED field where game_engine.h has it: pad5 0x2a COMBAT FEEDBACK / SILENT, pack_loadout 0x2b)
        fields = self.universal_variant_offsets()
        self.assertEqual(fields["teams"], 0x1C)
        for name, offset in gametype_file.NAMED.items():
            self.assertEqual(fields.get(name), offset, name)
        self.assertEqual(gametype_file.NAMED["weapon_pack"], 0x29)
        self.assertEqual(gametype_file.NAMED["pad5"], 0x2A)
        self.assertEqual(gametype_file.NAMED["pack_loadout"], 0x2B)
        self.assertNotIn("pack_map_spawns", gametype_file.NAMED)
        self.assertEqual(gametype_file.NAMED["nhe_mode"], 0x1D)
        self.assertEqual(gametype_file.parse_set("weapon_pack=1"), (0x29, 1))
        self.assertEqual(gametype_file.parse_set("pack_loadout=17"), (0x2B, 17))
        self.assertEqual(gametype_file.parse_set("0x2a=5"), (0x2A, 5))
        with self.assertRaises(SystemExit):
            gametype_file.parse_set("weapon_pak=1")
        with self.assertRaises(SystemExit):
            gametype_file.parse_set("pack_map_spawns=1")
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            src = Path(d) / "src"
            self.make(src, "AE PRO TS", "AAAAAAAAAAAA")
            with redirect_stdout(io.StringIO()):
                self.assertEqual(gametype_file.main(["--from", str(src), "--to", str(Path(d) / "n"), "--name",
                                                     "AE PRO TS", "--set", "weapon_pack=1", "--set",
                                                     f"pack_loadout={1 | gametype_file.PACK_MAP_SPAWNS_BIT}"]), 0)
            b = gametype_file.find(Path(d) / "n", "AE PRO TS").read_bytes()
            # (pad5 left alone)
            self.assertEqual(list(b[0x29:0x2C]), [1, 0, 0x11])
            self.assertTrue(gametype_file.signed(b))

    def test_own_copy(self):
        """--own: a player's own gametype of a seed's name, in the folder the game keeps that name in"""
        # (folders a seeded root has: xbox_xapi.c's save_name_hash)
        self.assertEqual(gametype_file.save_folder_name("AE TEAM SLY"), "04B847B13D90")
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            src = Path(d) / "from"
            f = self.make(src, "AE TEAM SLY", gametype_file.save_folder_name("AE TEAM SLY"))
            g = gametype_file.copy_and_patch(src, Path(d) / "to", "AE TEAM SLY", {0x40: 33}, own_name="TS 50")
            self.assertEqual(g.parent.name, gametype_file.save_folder_name("TS 50"))
            b = g.read_bytes()
            self.assertEqual((gametype_file.name_of(b), b[0x40]), ("TS 50", 33))
            self.assertTrue(gametype_file.signed(b))
            self.assertEqual((g.parent / "SaveMeta.xbx").read_bytes(), "TS 50".encode("utf-16-le"))
            # (the original stays, unpatched)
            self.assertEqual(gametype_file.find(Path(d) / "to", "AE TEAM SLY").read_bytes(), f.read_bytes())
            # (a name in use, or too long, is refused)
            for own in ("AE TEAM SLY", "TWELVE CHARS"):
                with self.assertRaises(SystemExit):
                    gametype_file.copy_and_patch(src, Path(d) / own.replace(" ", "_"), "AE TEAM SLY", {}, own_name=own)


class SeedCompare(unittest.TestCase):
    """saved gametypes' hashes by stored name: content (after the name) and whole file"""

    def make(self, root, name, folder, value=1):
        block = bytearray(512)
        block[:24] = name.encode("utf-16-le").ljust(24, b"\0")
        block[0x40] = value
        block[0x100:0x104] = b"GPVO"
        f = Path(root) / "u" / "UDATA" / folder / "blam.lst"
        f.parent.mkdir(parents=True)
        f.write_bytes(gametype_file.sign(bytes(block)))

    def test_golden_rename_and_same_as(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            a, b = Path(d) / "a", Path(d) / "b"
            self.make(a, "AE PRO TS", "01")
            self.make(a, "TS 50", "02")
            self.make(b, "AE COMP TS", "01")
            self.make(b, "TS 50", "02")
            golden = seed_compare.gametype_hashes(a)
            hb = seed_compare.gametype_hashes(b)
            # (a rename keeps the content: found under the new name)
            self.assertEqual(seed_compare.compare_golden(hb, golden, {"AE PRO TS": "AE COMP TS"}), [])
            self.assertEqual(seed_compare.compare_golden(hb, golden), ["AE PRO TS: missing"])
            self.assertEqual(seed_compare.compare_roots(hb, golden, ["AE PRO TS"]), [])
            self.assertEqual(seed_compare.compare_roots(hb, golden), ["AE PRO TS: missing"])
            # (a changed value: content and file differ)
            c = Path(d) / "c"
            self.make(c, "TS 50", "02", value=2)
            hc = seed_compare.gametype_hashes(c)
            self.assertEqual(seed_compare.compare_golden(hc, golden, only=["TS 50"]),
                             ["TS 50: content differs from golden TS 50"])
            self.assertEqual(seed_compare.compare_roots(hc, golden, ["AE PRO TS"]), ["TS 50: not byte-identical (or moved)"])
            self.make(c, "ts 50", "03")
            with self.assertRaises(SystemExit):
                seed_compare.gametype_hashes(c)
            # (content covers the bytes past the options block too: matches compares all 512)
            e = Path(d) / "e"
            self.make(e, "TS 50", "02")
            f = e / "u" / "UDATA" / "02" / "blam.lst"
            raw = bytearray(f.read_bytes())
            raw[0x150] = 7
            f.write_bytes(bytes(raw))
            self.assertNotEqual(seed_compare.gametype_hashes(e)["TS 50"]["content"], golden["TS 50"]["content"])

    def test_migrations_old_block_hashes_are_golden(self):
        """each migration's old_block_hash (arena_gametypes.c) is the golden hash of the file the builds before
        seeded under the old name: revisions 1-3 golden_seeds_a17102e2.json's "block" (AE SLAYER / AE ODDBALL
        derived from a17102e2's AE FFA SLAY / AE FFA BALL under their old names), revision 4
        golden_seeds_rev3_083c49e9.json's (a root seeded at revision 3 by 083c49e9), revision 5
        golden_seeds_rev4_da271472.json's (a root seeded at revision 4 by da271472's tree), revision 6
        golden_seeds_rev5_736ce3a7.json's (a root seeded at revision 5 by 736ce3a7's tree), revision 7
        golden_seeds_rev6_519ce8a3.json's (a root seeded at revision 6 by 519ce8a3's tree), revision 12
        golden_seeds_rev11.json's (a root seeded at revision 11), revision 14 golden_seeds_rev13.json's (a root seeded
        at revision 13 by 971ed2fe's tree)"""
        import re
        source = (HERE.parent.parent / "source" / "saved games" / "arena_gametypes.c").read_text()
        table = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        found = re.findall(r'\.revision = (\d+),\s*\.old_row = \{ \.name = "([^"]+)".*?\.old_block_hash = "([0-9a-f]{40})"',
                           table, re.S)
        self.assertGreaterEqual(len(found), 41)
        golden = json.loads((HERE / "golden_seeds_a17102e2.json").read_text())
        golden_rev3 = json.loads((HERE / "golden_seeds_rev3_083c49e9.json").read_text())
        golden_rev4 = json.loads((HERE / "golden_seeds_rev4_da271472.json").read_text())
        golden_rev5 = json.loads((HERE / "golden_seeds_rev5_736ce3a7.json").read_text())
        golden_rev6 = json.loads((HERE / "golden_seeds_rev6_519ce8a3.json").read_text())
        golden_rev11 = json.loads((HERE / "golden_seeds_rev11.json").read_text())
        golden_rev13 = json.loads((HERE / "golden_seeds_rev13.json").read_text())
        for revision, name, digest in found:
            # (revision 15 on, as tools/ae_gametype_change.py makes them: golden_seeds_rev<N - 1>.json, a root seeded
            # by the revision before's tree)
            g = {4: golden_rev3, 5: golden_rev4, 6: golden_rev5, 7: golden_rev6, 12: golden_rev11,
                 14: golden_rev13}.get(int(revision))
            if g is None and int(revision) > 14:
                g = json.loads((HERE / f"golden_seeds_rev{int(revision) - 1}.json").read_text())
            self.assertEqual((g or golden)[name]["block"], digest, name)

    def test_golden_revision_11(self):
        """golden_seeds_rev11.json: a root seeded at revision 11 (all 86 seeds; a rev-10 root brought to 11 holds the
        same files): every seed of arena_gametypes.c's table there, the 61 of before as revision 10 seeded them
        (golden_seeds_rev6's where unchanged since, e.g. the NHE set), each AE BR twin other than its AE gametype"""
        import re
        source = (HERE.parent.parent / "source" / "saved games" / "arena_gametypes.c").read_text()
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        seeds = [n for n in re.findall(r'^\t\{ "([^"]+)"', table, re.M) if n not in ADDED_SEEDS]
        golden = json.loads((HERE / "golden_seeds_rev11.json").read_text())
        self.assertEqual(sorted(golden), sorted(seeds))
        self.assertEqual(len(seeds), 86)
        twins = {n: t for n, t in re.findall(r'\{ "(AE BR [^"]+)", "([^"]+)" \},', source) if n not in ADDED_SEEDS}
        self.assertEqual(len(twins), 25)
        for name, twin in twins.items():
            self.assertIn(twin, golden, name)
            self.assertNotEqual(golden[name]["content"], golden[twin]["content"], name)
        golden_rev6 = json.loads((HERE / "golden_seeds_rev6_519ce8a3.json").read_text())
        for name in ("TS 50", "CTF WIZARD", "1 V 1 NR"):
            self.assertEqual(golden[name]["block"], golden_rev6[name]["block"], name)

    def test_golden_revision_12(self):
        """golden_seeds_rev12.json: a root seeded at revision 12 (a rev-11 root brought to 12 holds the same files but
        the ones its player edited): the 19 seeds of ARENA_RADAR_ENEMIES (PLAYERS ON RADAR ENEMIES), each with a
        revision 12 migration, differ from revision 11's, in their folders; every other seed is revision 11's"""
        import re
        source = (HERE.parent.parent / "source" / "saved games" / "arena_gametypes.c").read_text()
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        rows = [r for r in re.findall(r'^\t\{ "([^"]+)",(.*?)(?=^\t\{ "|^\};)', table, re.M | re.S)
                if r[0] not in ADDED_SEEDS]
        enemies = sorted(name for name, body in rows if "ARENA_RADAR_ENEMIES" in body)
        self.assertEqual(len(rows), 86)
        self.assertEqual(len(enemies), 19)
        migrations = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        rev12 = re.findall(r'\.revision = 12,\s*\.old_row = \{ \.name = "([^"]+)"', migrations)
        self.assertEqual(sorted(rev12), enemies)
        golden = json.loads((HERE / "golden_seeds_rev12.json").read_text())
        golden_rev11 = json.loads((HERE / "golden_seeds_rev11.json").read_text())
        self.assertEqual(sorted(golden), sorted(name for name, _ in rows))
        changed = sorted(name for name in golden if golden[name]["block"] != golden_rev11[name]["block"])
        self.assertEqual(changed, enemies)
        for name in golden:
            self.assertEqual(golden[name]["folder"], golden_rev11[name]["folder"], name)

    def test_slayer_fields(self):
        """seed_compare's "slayer": a slayer variant's no_death_bonus, no_kill_penalty, kill_in_order bytes (0x4c..);
        none for another engine"""
        block = bytearray(512)
        block[0x18:0x1C] = (2).to_bytes(4, "little")
        block[0x4C:0x4F] = b"\x01\x00\x01"
        self.assertEqual(seed_compare.slayer_fields(block),
                         {"no_death_bonus": 1, "no_kill_penalty": 0, "kill_in_order": 1})
        block[0x18:0x1C] = (3).to_bytes(4, "little")
        self.assertIsNone(seed_compare.slayer_fields(block))

    @staticmethod
    def _slayer_seeds():
        """arena_gametypes.c's table rows built by a stock Slayer-engine builder (game_engine.c), as {name: (builder,
        adjust)}, and the builders that already have neither death bonus nor kill penalty"""
        import re
        root = HERE.parent.parent
        engine = (root / "source" / "game" / "game_engine.c").read_text(encoding="latin-1")
        builders = {m[1]: m[0] for m in re.finditer(r"^struct game_variant \*(build_game_variant_\w+)\(.*?^\}", engine,
                                                     re.M | re.S)}
        slayer = {n for n, body in builders.items() if "game_engine_index = game_engine_slayer;" in body}
        steady = {n for n in slayer if "slayer.no_death_bonus = TRUE;" in builders[n] and
                  "slayer.no_kill_penalty = TRUE;" in builders[n]}
        source = (root / "source" / "saved games" / "arena_gametypes.c").read_text(encoding="latin-1")
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        rows = re.findall(r'^\t\{ "([^"]+)", (build_game_variant_\w+),(.*?)(?=^\t\{ "|^\};)', table, re.M | re.S)
        seeds = {name: (build, (re.findall(r"\.adjust = (\w+)", body) or [None])[0]) for name, build, body in rows
                 if build in slayer}
        return seeds, steady, source

    def test_slayers_without_speed_changes(self):
        """revision 14 (owner, 2026-10-20): every seeded slayer gametype plays without the death bonus and the kill
        penalty: its adjust sets both (arena_gametype_no_speed_changes, or the NHE set's arena_gametype_nhe_slayer), or
        its stock builder has neither (Phantoms). A fresh revision 14 root (golden_seeds_rev14.json) has both bytes set
        in every slayer gametype; a revision 13 root (golden_seeds_rev13.json) had neither in the 27 revision 14 updates
        in place, under their names, each from its exact revision 13 file (old_block_hash), so a gametype the player
        edited or made himself is left alone. Nothing else changes: same folders, the other seeds' files as before"""
        import re
        seeds, steady, source = self._slayer_seeds()
        self.assertGreaterEqual(int(re.search(r"ARENA_GAMETYPES_REVISION = (\d+),", source)[1]), 14)
        # (revisions 16 and 17's OCTAGON drills, slayers seeded without both from the start: test_octagon_drills)
        seeds = {n: v for n, v in seeds.items() if n not in ADDED_SEEDS}
        self.assertEqual(len(seeds), 38)
        bodies = {}
        for name in re.findall(r"^static void (arena_gametype_\w+)\(\n\tstruct game_variant \*variant,\n\tstruct "
                               r"game_variant_options \*options\)\n\{", source, re.M):
            body = source[source.index(f"static void {name}(\n\tstruct game_variant *variant,\n\tstruct game_variant_options *options)\n{{"):]
            bodies[name] = body[:body.index("\n}\n")]
        for name in ("arena_gametype_no_speed_changes", "arena_gametype_nhe_slayer"):
            self.assertIn("variant->game_engine_variant.slayer.no_death_bonus = TRUE;", bodies[name], name)
            self.assertIn("variant->game_engine_variant.slayer.no_kill_penalty = TRUE;", bodies[name], name)

        def calls(adjust):
            """the adjust and every adjust it calls (a later revision's new adjust calls the one it follows:
            tools/ae_gametype_change.py)"""
            out = {adjust}
            for called in re.findall(r"^\t(arena_gametype_\w+)\(variant, options\);", bodies.get(adjust, ""), re.M):
                out |= calls(called)
            return out

        updated = sorted(n for n, (build, adjust) in seeds.items() if "arena_gametype_no_speed_changes" in calls(adjust))
        for name, (build, adjust) in seeds.items():
            self.assertTrue(calls(adjust) & {"arena_gametype_no_speed_changes", "arena_gametype_nhe_slayer"} or
                            build in steady, name)
        self.assertEqual(len(updated), 27)
        self.assertEqual(sorted(n for n, (b, _) in seeds.items() if b in steady), ["AE BR PHANT", "AE PHANTOMS"])
        # (the migrations: one same-name update each, the old row the new one without the adjust, a real hash)
        migrations = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        rev14 = re.findall(r'\{ \.revision = 14,\s*\.old_row = (\{ \.name = "([^"]+)".*?\}),\s*\.new_row = '
                           r'(\{ \.name = "([^"]+)".*?\}),\s*\.old_block_hash = "([0-9a-f]{40})" \}', migrations, re.S)
        self.assertEqual(migrations.count(".revision = 14,"), len(rev14))
        self.assertEqual(sorted(old_name for _, old_name, _, _, _ in rev14), updated)
        squash = lambda s: re.sub(r"\s+", " ", s)
        for old_row, old_name, new_row, new_name, _ in rev14:
            self.assertEqual(old_name, new_name)
            self.assertNotIn(".adjust", old_row, old_name)
            self.assertEqual(squash(new_row), squash(old_row[:-2]) + ", .adjust = arena_gametype_no_speed_changes }",
                             old_name)
        # (the files: a fresh revision 14 root against a revision 13 one)
        golden = json.loads((HERE / "golden_seeds_rev14.json").read_text())
        golden_rev13 = json.loads((HERE / "golden_seeds_rev13.json").read_text())
        self.assertEqual(sorted(golden), sorted(golden_rev13))
        self.assertEqual(sorted(n for n, v in golden.items() if "slayer" in v), sorted(seeds))
        for name, value in golden.items():
            self.assertEqual(value["folder"], golden_rev13[name]["folder"], name)
            if "slayer" in value:
                self.assertEqual((value["slayer"]["no_death_bonus"], value["slayer"]["no_kill_penalty"]), (1, 1), name)
                self.assertEqual(value["slayer"]["kill_in_order"], golden_rev13[name]["slayer"]["kill_in_order"], name)
        self.assertEqual(sorted(n for n in golden if golden[n]["block"] != golden_rev13[n]["block"]), updated)
        for name in updated:
            self.assertEqual((golden_rev13[name]["slayer"]["no_death_bonus"],
                              golden_rev13[name]["slayer"]["no_kill_penalty"]), (0, 0), name)

    def test_octagon_drills(self):
        """revision 16 (owner, 2026-10-20): the OCTAGON set's four 1v1 drills, added seeds (no migration): AE FFA
        SLAY's row but instant respawns, no grenades, INFINITE AMMO and BOTTOMLESS CLIP (arena_gametype_octagon_drill:
        no speed changes, then the pack_loadout bits 5-6), each its start. golden_seeds_rev16.json is a fresh revision
        16 root: revision 15's 86 files as they were, in their folders, and the four drills; seed_rules expects them"""
        import re
        import seed_rules
        source = (HERE.parent.parent / "source" / "saved games" / "arena_gametypes.c").read_text(encoding="latin-1")
        self.assertEqual(int(re.search(r"ARENA_GAMETYPES_REVISION = (\d+),", source)[1]) >= 16, True)
        migrations = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        self.assertNotIn(".revision = 16,", migrations)
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        rows = dict(re.findall(r'^\t\{ "([^"]+)",(.*?)(?=^\t\{ "|^\};)', table, re.M | re.S))
        for name in OCTAGON_SEEDS:
            # (as one line: a later revision's tool may wrap the row otherwise)
            body = re.sub(r"\s+", " ", rows[name])
            self.assertIn("build_game_variant_slayer", body, name)
            self.assertIn(".starting_frags = STARTING_GRENADES_NONE", body, name)
            self.assertIn(".adjust = arena_gametype_octagon_drill", body, name)
            self.assertIn("ARENA_FFA_SCORE, ARENA_TIME_LIMIT, 0, ARENA_STOCK, ARENA_RADAR_ON", body, name)
        drill = source[source.index("static void arena_gametype_octagon_drill(\n\tstruct game_variant *variant,\n"
                                    "\tstruct game_variant_options *options)\n{"):]
        drill = drill[:drill.index("\n}\n")]
        self.assertIn("arena_gametype_no_speed_changes(variant, options);", drill)
        self.assertIn("ae_weapon_ammo_byte_set(variant->universal_variant.pack_loadout, 1, 1)", drill)
        golden = json.loads((HERE / "golden_seeds_rev16.json").read_text())
        golden_rev15 = json.loads((HERE / "golden_seeds_rev15.json").read_text())
        self.assertEqual(sorted(golden), sorted(list(golden_rev15) + list(OCTAGON_SEEDS)))
        for name, value in golden_rev15.items():
            self.assertEqual(golden[name], value, name)
        self.assertEqual(len({golden[n]["folder"] for n in golden}), len(golden))
        for name in OCTAGON_SEEDS:
            self.assertEqual((golden[name]["slayer"]["no_death_bonus"], golden[name]["slayer"]["no_kill_penalty"]),
                             (1, 1), name)
            want, spawn, frags = seed_rules.ROWS[name]
            self.assertEqual((want["respawn"], want["starting frags"], want["infinite ammo"], want["bottomless clip"],
                              frags), ("0 ticks (0 s)", "none", "on", "on", 0), name)
        self.assertEqual([seed_rules.ROWS[n][1] for n in OCTAGON_SEEDS],
                         ["battle_rifle", "battle_rifle + sniper rifle", "pistol", "pistol + sniper rifle"])
        # (every other seed expects both off, but revision 17's team drills: test_team_octagon_drills)
        for name, (want, _, _) in seed_rules.ROWS.items():
            if name not in ADDED_SEEDS:
                self.assertEqual((want["infinite ammo"], want["bottomless clip"]), ("off", "off"), name)

    def test_team_octagon_drills(self):
        """revision 17 (owner, 2026-10-20): the OCTAGON drills' four team versions, added seeds (no migration): AE TEAM
        SLY's row (team slayer, 50 kills, the motion sensor's enemies) with the drills' changes (instant respawns, no
        grenades, arena_gametype_octagon_drill) and their starts, after the free for all drills in the OCTAGON set.
        golden_seeds_rev17.json is a fresh revision 17 root: revision 16's 90 files as they were, in their folders, and
        the four team drills; seed_rules expects them"""
        import re
        import seed_rules
        source = (HERE.parent.parent / "source" / "saved games" / "arena_gametypes.c").read_text(encoding="latin-1")
        self.assertEqual(int(re.search(r"ARENA_GAMETYPES_REVISION = (\d+),", source)[1]) >= 17, True)
        migrations = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        self.assertNotIn(".revision = 17,", migrations)
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        rows = dict(re.findall(r'^\t\{ "([^"]+)",(.*?)(?=^\t\{ "|^\};)', table, re.M | re.S))
        squash = lambda s: re.sub(r"\s+", " ", s)
        for team, ffa in zip(TEAM_OCTAGON_SEEDS, OCTAGON_SEEDS):
            body = squash(rows[team])
            self.assertIn("build_game_variant_team_slayer", body, team)
            self.assertIn("ARENA_TEAM_SLAYER_SCORE, ARENA_TIME_LIMIT, 0, ARENA_STOCK, ARENA_RADAR_ENEMIES", body, team)
            # (the rest of the row its free for all drill's)
            tail = lambda s: s[s.index("ARENA_VEHICLES_STOCK"):s.index(" },") + 3]
            self.assertEqual(tail(body), tail(squash(rows[ffa])), team)
        golden = json.loads((HERE / "golden_seeds_rev17.json").read_text())
        golden_rev16 = json.loads((HERE / "golden_seeds_rev16.json").read_text())
        self.assertEqual(sorted(golden), sorted(list(golden_rev16) + list(TEAM_OCTAGON_SEEDS)))
        for name, value in golden_rev16.items():
            self.assertEqual(golden[name], value, name)
        self.assertEqual(len({golden[n]["folder"] for n in golden}), len(golden))
        for team, ffa in zip(TEAM_OCTAGON_SEEDS, OCTAGON_SEEDS):
            self.assertEqual((golden[team]["slayer"]["no_death_bonus"], golden[team]["slayer"]["no_kill_penalty"]),
                             (1, 1), team)
            want, spawn, frags = seed_rules.ROWS[team]
            team_slayer = seed_rules.ROWS["AE TEAM SLY"][0]
            self.assertEqual((want["engine"], want["score to win"], want["radar players"], want["respawn"],
                              want["starting frags"], want["infinite ammo"], want["bottomless clip"], frags),
                             ("slayer (teams)", "50", "enemies", "0 ticks (0 s)", "none", "on", "on", 0), team)
            self.assertEqual(want["suicide penalty"], team_slayer["suicide penalty"], team)
            ffa_want, ffa_spawn, _ = seed_rules.ROWS[ffa]
            self.assertEqual((want["loadout"], want["weapon pack"], spawn),
                             (ffa_want["loadout"], ffa_want["weapon pack"], ffa_spawn), team)

    def test_btb_and_comp_2v2(self):
        """revision 22 (owner, 2026-10-22): the AE BTB set and the AE COMP 2V2 set, each with its battle rifle version,
        added seeds (no migration). Each BTB gametype is its AE casual team gametype's row with 20 minutes, the map's
        own vehicles and BTB's score (slayer 100); BTB HEAVIES team slayer with each team's vehicles CUSTOM (rocket
        warthogs and scorpions, arena_gametype_..._heavy_vehicles). Each COMP 2V2 gametype is its AE COMP team
        gametype's row (the slayer to 25). golden_seeds_rev22.json is a fresh revision 22 root: revision 21's 94 files as
        they were, in their folders, and the 18 new ones; seed_rules expects them"""
        import re
        import seed_rules
        source = (HERE.parent.parent / "source" / "saved games" / "arena_gametypes.c").read_text(encoding="latin-1")
        self.assertGreaterEqual(int(re.search(r"ARENA_GAMETYPES_REVISION = (\d+),", source)[1]), 22)
        migrations = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        # (the new seeds have no migration of revision 22: only revision 21's AE seeds do; later revisions may
        # migrate them, as 23's VEHICLES DESTRUCTIBLE does the BTB ones)
        for name in REVISION_22_SEEDS:
            self.assertNotRegex(migrations, r'\.revision = 22,\s*\.old_row = \{ \.name = "' + re.escape(name) + '"')
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        # (each row as one line, the comments after it left out)
        rows = {n: re.sub(r"\s+", " ", re.sub(r"/\*.*?\*/", "", b, flags=re.S)).strip()
                for n, b in re.findall(r'^\t\{ "([^"]+)",(.*?)(?=^\t\{ "|^\};)', table, re.M | re.S)}
        bases = {"AE BTB SLY": "AE TEAM SLY", "AE BTB CTF": "AE CTF", "AE BTB KING": "AE KING", "AE BTB BALL": "AE TEAM OB",
                 "AE BTB HVY": "AE TEAM SLY", "AE C2V2 TS": "AE COMP TS", "AE C2V2 CTF": "AE COMP CTF",
                 "AE C2V2 KOH": "AE COMP KOH", "AE C2V2 OB": "AE COMP OB"}
        def same(name, base, swaps):
            text = rows[base]
            for old, new in swaps:
                text = text.replace(old, new, 1)
            # (the rows wrap otherwise: compare without spaces)
            self.assertEqual(rows[name].replace(" ", ""), text.replace(" ", ""), name)
        # (revision 27: the BTB rows start with the EVO RIFLE, their casual bases do not)
        btb = [("ARENA_TIME_LIMIT", "ARENA_BTB_TIME_LIMIT"), ("ARENA_VEHICLES_STOCK", "ARENA_VEHICLES_DEFAULT"),
               ("_and_spawns_modern", "_and_spawns_modern_and_evo_start")]
        same("AE BTB SLY", "AE TEAM SLY", [("ARENA_TEAM_SLAYER_SCORE", "ARENA_BTB_SLAYER_SCORE")] + btb)
        same("AE BTB CTF", "AE CTF", btb)
        same("AE BTB KING", "AE KING", btb)
        # (AE TEAM OB has no vehicles: revision 23's VEHICLES DESTRUCTIBLE is the BTB one's alone)
        same("AE BTB BALL", "AE TEAM OB",
             btb + [("mouse_aim_assist_off", "mouse_aim_assist_off_and_vehicles_destructible_1")])
        same("AE BTB HVY", "AE TEAM SLY", [("ARENA_TEAM_SLAYER_SCORE", "ARENA_BTB_SLAYER_SCORE")] + btb +
             [("controller_crouch_stays_down", "controller_crouch_stays_down_and_heavy_vehicles")])
        same("AE C2V2 TS", "AE COMP TS", [("ARENA_TEAM_SLAYER_SCORE", "25")])
        for name in ("AE C2V2 CTF", "AE C2V2 KOH", "AE C2V2 OB"):
            same(name, bases[name], [])
        heavy = source[source.index("static void arena_gametype_no_speed_changes_and_sensor_range_10_and_controller_"
                                    "crouch_stays_down_and_heavy_vehicles(\n\tstruct game_variant *variant,\n"
                                    "\tstruct game_variant_options *options)\n{"):]
        heavy = heavy[:heavy.index("\n}\n")]
        for line in ("arena_gametype_no_speed_changes_and_sensor_range_10_and_controller_crouch_stays_down(variant, "
                     "options);", "options->vehicle_set[side] = VARIANT_VEHICLE_SET_CUSTOM;",
                     "options->vehicle_counts[side][_variant_vehicle_rocket_warthog] = MAXIMUM_VARIANT_VEHICLE_COUNT;",
                     "options->vehicle_counts[side][_variant_vehicle_scorpion] = MAXIMUM_VARIANT_VEHICLE_COUNT;"):
            self.assertIn(line, heavy)
        # (the battle rifle versions: twins in the self-check's table, ARENA_BR_START)
        twins = dict(re.findall(r'\{ "(AE BR [^"]+)", "([^"]+)" \},', source))
        self.assertEqual({n: twins[n] for n in BR_22_SEEDS}, {n: t for n, t in seed_rules.BR_TWINS_22.items()})
        for name in BR_22_SEEDS:
            self.assertIn("ARENA_BR_START", rows[name], name)
        golden = json.loads((HERE / "golden_seeds_rev22.json").read_text())
        golden_rev21 = json.loads((HERE / "golden_seeds_rev21.json").read_text())
        self.assertEqual(sorted(golden), sorted(list(golden_rev21) + list(REVISION_22_SEEDS)))
        # (revision 22's migration, MELEE UNZOOMS ON and MOUSE AIM ASSIST OFF, changed every AE seed of revision 21 in
        # place, in its folder: tools/ae_gametype_change.py's entry; the NHE set's files are revision 21's)
        migrated = sorted(re.findall(r'\.revision = 22,\s*\.old_row = \{ \.name = "([^"]+)"', migrations))
        self.assertEqual(len(migrated), 69)
        for name, value in golden_rev21.items():
            if name in migrated:
                self.assertNotEqual(golden[name]["block"], value["block"], name)
                self.assertEqual(golden[name]["folder"], value["folder"], name)
            else:
                self.assertEqual(golden[name], value, name)
        self.assertEqual(len({golden[n]["folder"] for n in golden}), len(golden))
        for name in ("AE BTB SLY", "AE BTB HVY", "AE C2V2 TS", "AE BR BTB S", "AE BR BTB H", "AE BR C2 TS"):
            self.assertEqual((golden[name]["slayer"]["no_death_bonus"], golden[name]["slayer"]["no_kill_penalty"]),
                             (1, 1), name)
        # (each new one other than its base's file, and each battle rifle version other than its twin's; but the COMP
        # 2V2 CTF, KING and ODDBALL play their AE COMP gametype's rules under their own name, as AE 2V2 CTF plays AE
        # CTF's: their content, which leaves the name out, is the same)
        same_rules = ("AE C2V2 CTF", "AE C2V2 KOH", "AE C2V2 OB")
        for name, base in list(bases.items()) + list(seed_rules.BR_TWINS_22.items()):
            if name in same_rules:
                self.assertEqual(golden[name]["content"], golden[base]["content"], name)
                self.assertNotEqual(golden[name]["file"], golden[base]["file"], name)
            else:
                self.assertNotEqual(golden[name]["content"], golden[base]["content"], name)
        self.assertEqual(golden["AE 2V2 CTF"]["content"], golden["AE CTF"]["content"])
        for name in REVISION_22_SEEDS:
            want, _, _ = seed_rules.ROWS[name]
            self.assertEqual(want["controller crouch"], "stays down", name)
        for name in BTB_SEEDS:
            want = seed_rules.ROWS[name][0]
            self.assertEqual((want["time limit"], want["vehicle set"], want["radar players"]),
                             ("20 min", "0", "enemies"), name)
        self.assertEqual(seed_rules.ROWS["AE BTB SLY"][0]["score to win"], "100")
        self.assertEqual(seed_rules.ROWS["AE BTB HVY"][0]["vehicle sets"], "default (red custom, blue custom)")
        self.assertEqual(seed_rules.ROWS["AE C2V2 TS"][0]["score to win"], "25")


class Debug(unittest.TestCase):
    def test_counts(self):
        d = harness.parse_debug(DEBUG_MP)
        self.assertIn("Arena Evolved", d["version"])
        self.assertEqual(d["network_version"], 20)
        self.assertTrue(d["joined"])
        self.assertEqual(len(d["delta"]), 1)
        self.assertEqual(d["tick"], 592)
        self.assertEqual(d["players"], 2)
        self.assertEqual(d["items"], 31)
        self.assertEqual(d["asserts"], 1)
        self.assertEqual(d["exceptions"], 0)
        self.assertEqual(d["lost_scripts"], 2)
        self.assertEqual(d["scenario_scripts"], 12)
        self.assertEqual(d["corrected"], 1)
        self.assertTrue(d["clean_exit"])
        self.assertEqual(d["final"][1], {"k": 1, "d": 3})

    def test_joined_lines(self):
        delta = "10.20.26 09:52:02  joining a Delta host of this machine's wire (network version 32; this machine's is 32)"
        self.assertIn("Delta host", harness.parse_debug(delta)["joined"])
        for refused in ("10.20.26 09:52:02  not joining a host of network version 29 (this machine's is 32)",
                        "10.20.26 09:52:02  not joining a Delta host of wire chupa-28a (this machine's is chupa-32a)"):
            self.assertIsNone(harness.parse_debug(refused)["joined"], refused)

    def test_verdicts(self):
        s = harness.parse_spec({"map": "bloodgulch"})
        clean = harness.parse_debug(DEBUG_MP.replace("EXCEPTION assert", "note").replace("2 scripts won't", "ok"))
        r = {"exit_code": 0, "timed_out": False, "debug_found": True, "debug": clean}
        self.assertEqual(harness.evaluate(s, r), ("PASS", []))
        r2 = dict(r, debug=harness.parse_debug(DEBUG_MP))
        status, why = harness.evaluate(s, r2)
        self.assertEqual(status, "FAIL")
        self.assertIn("1 asserts", why)
        self.assertIn("2 lost scripts", why)
        self.assertEqual(harness.evaluate(s, dict(r, timed_out=True))[0], "FAIL")
        low = dict(r, debug=dict(clean, tick=20))
        self.assertEqual(harness.evaluate(s, low)[0], "FAIL")
        camp = harness.parse_spec({"map": "levels\\a10\\a10"})
        self.assertEqual(harness.evaluate(camp, dict(r, debug=dict(clean, tick=None)))[0], "PASS")
        self.assertEqual(harness.evaluate(camp, dict(r, debug=dict(clean, tick=None, scenario_scripts=None)))[0], "FAIL")

    def test_cache_overflow_fails(self):
        """a game whose texture or sound cache overflowed fails (the engine's own lines, verbatim); the same log
        without them passes (the negative control), and a spec may allow them"""
        s = harness.parse_spec({"map": "bloodgulch"})
        clean_text = DEBUG_MP.replace("EXCEPTION assert", "note").replace("2 scripts won't", "ok")
        clean = harness.parse_debug(clean_text)
        self.assertEqual(clean["cache_overflows"], 0)
        r = {"exit_code": 0, "timed_out": False, "debug_found": True, "debug": clean}
        self.assertEqual(harness.evaluate(s, r), ("PASS", []))
        for line in ('10.12.26 01:02:03  YOU GOT STABBED!!!! double-click "GETSTABBED.BAT" on your PC now!!!',
                     '10.12.26 01:02:03  SOUND CACHE BLOWN!!!! double-click "GETSTABBED.BAT" on your PC now!!!'):
            bad = harness.parse_debug(clean_text + "\n" + line + "\n")
            self.assertEqual(bad["cache_overflows"], 1)
            status, why = harness.evaluate(s, dict(r, debug=bad))
            self.assertEqual(status, "FAIL")
            self.assertTrue(any("cache overflows" in w for w in why), why)
            self.assertIn("cache overflows 1", harness.one_line("case", dict(r, status=status, seconds=1, debug=bad)))
            allowed = harness.parse_spec({"map": "bloodgulch", "expect": {"cache_overflow_ok": True}})
            self.assertEqual(harness.evaluate(allowed, dict(r, debug=bad))[0], "PASS")
        both = harness.parse_debug(clean_text + "\nYOU GOT STABBED!!!!\nSOUND CACHE BLOWN!!!!\nYOU GOT STABBED!!!!\n")
        self.assertEqual(both["cache_overflows"], 3)
    def test_expect_lines(self):
        s = harness.parse_spec({"map": "bloodgulch", "expect": {"lines": [r"weapon pack: none, pack loadout: primary \w+"],
                                                                "no_lines": [r"weapon pack: refused"]}})
        clean = harness.parse_debug(DEBUG_MP.replace("EXCEPTION assert", "note").replace("2 scripts won't", "ok"))
        r = {"exit_code": 0, "timed_out": False, "debug_found": True, "debug": clean}
        text = "a\nAE rules: x, weapon pack: none, pack loadout: primary stock, secondary stock\nb\n"
        self.assertEqual(harness.evaluate(s, r, text), ("PASS", []))
        status, why = harness.evaluate(s, r, "nothing here\n")
        self.assertEqual(status, "FAIL")
        self.assertTrue(any("expected line" in w for w in why))
        status, why = harness.evaluate(s, r, text + "weapon pack: refused it\n")
        self.assertEqual(status, "FAIL")
        self.assertTrue(any("unexpected line" in w for w in why))
        # (rules asked for, no text to look in: a failure, not a pass)
        self.assertEqual(harness.evaluate(s, r)[0], "FAIL")
        # (no rules: the text is not needed)
        self.assertEqual(harness.evaluate(harness.parse_spec({"map": "bloodgulch"}), r), ("PASS", []))

    def test_one_line(self):
        r = {"status": "PASS", "exit_code": 0, "seconds": 35.2, "debug": harness.parse_debug(DEBUG_MP), "shots": ["a"]}
        line = harness.one_line("stock_bloodgulch", r)
        self.assertTrue(line.startswith("stock_bloodgulch: PASS, exit 0, 35 s, tick 592"))


class Smoke(unittest.TestCase):
    def test_set(self):
        names = [c["name"] for c in smoke.SMOKE]
        for n in ("stock_bloodgulch", "stock_damnation", "stock_beavercreek", "campaign_a10", "nhe_badcreek",
                  "nhe_bloodgulch", "ce_beavercreek_v2", "cex_menu", "cex_mp"):
            self.assertIn(n, names)
        for c in smoke.SMOKE:
            harness.parse_spec(dict(c, build="x"))
        self.assertEqual([c["name"] for c in smoke.cases("nhe_")], ["nhe_badcreek", "nhe_bloodgulch"])

    def test_worse_counts_are_not_pre_existing(self):
        d = harness.parse_debug(DEBUG_MP)
        base = {"status": "FAIL", "why": ["1 asserts"], "debug": dict(d, asserts=1)}
        mine = {"status": "FAIL", "why": ["9 asserts"], "debug": dict(d, asserts=9)}
        verdict, notes = smoke.compare(mine, base)
        self.assertEqual(verdict, "FAIL")
        self.assertIn("asserts worse than the baseline", notes)
        same = {"status": "FAIL", "why": ["1 asserts"], "debug": dict(d, asserts=1)}
        self.assertEqual(smoke.compare(same, base)[0], "PRE-EXISTING")

    def test_baseline_compare(self):
        d = harness.parse_debug(DEBUG_MP)
        mine = {"status": "FAIL", "why": ["1 asserts"], "debug": d}
        base = {"status": "FAIL", "why": ["3 asserts", "exit 1"], "debug": dict(d, items=25)}
        verdict, notes = smoke.compare(mine, base)
        self.assertEqual(verdict, "PRE-EXISTING")
        self.assertIn("items 25 -> 31", notes)
        self.assertEqual(smoke.compare(mine, {"status": "PASS", "why": [], "debug": d})[0], "FAIL")
        self.assertIn("stock_bloodgulch", smoke.table({"stock_bloodgulch": mine}, {"stock_bloodgulch": base}))


class Images(unittest.TestCase):
    def test_bmp_to_png_both_orders(self):
        px = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (10, 20, 30), (200, 100, 50), (1, 2, 3)]
        for top, bpp in ((True, 32), (False, 24)):
            with tempfile.TemporaryDirectory() as d:
                p = Path(d) / "frame00001.bmp"
                p.write_bytes(bmp(3, 2, px, top, bpp))
                w, h, rgb = harness.read_bmp(p.read_bytes())
                self.assertEqual((w, h), (3, 2))
                self.assertEqual(tuple(rgb[0:3]), (255, 0, 0))
                self.assertEqual(tuple(rgb[9:12]), (10, 20, 30))
                pngs = harness.convert_shots(d)
                self.assertEqual([x.name for x in pngs], ["frame00001.png"])
                self.assertFalse(p.exists())
                w2, h2, rgb2 = harness.read_png(pngs[0])
                self.assertEqual((w2, h2, bytes(rgb2)), (3, 2, bytes(rgb)))

    def test_without_pillow(self):
        saved = {k: sys.modules.get(k) for k in ("PIL", "PIL.Image")}
        sys.modules["PIL"] = None  # (import PIL raises ImportError: the standard-library paths)
        try:
            px = [(1, 2, 3), (4, 5, 6), (7, 8, 9), (250, 251, 252)]
            with tempfile.TemporaryDirectory() as d:
                p = Path(d) / "frame00002.bmp"
                p.write_bytes(bmp(2, 2, px))
                png = harness.bmp_to_png(p)
                self.assertEqual(bytes(harness.read_png(png)[2]), bytes(b for c in px for b in c))
        finally:
            for k, v in saved.items():
                if v is None:
                    sys.modules.pop(k, None)
                else:
                    sys.modules[k] = v

    def test_pick_frame_skips_black(self):
        with tempfile.TemporaryDirectory() as d:
            for i, c in enumerate([(128, 128, 128), (200, 200, 200), (0, 0, 0)]):
                Path(d, f"frame{i:05d}.png").write_bytes(harness.png_bytes(4, 4, bytes(c) * 16))
            self.assertEqual(harness.pick_frame(sorted(Path(d).glob("*.png"))).name, "frame00001.png")

    def test_views(self):
        self.assertEqual(len(harness.split_views(1920, 1080, 3)), 3)
        self.assertEqual(harness.split_views(1920, 1080, 2)[1], (0, 540, 1920, 540))

    def test_reticle_offset(self):
        w, h = 200, 200
        rgb = bytearray(bytes((90, 60, 40)) * (w * h))
        for y in range(118, 125):          # a 7x7 white square centred at (113, 121): offset (+13.5, +21.5)
            for x in range(110, 117):
                rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = b"\xff\xff\xff"
        for y in range(5, 45):              # a bigger white decoy far from the middle (a light) is not taken
            for x in range(55, 95):
                rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = b"\xff\xff\xff"
        v = harness.reticle_offsets(w, h, rgb, 1)[0]
        self.assertTrue(v["found"])
        self.assertEqual(v["offset"], [13.5, 21.5])
        self.assertFalse(harness.reticle_offsets(w, h, bytearray(bytes((0, 0, 0)) * (w * h)), 1)[0]["found"])


class Handshake(unittest.TestCase):
    def test_pair_specs(self):
        host, client = handshake.pair_specs("ae", "stock", 90, 12)
        he = harness.spec_env(host, "/r", "/s")
        ce = harness.spec_env(client, "/r", "/s")
        self.assertEqual(he["HALO_NETWORK_TEST"], "host:bloodgulch")
        self.assertEqual(ce["HALO_NETWORK_TEST"], "join")
        self.assertEqual((he["HALO_NET_ADDRESS"], he["HALO_NET_BROADCAST"]), ("127.0.0.200", "127.0.0.201"))
        self.assertEqual((ce["HALO_NET_ADDRESS"], ce["HALO_NET_BROADCAST"]), ("127.0.0.201", "127.0.0.200"))
        self.assertEqual(client["delay"], 12)
        self.assertEqual(client["exit_after"], 78)
        # (a mod's map and the host's saved gametype)
        host, client = handshake.pair_specs("ae", "old", 90, 12, map_name="bloodgulch", mod="NHE",
                                            saved_gametype="NHE 2V2 TS", save_from="~/roots/x")
        he = harness.spec_env(host, "/r", "/s")
        ce = harness.spec_env(client, "/r", "/s")
        self.assertEqual((he["HALO_MOD"], ce["HALO_MOD"]), ("NHE", "NHE"))
        self.assertEqual(he["HALO_NETWORK_TEST_GAMETYPE"], "NHE 2V2 TS")
        self.assertNotIn("HALO_NETWORK_TEST_GAMETYPE", ce)
        self.assertEqual((host["save_from"], client.get("save_from")), ("~/roots/x", None))
        # (a pair on different discs: the joining machine's own, '' the stock maps)
        host, client = handshake.pair_specs("ae", "ae", 90, 12, mod="NHE", client_mod="")
        he = harness.spec_env(host, "/r", "/s")
        ce = harness.spec_env(client, "/r", "/s")
        self.assertEqual((he["HALO_MOD"], ce["HALO_MOD"]), ("NHE", ""))
        # (both machines' packs/: a folder, or none at all)
        host, client = handshake.pair_specs("ae", "ae", 90, 12, packs_from="~/packs-x")
        self.assertEqual((host["packs_from"], client["packs_from"]), ("~/packs-x", "~/packs-x"))
        host, client = handshake.pair_specs("ae", "ae", 90, 12)
        self.assertNotIn("packs_from", host)
        self.assertNotIn("packs_from", client)

    def test_compare_and_judge(self):
        host = DEBUG_MP
        client = DEBUG_MP.replace("(21.5 11.4 -0.2)", "(21.6 11.4 -0.2)")
        tracks = harness.compare_tracks(host, client)
        self.assertEqual(sorted(tracks), [0, 1])
        self.assertAlmostEqual(tracks[0]["median_m"], 0.1, places=3)
        clean = harness.parse_debug(DEBUG_MP.replace("EXCEPTION assert", "note"))
        r = {"debug": clean}
        self.assertEqual(handshake.judge(r, r, tracks)[0], "PASS")
        self.assertEqual(handshake.judge(r, {"debug": dict(clean, joined=None)}, tracks)[0], "FAIL")

    def test_game_log_takes_stdout_when_debug_has_no_ticks(self):
        """stock OpenCE prints the network test's tick lines to stdout only"""
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            ticks = [l for l in DEBUG_MP.splitlines() if "network test: tick" in l]
            (folder / "debug.txt").write_text("\n".join(l for l in DEBUG_MP.splitlines() if l not in ticks))
            (folder / "stdout.log").write_text("\n".join(ticks))
            self.assertEqual(harness.parse_debug(handshake.game_log(folder))["tick"], 592)
            (folder / "debug.txt").write_text(DEBUG_MP)
            (folder / "stdout.log").write_text("arena-evolved: network test: tick 999 player 0: (1 2 3) h1 k0 d0 f0 |")
            self.assertEqual(harness.parse_debug(handshake.game_log(folder))["tick"], 592)

    def test_kd_at_a_common_tick(self):
        """kills/deaths compared at the lower of the two machines' last ticks: a kill the host logs after the
        client's last line is not a difference; one at or before it is"""
        line = "x: network test: tick {t} player 0: (1.0 2.0 3.0) s0 k{k} d0 f0 t0 m0 player 1: (4.0 5.0 6.0) s0 k0 d{k} f0 t1 m1 | items 1"
        host = "\n".join(line.format(t=t, k=k) for t, k in ((100, 0), (130, 0), (160, 1)))
        client = "\n".join(line.format(t=t, k=k) for t, k in ((101, 0), (131, 0)))
        tracks = harness.compare_tracks(host, client)
        self.assertEqual((tracks[0]["kd_tick"], tracks[0]["host_kd"], tracks[0]["client_kd"]), (131, [0, 0], [0, 0]))
        self.assertTrue(tracks[0]["kd_match"])
        # (a kill the client logs a few ticks before the host's next line: matched within the window)
        host = "\n".join(line.format(t=t, k=k) for t, k in ((100, 0), (130, 0), (160, 1), (190, 1)))
        client = "\n".join(line.format(t=t, k=k) for t, k in ((101, 0), (131, 0), (150, 1)))
        self.assertTrue(harness.compare_tracks(host, client)[0]["kd_match"])
        # (a difference that lasts is one)
        client = "\n".join(line.format(t=t, k=k) for t, k in ((101, 0), (131, 2), (161, 2), (189, 2)))
        self.assertFalse(harness.compare_tracks(host, client)[0]["kd_match"])


class SeedRules(unittest.TestCase):
    def test_table_and_parse(self):
        """seed_rules.py: the 115 seeds, and a run's rules lines read back field by field"""
        import seed_rules
        self.assertEqual(len(seed_rules.ROWS), 115)
        for name in seed_rules.ROWS:
            self.assertLessEqual(len(name), 11, name)
        text = ("x  game rules: health halo 2, fall damage off, starting equipment generic, vehicle set 1, "
                "time limit none, timers line of sight, training off, no spread first shot, pre-game countdown on, practice off; "
                "slayer (teams), score to win 50, respawn 150 ticks (5 s), suicide penalty 150 ticks (5 s), friendly fire on, "
                "radar players none, goal radar motion tracker, shields on, invisible off, infinite grenades off, "
                "weapon set normal, loadout pistol + assault rifle, no map weapons off, vehicle sets none (red none, blue none)\n"
                "x  AE rules: timers level line of sight, spawn heat on, objective normal, nhe extras off, "
                "drop secondary always, nhe mode by vehicles, starting frags 2, ball melee lethal\n")
        f = seed_rules.fields(text)
        self.assertEqual(f["ball melee"], "lethal")
        # (revision 7: LETHAL on the four AE oddball seeds, STOCK on every other; revision 11: and their AE BR twins)
        lethal = {"AE FFA BALL", "AE TEAM OB", "AE 2V2 BALL", "AE COMP OB", "AE BR FBALL", "AE BR TM OB", "AE BR 2V2 B",
                  "AE BR C OB", "AE BTB BALL", "AE BR BTB B", "AE C2V2 OB", "AE BR C2 OB"}
        for name, (want, _, _) in seed_rules.ROWS.items():
            self.assertEqual(want["ball melee"], "lethal" if name in lethal else "stock", name)
        self.assertEqual((f["engine"], f["respawn"], f["timers"], f["loadout"], f["drop secondary"]),
                         ("slayer (teams)", "150 ticks (5 s)", "line of sight", "pistol + assault rifle", "always"))
        # (the weapon pack's fields: every seed expects none / stock / stock / off but the AE BR set's (revision 11),
        # the pack's rifle in the primary slot, with the pistol second, and its twin's values otherwise)
        self.assertNotIn("weapon pack", f)
        f = seed_rules.fields(text.replace("ball melee lethal", "ball melee stock, weapon pack: battle rifle (slot 1), "
                                           "pack loadout: primary pack, secondary stock, battle rifle on map: on"))
        self.assertEqual((f["ball melee"], f["weapon pack"], f["pack loadout primary"], f["pack loadout secondary"],
                          f["battle rifle on map"]), ("stock", "battle rifle (slot 1)", "pack", "stock", "on"))
        for name, (want, spawn, frags) in seed_rules.ROWS.items():
            br = name.startswith("AE BR ")
            pack = br or name in ("AE OCT BR", "AE OCT BRSN", "AE OCT TBR", "AE OCT TBSN")
            evo = name in seed_rules.EVO_START_SEEDS
            self.assertEqual((want["pack loadout primary"], want["pack loadout secondary"], want["battle rifle on map"]),
                             ("pack" if pack or evo else "stock", "stock", "off"), name)
            # (revision 25: the five packs named where GAME OPTIONS can start them; revision 27: the EVO rifle joins)
            self.assertIn(want["weapon pack"], ("none", "battle rifle (slot 1)", "evo_rifle (slot 7)", seed_rules.PACKS_NAMED,
                                                seed_rules.PACKS_NAMED.replace(" (slots 1, 2, 3, 4, 5)",
                                                                               " + evo_rifle (slots 1, 2, 3, 4, 5, 7)")), name)
            if evo:
                self.assertIn("evo_rifle", want["weapon pack"], name)
            elif want["weapon pack"] == "none":
                self.assertFalse(pack, name)
            if br:
                twin_want, _, twin_frags = seed_rules.ROWS[seed_rules.BR_TWINS[name]]
                # (revision 13: the battle rifle and an assault rifle)
                self.assertEqual((want["loadout"], spawn, frags), ("assault rifle + assault rifle",
                                                                   "battle_rifle + assault rifle", twin_frags), name)
                self.assertIn("battle_rifle", want["weapon pack"].replace("battle rifle", "battle_rifle"), name)
                differ = {k for k in set(want) | set(twin_want) if want.get(k) != twin_want.get(k)}
                self.assertLessEqual(differ, {"loadout", "weapon pack", "pack loadout primary"}, name)
        # (revision 27, owner 2026-10-30: SPAWNS MODERN in every AE gametype, the NHE set CLASSIC; the EVO rifle start in
        # AE BTB and AE COMP, 14 seeds, an assault rifle second and their AE BR versions keeping the battle rifle)
        self.assertEqual(sum(want["spawns"] == "classic" for want, _, _ in seed_rules.ROWS.values()), 25)
        self.assertEqual(sum(want["spawns"] == "modern" for want, _, _ in seed_rules.ROWS.values()), 90)
        self.assertEqual(len(seed_rules.EVO_START_SEEDS), 14)
        for name in seed_rules.EVO_START_SEEDS:
            want, spawn, _ = seed_rules.ROWS[name]
            self.assertEqual((want["loadout"], spawn), ("assault rifle + assault rifle", "dmr + assault rifle"), name)
        self.assertEqual(seed_rules.fields(text + "x  AE spawns: modern\n")["spawns"], "modern")
        self.assertEqual(seed_rules.fields(text.replace("ball melee lethal", "ball melee stock, weapon pack: "
                                                        + seed_rules.PACKS_NAMED + ", pack loadout: primary stock, secondary stock, "
                                                        "battle rifle on map: off"))["weapon pack"], seed_rules.PACKS_NAMED)
        self.assertEqual(len(seed_rules.BR_TWINS), 34)
        # (revision 12: PLAYERS ON RADAR ENEMIES on the AE team seeds with players on the motion sensor and their
        # AE BR twins, and revision 17's team drills, nowhere else; the free for all ones and the NHE set keep theirs)
        enemies = {name for name, (want, _, _) in seed_rules.ROWS.items() if want["radar players"] == "enemies"}
        # (and revision 22's BTB set and its battle rifle versions, from the AE team gametypes)
        btb = set(BTB_SEEDS) | {br for br, twin in seed_rules.BR_TWINS_22.items() if twin in BTB_SEEDS}
        # (and revision 24's AE BOMB, AE CTF's rules, and revision 26's AE SND, AE BOMB's)
        self.assertEqual(enemies, set(seed_rules.RADAR_ENEMIES) | set(TEAM_OCTAGON_SEEDS) | btb |
                         {"AE BOMB", "AE SND"} |
                         {br for br, twin in seed_rules.BR_TWINS.items() if twin in seed_rules.RADAR_ENEMIES})
        self.assertEqual(len(enemies), 35)
        for name in enemies:
            self.assertIn("(teams)", seed_rules.ROWS[name][0]["engine"], name)
        # (revision 14: every slayer seed without the death bonus and the kill penalty, 38 of them, 46 with revisions
        # 16 and 17's OCTAGON drills, 52 with revision 22's; kill in order only in the Phantoms ones)
        slayers = {name: want for name, (want, _, _) in seed_rules.ROWS.items() if want["engine"].startswith("slayer")}
        self.assertEqual(len(slayers), 52)
        for name, want in slayers.items():
            self.assertEqual((want["death bonus"], want["kill penalty"]), ("off", "off"), name)
        self.assertEqual(sorted(n for n, want in slayers.items() if want["kill in order"] == "on"),
                         ["AE BR PHANT", "AE PHANTOMS"])

    def test_grifball_and_bomb(self):
        """revision 24 (owner, 2026-10-23): AE GRIFBALL and AE BOMB, the Bomb game type on AE's rules, added seeds (no
        migration) in the builds with Bomb (HALO_GAME_BROWSER: their rows inside its #ifdef). GRIFBALL is Grifball's
        builder (melee only: no custom loadout, the PC options' default none + none) with AE's rules; BOMB is Bomb's
        builder on AE CTF's casual team row, no vehicles. golden_seeds_rev24.json is a fresh revision 24 root:
        revision 23's 112 gametypes in their folders and the two new ones (a revision 23 root started on revision 24
        holds the same files); seed_rules expects them; no battle rifle versions. (Since cb1e7b1ce the revision 24
        snapshot holds the blocks the v0.3.9.1 release writes: every block changed after the revision 23 snapshot
        without a revision, so only the folders are compared)"""
        import re
        import seed_rules
        source = (HERE.parent.parent / "source" / "saved games" / "arena_gametypes.c").read_text(encoding="latin-1")
        self.assertGreaterEqual(int(re.search(r"ARENA_GAMETYPES_REVISION = (\d+),", source)[1]), 24)
        migrations = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        self.assertNotIn(".revision = 24,", migrations)
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        guarded = table[table.index("#ifdef HALO_GAME_BROWSER"):]
        guarded = guarded[:guarded.index("#endif")]
        rows = {n: re.sub(r"\s+", " ", re.sub(r"/\*.*?\*/", "", b, flags=re.S)).strip()
                for n, b in re.findall(r'^\t\{ "([^"]+)",(.*?)(?=^\t\{ "|^\};|^#)', table, re.M | re.S)}
        # (and revision 26's AE SND after them: test_search_and_destroy)
        self.assertEqual(re.findall(r'^\t\{ "([^"]+)"', guarded, re.M)[:2], list(REVISION_24_SEEDS))
        self.assertIn("build_game_variant_grifball, ARENA_GAMETYPE_FLAGS, 0, 0,", rows["AE GRIFBALL"])
        self.assertIn("ARENA_VEHICLES_STOCK, FALSE,", rows["AE GRIFBALL"])
        self.assertIn("build_game_variant_bomb, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS, 0, ARENA_TIME_LIMIT,",
                      rows["AE BOMB"])
        # (AE CTF's row past its builder and score, without VEHICLES DESTRUCTIBLE: Bomb has no vehicles)
        ctf = rows["AE CTF"]
        tail = lambda s: s[s.index("ARENA_RADAR_ENEMIES"):]
        self.assertEqual(tail(rows["AE BOMB"]).replace(" ", ""),
                         tail(ctf).replace("_and_vehicles_destructible_1", "").replace(" ", ""))
        twins = dict(re.findall(r'\{ "(AE BR [^"]+)", "([^"]+)" \},', source))
        for name in REVISION_24_SEEDS:
            self.assertNotIn(name, twins.values())
        golden = json.loads((HERE / "golden_seeds_rev24.json").read_text())
        golden_rev23 = json.loads((HERE / "golden_seeds_rev23.json").read_text())
        self.assertEqual(sorted(golden), sorted(list(golden_rev23) + list(REVISION_24_SEEDS)))
        for name, value in golden_rev23.items():
            self.assertEqual(golden[name]["folder"], value["folder"], name)
        self.assertEqual(len({golden[n]["folder"] for n in golden}), len(golden))
        bomb, grifball = seed_rules.ROWS["AE BOMB"][0], seed_rules.ROWS["AE GRIFBALL"][0]
        self.assertEqual((bomb["engine"], bomb["radar players"], bomb["loadout"], bomb["vehicles destructible"]),
                         ("terminator (teams)", "enemies", "pistol + assault rifle", "off"))
        self.assertEqual((grifball["engine"], grifball["shields"], grifball["loadout"], grifball["no map weapons"],
                          grifball["weapon set"], grifball["controller crouch"], grifball["melee unzooms"]),
                         ("terminator (teams)", "off", "none + none", "on", "no grenades", "stays down", "on"))

    def test_search_and_destroy(self):
        """revision 26 (owner, 2026-10-26: "just make it a gametype"): AE SND, an added seed (no migration) in the builds
        with Bomb: AE BOMB's row with the Bomb variant's SEARCH AND DESTROY bit (ae_bomb_snd_rules.h, flags bit 7, set
        by its own adjust after AE BOMB's), the score to win 4 and no game time limit. golden_seeds_rev26.json is a
        fresh revision 26 root: revision 25's 114 files as they were, in their folders, and the new one (a revision 25
        root started on revision 26 holds the same files); seed_rules expects it; no battle rifle version; it sits
        after AE BOMB in AE ACTION SACK"""
        import re
        import seed_rules
        root = HERE.parent.parent
        source = (root / "source" / "saved games" / "arena_gametypes.c").read_text(encoding="latin-1")
        self.assertGreaterEqual(int(re.search(r"ARENA_GAMETYPES_REVISION = (\d+),", source)[1]), 26)
        migrations = source[source.index("arena_gametype_migrations[] ="):source.index("arena_gametype_test_migration =")]
        self.assertNotIn(".revision = 26,", migrations)
        table = source[source.index("arena_gametypes[] ="):source.index("arena_gametype_migrations[] =")]
        guarded = table[table.index("#ifdef HALO_GAME_BROWSER"):]
        guarded = guarded[:guarded.index("#endif")]
        self.assertEqual(re.findall(r'^\t\{ "([^"]+)"', guarded, re.M), list(REVISION_24_SEEDS + REVISION_26_SEEDS))
        rows = {n: re.sub(r"\s+", " ", re.sub(r"/\*.*?\*/", "", b, flags=re.S)).strip()
                for n, b in re.findall(r'^\t\{ "([^"]+)",(.*?)(?=^\t\{ "|^\};|^#)', table, re.M | re.S)}
        bomb, snd = rows["AE BOMB"], rows["AE SND"]
        self.assertIn("build_game_variant_bomb, ARENA_GAMETYPE_FLAGS | ARENA_CASUAL_FLAGS, 4, 0,", snd)
        # (revision 27's SPAWNS MODERN comes after both, in its own function)
        adjust = lambda row: re.search(r"\.adjust = (\w+)", row)[1].removesuffix("_and_spawns_modern")
        self.assertEqual(adjust(snd), adjust(bomb) + "_and_search_and_destroy")
        # (past the score and the time, AE BOMB's row)
        tail = lambda s: s[s.index("ARENA_STOCK"):s.index(".adjust")].replace(" ", "")
        self.assertEqual(tail(snd), tail(bomb))
        body = re.search(r"^static void %s\(\n[^;{]*\)\n\{\n(.*?)^\}" % adjust(snd), source, re.M | re.S)[1]
        self.assertIn(adjust(bomb) + "(variant, options);", body)
        self.assertIn("arena_gametype_search_and_destroy(variant, options);", body)
        own = re.search(r"^static void arena_gametype_search_and_destroy\(\n[^;{]*\)\n\{\n(.*?)^\}", source, re.M | re.S)[1]
        self.assertIn("ae_bomb_snd_flag_set(variant->game_engine_variant.bomb.flags, 1)", own)
        rules = (root / "port" / "linux" / "game" / "ae_bomb_snd_rules.h").read_text()
        self.assertIn("#define AE_BOMB_SND_FLAG 0x80u", rules)
        # (the variant's cleanup keeps the bit, and BOMB DROP's beside it; the rule reads it)
        engine = (root / "source" / "game" / "game_engine.c").read_text(encoding="latin-1")
        self.assertIn("FLAG(_bomb_melee_only_bit) | AE_BOMB_SND_FLAG | AE_BOMB_SND_DROP_FLAG; /* AE hook */", engine)
        assault = (root / "port" / "linux" / "game" / "ae_bomb_assault.c").read_text()
        self.assertIn("ae_bomb_snd_flag(game_engine_get_variant()->game_engine_variant.bomb.flags)", assault)
        twins = dict(re.findall(r'\{ "(AE BR [^"]+)", "([^"]+)" \},', source))
        self.assertNotIn("AE SND", twins.values())
        names = (root / "source" / "saved games" / "arena_gametype_names.c").read_text(encoding="latin-1")
        self.assertIn('{ "AE SND", "TEAM AE SEARCH AND DESTROY", _arena_gametype_set_ae, game_engine_bomb, TRUE, FALSE,',
                      names)
        # (0.4.2: the AE OBJECTIVE folder, AE SND first)
        self.assertIn('{ _arena_gametype_category_objective, "AE SND" }, '
                      '{ _arena_gametype_category_objective, "AE BOMB" },', names)
        golden = json.loads((HERE / "golden_seeds_rev26.json").read_text())
        golden_rev25 = json.loads((HERE / "golden_seeds_rev25.json").read_text())
        self.assertEqual(sorted(golden), sorted(list(golden_rev25) + list(REVISION_26_SEEDS)))
        # (the capture the flag rows' blocks since: OpenCE's Kills Score, the 0.5.0 merge, the snapshot refreshed to them)
        for name, value in golden_rev25.items():
            self.assertEqual(golden[name]["folder"], value["folder"], name)
            if name not in seed_rules.ROWS or not seed_rules.ROWS[name][0]["engine"].startswith("ctf"):
                self.assertEqual(golden[name], value, name)
        self.assertEqual(len({golden[n]["folder"] for n in golden}), len(golden))
        want = seed_rules.ROWS["AE SND"][0]
        self.assertEqual((want["engine"], want["score to win"], want["time limit"], want["radar players"]),
                         ("terminator (teams)", "4", "none", "enemies"))
        self.assertEqual(dict(want, **{"score to win": "3", "time limit": "15 min"}), seed_rules.ROWS["AE BOMB"][0])


class Windows(unittest.TestCase):
    def test_start_outcomes(self):
        self.assertEqual(windows.parse_start(10, "TEST NOT STARTED: not idle (gpu 40%)\n"),
                         ("not-started", "not idle (gpu 40%)"))
        done = 'TEST STARTED: r1\n{\n "status": "done", "reason": "exited", "exit_code": null\n}\n'
        self.assertEqual(windows.parse_start(0, done), ("done", "exited"))
        aborted = 'TEST STARTED: r1\n{"status": "aborted", "reason": "input"}\n'
        self.assertEqual(windows.parse_start(11, aborted), ("aborted", "input"))

    def test_request_and_prepare(self):
        a = windows.argparse.Namespace(window="1280x720", exit_after=60, shots=900, map=None, env=["X=1"], init=None)
        env = windows.game_env("C:\\halo-test", "ae-1", a)
        self.assertEqual(env["HALO_DISPLAY_MODE"], "windowed")
        self.assertEqual(env["HALO_DATA_ROOT"], "C:\\halo-test\\runs\\ae-1\\data")
        self.assertEqual(env["X"], "1")
        req = windows.make_request("ae-1", "C:\\halo-test\\builds\\b\\halo.exe", "C:\\halo-test\\builds\\b", env)
        self.assertEqual(req["run_id"], "ae-1")
        script = windows.prepare_script("C:\\halo-test", "ae-1", windows.init_lines(a), req)
        self.assertIn("New-Item -ItemType Junction -Path 'C:\\halo-test\\runs\\ae-1\\data\\maps' "
                      "-Target 'C:\\halo-test\\maps'", script)
        self.assertIn("levels\\test\\bloodgulch\\bloodgulch", script)
        self.assertNotIn("schtasks", script)
        for line in script.splitlines():
            for word in ("Remove-Item", "New-Item", "Set-Content"):
                if word in line:
                    self.assertIn("C:\\halo-test", line)
        self.assertIn("C:\\halo-test\\saves\\ae-1", windows.cleanup_script("C:\\halo-test", "ae-1"))

    def _ns(self, **kw):
        base = dict(window="1280x720", exit_after=60, shots=900, map=None, env=None, init=None,
                    isolation=None, network=False, gpu=None, save_from=None, keep_save=False)
        base.update(kw)
        return windows.argparse.Namespace(**base)

    def test_menu_start_has_no_init(self):
        self.assertEqual(windows.init_lines(self._ns(menu=True)), [])
        self.assertTrue(windows.init_lines(self._ns()))

    def test_sandbox_fields_in_request(self):
        root, exe = "C:\\halo-test", "C:\\halo-test\\branches\\abc\\halo.exe"
        req = windows.make_request("r", exe, "C:\\halo-test\\branches\\abc", {}, isolation="sandbox",
                                   network=True, gpu="software")
        self.assertEqual((req["isolation"], req["network"], req["gpu"]), ("sandbox", True, "software"))
        plain = windows.make_request("r", exe, "w", {})
        for k in ("isolation", "network", "gpu"):
            self.assertNotIn(k, plain)
        self.assertEqual(windows.check_isolation(self._ns(isolation="sandbox", network=True)), "sandbox")
        for bad in (dict(network=True), dict(gpu="software"), dict(isolation="vm"),
                    dict(isolation="sandbox", gpu="hardware")):
            with self.assertRaises(SystemExit, msg=str(bad)):
                windows.check_isolation(self._ns(**bad))

    def test_branch_build_and_exe(self):
        root = "C:\\halo-test"
        self.assertEqual(windows.branch_build_exe(root, "0233e03e1234"), "C:\\halo-test\\branches\\0233e03e1234")
        for bad in ("..\\x", "a\\b", "C:\\x", "", "..", "a/../b"):
            with self.assertRaises(SystemExit, msg=bad):
                windows.branch_build_exe(root, bad)
        old = "C:\\halo-test\\builds\\v0.1.0-beta\\halo.exe"
        self.assertEqual(windows.check_exe(root, old, "host"), old)
        self.assertEqual(windows.check_exe(root, "C:/halo-test/branches/x/halo.exe", None),
                         "C:\\halo-test\\branches\\x\\halo.exe")
        u = "C:\\halo-test\\untrusted\\f\\halo.exe"
        self.assertEqual(windows.check_exe(root, u, "sandbox"), u)
        for bad, iso in ((u, "host"), ("C:\\Windows\\halo.exe", "host"), ("C:\\halo-test\\builds\\x\\calc.exe", "host"),
                         ("C:\\halo-test\\builds\\..\\..\\x\\halo.exe", "host"), ("halo.exe", "host")):
            with self.assertRaises(SystemExit, msg=bad):
                windows.check_exe(root, bad, iso)

    def test_save_from_places_the_save_in_the_run(self):
        import tempfile
        a = self._ns(save_from="x")
        env = windows.game_env("C:\\halo-test", "ae-1", a)
        self.assertEqual(env["HALO_SAVE_ROOT"], "C:\\halo-test\\runs\\ae-1\\save")
        self.assertEqual(windows.game_env("C:\\halo-test", "ae-1", self._ns())["HALO_SAVE_ROOT"],
                         "C:\\halo-test\\saves\\ae-1")
        self.assertEqual(windows.game_env("C:\\halo-test", "ae-1", self._ns(keep_save=True))["HALO_SAVE_ROOT"],
                         "C:\\halo-test\\runs\\ae-1\\save")
        req = windows.make_request("ae-1", "C:\\halo-test\\builds\\b\\halo.exe", "C:\\halo-test\\builds\\b", env)
        script = windows.prepare_script("C:\\halo-test", "ae-1", [], req, save_in_run=True)
        self.assertNotIn("C:\\halo-test\\saves", script)
        self.assertIn("C:\\halo-test\\saves\\ae-1", windows.prepare_script("C:\\halo-test", "ae-1", [], req))
        self.assertIn("C:\\halo-test\\runs\\ae-1\\save", windows.cleanup_script("C:\\halo-test", "ae-1"))
        with tempfile.TemporaryDirectory(dir=str(Path(__file__).resolve().parent)) as d:
            self.assertEqual(windows.check_save_from(d), Path(d).resolve())
        for bad in ("/nonexistent-folder", "/tmp"):
            with self.assertRaises(SystemExit, msg=bad):
                windows.check_save_from(bad)

    def test_encoded_command(self):
        import base64
        self.assertEqual(base64.b64decode(windows.ps_encode("Write-Output 'a'")).decode("utf-16-le"),
                         "Write-Output 'a'")


class WindowsPaths(unittest.TestCase):
    def test_root_must_be_halo_test(self):
        self.assertEqual(windows.check_root("c:\\HALO-TEST\\"), "C:\\halo-test")
        for bad in ("C:\\", "C:\\halo-test\\..\\Windows", "D:\\halo-test", "C:\\halo-test2", "\\\\pc\\share"):
            with self.assertRaises(SystemExit, msg=bad):
                windows.check_root(bad)

    def test_inside(self):
        self.assertTrue(windows.inside("C:\\halo-test\\builds\\abc\\halo.exe", "C:\\halo-test\\builds"))
        self.assertTrue(windows.inside("c:/HALO-TEST/builds/abc", "C:\\halo-test\\builds"))
        self.assertFalse(windows.inside("C:\\halo-test\\builds\\..\\..\\Windows", "C:\\halo-test\\builds"))
        self.assertFalse(windows.inside("C:\\halo-test\\builds2\\x", "C:\\halo-test\\builds"))
        self.assertFalse(windows.inside("builds\\x", "C:\\halo-test"))

    def test_build_dir(self):
        self.assertEqual(windows.build_dir_path("C:\\halo-test", "86ac29d065e7"), "C:\\halo-test\\builds\\86ac29d065e7")
        for bad in ("..\\..\\Windows", "C:\\Windows", "\\Windows", "a\\..\\..", "../x", "D:x", ""):
            with self.assertRaises(SystemExit, msg=bad):
                windows.build_dir_path("C:\\halo-test", bad)

    def test_env_overrides(self):
        root, rid = "C:\\halo-test", "ae-1"
        self.assertEqual(windows.check_env_overrides(root, rid, ["HALO_MATCH_CLOCK=both", "HALO_NETWORK_TEST=host:x"]),
                         {"HALO_MATCH_CLOCK": "both", "HALO_NETWORK_TEST": "host:x"})
        ok = windows.check_env_overrides(root, rid, ["HALO_NET_BROKERS_FILE=C:\\halo-test\\runs\\ae-1\\b.txt"])
        self.assertEqual(len(ok), 1)
        for bad in ("HALO_DATA_ROOT=C:\\halo-test\\runs\\ae-1\\data", "halo_save_root=x",
                    "HALO_SCREENSHOT_DIR=C:\\halo-test\\runs\\ae-1\\s", "HALO_NET_BROKERS_FILE=C:\\Users\\x.txt",
                    "HALO_X=C:\\Windows\\y", "HALO_LOG_FILE=b.txt", "HALO_X=..\\..\\y", "noequals"):
            with self.assertRaises(SystemExit, msg=bad):
                windows.check_env_overrides(root, rid, [bad])


class Record(unittest.TestCase):
    def test_noop_without_support(self):
        with tempfile.TemporaryDirectory() as d:
            b = Path(d) / "halo"
            b.write_bytes(b"\0" * 100)
            self.assertIn("skipped", record.record_env(b, 20, Path(d) / "v")["status"])
            b.write_bytes(b"\0" * 100 + b"HALO_RECORD_SECONDS" + b"\0")
            self.assertIn("no ffmpeg", record.record_env(b, 20, Path(d) / "v", which=lambda _: None)["status"])
            r = record.record_env(b, 20, Path(d) / "v", which=lambda _: "/usr/bin/ffmpeg")
            self.assertEqual(r["env"]["HALO_RECORD_SECONDS"], "20")
            self.assertEqual(record.record_env(b, 0, Path(d) / "v")["env"], {})


class Recordings(unittest.TestCase):
    def test_result_and_line_list_the_clips(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d)
            (out / "video").mkdir(parents=True)  # (the game writes <HALO_RECORD_DIR>/<name>.mp4 and, until then,
            (out / "video" / "clip.mp4").write_bytes(b"x")  # <name>.video.mp4.part and <name>.audio.f32.part)
            (out / "video" / "clip2.video.mp4.part").write_bytes(b"x")
            (out / "video" / "clip2.audio.f32.part").write_bytes(b"x")
            self.assertEqual([p.name for p in harness.find_recordings(out / "video")], ["clip.mp4"])
            self.assertEqual(harness.find_recordings(out / "none"), [])
            work = out / "work"
            (work / "root").mkdir(parents=True)
            (work / "save").mkdir()
            clean = DEBUG_MP.replace("EXCEPTION assert", "note").replace("2 scripts won't", "ok")
            (work / "root" / "debug.txt").write_text(clean)
            spec = harness.parse_spec({"name": "g", "map": "bloodgulch", "record": 10})
            prepared = {"root": str(work / "root"), "save": str(work / "save"), "cwd": str(work / "bin"),
                        "shots": str(out / "shots"), "env": {}, "record": {"status": "recording 10 s"}}
            r = harness.collect_game(spec, prepared, {"exit_code": 0, "timed_out": False, "seconds": 40}, out)
            self.assertEqual(r["recordings"], ["video/clip.mp4"])
            self.assertEqual(r["status"], "PASS")
            self.assertEqual(json.loads((out / "result.json").read_text())["recordings"], ["video/clip.mp4"])
            self.assertIn("1 mp4 (video/clip.mp4)", harness.one_line("g", r))
            # (asked for, none made: a FAIL with its reason)
            (out / "video" / "clip.mp4").unlink()
            (work / "root").mkdir(parents=True, exist_ok=True)
            (work / "save").mkdir(exist_ok=True)
            (work / "root" / "debug.txt").write_text(clean)
            r2 = harness.collect_game(spec, prepared, {"exit_code": 0, "timed_out": False, "seconds": 40}, out)
            self.assertEqual(r2["status"], "FAIL")
            self.assertIn("recording asked, no mp4", r2["why"])
            self.assertIn("record: skipped (x)", harness.one_line("g", dict(r, recordings=[], record="skipped (x)")))

    def test_capture_counts_and_part_files(self):
        d = harness.parse_debug("chupa: capture: screenshot /s/frame00100.png skipped (1 so far): the screenshot "
                                "thread is behind\nchupa: capture: screenshot /s/frame00200.png skipped (3 so far): x\n"
                                "chupa: capture: stopped after 10.03 s: 602 frames due, 600 reads, 2 dropped, 0 reads\n")
        self.assertEqual((d["shots_skipped"], d["record_dropped"]), (3, 2))
        line = harness.one_line("g", {"status": "PASS", "exit_code": 0, "seconds": 1, "debug": d,
                                      "recordings": ["video/a.mp4"]})
        self.assertIn("3 screenshots skipped", line)
        self.assertIn("2 frames dropped", line)
        with tempfile.TemporaryDirectory() as t:
            Path(t, "frame00001.png").write_bytes(harness.png_bytes(1, 1, b"abc"))
            Path(t, "frame00002.png.part").write_bytes(b"half")
            self.assertEqual([p.name for p in harness.convert_shots(t)], ["frame00001.png"])
            self.assertFalse(Path(t, "frame00002.png.part").exists())

    def test_out_dir_is_absolute(self):
        cwd = os.getcwd()
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            os.chdir(d)
            try:
                out = harness.new_out_dir(dict(harness.DEFAULTS, out_dir="rel-out"), "run", None, "n")
            finally:
                os.chdir(cwd)
            self.assertTrue(out.is_absolute())
            self.assertEqual(out, Path(d).resolve() / "rel-out" / "run" / "n")

    def test_png_screenshots_when_the_game_has_them(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            d = Path(d)
            data = d / "data"
            (data / "maps").mkdir(parents=True)
            cfg = dict(harness.DEFAULTS, data_dir=str(data))
            spec = harness.parse_spec({"name": "g", "map": "bloodgulch", "screenshots": 100})
            for name, blob, want in (("new", b"..HALO_SCREENSHOT_FORMAT..", "png"), ("old", b"stock build", None)):
                b = d / name
                b.mkdir()
                (b / "halo").write_bytes(blob)
                build = {"dir": str(b), "binary": str(b / "halo"), "env": {}}
                prep = harness.prepare_game(cfg, spec, build, d / "work" / name, d / "out" / name)
                self.assertEqual(prep["env"].get("HALO_SCREENSHOT_FORMAT"), want, name)


class Inner(unittest.TestCase):
    """the inner runner with a fake Xvfb and fake games (no X, no game; geteuid stubbed, so even a root run never
    touches the real loopback or mounts)"""

    NOT_ROOT = staticmethod(lambda: 1000)

    def fake(self, d, name, body):
        f = Path(d) / name
        f.write_text("#!" + sys.executable + "\n" + body)
        f.chmod(0o755)
        return str(f)

    # (a fake Xvfb: notes its pid, says display 57 on the -displayfd fd, then does `tail`)
    def xvfb_body(self, tail, say=True):
        return ("import os, sys, time, pathlib\n"
                "pathlib.Path(__file__).with_name('xvfb-%d.pid' % os.getpid()).write_text('x')\n"
                "fd = int(sys.argv[sys.argv.index('-displayfd') + 1])\n" +
                ("os.write(fd, b'57\\n')\n" if say else "") + tail)

    def game(self, d, name, code=0, sleep=0.0, command=None):
        cwd = Path(d) / name / "bin"
        cwd.mkdir(parents=True)
        self.fake(cwd, "halo", f"import os, sys, time, pathlib\npathlib.Path('started').write_text(str(os.getpid()))\n"
                  f"time.sleep({sleep})\nprint('DISPLAY=' + os.environ.get('DISPLAY', ''))\nsys.exit({code})\n")
        g = {"name": name, "cwd": str(cwd), "env": {}, "delay": 0, "timeout": 30, "screen": "640x480",
             "address": "127.0.0.200"}
        if command:
            g["command"] = command
        return g

    def plan(self, d, xvfb_body, games):
        xvfb = self.fake(d, "Xvfb", xvfb_body)
        plan = {"games": games, "bots": None, "xvfb": True, "xvfb_bin": xvfb, "addresses": ["127.0.0.200"]}
        (Path(d) / "plan.json").write_text(json.dumps(plan))
        return Path(d) / "plan.json", Path(d) / "res.json"

    def left_running(self, d, wait=5):
        """processes still running from the fake scripts in d (their path is in their command line)"""
        t0 = time.time()
        while True:
            left = []
            for proc in Path("/proc").iterdir():
                if not proc.name.isdigit() or int(proc.name) == os.getpid():
                    continue
                try:
                    cmd = (proc / "cmdline").read_bytes()
                    state = (proc / "stat").read_text().rsplit(")", 1)[1].split()[0]
                except OSError:
                    continue
                if d.encode() in cmd and state != "Z":
                    left.append(int(proc.name))
            if not left or time.time() - t0 > wait:
                return left
            time.sleep(0.1)

    def xvfb_pids(self, d):
        return [int(p.name[5:-4]) for p in Path(d).glob("xvfb-*.pid")]

    def test_x_server_gone_first_is_not_a_failure(self):
        # (the flake: the X server is gone before the cleanup; xvfb-run then returned 1 for a clean game)
        with tempfile.TemporaryDirectory() as d:
            plan, res = self.plan(d, self.xvfb_body("sys.exit(0)\n"), [self.game(d, "g", 0, 0.5)])
            harness._inner(str(plan), str(res), geteuid=self.NOT_ROOT)
            r = json.loads(res.read_text())[0]
            self.assertEqual((r["exit_code"], r["display"]), (0, ":57"))
            self.assertIn("DISPLAY=:57", (Path(d) / "g" / "stdout.log").read_text())

    def test_game_exit_code_is_the_games(self):
        with tempfile.TemporaryDirectory() as d:
            plan, res = self.plan(d, self.xvfb_body("time.sleep(60)\n"), [self.game(d, "g", 3)])
            t0 = time.time()
            harness._inner(str(plan), str(res), geteuid=self.NOT_ROOT)
            self.assertEqual(json.loads(res.read_text())[0]["exit_code"], 3)
            self.assertLess(time.time() - t0, 20)  # (the X server was stopped, not waited for)
            self.assertEqual(self.left_running(d), [])

    def test_no_x_server_no_game(self):
        with tempfile.TemporaryDirectory() as d:
            saved = os.environ.get("DISPLAY")
            os.environ["DISPLAY"] = ":0"  # (the caller's desktop: must never reach a game)
            try:
                plan, res = self.plan(d, "import sys\nsys.exit(1)\n", [self.game(d, "g")])
                harness._inner(str(plan), str(res), geteuid=self.NOT_ROOT)
            finally:
                if saved is None:
                    os.environ.pop("DISPLAY", None)
                else:
                    os.environ["DISPLAY"] = saved
            r = json.loads(res.read_text())[0]
            self.assertIsNone(r["exit_code"])
            self.assertIn("no X server", r["error"])
            self.assertFalse((Path(d) / "g" / "bin" / "started").exists())
            spec = harness.parse_spec({"name": "g", "map": "bloodgulch"})
            status, why = harness.evaluate(spec, {"exit_code": None, "error": r["error"], "debug_found": False})
            self.assertEqual(status, "FAIL")
            self.assertTrue(why[0].startswith("no X server"))

    def test_silent_x_server_is_killed_at_the_deadline(self):
        with tempfile.TemporaryDirectory() as d:
            xvfb = self.fake(d, "Xvfb", self.xvfb_body("time.sleep(60)\n", say=False))
            t0 = time.time()
            with open(Path(d) / "x.log", "w") as log, self.assertRaises(RuntimeError):
                harness.start_xvfb("640x480", log, xvfb, wait=2)
            self.assertLess(time.time() - t0, 15)
            self.assertEqual(len(self.xvfb_pids(d)), 1)  # (it ran, and)
            self.assertEqual(self.left_running(d), [])   # (it was stopped)

    def test_an_error_stops_everything_started(self):
        # (the second game's binary is missing: Popen raises; the first game and both X servers must be stopped)
        with tempfile.TemporaryDirectory() as d:
            g1 = self.game(d, "g1", 0, 60)
            g2 = self.game(d, "g2", command="./no-such-binary")
            plan, res = self.plan(d, self.xvfb_body("time.sleep(60)\n"), [g1, g2])
            with self.assertRaises(OSError):
                harness._inner(str(plan), str(res), geteuid=self.NOT_ROOT)
            self.assertEqual(self.left_running(d), [])

    def test_double_term_does_not_stop_the_cleanup(self):
        # (games that ignore TERM: the cleanup waits its grace, then KILLs; a second TERM meanwhile changes nothing)
        import signal
        with tempfile.TemporaryDirectory() as d:
            g = self.game(d, "g", 0, 60)
            Path(g["cwd"], "halo").write_text("#!" + sys.executable + "\nimport os, signal, time, pathlib\n"
                                              "signal.signal(signal.SIGTERM, signal.SIG_IGN)\n"
                                              "pathlib.Path('started').write_text(str(os.getpid()))\ntime.sleep(60)\n")
            plan, res = self.plan(d, self.xvfb_body("time.sleep(60)\n"), [g])
            code = (f"import sys; sys.path.insert(0, {str(HERE)!r}); import harness; "
                    f"harness._inner({str(plan)!r}, {str(res)!r}, geteuid=lambda: 1000, grace=3)")
            p = harness.subprocess.Popen([sys.executable, "-c", code])
            started = Path(d) / "g" / "bin" / "started"
            t0 = time.time()
            while not started.exists() and time.time() - t0 < 15:
                time.sleep(0.1)
            self.assertTrue(started.exists())
            p.send_signal(signal.SIGTERM)
            time.sleep(1)  # (the cleanup is waiting out the game's grace now)
            p.send_signal(signal.SIGTERM)
            p.send_signal(signal.SIGINT)
            self.assertEqual(p.wait(timeout=30), 128 + signal.SIGTERM)
            self.assertEqual(self.left_running(d), [])
            r = json.loads(res.read_text())[0]
            self.assertEqual(r.get("error"), "interrupted")

    def test_root_without_namespace_never_mounts(self):
        # (no unshare: run_group never says in_namespace, so even root never mounts /tmp or touches lo)
        calls = []
        orig = harness.subprocess.run
        harness.subprocess.run = lambda cmd, **kw: calls.append(cmd) or orig(["true"], **kw)
        try:
            with tempfile.TemporaryDirectory() as d:
                plan, res = self.plan(d, self.xvfb_body("sys.exit(0)\n"), [self.game(d, "g")])
                harness._inner(str(plan), str(res), geteuid=lambda: 0)
                notes = []
                p = json.loads(plan.read_text())
                self.assertFalse(harness.private_x_tmp(p, notes))
                p.update(in_namespace=True, host_mnt_ns=harness._ns("mnt"))  # (flag set, but the host's mounts)
                self.assertFalse(harness.private_x_tmp(p, notes))
                self.assertIn("not in our own mount namespace", notes[0])
        finally:
            harness.subprocess.run = orig
        self.assertFalse([c for c in calls if c and c[0] in ("mount", "ip")])

    def test_cleanup_is_one_shared_grace(self):
        # (three groups ignoring TERM: about one grace in all, not one each)
        with tempfile.TemporaryDirectory() as d:
            body = "import signal, time\nsignal.signal(signal.SIGTERM, signal.SIG_IGN)\ntime.sleep(60)\n"
            procs = [harness.subprocess.Popen([self.fake(d, f"p{i}", body)], start_new_session=True) for i in range(3)]
            time.sleep(0.5)
            t0 = time.time()
            harness.stop_all(procs, grace=2)
            self.assertLess(time.time() - t0, 5)
            self.assertTrue(all(p.poll() is not None for p in procs))

    def test_tool_term_stops_inner_runners_and_new_games(self):
        import signal
        saved = {sig: signal.getsignal(sig) for sig in (signal.SIGTERM, signal.SIGINT)}
        with tempfile.TemporaryDirectory() as d:
            inner = harness.subprocess.Popen([self.fake(d, "inner", "import time\ntime.sleep(60)\n")],
                                             start_new_session=True)
            harness.LIVE_INNERS.add(inner)
            try:
                harness.term_as_exit()
                with self.assertRaises(SystemExit):
                    os.kill(os.getpid(), signal.SIGTERM)
                    time.sleep(1)
                self.assertEqual(inner.wait(timeout=10), -signal.SIGTERM)
                with self.assertRaises(RuntimeError):
                    harness.GameSlots(dict(harness.DEFAULTS, work_dir=d), 1, shared_network=False).acquire()
            finally:
                harness.LIVE_INNERS.discard(inner)
                harness.STOPPING.clear()
                for sig, h in saved.items():
                    signal.signal(sig, h)
                if inner.poll() is None:
                    inner.kill()
                    inner.wait()

    def test_term_stops_everything_started(self):
        # (the tool interrupted: run_group sends TERM to the inner runner, which must clean up)
        import signal
        with tempfile.TemporaryDirectory() as d:
            plan, res = self.plan(d, self.xvfb_body("time.sleep(60)\n"), [self.game(d, "g", 0, 60)])
            code = (f"import sys; sys.path.insert(0, {str(HERE)!r}); import harness; "
                    f"harness._inner({str(plan)!r}, {str(res)!r}, geteuid=lambda: 1000)")
            p = harness.subprocess.Popen([sys.executable, "-c", code])
            started = Path(d) / "g" / "bin" / "started"
            t0 = time.time()
            while not started.exists() and time.time() - t0 < 15:
                time.sleep(0.1)
            p.send_signal(signal.SIGTERM)
            self.assertTrue(started.exists())
            self.assertEqual(p.wait(timeout=30), 128 + signal.SIGTERM)
            self.assertEqual(self.left_running(d), [])


class Load(unittest.TestCase):
    def test_owner_detection(self):
        cfg = dict(harness.DEFAULTS, work_dir=str(Path.home() / "halo-test" / "ae_test" / "work"))
        work = str(harness.expand(cfg["work_dir"]))
        mine = [(1, "/x/halo", work + "/r/case/bin")]
        theirs = [(2, "/apps/halo", "/apps")]
        self.assertFalse(harness.owner_playing(cfg, mine, listening=False))
        self.assertTrue(harness.owner_playing(cfg, mine + theirs, listening=False))
        self.assertTrue(harness.owner_playing(cfg, [], listening=True))
        self.assertFalse(harness.owner_playing(dict(cfg, owner_check=False), theirs, listening=True))
        self.assertEqual(len(harness.other_games(cfg, mine + theirs, own={work + "/r"})), 1)
        sibling = [(3, "/x/halo", work + "-other/r/case/bin")]  # (a prefix, not inside work_dir)
        self.assertTrue(harness.owner_playing(cfg, sibling, listening=False))
        self.assertFalse(harness.under(work + "/../work-x", work))
        self.assertEqual(len(harness.other_games(cfg, mine + theirs, own={work + "/other-run"})), 2)

    def test_low_disk(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            cfg = dict(harness.DEFAULTS, work_dir=d)
            self.assertIsNone(harness.low_disk(dict(cfg, min_free_gb=0)))
            self.assertIn("GB free", harness.low_disk(dict(cfg, min_free_gb=1e9)))

    def test_parallel(self):
        # (stub the namespace and private /tmp checks: CI runners may have neither)
        saved = harness.netns_ok, harness.private_tmp_ok
        try:
            harness.netns_ok = lambda: True
            harness.private_tmp_ok = lambda: True
            cfg = dict(harness.DEFAULTS, _slow=True)
            self.assertEqual(harness.resolve_parallel(cfg, "3"), 1)
            cfg["_slow"] = False
            self.assertEqual(harness.resolve_parallel(cfg, "2"), 2)
            harness.private_tmp_ok = lambda: False
            self.assertEqual(harness.resolve_parallel(cfg, "2"), 1)
            harness.private_tmp_ok = lambda: True
            harness.netns_ok = lambda: False
            self.assertEqual(harness.resolve_parallel(cfg, "2"), 1)
        finally:
            harness.netns_ok, harness.private_tmp_ok = saved

    def test_targets(self):
        self.assertEqual(harness.expand_targets(["all"]), ["linux64", "linux", "server", "server-x64"])
        self.assertEqual(harness.expand_targets(["linux64", "servers"]), ["linux64", "server", "server-x64"])
        with self.assertRaises(SystemExit):
            harness.expand_targets(["android"])


class Remote(unittest.TestCase):
    def test_shell_paths(self):
        self.assertEqual(harness.remote_shell_path("~/ae test"), '"$HOME"/' + "'ae test'")
        self.assertEqual(harness.remote_shell_path("~"), '"$HOME"')
        self.assertEqual(harness.remote_shell_path("/srv/x;rm -rf ~"), "'/srv/x;rm -rf ~'")
        with self.assertRaises(SystemExit):
            harness.remote_shell_path("~other/x")

    def test_rsync_paths(self):
        self.assertEqual(harness.remote_rsync_path("~/ae-test"), "ae-test")
        self.assertEqual(harness.remote_rsync_path("/srv/a b"), "/srv/a b")
        self.assertEqual(harness.remote_rsync_path("~"), ".")

    def test_remote_command_is_quoted(self):
        calls = []

        class R:
            returncode = 0
            stdout = ""

        def fake_run(cmd, **kw):
            calls.append(cmd)
            return R()
        cfg = dict(harness.DEFAULTS, box={"ssh": "box", "git_url": "box:ae", "harness_dir": "~/h dir"})
        orig = harness.subprocess.run
        harness.subprocess.run = fake_run
        try:
            with redirect_stdout(io.StringIO()):
                harness.remote(cfg, "run", ["--map", "x; touch y", "--out-name", "n1"], fetch=False)
            with self.assertRaises(SystemExit):
                harness.remote(cfg, "run", ["--out-name", "a;b"], fetch=False)
            with self.assertRaises(SystemExit):
                harness.remote(cfg, "rm -rf", [], fetch=False)
        finally:
            harness.subprocess.run = orig
        ssh_cmds = [c[2] for c in calls if c[0] == "ssh"]
        self.assertIn("mkdir -p \"$HOME\"/'h dir'/tools/ae_test", ssh_cmds)
        self.assertTrue(any(c.startswith("cd \"$HOME\"/'h dir' && python3 tools/ae_test/run.py ") and
                            "'x; touch y'" in c for c in ssh_cmds))
        rsyncs = [c for c in calls if c[0] == "rsync"]
        self.assertTrue(rsyncs and all("--protect-args" in c for c in rsyncs))
        self.assertTrue(any(c[-1] == "box:h dir/tools/ae_test/" for c in rsyncs))


class BuildRemove(unittest.TestCase):
    def test_only_shas_inside_builds(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            cfg = dict(harness.DEFAULTS, builds_dir=d)
            self.assertEqual(harness.removable_build(cfg, "623d8f2223fea69530e6e87d11686fc41e82b3ae"),
                             Path(d).resolve() / "623d8f2223fe")
            for bad in ("../..", "/", "623d8f22", "623D8F2223FE", "623d8f2223fe/..", "", "x" * 12):
                with self.assertRaises(SystemExit, msg=bad):
                    harness.removable_build(cfg, bad)
            (Path(d) / "623d8f2223fe").symlink_to(Path.home())  # (a link out of builds_dir is refused)
            with self.assertRaises(SystemExit):
                harness.removable_build(cfg, "623d8f2223fe")
            (Path(d) / "aaaaaaaaaaaa").mkdir()                    # (a sha-named link to a sibling build too)
            (Path(d) / "bbbbbbbbbbbb").symlink_to(Path(d) / "aaaaaaaaaaaa")
            with self.assertRaises(SystemExit):
                harness.removable_build(cfg, "bbbbbbbbbbbb")
            self.assertEqual(harness.removable_build(cfg, "aaaaaaaaaaaa"), Path(d).resolve() / "aaaaaaaaaaaa")


class Slots(unittest.TestCase):
    def setUp(self):
        self.saved_tmp = harness.private_tmp_ok
        harness.private_tmp_ok = lambda: True  # (CI containers may have no namespaces)
        self.addCleanup(setattr, harness, "private_tmp_ok", self.saved_tmp)
        self.lockdir = tempfile.TemporaryDirectory(dir=Path.home())
        self.saved = os.environ.get(harness.LOCK_DIR_ENV)
        os.environ[harness.LOCK_DIR_ENV] = self.lockdir.name
        self.reg = Path(self.lockdir.name) / "slots"

    def tearDown(self):
        if self.saved is None:
            os.environ.pop(harness.LOCK_DIR_ENV, None)
        else:
            os.environ[harness.LOCK_DIR_ENV] = self.saved
        self.lockdir.cleanup()

    def test_lock_is_host_wide(self):
        os.environ.pop(harness.LOCK_DIR_ENV, None)
        self.assertEqual(harness.slots_dir(), Path.home() / ".cache" / "ae_test")
        a = harness.GameSlots(dict(harness.DEFAULTS, work_dir="~/w1"), 1, shared_network=False)
        b = harness.GameSlots(dict(harness.DEFAULTS, work_dir="~/w2"), 1, shared_network=False)
        self.assertEqual(a.dir, b.dir)  # (different configs, one registry)

    def test_registered_game_counted_once(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            cfg = dict(harness.DEFAULTS, work_dir=d, max_games_total=2)
            owner = os.getppid()  # (a live process that is not us: another harness with another work_dir)
            self.reg.mkdir(parents=True, exist_ok=True)
            (self.reg / f"{owner}-1").write_text(str(owner))
            game = [(900001, "/x/halo", "/other-work/run/case/bin")]   # halo 900001 <- xvfb 900000 <- owner
            parents = {900001: 900000, 900000: owner, owner: 1}
            orig = harness.list_halo_processes
            harness.list_halo_processes = lambda: game
            try:
                slots = harness.GameSlots(cfg, 3, shared_network=False)
                self.assertEqual(slots._others(game, parents.get), 1)   # (its slot, not also "outside")
                stray = game + [(900005, "/y/halo", "/elsewhere")]      # (another script's game counts)
                self.assertEqual(slots._others(stray, lambda p: parents.get(p, 1)), 2)
                mine = [(900007, "/x/halo", d + "/r/c/bin")]
                self.assertEqual(slots._others(mine, {900007: os.getpid()}.get), 1)  # (ours: only the registry's 1)
            finally:
                harness.list_halo_processes = orig

    def test_ancestors(self):
        self.assertEqual(harness.ancestors(5, {5: 4, 4: 1, 1: 0}.get), {5, 4, 1})
        self.assertIn(os.getppid(), harness.ancestors(os.getpid()))

    def test_orphaned_game_counts(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            cfg = dict(harness.DEFAULTS, work_dir=d, max_games_total=1)
            orphan = [(5, "/x/halo", d + "/dead-run/case/bin")]  # (its harness died: no slot file)
            orig = harness.list_halo_processes
            harness.list_halo_processes = lambda: orphan
            try:
                self.assertFalse(harness.GameSlots(cfg, 3, shared_network=False).try_acquire(1)[0])
                self.assertFalse(harness.GameSlots(cfg, 3, shared_network=True).try_acquire(1)[0])
                harness.list_halo_processes = lambda: []
                self.assertTrue(harness.GameSlots(cfg, 3, shared_network=True).try_acquire(1)[0])
            finally:
                harness.list_halo_processes = orig

    def test_registry_counts_other_processes(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            cfg = dict(harness.DEFAULTS, work_dir=d, max_games_total=2)
            orig = harness.list_halo_processes
            harness.list_halo_processes = lambda: []
            try:
                a = harness.GameSlots(cfg, 3, shared_network=False)
                self.assertTrue(a.try_acquire(1)[0])
                # another harness process holding a slot (a live pid that is not ours: our parent)
                (self.reg / f"{os.getppid()}-1").write_text("x")
                self.assertFalse(a.try_acquire(1)[0])
                a.release(1)
                self.assertTrue(a.try_acquire(1)[0])
                # a stale slot (dead pid) is removed and does not count
                (self.reg / f"{os.getppid()}-1").unlink()
                (self.reg / "999999999-1").write_text("x")
                self.assertTrue(a.try_acquire(1)[0])
                self.assertFalse((self.reg / "999999999-1").exists())
                a.release(2)
                self.assertEqual(list(self.reg.iterdir()), [])
            finally:
                harness.list_halo_processes = orig

    def test_shared_network_is_one_slot(self):
        with tempfile.TemporaryDirectory(dir=Path.home()) as d:
            cfg = dict(harness.DEFAULTS, work_dir=d)
            orig = harness.list_halo_processes
            harness.list_halo_processes = lambda: []
            try:
                a = harness.GameSlots(cfg, 3, shared_network=True)
                self.assertEqual(a.parallel, 1)
                self.assertTrue(a.try_acquire(2)[0])   # (a handshake pair takes it as one)
                self.assertFalse(a.try_acquire(1)[0])
                a.release(2)
                harness.list_halo_processes = lambda: [(1, "/x/halo", "/elsewhere")]
                self.assertFalse(a.try_acquire(1)[0])  # (any other game on the machine: wait)
            finally:
                harness.list_halo_processes = orig


class Sheet(unittest.TestCase):
    def test_cases(self):
        s = sheet.case_spec("4p-720", "x", flat=True)
        e = harness.spec_env(s, "/r", "/s", "/shots")
        self.assertEqual(e["HALO_NETWORK_TEST_LOCAL_PLAYERS"], "4")
        self.assertEqual(e["HALO_WINDOW_SIZE"], "1280x720")
        self.assertEqual(e["HALO_GPU_DEBUG_FLAT"], "1")
        self.assertEqual(e["HALO_MENUS"], "pc")

    def test_compose_without_games(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d)
            (out / "runs").mkdir()
            for label, c in (("before", (90, 90, 90)), ("after", (120, 120, 120))):
                (out / "runs" / f"{label}.png").write_bytes(harness.png_bytes(8, 6, bytes(c) * 48))
            meta = {"after": "a", "before": "b", "cases": {"2p": {
                "before": {"frame": "runs/before.png", "build_label": "b"},
                "after": {"frame": "runs/after.png", "build_label": "a",
                          "reticle": [{"view": 1, "found": True, "offset": [1.0, -2.0]}]}}}}
            (out / "sheet.json").write_text(json.dumps(meta))
            written = [p.name for p in sheet.compose(out)]
            self.assertIn("2p-after.png", written)
            self.assertIn("2p-before.png", written)
            self.assertIn("v1 +1,-2", sheet.reticle_text(meta["cases"]["2p"]["after"]))


if __name__ == "__main__":
    unittest.main()


class PanelChecks(unittest.TestCase):
    """panel_checks.py: the edge measure and the primitives' judge on a drawn plate"""

    def test_softness(self):
        self.assertEqual(panel_checks.softness([102, 40, 200, 200, 40, 102]), 0)
        self.assertEqual(panel_checks.softness([102, 40, 120, 200, 200, 120, 40, 102]), 1)
        self.assertEqual(panel_checks.softness([102, 120, 140, 160, 200]), 0)   # no DARK keyline: not an edge
        self.assertEqual(panel_checks.softness([30, 70, 100, 130, 160, 200]), 3)

    LINE = ("arena-evolved: ae panel: tick 600 view 0 name=Player weapon=assault rifle kind=rounds loaded=42/60 "
            "reserve=180 heat=0.00 battery=1.00 overheated=0 low=0 grenades=2/1 armed=frag shield=1.00 health=1.00 "
            "camo=0 overshield=0.0 seat=none origin=150.0,83.0 right=1758.0,83.0 scale=1.00 placed=1")

    def test_panel_line(self):
        (line,) = panel_checks.panel_lines("noise\n" + self.LINE + "\n")
        self.assertEqual((line["weapon"], line["loaded"], line["magazine"], line["frags"], line["armed"]),
                         ("assault rifle", 42, 60, 2, "frag"))
        self.assertEqual(panel_checks.panel_lines(self.LINE, "view 1"), [])

    def test_never_rises(self):
        lines = [{"tick": t, "camo": c} for t, c in ((0, 900), (60, 840), (120, 845), (180, 600))]
        self.assertEqual(len(panel_checks.never_rises(lines, "camo", 1)), 1)

    def test_compare_players(self):
        host = self.LINE.replace("view 0", "player 1").replace(" origin=150.0,83.0 right=1758.0,83.0 scale=1.00 "
                                                                 "placed=1", "")
        why, matched = panel_checks.compare_players(host, self.LINE)
        self.assertEqual((why, matched), ([], 1))
        why, _ = panel_checks.compare_players(host.replace("loaded=42", "loaded=12"), self.LINE)
        self.assertTrue(why)

    def test_plate_missing(self):
        w, h = 600, 340
        rgb = bytearray([20] * (w * h * 3))
        why, _ = panel_checks.judge_primitives(w, h, rgb)
        self.assertTrue(any("plate" in reason for reason in why))
        self.assertTrue(any("box" in reason for reason in why))


class StripChecks(unittest.TestCase):
    """strip_checks.py: the score strip's log lines, the cases' rules, the host/client comparison"""
    LINE = ('arena-evolved: score strip: tick 450 view 0 mode=ctf teams=1 left=2(blue) right=1(red) clock=4:12 up=0 '
            'goal="TO 3" lead="+1" context="" left_sub="DROPPED 0:18" right_sub="bot5" bars=1 fill=0.67/0.33 place=0 '
            'lead_me=1')

    def test_line(self):
        (line,) = strip_checks.strip_lines("noise\n" + self.LINE + "\n")
        self.assertEqual((line["mode"], line["left"], line["lc"], line["right"], line["rc"]),
                         ("ctf", "2", "blue", "1", "red"))
        self.assertEqual((line["goal"], line["left_sub"], line["right_sub"], line["lf"]),
                         ("TO 3", "DROPPED 0:18", "bot5", "0.67"))

    def test_rules(self):
        lines = strip_checks.strip_lines(self.LINE)
        self.assertEqual(strip_checks.check_rules(lines, {"all:mode": "ctf", "left_sub": r"DROPPED \d+:\d\d"}), [])
        self.assertEqual(len(strip_checks.check_rules(lines, {"all:goal": "TO 5"})), 1)
        self.assertEqual(strip_checks.check_rules(lines, {"carried": "bot5"}), [])
        self.assertEqual(strip_checks.check_rules([], {}), ["no score strip lines"])
        for name in strip_checks.CASES:
            spec = strip_checks.case_spec(name, "build")
            self.assertEqual(spec["env"]["HALO_DEBUG_SCORE_STRIP_LOG"], "1")
            self.assertTrue(spec["saved_gametype"])

    def test_pair_is_perspective_free(self):
        host = strip_checks.strip_lines(self.LINE)
        client = strip_checks.strip_lines(self.LINE.replace("left=2(blue) right=1(red)", "left=1(red) right=2(blue)")
                                          .replace("tick 450", "tick 452"))
        self.assertEqual(strip_checks.compare_pair(host, client), [])
        wrong = strip_checks.strip_lines(self.LINE.replace("left=2(blue)", "left=3(blue)"))
        self.assertEqual(len(strip_checks.compare_pair(host, wrong)), 1)

    def test_magenta_box(self):
        w, h = 8, 6
        rgb = bytearray(w * h * 3)
        for x, y in ((2, 1), (5, 4)):
            rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes((255, 0, 255))
        rgb[(3 * w + 1) * 3:(3 * w + 1) * 3 + 3] = bytes((255, 255, 255))  # (white is not the digits')
        self.assertEqual(strip_checks.magenta_box(w, h, rgb), (2, 1, 5, 4))
        self.assertIsNone(strip_checks.magenta_box(w, h, rgb, 6, 0, 8, 6))
        self.assertEqual(strip_checks.judge_digits("nothing", [])[0], ["no 'hud digits:' line"])

    def test_states_from_the_log(self):
        drawn = self.LINE + " drawn=1 alpha=1.00 clock_box=900,950,60,20 score_box=820,955,30,24"
        hidden = drawn.replace("drawn=1", "drawn=0").replace("tick 450", "tick 0")
        text = "\n".join([hidden, "arena-evolved: capture: screenshot /x/shots/frame00000.png (1920x1080, 9 ms)", drawn,
                          "arena-evolved: capture: screenshot /x/shots/frame00150.png (1920x1080, 9 ms)"])
        pairs = strip_checks.shots_with_lines(text)
        self.assertEqual([(name, line["drawn"]) for name, line in pairs],
                         [("frame00000.png", "0"), ("frame00150.png", "1")])
        self.assertEqual(strip_checks.judge_states("classic", hidden, Path(".")), [])
        self.assertEqual(strip_checks.judge_states("classic", drawn, Path(".")), ["drawn with HUD STYLE CLASSIC"])

    def test_clock_colours(self):
        # (the old corner clock's HUD blue at 0.7 over a dark scene; the T1 list's white, grey and team blue are not)
        self.assertTrue(strip_checks.old_clock_blue((90, 140, 185)))
        for colour in ((229, 240, 255), (188, 198, 212), (89, 140, 255), (255, 77, 77), (40, 40, 40)):
            self.assertFalse(strip_checks.old_clock_blue(colour), colour)
        self.assertTrue(strip_checks.hud_colour((117, 186, 255)) and not strip_checks.hud_colour((120, 120, 120)))

