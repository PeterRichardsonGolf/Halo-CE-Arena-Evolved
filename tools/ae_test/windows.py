#!/usr/bin/env python3
"""windows: one test game on the Windows test box, through its runner only.

    python3 tools/ae_test/windows.py [--artifact arena-evolved-windows64-release [--ci-run ID] | --build-dir SHA12|latest]
        [--map bloodgulch | --init 'map_name levels\\test\\bloodgulch\\bloodgulch'] [--exit-after 60] [--shots 900]
        [--env NAME=value ...] [--label x] [--dry-run]

The Windows box is a shared family PC: its runner (C:\\halo-test\\runner) decides whether a test may start
(idle rules) and stops ours at any input. This tool only ever:
  1. optionally fetches a CI build: fetch-build.ps1 -Artifact <name> [-RunId <id>] (main-branch push runs only),
  2. makes C:\\halo-test\\runs\\<id>\\{data,shots} (data\\maps a junction to C:\\halo-test\\maps) and
     C:\\halo-test\\saves\\<id>, and writes C:\\halo-test\\runner\\request.json with a NEW run id,
  3. starts it: powershell -NoProfile -ExecutionPolicy Bypass -File start-test.ps1 -Wait
     (never schtasks directly). Exit 10 / "TEST NOT STARTED: <reason>" is reported as not-started and never
     retried; exit 11 is aborted, timeout or failed; 0 is done,
  4. copies back result.json, stdout/stderr, debug.txt and the screenshots (converted to PNG here, the BMPs
     deleted on both sides), and deletes the run's save root (map cache) on the box.
Nothing outside C:\\halo-test is touched. Windowed mode comes from the environment (HALO_DISPLAY_MODE=windowed,
HALO_WINDOW_SIZE), as the runner cannot force a window.
"""
import argparse
import base64
import datetime
import json
import ntpath
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402

NOT_STARTED = 10
FAILED = 11
ROOT = "C:\\halo-test"  # the only folder this tool touches on the Windows box
# (set by this tool, never by --env: they decide where the game writes)
PROTECTED_ENV = ("HALO_DATA_ROOT", "HALO_SAVE_ROOT", "HALO_SCREENSHOT_DIR", "HALO_RECORD_DIR")


def check_root(root):
    """the configured root must be C:\\halo-test (Windows path rules); returns it"""
    if ntpath.normcase(ntpath.normpath(str(root))) != ntpath.normcase(ROOT):
        raise SystemExit(f"ae_test windows: windows.root must be {ROOT}, not {root!r}")
    return ROOT


def inside(path, folder):
    """path is strictly inside folder, by Windows path rules (case, separators, .. resolved)"""
    path, folder = str(path), str(folder)
    if not ntpath.isabs(path) or not ntpath.splitdrive(path)[0]:
        return False
    p = ntpath.normcase(ntpath.normpath(path))
    f = ntpath.normcase(ntpath.normpath(folder)).rstrip("\\")
    return p.startswith(f + "\\")


def build_dir_path(root, value):
    """C:\\halo-test\\builds\\<value>: value is a folder name (no drive, no absolute path, no ..)"""
    v = str(value)
    parts = re.split(r"[\\/]+", v)
    if not v or ntpath.isabs(v) or ntpath.splitdrive(v)[0] or ".." in parts or ":" in v:
        raise SystemExit(f"ae_test windows: --build-dir {v!r}: a folder name under {root}\\builds")
    path = ntpath.join(root, "builds", v)
    if not inside(path, ntpath.join(root, "builds")):
        raise SystemExit(f"ae_test windows: --build-dir {v!r} is not under {root}\\builds")
    return ntpath.normpath(path)


def check_env_overrides(root, run_id, pairs):
    """--env NAME=value pairs: never the path variables the tool sets, and any other path-like value
    (a *_DIR/_ROOT/_PATH/_FILE name, or a value with a drive, separator or ..) must be inside the run's folder"""
    run = ntpath.join(root, "runs", run_id)
    out = {}
    for kv in pairs or []:
        k, sep, v = kv.partition("=")
        if not sep or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", k):
            raise SystemExit(f"ae_test windows: --env {kv!r}: NAME=value")
        if k.upper() in PROTECTED_ENV:
            raise SystemExit(f"ae_test windows: --env may not set {k} (the tool sets it inside {run})")
        pathlike = re.search(r"_(DIR|ROOT|PATH|FILE)$", k.upper()) or ntpath.splitdrive(v)[0] or \
            "\\" in v or "/" in v or ".." in v
        if pathlike and not inside(v, run):
            raise SystemExit(f"ae_test windows: --env {k}: {v!r} is not inside {run}")
        out[k] = v
    return out


