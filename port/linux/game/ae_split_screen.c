/*
AE_SPLIT_SCREEN.C

Multiplayer > SPLIT SCREEN in the PC menus (ae_split_screen.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/players.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"

#include "ae_split_screen.h"
#include "bots.h"
#include "ae_bots_objective.h"

int config_write(const char *name, const char *value);
void platform_log(char const *format, ...);

/* menu_functions.c's (and menu_tags.c's) */
#define MAXIMUM_STRINGS 128
struct pc_menu_setting
{
	long definition_index;
	char const *setting;
	long value_count;
	char const *values[MAXIMUM_STRINGS];
	short loaded_index;
};

struct pc_menu_setting *pc_menu_setting_get(long definition_index);

/* the most players of a split screen game: the four people and the most bots */
#define MAXIMUM_PLAYERS (MAXIMUM_LOCAL_PLAYERS + BOTS_MAXIMUM)

/* the bot rows (tools/port_settings.py's _lobby), each with its spinner (its name + "_spinner") */
static char const *const bot_rows[] = { "ae_bots_count_row", "ae_bots_skill_row", "ae_bots_playfight_row",
	"ae_bots_teams_row" };
enum { BOT_ROW_COUNT, BOT_ROW_SKILL, BOT_ROW_PLAYFIGHT, BOT_ROW_TEAMS, NUMBER_OF_BOT_ROWS };

static boolean split_screen_chosen;

void ae_split_screen_chosen(char const *item_name)
{
	split_screen_chosen = item_name && strstr(item_name, "split_scr") != NULL;
}

boolean ae_split_screen_hosting(void)
{
	return split_screen_chosen;
}

short ae_split_screen_lobby_rows(short rows)
{
	return network_game_is_splitscreen_local() ? AE_SPLIT_SCREEN_LOBBY_ROWS : rows;
}

static struct widget_instance *child_named(struct widget_instance *widget, char const *name)
{
	struct widget_instance *child;

	for (child = widget ? widget->child : NULL; child; child = child->next)
	{
		struct widget_instance *found;

		if (child->name && !strcmp(child->name, name))
			return child;
		if ((found = child_named(child, name)) != NULL)
			return found;
	}
	return NULL;
}

/* a setting spinner's choice written to config.toml when it changed (as menu_functions.c's setting_save, but at
once: the lobby has no OK) */
static void spinner_save(struct widget_instance *spinner)
{
	struct pc_menu_setting *setting = spinner ? pc_menu_setting_get(spinner->definition_tag_index) : NULL;
	short index;

	if (!setting || setting->loaded_index == NONE)
		return;
	index = spinner->parameters.list.selected_index;
	if (index < 0 || index >= setting->value_count || index == setting->loaded_index)
		return;
	if (!config_write(setting->setting, setting->values[index]))
	{
		platform_log("split screen: could not write %s to config.toml", setting->setting);
		return;
	}
	platform_log("split screen: %s = %s", setting->setting, setting->values[index]);
	setting->loaded_index = index;
}

/* the lobby's seats (ae_split_screen_lobby_update): a device with no seat that presses START takes the next free
seat, and its START, down on its new port, is the lobby's own join ("port lobby join": its profile screen) */
static unsigned lobby_seats_update(void);
static void lobby_pick_update(struct widget_instance *list, unsigned bits);

void ae_split_screen_lobby_update(struct widget_instance *list, boolean teams, long game_engine_index)
{
	boolean local = network_game_is_splitscreen_local();
	short row;
	unsigned bits = 0;

	if (local)
		bits = lobby_seats_update();
	lobby_pick_update(list, bits);

	for (row = 0; row < NUMBER_OF_BOT_ROWS; row++)
	{
		struct widget_instance *widget = child_named(list, bot_rows[row]);
		char spinner_name[48];

		if (!widget)
			continue;
		widget->visible = local && ae_bots_engine_played(game_engine_index) && (row != BOT_ROW_TEAMS || teams);
		if (!widget->visible)
			continue;
		strcpy(spinner_name, bot_rows[row]);
		strcat(spinner_name, "_spinner");
		spinner_save(child_named(widget, spinner_name));
	}
}

/* Server Setup's rows a split screen game has no use for (a network game's), and the gametype's, in their order
(tools/ce_menus.py and port_settings.py place them 22 apart from y 73) */
static char const *const network_rows[] = { "op_server_name", "op_max_players", "op_invite", "op_listing", "op_password",
	"op_map_share_options" };
static char const *const gametype_rows[] = { "op_game_type", "op_player_options", "op_item_options",
	"op_vehicle_options", "op_indicator_options", "op_team_options", "op_arena_options" };
#define SERVER_SETTINGS_FIRST_ROW 73
#define SERVER_SETTINGS_ROW_SPACING 22

void ae_split_screen_server_settings(struct widget_instance *list)
{
	struct widget_instance *child;
	struct widget_instance *header;
	short row, index;
	boolean local = network_game_is_splitscreen_local();

	if (!list)
		return;
	/* (its title: GAME SETUP for a game on this PC, else the PC version's SERVER SETTINGS; port_settings.py puts both
	on the screen) */
	if ((header = child_named(list->parent, "header_server_settings")) != NULL)
		header->visible = !local;
	if ((header = child_named(list->parent, "header_game_setup")) != NULL)
		header->visible = local;
	if (!local)
		return;
	for (row = 0; row < (short)NUMBEROF(network_rows); row++)
	{
		struct widget_instance *widget = child_named(list, network_rows[row]);

		if (widget)
			widget->visible = FALSE;
	}
	for (row = 0; row < (short)NUMBEROF(gametype_rows); row++)
	{
		struct widget_instance *widget = child_named(list, gametype_rows[row]);

		if (widget)
			widget->vertical_offset = (short)(SERVER_SETTINGS_FIRST_ROW + SERVER_SETTINGS_ROW_SPACING * row);
	}
	/* (the focus off a hidden row, onto the first that shows: menu_functions.c's focus_off_hidden) */
	if (list->focused_child && !list->focused_child->visible)
	{
		for (child = list->child, index = 0; child; child = child->next, index++)
		{
			if (child->visible)
			{
				list->focused_child = child;
				list->parameters.list.selected_index = index;
				break;
			}
		}
	}
}

