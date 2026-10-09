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

    def test_run_dry_run(self):
        buf = io.StringIO()
        with redirect_stdout(buf):
            run.main(["--build", "abc", "--map", "damnation", "--dry-run"])
        self.assertEqual(json.loads(buf.getvalue())["env"]["HALO_NETWORK_TEST"], "host:damnation")


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
