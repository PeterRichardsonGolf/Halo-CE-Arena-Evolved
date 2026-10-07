"""Shared parts of the Arena Evolved test harness (tools/ae_test, docs/testing.md).

Python 3 standard library only (Pillow is used when present, for labelled images).
The tools (build.py, run.py, smoke.py, sheet.py, handshake.py, windows.py, record.py)
import this module. Machine-specific paths come from a JSON config file
(config.sample.json shows every key), never from the code:

    $AE_TEST_CONFIG, or ~/.config/ae_test/config.json, or --config FILE

A game runs headless in its own network namespace (unshare -rn: its own loopback,
so several games and system-link bots never meet), under xvfb-run when there is
no display, from its own copy of the binary (config.toml is written beside it),
with its own data root (links to the maps and mods) and save root. Its debug.txt,
stdout and screenshots (converted to PNG, the BMPs deleted) land in the run's
output folder with a result.json; the work folder (save root, map cache) is
deleted afterwards unless --keep-work.
"""

import argparse
import base64
import datetime
import json
import os
import re
import shlex
import shutil
import struct
import subprocess
import sys
import threading
import time
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent
REPO = TOOLS.parent

CONFIG_ENV = "AE_TEST_CONFIG"
CONFIG_DEFAULT = "~/.config/ae_test/config.json"

DEFAULTS = {
    # git checkout that builds commits (worktrees are added to it); default: the repository of this harness
    "repo": None,
    # one worktree and build per commit: <builds_dir>/<sha12>/build/<target>/
    "builds_dir": "~/halo-test/ae_test/builds",
    # per-game data root, binary copy and save root (map caches up to 290 MB each): never under /tmp
    "work_dir": "~/halo-test/ae_test/work",
    # results: <out_dir>/<tool>/<stamp>[-label]/
    "out_dir": "~/halo-test/ae_test/out",
    # maps/ (with ce/ for Custom Edition maps) and mods/<name>/maps; voices/ optional
    "data_dir": None,
    # builds by name: "stock": "<dir with halo>" or {"dir": ..., "env": {...}}
    "named_builds": {},
    "configure_args": ["--release", "--compiler-launcher", "ccache"],
    "pkg_config_path_32": "/usr/lib32/pkgconfig",
    "jobs": 16,
    "jobs_slow": 8,
    # nice level of the tools and the games (19 while the owner plays)
    "nice": 10,
    # games at once from one tool ("auto": 3, or 1 when other games already run here)
    "parallel": "auto",
    # headless games on this machine, all helpers together (a tool waits for a free slot)
    "max_games_total": 4,
    # no game starts while the work folder's disk has less free than this
    "min_free_gb": 3,
    # look for the owner's own game here (a halo process outside work_dir, or port 5150 listening)
    "owner_check": True,
    # "auto": xvfb-run when DISPLAY is unset
    "xvfb": "auto",
    "box": {
        "ssh": None,             # ssh alias of the build box
        "git_url": None,         # where commits are pushed (git push <git_url> <sha>:refs/ae-test/<sha12>)
        "harness_dir": "~/halo-test/ae_test/harness",  # the harness's copy on the box
    },
    "windows": {
        "ssh": None,             # ssh alias of the Windows test box
        "root": "C:\\halo-test",
        "artifact": "arena-evolved-windows64-release",
        "window_size": "1280x720",
    },
}

ADDRESS = "127.0.0.200"
GAME_PORT = 5150

# ---------------------------------------------------------------------------------------------- config


def deep_merge(base, extra):
    out = dict(base)
    for k, v in (extra or {}).items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = deep_merge(out[k], v)
        else:
            out[k] = v
    return out


def expand(p):
    return None if p is None else Path(os.path.expandvars(os.path.expanduser(str(p))))


def load_config(path=None):
    """the config: defaults, then the file (--config, $AE_TEST_CONFIG or ~/.config/ae_test/config.json)"""
    path = path or os.environ.get(CONFIG_ENV) or CONFIG_DEFAULT
    p = expand(path)
    data = {}
    if p and p.exists():
        data = json.loads(p.read_text())
    cfg = deep_merge(DEFAULTS, data)
    cfg["_path"] = str(p) if p and p.exists() else None
    if not cfg.get("repo") and (REPO / "configure.py").exists():
        cfg["repo"] = str(REPO)
    return cfg


def check_not_tmp(path, what):
    """save roots and work folders never in /tmp (a RAM disk on the laptop)"""
    r = str(Path(path).resolve())
    if r == "/tmp" or r.startswith("/tmp/"):
        raise SystemExit(f"ae_test: {what} {r} is under /tmp (RAM); set it elsewhere in the config")


# ---------------------------------------------------------------------------------------------- output


def stamp():
    return datetime.datetime.now().strftime("%Y%m%d-%H%M%S")


def new_out_dir(cfg, tool, label=None, name=None):
    base = expand(cfg["out_dir"]) / tool
    d = base / (name or (stamp() + (f"-{safe_name(label)}" if label else "")))
    d.mkdir(parents=True, exist_ok=True)
    return d


def safe_name(s):
    return re.sub(r"[^A-Za-z0-9_.+-]+", "_", str(s)).strip("_") or "x"


def write_json(path, data):
    Path(path).write_text(json.dumps(data, indent=2, sort_keys=False) + "\n")


def log(msg):
    print(msg, flush=True)


# ---------------------------------------------------------------------------------------------- machine load


def list_halo_processes():
    """[(pid, exe, cwd)] of the game's processes on this machine (exe named halo, also when replaced)"""
    out = []
    for p in Path("/proc").iterdir():
        if not p.name.isdigit():
            continue
        try:
            exe = os.readlink(p / "exe")
        except OSError:
            try:
                comm = (p / "comm").read_text().strip()
            except OSError:
                continue
            if comm != "halo":
                continue
            exe = comm
        base = exe.replace(" (deleted)", "").rsplit("/", 1)[-1]
        if base not in ("halo", "halo.exe") and "Quiver Launcher/Apps/" not in exe:
            continue
        try:
            cwd = os.readlink(p / "cwd")
        except OSError:
            cwd = ""
        out.append((int(p.name), exe, cwd))
    return out


def port_listening(port=GAME_PORT, tables=("/proc/net/tcp", "/proc/net/tcp6")):
    """a TCP listener on this port in this network namespace"""
    hexport = f":{port:04X}"
    for t in tables:
        try:
            lines = Path(t).read_text().splitlines()[1:]
        except OSError:
            continue
        for line in lines:
            f = line.split()
            if len(f) > 3 and f[1].endswith(hexport) and f[3] == "0A":
                return True
    return False