boolean ae_split_screen_server_start(void)
{
	if (network_game_is_splitscreen_local())
	{
		network_game_server_port_set_settings(NULL, MAXIMUM_PLAYERS);
		platform_log("split screen: %d players at most", MAXIMUM_PLAYERS);
	}
	return TRUE;
}

/* ---------- the JOIN screen (ae_split_screen.h) */

#include "interface/player_ui.h"
#include "saved games/player_profile.h"
#include "saved games/saved_game_files.h"
#include "ae_seats.h"

boolean ui_widget_port_multiplayer_player(short controller_index, long profile_index);
boolean ui_widget_port_host(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);
void p2p_set_hosting_allowed(int allowed);
void p2p_set_hosting_public(int public);
void ui_widget_port_post_button(short controller_index, short button_index);
unsigned long system_milliseconds(void);

#define JOIN_PROFILES 32
#define JOIN_TEXT_LENGTH 64
#define JOIN_MESSAGE_TIME 3000
/* a seat's first presses, the one that took it, are not its CONTINUE */
#define JOIN_PRESS_GUARD 600
/* (menu_functions.c's) */
#define JOIN_PROFILE_VALID_BIT 0x80000000UL
#define JOIN_BUTTON_B 1

/* the card's pictures' frames (tools/ae_seat_pictures.py) */
enum { CARD_EMPTY, CARD_SEATED, CARD_HELD };
enum { DEVICE_KEYBOARD, DEVICE_XBOX, DEVICE_PLAYSTATION, DEVICE_NINTENDO, DEVICE_OTHER, DEVICE_UNPLUGGED };
enum { BUTTON_XBOX_START, BUTTON_PLAYSTATION_OPTIONS, BUTTON_ENTER, BUTTON_XBOX_B, BUTTON_PLAYSTATION_CIRCLE,
	BUTTON_ESCAPE, BUTTON_NINTENDO_B, BUTTON_NINTENDO_PLUS };
enum { PILL_REST, PILL_FOCUS, PILL_FILLED, PILL_INERT };
enum { ROW_CONTROLLER, ROW_PROFILE };
/* a seat's device pressed (or a swap): its pill blinks three times, 150 ms filled, 100 ms not */
#define JOIN_FLASH_TIME 750
#define JOIN_FLASH_ON 150
#define JOIN_FLASH_PERIOD 250
/* ASSIGN's wait: the pill's edge between rest and focus, a second each way and back */
#define JOIN_PULSE_PERIOD 1000
/* a pill's value in the large font up to this many letters, else the small one (the pill's 108 units) */
#define JOIN_LARGE_FIT 7

static struct
{
	/* the screen is up */
	boolean open;
	/* a device with no seat pressed B / ESC: the B posted as controller 0 is that, not seat 0's leave */
	boolean back_requested;
	/* when each seat was taken: the press that took it is no CONTINUE (its START is down on its new port) */
	unsigned long joined_at[AE_SEATS_MAXIMUM];
	/* each seat's profile (an index of the profiles' list), NONE: none chosen yet */
	short profile[AE_SEATS_MAXIMUM];
	/* the profiles */
	long indices[JOIN_PROFILES];
	struct player_profile profiles[JOIN_PROFILES];
	short count;
	/* the first untitled profile name, as NEW PROFILE will make it (read with the profiles) */
	wchar_t untitled[128];
	/* the help line's word for a while (a fifth device: "4 players at most on one PC") */
	wchar_t const *message;
	unsigned long message_until;
	/* (a message made up here: "PLAYER 1 and PLAYER 3 swapped controllers.") */
	wchar_t message_text[JOIN_TEXT_LENGTH];
	/* each seat's row its own up and down move between: 0 its CONTROLLER, 1 its PROFILE */
	short row[AE_SEATS_MAXIMUM];
} join;

static void join_text(struct widget_instance *text_box, wchar_t const *text)
{
	short index;

	if (!text_box)
		return;
	if (!text_box->parameters.text_box.text)
	{
		text_box->parameters.text_box.text = ui_widget_realloc(NULL, JOIN_TEXT_LENGTH * sizeof(wchar_t), __FILE__,
			__LINE__);
	}
	if (!text_box->parameters.text_box.text)
		return;
	for (index = 0; index < JOIN_TEXT_LENGTH - 1 && text && text[index]; index++)
		text_box->parameters.text_box.text[index] = text[index];
	text_box->parameters.text_box.text[index] = 0;
}

