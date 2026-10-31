"""The desktop self-updater from end to end: a download and an install, not
just the list and the signatures (tools/test_update_manifest.py does those).

Two builds of the updater (tools/update_install_check.c: port/linux/src's
updater.c and posix_update.c whole, built for the test, HALO_UPDATE_TEST, with
a test key) are made, an old version and a new one. A local HTTPS server
answers as GitHub does (the latest release's JSON, the release's files behind
a redirect), with a certificate only the test's build trusts
(HALO_UPDATE_TEST_CA). The old build, installed in a folder of its own as
halo, then updates itself:

  - from a release with a signed list of update files (updates.json),
  - from a release without one (its zip's own .sig, as before the list),
  - not at all from a changed zip or a changed list, the folder left as it was,
  - from a zip that adds the game's executable by its later name, which
    arrives executable and is started (the list's "main"),
  - (AE) not at all when no release is newer, or when the player answers no
    or do not ask again (update.auto = false written), and never from a
    build that is not a release's nor the test's (it asks nothing of any
    server),
  - (AE) from an Arena Evolved release as the release kit makes it: its zip
    named by version (arena-evolved-<version>-<platform>.zip), which only
    the list names, with its folders packs/, voices/<voice>/ and sounds/
    installed and nothing outside them, the files an update leaves behind
    (<name>.old, update.partial/) gone at the new version's start.

The server answers the releases list (AE's releases are pre-releases, which
GitHub's releases/latest never returns: it answers 404 here, as GitHub does
for AE).

Linux only (the updater finds itself in /proc/self/exe); the 32-bit build
where the system's SDL 3 is the i386 one. It needs OpenSSL 3's openssl for
the test key and certificate."""
import hashlib
import http.server
import json
import os
import shutil
import ssl
import stat
import subprocess
import sys
import threading
import time
import zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import delta_table  # noqa: E402
import update_sign  # noqa: E402

pytestmark = pytest.mark.skipif(not sys.platform.startswith("linux"), reason="the updater installs on Linux")

OLD, NEW = "0.4.2-beta", "0.4.3-beta"
REPOSITORY = "PeterRichardsonGolf/Halo-CE-Arena-Evolved"
# (the updater's platform, as the test's build is made: abi())
PLATFORM = "linux64" if sys.maxsize > 2 ** 32 else "linux"
# the zip by the name the updater asks for without a list, and the one an AE release gives it
ASSET = f"arena-evolved-{PLATFORM}-release.zip"
AE_ASSET = f"arena-evolved-{NEW}-{PLATFORM}.zip"
MBEDTLS = ROOT / "port/third_party/mbedtls"
ZLIB_SOURCES = ("adler32.c", "crc32.c", "inffast.c", "inflate.c", "inftrees.c", "uncompr.c", "zutil.c")
MONOCYPHER = ("monocypher.c", "monocypher-ed25519.c")


def abi():
    """the host's SDL 3, else the i386 one with -m32"""
    found = subprocess.run(["pkg-config", "--exists", "sdl3"], capture_output=True).returncode == 0 \
        if shutil.which("pkg-config") else False
    if found:
        return ["-DHALO_64BIT"] if PLATFORM == "linux64" else []
    if Path("/usr/lib/i386-linux-gnu/libSDL3.so").exists() and PLATFORM == "linux":
        return ["-m32"]
    pytest.skip("needs SDL 3's development files")


def compile_objects(cc, flags, sources, directory):
    def build(source):
        target = directory / (source.parent.name + "-" + source.stem + ".o")
        done = subprocess.run([cc, *flags, "-c", "-o", str(target), str(source)], capture_output=True, text=True)
        assert done.returncode == 0, done.stderr
        return str(target)
    with ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        return list(pool.map(build, sources))


