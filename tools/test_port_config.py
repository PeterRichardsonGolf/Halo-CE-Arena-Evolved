"""Check config.toml's handling of defaults (port/linux/src/port_config.c).

Builds port_config.c on its own with tools/tests/port_config_test.c (SDL's
few functions stood in for with the C library), once for each kind of build
whose defaults differ, and runs it on config.toml files of each history a
player's can have:

- a new file: every setting commented out at its default;
- a file from before config_version, every setting written as a value:
  values at this version's default are commented out, every other kept
  (an earlier default may be the player's choice);
- a file of this layout that an older version then ran on, which added each
  commented-out setting again as a value at its default: left alone until the
  player answers the question about the new defaults, then those values are
  taken out ("use the new settings") or stay ("keep mine");
- a file of the previous release, with the old defaults commented out: the
  settings keep their old values and the file stays as it is, the player is
  asked once, "keep mine" writes the old values and "use the new settings"
  lets them follow the new defaults; the question never comes again;
- a file with a setting under its older name: a value the player chose
  goes on under the new one (Arena Evolved: config_carry_over);
- the per-platform defaults of each build (DEFAULTS below).

Needs only a C compiler. Usage: test_port_config.py [--cc clang]
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# the defaults this version gives each kind of build, for the settings whose
# default is not the same everywhere or changed in this version (Arena
# Evolved: the Xbox's anti-aliasing and shadows everywhere, as before;
# ChupathingyCE 0.8's new-install defaults, 2x multisampling and 256 shadows,
# are not taken, so no existing file is asked about them)
DEFAULTS = {
	"desktop": {
		"display.anti_aliasing": "off",
		"display.anisotropic": "off",
		"display.model_detail": "stock",
		"display.per_pixel_lighting": "false",
		"input.aim_control": "modern",
		"display.enemy_name_color": "red",
		"network.shield_effects": "glow_overshield",
		"display.shield_glow": "false",
		"display.overshield_effect": "false",
		"display.viewmodel_shield": "false",
		"game.callouts": "items_clock",
		"display.powerup_timers": "true",
		"network.map_share_internet": "true",
		"input.zoom_mode": "both",
		"network.browser_pings": "true",
		"input.wheel_zoom": "true",
		"display.fov": "0",
		"display.viewmodel_fov": "0",
		"display.match_clock": "both",
		"display.first_person_legs": "false",
		"display.zoom_blur": "false",
		"game.announcer": "auto",
		"controls.zoom": "Z, Mouse Right",
		"controls.throw_grenade": "G",
		"network.map_share_upload": "4096",
		"network.map_seed_upload": "1024",
		"game.callout_detail": "verbose",
		"input.mouse_style": "mcc",
		"display.shadow_resolution": "128",
	},
	"desktop32": {
		"display.anti_aliasing": "off",
		"display.anisotropic": "off",
		"display.model_detail": "stock",
		"display.per_pixel_lighting": "false",
		"input.aim_control": "modern",
		"display.enemy_name_color": "red",
		"network.shield_effects": "glow_overshield",
		"display.shield_glow": "false",
		"display.overshield_effect": "false",
		"display.viewmodel_shield": "false",
		"game.callouts": "items_clock",
		"display.powerup_timers": "true",
		"network.map_share_internet": "true",
		"input.zoom_mode": "both",
		"network.browser_pings": "true",
		"input.wheel_zoom": "true",
		"display.fov": "0",
		"display.viewmodel_fov": "0",
		"display.match_clock": "both",
		"display.first_person_legs": "false",
		"display.zoom_blur": "false",
		"game.announcer": "auto",
		"controls.zoom": "Z, Mouse Right",
		"controls.throw_grenade": "G",
		"network.map_share_upload": "4096",
		"network.map_seed_upload": "1024",
		"game.callout_detail": "verbose",
		"input.mouse_style": "mcc",
		"display.shadow_resolution": "128",
		"audio.buffer_frames": "512",
	},
	"android": {
		"display.anti_aliasing": "off",
		"display.anisotropic": "off",
		"display.model_detail": "stock",
		"display.per_pixel_lighting": "false",
		"input.aim_control": "modern",
		"display.enemy_name_color": "red",
		"network.shield_effects": "glow_overshield",
		"display.shield_glow": "false",
		"display.overshield_effect": "false",
		"display.viewmodel_shield": "false",
		"game.callouts": "items_clock",
		"display.powerup_timers": "true",
		"network.map_share_internet": "true",
		"input.zoom_mode": "both",
		"network.browser_pings": "true",
		"input.wheel_zoom": "true",
		"display.fov": "0",
		"display.viewmodel_fov": "0",
		"display.match_clock": "both",
		"display.first_person_legs": "false",
		"display.zoom_blur": "false",
		"game.announcer": "auto",
		"controls.zoom": "Z, Mouse Right",
		"controls.throw_grenade": "G",
		"network.map_share_upload": "4096",
		"network.map_seed_upload": "1024",
		"game.callout_detail": "verbose",
		"display.shadow_resolution": "128",
		"audio.buffer_frames": "1024",
	},
	"server": {
		"display.anti_aliasing": "off",
		"display.anisotropic": "off",
		"display.model_detail": "stock",
		"display.per_pixel_lighting": "false",
		"input.aim_control": "modern",
		"display.enemy_name_color": "red",
		"network.shield_effects": "glow_overshield",
		"display.shield_glow": "false",
		"display.overshield_effect": "false",
		"display.viewmodel_shield": "false",
		"game.callouts": "items_clock",
		"display.powerup_timers": "true",
		"network.map_share_internet": "true",
		"input.zoom_mode": "both",
		"network.browser_pings": "true",
		"input.wheel_zoom": "true",
		"display.fov": "0",
		"display.viewmodel_fov": "0",
		"display.match_clock": "both",
		"display.first_person_legs": "false",
		"display.zoom_blur": "false",
		"game.announcer": "auto",
		"controls.zoom": "Z, Mouse Right",
		"controls.throw_grenade": "G",
		"network.map_share_upload": "10240",
		"network.map_seed_upload": "1024",
		"game.callout_detail": "verbose",
		"input.mouse_style": "mcc",
		"display.shadow_resolution": "128",
	},
}

# the settings an existing file would see change, for each kind of build, with
# the value the previous release had (the dedicated server is not asked: its
# defaults are its own). Arena Evolved changes none of its defaults here: its
# menus stay "pc", and nothing is asked
OLD = {
	"display.menus": "xbox",
	"display.shadow_resolution": "128",
	"audio.buffer_frames": "512",
	"input.aim_control": "classic",
	"display.enemy_name_color": "classic",
	"network.shield_effects": "stock",
	"display.shield_glow": "false",
	"display.overshield_effect": "false",
	"display.viewmodel_shield": "false",
	"game.callouts": "off",
	"game.callout_detail": "standard",
	"display.powerup_timers": "false",
	"display.anisotropic": "off",
	"display.anti_aliasing": "off",
	"display.model_detail": "stock",
	"display.per_pixel_lighting": "false",
	"network.map_share_internet": "false",
	"network.map_share_upload": "1024",
	"network.map_seed_upload": "256",
	"input.zoom_mode": "toggle",
	"network.browser_pings": "false",
	"display.fov": "0.0",
	"display.viewmodel_fov": "0.0",
	"display.match_clock": "down",
	"display.first_person_legs": "false",
	"display.zoom_blur": "true",
	"game.announcer": "classic",
}
# (0.7.8) the line each change has in the question, one line for the settings that are one change
CHANGE_LINES = {
	"input.aim_control": "Modern aim on the controller",
	"display.enemy_name_color": "Red enemy names",
	"network.shield_effects": "Shield glow and the overshield look",
	"display.shield_glow": "Shield glow and the overshield look",
	"display.overshield_effect": "Shield glow and the overshield look",
	"display.viewmodel_shield": "Shield glow and the overshield look",
	"game.callouts": "Item and match callouts (spoken)",
	"game.callout_detail": "Item and match callouts (spoken)",
	"display.powerup_timers": "Powerup timers",
	"display.anisotropic": "Sharper textures and smoother edges (PC)",
	"display.anti_aliasing": "Sharper textures and smoother edges (PC)",
	"display.model_detail": "More detail on far models, per-pixel lighting (PC)",
	"display.per_pixel_lighting": "More detail on far models, per-pixel lighting (PC)",
	"network.map_share_internet": "Hosts send maps to internet players",
	"network.map_share_upload": "Faster map sharing",
	"network.map_seed_upload": "Faster map sharing",
	"input.zoom_mode": "Zoom: a tap steps the zoom, a hold zooms while held",
	"network.browser_pings": "Server pings in the server browser",
	"display.fov": "Wider field of view (85)",
	"display.viewmodel_fov": "Wider field of view (85)",
	"display.match_clock": "Match clock with the time left and the time played",
	"display.first_person_legs": "Your own legs in first person",
	"display.zoom_blur": "No blur at the scope's edges",
	"game.announcer": "The MCC announcer, when MCC is installed",
}
# (0.8.1, owner 11-11: these went back to OpenCE's defaults for a new install, so a file from before 0.7.8 is not asked
# about them; a file of 0.7.8 to 0.8.0 keeps its values: test_stock_graphics_new_installs_only)
STOCK_GRAPHICS = {'display.anti_aliasing': '"smaa"', 'display.anisotropic': '"16x"', 'display.model_detail': '"native"', 'display.per_pixel_lighting': 'true', 'display.shield_glow': 'true', 'display.overshield_effect': 'true', 'display.viewmodel_shield': 'true', 'display.fov': '85.0', 'display.viewmodel_fov': '85.0', 'display.first_person_legs': 'true'}
_ASKED = [name for name in CHANGE_LINES if name not in STOCK_GRAPHICS]
CHANGED = {
	"desktop": _ASKED,
	"desktop32": _ASKED,
	"android": [n for n in _ASKED if n not in ("display.anisotropic", "display.anti_aliasing", "display.model_detail", "display.per_pixel_lighting")],
	"server": [],
}
NEW = {}

# the macros each kind of build is compiled with; the others are not macOS
# (whose own defaults the native build checks on a Mac)
DEFINES = {
	"desktop": ["-DHALO_64BIT", "-DHALO_GAME_BROWSER"],
	"desktop32": ["-U__APPLE__", "-DHALO_GAME_BROWSER"],
	# (Android has no command line: OpenCE's config_probe_mode is unused there)
	"android": ["-U__APPLE__", "-DHALO_ANDROID", "-DHALO_GAME_BROWSER", "-Wno-unused-variable"],
	"server": ["-U__APPLE__", "-DHALO_SERVER", "-DHALO_64BIT", "-DHALO_GAME_BROWSER"],
}

STUB_PLATFORM = """#include <stddef.h>
void platform_log(const char *format, ...);
"""

STUB_POSIX = """#include <sys/stat.h>
typedef unsigned long posix_ulong;
struct posix_file_information
{
	posix_ulong flags, size_low, size_high, modification_seconds, modification_nanoseconds,
		access_seconds, access_nanoseconds, creation_seconds, creation_nanoseconds;
};
static int posix_stat(const char *path, struct posix_file_information *information)
{
	struct stat st;

	if (stat(path, &st) != 0)
		return -1;
	information->flags = 0;
	information->size_low = (posix_ulong)st.st_size;
	information->size_high = 0;
	information->modification_seconds = (posix_ulong)st.st_mtime;
	information->modification_nanoseconds = 0;
	return 0;
}
enum { _posix_file_is_directory = 1 << 0 };
static void *posix_directory_open(const char *path) { (void)path; return 0; }
static int posix_directory_next(void *directory, char *name, unsigned long size)
{ (void)directory; (void)name; (void)size; return 0; }
static void posix_directory_close(void *directory) { (void)directory; }
"""

# (Arena Evolved's port_config.c pauses the capture for its restart, capture.h, and lists folders, posix.h)
STUB_CAPTURE = """static inline void capture_shutdown(void) {}
static inline void capture_resume(void) {}
"""

STUB_SDL = """#include <stdbool.h>
#include <stddef.h>
const char *SDL_GetBasePath(void);
bool SDL_CreateDirectory(const char *path);
void *SDL_LoadFile(const char *path, size_t *size);
bool SDL_SaveFile(const char *path, const void *data, size_t size);
void SDL_free(void *memory);
typedef struct { long long size; long long modify_time; } SDL_PathInfo;
bool SDL_GetPathInfo(const char *path, SDL_PathInfo *info);
int SDL_strncasecmp(const char *first, const char *second, size_t length);
"""


def build(cc, kind, directory):
	"""The test program for a kind of build, in directory."""
	source = directory / "source"
	(source / "SDL3").mkdir(parents=True, exist_ok=True)
	shutil.copy(ROOT / "port/linux/src/port_config.c", source)
	shutil.copy(ROOT / "port/linux/src/port_config.h", source)
	# (Arena Evolved 0.3.7: the players' own settings, port_config.c's hook: ae_config_players.c)
	shutil.copy(ROOT / "port/linux/src/ae_config_players.c", source)
	shutil.copy(ROOT / "port/linux/src/ae_config_players.h", source)
	(source / "platform.h").write_text(STUB_PLATFORM)
	(source / "posix.h").write_text("#ifndef STUB_POSIX_H\n#define STUB_POSIX_H\n" + STUB_POSIX + "\n#endif\n")
	(source / "capture.h").write_text(STUB_CAPTURE)
	# (the dedicated server's backups: port_config.c copies a config file before it writes one)
	(source / "ae_server_admin.h").write_text("static inline void ae_server_backup_file(const char *path) { (void)path; }\n")
	# (OpenCE's dedicated server: a secret's file kept private, posix_server.c's)
	(source / "posix_server.h").write_text(
		"static inline int posix_server_private_file(const char *path) { (void)path; return 1; }\n"
		"static inline int posix_server_write_private_file(const char *path, const char *text, unsigned long size)\n"
		"{ (void)path; (void)text; (void)size; return 0; }\n"
		# (and its command line, which this test's program has none of)
		"static inline int posix_command_line_argument(int index, char *buffer, unsigned long size)\n"
		"{ (void)index; (void)buffer; (void)size; return 0; }\n")
	(source / "SDL3/SDL.h").write_text(STUB_SDL)
	program = directory / f"port_config_test_{kind}"
	subprocess.run([cc, "-std=c11", "-D_DEFAULT_SOURCE", "-D_DARWIN_C_SOURCE", "-Wall", "-Werror", "-g",
		*DEFINES[kind], "-I", str(source), "-I", str(ROOT / "port/third_party/tomlc17"),
		str(source / "port_config.c"), str(source / "ae_config_players.c"), str(ROOT / "tools/tests/port_config_test.c"),
		str(ROOT / "port/third_party/tomlc17/tomlc17.c"), "-lpthread", "-o", str(program)],
		check=True)
	return program


class Case:
	def __init__(self, program, directory):
		self.program = program
		self.folder = directory
		self.folder.mkdir(parents=True, exist_ok=True)
		self.path = self.folder / "config.toml"

	def run(self, *arguments):
		environment = {key: value for key, value in os.environ.items() if not key.startswith("HALO_")}
		environment["PORT_CONFIG_TEST_FOLDER"] = str(self.folder)
		environment["HALO_DATA_ROOT"] = str(self.folder)
		result = subprocess.run([str(self.program), *arguments], capture_output=True, text=True,
			env=environment, check=True)
		values = dict(re.findall(r"^([a-z_]+\.[a-z_0-9]+)=(.*)$", result.stdout, re.M))
		return values, result.stdout

	def get(self, *names):
		return self.run("get", *names)

	def text(self):
		return self.path.read_text()


def value_lines(text, key):
	"""The file's lines "key = ..." and "# key = ..." for a key."""
	values = re.findall(rf"^\s*{key}\s*=.*$", text, re.M)
	commented = re.findall(rf"^\s*#\s*{key}\s*=.*$", text, re.M)
	return values, commented


