"""The weapon pack builder, tools/ae_pack_build.c (+ port/linux/game/ae_pack_build.c): a weapon's tag graph out of a
donor map into a pack (docs/weapon-packs.md, "Building a pack").

Everywhere (CI): the builder's argument checks and its refusals of files that are not maps, the table of tag groups
whose external tags a map may lack (checked against the tag schema), and the builder unit kept out of the game.

On the build box (needs the maps; skipped elsewhere): packs built from CE+ X's Blood Gulch and from the approved v6e
donor are accepted by ae_pack_check and tools/weapon_pack/aepack.py, own tags all renamed under the prefix and none
of them a stock map's tag, externals all stock tags; v6e's icon and smooth flags; two runs give the same bytes; a
donor without the weapon is refused. Paths (each may be set in the environment):
    AE_PACK_STOCK_MAPS   the stock maps' folder            (default ~/halo-data/maps)
    AE_PACK_CEX_DONOR    CE+ X's Blood Gulch               (default ~/halo-data/mods/CE+ X/maps/bloodgulch.map)
    AE_PACK_V6E_DONOR    the approved v6e donor            (default ~/halo-test/weapon-pack/donors/ae_cmt_br_v6e.map)
    AE_PACK_TEST_WORK    where the packs are written       (default: pytest's temporary folder)
Packs and name lists made here are never committed."""
import hashlib
import os
import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "weapon_pack"))

import aepack  # noqa: E402

BUILDER = ROOT / "build" / "linux" / "ae_pack_build"
CHECKER = ROOT / "build" / "linux" / "ae_pack_check"
BUILDER_UNIT = ROOT / "port" / "linux" / "game" / "ae_pack_build.c"
SCHEMA_SOURCES = sorted((ROOT / "port" / "linux" / "game").glob("tag_schema*.c"))
VALIDATOR_SOURCE = ROOT / "port" / "linux" / "game" / "tag_validate.c"

PREFIX = "ae\\packs\\battle_rifle\\"
# the groups a map may lack a pack's external tag of (the builder's table): effects, sounds, decals, particles, lights
OPTIONAL_GROUPS = {"effe", "snd!", "deca", "part", "ligh"}
# the stock multiplayer maps (the externals' names come from these)
MULTIPLAYER_MAPS = ("beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation", "hangemhigh",
                    "longest", "prisoner", "putput", "ratrace", "sidewinder", "wizard")
CEX_WEAPON = "dw2\\weapons\\battle_rifle\\battle_rifle"
V6E_WEAPON = "cmt\\weapons\\evolved\\battle_rifle\\battle_rifle"
V6E_SHA256 = "0aa3ffcbb704e59407945cd5eece3aaf53740992a582e23fe61aec51dca36884"
# v6e's HUD bitmaps (round 6b): the counter's two sheets (smooth; the meter's green is coverage) and the swap icon's
# sheet (an off-screen meter of the interface whose sequence 0 is sprites)
V6E_METER = "ae_pack\\hud\\br_meter_smooth"
V6E_ALPHAS = "ae_pack\\hud\\br_alphas_smooth"
V6E_ICON = "ae_pack\\hud\\br_meter"


def environment_path(name, default):
    return Path(os.environ.get(name, str(default))).expanduser()


STOCK_MAPS = environment_path("AE_PACK_STOCK_MAPS", Path.home() / "halo-data" / "maps")
CEX_DONOR = environment_path("AE_PACK_CEX_DONOR", Path.home() / "halo-data" / "mods" / "CE+ X" / "maps" /
                             "bloodgulch.map")
V6E_DONOR = environment_path("AE_PACK_V6E_DONOR", Path.home() / "halo-test" / "weapon-pack" / "donors" /
                             "ae_cmt_br_v6e.map")

needs_builder = pytest.mark.skipif(not BUILDER.is_file(), reason="build/linux/ae_pack_build is built by ninja linux")
needs_stock_maps = pytest.mark.skipif(not all((STOCK_MAPS / f"{name}.map").is_file() for name in MULTIPLAYER_MAPS),
                                      reason="the stock maps are on the build box")


