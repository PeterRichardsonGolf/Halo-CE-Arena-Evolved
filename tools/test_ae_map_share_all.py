"""Map sharing for every map but the game's own (0.2.3; port/linux/game/ae_map_share_all.c), in real games: a host and
a guest on one loopback network, each with its own data root, the guest without the host's maps.

Runs only when asked (two games of about a minute each case), on a machine with the game's maps:

    AE_SHARE_BUILD=$PWD/build/linux64 AE_SHARE_DATA=~/halo-data AE_SHARE_WORK=~/ae-share-work \
        python3 -m pytest -q tools/test_ae_map_share_all.py

AE_SHARE_DATA needs the stock maps (maps/), a community map in the Xbox's format (AE_SHARE_XBOX_MAP, by default
mods/NHE/maps/badcreek.map), and Custom Edition's homobox.map with bitmaps.map, sounds.map and loc.map (maps/ce).
AE_SHARE_WORK must not be under /tmp (the data roots and the downloads go there; it is emptied after each case).
Nothing here is committed with a map: the test copies the maps it finds.
"""
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

BUILD = os.environ.get("AE_SHARE_BUILD")
DATA = os.environ.get("AE_SHARE_DATA")
WORK = os.environ.get("AE_SHARE_WORK")
HERE = Path(__file__).resolve().parent
HOST, GUEST, GUEST2, BOT = "127.0.0.200", "127.0.0.201", "127.0.0.202", "127.0.0.210"
PIECE = 0x40000
STOCK = ("beavercreek sidewinder damnation ratrace prisoner hangemhigh chillout carousel boardingaction bloodgulch "
         "wizard putput longest a10 ui").split()

pytestmark = pytest.mark.skipif(not (BUILD and DATA and WORK), reason="needs AE_SHARE_BUILD, AE_SHARE_DATA, AE_SHARE_WORK")


def share_hash(data):
    """map sharing's hash of a file (map_share_files.c): BLAKE2b-256 of a label, the size and the pieces' hashes"""
    pieces = b"".join(hashlib.blake2b(data[at:at + PIECE], digest_size=32).digest() for at in range(0, len(data), PIECE))
    return hashlib.blake2b(b"OpenCE map 1" + struct.pack("<I", len(data)) + pieces, digest_size=32).digest()


def xbox_map_named(source, name):
    """a copy of an Xbox map's bytes with its header's name changed (the game lists a map whose header's name is its
    file's)"""
    data = bytearray(Path(source).read_bytes())
    data[0x20:0x40] = name.encode().ljust(32, b"\0")
    return bytes(data)