def check(condition, message):
	if not condition:
		raise AssertionError(message)


def pending(case):
	"""The count of changes the player would be asked about, and their words."""
	_, output = case.run("pending")
	return int(re.search(r"^pending=(\d+)$", output, re.M).group(1)), output


def new_value(name, defaults):
	return NEW.get(name) or defaults[name]


def old_value_text(name):
	return OLD[name] if re.fullmatch(r"[0-9.]+|true|false", OLD[name]) else f'"{OLD[name]}"'


def previous_release_file(case, kind):
	"""A file as the previous release wrote it: every setting commented out,
	at that release's defaults."""
	case.get("display.vsync")
	text = case.text()
	for name in CHANGED["android" if kind == "server" else kind]:
		key = name.split(".")[1]
		text = re.sub(rf"^# {key} = .*$", f"# {key} = {old_value_text(name)}", text, flags=re.M)
	case.path.write_text(text)
	return text


def test_new_file(program, directory, defaults, kind):
	case = Case(program, directory / "new")
	values, _ = case.get(*defaults)
	for name, expected in defaults.items():
		check(values[name] == expected, f"new file: {name} is {values[name]}, not {expected}")
	text = case.text()
	check("config_version = " in text, "new file: no config_version")
	for name, expected in defaults.items():
		key = name.split(".")[1]
		lines, commented = value_lines(text, key)
		check(not lines and len(commented) == 1, f"new file: {key} is not commented out once: {lines} {commented}")
	# (a new file is not asked about anything, then or on the next start)
	check(pending(case)[0] == 0, "new file: something to ask about")
	before = case.text()
	case.get("display.vsync")
	check(case.text() == before and pending(case)[0] == 0, "new file: changed or asked on the next start")