def run(*arguments, check=None):
    result = subprocess.run([str(BUILDER), *map(str, arguments)], capture_output=True, text=True, timeout=600)
    if check is not None:
        assert result.returncode == check, (result.returncode, result.stdout, result.stderr)
    return result


@pytest.fixture
def work(tmp_path):
    folder = os.environ.get("AE_PACK_TEST_WORK")
    if not folder:
        yield tmp_path
        return
    path = Path(folder).expanduser() / tmp_path.name
    path.mkdir(parents=True, exist_ok=True)
    yield path
    for item in sorted(path.rglob("*"), reverse=True):
        item.unlink() if item.is_file() else item.rmdir()
    path.rmdir()


def write(path, text):
    path.write_text(text, encoding="utf-8")
    return path


def side_files(folder):
    """an empty externals list, a texts file and a credits file"""
    return {
        "externals": write(folder / "externals.txt", ""),
        "texts": write(folder / "texts.txt", "menu_name=BATTLE RIFLE\nweapon_pickup=Picked up a battle rifle\n"
                                             "ammo_pickup=Picked up %d rounds for battle rifle\n"),
        "credits": write(folder / "CREDITS.txt", "Test credits.\n"),
    }


def arguments(donor, weapon, externals, out, *extra, texts=None, credits=None):
    line = ["--donor", donor, "--weapon", weapon, "--external-names", externals, "--prefix", PREFIX,
            "--id", "ae_battle_rifle", "--revision", "1", "--display", "BATTLE RIFLE", "--weapon-key",
            "battle_rifle", "--behaviour", "1", "--labels", "h_br,ar", "--replaces", "2", "--out", out]
    if texts:
        line += ["--texts", texts]
    if credits:
        line += ["--credits", credits]
    return [*line, *extra]


# ---------- everywhere: arguments and files that are not maps

@needs_builder
def test_no_arguments_prints_the_usage():
    result = run(check=2)
    assert "usage:" in result.stderr


@needs_builder
@pytest.mark.parametrize("change, message", [
    (("--revision", "0"), "--revision"),
    (("--revision", "x"), "--revision"),
    (("--id", "Bad-Id"), "--id"),
    (("--id", "a" * 32), "--id"),
    (("--display", ""), "--display"),
    (("--weapon-key", "Battle Rifle"), "--weapon-key"),
    (("--labels", "h_br,ar,pistol,x"), "--labels"),
    (("--labels", ""), "--labels"),
    (("--behaviour", "4"), "--behaviour"),
    (("--replaces", "1"), "--replaces"),
    (("--replaces", "10"), "--replaces"),
    (("--prefix", "ae\\packs\\battle_rifle"), "--prefix"),
    (("--root", "weap"), "--root"),
    (("--root", "weapon:x"), "--root"),
    (("--icon", "ae_pack\\hud\\br_meter"), "--icon"),
    (("--icon", "ae_pack\\hud\\br_meter:x"), "--icon"),
    (("--smooth", "ae_pack\\hud\\br_meter:shiny"), "--smooth"),
    (("--frobnicate",), "--frobnicate"),
])
def test_bad_arguments_are_refused_before_anything_is_read(tmp_path, change, message):
    files = side_files(tmp_path)
    out = tmp_path / "out.aepack"
    line = arguments(tmp_path / "no-such.map", CEX_WEAPON, files["externals"], out)
    if change[0] in line:
        line[line.index(change[0]) + 1] = change[1]
    else:
        line += list(change)
    result = run(*line, check=2)
    assert message in result.stderr
    assert not out.exists()


@needs_builder
@pytest.mark.parametrize("missing", ["--donor", "--weapon", "--external-names", "--prefix", "--id", "--revision",
                                     "--display", "--weapon-key", "--labels", "--out"])
def test_each_required_argument(tmp_path, missing):
    files = side_files(tmp_path)
    line = arguments(tmp_path / "no-such.map", CEX_WEAPON, files["externals"], tmp_path / "out.aepack")
    index = line.index(missing)
    del line[index:index + 2]
    result = run(*line, check=2)
    assert missing in result.stderr


