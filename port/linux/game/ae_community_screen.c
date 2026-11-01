/*
AE_COMMUNITY_SCREEN.C

Arena Evolved: the COMMUNITY SERVICES screen (ae_community_screen.h). Its numbers are the A3 mockup's (the opt-in
SPEC, "A3 layout"), in 1080 units with x from the menus' 4:3 frame (240 at 16:9): the left column at 160 (700 wide),
small text (Overpass 750, 31) at a 40 pitch, the labels in Overpass 900 at 41, the menus' 2-unit drop shadow; the
card in the lobby map card's look (fill, 4-unit edge, radius 18), 8 units under the band's line. Every gap is at
least 8 px at 1080p (the clearance rule).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "interface/ui_widget_definitions.h"
#include "tag_files/tag_groups.h"

#include "ae_community_screen.h"
#include "ae_hooks.h"
#include "../src/ae_community.h"
#include "../src/ae_draw.h"

void platform_log(char const *format, ...);
char const *config_string(char const *name);
int config_boolean(char const *name);
double config_real(char const *name);
/* (menu_tags.c) the PC menus' main menu, or debug.menu_open's screen */
char const *pc_menus_root_name(void);
/* (ui_widget.c) back to the screen before */
void ui_widget_port_go_back(struct widget_instance *widget);
#ifdef HALO_GAME_BROWSER
/* (browser.c) a dedicated server, or a probe: nobody at the screen */
int browser_headless(void);
#endif

#define SCREEN_NAME "pc\\main_menu\\community_services\\community_services_screen"
#define MAIN_MENU_NAME "pc\\main_menu\\main_menu"

static struct
{
	/* when the screen opened (its list's created): the input guard's start */
	unsigned long opened_at;
	/* the frame last drawn (ae_draw_frame): the screen renders once a view */
	unsigned int drawn_frame;
	boolean drawn;
	/* (0.7.0) asked and not answered yet: AE's main menu waits (ae_hooks.c) */
	boolean asking;
} community;

/* ---------- finding the screen's widgets */

static struct widget_instance *named(struct widget_instance *widget, char const *name)
{
	struct widget_instance *child;

	for (child = widget ? widget->child : NULL; child; child = child->next)
	{
		struct widget_instance *found;

		if (child->name && !strcmp(child->name, name))
			return child;
		found = named(child, name);
		if (found)
			return found;
	}
	return NULL;
}

static struct widget_instance *row_spinner(struct widget_instance *widget)
{
	return named(widget_instance_get_topmost_parent(widget), "list_hosted_games_spinner");
}

/* ---------- opening */

void ae_community_main_menu_loaded(
	void)
{
	struct ae_community_context context;
	long main_menu;

	/* (AE's main menu takes the game's place only once it is there: ae_hooks.c) */
	ae_ui_game_main_menu_loaded();
	context.state = ae_community_state();
	if (context.state != AE_COMMUNITY_ASK)
		return;
	/* (debug.menu_open's screen is the root then: checked first, as asking for the root's name with it set begins
	a profile's edit) */
	context.headless = config_string("debug.menu_open")[0] || config_boolean("debug.hidden_window") ||
		config_real("debug.exit_after") > 0.0 || config_string("debug.network_test")[0];
#ifdef HALO_GAME_BROWSER
	context.headless = context.headless || browser_headless();
#endif
	main_menu = tag_loaded(UI_WIDGET_DEFINITION_TAG, MAIN_MENU_NAME);
	/* (the PC menus are the menus, and the main menu loaded is theirs; with AE's menus on too: AE's main menu waits
	until the question is answered (ae_community_screen_asking), until AE's own question, 0.7.2) */
	context.pc_menus = !context.headless && main_menu != NONE && !strcmp(pc_menus_root_name(), MAIN_MENU_NAME) &&
		tag_loaded(UI_WIDGET_DEFINITION_TAG, SCREEN_NAME) != NONE;
	context.renderer = ae_draw_available();
	if (!ae_community_should_ask(&context))
	{
		static boolean logged = FALSE;

		if (logged)
			return;
		logged = TRUE;
		platform_log("community services: not asked (network.community_services = \"ask\": off; %s)",
			context.headless ? "nobody to ask" : !context.pc_menus ? "not the PC menus" : "no renderer");
		return;
	}
	/* (in the main menu's place, which BACK brings back) */
	if (!ui_widget_load_by_name_or_tag(SCREEN_NAME, NONE, NULL, NONE, main_menu, NONE, NONE))
	{
		platform_log("community services: the screen did not open");
		return;
	}
	community.asking = TRUE;
	platform_log("community services: asking (network.community_services = \"ask\")");
}