/* (an ASCII text) */
static void join_text_ascii(struct widget_instance *text_box, char const *text)
{
	wchar_t wide[JOIN_TEXT_LENGTH];
	short index;

	for (index = 0; index < JOIN_TEXT_LENGTH - 1 && text && text[index]; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
	join_text(text_box, wide);
}

static short join_length(wchar_t const *text)
{
	short length = 0;

	while (text && text[length])
		length++;
	return length;
}

/* (the build's wchar_t is two bytes: no swprintf) */
static void join_concat(wchar_t *text, wchar_t const *first, wchar_t const *second)
{
	short length = 0;

	while (*first && length < JOIN_TEXT_LENGTH - 1)
		text[length++] = *first++;
	while (*second && length < JOIN_TEXT_LENGTH - 1)
		text[length++] = *second++;
	text[length] = 0;
}

static void join_show(struct widget_instance *widget, boolean visible)
{
	if (widget)
		widget->visible = visible;
}

static struct widget_instance *join_child(struct widget_instance *screen, char const *name, short seat)
{
	char full[48];

	snprintf(full, sizeof(full), "%s_%d", name, seat);
	return child_named(screen, full);
}

static void join_frame(struct widget_instance *widget, short frame)
{
	if (widget)
		widget->animation.current_frame_index = frame;
}

/* the profiles, as the PC menus' list reads them (menu_functions.c's profile_list_read) */
static void join_profiles_read(void)
{
	long indices[JOIN_PROFILES];
	word count = JOIN_PROFILES;
	short index;

	player_profiles_enumerate_available_to_local_player_index(NONE, &count, indices, FALSE);
	saved_game_file_get_useable_untitled_profile_name(join.untitled);
	join.untitled[11] = 0;
	join.count = 0;
	for (index = 0; index < (short)count && join.count < JOIN_PROFILES; index++)
	{
		if (((unsigned long)indices[index] & JOIN_PROFILE_VALID_BIT) &&
			player_profile_get(indices[index], &join.profiles[join.count]))
		{
			join.indices[join.count++] = indices[index];
		}
	}
}

/* (the list's last choice, after the profiles: NEW PROFILE, made at CONTINUE; any number of seats may be on it) */
#define JOIN_NEW_PROFILE (join.count)

static boolean join_profile_taken(short profile, short except_seat)
{
	struct ae_seats seats;
	short seat;

	if (profile == JOIN_NEW_PROFILE)
		return FALSE;
	ae_seats_get(&seats);
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seat != except_seat && seats.seat[seat].kind != AE_SEAT_EMPTY && join.profile[seat] == profile)
			return TRUE;
	}
	return FALSE;
}

/* the next profile from `from` in the direction (1, -1) no other seat has; NONE if none */
static short join_profile_next(short seat, short from, short direction)
{
	short step, profile;

	for (step = 1; step <= join.count + 1; step++)
	{
		profile = (short)(((from + direction * step) % (join.count + 1) + join.count + 1) % (join.count + 1));
		if (!join_profile_taken(profile, seat))
			return profile;
	}
	return NONE;
}

/* a seat just taken: player 1's profile for the first person, else the first no seat has */
static void join_profile_pick(short seat)
{
	long active = player_ui_get_active_player_profile_index(0);
	short profile;

	if (join.profile[seat] != NONE && join.profile[seat] <= JOIN_NEW_PROFILE && !join_profile_taken(join.profile[seat], seat))
		return;
	join.profile[seat] = NONE;
	for (profile = 0; profile < join.count; profile++)
	{
		if (join.indices[profile] == active && !join_profile_taken(profile, seat))
			join.profile[seat] = profile;
	}
	if (join.profile[seat] == NONE)
		join.profile[seat] = join_profile_next(seat, JOIN_NEW_PROFILE, 1);
}

static void join_message(wchar_t const *text)
{
	join.message = text;
	join.message_until = system_milliseconds() + JOIN_MESSAGE_TIME;
}

/* the name NEW PROFILE will make for the seat (CONTINUE makes them in seat order, each the first untitled name:
"New002", then "New003"); "NEW" when the untitled name has no number to count on */
static void join_new_name(short seat, wchar_t *name)
{
	wchar_t base[JOIN_TEXT_LENGTH];
	short before = 0, other, length, digits, index;
	long number = 0, power = 1;

	for (other = 0; other < seat; other++)
		before += join.profile[other] == JOIN_NEW_PROFILE;
	join_concat(base, join.untitled, L"");
	length = join_length(base);
	for (digits = 0; digits < length && base[length - 1 - digits] >= L'0' && base[length - 1 - digits] <= L'9'; digits++)
	{
		number += (base[length - 1 - digits] - L'0') * power;
		power *= 10;
	}
	if (!digits || digits > 6 || length >= JOIN_TEXT_LENGTH)
	{
		join_concat(name, L"NEW", L"");
		return;
	}
	number += before;
	for (index = 0; index < length - digits; index++)
		name[index] = base[index];
	for (index = 0; index < digits; index++)
	{
		name[length - 1 - index] = (wchar_t)(L'0' + number % 10);
		number /= 10;
	}
	name[length] = 0;
}

static short device_frame(struct ae_seat_view const *view)
{
	if (view->held)
		return DEVICE_UNPLUGGED;
	switch (view->family)
	{
	case AE_SEAT_FAMILY_KEYBOARD: return DEVICE_KEYBOARD;
	case AE_SEAT_FAMILY_XBOX: return DEVICE_XBOX;
	case AE_SEAT_FAMILY_PLAYSTATION: return DEVICE_PLAYSTATION;
	case AE_SEAT_FAMILY_NINTENDO: return DEVICE_NINTENDO;
	default: return DEVICE_OTHER;
	}
}

static short leave_frame(struct ae_seat_view const *view)
{
	switch (view->family)
	{
	case AE_SEAT_FAMILY_PLAYSTATION: return BUTTON_PLAYSTATION_CIRCLE;
	case AE_SEAT_FAMILY_NINTENDO: return BUTTON_NINTENDO_B;
	default: return BUTTON_XBOX_B;
	}
}

/* the widgets of a card that show for a seated player, and for an empty seat (tools/port_settings.py's _join_screen) */
static char const *const seated_widgets[] = { "join_device", "join_devlabel", "join_devbar", "join_profbar",
	"join_profarrow_l", "join_profarrow_r", "join_proflabel", "join_armour", "join_hit_dev_l",
	"join_hit_dev_m", "join_hit_dev_r", "join_hit_prof_l", "join_hit_prof_m", "join_hit_prof_r" };
static char const *const empty_widgets[] = { "join_press", "join_tojoin", "join_glyph0", "join_glyph1", "join_glyph2",
	"join_glyph3", "join_glyphtext0", "join_glyphtext1", "join_glyphtext2", "join_glyphtext3", "join_pick",
	"join_candbar" };

