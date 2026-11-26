/*
AE_RANDOMIZER_GLUE.C

What the campaign randomizer (randomizer.c, from Content Evolved) calls and this build has no copy
of: the shared multiplayer profiles of tag_multiplayer.c (a game that hosts or joins none here, so
every question answers none), and Floodfection's
tags (infection.c). The randomizer's own glue is ae_randomizer_glue.inc.
*/

#include "cseries/cseries.h"

#include "ae_randomizer_glue.h"
#include "community.h"
#include "tag_multiplayer.h"

/* ---------- Floodfection's tags */

boolean infection_tag_import_wanted(void)
{
	return FALSE;
}

void infection_tag_import(void)
{
}

/* ---------- shared multiplayer profiles */

long tag_multiplayer_game_campaign_profile(void)
{
	return NONE;
}

long tag_multiplayer_game_randomizer_profile(void)
{
	return NONE;
}

boolean tag_multiplayer_campaign_hosted(void)
{
	return FALSE;
}

boolean tag_multiplayer_randomizer_shared(void)
{
	return FALSE;
}

boolean tag_multiplayer_campaign_takes(char const *level_name)
{
	(void)level_name;

	return FALSE;
}

long tag_multiplayer_firefight_level_profile(char const *scenario_name)
{
	(void)scenario_name;

	return NONE;
}

long tag_multiplayer_forge_level_profile(char const *scenario_name)
{
	(void)scenario_name;

	return NONE;
}

long tag_multiplayer_level_profile(char const *scenario_name)
{
	(void)scenario_name;

	return NONE;
}

long tag_multiplayer_campaign_level_profile(char const *scenario_name)
{
	(void)scenario_name;

	return NONE;
}

long tag_multiplayer_randomizer_level_profile(char const *scenario_name)
{
	(void)scenario_name;

	return NONE;
}

void tag_multiplayer_level_loaded(long profile, long const *roots, long const *root_tags, long count)
{
	(void)profile;
	(void)roots;
	(void)root_tags;
	(void)count;
}

void tag_multiplayer_level_unloaded(void)
{
}