boolean ae_community_screen_asking(
	void)
{
	/* (answered, or the screen gone some other way: AE's main menu may come) */
	if (community.asking && ae_community_state() != AE_COMMUNITY_ASK)
		community.asking = FALSE;
	return community.asking;
}

/* ---------- its functions */

/* the list's created: NOT NOW focused (an opt-in is not answered by a stray A), the guard started */
static boolean focus_not_now(struct widget_instance *list)
{
	struct widget_instance *child;
	short index = 0;

	community.opened_at = system_milliseconds();
	for (child = list->child; child; child = child->next, index++)
	{
		if (child->name && !strcmp(child->name, "button_bar"))
		{
			struct widget_instance *button;
			short button_index = 0;

			list->focused_child = child;
			list->parameters.list.selected_index = index;
			for (button = child->child; button; button = button->next, button_index++)
			{
				if (button->name && !strcmp(button->name, "button_not_now"))
				{
					child->focused_child = button;
					child->parameters.list.selected_index = button_index;
				}
			}
		}
	}
	return TRUE;
}

static boolean input_open(void)
{
	return ae_community_input_open(community.opened_at, system_milliseconds());
}

static boolean answer(struct widget_instance *widget, boolean use, boolean *widget_deleted)
{
	struct widget_instance *spinner = row_spinner(widget);
	boolean list_row_on = spinner && spinner->parameters.list.selected_index == 1;

	if (!input_open())
		return TRUE;
	ae_community_write_answer(use, list_row_on);
	community.asking = FALSE;
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

unsigned char ae_community_menu_event(
	char const *name,
	struct widget_instance *widget,
	unsigned char *widget_deleted)
{
	boolean deleted = FALSE;
	boolean handled = FALSE;

	if (!strcmp(name, "ae community focus"))
		handled = focus_not_now(widget);
	else if (!strcmp(name, "ae community row"))
	{
		struct widget_instance *spinner = row_spinner(widget);

		if (spinner && input_open())
			spinner->parameters.list.selected_index = spinner->parameters.list.selected_index == 1 ? 0 : 1;
		handled = TRUE;
	}
	else if (!strcmp(name, "ae community use"))
		handled = answer(widget, TRUE, &deleted);
	else if (!strcmp(name, "ae community not now"))
		handled = answer(widget, FALSE, &deleted);
	if (deleted)
		*widget_deleted = TRUE;
	return handled;
}

/* ---------- drawing (the opt-in SPEC's A3 layout) */

enum
{
	SMALL = 31,
	LARGE = 41,
	CARD_LARGE = 40,
	PITCH = 40,
	SHADOW_OFFSET = 2,
	/* (from the 4:3 frame's left) */
	COLUMN_X = 160,
	CARD_X = 900,
	CARD_WIDTH = 495,
	CARD_TOP = 168,
	CARD_BOTTOM = 822,
	CARD_EDGE = 4,
	CARD_RADIUS = 18,
	CARD_TEXT_X = CARD_X + CARD_EDGE + 22,
	CARD_DIVIDER_Y = 482,
	CARD_DIVIDER_HEIGHT = 2,
};

#define BLUE 0x2995FFFFu
#define GREY 0xCCCCCCFFu
#define WHITE 0xFFFFFFFFu
#define CARD_FILL 0x000E23FFu
#define DIVIDER 0x2995FF6Eu
#define SHADOW 0x000000AAu

struct line
{
	char large;
	short x;
	short baseline;
	unsigned int color;
	char const *text;
};

/* the left column and the card, each line where the mockup has it (its own breaks: the column's 700 and the card's
443 units wide) */
static struct line const lines[] =
{
	{ 0, COLUMN_X, 214, WHITE, "Two free services run by the ChupathingyCE" },
	{ 0, COLUMN_X, 254, WHITE, "team. Both stay off unless you turn them on." },
	{ 1, COLUMN_X, 320, BLUE, "GAME LIST" },
	{ 0, COLUMN_X, 364, GREY, "Public internet games show up in System Link." },
	{ 0, COLUMN_X, 404, GREY, "The list's server sees your IP address." },
	/* (the LIST HOSTED GAMES row is the screen's widget, 422-482) */
	{ 0, COLUMN_X, 515, GREY, "Strangers can find and join games you host." },
	{ 0, COLUMN_X, 555, GREY, "Only with USE COMMUNITY SERVICES." },
	{ 1, COLUMN_X, 625, BLUE, "GAME STATS" },
	{ 0, COLUMN_X, 669, GREY, "Games you host or join send their stats to the" },
	{ 0, COLUMN_X, 709, GREY, "site's match pages: player names, scores, kills," },
	{ 0, COLUMN_X, 749, GREY, "medals, where players moved, the map, the" },
	{ 0, COLUMN_X, 789, GREY, "time and a player ID." },
	{ 0, COLUMN_X, 857, GREY, "LAN, invites and Direct IP work either way." },
	{ 0, COLUMN_X, 897, GREY, "Change it any time: Settings > Network Setup." },
	{ 2, CARD_TEXT_X, 223, WHITE, "Run by ChupathingyCE" },
	{ 0, CARD_TEXT_X, 267, BLUE, "halo.milenko.org" },
	{ 0, CARD_TEXT_X, 329, WHITE, "Arena Evolved does not host," },
	{ 0, CARD_TEXT_X, 369, WHITE, "run or maintain them, so it" },
	{ 0, CARD_TEXT_X, 409, WHITE, "cannot promise they are safe," },
	{ 0, CARD_TEXT_X, 449, WHITE, "private or always online." },
	{ 2, CARD_TEXT_X, 540, BLUE, "THANK YOU" },
	{ 0, CARD_TEXT_X, 584, GREY, "To Milenko and the" },
	{ 0, CARD_TEXT_X, 624, GREY, "ChupathingyCE team, for" },
	{ 0, CARD_TEXT_X, 664, GREY, "running them and for the work" },
	{ 0, CARD_TEXT_X, 704, GREY, "Arena Evolved is built on." },
};

static void line_draw(struct line const *line, float frame_left)
{
	int font = line->large ? AE_FONT_ROW : AE_FONT_BODY;
	float size = line->large == 2 ? (float)CARD_LARGE : line->large ? (float)LARGE : (float)SMALL;
	float top = (float)line->baseline - ae_draw_cap_height(font, size);
	float x = frame_left + (float)line->x;

	ae_draw_text(font, size, x + SHADOW_OFFSET, top + SHADOW_OFFSET, AE_ALIGN_LEFT, SHADOW, line->text);
	ae_draw_text(font, size, x, top, AE_ALIGN_LEFT, line->color, line->text);
}

void ae_community_screen_update(
	struct widget_instance *screen)
{
	struct ae_layout layout;
	float frame_left;
	unsigned int frame;
	short index;

	/* one focus on the screen: the button bar keeps its focused button's glow (column_list_update runs on the focused
	list only), so with the row focused its buttons are drawn at rest (the mockup's A3c) */
	{
		struct widget_instance *list = named(screen, "options_menu");
		struct widget_instance *bar = named(screen, "button_bar");

		if (list && bar && list->focused_child != bar)
		{
			struct widget_instance *button;

			for (button = bar->child; button; button = button->next)
			{
				if (button->animation.number_of_sprite_frames == 2)
					button->animation.current_frame_index = 0;
			}
		}
	}
	/* (the row at rest while the guard holds: a carried-over press does not turn listing on) */
	if (!input_open())
	{
		struct widget_instance *spinner = named(screen, "list_hosted_games_spinner");

		if (spinner)
			spinner->parameters.list.selected_index = 0;
	}
	if (!ae_draw_available())
		return;
	frame = ae_draw_frame();
	if (community.drawn && community.drawn_frame == frame)
		return;
	community.drawn = TRUE;
	community.drawn_frame = frame;
	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	frame_left = (layout.width - 1440.0f) * 0.5f;
	ae_draw_rect(frame_left + CARD_X, CARD_TOP, CARD_WIDTH, CARD_BOTTOM - CARD_TOP, CARD_RADIUS, CARD_FILL);
	ae_draw_outline(frame_left + CARD_X, CARD_TOP, CARD_WIDTH, CARD_BOTTOM - CARD_TOP, CARD_RADIUS, CARD_EDGE, BLUE);
	ae_draw_rect(frame_left + CARD_X + CARD_EDGE, CARD_DIVIDER_Y, CARD_WIDTH - 2 * CARD_EDGE, CARD_DIVIDER_HEIGHT, 0.0f,
		DIVIDER);
	for (index = 0; index < (short)NUMBEROF(lines); index++)
		line_draw(&lines[index], frame_left);
}
