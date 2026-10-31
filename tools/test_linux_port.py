"""Tests for the native Linux build tooling (port/linux, tools/linux_*.py)."""

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

from tools import linux_build, linux_link_check, linux_msvc_semantics

# for tests that compile for the Linux target (and run what they build)
linux_host_only = pytest.mark.skipif(not sys.platform.startswith("linux"),
                                     reason="builds for the Linux target, so needs a Linux host")


# ---------- MSVC semantics header


def test_strip_cplusplus_keeps_c_branches_only():
    text = "\n".join([
        "a",
        "#ifdef __cplusplus", "cpp1", "#else", "c1", "#endif",
        "#if FOO", "b", "#else", "c", "#endif",
        "#ifndef __cplusplus", "c2", "#else", "cpp2", "#endif",
        "#if defined(__cplusplus)", "cpp3", "#if X", "cpp4", "#endif", "#endif",
        "#ifdef _WIN64", "win64", "#endif",
        "end",
    ])
    assert linux_msvc_semantics.strip_cplusplus(text).split() == ["a", "c1", "b", "c", "c2", "end"]


def write(path: Path, text: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="latin-1")
    return path


def test_scan_finds_prototype_scope_tags_and_inline_functions(tmp_path):
    header = write(tmp_path / "a.h", """
        struct location;
        void f(struct scenario *s, union color *c);
        /* struct commented_out */
        __inline long prototyped(long x) { return x; }
        __inline long private_helper(long x) { return x; }
        long prototyped(long x);
        #ifdef __cplusplus
        __inline long cpp_only(long x) { return x; }
        #endif
    """)
    files = [header]
    tags = linux_msvc_semantics.scan_tags(files)
    assert tags == {"location": {"struct"}, "scenario": {"struct"}, "color": {"union"}}
    assert linux_msvc_semantics.scan_inline_functions(files, all_inlines=False) == {"prototyped"}
    assert linux_msvc_semantics.scan_inline_functions(files, all_inlines=True) == {
        "prototyped", "private_helper",
    }


def test_render_skips_tags_used_as_both_struct_and_union():
    text = linux_msvc_semantics.render(
        {"a": {"struct"}, "b": {"union"}, "mixed": {"struct", "union"}}, {"f"}
    )
    assert "struct a;" in text
    assert "union b;" in text
    assert "mixed" not in text
    assert "#pragma weak f" in text


# ---------- Xbox SDK declarations (port/include/xdk)


XDK_INCLUDE = Path(__file__).resolve().parent.parent / "port" / "include" / "xdk"


def test_xdk_headers_use_the_sdk_spellings():
    # each port gives the SDK's keywords and names its own meaning (its
    # prefix header), so none of one port's must be written into them
    for header in sorted(XDK_INCLUDE.glob("*.h")):
        text = re.sub(r"/\*.*?\*/", "", header.read_text(encoding="utf-8"), flags=re.S)
        assert "__attribute__" not in text, header.name
        assert not re.findall(r"\bhalo_\w+", text), header.name


@linux_host_only
@pytest.mark.skipif(shutil.which("clang") is None, reason="clang is needed to compile the headers")
def test_xdk_headers_compile_for_the_game(tmp_path):
    root = XDK_INCLUDE.parent.parent.parent
    source = write(tmp_path / "unit.c", "".join(
        f"#include <{name}>\n" for name in ("xtl.h", "xbdm.h", "xkbd.h", "d3d8perf.h")
    ))
    flags = [flag for flag in linux_build.LINUX_ABI_FLAGS + linux_build.GAME_FLAGS if flag != "-w"]
    for defines in ([], ["-DDEBUG_KEYBOARD"], ["-DNOD3D", "-DNODSOUND"]):
        subprocess.run(
            ["clang", *flags, *defines, "-Werror", "-fsyntax-only",
             "-include", str(root / "port/linux/include/halo_linux_prefix.h"),
             "-I", str(root / "port/linux/include"), "-idirafter", str(XDK_INCLUDE), str(source)],
            check=True,
        )


def pdb_writer():
    """An xdk_headers Writer over an empty type table: basic types only."""
    from tools import pdb200_types, xdk_headers
    pdb = object.__new__(pdb200_types.Pdb)
    pdb.types, pdb.aggregates = {}, {}
    return xdk_headers, xdk_headers.Writer(pdb)


def test_xdk_headers_regroups_anonymous_members():
    # the PDB lists an anonymous structure's or union's members in their
    # container, at their offsets
    xdk_headers, writer = pdb_writer()
    member = xdk_headers.Member
    ulong, int64 = 0x22, 0x13
    union = writer.render(writer.group(
        [member("LowPart", ulong, 0), member("HighPart", ulong, 4), member("QuadPart", int64, 0)], True), "", "u")
    assert union == ["struct {", "    unsigned long LowPart;", "    unsigned long HighPart;", "};",
                     "__int64 QuadPart;"]
    structure = writer.render(writer.group(
        [member("Tag", ulong, 0), member("Low", ulong, 4), member("High", ulong, 8),
         member("Whole", int64, 4), member("After", ulong, 12)], False), "", "s")
    assert structure == ["unsigned long Tag;", "union {", "    struct {", "        unsigned long Low;",
                         "        unsigned long High;", "    };", "    __int64 Whole;", "};",
                         "unsigned long After;"]


# ---------- weak reference link check


@pytest.mark.skipif(shutil.which("clang") is None or shutil.which("nm") is None,
                    reason="clang and nm are needed to build test objects")