def test_previous_release_file(program, directory, defaults, kind):
	"""The question: the previous release's file keeps its old values and
	stays as it is until the player answers, then once for all."""
	changed = CHANGED[kind]
	names = list(changed) or ["display.vsync"]
	for answer in ("keep", "new"):
		case = Case(program, directory / f"previous_{answer}")
		written = previous_release_file(case, kind)
		values, _ = case.get(*names)
		for name in changed:
			check(float_equal(values[name], OLD[name]) or values[name] == OLD[name], f"previous/{answer}: {name} is {values[name]} before the answer")
		check(case.text() == written, f"previous/{answer}: the file changed before the answer")
		count, output = pending(case)
		lines = {CHANGE_LINES[name] for name in changed}
		check(count == len(lines), f"previous/{answer}: {count} lines, not {len(lines)}: {output}")
		for line in lines:
			check(line in output, f"previous/{answer}: no line {line!r}")
		if not changed:
			continue
		case.run("answer", answer)
		text = case.text()
		check(re.search(r"^defaults_answered = 2$", text, re.M), f"previous/{answer}: the answer is not written")
		# (asked once: a restart finds nothing, and changes nothing)
		check(pending(case)[0] == 0, f"previous/{answer}: asked again")
		values, _ = case.get(*names)
		before = case.text()
		case.get("display.vsync")
		check(case.text() == before, f"previous/{answer}: the file changed on the next start")
		for name in changed:
			key = name.split(".")[1]
			lines, commented = value_lines(text, key)
			if answer == "keep":
				check(float_equal(values[name], OLD[name]) or values[name] == OLD[name], f"previous/keep: {name} is {values[name]}, not {OLD[name]}")
				check(lines == [f"{key} = {old_value_text(name)}"], f"previous/keep: {key} not pinned: {lines}")
			else:
				check(float_equal(values[name], new_value(name, defaults)) or values[name] == new_value(name, defaults), f"previous/new: {name} is {values[name]}")
				check(not lines and len(commented) == 1 and re.sub(r'["\s]', "", commented[0]).split("=")[1].rstrip("0.") ==
					values[name].rstrip("0."), f"previous/new: {key}: {lines} {commented}")


