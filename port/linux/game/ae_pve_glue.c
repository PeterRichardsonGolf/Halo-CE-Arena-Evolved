/*
AE_PVE_GLUE.C

What Content Evolved's campaign score, medals, service rank and Firefight stats (ae_pve_glue.h) ask of this build
and that it has no copy of: the multiplayer score panel's numbers, the social service and
the Custom Edition level titles.
*/

#include "cseries/cseries.h"

#include "ae_pve_glue.h"
#include "ae_pve_rules.h"
#include "community.h"
#include "social.h"
#include <stdio.h>
#include <string.h>

#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

#include "halo_firefight.h"
#include "../src/ae_community.h"

/* ---------- the stats' gates (the pre-release security review reads these) */

/* a campaign level, co-op included: the games the campaign score, medals, rank and stats play in */
static int campaign_game(void)
{
	struct scenario *scenario = global_scenario_try_and_get();

	return scenario && scenario->type == _scenario_type_solo;
}

int ae_pve_records_allowed(void)
{
	return ae_pve_rules_records(campaign_game(), firefight_active() != 0);
}

int ae_stats_upload_allowed(void)
{
	return ae_pve_rules_stats_upload(campaign_game(), firefight_active() != 0, ae_community_services_on());
}

int ae_stats_socket_allowed(void)
{
	return ae_pve_rules_stats_socket(ae_community_services_on());
}

/* the transport is not part of this build: a request never leaves */
int update_request(const char *method, const char *url, const char *headers, const char *body, char *response,
	int response_size, char *error, int error_size)
{
	(void)method;
	(void)url;
	(void)headers;
	(void)body;
	if (response && response_size > 0)
		response[0] = 0;
	if (error && error_size > 0)
		snprintf(error, (size_t)error_size, "no web client in this build");

	return 0;
}

/* ---------- the multiplayer score panel */

boolean game_engine_hud_score_get(long player_index, struct game_engine_hud_score *hud_score)
{
	(void)player_index;
	if (hud_score)
		memset(hud_score, 0, sizeof(*hud_score));

	return FALSE;
}

/* ---------- Custom Edition level titles */

wchar_t const *custom_edition_maps_level_title(char const *level_name)
{
	(void)level_name;

	return NULL;
}

/* ---------- the social service (none) */

void social_firefight_changed(void)
{
}

void social_emblem_changed(void)
{
}

/* ---------- Forge (port/forge, where the build has it; else no Forge session is ever on) */

#include "opence_features.h"

#if !OPENCE_ENABLE_FORGE
int g_forge_session_enabled = 0;

int forge_level_is_session(void)
{
	return 0;
}

/* whether a Forge menu is open (the game's chat leaves the keys to it) */
int forge_menu_open(void)
{
	return 0;
}
#endif

void social_level_won(short player_index, struct community_run const *run, char const *level_name,
	wchar_t const *name)
{
	(void)player_index;
	(void)run;
	(void)level_name;
	(void)name;
}

/* ---------- co-op score notes to the clients (no message here) */

void network_coop_note_score_kill(short player_index, long variant_index, long points, short kind, byte style,
	boolean competitive)
{
	(void)player_index;
	(void)variant_index;
	(void)points;
	(void)kind;
	(void)style;
	(void)competitive;
}

void network_coop_note_score_won(boolean competitive)
{
	(void)competitive;
}

/* ---------- the rank and emblem drawing and the post-game XP card (no screen of this build reaches them) */

void game_engine_player_rank_and_emblem(long player_index, struct firefight_mcc_rank_info *out_rank,
	struct firefight_emblem_info *out_emblem)
{
	(void)player_index;
	if (out_rank)
		memset(out_rank, 0, sizeof(*out_rank));
	if (out_emblem)
		memset(out_emblem, 0, sizeof(*out_emblem));
}

void firefight_draw_mcc_rank_icon(short x0, short y0, short size, short tour_number, short rank_in_tour, real alpha)
{
	(void)x0;
	(void)y0;
	(void)size;
	(void)tour_number;
	(void)rank_in_tour;
	(void)alpha;
}

void firefight_draw_player_emblem(short x0, short y0, short size, struct firefight_emblem_info const *emblem,
	real alpha)
{
	(void)x0;
	(void)y0;
	(void)size;
	(void)emblem;
	(void)alpha;
}

void firefight_hud_xp_card_show(struct firefight_postgame_xp_summary const *summary)
{
	(void)summary;
}

void firefight_hud_xp_exit_note(void)
{
}