def owner_playing(cfg, procs=None, listening=None):
    """the owner's own game runs here: a halo process whose cwd is outside the harness's work folder,
    or a listener on 5150 (a lobby) in the machine's own namespace. Never touch it."""
    if not cfg.get("owner_check", True):
        return False
    work = str(expand(cfg["work_dir"]))
    procs = list_halo_processes() if procs is None else procs
    for _pid, _exe, cwd in procs:
        if not cwd.startswith(work):
            return True
    return port_listening() if listening is None else listening


OWN_WORK = set()  # this tool's work folders: games there are ours, all others count against the machine's limit


def other_games(cfg, procs=None, own=None):
    """halo processes on this machine that are not this tool's (other helpers' or tools' games, the owner's)"""
    own = OWN_WORK if own is None else own
    procs = list_halo_processes() if procs is None else procs
    return [p for p in procs if not any(p[2].startswith(str(o) + "/") for o in own)]


def apply_load_policy(cfg, args=None):
    """nice, and slow mode while the owner plays: one game at a time, fewer build jobs"""
    slow = owner_playing(cfg)
    level = 19 if slow else int(cfg.get("nice", 10))
    try:
        cur = os.getpriority(os.PRIO_PROCESS, 0)
        if level > cur:
            os.setpriority(os.PRIO_PROCESS, 0, level)
    except (OSError, AttributeError):
        pass
    if slow:
        log("ae_test: the owner's game runs here: slow mode (nice 19, one game at a time, "
            f"{cfg['jobs_slow']} build jobs). It is never touched.")
    cfg["_slow"] = slow
    return slow


def resolve_parallel(cfg, requested):
    """games at once: an explicit number, else 3, or 1 when other games already run here (or in slow mode)"""
    if cfg.get("_slow"):
        return 1
    req = requested if requested not in (None, "auto") else cfg.get("parallel", "auto")
    if req in (None, "auto"):
        return 1 if other_games(cfg) else 3
    return max(1, int(req))


class GameSlots:
    """at most max_games_total games on this machine (all helpers), and at most `parallel` from this tool"""

    def __init__(self, cfg, parallel):
        self.cfg = cfg
        self.parallel = parallel
        self.lock = threading.Lock()
        self.mine = 0

    def acquire(self, games=1, poll=5, quiet=False):
        waited = False
        while True:
            with self.lock:
                others = len(other_games(self.cfg))
                limit = int(self.cfg.get("max_games_total", 4))
                if self.mine + games <= max(self.parallel, games) and others + self.mine + games <= max(limit, games):
                    self.mine += games
                    return
            if not waited and not quiet:
                log(f"ae_test: waiting for a game slot ({others} other games run here, limit {limit})")
                waited = True
            time.sleep(poll)

    def release(self, games=1):
        with self.lock:
            self.mine -= games


# ---------------------------------------------------------------------------------------------- builds

TARGETS = {
    "linux64": "build/linux64/halo",
    "linux": "build/linux/halo",
    "server": "build/server-x86/chupathingyce-server",
    "server-x64": "build/server-x64/chupathingyce-server",
}
ALL_TARGETS = ["linux64", "linux", "server", "server-x64"]


def git(repo, *args, check=True, capture=True):
    r = subprocess.run(["git", "-C", str(repo), *args], text=True,
                       stdout=subprocess.PIPE if capture else None, stderr=subprocess.PIPE if capture else None)
    if check and r.returncode:
        raise SystemExit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return (r.stdout or "").strip()


def resolve_commit(repo, rev):
    r = subprocess.run(["git", "-C", str(repo), "rev-parse", "--verify", "--quiet", f"{rev}^{{commit}}"],
                       text=True, capture_output=True)
    return r.stdout.strip() if r.returncode == 0 else None


def build_commit(cfg, rev, targets=("linux64",), rebuild=False, jobs=None, log_dir=None):
    """worktree <builds_dir>/<sha12> at the commit, configure, ninja the targets; {target: path}"""
    repo = expand(cfg["repo"])
    if not repo or not (repo / ".git").exists():
        raise SystemExit("ae_test build: no git repo to build from (config 'repo')")
    sha = resolve_commit(repo, rev)
    if not sha:
        raise SystemExit(f"ae_test build: {rev} is not a commit in {repo} (push it to the box first: --box)")
    tree = expand(cfg["builds_dir"]) / sha[:12]
    check_not_tmp(tree.parent, "builds_dir")
    tree.parent.mkdir(parents=True, exist_ok=True)
    targets = expand_targets(targets)
    lock = open(str(tree) + ".lock", "w")
    import fcntl
    fcntl.flock(lock, fcntl.LOCK_EX)
    try:
        if not (tree / "configure.py").exists():
            if tree.exists():
                shutil.rmtree(tree)
            git(repo, "worktree", "prune")
            git(repo, "worktree", "add", "--detach", str(tree), sha)
        log_dir = Path(log_dir) if log_dir else tree
        log_dir.mkdir(parents=True, exist_ok=True)
        configure_args = list(cfg["configure_args"])
        stamp_file = tree / ".ae_test_configure"
        if rebuild or not (tree / "build.ninja").exists() or not stamp_file.exists() or \
                stamp_file.read_text() != json.dumps(configure_args):
            with open(log_dir / f"configure-{sha[:12]}.log", "w") as f:
                r = subprocess.run([sys.executable, "configure.py", *configure_args], cwd=tree, stdout=f,
                                   stderr=subprocess.STDOUT)
            if r.returncode:
                raise SystemExit(f"ae_test build: configure failed (log {log_dir}/configure-{sha[:12]}.log)")
            stamp_file.write_text(json.dumps(configure_args))
        jobs = jobs or (cfg["jobs_slow"] if cfg.get("_slow") else cfg["jobs"])
        out = {}
        for t in targets:
            env = dict(os.environ)
            if t == "linux" and cfg.get("pkg_config_path_32"):
                env["PKG_CONFIG_PATH"] = cfg["pkg_config_path_32"]
            start = time.time()
            logf = log_dir / f"build-{sha[:12]}-{t}.log"
            with open(logf, "w") as f:
                r = subprocess.run(["ninja", f"-j{jobs}", t], cwd=tree, env=env, stdout=f, stderr=subprocess.STDOUT)
            if r.returncode:
                tail = logf.read_text(errors="replace").splitlines()[-15:]
                raise SystemExit(f"ae_test build: ninja {t} failed (log {logf}):\n" + "\n".join(tail))
            out[t] = {"path": str(tree / TARGETS[t]), "seconds": round(time.time() - start, 1), "log": str(logf)}
        return {"commit": sha, "tree": str(tree), "targets": out}
    finally:
        fcntl.flock(lock, fcntl.LOCK_UN)
        lock.close()