def test_file_without_the_settings(program, directory, defaults, kind):
	"""A file from before the settings existed: they were at the old
	defaults; asked about, and the file is left alone until answered."""
	changed = CHANGED[kind]
	if not changed:
		return
	case = Case(program, directory / "without")
	case.path.write_text("config_version = 2\n\n[display]\nfullscreen = false\n")
	values, _ = case.get(*changed)
	for name in changed:
		check(float_equal(values[name], OLD[name]) or values[name] == OLD[name], f"without: {name} is {values[name]}")
		lines, commented = value_lines(case.text(), name.split(".")[1])
		check(not lines and not commented, f"without: {name} was added before the answer: {lines} {commented}")
	check(pending(case)[0] == len({CHANGE_LINES[name] for name in changed}), "without: not asked about every change")
	case.run("answer", "keep")
	values, _ = case.get(*changed)
	check(all(float_equal(values[name], OLD[name]) or values[name] == OLD[name] for name in changed), f"without: keep: {values}")
	check(pending(case)[0] == 0, "without: asked again")
	check("fullscreen = false" in case.text(), "without: the player's fullscreen was lost")


def test_readded_by_older_version(program, directory, defaults, kind):
	"""The owner's case: a config_version 2 file that a build from before
	config_version then ran on, adding every setting again as a value. It is
	left alone until the player answers: use the new settings takes the
	values out, keep mine leaves them."""
	changed = CHANGED[kind]
	for answer in ("new", "keep"):
		case = Case(program, directory / f"readded_{answer}")
		case.get("display.vsync")
		text = case.text()
		# what that older build appends to [display]: its default (the old one
		# for each setting this version changed) and one the player chose
		old = {"shadow_resolution": "128", "anti_aliasing": "\"off\"", "interpolation": "true", "vsync": "true",
			"high_res_hud": "false"}
		block = "".join(f"\n# An older version's comment for {key}.\n# (for one run: HALO_X=<value>)\n{key} = {value}\n"
			for key, value in old.items())
		# (at the end of [display], whichever section follows it: OpenCE's button settings put [input] there)
		audio = text.index("\n[", text.index("\n[display]") + 1)
		text = text[:audio].rstrip("\n") + "\n" + block + text[audio:]
		case.path.write_text(text)
		values, _ = case.get("display.shadow_resolution", "display.anti_aliasing", "display.interpolation",
			"display.vsync", "display.high_res_hud")
		check(values["display.shadow_resolution"] == "128", f"readded: shadow_resolution is {values['display.shadow_resolution']}")
		check(values["display.anti_aliasing"] == "off", f"readded: anti_aliasing is {values['display.anti_aliasing']}")
		check(values["display.high_res_hud"] == "false", "readded: the player's high_res_hud = false was lost")
		check(case.text() == text, f"readded/{answer}: the file changed before the answer")
		count, _ = pending(case)
		wanted = len([name for name in changed if name in ("display.shadow_resolution", "display.anti_aliasing")])
		# (the menus and Android's buffer were not added as values here)
		check(count == wanted, f"readded/{answer}: {count} changes, not {wanted}")
		if not wanted:
			continue
		case.run("answer", answer)
		values, _ = case.get("display.shadow_resolution", "display.anti_aliasing", "display.high_res_hud")
		check(values["display.high_res_hud"] == "false", f"readded/{answer}: high_res_hud lost")
		text = case.text()
		for key in ("shadow_resolution", "anti_aliasing"):
			lines, commented = value_lines(text, key)
			if answer == "new":
				check(not lines and len(commented) == 1, f"readded/new: {key}: {lines} {commented}")
				check(f"An older version's comment for {key}." not in text, f"readded/new: {key}'s added comment stayed")
			else:
				check(len(lines) == 1, f"readded/keep: {key}: {lines}")
		if answer == "new":
			check(values["display.shadow_resolution"] == defaults["display.shadow_resolution"],
				f"readded/new: shadow_resolution stayed {values['display.shadow_resolution']}")
			check(values["display.anti_aliasing"] == defaults["display.anti_aliasing"],
				f"readded/new: anti_aliasing stayed {values['display.anti_aliasing']}")
			lines, commented = value_lines(text, "interpolation")
			check(not lines and len(commented) == 1, f"readded/new: interpolation: {lines} {commented}")
		else:
			check(values["display.shadow_resolution"] == "128" and values["display.anti_aliasing"] == "off",
				"readded/keep: the old values were lost")
		lines, _ = value_lines(text, "high_res_hud")
		check(lines == ["high_res_hud = false"], f"readded/{answer}: high_res_hud: {lines}")
		# (nothing more to do the second time)
		check(pending(case)[0] == 0, f"readded/{answer}: asked again")
		before = case.text()
		case.get("display.vsync")
		check(case.text() == before, f"readded/{answer}: the file changed again on the next start")