/* a pill's value: in the large font when it fits, else the small; navy on the filled bar */
static void join_value(struct widget_instance *screen, char const *pill, short seat, wchar_t const *text,
	boolean shown, boolean filled)
{
	static char const *const variants[] = { "large", "small", "largeinv", "smallinv" };
	boolean large = join_length(text) <= JOIN_LARGE_FIT;
	short index;

	for (index = 0; index < (short)NUMBEROF(variants); index++)
	{
		char name[32];
		boolean mine = shown && (index % 2 == 0) == large && (index >= 2) == filled;
		struct widget_instance *widget;

		snprintf(name, sizeof(name), "join_%s%s", pill, variants[index]);
		widget = join_child(screen, name, seat);
		join_show(widget, mine);
		if (mine)
			join_text(widget, text);
	}
}

static void join_value_ascii(struct widget_instance *screen, char const *pill, short seat, char const *text,
	boolean shown, boolean filled)
{
	wchar_t wide[JOIN_TEXT_LENGTH];
	short index;

	for (index = 0; index < JOIN_TEXT_LENGTH - 1 && text && text[index]; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
	join_value(screen, pill, seat, wide, shown, filled);
}

/* a seat's device as the picker names it (XBOX 2, KEYBOARD), from the devices connected */
static void join_device_name(short seat, char *name, size_t size)
{
	struct ae_seat_device list[AE_SEATS_DEVICES_MAXIMUM];
	struct ae_seats seats;
	int count, index;

	ae_seats_get(&seats);
	count = ae_seats_devices(list, AE_SEATS_DEVICES_MAXIMUM);
	snprintf(name, size, "CONTROLLER");
	for (index = 0; index < count; index++)
	{
		if (list[index].seat == seat)
			snprintf(name, size, "%s", list[index].name);
	}
	(void)seats;
}

/* the seat's pill flashing now: within the flash of its last press, in an "on" part of the blink */
static boolean join_flashing(short seat)
{
	long ago = ae_seats_pressed_ago(seat);

	return ago >= 0 && ago < JOIN_FLASH_TIME && ago % JOIN_FLASH_PERIOD < JOIN_FLASH_ON;
}

static void join_card_update(struct widget_instance *screen, short seat)
{
	struct ae_seat_view view;
	struct player_profile const *profile;
	short index, state;
	int waiting = ae_seats_assign_waiting(NULL);
	boolean flashing, mine_waits;

	ae_seats_view(seat, &view);
	state = view.kind == AE_SEAT_EMPTY ? CARD_EMPTY : view.held ? CARD_HELD : CARD_SEATED;
	flashing = state != CARD_EMPTY && join_flashing(seat);
	mine_waits = state != CARD_EMPTY && waiting == seat;
	join_frame(join_child(screen, "join_card", seat), state);
	join_show(join_child(screen, "join_title_on", seat), state == CARD_SEATED && !flashing);
	join_show(join_child(screen, "join_title_off", seat), state == CARD_EMPTY);
	join_show(join_child(screen, "join_title_held", seat), state == CARD_HELD && !flashing);
	join_show(join_child(screen, "join_title_flash", seat), flashing);
	for (index = 0; index < (short)NUMBEROF(seated_widgets); index++)
		join_show(join_child(screen, seated_widgets[index], seat), state != CARD_EMPTY);
	for (index = 0; index < (short)NUMBEROF(empty_widgets); index++)
		join_show(join_child(screen, empty_widgets[index], seat), state == CARD_EMPTY);
	if (state == CARD_EMPTY)
	{
		struct ae_seat_device candidate;
		boolean any;

		/* (the join buttons: Xbox START, PlayStation OPTIONS, Nintendo +, ENTER) */
		join_frame(join_child(screen, "join_glyph0", seat), BUTTON_XBOX_START);
		join_frame(join_child(screen, "join_glyph1", seat), BUTTON_PLAYSTATION_OPTIONS);
		join_frame(join_child(screen, "join_glyph2", seat), BUTTON_NINTENDO_PLUS);
		join_frame(join_child(screen, "join_glyph3", seat), BUTTON_ENTER);
		ae_seats_candidate(seat, &candidate);
		any = candidate.kind != AE_SEAT_EMPTY;
		/* (no free pad: nothing to pick, so the pill's edge is dim as its NO FREE PAD) */
		join_frame(join_child(screen, "join_candbar", seat), any ? PILL_REST : PILL_INERT);
		join_value_ascii(screen, "cand", seat, candidate.name, any, FALSE);
		join_show(join_child(screen, "join_candnone", seat), !any);
		join_show(join_child(screen, "join_candarrow_l", seat), any);
		join_show(join_child(screen, "join_candarrow_r", seat), any);
		join_show(join_child(screen, "join_hit_cand_l", seat), any);
		join_show(join_child(screen, "join_hit_cand_m", seat), any);
		join_show(join_child(screen, "join_hit_cand_r", seat), any);
		join_show(join_child(screen, "join_assign", seat), FALSE);
		join_show(join_child(screen, "join_cancel", seat), FALSE);
		join_show(join_child(screen, "join_leavebtn", seat), FALSE);
		join_show(join_child(screen, "join_leavekey", seat), FALSE);
		join_show(join_child(screen, "join_leave", seat), FALSE);
		join_value(screen, "dev", seat, L"", FALSE, FALSE);
		join_value(screen, "prof", seat, L"", FALSE, FALSE);
		join_show(join_child(screen, "join_devarrow_l", seat), FALSE);
		join_show(join_child(screen, "join_devarrow_r", seat), FALSE);
		join_show(join_child(screen, "join_devwait", seat), FALSE);
		return;
	}
	join_show(join_child(screen, "join_candnone", seat), FALSE);
	join_show(join_child(screen, "join_candarrow_l", seat), FALSE);
	join_show(join_child(screen, "join_candarrow_r", seat), FALSE);
	join_show(join_child(screen, "join_hit_cand_l", seat), FALSE);
	join_show(join_child(screen, "join_hit_cand_m", seat), FALSE);
	join_show(join_child(screen, "join_hit_cand_r", seat), FALSE);
	join_value(screen, "cand", seat, L"", FALSE, FALSE);
	join_frame(join_child(screen, "join_device", seat), device_frame(&view));
	/* (the CONTROLLER pill: its device's name; ASSIGN's wait: the edge pulsing, no arrows, PRESS A BUTTON; a press:
	filled) */
	if (mine_waits)
	{
		join_frame(join_child(screen, "join_devbar", seat),
			system_milliseconds() % JOIN_PULSE_PERIOD < JOIN_PULSE_PERIOD / 2 ? PILL_REST : PILL_FOCUS);
		join_value(screen, "dev", seat, L"", FALSE, FALSE);
	}
	else if (view.held)
	{
		join_frame(join_child(screen, "join_devbar", seat), join.row[seat] == ROW_CONTROLLER ? PILL_FOCUS : PILL_REST);
		join_value(screen, "dev", seat, L"RECONNECT", TRUE, FALSE);
	}
	else
	{
		char name[24];

		join_device_name(seat, name, sizeof(name));
		join_frame(join_child(screen, "join_devbar", seat), flashing ? PILL_FILLED :
			join.row[seat] == ROW_CONTROLLER ? PILL_FOCUS : PILL_REST);
		join_value_ascii(screen, "dev", seat, name, TRUE, flashing);
	}
	join_show(join_child(screen, "join_devwait", seat), mine_waits);
	join_show(join_child(screen, "join_devarrow_l", seat), !mine_waits && !flashing);
	join_show(join_child(screen, "join_devarrow_r", seat), !mine_waits && !flashing);
	join_show(join_child(screen, "join_assign", seat), !mine_waits);
	join_show(join_child(screen, "join_cancel", seat), mine_waits);
	join_show(join_child(screen, "join_leave", seat), TRUE);
	join_show(join_child(screen, "join_leavebtn", seat), view.family != AE_SEAT_FAMILY_KEYBOARD);
	join_show(join_child(screen, "join_leavekey", seat), view.family == AE_SEAT_FAMILY_KEYBOARD);
	join_frame(join_child(screen, "join_leavebtn", seat), leave_frame(&view));
	join_frame(join_child(screen, "join_profbar", seat), join.row[seat] == ROW_PROFILE ? PILL_FOCUS : PILL_REST);
	profile = join.profile[seat] != NONE && join.profile[seat] < join.count ? &join.profiles[join.profile[seat]] : NULL;
	if (profile)
		join_value(screen, "prof", seat, profile->player_name, TRUE, FALSE);
	else
	{
		wchar_t name[JOIN_TEXT_LENGTH];

		join_new_name(seat, name);
		join_value(screen, "prof", seat, name, TRUE, FALSE);
	}
	/* (NEW PROFILE: the first colour's armour, as a new profile has until it picks one) */
	join_frame(join_child(screen, "join_armour", seat), profile ? (short)MAX(profile->primary_color_index, 0) : 0);
}

/* the seat map on, with no seat and no profile chosen (the screen's open, or its first update, whichever is first) */
static void join_begin(void)
{
	short seat;

	if (ae_seats_active())
		return;
	ae_seats_begin();
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		join.profile[seat] = NONE;
		join.joined_at[seat] = 0;
		join.row[seat] = ROW_PROFILE;
	}
}

