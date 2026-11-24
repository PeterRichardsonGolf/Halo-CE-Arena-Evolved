"""The Steam Workshop screen's Steam client (port/android/host/steam_*.c, the host's), built natively with -Wall -Wextra
-Werror, and its offline parts checked: its own SHA-1, HMAC, AES-256, CRC-32 and base64 against known
answers, the connection's envelope (which the seal outside Android also uses) refusing a changed byte, the session file
(the refresh token sealed, never written as it is where there is a key; another device's key opening nothing; a token
kept as it is where there was no key sealed at the next load; the file the owner's alone; forgetting it), the depots'
LZMA ("VZa"), Zstandard ("VSZa") and zip chunks, and the paths a Workshop item's files may take in its package.
Nothing here reaches Steam."""
import concurrent.futures
import hashlib
import hmac
import lzma
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import zipfile
import zlib

import pytest

ROOT = Path(__file__).resolve().parents[1]
ZLIB = ROOT / "port/third_party/zlib"
MBEDTLS = ROOT / "port/third_party/mbedtls"
# (as the Android host builds it, and everything that includes it: tools/android_build.py)
MBEDTLS_DEFINES = ["-DMBEDTLS_THREADING_C", "-DMBEDTLS_THREADING_PTHREAD"]
HOST = ROOT / "port/android/host"
STEAM_SOURCES = ["steam_cm.c", "steam_crypto.c", "steam_depot.c", "steam_net.c", "steam_seal.c", "steam_unpack.c",
                 # (an installed package's maps fingerprinted: steam_downloads.c)
                 "host_mcc_hash.c"]
TOKEN = "eyJhbGciOiJFZERTQSJ9.eyJzdWIiOiI3NjU2MTE5OTAwMDAwMDAwMSJ9.c2lnbmF0dXJl"