def test_before_config_version(program, directory, defaults, kind):
	"""A file from before config_version: every setting a value. Those at
	this version's default are commented out; an earlier default is kept,
	being possibly the player's choice (existing players keep their
	settings: the new defaults are for new files)."""
	case = Case(program, directory / "version1")
	case.get("display.vsync")
	text = case.text()
	# every commented-out setting as a value, the way version 1 wrote them,
	# at the old defaults for those this version changed
	old = {"shadow_resolution": "128", "anti_aliasing": "\"off\""}
	text = re.sub(r"^config_version = \d+\n", "", text, flags=re.M)
	text = text.replace("# The layout of this file, for the game: leave it.\n", "")

	def value(match):
		key = match.group(1)
		return f"{key} = {old.get(key, match.group(2))}"
	text = re.sub(r"^# ([a-z_0-9]+) = (.*)$", value, text, flags=re.M)
	text = text.replace("volume = 1.0", "volume = 0.5", 1)
	case.path.write_text(text)
	values, _ = case.get("display.shadow_resolution", "display.anti_aliasing", "audio.volume")
	check(values["display.shadow_resolution"] == "128", f"version 1: shadow_resolution {values['display.shadow_resolution']}")
	check(values["display.anti_aliasing"] == "off", f"version 1: anti_aliasing {values['display.anti_aliasing']}")
	check(values["audio.volume"] == "0.5", "version 1: the player's audio.volume was lost")
	text = case.text()
	check("config_version = 2" in text, "version 1: no config_version written")
	# (values the player may have chosen are theirs: nothing to ask about)
	check(pending(case)[0] == 0, "version 1: asked about values the file holds")
	lines, commented = value_lines(text, "vsync")
	check(not lines and len(commented) == 1, f"version 1: vsync, at its default, is not commented out: {lines} {commented}")