def test_link_check_rejects_undefined_weak_references(tmp_path):
    def compile_object(name: str, text: str) -> Path:
        source = write(tmp_path / f"{name}.c", text)
        output = tmp_path / f"{name}.o"
        subprocess.run(["clang", "-c", "-o", str(output), str(source)], check=True)
        return output

    caller = compile_object("caller", "#pragma weak helper\nint helper(int);\nint call(void) { return helper(1); }\n")
    local = compile_object("local", "static int helper(int x) { return x; }\nint use(void) { return helper(2); }\n")
    provider = compile_object("provider", "#pragma weak helper\nint helper(int x) { return x; }\n")

    def check(*objects: Path) -> int:
        response = write(tmp_path / "objects.rsp", "\n".join(f"'{o}'" for o in objects))
        return subprocess.run(
            [sys.executable, linux_link_check.__file__, str(response)], capture_output=True
        ).returncode

    # a file-local copy elsewhere does not satisfy the reference
    assert check(caller, local) == 1
    assert check(caller, local, provider) == 0


# ---------- game sources


def test_game_sources_leave_out_the_excluded(tmp_path, monkeypatch):
    write(tmp_path / "source" / "a.c", "")
    write(tmp_path / "source" / "zlib" / "b.c", "")
    write(tmp_path / "source" / "zlib" / "example.c", "")
    write(tmp_path / "source" / "tiff" / "c.c", "")
    write(tmp_path / "source" / "tiff" / "deep" / "d.c", "")
    write(tmp_path / "source" / "tiffx.c", "")
    write(tmp_path / "source" / "a.h", "")
    monkeypatch.chdir(tmp_path)
    # (a folder is a name ending in "/": not a file that only starts like it)
    config = {"game": {"root": "source", "exclude": ["source/zlib/example.c", "source/tiff/"],
                       "defines": ["DEBUG"], "include_dirs": ["source", "source/saved games"]}}
    assert linux_build.game_sources(config) == [Path("source/a.c"), Path("source/tiffx.c"), Path("source/zlib/b.c")]
    assert linux_build.game_defines_and_includes(config) == '-DDEBUG -Isource -I"source/saved games"'


# ---------- menus (port/assets/menus)


MENUS = Path(__file__).resolve().parent.parent / "port/assets/menus"
MENU_ATTRIBUTES = {
    "menus": {"root"},
    "bitmap": {"name", "width", "height", "frames", "platform"},
    "frame": {"png", "map", "index", "width", "height", "x", "y", "platform"},
    "strings": {"name", "map", "platform"},
    "string": {"text", "map", "platform"},
    "widget": {"name", "type", "controller", "flags", "bitmap", "text", "strings", "values", "setting", "font",
               "color", "align", "description", "string_list", "list_flags", "text_flags", "header_bitmap",
               "footer_bitmap", "header_bounds", "footer_bounds", "child_controller", "x", "y", "left", "top",
               "width", "height", "text_x", "text_y", "auto_close", "auto_close_fade", "string_index", "platform"},
    "child": {"widget", "controller", "x", "y", "platform"},
    "on": {"event", "run", "script", "open", "replace", "close", "widget", "focus", "reload", "sound", "otherwise",
           "label", "back", "branch", "platform"},
    "data": {"input", "platform"},
    "conditional": {"widget", "if_failed", "platform"},
    "replace": {"search", "function", "platform"},
}


def c_strings(path: Path, start: str, end: str) -> list:
    """the string literals of a C file between two markers"""
    text = path.read_text(encoding="latin-1")
    text = text[text.index(start):]
    return re.findall(r'"([^"\\]*)"', text[:text.index(end)])


# menu_files.c's attributes read as whole numbers (its tables' { name, NULL, &long })
MENU_INTEGER_ATTRIBUTES = {"auto_close", "auto_close_fade", "height", "index", "left", "string_index", "text_x",
                           "text_y", "top", "width", "x", "y"}


