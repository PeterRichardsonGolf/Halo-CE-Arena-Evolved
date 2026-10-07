"""The test harness itself (tools/ae_test): spec parsing, the games' environment, debug.txt counts, the
verdicts and baseline comparison, images (BMP to PNG, frame picking, reticle offsets), the handshake
comparison, the Windows runner contract and the remote argument rewriting. No game, box or network.
"""
import io
import json
import struct
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

HERE = Path(__file__).resolve().parent / "ae_test"
sys.path.insert(0, str(HERE))

import harness  # noqa: E402
import handshake  # noqa: E402
import record  # noqa: E402
import run  # noqa: E402
import sheet  # noqa: E402
import smoke  # noqa: E402
import windows  # noqa: E402

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
        self.assertIn("mklink /J C:\\halo-test\\runs\\ae-1\\data\\maps C:\\halo-test\\maps", script)
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
        self.assertEqual(len(harness.other_games(cfg, mine + theirs, own={work + "/other-run"})), 2)

    def test_parallel(self):
        cfg = dict(harness.DEFAULTS, _slow=True)
        self.assertEqual(harness.resolve_parallel(cfg, "3"), 1)
        cfg["_slow"] = False
        self.assertEqual(harness.resolve_parallel(cfg, "2"), 2)

    def test_targets(self):
        self.assertEqual(harness.expand_targets(["all"]), ["linux64", "linux", "server", "server-x64"])
        self.assertEqual(harness.expand_targets(["linux64", "servers"]), ["linux64", "server", "server-x64"])
        with self.assertRaises(SystemExit):
            harness.expand_targets(["android"])


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