@pytest.fixture(scope="module")
def keys(tmp_path_factory):
    try:
        os.environ["OPENSSL"] = delta_table.openssl()
    except SystemExit:
        pytest.skip("needs OpenSSL 3's openssl")
    directory = tmp_path_factory.mktemp("update_key")
    return [(directory / f"{name}.pem", delta_table.keygen(directory / f"{name}.pem")) for name in ("key", "other")]


@pytest.fixture(scope="module")
def builds(keys, tmp_path_factory):
    """{version: the update test's program}"""
    cc = shutil.which("clang") or shutil.which("cc")
    if not cc:
        pytest.skip("no C compiler")
    target = abi()
    directory = tmp_path_factory.mktemp("update_install")
    library = compile_objects(cc, [*target, "-std=gnu11", "-O1", "-w", f"-I{MBEDTLS / 'include'}",
                                   f"-I{MBEDTLS / 'library'}"], sorted((MBEDTLS / "library").glob("*.c")), directory)
    library += compile_objects(cc, [*target, "-std=gnu11", "-O1", "-w", "-DZ_PREFIX", "-Dz_errmsg=z_port_errmsg"],
                               [ROOT / "port/third_party/zlib" / name for name in ZLIB_SOURCES], directory)
    library += compile_objects(cc, [*target, "-std=gnu11", "-O1", "-w"],
                               [ROOT / "port/third_party/monocypher" / name for name in MONOCYPHER], directory)
    key = ",".join(f"0x{byte:02x}" for byte in keys[0][1])
    programs = {}
    # (and, AE, a build that is neither a release's nor the test's, as every build of ours but a release's is)
    for version, defines in ((OLD, ["-DHALO_UPDATE_TEST", f"-DHALO_UPDATE_TEST_KEY={key}"]),
                             (NEW, ["-DHALO_UPDATE_TEST", f"-DHALO_UPDATE_TEST_KEY={key}"]),
                             ("dev", ["-DHALO_RELEASE_BUILD=0"])):
        program = directory / f"update-{version}"
        built = subprocess.run(
            [cc, *target, "-std=gnu11", "-O1", "-Wall", "-Werror", "-Wno-unused-function",
             f'-DHALO_VERSION="{OLD if version == "dev" else version}"', *defines, "-DZ_PREFIX",
             "-Dz_errmsg=z_port_errmsg",
             "-Iport/linux/src", "-iquote", "port/linux/include", "-Iport/third_party/zlib", "-Iport/third_party/monocypher",
             f"-I{MBEDTLS / 'include'}", "-o", str(program), "tools/update_install_check.c",
             "port/linux/src/posix_update.c", "port/linux/src/update_manifest.c",
             "port/linux/src/update_signature.c", *library, "-lSDL3", "-lpthread"],
            capture_output=True, text=True, cwd=ROOT)
        assert built.returncode == 0, built.stderr
        programs[version] = program
    return programs