def expand_targets(targets):
    out = []
    for t in targets:
        for x in (ALL_TARGETS if t == "all" else ["server", "server-x64"] if t == "servers" else [t]):
            if x not in TARGETS:
                raise SystemExit(f"ae_test build: unknown target {x} (known: {', '.join(TARGETS)}, all, servers)")
            if x not in out:
                out.append(x)
    return out


def resolve_build(cfg, name, target="linux64", auto_build=True):
    """a build to play: {label, dir, binary, env}. name: a named build of the config, a path (a folder with
    the binary, or the binary), or a commit of the repo (built under builds_dir on first use)"""
    nb = cfg.get("named_builds", {}).get(name)
    if nb is not None:
        if isinstance(nb, str):
            nb = {"dir": nb}
        d = expand(nb["dir"])
        binary = d / nb.get("binary", "halo") if d.is_dir() else d
        return {"label": name, "dir": str(binary.parent), "binary": str(binary), "env": nb.get("env", {})}
    p = expand(name)
    if p.exists():
        binary = p / "halo" if p.is_dir() else p
        return {"label": str(name), "dir": str(binary.parent), "binary": str(binary), "env": {}}
    repo = expand(cfg.get("repo"))
    sha = resolve_commit(repo, name) if repo and (repo / ".git").exists() else None
    if not sha:
        raise SystemExit(f"ae_test: build '{name}' is not a named build, a path or a commit")
    binary = expand(cfg["builds_dir"]) / sha[:12] / TARGETS[target]
    if not binary.exists():
        if not auto_build:
            raise SystemExit(f"ae_test: {sha[:12]} is not built ({binary}); run build.py first")
        log(f"ae_test: building {sha[:12]} {target}")
        build_commit(cfg, sha, [target])
    return {"label": sha[:12], "dir": str(binary.parent), "binary": str(binary), "env": {}, "commit": sha}


# ---------------------------------------------------------------------------------------------- game spec

SPEC_KEYS = {
    "name": "case name (folder name)",
    "build": "named build, path or commit",
    "target": "linux64 (default) or linux (32-bit)",
    "map": "multiplayer map (host:<map>; '<name>@ce' for Custom Edition) or a campaign scenario "
           "path with backslashes (levels\\a10\\a10, through init.txt)",
    "gametype": "variant for the hosted game (host:<map>:<gametype>)",
    "network_test": "raw HALO_NETWORK_TEST (overrides map: 'join' for a client)",
    "flags": "HALO_NETWORK_TEST_FLAGS (gametype option bits)",
    "start": "HALO_NETWORK_TEST_START seconds",
    "local_players": "split-screen players 1-4 (HALO_NETWORK_TEST_LOCAL_PLAYERS)",
    "bots": "system-link bots joining once the lobby listens (tools/system_link_bots.py)",
    "bot_args": "more arguments for the bots, e.g. ['--move']",
    "mod": "HALO_MOD ('' stock, 'NHE', 'CE+ X')",
    "menus": "HALO_MENUS (pc / xbox)",
    "window": "WxH: windowed at this size (HALO_DISPLAY_MODE=windowed, HALO_WINDOW_SIZE)",
    "exit_after": "HALO_EXIT_AFTER seconds",
    "screenshots": "every N frames (BMP, converted to PNG)",
    "init": "init.txt lines",
    "test_input": "HALO_TEST_INPUT",
    "config_toml": "a config.toml to put beside the binary",
    "env": "more environment variables",
    "record": "seconds of MP4 to record (when the game supports HALO_RECORD_SECONDS and ffmpeg is present)",
    "address": "this game's loopback address (default 127.0.0.200)",
    "broadcast": "HALO_NET_BROADCAST",
    "delay": "seconds after the first game of a group before this one starts",
    "timeout_extra": "seconds past exit_after before the game is killed (default 150)",
    "expect": "pass rules: {'ticks': min tick, 'scripts': true, 'clean_exit': true}",
    "save": "a named save root shared by the cases of one run (default: the case's own)",
}


def parse_spec(data):
    """a game spec (dict) checked against SPEC_KEYS, with defaults"""
    if isinstance(data, str):
        data = json.loads(data)
    unknown = sorted(set(data) - set(SPEC_KEYS))
    if unknown:
        raise ValueError(f"unknown spec keys: {', '.join(unknown)} (known: {', '.join(SPEC_KEYS)})")
    spec = {"name": "game", "target": "linux64", "local_players": 1, "bots": 0, "bot_args": [], "exit_after": 35,
            "screenshots": 0, "init": [], "env": {}, "record": 0, "address": ADDRESS, "delay": 0,
            "timeout_extra": 150, "expect": {}}
    spec.update(data)
    if isinstance(spec["init"], str):
        spec["init"] = [spec["init"]]
    lp = int(spec["local_players"])
    if not 1 <= lp <= 4:
        raise ValueError("local_players is 1-4")
    if spec.get("window") and not re.fullmatch(r"\d+x\d+", spec["window"]):
        raise ValueError("window is WxH, e.g. 1920x1080")
    return spec


def spec_kind(spec):
    """mp (a hosted network test game), campaign (init.txt map_name), join, or menu"""
    nt = spec.get("network_test")
    if nt:
        return "join" if nt == "join" else "mp"
    m = spec.get("map")
    if not m:
        return "menu"
    return "campaign" if "\\" in m else "mp"