def test_renamed_setting(program, directory, defaults, kind):
	"""A setting under its older name: ChupathingyCE 0.8.0's
	display.loading_screen_min_seconds. A value the player chose goes on
	under the new name, display.loading_screen_campaign_seconds (Arena
	Evolved: config_carry_over reads it into the new one; the old line stays
	as the player wrote it), and the multiplayer one starts at its default;
	a value already under the new name is the one kept."""
	names = ("display.loading_screen", "display.loading_screen_campaign_seconds",
		"display.loading_screen_multiplayer_seconds")
	cases = {
		"chosen": ("loading_screen_min_seconds = 8\n", 8.0),
		"commented": ("# loading_screen_min_seconds = 0.0\n", 0.0),
		"both": ("loading_screen_min_seconds = 8\n\nloading_screen_campaign_seconds = 3.0\n", 3.0),
	}
	for name, (lines, campaign) in cases.items():
		case = Case(program, directory / f"renamed_{name}")
		case.get("display.vsync")
		text = case.text()
		for key in ("loading_screen_campaign_seconds", "loading_screen_min_seconds"):
			text = re.sub(rf"\n(# [^\n]*\n)*# {key} = [^\n]*\n", "\n", text)
		text = text.replace("\n[display]\n", "\n[display]\n" + lines, 1)
		case.path.write_text(text)
		values, output = case.get(*names)
		check(values["display.loading_screen"] == "true", f"renamed/{name}: loading_screen is {values}")
		check(float(values["display.loading_screen_campaign_seconds"]) == campaign,
			f"renamed/{name}: campaign is {values['display.loading_screen_campaign_seconds']}, not {campaign}")
		check(float(values["display.loading_screen_multiplayer_seconds"]) == 0.0,
			f"renamed/{name}: multiplayer is {values['display.loading_screen_multiplayer_seconds']}")
		check("unknown setting" not in output, f"renamed/{name}: the old name is reported: {output}")


# (0.7.8) the old defaults of the settings that changed, as a 0.7.7 file wrote them
SEVEN_SEVEN = {
	"input.aim_control": '"classic"', "display.enemy_name_color": '"classic"', "network.shield_effects": '"stock"',
	"display.shield_glow": "false", "display.overshield_effect": "false", "display.viewmodel_shield": "false",
	"game.callouts": '"off"', "display.powerup_timers": "false", "display.anisotropic": '"off"',
	"display.anti_aliasing": '"off"', "display.model_detail": '"stock"', "display.per_pixel_lighting": "false",
	"network.map_share_internet": "false", "input.zoom_mode": '"toggle"', "network.browser_pings": "false",
	"display.fov": "0", "display.viewmodel_fov": "0", "display.match_clock": '"down"', "display.first_person_legs": "false",
	"display.zoom_blur": "true", "network.map_share_upload": "1024", "network.map_seed_upload": "256",
	"game.callout_detail": '"standard"',
}


def test_new_defaults_of_a_077_file(program, directory, defaults, kind):
	"""A 0.7.7 file whose lines hold the old defaults as values (the player set them): not asked about, kept."""
	names = list(SEVEN_SEVEN)
	case = Case(program, directory / "d078_chosen")
	case.get("display.vsync")
	text = case.text()
	for name, old in SEVEN_SEVEN.items():
		key = name.split(".")[1]
		text = re.sub(rf"^# {key} = .*$", f"{key} = {old}", text, flags=re.M)
	case.path.write_text(text)
	values, _ = case.get(*names)
	for name in names:
		wanted = SEVEN_SEVEN[name].strip('"')
		check(values[name] == wanted or float_equal(values[name], wanted), f"d078/chosen: {name} is {values[name]}, not {wanted}")
	check(pending(case)[0] == 0, "d078/chosen: the player is asked")
	check(re.search(r'^aim_control = "classic"$', case.text(), re.M), "d078/chosen: the line was changed")