@pytest.fixture(scope="module")
def client(tmp_path_factory):
    clang = shutil.which("clang")
    if not clang:
        pytest.skip("clang required")
    work = tmp_path_factory.mktemp("steam-client")
    objects = []
    for source in ["posix_files.c", "posix_mcc_share.c", "workshop_authors.c", "server_json.c"]:
        objects.append(work / (source + ".o"))
        subprocess.run([clang, "-std=gnu99", "-w", "-D_GNU_SOURCE", "-c", str(ROOT / "port/linux/src" / source), "-o", str(objects[-1])],
                       check=True)
    for source in ["adler32.c", "crc32.c", "inffast.c", "inflate.c", "inftrees.c", "zutil.c"]:
        objects.append(work / (source + ".o"))
        subprocess.run([clang, "-std=gnu99", "-w", "-DZ_PREFIX", "-c", str(ZLIB / source), "-o", str(objects[-1])],
                       check=True)
    # (Mbed TLS, as the host has it: its crypto, in parallel)
    sources = sorted((MBEDTLS / "library").glob("*.c"))
    with concurrent.futures.ThreadPoolExecutor() as pool:
        list(pool.map(lambda source: subprocess.run(
            [clang, "-std=c99", "-O1", "-w", *MBEDTLS_DEFINES, "-I", str(MBEDTLS / "include"), "-I", str(MBEDTLS / "library"),
             "-c", str(source), "-o", str(work / ("mbedtls_" + source.name + ".o"))], check=True), sources))
    objects += [work / ("mbedtls_" + source.name + ".o") for source in sources]
    # (the desktop's Workshop picture decoder, which steam_downloads.c's preview thread calls)
    previews = [ROOT / "port/linux/src" / name for name in ("ae_steam_preview.c", "ae_steam_preview_rules.c")]
    for source in [*(HOST / name for name in STEAM_SOURCES), *previews, ROOT / "tools/tests/steam_client_driver.c"]:
        objects.append(work / (source.name + ".o"))
        # (-Wall -Wextra -Werror, but for the unpacker's third-party decoders; the driver
        # includes steam_downloads.c, so that is held to it too)
        warnings = ["-w"] if source.name == "steam_unpack.c" else ["-Wall", "-Wextra", "-Werror"]
        result = subprocess.run([clang, "-std=c11", "-D_GNU_SOURCE", "-DSTEAM_SEAL_TEST", *MBEDTLS_DEFINES, *warnings,
                                 "-I", str(HOST), "-I", str(ROOT / "port/android/include"),
                                 "-I", str(ROOT / "port/linux/src"), "-I", str(ZLIB), "-I", str(MBEDTLS / "include"),
                                 "-c", str(source), "-o", str(objects[-1])], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
    exe = work / "steam_client"
    subprocess.run([clang, *map(str, objects), "-lpthread", "-o", str(exe)], check=True)
    return exe


def run(client, *arguments, secret=None):
    environment = dict(os.environ)
    environment.pop("STEAM_SEAL_TEST_SECRET", None)
    if secret is not None:
        environment["STEAM_SEAL_TEST_SECRET"] = secret
    result = subprocess.run([str(client), *map(str, arguments)], capture_output=True, env=environment)
    assert result.returncode == 0, result.stderr.decode(errors="replace")
    return result.stdout


def test_crypto_known_answers(client):
    lines = run(client, "crypto").decode().split("\n")
    assert lines[0] == hashlib.sha1(b"abc").hexdigest()
    assert lines[1] == hmac.new(b"key", b"The quick brown fox jumps over the lazy dog", hashlib.sha1).hexdigest()
    # (FIPS 197's appendix C.3: AES-256)
    assert lines[2] == "8ea2b7ca516745bfeafc49904b496089"
    assert lines[3] == "00112233445566778899aabbccddeeff"
    assert lines[4] == "hello world"
    assert lines[5] == f"{zlib.crc32(b'123456789'):08x}"


def test_envelope_refuses_a_changed_byte(client):
    assert run(client, "envelope").decode().split("\n")[:2] == ["round trip 1", "tampered 0"]


def session_file(root):
    return root / "steam" / "session.txt"


def test_token_sealed_with_the_devices_key(client, tmp_path):
    run(client, "save", tmp_path, "player", TOKEN, secret="device one")
    text = session_file(tmp_path).read_text()
    assert "account=player\n" in text
    assert "sealed=device:" in text
    assert TOKEN not in text and "token=" not in text
    assert stat.S_IMODE(session_file(tmp_path).stat().st_mode) == 0o600
    assert not (tmp_path / "steam" / "session.txt.part").exists()
    assert run(client, "load", tmp_path, secret="device one").decode() == f"account player\ntoken {TOKEN}\n"


def test_another_devices_copy_opens_nothing(client, tmp_path):
    run(client, "save", tmp_path, "player", TOKEN, secret="device one")
    assert run(client, "load", tmp_path, secret="device two").decode() == "account player\ntoken \n"
    assert run(client, "load", tmp_path).decode() == "account player\ntoken \n"


def test_a_damaged_seal_opens_nothing(client, tmp_path):
    run(client, "save", tmp_path, "player", TOKEN, secret="device one")
    text = session_file(tmp_path).read_text()
    sealed = text.split("sealed=device:")[1].split("\n")[0]
    changed = sealed[:-1] + ("0" if sealed[-1] != "0" else "1")
    session_file(tmp_path).write_text(text.replace(sealed, changed))
    assert run(client, "load", tmp_path, secret="device one").decode() == "account player\ntoken \n"


def test_without_a_key_never_written(client, tmp_path):
    # (unsealed, the token is not written: the sign-in is the run's alone)
    run(client, "save", tmp_path, "player", TOKEN)
    text = session_file(tmp_path).read_text()
    assert TOKEN not in text and "token=" not in text and "sealed=" not in text
    assert run(client, "load", tmp_path, secret="device one").decode() == "account player\ntoken \n"


def test_a_plain_token_line_is_not_taken(client, tmp_path):
    run(client, "save", tmp_path, "player", TOKEN, secret="device one")
    path = session_file(tmp_path)
    path.write_text(path.read_text().split("sealed=")[0] + f"token={TOKEN}\n")
    assert run(client, "load", tmp_path, secret="device one").decode() == "account player\ntoken \n"


def test_forgotten(client, tmp_path):
    run(client, "save", tmp_path, "player", TOKEN, secret="device one")
    # (a save left unfinished goes with it)
    (tmp_path / "steam" / "session.txt.part").write_text(f"sealed=x\n")
    run(client, "forget", tmp_path, secret="device one")
    assert not session_file(tmp_path).exists()
    assert not (tmp_path / "steam" / "session.txt.part").exists()


@pytest.mark.parametrize("result,kind", [(5, 0), (15, 0), (26, 0), (27, 0), (84, 1), (20, 1), (48, 1), (16, 1), (3, 1),
                                         (2, 2), (999, 2), (1, 2)])
def test_refused_logon_kind(client, result, kind):
    # (a dead token is forgotten; rate limits and other busy answers keep the saved sign-in)
    assert run(client, "logon_kind", result).decode().strip() == str(kind)


@pytest.mark.parametrize("count,after,left,words", [
    (1, 0, 60000, "Steam is busy: try again in 1 minute."),
    (2, 0, 120000, "Steam is busy: try again in 2 minutes."),
    (3, 0, 240000, "Steam is busy: try again in 4 minutes."),
    (6, 0, 600000, "Steam is busy: try again in 10 minutes."),   # (doubling stops at 10 minutes)
    (9, 0, 600000, "Steam is busy: try again in 10 minutes."),
    (1, 30000, 30000, "Steam is busy: try again in 30 seconds."),
    (1, 60000, 0, "Steam is busy: try again in 1 second."),   # (waited out: nothing left)
    (0, 0, 0, "Steam is busy: try again in 1 second."),       # (never busy: nothing to wait)
])
def test_busy_backoff(client, count, after, left, words):
    assert run(client, "backoff", count, after).decode().strip() == f"{left} {words}"


@pytest.mark.parametrize("total,free,verdict", [
    (300 << 20, 10 << 30, 0),             # (a usual map pack, room to spare)
    (int(2.73 * (1 << 30)), 10 << 30, 0), # (the CE Digsite Multiplayer item, 2.73 GB)
    (16 << 30, 100 << 30, 0),             # (the limit itself)
    ((16 << 30) + 1, 100 << 30, 1),       # (past it: too large, whatever the disk)
    (64 << 30, 100 << 30, 1),
    (300 << 20, 300 << 20, 2),            # (no room: the size and a margin are needed)
    (300 << 20, (300 << 20) + (64 << 20), 2),
    (300 << 20, (300 << 20) + (300 << 20) // 10 + (64 << 20), 0),
])
def test_item_size_policy(client, total, free, verdict):
    # (a Workshop item is refused past 16 GB, and when the disk has not its size and a margin free: 0 ok, 1 too large, 2 no room)
    assert run(client, "item_size", total, free).decode().strip() == str(verdict)


def test_session_file_sealed_line(client, tmp_path):
    sealed = tmp_path / "a.txt"
    sealed.write_text("account=x\nseed=00\nsealed=device:abc\n")
    plain = tmp_path / "b.txt"
    plain.write_text("account=x\nseed=00\n")
    # (a long line holding the word later is not a sealed line)
    tricky = tmp_path / "c.txt"
    tricky.write_text("account=" + "a" * 40 + "sealed=1\n")
    assert run(client, "has_sealed", sealed).decode().strip() == "1"
    assert run(client, "has_sealed", plain).decode().strip() == "0"
    assert run(client, "has_sealed", tricky).decode().strip() == "0"
    assert run(client, "has_sealed", tmp_path / "none.txt").decode().strip() == "0"


def test_log_text_has_no_control_characters(client):
    assert run(client, "log_clean", "a\nsteam: manifest halo1: x\r\tb\x7f").decode() == "a?steam: manifest halo1: x??b?\n"
    assert run(client, "log_clean", "plain Title 42").decode() == "plain Title 42\n"


def unpack(client, tmp_path, chunk, size):
    (tmp_path / "chunk").write_bytes(chunk)
    out = run(client, "unpack", tmp_path / "chunk", size)
    head, _, body = out.partition(b"\n")
    return head == b"unpacked 1", body


PLAIN = bytes(range(256)) * 64 + b"the end of a chunk"


def test_lzma_chunk(client, tmp_path):
    properties = {"id": lzma.FILTER_LZMA1, "dict_size": 1 << 16, "lc": 3, "lp": 0, "pb": 2}
    raw = lzma.compress(PLAIN, format=lzma.FORMAT_RAW, filters=[properties])
    props = bytes([(2 * 5 + 0) * 9 + 3]) + struct.pack("<I", 1 << 16)
    chunk = b"VZa" + b"\0" * 4 + props + raw + struct.pack("<II", zlib.crc32(PLAIN), len(PLAIN)) + b"zv"
    assert unpack(client, tmp_path, chunk, len(PLAIN)) == (True, PLAIN)
    assert unpack(client, tmp_path, chunk, len(PLAIN) + 1)[0] is False


def test_zstd_chunk(client, tmp_path):
    zstd = shutil.which("zstd")
    if not zstd:
        pytest.skip("zstd required")
    frame = subprocess.run([zstd, "-q", "-c"], input=PLAIN, capture_output=True, check=True).stdout
    chunk = b"VSZa" + b"\0" * 4 + frame + b"\0" * 15
    assert unpack(client, tmp_path, chunk, len(PLAIN)) == (True, PLAIN)


def test_zip_chunk(client, tmp_path):
    zipped = tmp_path / "chunk.zip"
    with zipfile.ZipFile(zipped, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("z", PLAIN)
    assert unpack(client, tmp_path, zipped.read_bytes(), len(PLAIN)) == (True, PLAIN)


def test_zip_chunk_claiming_more_than_its_size_is_refused(client, tmp_path):
    # (the central directory says 200 MB unpacked: refused against the chunk's own size before any of it is allocated)
    zipped = tmp_path / "chunk.zip"
    with zipfile.ZipFile(zipped, "w", zipfile.ZIP_STORED) as archive:
        archive.writestr("z", PLAIN)
    data = bytearray(zipped.read_bytes())
    central = data.rindex(b"PK\x01\x02")
    struct.pack_into("<I", data, central + 24, 200 << 20)
    assert unpack(client, tmp_path, bytes(data), len(PLAIN))[0] is False


def test_unknown_chunk_refused(client, tmp_path):
    assert unpack(client, tmp_path, b"XXXX" + PLAIN, len(PLAIN))[0] is False


@pytest.mark.parametrize("name,expected", [
    ("maps/Level.map", "level.map"),
    ("halo1/maps/bitmaps.map", "bitmaps.map"),
    ("ModInfo.json", "modinfo.json"),
    ("Images/Preview.PNG", "images/preview.png"),
    ("maps/sub/level.map", None),
    ("other/level.map", None),
    ("../escape.json", None),
    ("/rooted.json", None),
    ("a//b.json", None),
    ("c:/drive.json", None),
    ("readme.txt", None),
])
def test_item_paths(client, name, expected):
    assert run(client, "path", name).decode().strip() == (expected or "(refused)")


def http(client, tmp_path, answer, maximum=1 << 20):
    path = tmp_path / "answer.bin"
    path.write_bytes(answer)
    result = subprocess.run([str(client), "http", str(path), str(maximum)], capture_output=True)
    assert result.returncode == 0, result
    return result.stdout.decode(errors="replace")


CHUNKED = b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"


def test_chunked_body(client, tmp_path):
    assert http(client, tmp_path, CHUNKED + b"5\r\nhello\r\n6;x=1\r\n world\r\n0\r\n\r\n") == "ok 11 hello world\n"


@pytest.mark.parametrize("length", [b"FFFFFFFFFFFFFFF0", b"-10", b"200000", b"zz", b""])
def test_chunk_length_refused(client, tmp_path, length):
    # (after 16 bytes, a length of 2^64 - 16 wrapped the bounds checks to zero
    # and asked for an enormous copy; past maximum, negative, not hex or
    # missing: each refused, nothing copied)
    answer = CHUNKED + b"10\r\n" + b"y" * 16 + b"\r\n" + length + b"\r\n" + b"x" * 64 + b"\r\n0\r\n\r\n"
    assert http(client, tmp_path, answer).startswith("fail ")


def test_remove_tree_leaves_what_a_link_leads_to(client, tmp_path):
    # (a link inside a staged folder: the MCC helpers refuse it, and that
    # refusal stands, so what it leads to is neither followed nor removed)
    outside = tmp_path / "outside"
    outside.mkdir()
    (outside / "keep.txt").write_text("keep")
    staging = tmp_path / "mcc_staging" / "steam_1"
    (staging / "maps").mkdir(parents=True)
    (staging / "maps" / "a.map").write_bytes(b"map")
    (staging / "link").symlink_to(outside, target_is_directory=True)
    run(client, "remove_tree", staging)
    assert (outside / "keep.txt").read_text() == "keep"
    assert not (staging / "maps").exists()


def own_package(client, data, item):
    return run(client, "own_package", data, item).decode().split()


def make_package(data, folder, item=None, record=None):
    folder_path = data / "mcc_maps" / folder
    folder_path.mkdir(parents=True)
    if item is not None:
        (folder_path / "workshop_id.txt").write_text("%d\nTitle\n" % item)
    if record is not None:
        (data / "steam").mkdir(exist_ok=True)
        with open(data / "steam" / "downloads.txt", "a") as file:
            file.write("%d %d\n" % (record, 77))


def test_own_package_is_the_folder_named_by_the_id_with_a_record(client, tmp_path):
    make_package(tmp_path, "123", item=123, record=123)
    assert own_package(client, tmp_path, 123) == ["123", "1"]


def test_a_claim_through_a_sidecar_or_a_name_is_not_the_downloaders_package(client, tmp_path):
    # (a shared folder says it is item 456; a hand copy has no record; neither is one the game may remove or replace)
    make_package(tmp_path, "dl_00112233445566778899", item=456, record=456)
    make_package(tmp_path, "789", item=789)
    assert own_package(client, tmp_path, 456) == ["none", "0"]
    assert own_package(client, tmp_path, 789)[0] == "none"


def test_a_digits_folder_keeps_its_own_id_whatever_its_sidecar_says(client, tmp_path):
    make_package(tmp_path, "123", item=456, record=456)
    assert own_package(client, tmp_path, 456)[0] == "none"


def test_a_record_whose_folder_is_gone_is_dropped(client, tmp_path):
    make_package(tmp_path, "123", item=123, record=123)
    (tmp_path / "steam" / "downloads.txt").write_text("123 77\n555 77\n")
    assert own_package(client, tmp_path, 123) == ["123", "1"]
    assert (tmp_path / "steam" / "downloads.txt").read_text().split() == ["123", "77"]


def varint(value):
    out = b""
    while value > 127:
        out += bytes([(value & 127) | 128])
        value >>= 7
    return out + bytes([value])


def pb_bytes(field, data):
    return varint(field << 3 | 2) + varint(len(data)) + data


def pb_varint(field, value):
    return varint(field << 3) + varint(value)


def chunk_record(offset, size, packed=None, sha=b"\x11" * 20):
    return pb_bytes(6, pb_bytes(1, sha) + pb_varint(3, offset) + pb_varint(4, size) + pb_varint(5, packed or size))


def manifest_bytes(*mappings, metadata=pb_varint(4, 0)):
    payload = b"".join(pb_bytes(1, mapping) for mapping in mappings)
    return (struct.pack("<II", 0x71F617D0, len(payload)) + payload +
            struct.pack("<II", 0x1F4812BE, len(metadata)) + metadata + struct.pack("<II", 0x32C415AB, 0))


def mapping(name, size, *chunks):
    return pb_bytes(1, name) + pb_varint(2, size) + b"".join(chunks)


def parse(client, tmp_path, data):
    (tmp_path / "manifest").write_bytes(data)
    return run(client, "manifest", tmp_path / "manifest").decode()


def test_manifest_read(client, tmp_path):
    good = manifest_bytes(mapping(b"maps\\a.map", 3000, chunk_record(0, 1000), chunk_record(1000, 2000)),
                          mapping(b"b.json", 10, chunk_record(0, 10)))
    assert parse(client, tmp_path, good) == "ok 2 3\n"


@pytest.mark.parametrize("bad", [
    # a chunk past the file's end, and two that overlap
    mapping(b"a.map", 1500, chunk_record(1000, 1000)),
    mapping(b"a.map", 3000, chunk_record(0, 2000), chunk_record(1000, 1000)),
    # an offset that wraps, a chunk bigger than any download, a chunk with no hash
    mapping(b"a.map", 3000, chunk_record((1 << 64) - 10, 20)),
    mapping(b"a.map", 8 << 20, chunk_record(0, 8 << 20)),
    mapping(b"a.map", 10, pb_bytes(6, pb_varint(3, 0) + pb_varint(4, 10) + pb_varint(5, 10))),
    # a chunk record cut short inside (its nested message malformed)
    mapping(b"a.map", 10, pb_bytes(6, pb_bytes(1, b"\x11" * 20)[:-5])),
    # a file larger than the bounds
    mapping(b"a.map", 32 << 30),
    # a size with no chunks (the client would write that many zeros), chunks that do not cover the size (a short one, and
    # one with a gap)
    mapping(b"a.map", 1 << 30),
    mapping(b"a.map", 3000, chunk_record(0, 1000)),
    mapping(b"a.map", 3000, chunk_record(0, 1000), chunk_record(2000, 1000)),
])
def test_manifest_refused(client, tmp_path, bad):
    # (one bad file and the whole list is refused, not just that file left out)
    good = mapping(b"b.json", 10, chunk_record(0, 10))
    assert parse(client, tmp_path, manifest_bytes(good, bad)).startswith("fail ")


def test_manifest_malformed_metadata_refused(client, tmp_path):
    good = mapping(b"b.json", 10, chunk_record(0, 10))
    assert parse(client, tmp_path, manifest_bytes(good, metadata=varint(4 << 3 | 2) + varint(50))).startswith("fail ")