@needs_builder
def test_a_missing_donor_is_refused(tmp_path):
    files = side_files(tmp_path)
    out = tmp_path / "out.aepack"
    result = run(*arguments(tmp_path / "no-such.map", CEX_WEAPON, files["externals"], out, texts=files["texts"],
                            credits=files["credits"]), check=1)
    assert "refused: the donor cannot be read" in result.stderr
    assert not out.exists()


@needs_builder
def test_a_ten_byte_donor_is_refused(tmp_path):
    files = side_files(tmp_path)
    donor = tmp_path / "ten.map"
    donor.write_bytes(b"head\x05\x00\x00\x00ab")
    out = tmp_path / "out.aepack"
    result = run(*arguments(donor, CEX_WEAPON, files["externals"], out), check=1)
    assert "refused: the donor is not an Xbox map" in result.stderr
    assert not out.exists()


@needs_builder
@pytest.mark.parametrize("contents, message", [
    ("snd!sound\\x\n", "line 1"),
    ("sn\tsound\\x\n", "line 1"),
    ("snd!\t\n", "line 1"),
    ("snd!\tsound\\x\nsnd!\t" + "a" * 256 + "\n", "line 2"),
])
def test_bad_external_names_are_refused(tmp_path, contents, message):
    files = side_files(tmp_path)
    write(files["externals"], contents)
    result = run(*arguments(tmp_path / "no-such.map", CEX_WEAPON, files["externals"], tmp_path / "out.aepack"),
                 check=1)
    assert "refused: external names" in result.stderr and message in result.stderr


@needs_builder
@pytest.mark.parametrize("contents, message", [
    ("colour=red\n", "line 1"),
    ("menu_name=A\nmenu_name=B\n", "line 2"),
    ("weapon_pickup\n", "line 1"),
])
def test_bad_texts_are_refused(tmp_path, contents, message):
    files = side_files(tmp_path)
    write(files["texts"], contents)
    result = run(*arguments(tmp_path / "no-such.map", CEX_WEAPON, files["externals"], tmp_path / "out.aepack",
                            texts=files["texts"]), check=1)
    assert "refused: texts" in result.stderr and message in result.stderr


@needs_builder
def test_list_names_refuses_a_file_that_is_not_a_map(tmp_path):
    bad = tmp_path / "bad.map"
    bad.write_bytes(bytes(10))
    result = run("--list-names", bad, check=1)
    assert "refused" in result.stderr


# ---------- everywhere: the groups whose external tags a map may lack

def schema_fields_naming(groups):
    """every schema field (in tag_schema*.c) whose groups name one of groups: (file, its text)"""
    found = []
    for source in SCHEMA_SOURCES:
        text = re.sub(r"/\*.*?\*/", "", source.read_text(encoding="utf-8"), flags=re.S)
        for match in re.finditer(r"TAG_SCHEMA_GROUPS\(([^)]*)\)", text):
            named = set(re.findall(r"'(.{4})'", match.group(1)))
            if named & groups:
                # (the field it is in: back to the entry's opening, a field macro or a brace)
                openings = list(re.finditer(r"TAG_SCHEMA_(?!OFFSET\b|GROUPS\b)[A-Z_]+\(|\{ _tag_schema_",
                                            text[:match.start()]))
                found.append((source.name, text[openings[-1].start():match.end()]))
    return found