/* "PLAYER a and PLAYER b swapped controllers." (made here: the build's wchar_t is two bytes, no swprintf) */
static void join_swap_message(int seat, int other)
{
	wchar_t a[2] = { (wchar_t)(L'1' + seat), 0 }, b[2] = { (wchar_t)(L'1' + other), 0 };
	wchar_t text[JOIN_TEXT_LENGTH];

	join_concat(text, L"PLAYER ", a);
	join_concat(join.message_text, text, L" and PLAYER ");
	join_concat(text, join.message_text, b);
	join_concat(join.message_text, text, L" swapped controllers.");
	join_message(join.message_text);
}

/* a change from the picker (AE_SEATS_* bits): a new seat's profile, the swap said */
static void join_picker_result(unsigned bits)
{
	int seat, other;

	if (!(bits & AE_SEATS_ASSIGNED))
		return;
	ae_seats_last_change(&seat, &other);
	if (bits & AE_SEATS_JOINED)
	{
		join_profiles_read();
		if (seat >= 0 && seat < AE_SEATS_MAXIMUM)
		{
			join.joined_at[seat] = system_milliseconds();
			join.row[seat] = ROW_PROFILE;
			join_profile_pick((short)seat);
		}
	}
	if ((bits & AE_SEATS_SWAPPED) && seat >= 0 && other >= 0)
		join_swap_message(seat, other);
}

void ae_split_screen_join_update(struct widget_instance *screen)
{
	unsigned bits;
	short seat;
	struct ae_seats seats;
	boolean any;

	while (screen && screen->parent)
		screen = screen->parent;
	if (!screen)
		return;
	join_begin();
	bits = ae_seats_update();
	ae_seats_get(&seats);
	if (bits & (AE_SEATS_JOINED | AE_SEATS_CHANGED))
	{
		join_profiles_read();
		for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
		{
			if (seats.seat[seat].kind == AE_SEAT_EMPTY)
				continue;
			if (join.profile[seat] == NONE)
				join.joined_at[seat] = system_milliseconds();
			join_profile_pick(seat);
		}
	}
	if (bits & AE_SEATS_FULL)
		join_message(L"4 players at most on one PC.");
	join_picker_result(bits);
	if (bits & AE_SEATS_WAIT_ENDED)
		join_message(L"No controller picked.");
	if (bits & AE_SEATS_BACK)
	{
		/* (a device with no seat: back to Multiplayer, as the screen's B does, with every seat let go) */
		join.back_requested = TRUE;
		platform_log("split screen: a device with no seat asks to go back");
		ui_widget_port_post_button(0, JOIN_BUTTON_B);
	}
	any = ae_seats_taken(&seats) > 0;
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
		join_card_update(screen, seat);
	join_show(child_named(screen, "join_help_ready"), any);
	join_show(child_named(screen, "join_help_nobody"), !any);
	join_show(child_named(screen, "join_continue_off"), !any);
	{
		int left, waiting = ae_seats_assign_waiting(&left);

		/* (ASSIGN's wait says what to do on the message line) */
		join_show(child_named(screen, "join_more"), waiting < 0 &&
			!(join.message && system_milliseconds() < join.message_until));
		if (waiting >= 0)
		{
			wchar_t number[2] = { (wchar_t)(L'1' + waiting), 0 }, text[JOIN_TEXT_LENGTH];

			join_concat(text, L"PLAYER ", number);
			join_concat(join.message_text, text, L": press any button on the controller to use.");
			join_text(child_named(screen, "join_message"), join.message_text);
		}
		else
		{
			join_text(child_named(screen, "join_message"),
				join.message && system_milliseconds() < join.message_until ? join.message : L"");
		}
	}
}