def test_menus_are_well_formed():
    """What port/linux/src/menu_files.c and port/linux/game/menu_tags.c check
    when the game loads them, but the map's own names (paths with backslashes),
    which only the map has."""
    import json
    import xml.etree.ElementTree as ElementTree

    root = MENUS.parent.parent.parent
    tags = root / "port/linux/game/menu_tags.c"
    events = c_strings(tags, "event_names[] =", "};")
    flags = c_strings(tags, "widget_flag_names[] =", "};")
    functions = set(c_strings(root / "source/interface/ui_widget_event_handler_functions.c", '\t{\n\t\t"NULL",', "}"))
    functions |= set(c_strings(tags, "port_function_names[] =", "};"))
    inputs = set(c_strings(tags, "game_data_input_names[] =", "};"))
    inputs |= set(c_strings(tags, "port_game_data_input_names[] =", "};"))
    listed = json.loads((MENUS / "menus.json").read_text())["files"]
    files = sorted(MENUS.rglob("*.xml"))
    assert sorted(path.relative_to(MENUS).as_posix() for path in files) == \
        sorted(name for name in listed if name.endswith(".xml"))
    widgets, bitmaps, strings, references, roots = {}, {}, {}, [], []
    for path in files:
        tree = ElementTree.parse(path)
        assert tree.getroot().tag == "menus", path
        if tree.getroot().get("root"):
            roots.append(tree.getroot().get("root"))
        for element in tree.getroot().iter():
            where = f"{path.name}: <{element.tag} {element.get('name', '')}>"
            assert element.tag in MENU_ATTRIBUTES, where
            assert set(element.attrib) <= MENU_ATTRIBUTES[element.tag], where
            # (the platforms that show it: menu_files.c's MENU_PLATFORM)
            assert element.get("platform") is None or \
                set(element.get("platform").split()) <= {"desktop", "android", "web"}, where
            # (menu_files.c's whole numbers, which the tags keep in shorts,
            # its true/false attributes, and no text but whitespace outside
            # <string>s)
            for attribute in MENU_INTEGER_ATTRIBUTES & set(element.attrib):
                value = element.get(attribute)
                assert re.fullmatch(r"-?[0-9]+", value) and -32768 <= int(value) <= 32767, f"{where} {attribute}"
            for attribute in ("back", "branch", "if_failed"):
                assert element.get(attribute) in (None, "true", "false"), f"{where} {attribute}"
            assert not (element.text or "").strip() and not (element.tail or "").strip(), where
            if element.tag == "bitmap":
                bitmaps[element.get("name")] = element
                for frame in element.iter("frame"):
                    assert (frame.get("png") is None) != (frame.get("map") is None), where
                    if frame.get("map"):
                        assert "\\" in frame.get("map") and int(frame.get("index")) >= 0, where
                        continue
                    assert frame.get("png") in listed, where
                    with (MENUS / frame.get("png")).open("rb") as png:
                        header = png.read(24)
                    width, height = int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")
                    logical = int(frame.get("width")), int(frame.get("height"))
                    assert width % logical[0] == 0 and width // logical[0] == height // logical[1], where
            elif element.tag == "strings":
                strings[element.get("name")] = element
                # (a string's map= is its number in the list's map list, menu_tags.c)
                for string in element.findall("string"):
                    assert string.get("map") is None or (element.get("map") and string.get("map").isdigit()), where
            elif element.tag == "widget":
                name = element.get("name")
                assert name and name not in widgets, where
                widgets[name] = element
                assert element.get("type", "container") in ("container", "text", "spinner", "column_list"), where
                assert set(element.get("flags", "").split()) <= set(flags), where
                children = element.findall("widget") + element.findall("child")
                if element.get("type") == "spinner":
                    assert len(children) in (0, 1, 3), where
                if element.get("strings") or "items_from_strings" in element.get("list_flags", ""):
                    assert element.get("type") == "spinner" and not children, where
                if element.get("setting"):
                    assert len(element.get("strings").split("|")) == len(element.get("values").split("|")), where
                for attribute in ("bitmap", "header_bitmap", "footer_bitmap"):
                    references.append((where, bitmaps, element.get(attribute)))
                references.append((where, strings, element.get("string_list")))
                references.append((where, widgets, element.get("description")))
            elif element.tag == "child" or element.tag == "conditional":
                references.append((where, widgets, element.get("widget")))
            elif element.tag == "on":
                assert set(element.get("event").split()) <= set(events), where
                run = element.get("run")
                assert run is None or run in functions or run.startswith("unwired "), where
                assert element.get("otherwise") is None or run, where
                for attribute in ("open", "replace", "focus", "widget", "otherwise"):
                    references.append((where, widgets, element.get(attribute)))
            elif element.tag == "data":
                assert element.get("input") in inputs or element.get("input").startswith("unwired "), where
    assert all(root_name in widgets for root_name in roots)
    for where, names, name in references:
        if name and "\\" not in name:
            assert name in names, f"{where} {name}"


def test_menu_settings_exist():
    """Every setting a menu's spinner sets is one of config.toml's
    (port/linux/src/port_config.c) or of the profile (menu_functions.c), and
    the keyboard's controls are the same in the settings, in the controls'
    screen and in the input code."""
    root = MENUS.parent.parent.parent
    config = (root / "port/linux/src/port_config.c").read_text()
    functions = (root / "port/linux/game/menu_functions.c").read_text()
    known = set(re.findall(r'^\t\{ "([a-z_]+\.[a-z_]+)", _config_', config, re.M))
    profile = set(re.findall(r'\{ "(profile\.[a-z_]+)", \d', functions))
    for path in (MENUS / "ce").glob("*.xml"):
        for setting in re.findall(r'setting="([^"]+)"', path.read_text()):
            assert setting in known | profile, f"{path.name}: {setting}"
    controls = set(re.findall(r'"(controls\.[a-z_]+)"', (root / "port/linux/src/xinput_sdl.c").read_text()))
    # (controls.controller_layout is the controller's, not a key's: controller_layout.c)
    assert controls == {name for name in known if name.startswith("controls.")} - {"controls.controller_layout"}
    # (Arena Evolved: ChupathingyCE's Delta text chat's keys are off with its chat, chat.enabled, and bound to
    # nothing; its text chat is OpenCE's, controls.chat and controls.team_chat, which Controls Setup has)
    delta_chat = {"controls.chat_all", "controls.chat_team", "controls.chat_log"}
    assert delta_chat <= controls
    # (DEV MODE's cameras are AE's KEY BINDINGS' only, while it is on: the classic Controls Setup is frozen)
    dev_mode = {"controls.dev_camera", "controls.dev_shoulder", "controls.dev_free_camera"}
    assert dev_mode <= controls
    # (TEST RUN's verdict keys are bindable DEV MODE actions in AE's KEY BINDINGS only: same)
    test_run = {f"controls.test_{name}" for name in ("pass", "fail", "note", "skip", "reload", "back", "hide_card")}
    assert test_run <= controls
    assert controls - delta_chat - dev_mode - test_run == set(re.findall(r'\{ "(controls\.[a-z_]+)", L"', functions))


def test_report_joined_games_stops_claims():
    """network.report_joined_games = false stops both what a copy sends the
    game list about a game it joined: its own report of the game
    (browser_client_report) and the claim of its players' lines in the
    host's report (browser_claim_game, /v1/claim). A game it hosts is
    claimed unless network.report_hosted_games is off."""
    root = MENUS.parent.parent.parent
    browser = (root / "port/linux/src/browser.c").read_text()
    for function in ("browser_client_report", "browser_claim_game"):
        body = re.search(r"^void " + function + r"\(.*?^\}", browser, re.M | re.S).group(0)
        gate = re.search(r'!config_boolean\("network\.report_joined_games"\)\s*\|\|\s*'
                         r'!p2p_joined_invite\(', body)
        assert gate, function
        # (before the request is queued; Arena Evolved returns earlier still, for a game with a password)
        assert body.index("pthread_once(&browser_once") > gate.start(), function
    readme = (root / "port/linux/README.md").read_text()
    row = next(line for line in readme.splitlines() if line.startswith("| `network.report_joined_games`"))
    assert "/v1/claim" in row