class Server:
    """GitHub, as the updater sees it: the latest release's JSON, and the
    releases' files behind a redirect to another address"""

    def __init__(self, directory):
        authority = directory / "ca.pem"
        authority_key = directory / "ca.key"
        certificate = directory / "server.pem"
        key = directory / "server.key"
        openssl = os.environ["OPENSSL"]
        for command in (
                ["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-subj", "/CN=update test authority",
                 "-keyout", authority_key, "-out", authority],
                ["req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost", "-keyout", key,
                 "-out", directory / "server.csr"]):
            subprocess.run([openssl, *map(str, command)], check=True, capture_output=True)
        (directory / "san.ext").write_text("subjectAltName=DNS:localhost,IP:127.0.0.1\n")
        subprocess.run([openssl, "x509", "-req", "-in", str(directory / "server.csr"), "-CA", str(authority),
                        "-CAkey", str(authority_key), "-CAcreateserial", "-days", "2", "-extfile",
                        str(directory / "san.ext"), "-out", str(certificate)], check=True, capture_output=True)
        self.authority = authority
        self.files = {}
        self.requests = []
        server = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *arguments):
                pass

            def do_GET(self):
                server.requests.append(self.path)
                prefix = f"/{REPOSITORY}/releases/download/"
                if self.path == f"/repos/{REPOSITORY}/releases?per_page=10":
                    # (AE: GitHub's list, newest first: a draft newer than all, the release, a map pack's, older)
                    body = json.dumps([
                        {"tag_name": "v9.9.9-beta", "name": "draft", "draft": True, "prerelease": True},
                        {"tag_name": f"v{server.latest}", "name": server.latest, "draft": False, "prerelease": True,
                         "assets": [{"name": name.split("/", 1)[1]} for name in server.files]},
                        {"tag_name": "map-pack-1", "draft": False, "prerelease": False},
                        {"tag_name": "v0.1.0-beta", "draft": False, "prerelease": True}], indent=2).encode()
                elif self.path.startswith(prefix):
                    if self.path[len(prefix):] not in server.files:
                        self.send_error(404)
                        return
                    self.send_response(302)
                    self.send_header("Location", f"{server.url}/files/{self.path[len(prefix):]}")
                    self.send_header("Content-Length", "0")
                    self.end_headers()
                    return
                elif self.path.startswith("/files/") and self.path[7:] in server.files:
                    body = server.files[self.path[7:]]
                else:
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(str(certificate), str(key))
        self.httpd.socket = context.wrap_socket(self.httpd.socket, server_side=True)
        self.url = f"https://localhost:{self.httpd.server_address[1]}"
        self.latest = NEW
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def release(self, version, directory, names):
        """the release's files (names, in directory) at v<version>"""
        self.files = {f"v{version}/{name}": (directory / name).read_bytes() for name in names}
        self.requests = []


@pytest.fixture
def server(keys, tmp_path):
    directory = tmp_path / "server"
    directory.mkdir()
    running = Server(directory)
    yield running
    running.httpd.shutdown()


