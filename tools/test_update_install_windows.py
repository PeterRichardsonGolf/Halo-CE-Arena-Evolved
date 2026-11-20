"""(AE) The self-updater from end to end on Windows: tools/test_update_install.py's Arena Evolved cases with the
Windows side, port/windows/src/win32_update.c (WinHTTP, the running executable renamed and replaced, the new one
started with CreateProcess), where test_update_install.py has posix_update.c.

The programs are tools/update_install_check.c built for 64-bit Windows with clang (an MSVC target, as the game's
windows64 build), SDL 3's Visual C++ package from the game's own Windows build (build/windows/third_party), and
the same test key, server and releases as the Linux test. WinHTTP trusts only the machine's certificate
authorities, so the test's authority is added to the machine's Root store for the run and taken out after: only
where AE_UPDATE_TEST_TRUST_CA=1 says the machine may be changed so (the CI runner, ninja windows64's job); it is
skipped elsewhere.

    AE_UPDATE_TEST_TRUST_CA=1 python -m pytest -q tools/test_update_install_windows.py
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import zipfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import update_sign  # noqa: E402
from test_update_install import (MBEDTLS, MONOCYPHER, NEW, OLD, REPOSITORY, ZLIB_SOURCES, Server, add,  # noqa: E402
                                 compile_objects, has, keys)

pytestmark = pytest.mark.skipif(os.name != "nt", reason="the Windows updater (win32_update.c)")

PLATFORM = "windows64"
EXE = "halo.exe"
AE_ASSET = f"arena-evolved-{NEW}-{PLATFORM}.zip"
TARGET = ["--target=x86_64-pc-windows-msvc", "-DHALO_64BIT", "-D_CRT_SECURE_NO_WARNINGS"]
LIBRARIES = ["-lwinhttp", "-lbcrypt", "-lws2_32", "-ladvapi32", "-lshell32", "-luser32", "-lkernel32"]


def sdl():
    """SDL 3's Visual C++ package, as the game's Windows build fetched it"""
    found = sorted((ROOT / "build/windows/third_party").glob("SDL3-*/lib/x64/SDL3.lib"))
    if not found:
        pytest.skip("needs SDL 3's Visual C++ package (the windows64 build fetches it)")
    return found[-1].parent.parent.parent


@pytest.fixture(scope="module")
def builds(keys, tmp_path_factory):
    """{version: the update test's program}, and SDL3.dll beside them"""
    cc = shutil.which("clang")
    if not cc:
        pytest.skip("no clang")
    package = sdl()
    directory = tmp_path_factory.mktemp("update_install_windows")
    flags = [*TARGET, "-std=gnu11", "-O1", "-w"]
    library = compile_objects(cc, [*flags, f"-I{MBEDTLS / 'include'}", f"-I{MBEDTLS / 'library'}"],
                              sorted((MBEDTLS / "library").glob("*.c")), directory)
    library += compile_objects(cc, [*flags, "-DZ_PREFIX", "-Dz_errmsg=z_port_errmsg"],
                               [ROOT / "port/third_party/zlib" / name for name in ZLIB_SOURCES], directory)
    library += compile_objects(cc, flags, [ROOT / "port/third_party/monocypher" / name for name in MONOCYPHER],
                               directory)
    key = ",".join(f"0x{byte:02x}" for byte in keys[0][1])
    programs = {}
    for version, defines in ((OLD, ["-DHALO_UPDATE_TEST", f"-DHALO_UPDATE_TEST_KEY={key}"]),
                             (NEW, ["-DHALO_UPDATE_TEST", f"-DHALO_UPDATE_TEST_KEY={key}"]),
                             ("dev", ["-DHALO_RELEASE_BUILD=0"])):
        program = directory / f"update-{version}.exe"
        built = subprocess.run(
            [cc, *TARGET, "-std=gnu11", "-O1", "-Wall", "-Wno-unused-function",
             f'-DHALO_VERSION="{OLD if version == "dev" else version}"', *defines, "-DZ_PREFIX",
             "-Dz_errmsg=z_port_errmsg", "-Iport/linux/src", "-iquote", "port/linux/include",
             "-Iport/third_party/zlib", "-Iport/third_party/monocypher", f"-I{MBEDTLS / 'include'}",
             f"-I{package / 'include'}", "-o", str(program), "tools/update_install_check.c",
             "port/windows/src/win32_update.c", "port/linux/src/update_manifest.c",
             "port/linux/src/update_signature.c", *library, str(package / "lib/x64/SDL3.lib"), *LIBRARIES],
            capture_output=True, text=True, cwd=ROOT)
        assert built.returncode == 0, built.stderr
        programs[version] = program
    programs["SDL3.dll"] = package / "lib/x64/SDL3.dll"
    return programs


@pytest.fixture(scope="module")
def trusted(tmp_path_factory):
    """the test authority in the machine's Root store while the module runs (AE_UPDATE_TEST_TRUST_CA=1)"""
    if os.environ.get("AE_UPDATE_TEST_TRUST_CA") != "1":
        pytest.skip("WinHTTP needs the test authority in the machine's store: AE_UPDATE_TEST_TRUST_CA=1 allows it")
    added = []

    def trust(authority):
        if authority not in added:
            subprocess.run(["certutil", "-addstore", "-f", "Root", str(authority)], check=True, capture_output=True)
            added.append(authority)
    yield trust
    subprocess.run(["certutil", "-delstore", "Root", "update test authority"], capture_output=True)


@pytest.fixture
def server(keys, tmp_path, trusted):
    directory = tmp_path / "server"
    directory.mkdir()
    running = Server(directory)
    trusted(running.authority)
    # (an address, not localhost: WinHTTP may try ::1 first, and the server listens on 127.0.0.1)
    running.url = running.url.replace("localhost", "127.0.0.1")
    yield running
    running.httpd.shutdown()


def tree(folder):
    """every file under folder, by its path in it: its SHA-256"""
    return {path.relative_to(folder).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in folder.rglob("*") if path.is_file()}


def install_ae(tmp_path, builds, version=OLD):
    """the old version's folder as an AE release has it, with the player's own files an update never touches"""
    folder = tmp_path / "game"
    folder.mkdir()
    shutil.copy(builds[version], folder / EXE)
    shutil.copy(builds["SDL3.dll"], folder / "SDL3.dll")
    (folder / "SDL3-LICENSE.txt").write_bytes(b"old data\n")
    for name, data in (("packs/ae_battle_rifle.aepack", b"old pack\n"), ("voices/cori/and.wav", b"old voice\n"),
                       ("sounds/horn.wav", b"old horn\n"), ("maps/bloodgulch.map", b"the player's map\n"),
                       ("data/config.toml", b"[update]\nauto = true\n")):
        (folder / name).parent.mkdir(parents=True, exist_ok=True)
        (folder / name).write_bytes(data)
    return folder


def ae_release(tmp_path, builds, keys, key_index=0):
    """the new version's release as the release kit makes it, updates.json signed"""
    release = tmp_path / "release"
    release.mkdir(parents=True, exist_ok=True)
    zip_path = release / AE_ASSET
    with zipfile.ZipFile(zip_path, "w") as archive:
        add(archive, EXE, builds[NEW].read_bytes())
        add(archive, "SDL3.dll", builds["SDL3.dll"].read_bytes())
        add(archive, "SDL3-LICENSE.txt", b"new data\n")
        archive.writestr(zipfile.ZipInfo("packs/"), b"")
        add(archive, "packs/ae_battle_rifle.aepack", b"new pack\n")
        add(archive, "voices/cori/and.wav", b"new voice\n")
        add(archive, "voices/aurora/and.wav", b"a new voice\n")
        add(archive, "sounds/horn.wav", b"new horn\n")
        add(archive, "maps/bloodgulch.map", b"never installed\n")
        add(archive, "data/config.toml", b"never installed\n")
    others = []
    for name in (f"arena-evolved-{NEW}-linux64.zip", f"arena-evolved-{NEW}-android-unsigned.zip"):
        (release / name).write_bytes(b"not this platform's")
        others.append(release / name)
    update_sign.manifest(keys[key_index][0], NEW, f"v{NEW}", f"v{NEW}-debug", "0" * 40, [zip_path, *others])
    listed = json.loads((release / "updates.json").read_text())["files"]
    assert (PLATFORM, AE_ASSET) in [(entry["platform"], entry["name"]) for entry in listed], listed
    return release, zip_path


def run(tmp_path, folder, server, finished, answer=None):
    """the old version, started as the player would; its log once finished's line ends in "done" """
    log = tmp_path / "update.log"
    log.write_text("")
    saves = tmp_path / "saves"
    saves.mkdir(exist_ok=True)
    environment = dict(os.environ, HALO_SAVE_ROOT=str(saves), HALO_UPDATE_TEST_SERVER=server.url,
                       HALO_UPDATE_TEST_LOG=str(log))
    environment.pop("HALO_UPDATE_TEST_ANSWER", None)
    if answer:
        environment["HALO_UPDATE_TEST_ANSWER"] = answer
    subprocess.run([str(folder / EXE)], cwd=folder, env=environment, timeout=120, capture_output=True)
    deadline = time.time() + 60
    while time.time() < deadline:
        lines = log.read_text().splitlines()
        if any(line.startswith(finished) and line.endswith(" done") for line in lines):
            return lines
        time.sleep(0.2)
    raise AssertionError("the update did not finish:\n" + log.read_text())


def test_ae_release_installed(keys, builds, server, tmp_path):
    """an AE release on Windows: the list's windows64 zip is installed (the running halo.exe replaced, SDL3.dll
    too, a pack replaced, a voice added) and nothing outside its folders; the new version starts and finds
    itself the latest; what the update left behind is gone by its next start"""
    folder = install_ae(tmp_path, builds)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{NEW} {EXE}")
    assert has(lines, f"{OLD} {EXE} update: version {NEW} is available (this is {OLD})")
    assert has(lines, f"{OLD} {EXE} update: the release's list names {AE_ASSET} (v{NEW}")
    assert has(lines, f"{OLD} {EXE} update: the download's SHA-256 is the release list's")
    assert has(lines, f"{OLD} {EXE} update: starting version {NEW} ({EXE})")
    assert has(lines, f"{NEW} {EXE} update: this is the latest version ({NEW})")
    assert has(lines, f"{OLD} {EXE} update: left out maps/bloodgulch.map")
    after = tree(folder)
    assert after[EXE] == hashlib.sha256(builds[NEW].read_bytes()).hexdigest()
    assert (folder / "packs/ae_battle_rifle.aepack").read_bytes() == b"new pack\n"
    assert (folder / "voices/cori/and.wav").read_bytes() == b"new voice\n"
    assert (folder / "voices/aurora/and.wav").read_bytes() == b"a new voice\n"
    assert (folder / "sounds/horn.wav").read_bytes() == b"new horn\n"
    assert (folder / "maps/bloodgulch.map").read_bytes() == b"the player's map\n"
    assert (folder / "data/config.toml").read_bytes() == b"[update]\nauto = true\n"
    # (the old halo.exe can still be running as the new one starts: its .old may wait for one more start)
    if any(name.endswith(".old") for name in after) or (folder / "update.partial").exists():
        server.latest = NEW
        run(tmp_path, folder, server, f"{NEW} {EXE}")
        after = tree(folder)
    assert not any(name.endswith(".old") for name in after), sorted(after)
    assert not (folder / "update.partial").exists()


def test_ae_no_newer_release(keys, builds, server, tmp_path):
    """the newest release is this version: nothing is asked, downloaded or changed"""
    folder = install_ae(tmp_path, builds)
    before = tree(folder)
    server.latest = OLD
    server.release(OLD, tmp_path, [])
    lines = run(tmp_path, folder, server, f"{OLD} {EXE}")
    assert has(lines, f"{OLD} {EXE} update: this is the latest version ({OLD})")
    assert not has(lines, "is available")
    assert server.requests == [f"/repos/{REPOSITORY}/releases?per_page=10"], server.requests
    assert tree(folder) == before


@pytest.mark.parametrize("answer", ["no", "never"])
def test_ae_answer_no(keys, builds, server, tmp_path, answer):
    """a newer release, and the player says no or do not ask again: nothing downloaded or changed"""
    folder = install_ae(tmp_path, builds)
    before = tree(folder)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{OLD} {EXE}", answer=answer)
    assert has(lines, f"{OLD} {EXE} update: answering {answer}")
    assert has(lines, "config: update.auto = false written") == (answer == "never")
    assert server.requests == [f"/repos/{REPOSITORY}/releases?per_page=10"], server.requests
    assert tree(folder) == before


@pytest.mark.parametrize("change", ["zip", "other-key"])
def test_ae_bad_signature_refused(keys, builds, server, tmp_path, change):
    """a zip changed after the list was signed, or a list another key signed: refused, the folder as it was"""
    folder = install_ae(tmp_path, builds)
    before = tree(folder)
    release, zip_path = ae_release(tmp_path, builds, keys, key_index=1 if change == "other-key" else 0)
    if change == "zip":
        data = bytearray(zip_path.read_bytes())
        data[len(data) // 2] ^= 0x01
        zip_path.write_bytes(bytes(data))
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{OLD} {EXE}")
    assert has(lines, f"{OLD} {EXE} update: failed: ")
    assert not has(lines, "starting version") and not any(line.startswith(NEW) for line in lines)
    assert tree(folder) == before


def test_ae_dev_build_never_checks(keys, builds, server, tmp_path):
    """a build that is not a release's asks nothing of any server, though a newer release is there"""
    folder = install_ae(tmp_path, builds, version="dev")
    before = tree(folder)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{OLD} {EXE}")
    assert has(lines, f"{OLD} {EXE} started") and has(lines, f"{OLD} {EXE} done")
    assert not has(lines, "update:"), lines
    assert server.requests == []
    assert tree(folder) == before


def test_ae_junction_refused(keys, builds, server, tmp_path):
    """the player's packs\\ is a junction to a folder elsewhere: the update is refused whole, before any file is
    replaced, and nothing is written in the folder it points to"""
    folder = install_ae(tmp_path, builds)
    elsewhere = tmp_path / "elsewhere"
    elsewhere.mkdir()
    (elsewhere / "ae_battle_rifle.aepack").write_bytes(b"outside\n")
    shutil.rmtree(folder / "packs")
    subprocess.run(["cmd", "/c", "mklink", "/J", str(folder / "packs"), str(elsewhere)], check=True,
                   capture_output=True)
    before = tree(folder)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{OLD} {EXE}")
    assert has(lines, "goes through a link: nothing installed"), lines
    assert not has(lines, "starting version")
    assert (elsewhere / "ae_battle_rifle.aepack").read_bytes() == b"outside\n"
    assert sorted(path.name for path in elsewhere.iterdir()) == ["ae_battle_rifle.aepack"]
    assert {name: value for name, value in tree(folder).items() if not name.startswith("update.partial/")} == before