def ps_encode(script):
    return base64.b64encode(script.encode("utf-16-le")).decode()


def ps(cfg, script, check=False, timeout=600):
    """run a PowerShell script on the Windows box: (exit code, stdout)"""
    host = cfg["windows"]["ssh"]
    if not host:
        raise SystemExit("ae_test windows: no windows.ssh in the config")
    r = subprocess.run(["ssh", host, f"powershell -NoProfile -ExecutionPolicy Bypass -EncodedCommand {ps_encode(script)}"],
                       capture_output=True, text=True, timeout=timeout)
    if check and r.returncode:
        raise SystemExit(f"ae_test windows: remote step failed ({r.returncode}): {r.stderr.strip()[-500:]}")
    return r.returncode, r.stdout


def ps_str(s):
    return "'" + str(s).replace("'", "''") + "'"


def make_request(run_id, exe, workdir, env, timeout_s=120, args=()):
    return {"run_id": run_id, "exe": exe, "workdir": workdir, "args": list(args), "env": env, "timeout_s": timeout_s}


def game_env(root, run_id, a):
    run = f"{root}\\runs\\{run_id}"
    env = {
        "HALO_DATA_ROOT": f"{run}\\data",
        "HALO_SAVE_ROOT": f"{root}\\saves\\{run_id}",
        "HALO_DISPLAY_MODE": "windowed",
        "HALO_WINDOW_SIZE": a.window,
        "SDL_AUDIO_DRIVER": "dummy",
        "HALO_NET_ONLINE": "false",
        "HALO_UPDATE_AUTO": "false",
        "HALO_EXIT_AFTER": str(a.exit_after),
    }
    if a.shots:
        env["HALO_SCREENSHOT_DIR"] = f"{run}\\shots"
        env["HALO_SCREENSHOT_EVERY"] = str(a.shots)
    if a.map:
        env["HALO_NETWORK_TEST"] = f"host:{a.map}"
        env["HALO_NET_ADDRESS"] = "127.0.0.1"
    env.update(check_env_overrides(root, run_id, a.env))
    return env


def init_lines(a):
    if a.init:
        return a.init
    if a.map:
        return []
    return ["game_variant slayer", "map_name levels\\test\\bloodgulch\\bloodgulch"]


def prepare_script(root, run_id, init, request):
    run = f"{root}\\runs\\{run_id}"
    lines = [
        "$ErrorActionPreference = 'Stop'",
        f"New-Item -ItemType Directory -Force {ps_str(run + chr(92) + 'data')} | Out-Null",
        f"New-Item -ItemType Directory -Force {ps_str(run + chr(92) + 'shots')} | Out-Null",
        f"New-Item -ItemType Directory -Force {ps_str(root + chr(92) + 'saves' + chr(92) + run_id)} | Out-Null",
        f"if (-not (Test-Path {ps_str(run + chr(92) + 'data' + chr(92) + 'maps')})) "
        f"{{ New-Item -ItemType Junction -Path {ps_str(run + chr(92) + 'data' + chr(92) + 'maps')} "
        f"-Target {ps_str(root + chr(92) + 'maps')} | Out-Null }}",
    ]
    if init:
        text = "`r`n".join(l.replace("`", "``").replace('"', '`"').replace("$", "`$") for l in init) + "`r`n"
        lines.append(f"Set-Content -Encoding ASCII {ps_str(run + chr(92) + 'data' + chr(92) + 'init.txt')} \"{text}\"")
    body = json.dumps(request, indent=1)
    lines.append(f"Set-Content -Encoding UTF8 {ps_str(root + chr(92) + 'runner' + chr(92) + 'request.json')} "
                 f"{ps_str(body)}")
    lines.append("Write-Output 'PREPARED'")
    return "\n".join(lines)