def test_the_builders_optional_groups_are_the_planned_ones():
    text = BUILDER_UNIT.read_text(encoding="utf-8")
    table = re.search(r"optional_external_groups\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    assert table, "ae_pack_build.c's optional_external_groups table"
    assert set(re.findall(r"\{\s*'(.{4})'", table.group(1))) == OPTIONAL_GROUPS


def test_every_reference_to_an_optional_group_may_be_none():
    """a pack's external tag of one of these groups resolves to NONE on a map without it: each field naming one
    (a reference or a tag index) must be one the schema lets be NONE, and the validator itself names none"""
    header = (ROOT / "port" / "linux" / "game" / "tag_schema.h").read_text(encoding="utf-8")
    for macro in ("TAG_SCHEMA_REFERENCE", "TAG_SCHEMA_REFERENCE_ARRAY", "TAG_SCHEMA_TAG_INDEX"):
        definition = re.search(r"#define " + macro + r"\(.*?(?=\n#define|\n/\*)", header, re.S).group(0)
        assert "FLAG(_tag_schema_none_bit)" in definition, macro
    fields = schema_fields_naming(OPTIONAL_GROUPS)
    assert len(fields) > 60
    for source, field in fields:
        assert (re.match(r"TAG_SCHEMA_(REFERENCE|REFERENCE_ARRAY|TAG_INDEX)\(", field) or
                (field.startswith("{ _tag_schema_reference") and "FLAG(_tag_schema_none_bit)" in field)), \
            (source, field)
    validator = VALIDATOR_SOURCE.read_text(encoding="utf-8")
    for group in OPTIONAL_GROUPS:
        assert f"'{group}'" not in validator, group


def test_the_builder_unit_is_left_out_of_the_game():
    from tools import linux_build
    config = {"game_sources": "port/linux/game"}
    cwd = os.getcwd()
    os.chdir(ROOT)
    try:
        sources = [source.name for source in linux_build.port_game_sources(config)]
    finally:
        os.chdir(cwd)
    assert "ae_pack_format.c" in sources
    assert "ae_pack_build.c" not in sources


# ---------- the build box: real donors

def pack_names(parsed):
    strings = parsed.strings

    def text(offset):
        return None if offset == aepack.NONE else aepack._string(strings, offset, "").decode("utf-8")

    return [(aepack._group_text(tag[0]), tag[4], tag[5], tag[6], text(tag[3]), text(tag[8])) for tag in parsed.tags]


def read_names(path):
    names = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        group, name = line.split("\t")
        names.add((group, name.lower()))
    return names


@pytest.fixture(scope="module")
def stock_names(tmp_path_factory):
    """the stock multiplayer maps' (group, name), as --list-names writes them"""
    if not BUILDER.is_file() or not all((STOCK_MAPS / f"{name}.map").is_file() for name in MULTIPLAYER_MAPS):
        pytest.skip("the stock maps are on the build box")
    path = tmp_path_factory.mktemp("names") / "stock-names.txt"
    result = run("--list-names", "--out", path, *(STOCK_MAPS / f"{name}.map" for name in MULTIPLAYER_MAPS), check=0)
    print(result.stdout)
    return path


def build_pack(work, donor, weapon, externals, name, *extra, check=0):
    files = side_files(work)
    out = work / name
    result = run(*arguments(donor, weapon, externals, out, *extra, texts=files["texts"], credits=files["credits"]),
                 "--verbose", check=check)
    print(result.stdout, result.stderr)
    return out, result


def check_pack(path, stock):
    """the pack accepted by both readers; own tags under the prefix and none of them a stock tag; externals all
    stock tags, required unless of an optional group"""
    checked = subprocess.run([str(CHECKER), str(path)], capture_output=True, text=True)
    assert checked.returncode == 0, checked.stdout + checked.stderr
    assert checked.stdout.strip().endswith(f"{path}: ok")
    parsed = aepack.read(path.read_bytes())
    tags = pack_names(parsed)
    own = [tag for tag in tags if tag[1] == aepack.OWN]
    external = [tag for tag in tags if tag[1] == aepack.EXTERNAL]
    for group, _, _, _, name, source in own:
        assert name.startswith(PREFIX), name
        assert (group, source.lower()) not in stock, (group, source)
        stripped = re.sub(r"^(cmt|ae_pack)\\", "", source, flags=re.I)
        assert name == PREFIX + stripped, (name, source)
    for group, _, required, flags, name, source in external:
        assert (group, name.lower()) in stock, (group, name)
        assert required == (0 if group in OPTIONAL_GROUPS else 1), (group, name, required)
        assert flags == 0
    assert parsed.weapons[0][1] == 0
    return parsed, own, external


@needs_builder
@needs_stock_maps
def test_list_names_is_sorted_and_whole(stock_names):
    lines = stock_names.read_text(encoding="utf-8").splitlines()
    keys = [(line.split("\t")[0], line.split("\t")[1].lower()) for line in lines]
    assert keys == sorted(set(keys))
    assert ("weap", "weapons\\assault rifle\\assault rifle") in set(keys)
    assert ("scnr", "levels\\test\\bloodgulch\\bloodgulch") in set(keys)


@needs_builder
@needs_stock_maps
@pytest.mark.skipif(not CEX_DONOR.is_file(), reason="CE+ X's Blood Gulch is on the build box")
def test_cex_blood_gulch_donor(work, stock_names):
    stock = read_names(stock_names)
    first, result = build_pack(work, CEX_DONOR, CEX_WEAPON, stock_names, "cex-1.aepack")
    parsed, own, external = check_pack(first, stock)
    assert own[0][4] == PREFIX + CEX_WEAPON and own[0][0] == "weap"
    print(f"CE+ X Blood Gulch: {len(parsed.tags)} tags, {len(own)} own, {len(external)} external "
          f"({sum(1 for tag in external if tag[2])} required)")
    second, _ = build_pack(work, CEX_DONOR, CEX_WEAPON, stock_names, "cex-2.aepack")
    assert first.read_bytes() == second.read_bytes()


@needs_builder
@needs_stock_maps
def test_a_donor_without_the_weapon_is_refused(work, stock_names):
    out, result = build_pack(work, STOCK_MAPS / "bloodgulch.map", CEX_WEAPON, stock_names, "none.aepack", check=1)
    assert "refused: the donor has no weapon named" in result.stderr
    assert not out.exists()


def v6e_present():
    if not V6E_DONOR.is_file():
        return False
    return hashlib.sha256(V6E_DONOR.read_bytes()).hexdigest() == V6E_SHA256


@needs_builder
@needs_stock_maps
@pytest.mark.skipif(not v6e_present(), reason="the approved v6e donor is on the build box")
def test_v6e_donor(work, stock_names):
    stock = read_names(stock_names)
    smooth = ["--smooth", f"{V6E_METER}:coverage", "--smooth", V6E_ALPHAS]
    first, result = build_pack(work, V6E_DONOR, V6E_WEAPON, stock_names, "v6e-1.aepack", "--icon", f"{V6E_ICON}:0",
                               *smooth)
    parsed, own, external = check_pack(first, stock)
    tags = pack_names(parsed)
    assert own[0][4] == PREFIX + "weapons\\evolved\\battle_rifle\\battle_rifle"
    by_source = {(tag[0], tag[5]): (index, tag) for index, tag in enumerate(tags) if tag[1] == aepack.OWN}
    meter = by_source[("bitm", V6E_METER)]
    alphas = by_source[("bitm", V6E_ALPHAS)]
    icon = by_source[("bitm", V6E_ICON)]
    assert meter[1][3] == aepack.FLAG_SMOOTH | aepack.FLAG_METER_COVERAGE
    assert alphas[1][3] == aepack.FLAG_SMOOTH
    assert sum(1 for tag in tags if tag[3]) == 2
    weapon = parsed.weapons[0]
    assert (weapon[10], weapon[11]) == (icon[0], 0)
    print(f"v6e: {len(parsed.tags)} tags, {len(own)} own, {len(external)} external "
          f"({sum(1 for tag in external if tag[2])} required)")
    second, _ = build_pack(work, V6E_DONOR, V6E_WEAPON, stock_names, "v6e-2.aepack", "--icon", f"{V6E_ICON}:0",
                           *smooth)
    assert first.read_bytes() == second.read_bytes()
    # the counter's sheet as the icon: drawn whole, no sprites (round 6b's bug): refused
    out, result = build_pack(work, V6E_DONOR, V6E_WEAPON, stock_names, "v6e-icon.aepack", "--icon",
                             f"{V6E_METER}:0", *smooth, check=1)
    assert "has no sprites" in result.stderr
    assert not out.exists()