def add(archive, name, data, mode=None):
    """a zip entry: made on Unix with mode, or on DOS without one"""
    info = zipfile.ZipInfo(name, date_time=(2026, 10, 16, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    if mode is None:
        info.create_system = 0
    else:
        info.create_system = 3
        info.external_attr = (stat.S_IFREG | mode) << 16
    archive.writestr(info, data)


def release_zip(directory, builds, extra=()):
    """the new version's zip: its halo, a library, the extra (name, data, mode)"""
    directory.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(directory / ASSET, "w") as archive:
        add(archive, "halo", builds[NEW].read_bytes(), 0o755)
        add(archive, "SDL3-LICENSE.txt", b"new data\n", 0o644)
        for name, data, mode in extra:
            add(archive, name, data, mode)
    return directory / ASSET


def install(tmp_path, builds):
    """the old version's folder: halo, and a file of its own mode"""
    folder = tmp_path / "game"
    folder.mkdir()
    shutil.copy(builds[OLD], folder / "halo")
    os.chmod(folder / "halo", 0o755)
    (folder / "SDL3-LICENSE.txt").write_bytes(b"old data\n")
    os.chmod(folder / "SDL3-LICENSE.txt", 0o600)
    return folder


def snapshot(folder):
    return {path.name: (hashlib.sha256(path.read_bytes()).hexdigest(), stat.S_IMODE(path.stat().st_mode))
            for path in folder.iterdir() if path.is_file()}


def run(tmp_path, folder, server, finished, answer=None, prefix=()):
    """the old version, started as the player would (answering answer, else
    yes); its log once a line has finished in it (the last program's
    "done"), or it times out"""
    log = tmp_path / "update.log"
    log.write_text("")
    home = tmp_path / "home"
    (home / "saves").mkdir(parents=True, exist_ok=True)
    environment = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "HOME": str(home),
                   "HALO_SAVE_ROOT": str(home / "saves"), "HALO_UPDATE_TEST_SERVER": server.url,
                   "HALO_UPDATE_TEST_CA": str(server.authority), "HALO_UPDATE_TEST_LOG": str(log)}
    if answer:
        environment["HALO_UPDATE_TEST_ANSWER"] = answer
    subprocess.run([*prefix, str(folder / "halo")], cwd=folder, env=environment, timeout=120, capture_output=True)
    deadline = time.time() + 60
    while time.time() < deadline:
        lines = log.read_text().splitlines()
        if any(line.startswith(finished) and line.endswith(" done") for line in lines):
            return lines
        time.sleep(0.2)
    raise AssertionError("the update did not finish:\n" + log.read_text())


def has(lines, text):
    return any(text in line for line in lines)


def test_update_from_the_list(keys, builds, server, tmp_path):
    """(a) the release's signed updates.json names the zip: it is installed,
    and the new version started in the same file, halo"""
    folder = install(tmp_path, builds)
    release = tmp_path / "release"
    zip_path = release_zip(release, builds)
    update_sign.manifest(keys[0][0], NEW, f"v{NEW}", f"v{NEW}-debug", "0" * 40, [zip_path])
    server.release(NEW, release, [ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{NEW} halo")
    assert has(lines, f"{OLD} halo update: the release's list names {ASSET}")
    assert has(lines, f"{OLD} halo update: the download's SHA-256 is the release list's")
    assert has(lines, f"{OLD} halo update: starting version {NEW} (halo)")
    assert has(lines, f"{NEW} halo update: this is the latest version ({NEW})")
    assert not any(path.endswith(".sig") and ASSET in path for path in server.requests)
    after = snapshot(folder)
    assert after["halo"][0] == hashlib.sha256(builds[NEW].read_bytes()).hexdigest()
    # (a file that was there keeps its mode; the .old files are gone at the new start)
    assert after["halo"][1] == 0o755 and after["SDL3-LICENSE.txt"][1] == 0o600
    assert (folder / "SDL3-LICENSE.txt").read_bytes() == b"new data\n"
    assert not any(name.endswith(".old") for name in after) and not (folder / "update.partial").exists()


def test_update_without_a_list(keys, builds, server, tmp_path):
    """(b) a release from before the list: its zip and the zip's .sig"""
    folder = install(tmp_path, builds)
    release = tmp_path / "release"
    zip_path = release_zip(release, builds)
    update_sign.sign(keys[0][0], NEW, [zip_path])
    server.release(NEW, release, [ASSET, f"{ASSET}.sig"])
    lines = run(tmp_path, folder, server, f"{NEW} halo")
    assert has(lines, f"{OLD} halo update: no list of update files")
    assert has(lines, f"{OLD} halo update: the download's signature checks")
    assert has(lines, f"{NEW} halo update: this is the latest version ({NEW})")
    assert snapshot(folder)["halo"][0] == hashlib.sha256(builds[NEW].read_bytes()).hexdigest()


@pytest.mark.parametrize("change", ["zip", "list", "zip-without-list", "other-key"])
def test_changed_update_refused(keys, builds, server, tmp_path, change):
    """(c) a zip or a list changed after it was signed (or signed by another
    key) is not installed: the folder is as it was, and the old version runs"""
    folder = install(tmp_path, builds)
    before = snapshot(folder)
    release = tmp_path / "release"
    zip_path = release_zip(release, builds)
    if change == "zip-without-list":
        update_sign.sign(keys[0][0], NEW, [zip_path])
        names = [ASSET, f"{ASSET}.sig"]
    else:
        update_sign.manifest(keys[1 if change == "other-key" else 0][0], NEW, f"v{NEW}", f"v{NEW}-debug", "0" * 40,
                             [zip_path])
        names = [ASSET, "updates.json", "updates.json.sig"]
    if change in ("zip", "zip-without-list"):
        data = bytearray(zip_path.read_bytes())
        data[len(data) // 2] ^= 0x01
        zip_path.write_bytes(bytes(data))
    elif change == "list":
        listed = release / "updates.json"
        listed.write_text(listed.read_text().replace(f'"size": {zip_path.stat().st_size}',
                                                     f'"size": {zip_path.stat().st_size + 1}'))
    server.release(NEW, release, names)
    lines = run(tmp_path, folder, server, f"{OLD} halo")
    assert has(lines, f"{OLD} halo update: failed: ")
    assert not has(lines, "starting version") and not any(line.startswith(NEW) for line in lines)
    if change in ("list", "other-key"):
        # (a list that is not the release's is no reason to take the zip without it)
        assert not any(path.startswith("/files/") and path.endswith(ASSET) for path in server.requests)
    after = snapshot(folder)
    assert after == before, (before, after)


def test_new_executable_started(keys, builds, server, tmp_path):
    """(d) a zip adds the game's executable by its later name (made on DOS:
    no mode in the zip), a tool marked executable in the zip and a text file:
    the two arrive executable, the text file not, and the list's main,
    chupathingy, is the program started"""
    folder = install(tmp_path, builds)
    release = tmp_path / "release"
    zip_path = release_zip(release, builds, [("chupathingy", builds[NEW].read_bytes(), None),
                                             ("helper-tool", b"#!/bin/sh\nexit 0\n", 0o755),
                                             ("notes-new.txt", b"notes\n", 0o644)])
    update_sign.manifest(keys[0][0], NEW, f"v{NEW}", f"v{NEW}-debug", "0" * 40, [zip_path])
    assert json.loads((release / "updates.json").read_text())["files"][0]["main"] == "chupathingy"
    server.release(NEW, release, [ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{NEW} chupathingy")
    assert has(lines, f"{OLD} halo update: starting version {NEW} (chupathingy)")
    assert has(lines, f"{NEW} chupathingy started")
    assert has(lines, f"{NEW} chupathingy update: this is the latest version ({NEW})")
    after = snapshot(folder)
    for name in ("chupathingy", "helper-tool"):
        assert after[name][1] & 0o111, (name, oct(after[name][1]))
    assert not after["notes-new.txt"][1] & 0o111
    assert after["halo"][1] == 0o755 and after["SDL3-LICENSE.txt"][1] == 0o600


# ---------- (AE) Arena Evolved's releases


def tree(folder):
    """every file under folder, by its path in it: its SHA-256 and mode"""
    return {path.relative_to(folder).as_posix(): (hashlib.sha256(path.read_bytes()).hexdigest(),
                                                  stat.S_IMODE(path.stat().st_mode))
            for path in folder.rglob("*") if path.is_file()}


def install_ae(tmp_path, builds):
    """the old version's folder as an AE release has it: halo, a weapon pack, a voice, the horn, and the
    player's own files (a map, settings in data/) that an update never touches"""
    folder = install(tmp_path, builds)
    for name, data in (("packs/ae_battle_rifle.aepack", b"old pack\n"), ("voices/cori/and.wav", b"old voice\n"),
                       ("sounds/horn.wav", b"old horn\n"), ("maps/bloodgulch.map", b"the player's map\n"),
                       ("data/config.toml", b"[update]\nauto = true\n")):
        (folder / name).parent.mkdir(parents=True, exist_ok=True)
        (folder / name).write_bytes(data)
    return folder


def ae_release(tmp_path, builds, keys, key_index=0):
    """the new version's release as the kit makes it: arena-evolved-<version>-<platform>.zip (with its folders,
    a folder entry of its own, a map and data/ that must never be installed, and a new voice), the other
    platforms' files beside it, and updates.json signed"""
    release = tmp_path / "release"
    release.mkdir(parents=True, exist_ok=True)
    zip_path = release / AE_ASSET
    with zipfile.ZipFile(zip_path, "w") as archive:
        add(archive, "halo", builds[NEW].read_bytes(), 0o755)
        add(archive, "SDL3-LICENSE.txt", b"new data\n", 0o644)
        archive.writestr(zipfile.ZipInfo("packs/"), b"")
        add(archive, "packs/ae_battle_rifle.aepack", b"new pack\n", 0o644)
        add(archive, "packs/ae_battle_rifle.CREDITS.txt", b"credits\n", 0o644)
        add(archive, "voices/cori/and.wav", b"new voice\n", 0o644)
        add(archive, "voices/aurora/and.wav", b"a new voice\n", 0o644)
        add(archive, "sounds/horn.wav", b"new horn\n", 0o644)
        add(archive, "maps/bloodgulch.map", b"never installed\n", 0o644)
        add(archive, "data/config.toml", b"never installed\n", 0o644)
    others = []
    for name in (f"arena-evolved-{NEW}-{PLATFORM}-arch.zip", f"arena-evolved-{NEW}-android-unsigned.zip",
                 f"arena-evolved-{NEW}-server-linux-x64.zip"):
        (release / name).write_bytes(b"not an update file")
        others.append(release / name)
    update_sign.manifest(keys[key_index][0], NEW, f"v{NEW}", f"v{NEW}-debug", "0" * 40, [zip_path, *others])
    listed = json.loads((release / "updates.json").read_text())["files"]
    assert [(entry["platform"], entry["name"]) for entry in listed] == [(PLATFORM, AE_ASSET)], listed
    return release, zip_path


def test_ae_release_installed(keys, builds, server, tmp_path):
    """(AE a) an AE release: the list names its versioned zip, whose folders are installed (a pack replaced,
    a voice added) and nothing outside them; the .old files and update.partial/ are gone at the new start"""
    folder = install_ae(tmp_path, builds)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{NEW} halo")
    assert has(lines, f"{OLD} halo update: version {NEW} is available (this is {OLD})")
    assert has(lines, f"{OLD} halo update: the release's list names {AE_ASSET} (v{NEW}")
    assert has(lines, f"{OLD} halo update: the download's SHA-256 is the release list's")
    assert has(lines, f"{OLD} halo update: starting version {NEW} (halo)")
    assert has(lines, f"{NEW} halo update: this is the latest version ({NEW})")
    assert has(lines, f"{NEW} halo update: ") and has(lines, "an update left behind deleted")
    assert has(lines, f"{OLD} halo update: left out maps/bloodgulch.map")
    assert not any(path.endswith("/releases/latest") for path in server.requests)
    after = tree(folder)
    assert after["halo"][0] == hashlib.sha256(builds[NEW].read_bytes()).hexdigest() and after["halo"][1] == 0o755
    assert (folder / "packs/ae_battle_rifle.aepack").read_bytes() == b"new pack\n"
    assert (folder / "packs/ae_battle_rifle.CREDITS.txt").read_bytes() == b"credits\n"
    assert (folder / "voices/cori/and.wav").read_bytes() == b"new voice\n"
    assert (folder / "voices/aurora/and.wav").read_bytes() == b"a new voice\n"
    assert (folder / "sounds/horn.wav").read_bytes() == b"new horn\n"
    assert (folder / "maps/bloodgulch.map").read_bytes() == b"the player's map\n"
    assert (folder / "data/config.toml").read_bytes() == b"[update]\nauto = true\n"
    assert not any(name.endswith(".old") for name in after), sorted(after)
    assert not (folder / "update.partial").exists()


def test_ae_no_newer_release(keys, builds, server, tmp_path):
    """(AE b) the newest release is this version: nothing is asked, downloaded or changed"""
    folder = install_ae(tmp_path, builds)
    before = tree(folder)
    server.latest = OLD
    server.release(OLD, tmp_path, [])
    lines = run(tmp_path, folder, server, f"{OLD} halo")
    assert has(lines, f"{OLD} halo update: this is the latest version ({OLD})")
    assert not has(lines, "is available") and not has(lines, "answering")
    assert server.requests == [f"/repos/{REPOSITORY}/releases?per_page=10"], server.requests
    assert tree(folder) == before


@pytest.mark.parametrize("answer", ["no", "never"])
def test_ae_answer_no(keys, builds, server, tmp_path, answer):
    """(AE c) a newer release, and the player says no (nothing, until the next start) or do not ask again
    (update.auto = false written): nothing is downloaded or changed either way"""
    folder = install_ae(tmp_path, builds)
    before = tree(folder)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{OLD} halo", answer=answer)
    assert has(lines, f"{OLD} halo update: version {NEW} is available")
    assert has(lines, f"{OLD} halo update: answering {answer}")
    assert has(lines, "config: update.auto = false written") == (answer == "never")
    assert server.requests == [f"/repos/{REPOSITORY}/releases?per_page=10"], server.requests
    assert tree(folder) == before


@pytest.mark.parametrize("change", ["zip", "other-key"])
def test_ae_bad_signature_refused(keys, builds, server, tmp_path, change):
    """(AE d) an AE release whose zip changed after the list was signed, or whose list another key signed:
    refused, the folder (its packs and voices too) as it was, and the old version still the one running"""
    folder = install_ae(tmp_path, builds)
    before = tree(folder)
    release, zip_path = ae_release(tmp_path, builds, keys, key_index=1 if change == "other-key" else 0)
    if change == "zip":
        data = bytearray(zip_path.read_bytes())
        data[len(data) // 2] ^= 0x01
        zip_path.write_bytes(bytes(data))
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{OLD} halo")
    assert has(lines, f"{OLD} halo update: failed: ")
    assert not has(lines, "starting version") and not any(line.startswith(NEW) for line in lines)
    if change == "other-key":
        assert has(lines, "the release's list of update files was not used")
        assert not any(path.startswith("/files/") and path.endswith(AE_ASSET) for path in server.requests)
    else:
        assert has(lines, "its SHA-256 is not the list's")
    assert tree(folder) == before


def test_ae_dev_build_never_checks(keys, builds, server, tmp_path):
    """(AE e) a build that is not a release's (a -dev build, AE Latest, a ci branch's): the updater asks nothing
    of any server and says nothing, though a newer release is there (and, where it can, run without a network
    at all, so that a broken switch could not get to GitHub either)"""
    folder = install_ae(tmp_path, builds)
    shutil.copy(builds["dev"], folder / "halo")
    before = tree(folder)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    isolated = shutil.which("unshare") and subprocess.run(["unshare", "-rn", "true"], capture_output=True).returncode == 0
    lines = run(tmp_path, folder, server, f"{OLD} halo", prefix=("unshare", "-rn") if isolated else ())
    assert has(lines, f"{OLD} halo started") and has(lines, f"{OLD} halo done")
    assert not has(lines, "update:"), lines
    assert server.requests == []
    assert tree(folder) == before


def test_ae_link_refused(keys, builds, server, tmp_path):
    """(AE f) the player's packs/ is a link to a folder elsewhere: the update is refused whole, before any file is
    replaced, and nothing is written in the folder the link points to"""
    folder = install_ae(tmp_path, builds)
    elsewhere = tmp_path / "elsewhere"
    elsewhere.mkdir()
    (elsewhere / "ae_battle_rifle.aepack").write_bytes(b"outside\n")
    shutil.rmtree(folder / "packs")
    os.symlink(elsewhere, folder / "packs")
    before = tree(folder)
    release, _ = ae_release(tmp_path, builds, keys)
    server.release(NEW, release, [AE_ASSET, "updates.json", "updates.json.sig"])
    lines = run(tmp_path, folder, server, f"{OLD} halo")
    assert has(lines, "goes through a link: nothing installed"), lines
    assert not has(lines, "starting version")
    assert (elsewhere / "ae_battle_rifle.aepack").read_bytes() == b"outside\n"
    assert sorted(path.name for path in elsewhere.iterdir()) == ["ae_battle_rifle.aepack"]
    # (the download unpacked in update.partial/ stays until the next start's clean-up)
    assert {name: value for name, value in tree(folder).items() if not name.startswith("update.partial/")} == before
