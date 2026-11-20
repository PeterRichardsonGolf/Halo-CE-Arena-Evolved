"""Check that each controller drives its own local player in split screen.

The menus' routing of a controller's events, real code compiled on its own and
driven as the game drives it (source/interface/ui_widget.c:
widget_event_controller, widget_takes_events_of_controller and what they
call), on the screens split screen goes through: Xbox SELECT PROFILE's four
quarters (each controller presses A to join its own), a one-person main menu
(any controller drives it), and a second player's own pause menu.

The negative control puts back what main had before: the first player's menus
taking every controller's events while nobody has joined (a second
controller's A joined the first player's quarter, as the first player).

Needs only a C compiler. Usage: test_split_screen_input.py [--cc clang]
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UI_WIDGET = ROOT / "source/interface/ui_widget.c"


def function(source, name):
    """The definition of the static function name (its comment left out)."""
    match = re.search(r"^static [^\n;]*\b%s\(\n?[^;{]*?\)\n\{" % re.escape(name), source, re.M)
    if not match:
        raise SystemExit("%s: no function %s" % (UI_WIDGET.name, name))
    depth = 0
    for index in range(match.end() - 1, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if not depth:
                return source[match.start():index + 1] + "\n"
    raise SystemExit("unbalanced function %s" % name)


ROUTING_PRELUDE = r'''
#include <stdio.h>
#include <string.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
struct widget_instance
{
	const char *name;
	short local_player_index;
	struct widget_instance *next;
	struct widget_instance *parent;
	struct widget_instance *child;
};
static struct { struct widget_instance *active_widgets[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS]; } widget_globals;
static boolean we_are_at_the_main_menu;
static short player_spawn_count;
static short local_players;
static unsigned char split_players;
static short local_player_count(void) { return local_players; }
static short players_port_local_player_count(void) { return local_players; }
unsigned char pc_menu_split_players(void) { return split_players; }
/* (OpenCE's screens that read every controller or route by controller: none of the screens driven here) */
unsigned char pc_menu_screen_reads_every_controller(const struct widget_instance *widget) { (void)widget; return 0; }
unsigned char pc_menu_screen_routes_by_controller(const struct widget_instance *widget) { (void)widget; return 0; }
static short player_ui_get_single_player_local_player_controller(short player) { return player; }
'''

ROUTING_DRIVER = r'''
static int failures;

#define CHECK(condition, text) \
	do \
	{ \
		if (!(condition)) \
		{ \
			printf("FAIL %s\n", text); \
			failures++; \
		} \
	} while (0)

/* a widget's own leaf that takes the event (process_ui_widgets: its
_widget_pass_unhandled_events_to_all_children_bit screens) */
static struct widget_instance *dispatch(struct widget_instance *widget, short controller_index)
{
	struct widget_instance *child;

	if (!widget_takes_events_of_controller(widget, controller_index))
		return NULL;
	if (!widget->child)
		return widget;
	for (child = widget->child; child; child = child->next)
	{
		struct widget_instance *taken = dispatch(child, controller_index);

		if (taken)
			return taken;
	}
	return NULL;
}

/* where a button of the controller ends up: each local player's screen in
turn reads the events of the controller widget_event_controller names (all
of them for NONE: get_next_event), and the first that reads it has it */
static struct widget_instance *press(short controller_index)
{
	short widget_index;

	for (widget_index = 0; widget_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; widget_index++)
	{
		struct widget_instance *widget = widget_globals.active_widgets[widget_index];
		short reads;

		if (!widget)
			continue;
		reads = widget_event_controller(widget);
		if (reads == NONE || reads == controller_index)
			return dispatch(widget, controller_index);
	}
	return NULL;
}

static void link_children(struct widget_instance *parent, struct widget_instance *children, int count)
{
	int index;

	parent->child = count ? &children[0] : NULL;
	for (index = 0; index < count; index++)
	{
		children[index].parent = parent;
		children[index].next = index + 1 < count ? &children[index + 1] : NULL;
	}
}

static void reset(void)
{
	memset(&widget_globals, 0, sizeof(widget_globals));
	we_are_at_the_main_menu = TRUE;
	player_spawn_count = 0;
	local_players = 1;
	split_players = 0;
}

int main(void)
{
	struct widget_instance join = { "4way_start2join_screen", NONE, NULL, NULL, NULL };
	struct widget_instance quarters[4] =
	{
		{ "qtr_screen_start2join", 0, NULL, NULL, NULL },
		{ "qtr_screen_start2join", 1, NULL, NULL, NULL },
		{ "qtr_screen_start2join", 2, NULL, NULL, NULL },
		{ "qtr_screen_start2join", 3, NULL, NULL, NULL },
	};
	struct widget_instance main_menu = { "main_menu", 0, NULL, NULL, NULL };
	struct widget_instance main_menu_items[2] = { { "campaign", 0, NULL, NULL, NULL }, { "multiplayer", 0, NULL, NULL, NULL } };
	struct widget_instance pause_1 = { "pause_menu", 0, NULL, NULL, NULL };
	struct widget_instance pause_2 = { "pause_menu", 1, NULL, NULL, NULL };
	short controller;

	link_children(&join, quarters, 4);
	link_children(&main_menu, main_menu_items, 2);

	/* SELECT PROFILE, nobody joined yet: each controller's A is its own quarter's */
	reset();
	widget_globals.active_widgets[0] = &join;
	for (controller = 0; controller < 4; controller++)
	{
		char text[96];

		snprintf(text, sizeof(text), "SELECT PROFILE: controller %d presses A in quarter %d", controller + 1, controller + 1);
		CHECK(press(controller) == &quarters[controller], text);
	}
	/* ... and once two have joined */
	split_players = 1;
	CHECK(press(1) == &quarters[1], "SELECT PROFILE, two joined: controller 2 stays in quarter 2");
	CHECK(press(3) == &quarters[3], "SELECT PROFILE, two joined: controller 4 joins quarter 4");

	/* one person at the main menu: any controller drives it (a phone's
	gamepad behind its touch controls, a second pad picked up) */
	reset();
	widget_globals.active_widgets[0] = &main_menu;
	CHECK(press(0) == &main_menu_items[0], "main menu: controller 1 drives it");
	CHECK(press(1) == &main_menu_items[0], "main menu, one person: controller 2 drives it");
	CHECK(press(3) == &main_menu_items[0], "main menu, one person: controller 4 drives it");
	/* ... but not once a second player is being added */
	split_players = 1;
	CHECK(press(1) == NULL, "main menu, a second player: controller 2 does not drive player 1's menu");

	/* in a split screen game: each player's pause menu is their own */
	reset();
	we_are_at_the_main_menu = FALSE;
	local_players = 2;
	split_players = 1;
	widget_globals.active_widgets[0] = &pause_1;
	widget_globals.active_widgets[1] = &pause_2;
	CHECK(press(0) == &pause_1, "game: controller 1 drives player 1's pause menu");
	CHECK(press(1) == &pause_2, "game: controller 2 drives player 2's pause menu");
	widget_globals.active_widgets[0] = NULL;
	CHECK(press(0) == NULL, "game: controller 1 does not drive player 2's pause menu");
	widget_globals.active_widgets[0] = &pause_1;
	widget_globals.active_widgets[1] = NULL;
	CHECK(press(1) == NULL, "game: controller 2 does not drive player 1's pause menu");

	/* one player, a second controller's own screen up (its sign-in): it keeps to it */
	reset();
	widget_globals.active_widgets[0] = &main_menu;
	widget_globals.active_widgets[1] = &pause_2;
	CHECK(press(1) == &pause_2, "one player: a controller with a screen of its own keeps to it");
	CHECK(press(0) == &main_menu_items[0], "one player: controller 1 still drives the main menu");

	printf("menus: %s\n", failures ? "FAILED" : "each controller drives its own player's screens");
	return failures != 0;
}
'''


def routing_code(old):
    source = UI_WIDGET.read_text()
    names = ["widget_tree_has_local_player", "controller_has_own_widget", "widget_takes_any_controller",
        "widget_event_controller", "widget_takes_events_of_controller"]
    code = "".join(function(source, name) for name in names)
    if old:
        # main before: a controller's own widget did not keep its events
        code, count = re.subn(r"controller_has_own_widget\((controller_index)\)", "FALSE", code)
        if count < 2:
            raise SystemExit("negative control: controller_has_own_widget not found where expected")
    return ROUTING_PRELUDE + code + ROUTING_DRIVER


def build_and_run(cc, folder, name, code, flags=()):
    path = Path(folder) / (name + ".c")
    binary = Path(folder) / name
    path.write_text(code)
    subprocess.run([cc, "-std=c99", "-Wall", "-Werror", "-Wno-unused-function", "-o", str(binary), str(path),
        *flags], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    return result.returncode, result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cc", default=shutil.which("cc") or "cc")
    args = parser.parse_args()
    failed = False
    with tempfile.TemporaryDirectory() as folder:
        runs = [("routing", routing_code)]
        for name, make in runs:
            code, output = build_and_run(args.cc, folder, name, make(old=False))
            print(output, end="")
            failed |= code != 0
            code, output = build_and_run(args.cc, folder, name + "_old", make(old=True))
            if not code:
                print("negative control (%s): main's old code passed, so this test proves nothing" % name)
                failed = True
            else:
                print("negative control (%s): main's old code fails, as it should (%s)" %
                    (name, output.strip().splitlines()[0]))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
