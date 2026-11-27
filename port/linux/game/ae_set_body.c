/*
AE_SET_BODY.C

See ae_set_body.h. The body a campaign level played alone gives the player: the playing set's biped (its PLAY AS IT) or a campaign
swap's. Which one is ae_item_sets_rules.c's ae_is_body_source; this file reads the settings, finds the biped among the tags the
level added, checks it fits a player (ae_biped_pack_rules.h's rule, on the tags as loaded) and tells the set's file the answer, which
the ITEM SETS card shows.
*/

#include "cseries.h"
#include "cseries_windows.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "math/real_math.h"
#include "scenario/scenario_definitions.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "units/biped_definitions.h"
#include "models/model_animation_definitions.h"
#include "physics/collision_model_definitions.h"
#include "networking/network_game_globals.h"
#include "community.h"
#include "halo_firefight.h"
#include "tag_campaign.h"
#include "ae_item_sets_rules.h"
#include "ae_set_body.h"
#include "ae_forge_engine.h"
#include "ae_weapon_seats_rules.h"
#include "../src/port_config.h"
#include "../src/port_tag_profiles.h"
#include "../src/port_tag_mp_profiles.h"

#include <stdio.h>
#include <string.h>

void platform_log(char const *format, ...);

/* the facts of a biped as loaded: the sizes from the biped, the physics flag, and whether it has a graph */
static struct animation_graph *biped_facts(struct biped_definition const *biped, struct ae_biped_facts *facts)
{
	struct animation_graph *graph = biped->object.animation_graph.index != NONE ?
		(struct animation_graph *)tag_get(ANIMATION_GRAPH_TAG, biped->object.animation_graph.index) : NULL;

	memset(facts, 0, sizeof(*facts));
	facts->radius = biped->biped.collision_radius;
	facts->standing_height = biped->biped.collision_height_standing;
	facts->standing_eye = biped->biped.standing_camera_height;
	facts->crouching_eye = biped->biped.crouching_camera_height;
	facts->uses_player_physics = TEST_FLAG(biped->biped.flags, _biped_uses_player_physics_bit);
	facts->has_animation_graph = graph != NULL;
	return graph;
}

static boolean graph_has_seat(struct animation_graph const *graph, char const *label)
{
	short seat_index;

	for (seat_index = 0; seat_index < graph->unit_seats.count; seat_index++)
	{
		struct animation_graph_unit_seat *seat =
			TAG_BLOCK_GET_ELEMENT(&graph->unit_seats, seat_index, struct animation_graph_unit_seat);

		if (!_stricmp(seat->label, label))
			return TRUE;
	}
	return FALSE;
}

/* whether the biped fits the map's player (sizes, physics, every vehicle seat of the stock player's graph); the reason when not */
static boolean body_fits(long biped_index, char *reason, int size)
{
	long globals_index = tag_loaded(GAME_GLOBALS_TAG, "globals\\globals");
	struct game_globals *globals = globals_index == NONE ? NULL : game_globals_definition_get(globals_index);
	struct game_globals_multiplayer_information *information = globals && globals->multiplayer_information.count > 0 ?
		TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0, struct game_globals_multiplayer_information) : NULL;
	/* (a campaign level's globals have no multiplayer block: its player is the player block's unit) */
	long stock_index = information ? information->unit.index : NONE;
	struct biped_definition const *stock;
	struct biped_definition const *biped = (struct biped_definition const *)tag_get(BIPED_DEFINITION_TAG, biped_index);
	struct ae_biped_facts stock_facts, facts;
	struct animation_graph *stock_graph, *graph;
	short seat_index;

	reason[0] = 0;
	if (stock_index == NONE && globals && globals->player_information.count > 0)
		stock_index = TAG_BLOCK_GET_ELEMENT(&globals->player_information, 0, struct game_globals_player_information)->player_unit.index;
	stock = stock_index != NONE ? (struct biped_definition const *)tag_get(BIPED_DEFINITION_TAG, stock_index) : NULL;
	if (!stock)
	{
		snprintf(reason, (size_t)size, "the map's player is not known");
		return FALSE;
	}
	stock_graph = biped_facts(stock, &stock_facts);
	graph = biped_facts(biped, &facts);
	if (stock_graph && graph)
	{
		for (seat_index = 0; seat_index < stock_graph->unit_seats.count; seat_index++)
		{
			struct animation_graph_unit_seat *seat =
				TAG_BLOCK_GET_ELEMENT(&stock_graph->unit_seats, seat_index, struct animation_graph_unit_seat);

			if (!seat->label[0] || !ae_weapon_seats_counted(seat->label))
				continue;
			facts.seats_stock++;
			if (!graph_has_seat(graph, seat->label) && !facts.seats_missing++)
				snprintf(facts.seat_first, sizeof(facts.seat_first), "%.31s", seat->label);
		}
	}
	return ae_biped_pack_fitness(&stock_facts, &facts, reason, size) != 0;
}

/* the set playing: its folder's profile, or NONE */
static long playing_set(void)
{
	char const *folder = config_string("game.randomizer_tag_profile");

	if (!config_boolean("game.tag_injection") || !folder || !folder[0])
		return NONE;
	/* (the profiles as the level loaded them: a scan now would forget the indices the level holds) */
	return port_tag_profiles_find(folder);
}

/* the profile tag of a group and path the body names, to write what the level found out */
static void fit_note(long profile, char const *path, int fits)
{
	long tag = port_tag_profiles_tag_find(profile, BIPED_DEFINITION_TAG, path);

	if (tag >= 0)
		port_tag_profiles_tag_set_option(profile, tag, "fit", fits ? "yes" : "no");
}