def small_map(name, size=300000):
    """a small file with an Xbox map's header under a name: one the game lists (its header is a map's, of this
    file's own length) and never loads; a headless game sends a quarter megabyte a second, so the cases that only
    fetch use these. Each name's bytes are its own"""
    header = bytearray(xbox_source().read_bytes()[:2048])
    header[0x20:0x40] = name.encode().ljust(32, b"\0")
    header[8:12] = struct.pack("<I", size)
    header[0x10:0x14] = struct.pack("<I", 2048)
    header[0x14:0x18] = struct.pack("<I", 0x1000)
    body = hashlib.blake2b(name.encode(), digest_size=32).digest()
    return (bytes(header) + body * ((size - 2048) // 32 + 1))[:size]


def seconds_of(text, line):
    """the time of day (seconds) of the first log line that holds `line`"""
    found = next(row for row in text.splitlines() if line in row)
    hours, minutes, seconds = (int(part) for part in found.split()[1].split(":"))
    return hours * 3600 + minutes * 60 + seconds


def xbox_source():
    return Path(os.environ.get("AE_SHARE_XBOX_MAP") or Path(DATA).expanduser() / "mods/NHE/maps/badcreek.map")


def make_root(root, xbox=(), mod=(), ce=(), resources=True, raw=()):
    """a data root: the stock maps linked, and its own maps: xbox / mod {name: bytes} (mod: of the mod "sharemod"),
    ce {file name: bytes} in maps_ce, raw {relative path: bytes}"""
    data = Path(DATA).expanduser()
    (root / "maps").mkdir(parents=True)
    for name in STOCK:
        if (data / "maps" / f"{name}.map").exists():
            (root / "maps" / f"{name}.map").symlink_to(data / "maps" / f"{name}.map")
    for name, content in dict(xbox).items():
        (root / "maps" / f"{name}.map").write_bytes(content)
    for name, content in dict(mod).items():
        (root / "mods/sharemod/maps").mkdir(parents=True, exist_ok=True)
        (root / "mods/sharemod/maps" / f"{name}.map").write_bytes(content)
    if resources:
        (root / "maps/ce").mkdir()
        for name in ("bitmaps", "sounds", "loc"):
            (root / "maps/ce" / f"{name}.map").symlink_to(data / "maps/ce" / f"{name}.map")
    for name, content in dict(ce).items():
        (root / "maps_ce").mkdir(exist_ok=True)
        (root / "maps_ce" / name).write_bytes(content)
    for path, content in dict(raw).items():
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).write_bytes(content)
    return root


def game_env(root, address, peer, seconds, extra):
    env = {
        "SDL_AUDIO_DRIVER": "dummy", "HALO_ARENA_MENUS": "0", "SDL_GAMECONTROLLER_IGNORE_DEVICES": "all", "HALO_HIDDEN_WINDOW": "1",
        "HALO_NET_ONLINE": "false", "HALO_NET_ADDRESS": address, "HALO_NET_BROADCAST": peer,
        "HALO_UPDATE_AUTO": "false", "HALO_UPDATE_ANSWER": "no", "HALO_DATA_ROOT": str(root),
        "HALO_SAVE_ROOT": str(root / "save"), "HALO_EXIT_AFTER": str(seconds), "HALO_MCC_USE": "never",
    }
    env.update(extra)
    return env


INNER = textwrap.dedent("""
    import json, os, subprocess, sys, time
    plan = json.load(open(sys.argv[1]))
    subprocess.run(["ip", "link", "set", "lo", "up"], capture_output=True)
    for address in plan["addresses"]:
        subprocess.run(["ip", "addr", "add", address + "/8", "dev", "lo"], capture_output=True)
    running = []
    for game in plan["games"]:
        time.sleep(game["delay"])
        env = dict(os.environ)
        env.update(game["env"])
        log = open(game["log"], "wb")
        running.append(subprocess.Popen(game["command"], cwd=game["cwd"], env=env, stdout=log, stderr=log))
    deadline = time.time() + plan["seconds"] + 40
    for process in running:
        try:
            process.wait(timeout=max(1, deadline - time.time()))
        except subprocess.TimeoutExpired:
            process.kill()
""")


def run_pair(name, host_root, guest_root, host_env=None, guest_env=None, seconds=55, level="bloodgulch", start=600,
             join_delay=8, bot=None, guest=True, second=None):
    """the host (network_test host:<level>, started after `start` seconds) and the guest (network_test join) played
    together; (host debug.txt, guest debug.txt). bot: arguments of tools/system_link_bots.py run beside them"""
    import json
    work = Path(WORK).expanduser()
    assert "/tmp" not in str(work.resolve()).split(os.sep)[1:2], "AE_SHARE_WORK must not be under /tmp"
    binary = str(Path(BUILD).expanduser() / "halo")
    xvfb = ["xvfb-run", "-a", "-s", "-screen 0 1280x720x24"] if shutil.which("xvfb-run") else []
    games = [{"delay": 0, "cwd": str(host_root), "log": str(host_root / "halo.log"), "command": xvfb + [binary],
              "env": game_env(host_root, HOST, f"{GUEST},{GUEST2}" if second is not None else GUEST, seconds, {"HALO_NETWORK_TEST": f"host:{level}",
                                                                 "HALO_NETWORK_TEST_START": str(start),
                                                                 "HALO_NET_MAP_SHARE_UPLOAD": "65536",
                                                                 **(host_env or {})})}]
    if guest:
        games.append({"delay": join_delay, "cwd": str(guest_root), "log": str(guest_root / "halo.log"),
                      "command": xvfb + [binary],
                      "env": game_env(guest_root, GUEST, HOST, seconds - join_delay,
                                      {"HALO_NETWORK_TEST": "join", **(guest_env or {})})})
    if second is not None:
        # (a second guest: (its data root, its environment), on an address of its own)
        games.append({"delay": 2, "cwd": str(second[0]), "log": str(second[0] / "halo.log"), "command": xvfb + [binary],
                      "env": game_env(second[0], GUEST2, HOST, seconds - join_delay - 2,
                                      {"HALO_NETWORK_TEST": "join", **second[1]})})
    if bot is not None:
        games.append({"delay": 0 if guest else join_delay, "cwd": str(work), "log": str(work / f"{name}-bot.log"),
                      "env": {}, "command": [sys.executable, str(HERE / "system_link_bots.py"), "--host", HOST,
                                             "--first-address", BOT, "--machines", "1",
                                             "--seconds", str(seconds - join_delay - 10), *bot]})
    plan = work / f"{name}-plan.json"
    inner = work / "inner.py"
    inner.write_text(INNER)
    plan.write_text(json.dumps({"addresses": [HOST, GUEST, GUEST2, BOT], "games": games, "seconds": seconds}))
    subprocess.run(["unshare", "-rnm", "--pid", "--fork", "--mount-proc", sys.executable, str(inner), str(plan)],
                   check=False, timeout=seconds + 120)
    texts = [(root / "debug.txt").read_text(errors="replace") if (root / "debug.txt").exists() else ""
             for root in (host_root, guest_root)]
    if os.environ.get("AE_SHARE_KEEP"):
        for root, text in zip(("host", "guest"), texts):
            (work / f"{name}-{root}-debug.txt").write_text(text)
    return texts


@pytest.fixture
def roots():
    work = Path(WORK).expanduser()
    work.mkdir(parents=True, exist_ok=True)
    for old in ("host", "guest", "guest2"):
        shutil.rmtree(work / old, ignore_errors=True)
    yield work / "host", work / "guest"
    if not os.environ.get("AE_SHARE_KEEP"):
        for old in ("host", "guest", "guest2"):
            shutil.rmtree(work / old, ignore_errors=True)


def share_lines(text):
    return "\n".join(line for line in text.splitlines() if "map sharing" in line)


def ce_map():
    """homobox as a host has it: by its own name (network version 29: a game names a Custom Edition map by its own
    name, its version the hash's first 32 bits; GET ALL's list still names it <name>_<hash>), and as a guest gets it"""
    data = (Path(DATA).expanduser() / "maps/ce/homobox.map").read_bytes()
    return "homobox.map", data


# (no question any more, the OpenCE way: the setting is the yes, on by default)
YES = {}


def test_played_xbox_map(roots):
    """(i) a guest without the game's community map gets it into maps_shared, and plays"""
    host, guest = roots
    content = xbox_map_named(xbox_source(), "sharecreek")
    make_root(host, xbox={"sharecreek": content})
    make_root(guest)
    host_text, guest_text = run_pair("played-xbox", host, guest, guest_env=YES, level="sharecreek", start=125,
                                     seconds=165)
    assert "runs this game: it gets this host's list" in host_text, share_lines(host_text)
    assert "sharecreek.map is in maps_shared now" in guest_text, share_lines(guest_text)
    assert (guest / "maps_shared/sharecreek.map").read_bytes() == content
    assert not list((guest / "maps_shared").glob("*.part")) and not list((guest / "maps_shared").glob("*_*.map"))
    # (never the stock maps folder)
    assert not (guest / "maps/sharecreek.map").exists()
    assert "map sharing: downloaded levels\\test\\sharecreek\\sharecreek" in guest_text, share_lines(guest_text)
    # (and plays: the game started, the guest's own player in it)
    assert "| local 1 camera" in guest_text.split("is in maps_shared now")[1]


def test_played_mod_map(roots):
    """(iii) a guest without the game's map, a map of the host's active mod, gets it"""
    host, guest = roots
    content = xbox_map_named(xbox_source(), "modcreek")
    make_root(host, mod={"modcreek": content})
    make_root(guest)
    host_text, guest_text = run_pair("played-mod", host, guest, host_env={"HALO_MOD": "sharemod"}, guest_env=YES,
                                     level="modcreek", start=600, seconds=140)
    assert (guest / "maps_shared/modcreek.map").exists(), share_lines(guest_text) + share_lines(host_text)
    assert (guest / "maps_shared/modcreek.map").read_bytes() == content


def test_played_ce_map(roots):
    """(ii) a guest without the game's Custom Edition map gets it into maps_ce, as before, after the same yes"""
    host, guest = roots
    name, content = ce_map()
    make_root(host, ce={name: content})
    make_root(guest)
    host_text, guest_text = run_pair("played-ce", host, guest, guest_env=YES, level=name[:-4] + "@ce", seconds=170)
    assert (guest / "maps_ce" / name).exists(), share_lines(guest_text) + share_lines(host_text)
    assert (guest / "maps_ce" / name).read_bytes() == content


def test_played_folder_map(roots):
    """a guest without the game's community map, which the host keeps in a page folder of its maps folder (map folders,
    0.3.3: maps/<folder>/), gets it into maps_shared as any other"""
    host, guest = roots
    content = xbox_map_named(xbox_source(), "foldercreek")
    make_root(host, raw={"maps/Pack/foldercreek.map": content})
    make_root(guest)
    host_text, guest_text = run_pair("played-folder", host, guest, guest_env=YES, level="foldercreek", start=125,
                                     seconds=165)
    assert (guest / "maps_shared/foldercreek.map").exists(), share_lines(guest_text) + share_lines(host_text)
    assert (guest / "maps_shared/foldercreek.map").read_bytes() == content


def test_played_ce_folder_map(roots):
    """a guest without the game's Custom Edition map, which the host keeps in a page folder of maps_ce (as a player
    keeps Custom Edition's Gearbox maps in their own folder), gets it into maps_ce"""
    host, guest = roots
    name, content = ce_map()
    make_root(host, raw={f"maps_ce/Gearbox/{name}": content})
    make_root(guest)
    host_text, guest_text = run_pair("played-ce-folder", host, guest, guest_env=YES, level=name[:-4] + "@ce",
                                     seconds=170)
    assert (guest / "maps_ce" / name).exists(), share_lines(guest_text) + share_lines(host_text)
    assert (guest / "maps_ce" / name).read_bytes() == content


def three_maps():
    name, content = ce_map()
    return {"sharecreek": small_map("sharecreek")}, {"modcreek": small_map("modcreek")}, {name: content}


def test_get_all(roots):
    """GET ALL with 3 maps (a community map, a mod's, a Custom Edition map) in a lobby on a stock map; no stock map
    is listed or fetched"""
    host, guest = roots
    xbox, mod, ce = three_maps()
    make_root(host, xbox=xbox, mod=mod, ce=ce)
    make_root(guest)
    host_text, guest_text = run_pair("get-all", host, guest, host_env={"HALO_MOD": "sharemod"},
                                     guest_env={}, seconds=120)
    assert "this host lists 6 maps" in host_text, share_lines(host_text)  # (3 maps and the 3 resource maps)
    assert "the host lists 6 maps; this machine lacks 3 of them" in guest_text, share_lines(guest_text)
    assert (guest / "maps_shared/sharecreek.map").read_bytes() == xbox["sharecreek"], share_lines(guest_text)
    assert (guest / "maps_shared/modcreek.map").read_bytes() == mod["modcreek"]
    assert (guest / "maps_ce" / next(iter(ce))).read_bytes() == next(iter(ce.values()))
    assert "is done: 3 of 3 fetched" in guest_text, share_lines(guest_text)
    # (no question: nothing asked, no key; the host's maps came by themselves)
    assert "the player said yes" not in guest_text and "the player is asked" not in guest_text, share_lines(guest_text)
    for stock in STOCK:
        assert not (guest / "maps_shared" / f"{stock}.map").exists()


def test_downloads_off(roots):
    """DOWNLOAD MAPS off: nothing is asked of the host and nothing is written; the game's own map is not fetched"""
    host, guest = roots
    make_root(host, xbox={"sharecreek": xbox_map_named(xbox_source(), "sharecreek")})
    make_root(guest)
    host_text, guest_text = run_pair("downloads-off", host, guest, guest_env={"HALO_NET_MAP_DOWNLOAD": "false"},
                                     level="sharecreek", seconds=45)
    assert "asks for" not in share_lines(host_text), share_lines(host_text)
    assert not (guest / "maps_shared").exists() or not list((guest / "maps_shared").iterdir())
    assert not (guest / "maps_ce").exists() or not list((guest / "maps_ce").iterdir())
    assert "downloads are off for this host" in guest_text, share_lines(guest_text)


def test_internet_without_opt_in(roots):
    """a host that is not on the local network is not asked for maps (the guest's switch, turned off here)"""
    host, guest = roots
    xbox, mod, ce = three_maps()
    make_root(host, xbox=xbox)
    make_root(guest)
    host_text, guest_text = run_pair("internet-guest", host, guest, guest_env={"HALO_MAP_SHARE_TEST": "internet",
                                                "HALO_NET_MAP_DOWNLOAD_INTERNET": "false"},
                                     seconds=40)
    # (it knows the host runs this game, and keeps no list of it)
    assert "the host runs this game" in guest_text, share_lines(guest_text)
    assert "the host lists" not in guest_text, share_lines(guest_text)
    assert "asks for" not in share_lines(host_text)
    assert not (guest / "maps_shared").exists() or not list((guest / "maps_shared").iterdir())


def test_internet_host_without_opt_in(roots):
    """a host does not share with a guest that is not on the local network (the host's switch, off by default): it
    says so, sends no list, and answers no request; with the switch on it shares"""
    host, guest = roots
    xbox, mod, ce = three_maps()
    make_root(host, xbox=xbox)
    make_root(guest)
    host_text, guest_text = run_pair("internet-host", host, guest, host_env={"HALO_MAP_SHARE_TEST": "internet"},
                                     guest_env={}, seconds=40)
    assert "not shared with (network.map_share, network.map_share_internet)" in host_text, share_lines(host_text)
    assert "was sent the list" not in host_text
    assert "does not share its maps with this machine" in guest_text, share_lines(guest_text)
    assert not (guest / "maps_shared").exists() or not list((guest / "maps_shared").iterdir())
    shutil.rmtree(guest)
    make_root(guest)
    host_text, guest_text = run_pair("internet-host-on", host, guest,
                                     host_env={"HALO_MAP_SHARE_TEST": "internet", "HALO_NET_MAP_SHARE_INTERNET": "true"},
                                     guest_env={}, seconds=50)
    assert (guest / "maps_shared/sharecreek.map").exists(), share_lines(guest_text) + share_lines(host_text)


def test_internet_with_opt_in(roots):
    host, guest = roots
    xbox, mod, ce = three_maps()
    make_root(host, xbox=xbox)
    make_root(guest)
    host_text, guest_text = run_pair("internet-on", host, guest,
                                     guest_env={"HALO_MAP_SHARE_TEST": "internet",
                                                "HALO_NET_MAP_DOWNLOAD_INTERNET": "true"},
                                     seconds=50)
    assert (guest / "maps_shared/sharecreek.map").exists(), share_lines(guest_text)


def test_over_the_total_cap(roots):
    """a map over what the guest takes from one host is refused before a byte is stored"""
    host, guest = roots
    make_root(host, xbox={"sharecreek": xbox_map_named(xbox_source(), "sharecreek")})
    make_root(guest)
    host_text, guest_text = run_pair("cap", host, guest, guest_env={**YES, "HALO_NET_MAP_DOWNLOAD_LIMIT": "5"},
                                     level="sharecreek", seconds=45)
    assert "over the 5 MB you take from one host" in guest_text, share_lines(guest_text)
    assert "asks for" not in share_lines(host_text)
    assert not (guest / "maps_shared").exists() or not list((guest / "maps_shared").iterdir())


def test_host_does_not_share(roots):
    """a host with sharing off says so: the guest asks for nothing and leaves"""
    host, guest = roots
    xbox, mod, ce = three_maps()
    make_root(host, xbox={"sharecreek": xbox_map_named(xbox_source(), "sharecreek")})
    make_root(guest)
    host_text, guest_text = run_pair("share-off", host, guest, host_env={"HALO_NET_MAP_SHARE": "false"},
                                     guest_env=YES, level="sharecreek", seconds=40)
    assert "not shared with" in host_text, share_lines(host_text)
    assert "The host does not share its maps" in guest_text, share_lines(guest_text)
    assert "asks for" not in share_lines(host_text)
    assert not (guest / "maps_shared").exists() or not list((guest / "maps_shared").iterdir())


@pytest.mark.parametrize("lie", ["lie-size", "lie-hash"])
def test_lying_list(roots, lie):
    """a host whose offer is not its list's (another size, another hash): refused before a byte is stored"""
    host, guest = roots
    xbox, mod, ce = three_maps()
    make_root(host, xbox=xbox)
    make_root(guest)
    host_text, guest_text = run_pair(lie, host, guest, host_env={"HALO_MAP_SHARE_TEST": lie},
                                     guest_env={}, seconds=45)
    assert "is not the map of its list (another size or hash)" in guest_text, share_lines(guest_text)
    assert not list((guest / "maps_shared").glob("*")), list((guest / "maps_shared").glob("*"))
    assert "0 of 1 fetched" in guest_text, share_lines(guest_text)


def test_lying_map_is_deleted(roots):
    """a host sends a file that is not the map its list named (the header is another map's): downloaded whole, it
    fails the check and is deleted; nothing is left under the map's name"""
    host, guest = roots
    make_root(host, xbox={"fakecreek": small_map("sharecreek")})
    make_root(guest)
    host_text, guest_text = run_pair("lie-map", host, guest, host_env={"HALO_MAP_SHARE_TEST": "lie-map"},
                                     guest_env={}, seconds=50)
    assert guest_text.count("map sharing: downloading fakecreek, 1 files") == 1, share_lines(guest_text)
    assert guest_text.count("removed d:\\maps_shared\\fakecreek_") == 1, share_lines(guest_text)
    assert "is done: 0 of 1 fetched, some could not be" in guest_text, share_lines(guest_text)
    assert not list((guest / "maps_shared").glob("*")), list((guest / "maps_shared").glob("*"))


def test_long_map_is_deleted(roots):
    """L3: a downloaded file far longer than its header says the map is (megabytes of anything after a small map's
    header) is not kept; an honest host does not list one"""
    host, guest = roots
    long_map = small_map("longcreek") + b"x" * 400000
    make_root(host, xbox={"longcreek": long_map})
    make_root(guest)
    honest, guest_text = run_pair("long-honest", host, guest, guest_env={}, seconds=30)
    assert "this host lists 0 maps" in honest, share_lines(honest)
    shutil.rmtree(guest)
    make_root(guest)
    host_text, guest_text = run_pair("long-map", host, guest, host_env={"HALO_MAP_SHARE_TEST": "lie-map"},
                                     guest_env={}, seconds=50)
    assert guest_text.count("removed d:\\maps_shared\\longcreek_") == 1, share_lines(guest_text)
    assert not list((guest / "maps_shared").glob("*")), list((guest / "maps_shared").glob("*"))


def test_many_maps_do_not_stall_the_played_map(roots):
    """M2: a host with more listed files than map sharing's catalogue of hashed files holds (16). One guest fetches
    them all, one after another, while another guest downloads the game's own map: both end with what they asked
    for. (A regression case: the rule itself, a file in use keeps its place, is proved with negative controls in
    tools/harness/tests/test_ae_map_share_kept.py; with the rule off these games still ended well, as map sharing
    hashes the game's own file again when it has lost its place)"""
    host, guest = roots
    second = guest.parent / "guest2"
    played = xbox_map_named(xbox_source(), "sharecreek")
    many = {f"extra{number:02d}": small_map(f"extra{number:02d}") for number in range(20)}
    make_root(host, xbox={"sharecreek": played, **many})
    make_root(guest)
    make_root(second, xbox={"sharecreek": played})
    host_text, guest_text = run_pair("many", host, guest, guest_env=YES, level="sharecreek", seconds=200,
                                     second=(second, {}))
    second_text = (second / "debug.txt").read_text(errors="replace")
    assert "this host lists 21 maps" in host_text, share_lines(host_text)
    assert "is done: 20 of 20 fetched" in second_text, share_lines(second_text)[-3000:]
    assert len(list((second / "maps_shared").glob("extra*.map"))) == 20
    assert (guest / "maps_shared/sharecreek.map").read_bytes() == played, share_lines(guest_text)[-3000:]
    assert "failed" not in share_lines(guest_text)


def test_never_overwrites(roots):
    """a file of the map's name in maps_shared is kept: the download is not put over it"""
    host, guest = roots
    xbox, mod, ce = three_maps()
    make_root(host, xbox=xbox)
    # (a file the game does not list as a map, so the guest still lacks the map)
    make_root(guest, raw={"maps_shared/sharecreek.map": b"mine"})
    host_text, guest_text = run_pair("overwrite", host, guest, guest_env={}, seconds=50)
    assert (guest / "maps_shared/sharecreek.map").read_bytes() == b"mine", share_lines(guest_text)


def test_stock_like_peer_on_ce_map(roots):
    """a machine without this game's mark (tools/system_link_bots.py) in a game on a Custom Edition map: it is sent no
    list, and the game goes on as before"""
    host, guest = roots
    name, content = ce_map()
    make_root(host, ce={name: content}, xbox={"sharecreek": xbox_map_named(xbox_source(), "sharecreek")})
    make_root(guest)
    host_text, _ = run_pair("stock-peer", host, guest, level=name[:-4] + "@ce", seconds=40, bot=[], guest=False)
    assert "runs this game" not in host_text, share_lines(host_text)
    assert "was sent the list" not in host_text
    marked, _ = run_pair("marked-peer", host, guest, level=name[:-4] + "@ce", seconds=40, bot=["--ae-silent"],
                         guest=False)
    assert "runs this game: it gets this host's list" in marked, share_lines(marked)


def test_game_starts_during_get_all(roots):
    """the game starts while the guest fetches the host's maps: the guest is not held out, it plays, and the map
    being fetched stops"""
    host, guest = roots
    xbox, mod, ce = three_maps()
    # (a slow upload, so that the fetch is still going when the game starts)
    make_root(host, xbox=xbox, mod=mod, ce=ce)
    make_root(guest)
    host_text, guest_text = run_pair("start-mid-way", host, guest,
                                     host_env={"HALO_MOD": "sharemod", "HALO_NET_MAP_SHARE_UPLOAD": "256"},
                                     guest_env={}, start=22, seconds=75)
    assert "getting all of the host's maps" in guest_text, share_lines(guest_text)
    assert "Getting the host's maps was stopped" in guest_text, share_lines(guest_text)
    # (the guest is in the game, its own player playing)
    assert "| local 1 camera" in guest_text.split("getting all of the host's maps")[1]


# ---------- layouts travel with maps (0.3.5; port/linux/game/ae_layout_share_rules.h)
#
# AE_SHARE_LAYOUT_MAP: a map with a layout file beside it (<its folder>/layouts/<map>.txt) whose layout removes placed
# scenery, e.g. the H2 Demakes' h2d_salicreek.map (its fusion coils): copied for the test, never committed.

def layout_map():
    path = os.environ.get("AE_SHARE_LAYOUT_MAP")
    if not path:
        pytest.skip("needs AE_SHARE_LAYOUT_MAP (a map with layouts/<map>.txt beside it)")
    path = Path(path).expanduser()
    return path.stem, path.read_bytes(), (path.parent / "layouts" / f"{path.stem}.txt").read_bytes()


def layout_lines(text):
    return "\n".join(line for line in text.splitlines() if "layout" in line)


def removed_objects(text):
    import re
    found = re.findall(r"layout: (\d+) scenery and other objects? removed", text)
    return int(found[-1]) if found else None


def layout_travels(roots, name_of_run, guest_env, kept):
    host, guest = roots
    name, content, layout = layout_map()
    make_root(host, raw={f"maps/Pack/{name}.map": content, f"maps/Pack/layouts/{name}.txt": layout})
    make_root(guest, raw={f"maps/Pack/{name}.map": content})
    host_text, guest_text = run_pair(name_of_run, host, guest, guest_env=guest_env, level=name, start=40, seconds=80)
    assert f"{name} ({len(layout)} bytes) goes to its guests" in host_text, share_lines(host_text)
    assert "was sent this host's layout for" in host_text, share_lines(host_text)
    assert f"{name} came ({len(layout)} bytes)" in guest_text, share_lines(guest_text)
    assert f"layout: the host's layout for {name} is used ({len(layout)} bytes; this machine's own file is not there)" \
        in guest_text, layout_lines(guest_text)
    assert "layout: the host's layout (" in guest_text, layout_lines(guest_text)
    host_removed = removed_objects(host_text)
    assert host_removed and removed_objects(guest_text) == host_removed, layout_lines(guest_text)
    assert (guest / f"maps/Pack/layouts/{name}.txt").exists() == kept


def test_layout_travels_to_a_guest_with_the_map(roots):
    """a guest that has the map but not its layout plays the host's layout (the scenery it removes is gone there
    too), and with DOWNLOAD MAPS on (the player's yes) keeps it beside the map"""
    layout_travels(roots, "layout-travels", None, True)


def test_layout_is_not_kept_with_downloads_off(roots):
    """the same with DOWNLOAD MAPS off: the layout is played from memory and nothing is written"""
    layout_travels(roots, "layout-travels-off", {"HALO_NET_MAP_DOWNLOAD": "false"}, False)


def test_layout_comes_with_a_downloaded_map(roots):
    """a guest without the map downloads it (yes), and the host's layout is kept beside it in maps_shared/layouts and
    applied"""
    host, guest = roots
    name, content, layout = layout_map()
    make_root(host, raw={f"maps/Pack/{name}.map": content, f"maps/Pack/layouts/{name}.txt": layout})
    make_root(guest)
    host_text, guest_text = run_pair("layout-download", host, guest, guest_env=YES, level=name, start=150, seconds=200)
    assert (guest / f"maps_shared/{name}.map").exists(), share_lines(guest_text)
    assert (guest / f"maps_shared/layouts/{name}.txt").read_bytes() == layout, layout_lines(guest_text)
    assert "the host's layout kept in" in guest_text, layout_lines(guest_text)
    assert removed_objects(guest_text) == removed_objects(host_text) and removed_objects(host_text), \
        layout_lines(guest_text)


def test_layout_of_a_guest_that_differs_is_kept(roots):
    """a guest's own layout that differs from the host's is never overwritten: the host's goes beside it as
    <map>.host.txt, and the host's is the one played"""
    host, guest = roots
    name, content, layout = layout_map()
    mine = layout.split(b"\n")[0] + b"\n# this guest's own\n"
    make_root(host, raw={f"maps/Pack/{name}.map": content, f"maps/Pack/layouts/{name}.txt": layout})
    make_root(guest, raw={f"maps_shared/layouts/{name}.txt": mine})
    host_text, guest_text = run_pair("layout-differs", host, guest, guest_env=YES, level=name, start=150, seconds=200)
    assert (guest / f"maps_shared/{name}.map").exists(), share_lines(guest_text)
    assert (guest / f"maps_shared/layouts/{name}.txt").read_bytes() == mine
    assert (guest / f"maps_shared/layouts/{name}.host.txt").read_bytes() == layout, layout_lines(guest_text)
    assert "this machine's own file differs" in guest_text, layout_lines(guest_text)
    assert removed_objects(guest_text) == removed_objects(host_text), layout_lines(guest_text)


def test_host_without_a_layout(roots):
    """a host without a layout for the map: its guest plays none, though it has a file of its own (left as it is)"""
    host, guest = roots
    name, content, layout = layout_map()
    make_root(host, raw={f"maps/Pack/{name}.map": content})
    make_root(guest, raw={f"maps/Pack/{name}.map": content, f"maps/Pack/layouts/{name}.txt": layout})
    host_text, guest_text = run_pair("layout-none", host, guest, level=name, start=40, seconds=80)
    assert f"this host has no layout for levels\\test\\{name}" in host_text, share_lines(host_text)
    assert "map sharing: the host has no layout for" in guest_text, share_lines(guest_text)
    assert f"layout: the host has no layout for {name}: none applied" in guest_text, layout_lines(guest_text)
    assert removed_objects(guest_text) is None, layout_lines(guest_text)
    assert (guest / f"maps/Pack/layouts/{name}.txt").read_bytes() == layout


if __name__ == "__main__":
    sys.exit(pytest.main(["-q", "-x", *sys.argv[1:], __file__]))