def float_equal(first, second):
	try:
		return float(first) == float(second)
	except ValueError:
		return False


def test_mouse_style_new_installs_only(program, directory, defaults, kind):
	"""input.mouse_style is "mcc" for a new install; a file that exists keeps
	"classic" (commented out at it, or without the setting), written as a
	value; a value of the player's is kept, and a file this version made is
	left as it is."""
	if kind == "server":
		return
	# (wheel zoom, every desktop and Android build: a new install's true, a file that exists keeps false)
	for how in ("commented", "absent", "chosen"):
		case = Case(program, directory / f"wheel_{how}")
		case.get("display.vsync")
		text = case.text()
		if how == "commented":
			text = re.sub(r'^# wheel_zoom = .*$', '# wheel_zoom = false', text, flags=re.M)
		elif how == "absent":
			text = re.sub(r'(\n(# [^\n]*\n)*# wheel_zoom = [^\n]*\n)', "\n", text)
		else:
			text = re.sub(r'^# wheel_zoom = .*$', 'wheel_zoom = true', text, flags=re.M)
		case.path.write_text(text)
		values, _ = case.get("input.wheel_zoom")
		wanted = "true" if how == "chosen" else "false"
		check(values["input.wheel_zoom"] == wanted, f"wheel/{how}: {values}")
		check(re.search(rf"^wheel_zoom = {wanted}$", case.text(), re.M), f"wheel/{how}: no value in the file")
		values, _ = case.get("input.wheel_zoom")
		check(values["input.wheel_zoom"] == wanted, f"wheel/{how}: second start {values}")
	case = Case(program, directory / "wheel_new")
	values, _ = case.get("input.wheel_zoom")
	check(values["input.wheel_zoom"] == "true", f"wheel/new: {values}")
	check(re.search(r"^# wheel_zoom = true$", case.text(), re.M), "wheel/new: not commented out at true")
	if kind == "android":
		return
	case = Case(program, directory / "mouse_new")
	values, _ = case.get("input.mouse_style")
	check(values["input.mouse_style"] == "mcc", f"mouse/new: {values}")
	check(re.search(r'^# mouse_style = "mcc"$', case.text(), re.M), "mouse/new: not commented out at mcc")
	text = case.text()
	values, _ = case.get("input.mouse_style")
	check(case.text() == text and values["input.mouse_style"] == "mcc", "mouse/new: the second start changed it")
	# (a file of 0.7.7: commented out at classic)
	case = Case(program, directory / "mouse_old")
	case.get("display.vsync")
	text = re.sub(r'^# mouse_style = .*$', '# mouse_style = "classic"', case.text(), flags=re.M)
	case.path.write_text(text)
	values, _ = case.get("input.mouse_style")
	check(values["input.mouse_style"] == "classic", f"mouse/old: {values}")
	check(re.search(r'^mouse_style = "classic"$', case.text(), re.M), "mouse/old: no value written")
	values, _ = case.get("input.mouse_style")
	check(values["input.mouse_style"] == "classic", f"mouse/old: second start {values}")
	# (a file from before the setting: absent)
	case = Case(program, directory / "mouse_absent")
	case.get("display.vsync")
	text = re.sub(r'(\n(# [^\n]*\n)*# mouse_style = [^\n]*\n)', "\n", case.text())
	check("mouse_style" not in text, "mouse/absent: the test file still has it")
	case.path.write_text(text)
	values, _ = case.get("input.mouse_style")
	check(values["input.mouse_style"] == "classic", f"mouse/absent: {values}")
	check(re.search(r'^mouse_style = "classic"$', case.text(), re.M), "mouse/absent: no value written")
	# (the player's own: kept, whatever it is)
	for chosen in ("mcc", "classic"):
		case = Case(program, directory / f"mouse_{chosen}")
		case.get("display.vsync")
		text = re.sub(r'^# mouse_style = .*$', f'mouse_style = "{chosen}"', case.text(), flags=re.M)
		case.path.write_text(text)
		values, _ = case.get("input.mouse_style")
		check(values["input.mouse_style"] == chosen, f"mouse/{chosen}: {values}")
		check(case.text() == text, f"mouse/{chosen}: the file changed")