/* the body this game plays, as the level has it */
struct body_choice
{
	int source, game;
	long set, swap;
	char const *path;
	char const *hands, *movement;
	long biped_index;
	int fits;
	char reason[AE_BIPED_PACK_REASON_SIZE];
};

/* where the body comes from and whether it can play: the source (AE_IS_BODY_NONE in any game but a campaign level played alone, or with no
body named), the biped's tag in this level (NONE when the level has none) and the fit */
static int body_choose(struct body_choice *choice)
{
	char const *swap_path, *set_path;
	int game = AE_IS_GAME_CAMPAIGN_ALONE;

	memset(choice, 0, sizeof(*choice));
	choice->biped_index = NONE;
	choice->source = AE_IS_BODY_NONE;
	/* (the game first: a multiplayer, online, Firefight or challenge game never reads a set's settings) */
	if (game_engine_running())
		game = AE_IS_GAME_MULTIPLAYER;
	else if (firefight_active())
		game = AE_IS_GAME_FIREFIGHT;
	else if (community_challenge_running())
		game = AE_IS_GAME_CHALLENGE;
	else if (network_game_get_game())
		game = AE_IS_GAME_ONLINE;
	choice->game = game;
	if (game != AE_IS_GAME_CAMPAIGN_ALONE)
		return AE_IS_BODY_NONE;
	choice->swap = tag_campaign_profile_chosen();
	choice->set = playing_set();
	swap_path = choice->swap != NONE ? port_tag_campaign_profiles_option(choice->swap, "player_biped") : "";
	set_path = choice->set != NONE ? port_tag_profiles_option(choice->set, "player_biped") : "";
	choice->source = ae_is_body_source(game, config_boolean("game.randomizer") != 0, choice->set != NONE, set_path[0] != 0,
		choice->swap != NONE, swap_path[0] != 0);
	if (choice->source == AE_IS_BODY_NONE)
		return AE_IS_BODY_NONE;
	if (choice->source == AE_IS_BODY_SWAP)
	{
		choice->path = swap_path;
		choice->hands = port_tag_campaign_profiles_option(choice->swap, "player_hands");
		choice->movement = port_tag_campaign_profiles_option(choice->swap, "player_movement");
	}
	else
	{
		choice->path = set_path;
		choice->hands = port_tag_profiles_option(choice->set, "player_hands");
		choice->movement = port_tag_profiles_option(choice->set, "player_movement");
	}
	choice->biped_index = tag_loaded(BIPED_DEFINITION_TAG, choice->path);
	if (choice->biped_index == NONE)
		return choice->source;
	choice->fits = body_fits(choice->biped_index, choice->reason, (int)sizeof(choice->reason)) != 0;
	if (choice->set != NONE)
		fit_note(choice->set, choice->path, choice->fits);
	return choice->source;
}

long ae_set_body_biped(long unit_definition_index)
{
	struct body_choice choice;

	if (body_choose(&choice) == AE_IS_BODY_NONE)
		return unit_definition_index;
	if (choice.biped_index == NONE)
	{
		platform_log("set body: %s is not in this level (it is off in its set, or the level could not add it): the campaign's player", choice.path);
		return unit_definition_index;
	}
	if (!choice.fits)
	{
		platform_log("set body: %s does not fit a player (%s): the campaign's player", choice.path, choice.reason);
		return unit_definition_index;
	}
	return choice.biped_index;
}

void ae_set_body_players(int quiet)
{
	struct body_choice choice;
	int layer;

	body_choose(&choice);
	layer = ae_is_body_layer(choice.source, choice.biped_index != NONE, choice.fits);
	if (layer == AE_IS_LAYER_LEAVE)
	{
		if (!quiet && choice.game != AE_IS_GAME_CAMPAIGN_ALONE && config_boolean("game.randomizer") && playing_set() != NONE)
			platform_log("set body: not played, this game is not a campaign level played alone (game %d: companions, online, Firefight or a challenge)",
				choice.game);
		return;
	}
	if (layer == AE_IS_LAYER_CLEAR)
	{
		/* (a campaign swap's body that cannot play was put in the layer by the campaign's own players: not used) */
		ae_forge_players_body(NONE, "", "", FALSE);
		return;
	}
	/* (a set's hands: "" its own, picked from its biped's map; a campaign swap's keep what the swap's file says, "" the campaign's own) */
	if (choice.source == AE_IS_BODY_SET && ae_is_hands_choice(choice.hands) == AE_IS_HANDS_OWN)
		ae_forge_players_body(choice.set, choice.path, NULL, ae_is_movement_own(choice.movement));
	else
		ae_forge_players_body(choice.set, choice.path, ae_is_hands_choice(choice.hands) == AE_IS_HANDS_CAMPAIGN ? "" : choice.hands,
			ae_is_movement_own(choice.movement));
	if (!quiet)
		platform_log("set body: %s plays as %s, hands %s, movement %s", choice.source == AE_IS_BODY_SWAP ? "a campaign swap" : "the set",
			choice.path, ae_is_hands_choice(choice.hands) == AE_IS_HANDS_CHOSEN ? choice.hands :
			ae_is_hands_choice(choice.hands) == AE_IS_HANDS_CAMPAIGN || choice.source == AE_IS_BODY_SWAP ? "the campaign's own" : "its own",
			ae_is_movement_own(choice.movement) ? "its own" : "the campaign's player's");
}