def parse_start(code, output):
    """(status, reason) from start-test.ps1 -Wait's exit code and output"""
    lines = [l for l in output.splitlines() if l.strip()]
    first = lines[0].strip() if lines else ""
    if code == NOT_STARTED or first.startswith("TEST NOT STARTED"):
        return "not-started", first.partition(":")[2].strip() or "start-test.ps1 exit 10"
    text = output[output.find("{"):output.rfind("}") + 1] if "{" in output else ""
    try:
        res = json.loads(text) if text else {}
    except ValueError:
        res = {}
    if code == 0:
        return res.get("status", "done"), res.get("reason", "")
    if code == FAILED:
        return res.get("status", "failed"), res.get("reason", "") or (lines[0] if lines else "")
    return "error", f"start-test.ps1 exit {code}: {first}"


def fetch(cfg, root, run_id, local):
    """copy the run's files back (never the maps junction): scp of each file and the shots folder"""
    host = cfg["windows"]["ssh"]
    run = f"{root}\\runs\\{run_id}".replace("\\", "/")
    local.mkdir(parents=True, exist_ok=True)
    got = []
    for rel in ("result.json", "stdout.txt", "stderr.txt", "launched.txt", "data/debug.txt", "data/gamestate.txt"):
        dst = local / Path(rel).name
        r = subprocess.run(["scp", "-q", f"{host}:{run}/{rel}", str(dst)], capture_output=True)
        if r.returncode == 0:
            got.append(dst.name)
    r = subprocess.run(["scp", "-q", "-r", f"{host}:{run}/shots", str(local)], capture_output=True)
    pngs = harness.convert_shots(local / "shots") if r.returncode == 0 else []
    return got, pngs


