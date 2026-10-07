"""The map name a network client takes from its host's game settings
(source/networking/network_client_manager.c, network_game_client_map_name_is_valid):
a scenario's path of letters, digits and _ - . space and backslash, whose
parts may end in a map family's suffix (<file>@ce, <file>@md:
port/linux/include/halo_map_families.h), as ChupathingyCE names Custom
Edition and HaloMD maps. The two functions are taken from the source as
they are and built with the families' suffixes of map_families.c."""
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "source" / "networking" / "network_client_manager.c"
FAMILIES = ROOT / "port" / "linux" / "game" / "map_families.c"

ACCEPTED = [
    "levels\\test\\bloodgulch\\bloodgulch",
    "bloodgulch",
    "levels\\test\\beavercreek_v2@ce\\beavercreek_v2@ce",
    "levels\\test\\H2_Zanzibar@ce\\H2_Zanzibar@ce",
    "beavercreek_v2@ce",
    "levels\\test\\some map@md\\some map@md",
    "levels\\a10\\a10",
    "ui.v2",
]

REJECTED = [
    "",                                 # no name
    "levels\\",                         # an empty leaf
    "levels\\test\\..",                 # traversal
    "levels\\..\\..\\x",                # traversal
    "..@ce",                            # traversal before a suffix
    "levels\\ . ",                      # a leaf of dots and spaces only
    "levels\\..@ce",                    # dots only, then a suffix
    "levels\\ . @ce",                   # dots and spaces only, then a suffix
    "@ce",                              # a bare suffix
    "@md",
    "levels\\@ce",                      # a part that is only a suffix
    "levels\\@ce\\x",
    "a@ce@ce",                          # a suffix not ending its part
    "x@ceb",
    "levels\\x@zz\\x",                  # no such family
    "x@CE",                             # suffixes are lower case
    "a/b",                              # a slash
    "c:\\x",                            # a drive
    "x\ty",                             # a control character
    "x%s",
]


def extract(text):
    start = text.index("#ifdef HALO_CUSTOM_EDITION\n/* port: the length of a map family")
    definition = text.index("static boolean network_game_client_map_name_is_valid(\n\tchar const *map_name,\n\tlong size)\n{")
    end = text.index("\n}\n", definition) + 3
    return text[start:end]


def suffixes():
    text = FAMILIES.read_text(encoding="utf-8")
    found = re.findall(r'\{\s*"(@\w+)",\s*"[^"]*",\s*"[^"]*"\s*\}', text)
    assert found == ["@ce", "@md"], found
    return found


def c_string(name):
    return '"' + "".join(f"\\{ord(c):03o}" if not c.isalnum() else c for c in name) + '"'


def test_map_names(tmp_path):
    compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        pytest.skip("needs a C compiler")
    ce, md = suffixes()
    program = tmp_path / "map_names.c"
    cases = "".join(f"\t{{ {c_string(name)}, 1 }},\n" for name in ACCEPTED) + \
        "".join(f"\t{{ {c_string(name)}, 0 }},\n" for name in REJECTED)
    program.write_text(f"""#include <stdio.h>
#include <string.h>
#define HALO_CUSTOM_EDITION
typedef int boolean;
#define FALSE 0
#define TRUE 1
enum {{ _map_family_xbox, _map_family_custom_edition, _map_family_halomd, NUMBER_OF_MAP_FAMILIES }};
static char const *map_family_suffix(short family)
{{
	return family == _map_family_custom_edition ? "{ce}" : family == _map_family_halomd ? "{md}" : "";
}}
{extract(SOURCE.read_text(encoding="utf-8"))}
static struct {{ char const *name; int valid; }} const cases[] = {{
{cases}}};
int main(void)
{{
	unsigned index;
	int failed = 0;
	char unterminated[8];

	for (index = 0; index < sizeof(cases) / sizeof(*cases); index++)
	{{
		int valid = network_game_client_map_name_is_valid(cases[index].name, 128) != 0;

		if (valid != cases[index].valid)
		{{
			printf("FAIL %s: %d\\n", cases[index].name, valid);
			failed = 1;
		}}
	}}
	/* (a name that does not end in its field) */
	memset(unterminated, 'a', sizeof(unterminated));
	if (network_game_client_map_name_is_valid(unterminated, sizeof(unterminated)))
	{{
		printf("FAIL unterminated\\n");
		failed = 1;
	}}
	if (!failed)
		printf("PASS\\n");
	return failed;
}}
""", encoding="utf-8")
    binary = tmp_path / "map_names"
    built = subprocess.run([compiler, "-std=gnu99", "-Wall", "-Werror", "-o", str(binary), str(program)],
                           capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-4000:]
    result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0 and "PASS" in result.stdout, result.stdout