def test_joined_invite_follows_the_joined_host():
    """Every join (a listed game, an invite, a direct address, the local
    network, a server's change of game) goes through
    network_game_client_initiate_join_game, which tells p2p the host it
    joins (p2p_joining_host); an invite of another host is forgotten, so a
    game's report never goes out under a previous game's invite. Hosting
    has no joined invite."""
    root = MENUS.parent.parent.parent
    manager = (root / "source/networking/network_client_manager.c").read_text()
    join = re.search(r"^boolean network_game_client_initiate_join_game\(.*?^\}", manager, re.M | re.S).group(0)
    assert "p2p_joining_host(network_game_client_joined_host)" in join
    p2p = (root / "port/linux/src/p2p.c").read_text()
    forget = re.search(r"^void p2p_joining_host\(.*?^\}", p2p, re.M | re.S).group(0)
    assert "p2p_identifier_from_hash(p2p.join_host_hash" in forget
    assert "memset(p2p.join_host_hash" in forget and "memset(p2p.join_token" in forget
    joined = re.search(r"^int p2p_joined_invite\(.*?^\}", p2p, re.M | re.S).group(0)
    assert "!p2p.hosting" in joined


def test_report_hosted_games_keeps_listing():
    """network.report_hosted_games = false stops what a host sends the game
    list about its games (the carnage report, the Delta Stats batch and its
    own players' claims) and not the listing, and Network Setup sets it with
    the other stats rows (Arena Evolved: no Privacy screen of its own; crash
    reports are off)."""
    root = MENUS.parent.parent.parent
    browser = (root / "port/linux/src/browser.c").read_text()
    for function in ("browser_report_game", "browser_claim_game"):
        body = re.search(r"^void " + function + r"\(.*?^\}", browser, re.M | re.S).group(0)
        assert 'config_boolean("network.report_hosted_games")' in body, function
    hosting = re.search(r"^static void update_hosting\(.*?^\}", browser, re.M | re.S).group(0)
    assert "report_hosted_games" not in hosting
    upload = (root / "port/linux/src/event_upload.c").read_text()
    assert 'config_boolean("network.report_hosted_games")' in upload
    privacy = (MENUS / "ce/main_menu.settings_select.player_setup.player_profile_edit.network_setup.xml").read_text()
    for setting in ("network.report_hosted_games", "network.report_events", "network.report_joined_games"):
        assert f'setting="{setting}"' in privacy, setting
    readme = (root / "port/linux/README.md").read_text()
    assert any(line.startswith("| `network.report_hosted_games`") for line in readme.splitlines())


def c_format_json(format_text: str, values: list) -> str:
    """a C printf format of JSON members (%d, %ld, %s only) filled in"""
    parts = iter(values)
    return re.sub(r"%l?[ds]", lambda m: str(next(parts)), format_text.encode().decode("unicode_escape"))


def test_minimal_report_lines():
    """A carnage report line of a player not on Chupathingy (the host's
    report), or of anyone but the reporter (a client's report), is only the
    scoreboard's numbers: name, team, place, score, kills, deaths (and the
    host's claim tag). Its recorded parts (medals, weapons, ...) never go."""
    root = MENUS.parent.parent.parent
    browser = (root / "port/linux/src/browser.c").read_text()
    body = re.search(r"^static char \*report_json\(.*?^\}", browser, re.M | re.S).group(0)
    minimal = re.search(r"if \(player->minimal\)\s*\{.*?append\(report, size, used, \"(.*?)\",", body, re.S).group(1)
    line = json.loads('{"name": "Someone"' + c_format_json(minimal, [1, 2, 3, 4, 5]) + "}")
    assert set(line) == {"name", "team", "place", "score", "kills", "deaths"}
    # (the tag stays, so a Chupathingy player the handshake missed can still claim it)
    assert body.index("if (player->minimal)") < body.index('"tag\\"')
    assert "!player->minimal && player->extra" in body
    stats = (root / "port/linux/game/game_stats.c").read_text()
    departed = re.search(r"if \(!full\)\s*\{\s*return stats_append\(text, size, used, \"(.*?)\",", stats, re.S).group(1)
    assert set(json.loads(c_format_json(departed, ['"Someone"', 1, 2, 3]))) == {"name", "team", "kills", "deaths"}
    # (departed players in full only on the host's report, and only Chupathingy players)
    assert "host && game_stats.departed[index].delta" in stats
    full = re.search(r"^boolean game_stats_line_full\(.*?^\}", stats, re.M | re.S).group(0)
    assert "local_player_index != NONE" in full and "return host && (" in full
    engine = (root / "source/game/game_engine.c").read_text()
    assert "line->minimal = !game_stats_line_full(ranking[index].player_index, server != NULL);" in engine


def test_report_events_stops_only_events():
    """network.report_events = false stops the Delta Stats batch
    (event_upload.c) and nothing else: the host's carnage report goes
    whenever the game is listed (network.list_hosted_games), and the docs
    say so."""
    root = MENUS.parent.parent.parent
    upload = (root / "port/linux/src/event_upload.c").read_text()
    assert 'config_boolean("network.report_events")' in upload
    browser = (root / "port/linux/src/browser.c").read_text()
    assert "report_events" not in browser
    assert 'config_boolean("network.list_hosted_games")' in browser
    readme = (root / "port/linux/README.md").read_text()
    row = next(line for line in readme.splitlines() if line.startswith("| `network.report_events`"))
    assert "list_hosted_games" in row and "hardware ID" in row
    row = next(line for line in readme.splitlines() if line.startswith("| `network.list_hosted_games`"))
    assert "carnage report" in row