def spec_env(spec, data_root, save_root, shots_dir=None):
    """the environment of one game (without the inherited one)"""
    env = {
        "SDL_AUDIO_DRIVER": "dummy",
        "SDL_GAMECONTROLLER_IGNORE_DEVICES": "all",
        "HALO_HIDDEN_WINDOW": "1",
        "HALO_NET_ONLINE": "false",
        "HALO_NET_ADDRESS": spec.get("address", ADDRESS),
        "HALO_UPDATE_AUTO": "false",
        "HALO_UPDATE_ANSWER": "no",
        "HALO_DATA_ROOT": str(data_root),
        "HALO_SAVE_ROOT": str(save_root),
        "HALO_EXIT_AFTER": str(spec["exit_after"]),
    }
    kind = spec_kind(spec)
    if spec.get("network_test"):
        env["HALO_NETWORK_TEST"] = spec["network_test"]
    elif kind == "mp":
        env["HALO_NETWORK_TEST"] = f"host:{spec['map']}" + (f":{spec['gametype']}" if spec.get("gametype") else "")
    if kind in ("mp",) and int(spec.get("local_players", 1)) > 1:
        env["HALO_NETWORK_TEST_LOCAL_PLAYERS"] = str(spec["local_players"])
    if spec.get("flags") is not None:
        env["HALO_NETWORK_TEST_FLAGS"] = str(spec["flags"])
    if spec.get("start") is not None:
        env["HALO_NETWORK_TEST_START"] = str(spec["start"])
    if spec.get("mod") is not None:
        env["HALO_MOD"] = spec["mod"]
    if spec.get("menus"):
        env["HALO_MENUS"] = spec["menus"]
    if spec.get("window"):
        env["HALO_DISPLAY_MODE"] = "windowed"
        env["HALO_WINDOW_SIZE"] = spec["window"]
    if spec.get("broadcast"):
        env["HALO_NET_BROADCAST"] = spec["broadcast"]
    if spec.get("test_input"):
        env["HALO_TEST_INPUT"] = spec["test_input"]
    if spec.get("screenshots") and shots_dir:
        env["HALO_SCREENSHOT_DIR"] = str(shots_dir)
        env["HALO_SCREENSHOT_EVERY"] = str(spec["screenshots"])
    env.update({k: str(v) for k, v in spec.get("env", {}).items()})
    return env


def spec_init(spec):
    lines = list(spec.get("init", []))
    if spec_kind(spec) == "campaign" and not any(l.startswith("map_name") for l in lines):
        lines.append(f"map_name {spec['map']}")
    return lines


def map_available(cfg, spec):
    """the data a spec needs is there: (True, '') or (False, why)"""
    data = expand(cfg.get("data_dir"))
    if not data:
        return False, "no data_dir in the config"
    mod = spec.get("mod") or ""
    if mod and not (data / "mods" / mod).exists():
        return False, f"mod '{mod}' missing ({data}/mods)"
    m = spec.get("map")
    if not m or spec.get("network_test"):
        return True, ""
    if "\\" in m:
        name = m.rsplit("\\", 1)[-1]
        dirs = [data / "maps"] + ([data / "mods" / mod / "maps"] if mod else [])
        ok = any((d / f"{name}.map").exists() for d in dirs)
        return (ok, "" if ok else f"campaign map {name}.map missing")
    if m.endswith("@ce"):
        name = m[:-3]
        ok = (data / "maps" / "ce" / f"{name}.map").exists()
        return (ok, "" if ok else f"Custom Edition map {name}.map missing ({data}/maps/ce)")
    dirs = [data / "maps"] + ([data / "mods" / mod / "maps"] if mod else [])
    ok = any((d / f"{m}.map").exists() for d in dirs)
    return (ok, "" if ok else f"map {m}.map missing")


# ---------------------------------------------------------------------------------------------- debug.txt

RE_TICK = re.compile(r"network test: tick (\d+) (.*)")
RE_PLAYER = re.compile(r"player (\d+): \(([-\d.]+) ([-\d.]+) ([-\d.]+)\)[^|]*? k(-?\d+) d(-?\d+)")


def parse_debug(text):
    """the counts a test looks at in a debug.txt"""
    lines = text.splitlines()
    r = {"version": "", "network_version": None, "joined": None, "delta": [], "tick": None, "players": 0,
         "items": None, "asserts": 0, "exceptions": 0, "refusals": 0, "lost_scripts": 0, "lost_lines": [],
         "scenario_scripts": None, "corrected": 0, "clean_exit": False, "problems": [], "final": {}}
    for line in lines:
        s = line.strip()
        if not r["version"] and re.match(r"^[\w-]+: (Halo CE|ChupathingyCE|OpenCE|halo)", s, re.I) and \
                "network version" not in s:
            r["version"] = s[:200]
        m = re.search(r"OpenCE network version (\d+)", s)
        if m and r["network_version"] is None:
            r["network_version"] = int(m[1])
        if "joining a host of network version" in s:
            r["joined"] = s.split("  ", 1)[-1][:200]
        if "Delta Peer:" in s and ("speaks Delta" in s or "agreed" in s or "legacy" in s):
            r["delta"].append(s.split(": ", 1)[-1][:200])
        m = RE_TICK.search(s)
        if m:
            r["tick"] = int(m[1])
            players = RE_PLAYER.findall(m[2])
            r["players"] = len(players)
            r["final"] = {int(p[0]): {"k": int(p[4]), "d": int(p[5])} for p in players}
            im = re.search(r"\| items (\d+)", m[2])
            if im:
                r["items"] = int(im[1])
            continue
        low = s.lower()
        if re.search(r"\bassert", low):
            r["asserts"] += 1
            if len(r["problems"]) < 20:
                r["problems"].append(s[:220])
        elif re.search(r"exception|fault at|segmentation fault|unhandled signal", low):
            r["exceptions"] += 1
            if len(r["problems"]) < 20:
                r["problems"].append(s[:220])
        if "cannot be played" in low or re.search(r"\brefus", low):
            r["refusals"] += 1
            if len(r["problems"]) < 20:
                r["problems"].append(s[:220])
        m = re.search(r"(\d+) scripts won't run", s)
        if m:
            r["lost_scripts"] += int(m[1])
            r["lost_lines"].append(s[:220])
        elif "scripts won't run" in s:
            r["lost_scripts"] += 1
            r["lost_lines"].append(s[:220])
        m = re.search(r"scenario scripts: (\d+) scripts", s)
        if m:
            r["scenario_scripts"] = int(m[1])
        if "is corrected" in s:
            r["corrected"] += 1
        if "exiting after debug.exit_after" in s:
            r["clean_exit"] = True
    return r


def player_tracks(text):
    """{player: {tick: (x, y, z, kills, deaths)}} from the network test's per-second lines"""
    d = {}
    for line in text.splitlines():
        m = RE_TICK.search(line)
        if not m:
            continue
        t = int(m[1])
        for p in RE_PLAYER.finditer(m[2]):
            d.setdefault(int(p[1]), {})[t] = (float(p[2]), float(p[3]), float(p[4]), int(p[5]), int(p[6]))
    return d


