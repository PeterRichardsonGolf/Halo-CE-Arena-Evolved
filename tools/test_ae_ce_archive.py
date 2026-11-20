"""The community map archive's library (port/linux/src/ae_ce_archive.c) against made-up archives on 127.0.0.1: a list
signed with a key made here (tools/tests/ae_ce_archive_driver.c signs it), made-up Custom Edition maps, and a local
server that also misbehaves: a list with a bad signature, one cut short, one claiming a million entries or a gigabyte
of bytes, names with paths in them, a map whose bytes differ from the signed hash, one larger than its listed size
and than the hard cap, a redirect to another host, a stall, a small disk, a cancel in the middle of a file, a map
already there. Nothing here reaches the real archive: the library takes only this machine's address, and the test
server is the only one that is asked."""
import hashlib
import http.server
import os
from pathlib import Path
import shutil
import socketserver
import subprocess
import threading
import time

import pytest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "port/linux/src"
ZLIB = ROOT / "port/third_party/zlib"
MONOCYPHER = ROOT / "port/third_party/monocypher"
ZLIB_SOURCES = ["adler32.c", "crc32.c", "inffast.c", "inflate.c", "inftrees.c", "zutil.c"]
AGENT = "HaloMapClient/1"


@pytest.fixture(scope="module")
def driver(tmp_path_factory):
    cc = shutil.which("clang") or shutil.which("cc") or shutil.which("gcc")
    if not cc:
        pytest.skip("no C compiler")
    work = tmp_path_factory.mktemp("ce-archive")
    objects = []
    for source in [*(ZLIB / name for name in ZLIB_SOURCES), MONOCYPHER / "monocypher.c",
                   MONOCYPHER / "monocypher-ed25519.c"]:
        obj = work / (source.name + ".o")
        flags = ["-DZ_PREFIX", "-Dz_errmsg=z_port_errmsg"] if source.parent == ZLIB else []
        result = subprocess.run([cc, "-c", "-O1", "-w", *flags, str(source), "-o", str(obj)], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        objects.append(obj)
    for source in [SRC / "ae_ce_archive.c", SRC / "ae_ce_archive_rules.c", SRC / "ae_marks_file.c", SRC / "map_archive.c",
                   ROOT / "tools/tests/ae_ce_archive_driver.c"]:
        obj = work / (source.name + ".o")
        warnings = ["-Wall", "-Wextra", "-Werror"]
        result = subprocess.run([cc, "-std=gnu11", "-c", "-O1", "-g", *warnings, "-DZ_PREFIX", "-Dz_errmsg=z_port_errmsg",
                                 f"-I{SRC}", f"-I{ZLIB}", f"-I{MONOCYPHER}", str(source), "-o", str(obj)],
                                capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        objects.append(obj)
    exe = work / "ae_ce_archive_driver"
    result = subprocess.run([cc, *map(str, objects), "-lpthread", "-o", str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return exe


# ---------- the archive's side

def cache_map(stem, size=0x3000, seed=1):
    """a Custom Edition cache file: its header (daeh, version 609, length, tag data, name, toof) and bytes after it"""
    header = bytearray(0x800)
    header[0:4] = b"daeh"
    header[4:8] = (609).to_bytes(4, "little")
    header[8:12] = size.to_bytes(4, "little")
    header[0x10:0x14] = (0x800).to_bytes(4, "little")
    header[0x14:0x18] = (0x100).to_bytes(4, "little")
    header[0x20:0x20 + len(stem)] = stem.encode()
    header[0x7fc:0x800] = b"toof"
    body = bytes((i * seed + 3) & 255 for i in range(size - 0x800))
    return bytes(header) + body


def list_line(name, data=None, size=None, checksum="0badf00d"):
    data = data if data is not None else b"x"
    digest = hashlib.blake2b(data, digest_size=32).hexdigest()
    return f"{name}\t{checksum}\t{size if size is not None else len(data)}\t262144\tinfohash\tblake2b:{digest}\tshare:{digest}\tsrc\n"


def list_text(lines, serial=5):
    return f"# halo map torrents v1\n# serial {serial}\n" + "".join(lines)


class Archive:
    """a local server and its routes: bytes (a 200), or a function that writes its own answer; every request is kept"""

    def __init__(self):
        self.routes = {}
        self.requests = []
        archive = self

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *arguments):
                pass

            def do_GET(self):
                archive.requests.append((self.path, dict(self.headers)))
                route = archive.routes.get(self.path)
                if route is None:
                    self.send_response(404)
                    self.send_header("Content-Length", "0")
                    self.send_header("Connection", "close")
                    self.end_headers()
                elif callable(route):
                    try:
                        route(self)
                    except (BrokenPipeError, ConnectionResetError):
                        archive.broken += 1
                else:
                    self.send_response(200)
                    self.send_header("Content-Length", str(len(route)))
                    self.send_header("Connection", "close")
                    self.end_headers()
                    self.wfile.write(route)
                self.close_connection = True

        class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
            daemon_threads = True

        self.broken = 0
        self.server = Server(("127.0.0.1", 0), Handler)
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()

    def map_requests(self):
        return [path for path, _ in self.requests if path.startswith("/maps/")]


def raw(handler, head, body=b"", then=None):
    handler.connection.sendall(head.encode() + body)
    if then:
        then(handler)


@pytest.fixture
def archive():
    server = Archive()
    yield server
    server.close()


class World:
    """a maps folder, a cache folder, a signing key, and the driver run against an archive"""

    def __init__(self, driver, archive, tmp_path):
        self.driver = driver
        self.archive = archive
        self.maps = tmp_path / "custom_maps"
        self.cache = tmp_path / "cache"
        self.maps.mkdir()
        self.tmp = tmp_path
        self.marks = tmp_path / "maps_ce" / "downloaded_maps.txt"
        self.key = None
        self.env = dict(os.environ, AE_TEST_DATA_ROOT=str(tmp_path / "data"))

    def publish(self, text, seed=7, sign_text=None):
        """the list at the archive, signed with the key of that seed (the signature is over sign_text when given)"""
        listing = self.tmp / "list.txt"
        listing.write_text(sign_text if sign_text is not None else text)
        signature = self.tmp / "list.sig"
        result = subprocess.run([str(self.driver), "sign", str(seed), str(listing), str(signature)], capture_output=True,
                                text=True)
        assert result.returncode == 0
        if self.key is None:
            self.key = result.stdout.strip()
        self.archive.routes["/map_torrents.txt"] = text.encode()
        self.archive.routes["/map_torrents.txt.sig"] = signature.read_bytes()
        return result.stdout.strip()

    def run(self, *words, key=None, base=None, timeout=120, cache=None):
        result = subprocess.run([str(self.driver), base or self.archive.base, key or self.key, str(self.maps),
                                 (str(self.cache) if cache is None else cache), "marks", str(self.marks), *words], capture_output=True, text=True,
                                timeout=timeout, env=self.env)
        assert result.returncode == 0, result.stdout + result.stderr
        return [line for line in result.stdout.splitlines()], result.stderr

    def files(self):
        return sorted(path.name for path in self.maps.iterdir())


@pytest.fixture
def world(driver, archive, tmp_path):
    return World(driver, archive, tmp_path)


def fields(line):
    """key=value words of a driver line; the last key (error) takes the rest of the line"""
    result = {}
    text = line.split(" ", 1)[1] if " " in line else ""
    while text:
        key, _, rest = text.partition("=")
        if key == "error":
            result[key] = rest
            break
        value, _, text = rest.partition(" ")
        result[key] = value
    return result


def find(lines, prefix):
    found = [line for line in lines if line.startswith(prefix)]
    assert found, (prefix, lines)
    return fields(found[-1])


# ---------- a good list and map

def test_a_good_list_and_map_are_installed(world):
    data = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", data), list_line("Beta.map", cache_map("beta", 0x1000, 2))]))
    world.archive.routes["/maps/alpha.map"] = data
    lines, _ = world.run("start", "query", "", "query", "BET", "get", "alpha.map", "poll", "status")
    status = find(lines, "status")
    assert status["state"] == "READY" and status["network"] == "1" and status["count"] == "2" and status["serial"] == "5"
    assert "query rows=2 files=alpha.map,beta.map" in lines and "query rows=1 files=beta.map" in lines
    download = find(lines, "download")
    assert download["state"] == "DONE" and download["done"] == str(len(data)) and download["total"] == str(len(data))
    assert (world.maps / "alpha.map").read_bytes() == data
    assert world.files() == ["alpha.map"]
    assert "installed file=alpha.map" in lines
    assert world.archive.map_requests() == ["/maps/alpha.map"]
    # (the list and its signature are kept)
    assert (world.cache / "map_list.txt").exists() and (world.cache / "map_list.txt.sig").exists()


def test_the_request_names_no_install_key_and_no_id(world):
    data = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", data)]))
    world.archive.routes["/maps/alpha.map"] = data
    world.run("start", "get", "alpha.map")
    assert len(world.archive.requests) == 3
    for _, headers in world.archive.requests:
        names = {name.lower() for name in headers}
        assert names == {"host", "user-agent", "accept", "connection"}, names
        assert headers["User-Agent"] == AGENT
        assert headers["Host"].startswith("127.0.0.1:")


def test_the_kept_list_serves_when_the_archive_is_down(world):
    data = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", data)]))
    world.run("start")
    kept = (world.cache / "map_list.txt").read_bytes()
    for path in list(world.archive.routes):
        del world.archive.routes[path]
    lines, _ = world.run("start")
    status = find(lines, "status")
    assert status["state"] == "READY" and status["network"] == "0" and status["count"] == "1"
    assert (world.cache / "map_list.txt").read_bytes() == kept


def test_a_list_older_than_the_one_in_use_is_refused(world):
    world.publish(list_text([list_line("alpha.map")], serial=9))
    world.run("start")
    world.publish(list_text([list_line("alpha.map"), list_line("beta.map")], serial=4))
    kept = (world.cache / "map_list.txt").read_bytes()
    lines, _ = world.run("start")
    status = find(lines, "status")
    assert status["state"] == "READY" and status["serial"] == "9" and status["count"] == "1"
    assert "older" in status["error"]
    assert (world.cache / "map_list.txt").read_bytes() == kept


# ---------- lists that are not taken

def test_a_list_with_a_bad_signature_is_refused_whole(world):
    other = world.publish(list_text([list_line("alpha.map")]), seed=9)
    world.key = None
    good = subprocess.run([str(world.driver), "sign", "7", str(world.tmp / "list.txt"), str(world.tmp / "x.sig")],
                          capture_output=True, text=True).stdout.strip()
    assert good != other
    lines, _ = world.run("start", "status", key=good)
    status = find(lines, "status")
    assert status["state"] == "FAILED" and status["count"] == "0" and "not signed" in status["error"]
    assert not world.cache.exists()


def test_a_bad_network_list_leaves_the_kept_one(world):
    world.publish(list_text([list_line("alpha.map")]))
    world.run("start")
    kept = (world.cache / "map_list.txt").read_bytes()
    world.publish(list_text([list_line("alpha.map"), list_line("evil.map")], serial=99), sign_text="changed")
    lines, _ = world.run("start")
    status = find(lines, "status")
    assert status["state"] == "READY" and status["network"] == "0" and status["count"] == "1"
    assert "not signed" in status["error"]
    assert (world.cache / "map_list.txt").read_bytes() == kept


def test_a_signature_over_a_longer_list_does_not_verify_a_cut_one(world):
    text = list_text([list_line("alpha.map"), list_line("beta.map")])
    world.publish(text[:-60], sign_text=text)
    lines, _ = world.run("start")
    assert find(lines, "status")["state"] == "FAILED" and "not signed" in find(lines, "status")["error"]


def test_a_list_cut_short_on_the_wire_is_refused(world):
    text = list_text([list_line("alpha.map"), list_line("beta.map")]).encode()
    world.publish(text.decode())

    def cut(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(text)}\r\nConnection: close\r\n\r\n", text[:100])

    world.archive.routes["/map_torrents.txt"] = cut
    lines, _ = world.run("start")
    status = find(lines, "status")
    assert status["state"] == "FAILED" and "cut short" in status["error"]
    assert not world.cache.exists()


def test_a_list_of_a_million_entries_is_refused_before_it_is_read(world):
    world.publish(list_text([list_line("alpha.map")]))

    def million(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {1_000_000 * 190}\r\nConnection: close\r\n\r\n", b"#" * 1000)

    world.archive.routes["/map_torrents.txt"] = million
    lines, _ = world.run("start")
    status = find(lines, "status")
    assert status["state"] == "FAILED" and "larger than allowed" in status["error"]


def test_a_signed_list_with_too_many_maps_or_a_gigabyte_string_is_refused(world):
    many = list_text([list_line(f"m{i}.map") for i in range(20001)])
    assert len(many) < 8 * 1024 * 1024
    world.publish(many)
    lines, _ = world.run("start")
    status = find(lines, "status")
    assert status["state"] == "FAILED" and "more than" in status["error"]
    # a gigabyte long answer (the Content-Length alone: nothing is read)

    def gigabyte(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {1 << 30}\r\nConnection: close\r\n\r\n", b"x" * 1000)

    world.archive.routes["/map_torrents.txt"] = gigabyte
    lines, _ = world.run("start")
    assert find(lines, "status")["state"] == "FAILED"
    # a signed list holding one line of three kilobytes
    world.publish(list_text([list_line("alpha.map"), "z" * 3000 + "\n"]))
    lines, _ = world.run("start")
    status = find(lines, "status")
    assert status["state"] == "FAILED" and "longer" in status["error"]


def test_names_with_paths_and_the_game_own_maps_are_left_out(world):
    bad = ["../x.map", "a/b.map", "CON.map", "x.map.exe", "trail.map.", "space.map ", "a\\b.map", "c:x.map",
           "bloodgulch.map", "bitmaps.map", "longna~1.map", "trap_1a2b3c4d.map"]
    world.publish(list_text([list_line("good.map", cache_map("good"))] + [list_line(name) for name in bad]))
    lines, _ = world.run("start", "get", "../x.map", "get", "con.map", "get", "bloodgulch.map", "get", "a%2fb.map")
    status = find(lines, "status")
    assert status["state"] == "READY" and status["count"] == "1" and status["skipped"] == str(len(bad))
    assert lines.count("get refused: that is not a map file name") == 3
    assert "get refused: that map is not in the list" in lines
    assert world.archive.map_requests() == []


# ---------- maps that are not taken

def downloaded(world, data, route, name="alpha.map", size=None, words=("get",)):
    world.publish(list_text([list_line(name, data, size=size)]))
    world.archive.routes["/maps/" + name] = route
    lines, _ = world.run("start", *words, name)
    return lines


def test_a_map_whose_bytes_do_not_match_the_hash_is_not_kept(world):
    good = cache_map("alpha")
    changed = bytearray(good)
    changed[5000] ^= 1
    lines = downloaded(world, good, bytes(changed))
    download = find(lines, "download")
    assert download["state"] == "FAILED" and "BLAKE2b" in download["error"]
    assert world.files() == []


def test_a_map_that_is_not_a_cache_file_is_not_kept(world):
    junk = bytes(0x3000)
    lines = downloaded(world, junk, junk)
    download = find(lines, "download")
    assert download["state"] == "FAILED" and "not a cache file" in download["error"]
    assert world.files() == []


def test_a_map_larger_or_smaller_than_its_listed_size_is_refused(world):
    good = cache_map("alpha")
    for served in (good + b"extra", good[:-1]):
        lines = downloaded(world, good, served)
        download = find(lines, "download")
        assert download["state"] == "FAILED" and "list's size" in download["error"], download
        assert world.files() == []


def test_a_server_that_sends_more_than_it_said_is_read_to_its_length_only(world):
    good = cache_map("alpha")

    def long_body(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(good)}\r\nConnection: close\r\n\r\n", good + b"more" * 1000)

    lines = downloaded(world, good, long_body)
    assert find(lines, "download")["state"] == "DONE"
    assert (world.maps / "alpha.map").read_bytes() == good


def test_a_map_above_the_hard_cap_is_not_in_the_list_and_not_fetched(world):
    good = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", good), list_line("giant.map", good, size=401 * 1024 * 1024)]))
    lines, _ = world.run("start", "get", "giant.map")
    assert find(lines, "status")["count"] == "1" and "get refused: that map is not in the list" in lines
    # a server announcing more than the cap for a listed map
    world.archive.routes["/maps/alpha.map"] = lambda handler: raw(
        handler, f"HTTP/1.1 200 OK\r\nContent-Length: {500 * 1024 * 1024}\r\nConnection: close\r\n\r\n", good)
    lines, _ = world.run("start", "get", "alpha.map")
    assert "list's size" in find(lines, "download")["error"]
    assert world.files() == []


def test_a_redirect_to_another_host_is_not_followed(world):
    good = cache_map("alpha")

    def redirect(handler):
        raw(handler, "HTTP/1.1 302 Found\r\nLocation: http://other.invalid/maps/alpha.map\r\nContent-Length: 0\r\n"
                     "Connection: close\r\n\r\n")

    lines = downloaded(world, good, redirect)
    download = find(lines, "download")
    assert download["state"] == "FAILED" and "another address" in download["error"]
    assert world.archive.map_requests() == ["/maps/alpha.map"]
    assert world.files() == []


def test_a_chunked_or_encoded_answer_a_missing_map_and_an_error_are_refused(world):
    good = cache_map("alpha")
    chunked = lambda handler: raw(handler, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n",
                                  b"5\r\nhello\r\n0\r\n\r\n")
    encoded = lambda handler: raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(good)}\r\nContent-Encoding: gzip\r\n"
                                           "Connection: close\r\n\r\n", good)
    nolength = lambda handler: raw(handler, "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n", good)
    broken = lambda handler: raw(handler, "HTTP/1.1 500 Oops\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
    for route, words in ((chunked, "not the file's own bytes"), (encoded, "not the file's own bytes"),
                         (nolength, "did not say"), (broken, "answered 500")):
        lines = downloaded(world, good, route)
        assert words in find(lines, "download")["error"], words
    world.publish(list_text([list_line("alpha.map", good)]))
    del world.archive.routes["/maps/alpha.map"]
    lines, _ = world.run("start", "get", "alpha.map")
    assert "does not have it" in find(lines, "download")["error"]
    assert world.files() == []


def test_a_server_that_stalls_ends_the_download_and_leaves_nothing(world):
    good = cache_map("alpha", 0x20000)

    def stall(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(good)}\r\nConnection: close\r\n\r\n", good[:5000])
        time.sleep(6)

    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = stall
    started = time.time()
    lines, _ = world.run("start", "limits", "1000", "60000", "0", "get", "alpha.map")
    download = find(lines, "download")
    assert download["state"] == "FAILED" and "stopped sending" in download["error"]
    assert time.time() - started < 6
    assert world.files() == []


def test_a_server_that_never_answers_ends_at_the_stall_time(world):
    good = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = lambda handler: time.sleep(5)
    lines, _ = world.run("start", "limits", "1000", "60000", "0", "get", "alpha.map")
    assert "stopped answering" in find(lines, "download")["error"]
    assert world.files() == []


def test_a_slow_trickle_ends_at_the_deadline(world):
    good = cache_map("alpha", 0x20000)

    def trickle(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(good)}\r\nConnection: close\r\n\r\n")
        for start in range(0, len(good), 100):
            handler.connection.sendall(good[start:start + 100])
            time.sleep(0.2)

    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = trickle
    started = time.time()
    lines, _ = world.run("start", "limits", "5000", "1500", "0", "get", "alpha.map")
    assert "took too long" in find(lines, "download")["error"]
    assert time.time() - started < 8
    assert world.files() == []


def test_a_small_disk_refuses_before_a_byte_is_fetched(world):
    good = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = good
    lines, _ = world.run("start", "limits", "20000", "60000", "1048576", "get", "alpha.map")
    assert "get refused: there is not enough free disk space" in lines
    assert world.archive.map_requests() == [] and world.files() == []
    # room for the map and the margin: taken
    lines, _ = world.run("start", "limits", "20000", "60000", str(256 * 1024 * 1024 + len(good)), "get", "alpha.map")
    assert find(lines, "download")["state"] == "DONE"


def test_a_cancel_in_the_middle_of_a_file_leaves_no_file(world):
    good = cache_map("alpha", 3 * 1024 * 1024)

    def slow(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(good)}\r\nConnection: close\r\n\r\n")
        for start in range(0, len(good), 65536):
            handler.connection.sendall(good[start:start + 65536])
            time.sleep(0.05)

    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = slow
    lines, _ = world.run("start", "getcancel", "alpha.map", "300000", "poll")
    download = find(lines, "download")
    assert download["state"] == "CANCELLED" and int(download["done"]) < len(good)
    assert world.files() == []
    assert not any(line.startswith("installed") for line in lines)


def test_a_map_already_there_is_not_overwritten(world):
    good = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = good
    (world.maps / "alpha.map").write_bytes(b"mine")
    lines, _ = world.run("start", "get", "alpha.map")
    assert "get refused: a file of that name is in custom_maps already" in lines
    assert (world.maps / "alpha.map").read_bytes() == b"mine" and world.archive.map_requests() == []
    # only when the caller says so
    lines, _ = world.run("start", "getover", "alpha.map")
    assert find(lines, "download")["state"] == "DONE"
    assert (world.maps / "alpha.map").read_bytes() == good and world.files() == ["alpha.map"]


def test_a_map_put_there_while_it_downloads_is_not_overwritten(world):
    good = cache_map("alpha")

    def late(handler):
        (world.maps / "alpha.map").write_bytes(b"late")
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(good)}\r\nConnection: close\r\n\r\n", good)

    lines = downloaded(world, good, late)
    download = find(lines, "download")
    assert download["state"] == "FAILED" and "is in custom_maps" in download["error"]
    assert (world.maps / "alpha.map").read_bytes() == b"late" and world.files() == ["alpha.map"]


def test_installed_maps_are_marked_and_filtered(world):
    world.publish(list_text([list_line("alpha.map"), list_line("beta.map")]))
    (world.maps / "alpha.map").write_bytes(b"x")
    lines, _ = world.run("start", "query", "alpha")
    assert "query rows=1 files=alpha.map*" in lines


# ---------- the setting, the consent and the address

def test_nothing_is_asked_with_the_setting_off_or_no_consent(world):
    world.publish(list_text([list_line("alpha.map")]))
    lines, _ = world.run("config", "0", "1", "start", "config", "1", "0", "start", "get", "alpha.map")
    assert "start refused: browsing maps online is off (game.ce_archive)" in lines
    assert "start refused: the first-use consent is not given (game.ce_archive_consented)" in lines
    assert "get refused: the first-use consent is not given (game.ce_archive_consented)" in lines
    assert world.archive.requests == []


def test_only_this_machine_may_be_a_test_archive(world):
    world.publish(list_text([list_line("alpha.map")]))
    for base in ("http://192.0.2.1:80", "http://example.org", "https://127.0.0.1:1", "http://127.0.0.2:5", "ftp://127.0.0.1"):
        lines, _ = world.run("start", base=base)
        assert lines and lines[0].startswith("test_server refused"), (base, lines)
    lines, _ = world.run("start", key="zz")
    assert lines[0].startswith("test_server refused")


def test_an_archive_that_is_not_there_fails_and_says_so(world):
    world.publish(list_text([list_line("alpha.map")]))
    lines, _ = world.run("start", base="http://127.0.0.1:1")
    status = find(lines, "status")
    assert status["state"] == "FAILED" and "could not be reached" in status["error"]


def test_a_map_name_from_a_header_is_never_used(world):
    good = cache_map("alpha")

    def named(handler):
        raw(handler, f"HTTP/1.1 200 OK\r\nContent-Length: {len(good)}\r\nContent-Disposition: attachment; "
                     "filename=\"../../evil.map\"\r\nConnection: close\r\n\r\n", good)

    lines = downloaded(world, good, named)
    assert find(lines, "download")["state"] == "DONE"
    assert world.files() == ["alpha.map"]
    assert not (world.tmp / "evil.map").exists()


# ---------- what the security review found

def several(world, count):
    maps = {}
    for number in range(count):
        stem = f"m{number}"
        maps[stem] = cache_map(stem, 0x1000, number + 1)
        world.archive.routes[f"/maps/{stem}.map"] = maps[stem]
    world.publish(list_text([list_line(f"{stem}.map", data) for stem, data in maps.items()]))
    return maps


def test_a_map_is_marked_as_downloaded_before_it_has_its_name_and_without_a_poll(world):
    maps = several(world, 20)
    words = []
    for stem in maps:
        words += ["get", f"{stem}.map"]
    lines, _ = world.run("start", *words)
    assert sum(1 for line in lines if line.startswith("download state=DONE")) == 20
    # (no poll ran: more than the hook's sixteen, and the list is on disk for the next run)
    assert world.marks.read_text().split() == [f"{stem}@ce" for stem in maps]
    # each mark is a line of its own
    assert world.marks.read_text() == "".join(f"{stem}@ce\n" for stem in maps)
    # a map installed again is not listed again
    (world.maps / "m3.map").unlink()
    world.run("start", "get", "m3.map")
    assert world.marks.read_text().split().count("m3@ce") == 1 and len(world.marks.read_text().split()) == 20


def test_a_map_is_not_installed_when_its_mark_cannot_be_written(world):
    good = cache_map("alpha")
    lines = downloaded(world, good, good)
    assert find(lines, "download")["state"] == "DONE"
    (world.maps / "alpha.map").unlink()
    world.marks = world.tmp / "blocker" / "downloaded_maps.txt"
    (world.tmp / "blocker").write_text("a file where the folder should be")
    lines, _ = world.run("start", "get", "alpha.map")
    download = find(lines, "download")
    assert download["state"] == "FAILED" and "could not be written" in download["error"]
    assert world.files() == []


def test_a_full_list_of_downloaded_maps_refuses_the_download(world):
    good = cache_map("alpha")
    world.marks.parent.mkdir()
    world.marks.write_text("".join("y" * 60 + "\n" for _ in range(70000)))
    lines = downloaded(world, good, good)
    download = find(lines, "download")
    assert download["state"] == "FAILED" and "is full" in download["error"]
    assert world.files() == [] and world.marks.stat().st_size == 70000 * 61


def test_the_same_serial_with_other_bytes_and_an_older_one_are_refused_after_the_kept_copy_is_gone(world):
    world.publish(list_text([list_line("alpha.map")], serial=9))
    world.run("start")
    assert (world.cache / "highest_serial.txt").read_text().startswith("serial 9 ")
    # the kept copy cleared: the record still refuses an older list, and the same serial with other bytes
    shutil.rmtree(world.cache / "map_list.txt", ignore_errors=True)
    (world.cache / "map_list.txt").unlink()
    (world.cache / "map_list.txt.sig").unlink()
    world.publish(list_text([list_line("alpha.map"), list_line("beta.map")], serial=4))
    status = find(world.run("start")[0], "status")
    assert status["state"] == "FAILED" and "older than one seen" in status["error"]
    world.publish(list_text([list_line("evil.map")], serial=9))
    status = find(world.run("start")[0], "status")
    assert status["state"] == "FAILED" and "other bytes" in status["error"]
    # the same bytes again, and a higher serial, are taken
    world.publish(list_text([list_line("alpha.map")], serial=9))
    assert find(world.run("start")[0], "status")["state"] == "READY"
    world.publish(list_text([list_line("beta.map")], serial=10))
    status = find(world.run("start")[0], "status")
    assert status["state"] == "READY" and status["serial"] == "10"
    assert (world.cache / "highest_serial.txt").read_text().startswith("serial 10 ")


def test_a_serial_out_of_range_or_far_ahead_of_the_clock_is_refused(world):
    now = int(time.time())
    for serial in ("1000000000000000", "99999999999999999999999", "9" * 16, "999999999999999", now + 31 * 86400):
        world.publish(list_text([list_line("alpha.map")], serial=serial))
        status = find(world.run("start")[0], "status")
        assert status["state"] == "FAILED" and "serial" in status["error"], serial
    assert not (world.cache / "highest_serial.txt").exists()
    # up to 30 days ahead of the clock is taken (a clock a little slow), and the record then holds it
    world.publish(list_text([list_line("alpha.map")], serial=now + 29 * 86400))
    assert find(world.run("start")[0], "status")["state"] == "READY"
    # a real serial, in seconds, then a newer one
    shutil.rmtree(world.cache)
    world.publish(list_text([list_line("alpha.map")], serial=now))
    assert find(world.run("start")[0], "status")["state"] == "READY"


def test_a_damaged_or_oversized_serial_record_refuses_lists_and_a_missing_one_is_first_use(world):
    world.publish(list_text([list_line("alpha.map")], serial=9))
    world.run("start")
    record = world.cache / "highest_serial.txt"
    good = record.read_bytes()
    for damaged in (b"serial 9 ab", b"garbage", b"serial 9 " + b"z" * 64, b"x" * 300, b""):
        record.write_bytes(damaged)
        (world.cache / "map_list.txt").unlink(missing_ok=True)
        status = find(world.run("start")[0], "status")
        assert status["state"] == "FAILED" and "damaged" in status["error"], damaged
        assert str(world.tmp) not in status["error"] and "ce_archive/highest_serial.txt" in status["error"]
        assert record.read_bytes() == damaged
    record.unlink()
    assert find(world.run("start")[0], "status")["state"] == "READY"
    assert record.read_bytes() == good
    assert not list(world.cache.glob("*.part"))


def test_a_torn_line_or_a_long_planted_line_does_not_hide_or_join_a_mark(world):
    good = cache_map("alpha")
    world.marks.parent.mkdir()
    world.marks.write_text("foo@" + "z" * 159 + "\nalpha@ce-not")
    lines = downloaded(world, good, good)
    assert find(lines, "download")["state"] == "DONE"
    names = world.marks.read_text().split()
    assert names[-1] == "alpha@ce" and names[0].startswith("foo@z")
    assert world.marks.read_text().endswith("\nalpha@ce-not\nalpha@ce\n")


def test_the_test_archive_keeps_its_list_apart_from_the_real_one(world):
    world.publish(list_text([list_line("alpha.map")]))
    data = world.tmp / "data"
    data.mkdir()
    world.run("start", cache="")
    assert (data / "ce_archive_test" / "map_list.txt").exists()
    assert (data / "ce_archive_test" / "highest_serial.txt").exists()
    assert not (data / "ce_archive").exists()


def test_a_planted_link_at_the_temporary_name_is_not_written_through(world):
    good = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = good
    victim = world.tmp / "victim.txt"
    victim.write_text("mine")
    (world.maps / "alpha.map.part").symlink_to(victim)
    lines, _ = world.run("start", "get", "alpha.map")
    assert find(lines, "download")["state"] == "DONE" and victim.read_text() == "mine"
    (world.maps / "alpha.map").unlink()
    os.link(victim, world.maps / "alpha.map.part")
    lines, _ = world.run("start", "get", "alpha.map")
    assert find(lines, "download")["state"] == "DONE" and victim.read_text() == "mine"
    assert (world.maps / "alpha.map").read_bytes() == good


def test_a_map_is_replaced_only_while_no_map_is_loaded(world):
    good = cache_map("alpha")
    world.publish(list_text([list_line("alpha.map", good)]))
    world.archive.routes["/maps/alpha.map"] = good
    (world.maps / "alpha.map").write_bytes(b"mine")
    lines, _ = world.run("start", "maploaded", "1", "getover", "alpha.map")
    assert "get refused: a map is loaded: nothing is replaced now" in lines
    assert (world.maps / "alpha.map").read_bytes() == b"mine" and world.archive.map_requests() == []


def test_the_hosts_the_consent_card_names_are_the_ones_asked(world):
    lines = subprocess.run([str(world.driver), "hosts"], capture_output=True, text=True).stdout.splitlines()
    assert lines == ["hosts list=raw.githubusercontent.com files=halomaps.fractumseraph.net"]
    world.publish(list_text([list_line("alpha.map")]))
    lines, _ = world.run("hosts")
    assert lines == ["hosts list=127.0.0.1 files=127.0.0.1"]


def test_the_marks_list_refuses_by_the_size_it_would_have_and_ends_a_torn_last_line(world):
    good = cache_map("alpha")
    world.marks.parent.mkdir()
    # exactly the table's size already: one more name is over it; no file is left beside the list
    world.marks.write_text(("y" * 63 + "\n") * 65536)
    lines = downloaded(world, good, good)
    assert "is full" in find(lines, "download")["error"]
    assert world.marks.stat().st_size == 65536 * 64 and not list(world.marks.parent.glob("*.part"))
    # one name's room left (the name and its line feed): taken, and the list ends at the limit's edge
    world.marks.write_text(("y" * 63 + "\n") * 65535 + "y" * 54 + "\n")
    lines, _ = world.run("start", "get", "alpha.map")
    assert find(lines, "download")["state"] == "DONE"
    assert world.marks.stat().st_size == 65535 * 64 + 55 + 9
    # a torn last line is ended, not joined to the name
    (world.maps / "alpha.map").unlink()
    world.marks.write_text("foo@")
    lines, _ = world.run("start", "get", "alpha.map")
    assert find(lines, "download")["state"] == "DONE"
    assert world.marks.read_text() == "foo@\nalpha@ce\n"