def cleanup_script(root, run_id):
    run = f"{root}\\runs\\{run_id}"
    return "\n".join([
        f"Remove-Item -Recurse -Force -ErrorAction SilentlyContinue {ps_str(root + chr(92) + 'saves' + chr(92) + run_id)}",
        f"Remove-Item -Force -ErrorAction SilentlyContinue {ps_str(run + chr(92) + 'shots' + chr(92) + '*.bmp')}",
    ])


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--config")
    p.add_argument("--out-name")
    p.add_argument("--artifact", help="fetch this CI artifact first (e.g. arena-evolved-windows64-release)")
    p.add_argument("--ci-run", help="the CI run id (default: the newest successful main push run)")
    p.add_argument("--build-dir", default="latest", help="a folder under C:\\halo-test\\builds (sha12), or latest")
    p.add_argument("--map", help="host this multiplayer map (network test) instead of the init.txt Blood Gulch")
    p.add_argument("--init", action="append", help="init.txt line (repeat)")
    p.add_argument("--exit-after", type=int, default=60)
    p.add_argument("--shots", type=int, default=900)
    p.add_argument("--window", help="WxH (default from the config)")
    p.add_argument("--env", action="append")
    p.add_argument("--timeout", type=int, default=120, help="the runner's timeout_s (its hard limit is 180)")
    p.add_argument("--label", default="game")
    p.add_argument("--dry-run", action="store_true", help="print the request and the scripts, run nothing")
    a = p.parse_args(argv)
    cfg = harness.load_config(a.config)
    w = cfg["windows"]
    root = check_root(w["root"])
    a.window = a.window or w.get("window_size", "1280x720")
    run_id = f"ae-{datetime.datetime.now().strftime('%m%d-%H%M%S')}-{harness.safe_name(a.label)}"
    out = harness.new_out_dir(cfg, "windows", None, a.out_name or run_id)
    result = {"run_id": run_id, "status": None, "reason": None}

    # the build
    if a.artifact:
        if a.dry_run:
            build_dir = f"{root}\\builds\\<fetched>"
        else:
            script = (f"& powershell -NoProfile -ExecutionPolicy Bypass -File "
                      f"{ps_str(root + chr(92) + 'runner' + chr(92) + 'fetch-build.ps1')} "
                      f"-Artifact {ps_str(a.artifact)}" + (f" -RunId {int(a.ci_run)}" if a.ci_run else "") +
                      "; Write-Output \"AE_EXIT=$LASTEXITCODE\"")
            code, outp = ps(cfg, script, timeout=1800)
            paths = [l.strip() for l in outp.splitlines() if l.strip().lower().startswith(root.lower() + "\\builds")]
            if not paths:
                result.update(status="no-build", reason="fetch-build.ps1: " + " | ".join(outp.strip().splitlines()[-3:]))
                harness.write_json(out / "result.json", result)
                print(json.dumps(result, indent=2))
                return 2
            build_dir = paths[-1]
            if not inside(build_dir, ntpath.join(root, "builds")):
                raise SystemExit(f"ae_test windows: fetch-build.ps1 gave {build_dir!r}, not under {root}\\builds")
    elif a.build_dir == "latest":
        code, outp = ps(cfg, f"(Get-ChildItem {ps_str(root + chr(92) + 'builds')} -Directory | Sort-Object LastWriteTime "
                             f"| Select-Object -Last 1).FullName")
        build_dir = outp.strip().splitlines()[-1] if outp.strip() else ""
        if not build_dir:
            raise SystemExit("ae_test windows: no build under builds; pass --artifact")
        if not inside(build_dir, ntpath.join(root, "builds")):
            raise SystemExit(f"ae_test windows: {build_dir!r} is not under {root}\\builds")
    else:
        build_dir = build_dir_path(root, a.build_dir)
    exe = f"{build_dir}\\halo.exe"
    if not a.dry_run:
        code, outp = ps(cfg, f"(Get-ChildItem {ps_str(build_dir)} -Recurse -Filter halo.exe | Select-Object -First 1).FullName")
        found = outp.strip().splitlines()[-1] if outp.strip() else ""
        if not found:
            raise SystemExit(f"ae_test windows: no halo.exe under {build_dir}")
        exe = found
    workdir = ntpath.dirname(exe)
    builds = ntpath.join(root, "builds")
    if not inside(exe, builds) or not inside(workdir, builds):
        raise SystemExit(f"ae_test windows: exe and workdir must be under {builds}")
    env = game_env(root, run_id, a)
    request = make_request(run_id, exe, workdir, env, a.timeout)
    prep = prepare_script(root, run_id, init_lines(a), request)
    result.update(build=build_dir, exe=exe, request=request)
    if a.dry_run:
        print(json.dumps(request, indent=2))
        print(prep)
        return 0

    code, outp = ps(cfg, prep)
    if "PREPARED" not in outp:
        result.update(status="error", reason=f"prepare failed ({code}): {outp.strip()[-300:]}")
        harness.write_json(out / "result.json", result)
        print(json.dumps(result, indent=2))
        return 2
    start = (f"& powershell -NoProfile -ExecutionPolicy Bypass -File "
             f"{ps_str(root + chr(92) + 'runner' + chr(92) + 'start-test.ps1')} -Wait; "
             "Write-Output \"AE_EXIT=$LASTEXITCODE\"")
    code, outp = ps(cfg, start, timeout=400)
    (out / "start-test.txt").write_text(outp)
    exit_line = [l for l in outp.splitlines() if l.startswith("AE_EXIT=")]
    start_code = int(exit_line[-1].split("=", 1)[1]) if exit_line and exit_line[-1].split("=", 1)[1].strip().lstrip("-").isdigit() else code
    body = "\n".join(l for l in outp.splitlines() if not l.startswith("AE_EXIT="))
    status, reason = parse_start(start_code, body)
    result.update(status=status, reason=reason, start_exit=start_code)
    if status != "not-started":
        got, pngs = fetch(cfg, root, run_id, out)
        result["files"] = got
        result["shots"] = [str(p.relative_to(out)) for p in pngs]
        if (out / "result.json").exists():
            runner = json.loads((out / "result.json").read_text(encoding="utf-8-sig"))
            (out / "runner-result.json").write_text(json.dumps(runner, indent=2))
            result["runner"] = runner
            result["exit_code"] = runner.get("exit_code")  # (null on some runner versions: not a failure by itself)
            result["seconds"] = runner.get("seconds")
        if (out / "debug.txt").exists():
            result["debug"] = harness.parse_debug((out / "debug.txt").read_text(errors="replace"))
        ps(cfg, cleanup_script(root, run_id))
    harness.write_json(out / "result.json", result)
    d = result.get("debug") or {}
    line = (f"windows {run_id}: {status}" + (f" ({reason})" if reason else "") +
            (f", {result.get('seconds')} s, exit {result.get('exit_code')}, asserts {d.get('asserts')} "
             f"exc {d.get('exceptions')}, {len(result.get('shots', []))} png" if status != "not-started" else ""))
    (out / "summary.txt").write_text(line + "\n")
    print(line)
    print(f"ae_test: results in {out}")
    return {"done": 0, "not-started": NOT_STARTED}.get(status, 1)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
