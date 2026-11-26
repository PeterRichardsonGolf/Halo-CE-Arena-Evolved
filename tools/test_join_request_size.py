"""The join request's three shapes agree: the packet network_messages.c decodes (DEFINE_NETWORK_GAME_MESSAGE's
size), the client's struct (network_client_manager.c, which encodes it) and the host's (network_server_message_handler.c,
which it is decoded into).

Each struct is compiled as the game is (-fshort-wchar, 32-bit and 64-bit) with its file's own constants."""
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
MESSAGES = ROOT / "source/networking/network_messages.c"
CLIENT = ROOT / "source/networking/network_client_manager.c"
SERVER = ROOT / "source/networking/network_server_message_handler.c"


def packet_size():
    text = MESSAGES.read_text(errors="replace")
    size = re.search(r"DEFINE_NETWORK_GAME_MESSAGE\(message_client_join_game_request, ([^)]*)\)", text).group(1)
    return eval(size, {})  # (a sum of literals)


def struct_of(text):
    body = re.search(r"^struct message_client_join_game_request\n\{\n(.*?)^\};", text, re.S | re.M).group(1)
    constants = {}
    for name in re.findall(r"\b([A-Z_]{4,})\b", body):
        value = re.search(rf"\b{name}\s*=\s*(\w+)", text)
        constants[name] = value.group(1)
    return body, constants


def size_of(text, bits):
    compiler = shutil.which("clang") or pytest.skip("no clang")
    body, constants = struct_of(text)
    source = "typedef unsigned char byte; typedef unsigned short word; typedef __WCHAR_TYPE__ wchar_t;\n"
    source += "".join(f"enum {{ {name} = {value} }};\n" for name, value in constants.items())
    source += "struct message_client_join_game_request {\n" + body + "};\n"
    source += "char size[sizeof(struct message_client_join_game_request)];\n"
    result = subprocess.run([compiler, f"-m{bits}", "-fshort-wchar", "-x", "c", "-S", "-o", "-", "-"], input=source,
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    match = re.search(r"\.(?:comm|lcomm)\s+size,(\d+)", result.stdout) or \
        re.search(r"\.size\s+size,\s*(\d+)", result.stdout)
    return int(match.group(1))


@pytest.mark.parametrize("bits", (32, 64))
def test_the_host_and_client_structs_are_the_packet(bits):
    want = packet_size()
    assert size_of(CLIENT.read_text(errors="replace"), bits) == want
    assert size_of(SERVER.read_text(errors="replace"), bits) == want


def test_negative_control_a_host_struct_without_the_version_word():
    text = SERVER.read_text(errors="replace").replace("\tword network_version; /* AE hook */\n", "", 1)
    assert size_of(text, 64) == packet_size() - 2