def test_stock_graphics_new_installs_only(program, directory, defaults, kind):
	"""(0.8.1, owner 11-11) OpenCE's graphics defaults for a new install (a weak laptop starts light); a file of 0.7.8 to
	0.8.0 (commented out at that release's defaults, or without the settings) keeps them, written as values; a value of
	the player's is kept; a new file is commented out at the stock values and left alone on the next start."""
	if kind == "server":
		return
	names = [n for n in STOCK_GRAPHICS if kind != "android" or n not in
		("display.anisotropic", "display.anti_aliasing", "display.model_detail", "display.per_pixel_lighting")]
	case = Case(program, directory / "stock_new")
	values, _ = case.get(*names)
	for name in names:
		check(values[name] == defaults[name] or float_equal(values[name], defaults[name]), f"stock/new: {name} is {values[name]}")
	text = case.text()
	case.get("display.vsync")
	check(case.text() == text, "stock/new: the second start changed the file")
	for how in ("commented", "absent"):
		case = Case(program, directory / f"stock_{how}")
		case.get("display.vsync")
		text = case.text()
		for name in names:
			key = name.split(".")[1]
			if how == "commented":
				text = re.sub(rf"^# {key} = .*$", f"# {key} = {STOCK_GRAPHICS[name]}", text, flags=re.M)
			else:
				text = re.sub(rf"(\n(# [^\n]*\n)*# {key} = [^\n]*\n)", "\n", text)
		case.path.write_text(text)
		values, _ = case.get(*names)
		for name in names:
			key = name.split(".")[1]
			if how == "commented":
				kept = STOCK_GRAPHICS[name].strip('"')
				check(values[name] == kept or float_equal(values[name], kept), f"stock/{how}: {name} is {values[name]}, not {kept}")
				check(re.search(rf"^{key} = {re.escape(STOCK_GRAPHICS[name])}$", case.text(), re.M), f"stock/{how}: {key} not written")
			else:
				# (a file without the line is from before 0.7.8: the stock value, which the new default is)
				check(values[name] == defaults[name] or float_equal(values[name], defaults[name]),
					f"stock/{how}: {name} is {values[name]}, not {defaults[name]}")
				check(not re.search(rf"^{key} = ", case.text(), re.M), f"stock/{how}: {key} was written as a value")
		check(pending(case)[0] == 0, f"stock/{how}: asked about something")
	case = Case(program, directory / "stock_chosen")
	case.get("display.vsync")
	text = re.sub(r"^# fov = .*$", "fov = 90.0", case.text(), flags=re.M)
	case.path.write_text(text)
	values, _ = case.get("display.fov")
	check(float_equal(values["display.fov"], "90"), f"stock/chosen: {values}")
	check(case.text() == text, "stock/chosen: the file changed")


def test_control_binds_new_installs_only(program, directory, defaults, kind):
	"""controls.zoom and controls.throw_grenade: a new install has the MCC binds; a file that exists keeps its binds,
	commented out at the old ones or not (written as values), and a bind of the player's stays."""
	if kind == "server":
		return
	old = {"zoom": "Z, Mouse Middle", "throw_grenade": "Mouse Right, G"}
	new = {"zoom": "Z, Mouse Right", "throw_grenade": "G"}
	case = Case(program, directory / "binds_new")
	values, _ = case.get("controls.zoom", "controls.throw_grenade")
	check(values["controls.zoom"] == new["zoom"] and values["controls.throw_grenade"] == new["throw_grenade"], f"binds/new: {values}")
	for how in ("commented", "absent", "chosen"):
		case = Case(program, directory / f"binds_{how}")
		case.get("display.vsync")
		text = case.text()
		for key in old:
			if how == "commented":
				text = re.sub(rf'^# {key} = .*$', f'# {key} = "{old[key]}"', text, flags=re.M)
			elif how == "absent":
				text = re.sub(rf'(\n(# [^\n]*\n)*# {key} = [^\n]*\n)', "\n", text)
			else:
				text = re.sub(rf'^# {key} = .*$', f'{key} = "Mouse 5"', text, flags=re.M)
		case.path.write_text(text)
		values, _ = case.get("controls.zoom", "controls.throw_grenade")
		for key in old:
			wanted = "Mouse 5" if how == "chosen" else old[key]
			check(values[f"controls.{key}"] == wanted, f"binds/{how}: {key} is {values[f'controls.{key}']}")
			check(re.search(rf'^{key} = "{wanted}"$', case.text(), re.M), f"binds/{how}: {key} not a value in the file")


def main():
	parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
	parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
	arguments = parser.parse_args()
	failures = 0
	with tempfile.TemporaryDirectory() as directory:
		directory = Path(directory)
		for kind in DEFAULTS:
			program = build(arguments.cc, kind, directory / kind)
			for test in (test_new_file, test_previous_release_file, test_file_without_the_settings,
				test_readded_by_older_version, test_before_config_version, test_renamed_setting,
				test_new_defaults_of_a_077_file, test_mouse_style_new_installs_only,
				test_control_binds_new_installs_only, test_stock_graphics_new_installs_only):
				try:
					test(program, directory / kind / "cases", DEFAULTS[kind], kind)
					print(f"ok   {kind}: {test.__name__}")
				except AssertionError as error:
					print(f"FAIL {kind}: {test.__name__}: {error}")
					failures += 1
	sys.exit(1 if failures else 0)


if __name__ == "__main__":
	main()