int ae_split_screen_seats_wanted(void)
{
	return join.open || network_game_is_splitscreen_local() || (split_screen_chosen && global_network_game_server_get());
}

/* "ae split continue": the gaps closed, each seat's player on its profile, then the game made (as Create Game's
"join controller to mp game" makes it, but every seat on its own profile) */
static boolean join_continue(struct widget_instance *widget, boolean *widget_deleted)
{
	int moved[AE_SEATS_MAXIMUM];
	short profiles[AE_SEATS_MAXIMUM];
	struct ae_seats seats;
	short seat, taken;

	*widget_deleted = FALSE;
	join_profiles_read();
	ae_seats_get(&seats);
	if (!ae_seats_present(&seats))
		return FALSE;
	taken = (short)ae_seats_close(moved);
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
		profiles[seat] = NONE;
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (moved[seat] >= 0)
			profiles[moved[seat]] = join.profile[seat];
	}
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
		join.profile[seat] = profiles[seat];
	for (seat = 0; seat < taken; seat++)
	{
		long profile_index = join.profile[seat] != NONE && join.profile[seat] < join.count ?
			join.indices[join.profile[seat]] : NONE;

		/* (NEW PROFILE: one made now, by the first untitled name, as Settings makes one: renamed in PROFILES) */
		if (profile_index == NONE)
		{
			wchar_t name[128];

			saved_game_file_get_useable_untitled_profile_name(name);
			name[11] = L'\0';
			profile_index = player_profile_new(seat, name);
			platform_log("split screen: P%d made a new profile", seat + 1);
		}
		if (profile_index == NONE || !ui_widget_port_multiplayer_player(seat, profile_index))
		{
			platform_log("split screen: P%d has no profile to play with", seat + 1);
			return FALSE;
		}
	}
	platform_log("split screen: %d people", taken);
	split_screen_chosen = TRUE;
	p2p_set_hosting_allowed(FALSE);
	p2p_set_hosting_public(FALSE);
	return ui_widget_port_host(widget, NULL, widget_deleted);
}

/* a seat's profile, the next or previous no other seat has */
static void join_profile_step(short seat, short direction)
{
	short next = join_profile_next(seat, join.profile[seat] != NONE ? join.profile[seat] : 0, direction);

	if (next != NONE)
		join.profile[seat] = next;
}

static boolean lobby_pick_event(char const *name, struct widget_instance *widget, short controller);

unsigned char ae_split_screen_event(char const *name, struct widget_instance *widget, short controller,
	unsigned char *widget_deleted)
{
	struct ae_seats seats;
	boolean seated;
	boolean deleted = FALSE;

	if (!strncmp(name, "ae split lpick ", 15) || !strcmp(name, "ae split lobby assign"))
		return lobby_pick_event(name, widget, controller);
	ae_seats_get(&seats);
	seated = controller >= 0 && controller < AE_SEATS_MAXIMUM && seats.seat[controller].kind != AE_SEAT_EMPTY;
	if (!strcmp(name, "ae split join open"))
	{
		join.open = TRUE;
		join.back_requested = FALSE;
		join_begin();
		join_profiles_read();
		return TRUE;
	}
	if (!strcmp(name, "ae split join close"))
	{
		join.open = FALSE;
		return TRUE;
	}
	if (!strcmp(name, "ae split b"))
	{
		/* (a seated player's B leaves the seat; a device with no seat's goes back, every seat let go) */
		if (!join.back_requested && seated)
		{
			ae_seats_leave_seat(controller);
			join.profile[controller] = NONE;
			return FALSE;
		}
		join.back_requested = FALSE;
		join.open = FALSE;
		ae_seats_end_released();
		return TRUE;
	}
	if (!strcmp(name, "ae split back key"))
	{
		/* (the BACK event: a pad's BACK, and the keyboard's ESC, which also presses B on the keyboard's own port: a
		seated keyboard leaves through that B, so its BACK does nothing; a device with no seat goes back) */
		if (seated || ae_seats_keyboard(&seats) >= 0)
			return FALSE;
		join.back_requested = FALSE;
		join.open = FALSE;
		ae_seats_end_released();
		return TRUE;
	}
	if (!strcmp(name, "ae split back"))
	{
		join.open = FALSE;
		ae_seats_end_released();
		return TRUE;
	}
	if (!strcmp(name, "ae split profile left") || !strcmp(name, "ae split profile right"))
	{
		/* (a player's own left and right: the row its up and down chose, its CONTROLLER or its PROFILE) */
		if (!seated)
			return FALSE;
		if (join.row[controller] == ROW_CONTROLLER)
			join_picker_result(ae_seats_cycle(controller, strstr(name, "right") ? 1 : -1));
		else
			join_profile_step(controller, strstr(name, "right") ? 1 : -1);
		return TRUE;
	}
	if (!strcmp(name, "ae split row up") || !strcmp(name, "ae split row down"))
	{
		if (!seated)
			return FALSE;
		join.row[controller] = !strcmp(name, "ae split row up") ? ROW_CONTROLLER : ROW_PROFILE;
		return TRUE;
	}
	if (!strcmp(name, "ae split own assign"))
	{
		if (!seated)
			return FALSE;
		ae_seats_assign_wait(controller);
		return TRUE;
	}
	/* (a card's own items, the mouse's: the seat is the name's last word) */
	{
		size_t length = strlen(name);
		short seat = length ? (short)(name[length - 1] - '0') : NONE;

		if (length > 2 && name[length - 2] == ' ' && seat >= 0 && seat < AE_SEATS_MAXIMUM)
		{
			boolean taken = seats.seat[seat].kind != AE_SEAT_EMPTY;

			if (!strncmp(name, "ae split dev ", 13) && taken)
				join_picker_result(ae_seats_cycle(seat, strstr(name, "left") ? -1 : 1));
			else if (!strncmp(name, "ae split prof ", 14) && taken)
				join_profile_step(seat, strstr(name, "left") ? -1 : 1);
			else if (!strncmp(name, "ae split assign ", 16) && taken)
			{
				if (ae_seats_assign_waiting(NULL) == seat)
					ae_seats_assign_cancel();
				else
					ae_seats_assign_wait(seat);
			}
			else if (!strncmp(name, "ae split leave ", 15) && taken)
			{
				ae_seats_leave_seat(seat);
				join.profile[seat] = NONE;
			}
			else if (!strncmp(name, "ae split cand take ", 19) && !taken)
				join_picker_result(ae_seats_candidate_take(seat));
			else if (!strncmp(name, "ae split cand ", 14) && !taken)
				ae_seats_candidate_cycle(seat, strstr(name, "left") ? -1 : 1);
			else
				return FALSE;
			return TRUE;
		}
	}
	if (!strcmp(name, "ae split continue"))
	{
		boolean result;

		/* (the press that seated the device is not its CONTINUE) */
		if (seated && system_milliseconds() - join.joined_at[controller] < JOIN_PRESS_GUARD)
			return FALSE;
		result = join_continue(widget, &deleted);

		*widget_deleted = (unsigned char)deleted;
		return result;
	}
	return FALSE;
}

