/*
AE_FIREFIGHT_GLUE.C

What Firefight (firefight*.c, from Content Evolved) leaves to this build: its
options (the game variant's options carry none of them here), and a player's
name for its HUD. The stats, rank and emblem screens of that code are not
part of this build (firefight_stubs.c).
*/

#include <string.h>

#include "cseries/cseries.h"
#include "memory/data.h"
#include "game/players.h"
#include "saved games/player_profile.h"

#include "ae_firefight_glue.h"
#include "halo_firefight.h"

/* ---------- options */

static struct firefight_options firefight_options_current;
static boolean firefight_options_initialized;

struct firefight_options const *firefight_options_get(void)
{
	if (!firefight_options_initialized)
	{
		firefight_variant_options_default(&firefight_options_current);
		firefight_options_initialized = TRUE;
	}
	return &firefight_options_current;
}

void firefight_options_set(struct firefight_options const *options)
{
	if (options)
	{
		firefight_options_current = *options;
		firefight_options_initialized = TRUE;
	}
}

/* ---------- names */

void campaign_score_player_display_name(long player_index, wchar_t *name, long count)
{
	struct player_datum const *player = player_try_and_get(player_index);
	wchar_t const *source = player ? player->name : L"";
	long length = player ? (long)NUMBEROF(player->name) : 0;
	long index;

	for (index = 0; index < count - 1 && index < length && source[index]; index++)
		name[index] = source[index];
	name[index] = 0;
	if (index == 0 && player)
	{
		char text[16];

		snprintf(text, sizeof(text), "Player %d", (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) + 1);
		for (index = 0; index < count - 1 && text[index]; index++)
			name[index] = (wchar_t)text[index];
		name[index] = 0;
	}
}

/* ---------- bots */

/* Firefight's score multiplier takes this off for the bots' skill; none here */
float bots_score_multiplier_penalty(void)
{
	return 0.0f;
}

/* ---------- stats and score panel (not part of this build) */

void firefight_stats_record_match_end(char const *map_name)
{
	(void)map_name;
}

void firefight_stats_notify_enemy_kill(long killer_player_index, short category, boolean headshot, boolean melee,
	boolean grenade, boolean boss_or_heavy, long xp)
{
	(void)killer_player_index;
	(void)category;
	(void)headshot;
	(void)melee;
	(void)grenade;
	(void)boss_or_heavy;
	(void)xp;
}

/* the campaign score's panel lifts the skulls' icons: AE's panels place their own */
short campaign_score_hud_height(void)
{
	return 0;
}

/* ---------- skulls (the skulls' own sounds are not part of this build: no sound added to a level) */

const unsigned int skulls_party_sound[1] = { 0 };
const unsigned long skulls_party_sound_size = 0;
const unsigned int skulls_pickup_sound[1] = { 0 };
const unsigned long skulls_pickup_sound_size = 0;
const unsigned int skulls_carrier_sound[1] = { 0 };
const unsigned long skulls_carrier_sound_size = 0;
const unsigned int skulls_floor_sound[1] = { 0 };
const unsigned long skulls_floor_sound_size = 0;

void firefight_stats_notify_skull_claimed(int skull_enum_id, boolean from_elite_carrier)
{
	(void)skull_enum_id;
	(void)from_elite_carrier;
}

/* (a multiplayer map played as Firefight by a menu's choice: this build has no such menu) */
boolean multiplayer_firefight_mode(void)
{
	return FALSE;
}

/* (a multiplayer game hosted with a tag profile's changes: this build hosts none, so the skulls' tags are sent to a
joining machine only for the randomizer and the hosted skulls) */
boolean tag_multiplayer_hosted(void)
{
	return FALSE;
}