def test_no_hardware_id_in_events():
    """No hardware ID, nor a hash of one, goes in a Delta Stats batch."""
    root = MENUS.parent.parent.parent
    for path in ("port/linux/src/event_log.c", "port/linux/src/event_log.h", "port/linux/game/game_events.c"):
        text = (root / path).read_text()
        assert "hardware_id" not in text and not re.search(r"\bident\b|blake2b", text), path


def test_default_brokers_are_brokers_txt():
    """The brokers a game uses with no brokers.txt beside its config.toml
    (p2p_signal.c's DEFAULT_BROKERS) are port/assets/network/brokers.txt's."""
    root = MENUS.parent.parent.parent
    listed = [line.split("#", 1)[0].strip()
              for line in (root / "port/assets/network/brokers.txt").read_text().splitlines()]
    default = re.search(r'^#define DEFAULT_BROKERS "([^"]*)"',
                        (root / "port/linux/src/p2p_signal.c").read_text(), re.M).group(1)
    assert default.split(",") == [line for line in listed if line]


@linux_host_only
def test_p2p_signatures_and_listings(tmp_path):
    """internet play's Ed25519 (RFC 8032), the X25519 key of a seed, and the
    server browser's listings from host to browser (tools/p2p_lobby_check.c),
    built with the flags ninja gives the platform layer"""
    import shlex

    if not shutil.which("clang") or not shutil.which("ninja") or not Path("build.ninja").is_file():
        pytest.skip("needs clang, ninja and a configured build")
    command = subprocess.run(["ninja", "-t", "commands", "build/linux/obj/port/linux/src/p2p_crypto.o"],
                             capture_output=True, text=True, check=True).stdout.strip().splitlines()[-1]
    words = shlex.split(command)
    # (the compiler, and whatever runs it, ccache in CI: up to the first flag)
    while words and not words[0].startswith("-"):
        words = words[1:]
    flags = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word in ("-MF", "-o", "-c"):
            skip = True
        elif word == "-MMD" or word.startswith("-flto") or word.startswith("-fprofile-use"):
            continue
        else:
            flags.append(word)
    program = tmp_path / "p2p_lobby_check"
    # (-pthread: the check starts a thread; with the portable build's system
    # root, glibc 2.31, pthread_create is in libpthread, not libc, and an
    # unresolved symbol is let through as a null call)
    built = subprocess.run(["clang", *flags, "-O1", "-no-pie", "-pthread", "-Wl,--unresolved-symbols=ignore-all", "-o",
                            str(program), "tools/p2p_lobby_check.c", "port/linux/src/p2p_crypto.c",
                            "port/linux/src/p2p_lobby.c", "port/linux/src/p2p_copies.c",
                            "port/third_party/monocypher/monocypher.c",
                            "port/third_party/monocypher/monocypher-ed25519.c"],
                           capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-4000:]
    result = subprocess.run([str(program)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout
    assert "PASS" in result.stdout


def _ninja_compile_flags(obj: str) -> list:
    """the flags ninja compiles an object with, less the compiler (and
    ccache), its output and dependency file, LTO and the PGO profile"""
    import shlex

    command = subprocess.run(["ninja", "-t", "commands", obj],
                             capture_output=True, text=True, check=True).stdout.strip().splitlines()[-1]
    words = shlex.split(command)
    while words and not words[0].startswith("-"):
        words = words[1:]
    flags = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word in ("-MF", "-o", "-c"):
            skip = True
        elif word == "-MMD" or word.startswith("-flto") or word.startswith("-fprofile-use"):
            continue
        else:
            flags.append(word)
    return flags


def test_callouts_plan(tmp_path):
    """the callouts' plan (source/game/callouts.c) through a game of Blood
    Gulch's power items at each CALLOUT DETAIL with each shipped voice pack's clips
    (tools/callouts_check.c): items on their 10 s mark and spawn tick, waves
    ending by their mark, the clock's "ten" giving way, "is up" before the
    minute, MINIMAL's and VERBOSE's lines, an OS/CAMO spot's "is up"; built
    with the flags ninja gives the game's code"""
    if not shutil.which("clang") or not shutil.which("ninja") or not Path("build.ninja").is_file():
        pytest.skip("needs clang, ninja and a configured build")
    flags = _ninja_compile_flags("build/linux/obj/source/game/callouts.o")
    program = tmp_path / "callouts_check"
    built = subprocess.run(["clang", *flags, "-O1", "-no-pie", "-Wl,--unresolved-symbols=ignore-all", "-o",
                            str(program), "tools/callouts_check.c", "source/game/callouts.c"],
                           capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-4000:]
    # (every voice pack AE ships: the plan fits each one's clip lengths)
    for pack in sorted(p for p in Path("port/assets/voices").iterdir() if (p / "overshield_in_ten.wav").is_file()):
        result = subprocess.run([str(program), str(pack)], capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, pack.name + ": " + result.stdout[-4000:]
        assert "PASS" in result.stdout, pack.name


def test_arena_gametype_names(tmp_path):
    """the seeded gametypes' names table (source/saved games/arena_gametype_names.c,
    tools/arena_gametype_names_check.c): stored names at most 11 ASCII
    characters and unique, display names at most 31 and unique, every alias's
    successor there and its info its successor's place with its own name,
    the sets' counts and orders, lookups letters' case aside, unknown names
    shown as they are; built with the flags ninja gives the game's code"""
    if not shutil.which("clang") or not shutil.which("ninja") or not Path("build.ninja").is_file():
        pytest.skip("needs clang, ninja and a configured build")
    flags = _ninja_compile_flags("build/linux/obj/source/saved games/arena_gametype_names.o")
    program = tmp_path / "arena_gametype_names_check"
    built = subprocess.run(["clang", *flags, "-O1", "-no-pie", "-o", str(program),
                            "tools/arena_gametype_names_check.c"], capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-4000:]
    result = subprocess.run([str(program)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout[-4000:]
    assert "PASS" in result.stdout


def test_playlist_display_name(tmp_path):
    """an own gametype's display name block (source/saved games/
    playlist_display_name.c, tools/playlist_display_name_check.c): round trip,
    absent / torn / wrong-signature blocks are no name, cut at 31, empty
    clears, only the block's own bytes change (0..0x67 and the GPVO block
    untouched), carrying over; built with the flags ninja gives the game's code"""
    if not shutil.which("clang") or not shutil.which("ninja") or not Path("build.ninja").is_file():
        pytest.skip("needs clang, ninja and a configured build")
    flags = _ninja_compile_flags("build/linux/obj/source/saved games/arena_gametype_names.o")
    program = tmp_path / "playlist_display_name_check"
    # (linked strictly, the headers' unused D3D stand-ins dropped: a call to anything else unlinked is a link error)
    built = subprocess.run(["clang", *flags, "-O1", "-no-pie", "-ffunction-sections", "-Wl,--gc-sections", "-o",
                            str(program), "tools/playlist_display_name_check.c"], capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-4000:]
    result = subprocess.run([str(program)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout[-4000:]
    assert "PASS" in result.stdout


def test_gametype_layout(tmp_path):
    """(Arena Evolved, OpenCE's network 36) a gametype's rules: OpenCE's 0x2C bytes of PC options with version 1's
    0x1C first, Arena Evolved's rules in the variant's flags from bit 6 and its old pads, the saved gametype's 'GPVO'
    block to 0x148 and the display name block at 0x150 (tools/ae_gametype_layout_check.c), built with the flags ninja
    gives the game's code"""
    if not shutil.which("clang") or not shutil.which("ninja") or not Path("build.ninja").is_file():
        pytest.skip("needs clang, ninja and a configured build")
    flags = _ninja_compile_flags("build/linux/obj/source/saved games/arena_gametype_names.o")
    program = tmp_path / "ae_gametype_layout_check"
    built = subprocess.run(["clang", *flags, "-O1", "-no-pie", "-o", str(program), "tools/ae_gametype_layout_check.c"],
                           capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-4000:]
    result = subprocess.run([str(program)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout[-4000:]
    assert "PASS" in result.stdout


def build_spawn_heat_check(tmp_path: Path, game_engine: str = "source/game/game_engine.c",
                           base: bool = False) -> Path:
    """tools/spawn_heat_check.c with source/game/spawn_heat.c, game_engine.c
    (or another, base: the stock one, without spawn_heat.c) and the game's
    maths (port/third_party/musl-math: halo_pow and the rest), built with
    the flags ninja gives them"""
    musl = sorted(Path("port/third_party/musl-math/src").glob("*.c"))
    musl_flags = _ninja_compile_flags(f"build/linux/obj/{musl[0].with_suffix('.o')}")
    objects = tmp_path / "musl"
    objects.mkdir()
    for source in musl:
        built = subprocess.run(["clang", *musl_flags, "-O1", "-c", str(source), "-o",
                                str(objects / source.with_suffix(".o").name)], capture_output=True, text=True)
        assert built.returncode == 0, built.stderr[-4000:]
    flags = _ninja_compile_flags("build/linux/obj/source/game/spawn_heat.o")
    program = tmp_path / ("spawn_heat_check_base" if base else "spawn_heat_check")
    sources = (["-DSPAWN_HEAT_CHECK_BASE", "tools/spawn_heat_check.c", game_engine] if base else
               ["tools/spawn_heat_check.c", "source/game/spawn_heat.c", game_engine])
    built = subprocess.run(["clang", *flags, "-O1", "-no-pie", "-Wl,--unresolved-symbols=ignore-all", "-o",
                            str(program), *sources, *(str(path) for path in sorted(objects.glob("*.o")))],
                           capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-4000:]
    return program


def test_spawn_heat(tmp_path):
    """TRAINING's spawn heat (source/game/spawn_heat.c,
    tools/spawn_heat_check.c): the spawn chances worked out as
    find_best_starting_location_index's draw picks (its examples, sums,
    order, and the engine's own draw run 200000 times), and the spawn
    ratings bit for bit the stock rules, with a living player's own unit
    left out the same as if dead, and why a spawn rates 0"""
    if not shutil.which("clang") or not shutil.which("ninja") or not Path("build.ninja").is_file():
        pytest.skip("needs clang, ninja and a configured build")
    program = build_spawn_heat_check(tmp_path)
    result = subprocess.run([str(program)], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout[-4000:]
    assert "PASS" in result.stdout

# port_config.c on its own, with the app's file paths (HALO_DATA_ROOT) and C
# library calls: "get <setting>" prints it, "write <setting> <value>" sets it
CONFIG_PROGRAM = r"""
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "port_config.h"

void platform_log(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

int main(int argc, char **argv)
{
    char text[256];

    if (argc == 3 && !strcmp(argv[1], "get") && config_text(argv[2], text, sizeof(text)))
        return puts(text) < 0;
    if (argc == 4 && !strcmp(argv[1], "write"))
        return !config_write(argv[2], argv[3]);
    return 1;
}
"""


def test_config_has_each_section_once(tmp_path):
    """config.toml names each section once, as TOML asks: the defaults the
    game writes, and a file an older version wrote with [display] twice
    (which went unread), joined"""
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("needs a C compiler")
    root = Path(__file__).resolve().parent.parent
    stubs = tmp_path / "stubs"
    (stubs / "SDL3").mkdir(parents=True)
    (stubs / "SDL3" / "SDL.h").write_text("#include <strings.h>\n#define SDL_strncasecmp strncasecmp\n")
    (stubs / "platform.h").write_text("void platform_log(const char *format, ...);\n")
    # (Arena Evolved's port_config.c also lists folders, posix.h, and pauses the capture for its restart,
    # capture.h: none of it runs here)
    (stubs / "posix.h").write_text(
        "enum { _posix_file_is_directory = 1 << 0 };\n"
        "typedef unsigned long posix_ulong;\n"
        "struct posix_file_information { posix_ulong flags, size_low, modification_seconds, modification_nanoseconds; };\n"
        "static int posix_stat(const char *path, struct posix_file_information *information)\n"
        "{ (void)path; (void)information; return -1; }\n"
        "static void *posix_directory_open(const char *path) { (void)path; return 0; }\n"
        "static int posix_directory_next(void *directory, char *name, unsigned long size)\n"
        "{ (void)directory; (void)name; (void)size; return 0; }\n"
        "static void posix_directory_close(void *directory) { (void)directory; }\n")
    (stubs / "capture.h").write_text("static void capture_shutdown(void) {}\nstatic void capture_resume(void) {}\n")
    # (beside the stubs, which it then includes instead of the platform layer's)
    for name in ("port_config.c", "port_config.h", "ae_config_players.c", "ae_config_players.h"):
        shutil.copy(root / "port/linux/src" / name, stubs / name)
    (tmp_path / "main.c").write_text(CONFIG_PROGRAM)
    program = tmp_path / "config_program"
    subprocess.run([compiler, "-DHALO_ANDROID=1", "-DHALO_ARM64_GUEST=1", f"-I{stubs}",
                    f"-I{root}/port/third_party/tomlc17", "-o", str(program),
                    str(tmp_path / "main.c"), str(stubs / "port_config.c"), str(stubs / "ae_config_players.c"),
                    str(root / "port/third_party/tomlc17/tomlc17.c"), "-lpthread"], check=True)
    data = tmp_path / "data"
    data.mkdir()
    config = data / "config.toml"

    def run(*command):
        result = subprocess.run([str(program), *command], capture_output=True, text=True,
                                env={**os.environ, "HALO_DATA_ROOT": str(data)})
        assert result.returncode == 0, result.stderr
        assert "using the defaults" not in result.stderr, result.stderr
        return result.stdout.strip(), result.stderr

    def sections():
        return re.findall(r"^\[(\w+)\]$", config.read_text(), re.M)

    # the defaults
    assert run("get", "display.first_person_legs")[0] == "false"
    assert len(sections()) == len(set(sections())), sections()
    run("write", "display.first_person_legs", "true")
    assert run("get", "display.first_person_legs")[0] == "true"

    # an older version's
    config.write_text("[display]\nvsync = false\n\n[game]\nconsole_log = \"all\"\n\n"
                      "[display]\nfirst_person_legs = true\n")
    value, log = run("get", "display.first_person_legs")
    assert value == "true" and "joined" in log
    assert sections().count("display") == 1
    assert run("get", "display.vsync")[0] == "false"
    assert run("get", "game.console_log")[0] == "all"


# ---------- system link's stand-in machines (tools/system_link_bots.py)


def test_system_link_bots_messages_are_the_games():
    """the stand-in machines number their messages as network_messages.h does,
    and send each at the size network_messages.c's fields decode (pad 0,
    bytes 1, shorts 2, longs 4; an array a count byte and one element)"""
    import struct
    import time
    import types
    from tools import system_link_bots as bots

    root = Path(__file__).resolve().parents[1]
    header = (root / "source/networking/network_messages.h").read_text()
    enum = header[header.index("enum network_game_message_type"):]
    names = re.findall(r"\t_message_(\w+)", enum[:enum.index("};")])
    for name, value in vars(bots).items():
        if name.lower() in names:
            assert names.index(name.lower()) == value, name
    sizes = {}
    source = (root / "source/networking/network_messages.c").read_text()
    for fields, name in re.findall(r"\t\{\n((?:\t\t.*\n)*?)\t\},\n\tNETWORK_GAME_MESSAGE_DEFINITION\((\w+),", source):
        sizes[name] = sum(int(count) * {"pad": 0, "bytes": 1, "shorts": 2, "longs": 4}.get(kind, 0) + (kind == "array")
                          for kind, count in re.findall(r"_data_packet_field_(\w+), (\d+)", fields))

    sent = []
    machine = bots.Machine(1, "127.0.0.2", "127.0.0.1", print)
    machine.udp.close()
    machine.send = sent.append
    machine.udp = types.SimpleNamespace(sendto=lambda data, address: sent.append(data))
    machine.joined()
    machine.handle(bots.SERVER_MACHINE_ACCEPTED, struct.pack(">ih", 0, 1))
    settings = bytes(0x24) + b"levels\\test\\bloodgulch\\bloodgulch".ljust(0x80, b"\0")
    machine.handle(bots.SERVER_GAME_SETTINGS_UPDATE, struct.pack(">HHH", len(settings), 0, len(settings)) + settings)
    machine.tick(time.monotonic())
    machine.handle(bots.SERVER_BEGIN_GAME, b"")
    machine.tick(time.monotonic() + 60)
    machine.handle(bots.SERVER_GAME_UPDATE, struct.pack(">I", 1))
    machine.tick(time.monotonic())
    assert len(sent) == 6
    for data in sent:
        name = names[data[-1]]
        assert len(data) - 4 == sizes[name], name


# ---------- the wire format (tools/network_format.py)


def test_network_format_is_the_recorded_one():
    """what the machines send each other is what port/linux/network_format.txt
    records for MCC_PROTOCOL_VERSION, and NETCODE.md says what that version
    changed. The ordinary OpenCE contract is pinned by test_mcc_protocol.py."""
    from tools import network_format

    problems = network_format.check()
    assert not problems, "\n".join(problems)


# ---------- the tag validator (port/linux/game/tag_validate.c)

RETAIL_MAPS = Path("assets/maps")
MAP_VALIDATE = Path("build/linux/map_validate")


def _retail_maps():
    maps = sorted(RETAIL_MAPS.glob("*.map"))
    if not maps or not MAP_VALIDATE.is_file():
        pytest.skip("needs the retail maps in assets/maps and ninja linux's build/linux/map_validate")
    return maps


def test_system_link_bots_ae_silent_mark():
    """--ae-silent marks a stand-in machine's join request as port/linux/game/ae_silent_rules.c does (the
    name's last character after its end: the mark and the capability); without it the name's tail is zeros, as
    a stock machine's"""
    import struct
    from tools import system_link_bots as bots

    root = Path(__file__).resolve().parents[1]
    rules = (root / "port/linux/game/ae_silent_rules.h").read_text()
    assert int(re.search(r"#define AE_SILENT_JOIN_MARK (0x[0-9A-Fa-f]+)u", rules)[1], 16) == bots.AE_SILENT_JOIN_MARK
    assert int(re.search(r"#define AE_SILENT_CAPABILITY (\d+)", rules)[1]) == bots.AE_SILENT_CAPABILITY
    marked = struct.unpack(">32H", bots.machine_name("bot1", True))
    assert marked[:5] == (ord("b"), ord("o"), ord("t"), ord("1"), 0)
    assert marked[30] == 0 and marked[31] == bots.AE_SILENT_JOIN_MARK | bots.AE_SILENT_CAPABILITY
    plain = struct.unpack(">32H", bots.machine_name("bot1", False))
    assert plain[4:] == (0,) * 28
    long_name = struct.unpack(">32H", bots.machine_name("x" * 40, True))
    assert long_name[29] == ord("x") and long_name[30] == 0


def test_retail_maps_need_no_corrections():
    """every retail map in assets/maps passes the tag validator, each of its
    structure bsps too, with no correction: the schemas (tag_schema_*.c)
    say what the game's own maps hold"""
    maps = _retail_maps()
    result = subprocess.run([str(MAP_VALIDATE), "--strict", *map(str, maps)], capture_output=True, text=True,
                            timeout=1800)
    assert result.returncode == 0, result.stdout[-6000:]


def test_tag_validator_survives_damaged_maps():
    """retail maps with a few of their tags' words changed at random: the
    validator never crashes or hangs, and a map it lets through is clean
    when checked again"""
    maps = _retail_maps()
    chosen = [path for path in maps if path.stem in ("bloodgulch", "a10", "ui")] or maps[:1]
    result = subprocess.run([str(MAP_VALIDATE), "--fuzz", "300", "--seed", "1", *map(str, chosen)],
                            capture_output=True, text=True, timeout=1800)
    assert result.returncode == 0, (result.stdout + result.stderr)[-6000:]


# ---------- (Arena Evolved) bytes shared by several structures (tag_validate.c extent_claim)

def _crafted_overlap(source: Path, destination: Path, moved_group: str, onto_group: str,
                     moved_zero_root: bool = False) -> None:
    """a retail map, uncompressed, with the root of a tag of moved_group
    moved onto the root of the first tag of onto_group: a cross-kind
    overlap from the same first byte"""
    import struct
    import zlib

    base = 0x803A6000
    raw = source.read_bytes()
    header = raw[:0x800]
    file_length, tag_offset = struct.unpack_from("<i", header, 8)[0], struct.unpack_from("<i", header, 16)[0]
    data = bytearray(header + zlib.decompressobj().decompress(raw[0x800:]))
    data += b"\0" * (file_length - len(data))
    instances, _, _, count = struct.unpack_from("<IiIi", data, tag_offset)
    first = tag_offset + instances - base

    def group(index):
        return struct.unpack_from("<I", data, first + index * 32)[0].to_bytes(4, "big").decode("latin1")

    def root(index):
        return struct.unpack_from("<I", data, first + index * 32 + 20)[0]

    def zero_root(index):
        offset = tag_offset + root(index) - base
        return root(index) and data[offset:offset + 12] == bytes(12)

    moved = next(index for index in range(count) if group(index) == moved_group and
                 (not moved_zero_root or zero_root(index)))
    onto = next(index for index in range(count) if group(index) == onto_group)
    struct.pack_into("<I", data, first + moved * 32 + 20, root(onto))
    destination.write_bytes(bytes(data))


def _validate(*arguments) -> subprocess.CompletedProcess:
    return subprocess.run([str(MAP_VALIDATE), *map(str, arguments)], capture_output=True, text=True, timeout=600)


def test_shared_bytes_only_in_a_mods_maps(tmp_path):
    """a color table's empty root moved onto a model collision's: two kinds
    sharing bytes with no runtime value. A map checked as upstream's (not a
    mod's) is refused for it; a mod's map (--mod: CE+ X's ui.map) is played,
    the shared tag noted"""
    maps = [path for path in _retail_maps() if path.stem == "bloodgulch"]
    if not maps:
        pytest.skip("needs bloodgulch.map in assets/maps")
    crafted = tmp_path / "crafted.map"
    _crafted_overlap(maps[0], crafted, "colo", "coll", moved_zero_root=True)
    plain = _validate(crafted)
    assert plain.returncode == 1 and "overlapping another's" in plain.stdout, plain.stdout[-4000:]
    mod = _validate("--mod", crafted)
    assert mod.returncode == 0 and "under another name" in mod.stdout, mod.stdout[-4000:]


def test_shared_bytes_never_over_a_runtime_value(tmp_path):
    """a model collision's root moved onto a looping sound's, whose
    runtime_scripting_sound_index (a value the game writes as it runs, at
    0x1C) the overlap covers: refused even as a mod's map"""
    maps = [path for path in _retail_maps() if path.stem == "bloodgulch"]
    if not maps:
        pytest.skip("needs bloodgulch.map in assets/maps")
    crafted = tmp_path / "crafted.map"
    _crafted_overlap(maps[0], crafted, "coll", "lsnd")
    mod = _validate("--mod", crafted)
    assert mod.returncode == 1 and "with a value the game writes as it runs" in mod.stdout, mod.stdout[-4000:]