def compare_tracks(host_text, client_text, window=2):
    """per player: samples, median and 90th percentile of the host-client distance at the same tick
    (within `window` ticks), and both machines' last kills/deaths"""
    import math
    h, c = player_tracks(host_text), player_tracks(client_text)
    out = {}
    for p in sorted(set(h) & set(c)):
        diffs = []
        for t, v in h[p].items():
            near = [u for u in c[p] if abs(u - t) <= window]
            if near:
                u = min(near, key=lambda u: abs(u - t))
                diffs.append(math.dist(v[:3], c[p][u][:3]))
        diffs.sort()
        ht, ct = max(h[p]), max(c[p])
        out[p] = {"samples": len(diffs),
                  "median_m": round(diffs[len(diffs) // 2], 3) if diffs else None,
                  "p90_m": round(diffs[int(len(diffs) * 0.9)], 3) if diffs else None,
                  "host_kd": [h[p][ht][3], h[p][ht][4]], "client_kd": [c[p][ct][3], c[p][ct][4]]}
    return out


def evaluate(spec, result):
    """PASS / FAIL with reasons, from the run and its debug.txt"""
    why = []
    d = result.get("debug") or {}
    exp = spec.get("expect", {})
    if result.get("skipped"):
        return "SKIP", [result["skipped"]]
    if result.get("timed_out"):
        why.append("timed out (killed)")
    elif result.get("exit_code") not in (0,):
        why.append(f"exit {result.get('exit_code')}")
    if not result.get("debug_found"):
        why.append("no debug.txt")
    else:
        if exp.get("clean_exit", True) and not d.get("clean_exit"):
            why.append("no clean exit line")
        if d.get("asserts"):
            why.append(f"{d['asserts']} asserts")
        if d.get("exceptions"):
            why.append(f"{d['exceptions']} exceptions")
        if d.get("refusals") and not exp.get("refusals_ok"):
            why.append(f"{d['refusals']} refusals")
        if d.get("lost_scripts") and not exp.get("lost_ok"):
            why.append(f"{d['lost_scripts']} lost scripts")
        kind = spec_kind(spec)
        min_tick = exp.get("ticks", 150 if kind in ("mp", "join") else None)
        if min_tick and (d.get("tick") or 0) < min_tick:
            why.append(f"tick {d.get('tick')} < {min_tick}")
        if exp.get("scripts", kind == "campaign") and not d.get("scenario_scripts"):
            why.append("no scenario scripts line")
    return ("FAIL" if why else "PASS"), why


def one_line(name, result):
    d = result.get("debug") or {}
    if result.get("skipped"):
        return f"{name}: SKIP ({result['skipped']})"
    bits = [f"{result.get('status', '?')}", f"exit {result.get('exit_code')}", f"{result.get('seconds', 0):.0f} s"]
    if d.get("tick") is not None:
        bits.append(f"tick {d['tick']}")
    if d.get("items") is not None:
        bits.append(f"items {d['items']}")
    if d.get("scenario_scripts") is not None:
        bits.append(f"scripts {d['scenario_scripts']}")
    bits.append(f"asserts {d.get('asserts', '-')} exc {d.get('exceptions', '-')} "
                f"refused {d.get('refusals', '-')} lost {d.get('lost_scripts', '-')}")
    if result.get("shots"):
        bits.append(f"{len(result['shots'])} png")
    if result.get("why"):
        bits.append("(" + "; ".join(result["why"]) + ")")
    return f"{name}: " + ", ".join(bits)


# ---------------------------------------------------------------------------------------------- images


def png_bytes(width, height, rgb):
    """a PNG (8-bit RGB, filter 0) from packed RGB rows"""
    stride = width * 3
    raw = bytearray()
    for y in range(height):
        raw += b"\0"
        raw += rgb[y * stride:(y + 1) * stride]

    def chunk(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b""))


def read_bmp(data):
    """(width, height, rgb) of an uncompressed 24- or 32-bit BMP, either row order"""
    if data[:2] != b"BM":
        raise ValueError("not a BMP")
    off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    comp = struct.unpack_from("<I", data, 30)[0]
    if bpp not in (24, 32) or comp not in (0, 3):
        raise ValueError(f"BMP {bpp} bit, compression {comp}: not supported")
    n = bpp // 8
    stride = (w * n + 3) & ~3
    top_down = h < 0
    h = abs(h)
    rgb = bytearray(w * h * 3)
    for y in range(h):
        sy = y if top_down else h - 1 - y
        row = data[off + sy * stride: off + sy * stride + w * n]
        o = y * w * 3
        out = bytearray(w * 3)
        out[0::3] = row[2::n]
        out[1::3] = row[1::n]
        out[2::3] = row[0::n]
        rgb[o:o + w * 3] = out
    return w, h, rgb


def read_png(path):
    """(width, height, rgb) of a PNG: Pillow when present, else 8-bit RGB/RGBA with any filter"""
    try:
        from PIL import Image
        im = Image.open(path).convert("RGB")
        return im.width, im.height, bytearray(im.tobytes())
    except ImportError:
        pass
    data = Path(path).read_bytes()
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, t = struct.unpack_from(">I4s", data, pos)
        body = data[pos + 8:pos + 8 + n]
        if t == b"IHDR":
            w, h, depth, ctype = struct.unpack_from(">IIBB", body)
            if depth != 8 or ctype not in (2, 6):
                raise ValueError("only 8-bit RGB/RGBA PNGs without Pillow")
            ch = 3 if ctype == 2 else 4
        elif t == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * ch
    prev = bytearray(stride)
    rgb = bytearray(w * h * 3)
    i = 0
    for y in range(h):
        f = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in (range(stride) if f else ()):
            a = line[x - ch] if x >= ch else 0
            b = prev[x]
            c = prev[x - ch] if x >= ch else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        o = y * w * 3
        if ch == 3:
            rgb[o:o + w * 3] = line
        else:
            seg = bytearray(w * 3)
            seg[0::3], seg[1::3], seg[2::3] = line[0::4], line[1::4], line[2::4]
            rgb[o:o + w * 3] = seg
    return w, h, rgb


def bmp_to_png(path, delete=True):
    """convert one BMP to a PNG beside it (Pillow when present), delete the BMP; the PNG's path"""
    path = Path(path)
    png = path.with_suffix(".png")
    try:
        from PIL import Image
        with Image.open(path) as im:
            im.convert("RGB").save(png, optimize=False)
    except ImportError:
        w, h, rgb = read_bmp(path.read_bytes())
        png.write_bytes(png_bytes(w, h, rgb))
    if delete:
        path.unlink()
    return png


def convert_shots(folder, delete=True):
    """every BMP in a folder to PNG; the sorted PNG paths"""
    folder = Path(folder)
    if not folder.exists():
        return []
    for b in sorted(folder.glob("*.bmp")):
        try:
            bmp_to_png(b, delete)
        except Exception as e:  # a frame cut short when the game was killed
            log(f"ae_test: {b.name}: {e}")
    return sorted(folder.glob("*.png"))


def mean_brightness(w, h, rgb, step=97):
    """the mean of a sample of the image's channels, 0..1"""
    sample = rgb[::step]
    return (sum(sample) / len(sample) / 255.0) if sample else 0.0


def pick_frame(pngs, dark=0.06):
    """the last frame that is not (nearly) black: the game's last picture before it quit"""
    for p in reversed(list(pngs)):
        try:
            w, h, rgb = read_png(p)
        except Exception:
            continue
        if mean_brightness(w, h, rgb) > dark:
            return Path(p)
    return Path(pngs[-1]) if pngs else None


def split_views(w, h, players):
    """the split-screen views (x, y, w, h): 2p top/bottom, 3p one top and two below, 4p quarters"""
    if players == 1:
        return [(0, 0, w, h)]
    if players == 2:
        return [(0, 0, w, h // 2), (0, h // 2, w, h - h // 2)]
    if players == 3:
        return [(0, 0, w, h // 2), (0, h // 2, w // 2, h - h // 2), (w // 2, h // 2, w - w // 2, h - h // 2)]
    return [(0, 0, w // 2, h // 2), (w // 2, 0, w - w // 2, h // 2), (0, h // 2, w // 2, h - h // 2),
            (w // 2, h // 2, w - w // 2, h - h // 2)]


def reticle_offsets(w, h, rgb, players, threshold=(200, 245, 235), radius=0.25, join=12):
    """per view: the reticle's centre (the bounding box of the biggest near-white blob near the view's
    middle, grown by the blobs within `join` px of it: a ring of ticks is several blobs) and its offset
    from the view's centre in px. Best on a flat render (HALO_GPU_DEBUG_FLAT=1), where the world is flat
    colour and the HUD keeps its own: on a real scene the sky or a light can win."""
    rt, gt, bt = threshold
    out = []
    for i, (x, y, vw, vh) in enumerate(split_views(w, h, players)):
        cx, cy = x + vw / 2.0, y + vh / 2.0
        r = int(min(vw, vh) * radius)
        x0, y0 = max(0, int(cx - r)), max(0, int(cy - r))
        x1, y1 = min(w, int(cx + r)), min(h, int(cy + r))
        cw = x1 - x0
        mask = bytearray((x1 - x0) * (y1 - y0))
        for yy in range(y0, y1):
            row = rgb[(yy * w + x0) * 3:(yy * w + x1) * 3]
            base = (yy - y0) * cw
            for xx in range(cw):
                if row[xx * 3] > rt and row[xx * 3 + 1] > gt and row[xx * 3 + 2] > bt:
                    mask[base + xx] = 1
        blobs = []
        seen = bytearray(len(mask))
        ch = y1 - y0
        for start in range(len(mask)):
            if not mask[start] or seen[start]:
                continue
            stack = [start]
            seen[start] = 1
            n, bx0, by0, bx1, by1 = 0, cw, ch, -1, -1
            while stack:
                k = stack.pop()
                n += 1
                kx, ky = k % cw, k // cw
                bx0, bx1, by0, by1 = min(bx0, kx), max(bx1, kx), min(by0, ky), max(by1, ky)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        nx, ny = kx + dx, ky + dy
                        if 0 <= nx < cw and 0 <= ny < ch:
                            j = ny * cw + nx
                            if mask[j] and not seen[j]:
                                seen[j] = 1
                                stack.append(j)
            blobs.append([n, bx0, by0, bx1, by1])
        if not blobs:
            out.append({"view": i + 1, "found": False})
            continue
        big = max(range(len(blobs)), key=lambda k: blobs[k][0])
        _, bx0, by0, bx1, by1 = blobs[big]
        used, grown = {big}, True
        while grown:
            grown = False
            for k, (n, a0, b0, a1, b1) in enumerate(blobs):
                if k in used or n < 2:
                    continue
                if a0 <= bx1 + join and a1 >= bx0 - join and b0 <= by1 + join and b1 >= by0 - join:
                    bx0, by0, bx1, by1 = min(bx0, a0), min(by0, b0), max(bx1, a1), max(by1, b1)
                    used.add(k)
                    grown = True
        rx, ry = x0 + (bx0 + bx1) / 2.0, y0 + (by0 + by1) / 2.0
        vx, vy = cx - 0.5, cy - 0.5
        out.append({"view": i + 1, "found": True, "reticle": [round(rx, 1), round(ry, 1)],
                    "size": [bx1 - bx0 + 1, by1 - by0 + 1], "centre": [vx, vy],
                    "offset": [round(rx - vx, 1), round(ry - vy, 1)]})
    return out


# ---------------------------------------------------------------------------------------------- running games


def netns_ok():
    """unshare -rn works here (a user and network namespace without root)"""
    if not shutil.which("unshare") or not shutil.which("ip"):
        return False
    r = subprocess.run(["unshare", "-rn", "true"], capture_output=True)
    return r.returncode == 0


def use_xvfb(cfg):
    x = cfg.get("xvfb", "auto")
    if x == "auto":
        return not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY")
    return bool(x)


def prepare_game(cfg, spec, build, work, out, save_roots=None):
    """the folders and launch description of one game: work/{root,bin,save}, out/shots"""
    check_not_tmp(work, "work folder")
    data = expand(cfg.get("data_dir"))
    if work.exists():
        shutil.rmtree(work)
    root, binf = work / "root", work / "bin"
    root.mkdir(parents=True)
    binf.mkdir(parents=True)
    if spec.get("save"):
        save = (save_roots or {}).setdefault(spec["save"], work.parent / f"save-{safe_name(spec['save'])}")
    else:
        save = work / "save"
    Path(save).mkdir(parents=True, exist_ok=True)
    shots = out / "shots"
    if spec.get("screenshots"):
        shots.mkdir(parents=True, exist_ok=True)
    src = Path(build["dir"])
    for d in ("maps", "mods"):
        if data and (data / d).exists():
            (root / d).symlink_to(data / d)
    voices = src / "voices" if (src / "voices").exists() else (data / "voices" if data and (data / "voices").exists() else None)
    if voices:
        (root / "voices").symlink_to(voices)
        (binf / "voices").symlink_to(voices)
    for f in ("brokers.txt",):
        if (src / f).exists():
            shutil.copy2(src / f, root / f)
            shutil.copy2(src / f, binf / f)
    shutil.copy2(build["binary"], binf / "halo")
    init = spec_init(spec)
    if init:
        (root / "init.txt").write_text("\n".join(init) + "\n")
    if spec.get("config_toml"):
        shutil.copy2(expand(spec["config_toml"]), binf / "config.toml")
    env = spec_env(spec, root, save, shots)
    env.update({k: str(v) for k, v in build.get("env", {}).items()})
    rec = None
    if spec.get("record"):
        import record as record_mod
        rec = record_mod.record_env(build["binary"], spec["record"], out / "video")
        env.update(rec.get("env", {}))
    w, h = (spec.get("window") or "1920x1080").split("x")
    return {"name": spec["name"], "cwd": str(binf), "env": env, "root": str(root), "save": str(save),
            "shots": str(shots), "delay": spec.get("delay", 0), "address": spec.get("address", ADDRESS),
            "timeout": float(spec["exit_after"]) + float(spec.get("timeout_extra", 150)),
            "screen": f"{max(int(w), 640)}x{max(int(h), 480)}", "record": rec}


def run_group(cfg, games, bots=None, inner_out=None):
    """run prepared games together (one network namespace) and wait; [{exit_code, timed_out, seconds}]"""
    plan = {"games": games, "bots": bots, "xvfb": use_xvfb(cfg), "nice": 19 if cfg.get("_slow") else cfg.get("nice", 10),
            "addresses": sorted({g["address"] for g in games})}
    inner_out = Path(inner_out)
    plan_path = inner_out / "plan.json"
    write_json(plan_path, plan)
    res_path = inner_out / "inner_result.json"
    cmd = [sys.executable, str(HERE / "harness.py"), "--inner", str(plan_path), str(res_path)]
    if netns_ok():
        cmd = ["unshare", "-rn", *cmd]
    else:
        log("ae_test: no unshare -rn here: the game shares this machine's network (one game at a time)")
    subprocess.run(cmd)
    try:
        return json.loads(res_path.read_text())
    except (OSError, ValueError):  # (no result: e.g. the disk filled up)
        pass
    return [{"exit_code": None, "timed_out": False, "seconds": 0, "error": "inner runner wrote no result"}
            for _ in games]


def _inner(plan_path, res_path):
    """(inside the namespace) loopback addresses, the games under xvfb-run and timeout, the bots"""
    import socket
    plan = json.loads(Path(plan_path).read_text())
    if os.geteuid() == 0:  # root of the new user namespace: our own loopback
        subprocess.run(["ip", "link", "set", "lo", "up"], capture_output=True)
        for a in plan["addresses"]:
            subprocess.run(["ip", "addr", "add", f"{a}/8", "dev", "lo"], capture_output=True)
    procs = []
    t0 = time.time()
    for g in plan["games"]:
        while time.time() - t0 < g.get("delay", 0):
            time.sleep(0.5)
        cmd = ["./halo"]
        if plan["xvfb"]:
            cmd = ["xvfb-run", "-a", "-s", f"-screen 0 {g['screen']}x24", *cmd]
        env = dict(os.environ)
        env.update(g["env"])
        out = open(Path(g["cwd"]).parent / "stdout.log", "w")
        p = subprocess.Popen(cmd, cwd=g["cwd"], env=env, stdout=out, stderr=subprocess.STDOUT,
                             start_new_session=True)
        procs.append((g, p, time.time(), out))
    bot_proc = None
    b = plan.get("bots")
    if b and b.get("machines"):
        host = b.get("host", ADDRESS)
        deadline = time.time() + 120
        while time.time() < deadline:
            try:
                with socket.create_connection((host, GAME_PORT), timeout=1):
                    break
            except OSError:
                time.sleep(1)
        bf = open(b["log"], "w")
        bot_proc = subprocess.Popen([sys.executable, str(TOOLS / "system_link_bots.py"), "--host", host,
                                     "--machines", str(b["machines"]), "--seed", str(b.get("seed", 7)),
                                     "--seconds", str(b.get("seconds", 80)), *b.get("args", [])],
                                    stdout=bf, stderr=subprocess.STDOUT, start_new_session=True)
    results = []
    import signal
    for g, p, start, out in procs:
        timed_out = False
        try:
            rc = p.wait(timeout=max(1, g["timeout"] - (time.time() - start)))
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(p.pid, signal.SIGTERM)
                rc = p.wait(timeout=10)
            except (subprocess.TimeoutExpired, ProcessLookupError):
                try:
                    os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                rc = p.wait()
        out.close()
        results.append({"exit_code": rc, "timed_out": timed_out, "seconds": round(time.time() - start, 1)})
    if bot_proc:
        try:
            os.killpg(bot_proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        bot_proc.wait()
    write_json(res_path, results)


def collect_game(spec, prepared, inner, out, keep_work=False):
    """debug.txt, stdout and screenshots into out/, result.json with the counts and the verdict"""
    out.mkdir(parents=True, exist_ok=True)
    root = Path(prepared["root"])
    work = root.parent
    found = None
    for cand in (root / "debug.txt", Path(prepared["save"]) / "debug.txt", Path(prepared["cwd"]) / "debug.txt"):
        if cand.exists():
            found = cand
            break
    text = ""
    if found:
        shutil.copy2(found, out / "debug.txt")
        text = found.read_text(errors="replace")
    if (work / "stdout.log").exists():
        shutil.copy2(work / "stdout.log", out / "stdout.log")
    for extra in ("gamestate.txt", "crash.txt"):
        if (root / extra).exists():
            shutil.copy2(root / extra, out / extra)
    pngs = convert_shots(prepared["shots"]) if spec.get("screenshots") else []
    result = {"name": spec["name"], "build": spec.get("build"), "kind": spec_kind(spec),
              "exit_code": inner.get("exit_code"), "timed_out": inner.get("timed_out"),
              "seconds": inner.get("seconds"), "debug_found": bool(found), "debug": parse_debug(text),
              "shots": [str(Path(p).relative_to(out)) for p in pngs], "env": prepared["env"],
              "record": (prepared.get("record") or {}).get("status")}
    status, why = evaluate(spec, result)
    result["status"], result["why"] = status, why
    write_json(out / "result.json", result)
    if not keep_work:
        shutil.rmtree(work, ignore_errors=True)
    return result


def low_disk(cfg):
    """a reason not to start a game when the work folder's disk has less than min_free_gb free (a save root
    with its map cache takes up to 290 MB; other helpers share the disk), else None"""
    need = float(cfg.get("min_free_gb", 3))
    p = expand(cfg["work_dir"])
    p.mkdir(parents=True, exist_ok=True)
    free = shutil.disk_usage(p).free / 1e9
    return f"only {free:.1f} GB free on the work disk (min_free_gb {need:g})" if free < need else None


def play(cfg, spec, out, slots=None, keep_work=False, run_id=None):
    """one game, start to finish: the result dict (also out/result.json)"""
    if "timeout_extra" not in spec:  # (not parsed yet)
        spec = parse_spec(spec)
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    ok, why = map_available(cfg, spec)
    if not ok:
        result = {"name": spec["name"], "build": spec.get("build"), "skipped": why, "status": "SKIP", "why": [why]}
        write_json(out / "result.json", result)
        return result
    build = resolve_build(cfg, spec["build"], spec.get("target", "linux64"))
    low = low_disk(cfg)
    if low:
        result = {"name": spec["name"], "build": spec.get("build"), "status": "FAIL", "why": [low]}
        write_json(out / "result.json", result)
        return result
    work = expand(cfg["work_dir"]) / (run_id or stamp()) / safe_name(spec["name"])
    OWN_WORK.add(work.parent)
    prepared = prepare_game(cfg, spec, build, work, out)
    bots = None
    if spec.get("bots"):
        bots = {"machines": spec["bots"], "args": spec.get("bot_args", []), "host": spec.get("address", ADDRESS),
                "seconds": spec["exit_after"], "log": str(out / "bots.log")}
    slots = slots or GameSlots(cfg, 1)
    slots.acquire()
    try:
        inner = run_group(cfg, [prepared], bots, work)[0]
    finally:
        slots.release()
    result = collect_game(spec, prepared, inner, out, keep_work)
    result["build_label"] = build["label"]
    write_json(out / "result.json", result)
    return result


# ---------------------------------------------------------------------------------------------- the box (remote)

BUILD_ARGS = ("--build", "--baseline", "--before", "--after", "--host-build", "--client-build", "--ae", "--other",
              "--commit")


def add_common_args(p):
    p.add_argument("--config", help="config file (default $AE_TEST_CONFIG or ~/.config/ae_test/config.json)")
    p.add_argument("--box", action="store_true",
                   help="run on the build box: push the commits, copy the harness, run there, fetch the results")
    p.add_argument("--out-name", help="name of the output folder (default: a time stamp)")
    p.add_argument("--keep-work", action="store_true", help="keep the work folders (save roots, binary copies)")


def remote(cfg, tool, argv, fetch=True):
    """run `tool` on the box with these arguments: the box's own config applies there. Commits named in
    the build arguments are resolved here and pushed to the box first. Returns (exit code, local out dir)."""
    box = cfg["box"]
    if not box.get("ssh"):
        raise SystemExit("ae_test: no box.ssh in the config")
    hdir = box["harness_dir"]
    argv = list(argv)
    out_args = []
    i = 0
    pushed = {}
    while i < len(argv):
        a = argv[i]
        if a == "--box":
            i += 1
            continue
        if a == "--config":
            i += 2
            continue
        key, val, step = a, None, 1
        if "=" in a and a.split("=", 1)[0] in BUILD_ARGS:
            key, val = a.split("=", 1)
        elif a in BUILD_ARGS and i + 1 < len(argv):
            val, step = argv[i + 1], 2
        if val is not None and key in BUILD_ARGS:
            vals = []
            for v in val.split(","):
                sha = push_commit(cfg, v, pushed)
                vals.append(sha or v)
            out_args += [key, ",".join(vals)]
        else:
            out_args.append(a)
        i += step
    name = None
    if "--out-name" in out_args:
        name = out_args[out_args.index("--out-name") + 1]
    else:
        name = stamp()
        out_args += ["--out-name", name]
    sync_harness(cfg)
    cmd = f"cd {hdir} && python3 tools/ae_test/{tool}.py " + " ".join(shlex.quote(a) for a in out_args)
    log(f"ae_test: on {box['ssh']}: {tool}.py {' '.join(out_args)}")
    r = subprocess.run(["ssh", box["ssh"], cmd])
    local = None
    if fetch:
        local = expand(cfg["out_dir"]) / tool / name
        local.mkdir(parents=True, exist_ok=True)
        rr = subprocess.run(["ssh", box["ssh"], f"cd {hdir} && python3 tools/ae_test/harness.py --out-dir {tool} {name}"],
                            capture_output=True, text=True)
        rdir = rr.stdout.strip().splitlines()[-1] if rr.stdout.strip() else None
        if rdir:
            subprocess.run(["rsync", "-a", f"{box['ssh']}:{rdir}/", f"{local}/"])
            log(f"ae_test: results fetched to {local}")
    return r.returncode, local


def push_commit(cfg, rev, pushed):
    """a commit of this repo pushed to the box (refs/ae-test/<sha12>): its full sha, or None (not a commit)"""
    if rev in pushed:
        return pushed[rev]
    if expand(rev).exists() and "/" in rev:
        pushed[rev] = None
        return None
    sha = resolve_commit(REPO, rev)
    if sha:
        url = cfg["box"].get("git_url")
        if not url:
            raise SystemExit("ae_test: no box.git_url in the config")
        r = subprocess.run(["git", "-C", str(REPO), "push", "-q", url, f"{sha}:refs/ae-test/{sha[:12]}"],
                           capture_output=True, text=True)
        if r.returncode and "up to date" not in r.stderr:
            raise SystemExit(f"ae_test: push {sha[:12]} to the box failed: {r.stderr.strip()}")
    pushed[rev] = sha
    return sha


def sync_harness(cfg):
    box = cfg["box"]
    hdir = box["harness_dir"]
    subprocess.run(["ssh", box["ssh"], f"mkdir -p {hdir}/tools/ae_test"], check=True)
    subprocess.run(["rsync", "-a", "--delete", "--exclude", "__pycache__", f"{HERE}/",
                    f"{box['ssh']}:{hdir}/tools/ae_test/"], check=True)
    subprocess.run(["rsync", "-a", f"{TOOLS}/system_link_bots.py", f"{box['ssh']}:{hdir}/tools/"], check=True)


# ---------------------------------------------------------------------------------------------- entry


def main(argv):
    if len(argv) >= 3 and argv[0] == "--inner":
        _inner(argv[1], argv[2])
        return 0
    if len(argv) >= 3 and argv[0] == "--out-dir":  # (for remote(): where a tool's named run is)
        cfg = load_config()
        print(expand(cfg["out_dir"]) / argv[1] / argv[2])
        return 0
    p = argparse.ArgumentParser(description="ae_test harness: show the config, or the machine's load")
    p.add_argument("--config")
    p.add_argument("--show-config", action="store_true")
    p.add_argument("--status", action="store_true", help="the games running here and whether the owner plays")
    a = p.parse_args(argv)
    cfg = load_config(a.config)
    if a.status:
        procs = list_halo_processes()
        print(json.dumps({"games": procs, "other_games": len(other_games(cfg, procs)),
                          "owner_playing": owner_playing(cfg, procs)}, indent=2))
        return 0
    print(json.dumps(cfg, indent=2, default=str))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
