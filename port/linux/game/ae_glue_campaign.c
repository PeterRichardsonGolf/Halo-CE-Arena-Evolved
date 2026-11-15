/*
AE_GLUE_CAMPAIGN.C

AE's CAMPAIGN glue. See ae_glue_campaign.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "main/main.h"
#include "saved games/player_profile.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "ae_glue_campaign.h"
#include "ae_glue_profiles.h"
#include "ae_input.h"

void platform_log(char const *format, ...);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
boolean ui_widget_port_saved_game(char const **map_name, short *level, short *difficulty);

typedef char verify_levels[AE_CAMPAIGN_LEVELS == NUMBER_OF_SINGLE_PLAYER_LEVELS ? 1 : -1];

static const char *const difficulty_names[AE_CAMPAIGN_DIFFICULTIES] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };

const char *ae_campaign_difficulty_name(short difficulty)
{
	return difficulty >= 0 && difficulty < AE_CAMPAIGN_DIFFICULTIES ? difficulty_names[difficulty] : "";
}

/* the game's text on one line in capitals (its line breaks spaces, runs of spaces one: as string_get) */
static void narrow(wchar_t const *wide, char *text, int size)
{
	int length = 0;

	for (; wide && *wide && length < size - 1; wide++)
	{
		wchar_t character = *wide == '\r' || *wide == '\n' || *wide == '\t' ? ' ' : *wide;
		char c = character >= 0x20 && character < 0x7F ? (char)character : '?';

		if (c >= 'a' && c <= 'z')
			c = (char)(c - 32);
		if (c != ' ' || (length && text[length - 1] != ' '))
			text[length++] = c;
	}
	while (length && text[length - 1] == ' ')
		length--;
	text[length] = 0;
}

/* player 1's profile (the active one, else the one used last, else the first; with make, one made when this PC has
none), made player 1's on controller 0 */
static int player1_profile(struct player_profile *profile, long *index, int make)
{
	long profile_index = player_ui_get_active_player_profile_index(0);

	if (profile_index == NONE || !player_profile_get(profile_index, profile))
	{
		profile_index = ae_profile_player1_fallback(profile);
		if (profile_index == NONE)
		{
			/* (no profile on this PC at all: one made, "Player", as a first run would; PROFILES renames it) */
			int made = -1;

			/* (the API's int is the index's 32 bits: -1 is NONE, as ae_glue_profiles.c's file_index) */
			if (!make || !ae_profile_new("Player", &made).ok || made == -1)
				return 0;
			profile_index = (long)(unsigned int)made;
			if (!player_profile_get(profile_index, profile))
				return 0;
			platform_log("ae campaign: no profile on this PC: made 'Player'");
		}
	}
	player_ui_set_active_player_profile(0, profile_index, profile);
	player_ui_set_single_player_local_player_controller(0, 0);
	ae_input_controller_assigned(0);
	*index = profile_index;
	return 1;
}

int ae_campaign_read(struct ae_campaign *campaign)
{
	struct player_profile profile;
	long index;
	long names = tag_loaded('ustr', "pc\\main_menu\\map_list");
	short level, highest_level, highest_difficulty, marker;
	char const *map_name;

	memset(campaign, 0, sizeof(*campaign));
	campaign->saved_level = campaign->saved_difficulty = -1;
	for (level = 0; level < AE_CAMPAIGN_LEVELS; level++)
	{
		if (names != NONE)
			narrow(unicode_string_list_get_string(names, level), campaign->levels[level].name,
				sizeof(campaign->levels[level].name));
		if (!campaign->levels[level].name[0])
			snprintf(campaign->levels[level].name, sizeof(campaign->levels[level].name), "%s",
				main_get_solo_level_name(level));
	}
	campaign->difficulty = main_get_difficulty();
	if (!player1_profile(&profile, &index, 0))
	{
		campaign->levels[0].reached = 1;
		return 0;
	}
	campaign->have_profile = 1;
	narrow(profile.player_name, campaign->profile, sizeof(campaign->profile));
	player_profile_get_highest_completed_solo_level(&profile, &highest_level, &highest_difficulty);
	for (level = 0; level < AE_CAMPAIGN_LEVELS; level++)
	{
		byte flags = profile.single_player_map_flags[level];

		campaign->levels[level].reached = flags || level == highest_level + 1 || level == 0;
		for (marker = 0; marker < 3; marker++)
			campaign->levels[level].finished[marker + 1] = (flags >> (marker + 1)) & 1;
	}
	if (ui_widget_port_saved_game(&map_name, &campaign->saved_level, &campaign->saved_difficulty))
		platform_log("ae campaign: saved game: level %d, %s", campaign->saved_level + 1,
			ae_campaign_difficulty_name(campaign->saved_difficulty));
	else
		campaign->saved_level = campaign->saved_difficulty = -1;
	return 1;
}

/* as menu_functions.c's campaign_start */
static void start(char const *map_name, short difficulty)
{
	main_set_map_name(map_name);
	main_set_difficulty(difficulty);
	game_connection_set(0);
	main_menu_switch_to_single_player();
	player_ui_remember_player1_profile(TRUE);
}

int ae_campaign_start(struct ae_campaign const *campaign, short level, short difficulty)
{
	struct player_profile profile;
	long index;

	if (level < 0 || level >= AE_CAMPAIGN_LEVELS || !campaign->levels[level].reached ||
		!player1_profile(&profile, &index, 1))
		return 0;
	platform_log("ae campaign: start level %d on %s", level + 1, ae_campaign_difficulty_name(difficulty));
	start(main_get_solo_level_name(level), (short)(difficulty < 0 ? 0 : difficulty > 3 ? 3 : difficulty));
	return 1;
}

int ae_campaign_continue(struct ae_campaign const *campaign)
{
	struct player_profile profile;
	char const *map_name;
	short level, difficulty;
	long index;

	if (campaign->saved_level < 0 || !player1_profile(&profile, &index, 1) ||
		!ui_widget_port_saved_game(&map_name, &level, &difficulty))
		return 0;
	platform_log("ae campaign: continue level %d on %s", level + 1, ae_campaign_difficulty_name(difficulty));
	start(map_name, difficulty);
	return 1;
}