static unsigned lobby_seats_update(void)
{
	unsigned bits;

	if (!ae_seats_active())
		return 0;
	bits = ae_seats_update();
	if (bits & AE_SEATS_FULL)
		platform_log("split screen: a fifth device pressed START (4 players at most on one PC)");
	return bits;
}

/* ---- the lobby's controller picker: this machine's players' rows (those the lobby marks [Pn]) get their
controller's name, arrows and ASSIGN. A split screen game's go through the seats; a LAN or internet game's through
the stable pad ports (ae_platform.c: the keyboard stays player 1's there) */

unsigned long ae_platform_port_pad(int port);
int ae_platform_port_give(int port, unsigned long pad_id);
void ae_platform_port_pick_wait(int port);
void ae_platform_port_pick_cancel(void);
int ae_platform_port_pick_waiting(int *left_ms);
unsigned ae_platform_port_pick_update(int *port, int *other);

#define LOBBY_PICK_ROWS 11
/* the row's name's place: the PC lobby's (centred on the row), or left of the picker (its centre about a quarter in) */
#define LOBBY_NAME_X 25
#define LOBBY_PICK_NAME_X (-78) /* (centred between the focus arrows and the picker's: -72 let "New001 2 [P2]" touch its arrow at 1080p, -84 the focus arrows) */

/* the row's player's port, from its text's "[Pn]" (only this machine's players, with two or more, have one); -1 */
static short lobby_row_port(struct widget_instance *list, short row)
{
	char name[24];
	struct widget_instance *item, *text;
	wchar_t const *letters;
	short index;

	snprintf(name, sizeof(name), "list_item_%d", row);
	item = child_named(list, name);
	text = item && item->visible ? child_named(item, "list_item_text") : NULL;
	letters = text ? text->parameters.text_box.text : NULL;
	for (index = 0; letters && letters[index] && letters[index + 1] && letters[index + 2]; index++)
	{
		if (letters[index] == L'[' && letters[index + 1] == L'P' && letters[index + 2] >= L'1' &&
			letters[index + 2] <= L'4')
		{
			return (short)(letters[index + 2] - L'1');
		}
	}
	return -1;
}

/* a port's controller as the picker names it */
static void lobby_port_name(short port, char *name, size_t size)
{
	struct ae_seat_device list[AE_SEATS_DEVICES_MAXIMUM];
	unsigned long pad = ae_platform_port_pad(port);
	int count, index;

	if (ae_seats_active())
	{
		join_device_name(port, name, size);
		return;
	}
	snprintf(name, size, "%s", port == 0 ? "KEYBOARD" : "NONE");
	count = ae_seats_devices(list, AE_SEATS_DEVICES_MAXIMUM);
	for (index = 0; pad && index < count; index++)
	{
		if (list[index].kind == AE_SEAT_PAD && list[index].pad_id == pad)
			snprintf(name, size, "%s", list[index].name);
	}
}

static int lobby_pick_waiting(void)
{
	return ae_seats_active() ? ae_seats_assign_waiting(NULL) : ae_platform_port_pick_waiting(NULL);
}

static void lobby_pick_wait(short port)
{
	if (port < 0 || port >= AE_SEATS_MAXIMUM)
		return;
	if (lobby_pick_waiting() == port)
	{
		if (ae_seats_active())
			ae_seats_assign_cancel();
		else
			ae_platform_port_pick_cancel();
	}
	else if (ae_seats_active())
		ae_seats_assign_wait(port);
	else
		ae_platform_port_pick_wait(port);
}

