/*
AE_FORGE_GLUE.C

What Content Evolved's Forge (port/forge) asks of this build that it has no copy of: the account service's count of
what was made, the social chat's typing state, the classic menus' map list and the Custom Edition level titles, MCC's
own pause menus, the controller glyph artwork (Forge draws its own prompts) and the profile a Forge game plays. Each
answers as a build with no such part: nothing made, no chat typed, no map listed, no artwork, no profile.
*/

#include <stdio.h>

#include "cseries/cseries.h"
#include "../src/port_config.h"

#include "social_creator.h"
#include "tag_multiplayer.h"

struct bitmap_data;

/* ---------- what was made (CAREER's FORGE rows: the maps saved under a new name, kept in the settings; the time is ae_forge_dev.c's) */

void social_creator_add(short field, long count)
{
	char value[24];

	if (field != _social_creator_forge_maps || count <= 0)
		return;
	snprintf(value, sizeof(value), "%ld", (long)config_integer("forge.maps_made") + count);
	config_write("forge.maps_made", value);
}

/* ---------- the social chat (none: a line is typed in the game's own chat, which Forge asks network_chat_typing) */

boolean social_chat_typing(void)
{
	return FALSE;
}

/* ---------- MCC's pause menus (their SAVE MAP row is not drawn) */

void mcc_pause_forge(boolean session)
{
	(void)session;
}

/* ---------- controller glyph artwork (Forge draws its own prompts) */

boolean controller_glyph_lookup(short style, short slot, struct bitmap_data **bitmap, short bounds[4])
{
	(void)style;
	(void)slot;
	(void)bitmap;
	(void)bounds;
	return FALSE;
}

/* ---------- the classic menus' Custom Edition map list (this build lists them in its own screens) */

short custom_edition_maps_display_index(char const *level_name)
{
	(void)level_name;
	return NONE;
}

wchar_t *custom_edition_maps_description(short display_index)
{
	static wchar_t none[1] = { 0 };

	(void)display_index;
	return none;
}

/* ---------- a Forge game's share (the map's own files shared in a network game: none here) */

boolean tag_multiplayer_level_share_file(char const *file, char *path, unsigned long size)
{
	(void)file;
	if (path && size)
		path[0] = 0;
	return FALSE;
}

/* ---------- the profile a Forge game plays (none until a game of a saved Forge map says so) */

static long forge_game_profile = NONE;

long tag_multiplayer_game_forge_profile(void)
{
	return forge_game_profile;
}

void tag_multiplayer_game_forge_profile_set(long profile)
{
	forge_game_profile = profile;
}