/* the next or previous pad for a port (a LAN game's: pads only, swapped with the port that had it) */
static unsigned lobby_pick_cycle(short port, short direction)
{
	struct ae_seat_device list[AE_SEATS_DEVICES_MAXIMUM];
	unsigned long pads[AE_SEATS_DEVICES_MAXIMUM], current;
	int count, pad_count = 0, index, at = -1, other;

	if (port < 0 || port >= AE_SEATS_MAXIMUM)
		return 0;
	if (ae_seats_active())
		return ae_seats_cycle(port, direction);
	count = ae_seats_devices(list, AE_SEATS_DEVICES_MAXIMUM);
	for (index = 0; index < count; index++)
	{
		if (list[index].kind == AE_SEAT_PAD)
			pads[pad_count++] = list[index].pad_id;
	}
	if (!pad_count)
		return 0;
	current = ae_platform_port_pad(port);
	for (index = 0; index < pad_count; index++)
	{
		if (pads[index] == current)
			at = index;
	}
	if (at < 0)
		at = direction < 0 ? 0 : pad_count - 1;
	at = ((at + direction) % pad_count + pad_count) % pad_count;
	if (pads[at] == current)
		return 0;
	other = ae_platform_port_give(port, pads[at]);
	if (other < 0)
		return 0;
	if (other != port)
		join_swap_message(port, other);
	return AE_SEATS_ASSIGNED | (other != port ? AE_SEATS_SWAPPED : 0);
}

static void lobby_pick_show(struct widget_instance *list, char const *name, short row, boolean visible)
{
	char full[40];

	snprintf(full, sizeof(full), "%s_%d", name, row);
	join_show(child_named(list, full), visible);
}

static void lobby_pick_update(struct widget_instance *list, unsigned bits)
{
	struct widget_instance *screen = list;
	int changed_port, changed_other, waiting;
	boolean any = FALSE;
	short row;

	while (screen && screen->parent)
		screen = screen->parent;
	if (!screen)
		return;
	if (!ae_seats_active())
	{
		bits |= ae_platform_port_pick_update(&changed_port, &changed_other);
		if ((bits & AE_SEATS_SWAPPED) && changed_port >= 0 && changed_other >= 0)
			join_swap_message(changed_port, changed_other);
	}
	else
		join_picker_result(bits);
	if (bits & AE_SEATS_WAIT_ENDED)
		join_message(L"No controller picked.");
	waiting = lobby_pick_waiting();
	for (row = 0; row < LOBBY_PICK_ROWS; row++)
	{
		short port = lobby_row_port(list, row);
		boolean shown = port >= 0, waits = shown && waiting == port;
		char name[24], full[40];
		boolean large;

		any |= shown;
		/* (the row's name: centred on the row, or on its left part when the picker takes the right) */
		{
			char item_name[24];
			struct widget_instance *item, *text;

			snprintf(item_name, sizeof(item_name), "list_item_%d", row);
			item = child_named(list, item_name);
			text = item ? child_named(item, "list_item_text") : NULL;
			if (text)
				text->horizontal_offset = shown ? LOBBY_PICK_NAME_X : LOBBY_NAME_X;
		}
		lobby_pick_show(screen, "lobby_pick_l", row, shown && !waits);
		lobby_pick_show(screen, "lobby_pick_r", row, shown && !waits);
		lobby_pick_show(screen, "lobby_pick_arrow_left", row, shown && !waits);
		lobby_pick_show(screen, "lobby_pick_arrow_right", row, shown && !waits);
		lobby_pick_show(screen, "lobby_pick_assign", row, shown && !waits);
		lobby_pick_show(screen, "lobby_pick_cancel", row, waits);
		lobby_pick_show(screen, "lobby_pick_wait", row, waits);
		if (shown && !waits)
			lobby_port_name(port, name, sizeof(name));
		else
			name[0] = 0;
		shown = shown && !waits;
		large = strlen(name) <= JOIN_LARGE_FIT;
		snprintf(full, sizeof(full), "lobby_pick_value_large_%d", row);
		join_show(child_named(screen, full), shown && large);
		if (shown && large)
			join_text_ascii(child_named(screen, full), name);
		snprintf(full, sizeof(full), "lobby_pick_value_small_%d", row);
		join_show(child_named(screen, full), shown && !large);
		if (shown && !large)
			join_text_ascii(child_named(screen, full), name);
	}
	{
		struct widget_instance *help = child_named(screen, "lobby_pick_help");

		if (waiting >= 0)
		{
			wchar_t number[2] = { (wchar_t)(L'1' + waiting), 0 }, text[JOIN_TEXT_LENGTH];

			join_concat(text, L"PLAYER ", number);
			/* (the lobby's help line is 380 wide: these fit it at 1080p, where the longer ones were cut) */
			join_concat(join.message_text, text, L": press a button on the new controller.");
			join_text(help, join.message_text);
		}
		else if (join.message && system_milliseconds() < join.message_until)
			join_text(help, join.message);
		else
			join_text(help, any ? L"Y or ASSIGN, then a button on the new controller." : L"");
	}
}

/* "ae split lpick left|right|assign <row>" and "ae split lobby assign" (a player's own Y) */
static boolean lobby_pick_event(char const *name, struct widget_instance *widget, short controller)
{
	struct widget_instance *list = widget;
	char const *space = strrchr(name, ' ');
	short row, port;

	if (!strcmp(name, "ae split lobby assign"))
	{
		lobby_pick_wait(controller);
		return TRUE;
	}
	while (list && list->parent)
		list = list->parent;
	list = child_named(list, "lobby_list");
	row = space ? (short)atoi(space + 1) : NONE;
	port = list && row >= 0 && row < LOBBY_PICK_ROWS ? lobby_row_port(list, row) : -1;
	if (port < 0)
		return FALSE;
	if (strstr(name, " assign "))
		lobby_pick_wait(port);
	else
	{
		unsigned bits = lobby_pick_cycle(port, strstr(name, " left ") ? -1 : 1);

		if (ae_seats_active())
			join_picker_result(bits);
	}
	return TRUE;
}
