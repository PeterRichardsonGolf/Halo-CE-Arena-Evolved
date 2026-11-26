/*
CAMPAIGN_MEDALS.C

The campaign's medals (always, with game.campaign_score; game.campaign_medals
shows or hides those just won, at the center left): those
of Halo 3's and Halo: Reach's that the campaign's weapons, enemies and
vehicles can earn. Each is worth its points (campaign_score_values.json's
"medals", times the difficulty and the skulls, as a kill is), logged as an
entry of the score's log (campaign_score.c), and so sent to a co-op game's
clients as the kills are.

- multi-kills: Double Kill to Killionaire, kills within the multi-kill
  window of the one before;
- sprees, kills without dying: Killing Spree (5) to Unfriggenbelievable (40);
  and of a weapon's kills, 5 and 10: Shotgun Spree and Open Season, Sniper
  Spree and Sharpshooter, Splatter Spree and Vehicular Manslaughter, and
  Stick Spree (5 plasma grenades stuck);
- a kill's: Beat Down (melee), Assassin (melee from behind), Headshot,
  Sniper Kill, Grenade Stick, Splatter, Incineration (the flamethrower), Death
  from the Grave (the killer dead), Close Call (the killer's shield down and
  health low), First Strike (the level's first kill), Revenge (the player's
  last killer killed), Avenger (another player's) and Wheelman (a passenger,
  an AI's gunner too, killing while the player drives);
- the level's: Perfection, won without dying, with enough kills.

In a multiplayer game (game.multiplayer_medals: campaign_score.c) the same
medals but Perfection are a kill of another player's, an enemy's; Revenge is
the player's last killer killed, as a player (their unit is new each time
they spawn), and Avenger a teammate's, in a team game.

The weapon is told by the killing blow's damage effect's name (the JSON's
"weapons": sniper rifle\sniper bullet, plasma grenade\attached, ...).

Only the machine that runs the AI gives medals, as it scores the kills (the
host of a co-op game); in a multiplayer game each machine gives its own
players theirs, from the kills every machine is told of (with the host's
killer and killing blow: network_damage.c), and no other machine hears of
them. Every machine shows its own players' as they are logged: the medals just won at the center left of the view, up to ten (two
rows of five, the newest first) for the JSON's popup_seconds each, and the
last few, the medal history, in the score's panel (campaign_score.c).
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "interface/ui_widget.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "units/units.h"

#include "campaign_medals.h"
#include "ae_pve_glue.h" /* AE hook */
#include "campaign_score.h"
#include "campaign_score_values.h"
#include "halo_port_limits.h"
#include "halo_firefight.h"

#include <string.h>

/* ---------- constants */

enum
{
	/* the players medals are kept for (by absolute index: campaign_score.c's;
	a multiplayer game's every player) */
	CAMPAIGN_MEDALS_MAXIMUM_PLAYERS = HALO_PORT_MAXIMUM_NETWORK_PLAYERS,
	/* the medals shown at once, in rows of */
	CAMPAIGN_MEDALS_POPUPS = 10,
	CAMPAIGN_MEDALS_POPUPS_PER_ROW = 5,
	/* the popups' icons, their gap, and where they are, in the HUD's 480
	lines: from the view's left, and their middle's height */
	CAMPAIGN_MEDALS_POPUP_SIZE = 20,
	CAMPAIGN_MEDALS_POPUP_GAP = 3,
	CAMPAIGN_MEDALS_POPUP_LEFT = 24,
	CAMPAIGN_MEDALS_POPUP_MIDDLE = 250,
	/* a kill spree's medals: one every this many kills, as many as there are */
	CAMPAIGN_MEDALS_SPREE_STEP = 5,
	/* a weapon's spree's two medals' kills */
	CAMPAIGN_MEDALS_WEAPON_SPREE_FIRST = 5,
	CAMPAIGN_MEDALS_WEAPON_SPREE_SECOND = 10,
};

/* the weapons the medals tell apart (the JSON's "weapons", in this order) */
enum
{
	_campaign_medals_weapon_sniper = 0,
	_campaign_medals_weapon_shotgun,
	_campaign_medals_weapon_flame,
	_campaign_medals_weapon_grenade_stick,
	_campaign_medals_weapon_splatter,
	NUMBER_OF_CAMPAIGN_MEDALS_WEAPONS
};

#define CAMPAIGN_MEDALS_BITMAP "pc\\medals\\icons"
/* a popup's pop: this long growing in from this much larger, and its fade
out at its end */
#define CAMPAIGN_MEDALS_POP_SECONDS 0.15f
#define CAMPAIGN_MEDALS_POP_SCALE 1.5f
#define CAMPAIGN_MEDALS_FADE_SECONDS 0.5f
/* the popups' opacity (the icons' and the name's), and the name's size, of
the HUD's text */
#define CAMPAIGN_MEDALS_POPUP_OPACITY 0.5f
#define CAMPAIGN_MEDALS_NAME_SIZE 0.5f

/* ---------- structures */

struct campaign_medals_player
{
	/* the kills without dying, all and each weapon's */
	long spree;
	long weapon_sprees[NUMBER_OF_CAMPAIGN_MEDALS_WEAPONS];
	/* the multi-kill: its kills, and the last's time (the level's ticks) */
	long multi_kill;
	long last_kill_ticks;
	/* who killed the player last (Revenge, Avenger): an AI's unit, a
multiplayer game's player (its datum index); NONE */
	long last_killer_object_index;
	long last_killer_player_index;
	/* (whose these are: the player's datum index, as they last died) */
	long player_index;
};

struct campaign_medals_popup
{
	short medal;
	unsigned long start_milliseconds;
};

/* ---------- prototypes */

/* port_config.c's, tag_files.c's, cseries_windows.c's */
int config_boolean(const char *name);
unsigned long config_changes(void);
char *tag_get_name(long tag_index);
unsigned long system_milliseconds(void);

/* ---------- globals */

struct campaign_medal_description const campaign_medal_descriptions[NUMBER_OF_CAMPAIGN_MEDALS] =
{
	{ "double_kill", "DOUBLE KILL!" },
	{ "triple_kill", "TRIPLE KILL!" },
	{ "overkill", "OVERKILL!" },
	{ "killtacular", "KILLTACULAR!" },
	{ "killtrocity", "KILLTROCITY!" },
	{ "killimanjaro", "KILLIMANJARO!" },
	{ "killtastrophe", "KILLTASTROPHE!" },
	{ "killpocalypse", "KILLPOCALYPSE!" },
	{ "killionaire", "KILLIONAIRE!" },
	{ "killing_spree", "KILLING SPREE!" },
	{ "killing_frenzy", "KILLING FRENZY!" },
	{ "running_riot", "RUNNING RIOT!" },
	{ "rampage", "RAMPAGE!" },
	{ "untouchable", "UNTOUCHABLE!" },
	{ "invincible", "INVINCIBLE!" },
	{ "inconceivable", "INCONCEIVABLE!" },
	{ "unfriggenbelievable", "UNFRIGGENBELIEVABLE!" },
	{ "shotgun_spree", "SHOTGUN SPREE!" },
	{ "open_season", "OPEN SEASON!" },
	{ "sniper_spree", "SNIPER SPREE!" },
	{ "sharpshooter", "SHARPSHOOTER!" },
	{ "splatter_spree", "SPLATTER SPREE!" },
	{ "vehicular_manslaughter", "VEHICULAR MANSLAUGHTER!" },
	{ "stick_spree", "STICK SPREE!" },
	{ "beat_down", "BEAT DOWN!" },
	{ "assassin", "ASSASSIN!" },
	{ "headshot", "HEADSHOT!" },
	{ "sniper_kill", "SNIPER KILL!" },
	{ "grenade_stick", "GRENADE STICK!" },
	{ "splatter", "SPLATTER!" },
	{ "incineration", "INCINERATION!" },
	{ "death_from_the_grave", "DEATH FROM THE GRAVE!" },
	{ "close_call", "CLOSE CALL!" },
	{ "first_strike", "FIRST STRIKE!" },
	{ "revenge", "REVENGE!" },
	{ "avenger", "AVENGER!" },
	{ "wheelman", "WHEELMAN!" },
	{ "perfection", "PERFECTION!" },
};

static struct campaign_medals_player campaign_medals_players[CAMPAIGN_MEDALS_MAXIMUM_PLAYERS];
/* whether the level's first kill was made (First Strike) */
static boolean campaign_medals_first_kill;
/* each player's medals just won, the newest last (a ring) */
static struct campaign_medals_popup campaign_medals_popups[CAMPAIGN_MEDALS_MAXIMUM_PLAYERS][CAMPAIGN_MEDALS_POPUPS];
static short campaign_medals_popup_next[CAMPAIGN_MEDALS_MAXIMUM_PLAYERS];

/* ---------- private code */

static struct campaign_medals_player *campaign_medals_player(
	long player_index)
{
	short absolute_index;

	if (player_index == NONE || !player_try_and_get(player_index))
		return NULL;
	absolute_index = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	return absolute_index < CAMPAIGN_MEDALS_MAXIMUM_PLAYERS ? &campaign_medals_players[absolute_index] : NULL;
}

static void campaign_medals_reset_player(
	struct campaign_medals_player *state)
{
	csmemset(state, 0, sizeof(*state));
	state->last_kill_ticks = NONE;
	state->last_killer_object_index = NONE;
	state->last_killer_player_index = NONE;
	state->player_index = NONE;

	return;
}

/* the weapon the damage effect is of (NONE: none the medals tell apart) */
static short campaign_medals_weapon(
	long damage_definition_index)
{
	char const *name = damage_definition_index != NONE ? tag_get_name(damage_definition_index) : NULL;
	char lower[128];
	short weapon;
	long index;

	if (!name)
		return NONE;
	for (index = 0; index < (long)sizeof(lower) - 1 && name[index]; index++)
		lower[index] = (char)((name[index] >= 'A' && name[index] <= 'Z') ? name[index] + 32 : name[index]);
	lower[index] = '\0';
	for (weapon = 0; weapon < NUMBER_OF_CAMPAIGN_MEDALS_WEAPONS; weapon++)
	{
		char const *const *words = campaign_score_values.medal_weapon_words[weapon];

		for (; words && *words; words++)
		{
			if (strstr(lower, *words))
				return weapon;
		}
	}
	return NONE;
}

static void campaign_medals_give(
	long player_index,
	short medal)
{
	campaign_score_award_medal(player_index, medal);

	return;
}

/* a weapon's spree's kill: its two medals at their counts */
static void campaign_medals_weapon_spree(
	long player_index,
	struct campaign_medals_player *state,
	short weapon,
	short first_medal,
	short second_medal)
{
	long kills = ++state->weapon_sprees[weapon];

	if (kills == CAMPAIGN_MEDALS_WEAPON_SPREE_FIRST)
		campaign_medals_give(player_index, first_medal);
	else if (kills == CAMPAIGN_MEDALS_WEAPON_SPREE_SECOND && second_medal != NONE)
		campaign_medals_give(player_index, second_medal);

	return;
}

/* the player whose unit drives the vehicle the object rides (or is): NONE
for none, or the object itself driving */
static long campaign_medals_wheelman(
	long killer_object_index)
{
	struct object_datum *killer = killer_object_index != NONE ? object_try_and_get(killer_object_index) : NULL;
	long vehicle_index = NONE;
	struct unit_datum *vehicle;
	struct unit_datum *driver;

	if (!killer)
		return NONE;
	if (killer->object.type == _object_type_vehicle)
		vehicle_index = killer_object_index;
	else if (killer->object.parent_object_index != NONE)
		vehicle_index = killer->object.parent_object_index;
	vehicle = vehicle_index != NONE ?
		(struct unit_datum *)object_try_and_get_and_verify_type(vehicle_index, _object_mask_vehicle) : NULL;
	if (!vehicle || vehicle->unit.driver_object_index == NONE || vehicle->unit.driver_object_index == killer_object_index)
		return NONE;
	driver = (struct unit_datum *)object_try_and_get_and_verify_type(vehicle->unit.driver_object_index, _object_mask_unit);
	return driver ? driver->unit.player_index : NONE;
}

/* whether the player's last killer, killed by another (both datum indices),
was avenged: in a multiplayer game, a teammate's in a team game (in the
campaign every player is the others' ally) */
static boolean campaign_medals_avenged(
	long player_index,
	long avenger_player_index)
{
	struct player_datum *player = player_try_and_get(player_index);
	struct player_datum *avenger = player_try_and_get(avenger_player_index);

	if (!game_engine_running() || firefight_active())
		return TRUE;
	return player && avenger && player_index != avenger_player_index && game_engine_has_teams() &&
		player->team_index == avenger->team_index;
}

/* a kill by a player (NONE for none) or any unit (`killer_object_index`), of
an AI's unit (`victim_player_index` NONE) or a multiplayer game's player's */
static void campaign_medals_kill(
	long killer_player_index,
	long killer_object_index,
	long victim_unit_index,
	long victim_player_index,
	long damage_definition_index,
	boolean headshot,
	boolean melee,
	boolean backsmack);

/* ---------- public code */

boolean campaign_medals_enabled(
	void)
{
	return campaign_score_enabled() || campaign_score_multiplayer_medals();
}

boolean campaign_medals_popups_shown(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean shown = TRUE;

	if (firefight_active() || campaign_score_multiplayer_medals())
		return TRUE;
	if (changes != config_changes())
	{
		changes = config_changes();
		shown = config_boolean("game.campaign_medals") != 0;
	}
	return shown && campaign_score_enabled();
}

void campaign_medals_new_map(
	void)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	short index;

	for (index = 0; index < CAMPAIGN_MEDALS_MAXIMUM_PLAYERS; index++)
	{
		short popup;

		campaign_medals_reset_player(&campaign_medals_players[index]);
		for (popup = 0; popup < CAMPAIGN_MEDALS_POPUPS; popup++)
			campaign_medals_popups[index][popup].medal = NONE;
		campaign_medals_popup_next[index] = 0;
	}
	campaign_medals_first_kill = FALSE;

	return;
}

void campaign_medals_enemy_killed(
	long killer_player_index,
	long killer_object_index,
	long victim_unit_index,
	long damage_definition_index,
	boolean headshot,
	boolean melee,
	boolean backsmack)
{
	campaign_medals_kill(killer_player_index, killer_object_index, victim_unit_index, NONE, damage_definition_index,
		headshot, melee, backsmack);

	return;
}

void campaign_medals_player_killed_player(
	long killer_player_index,
	long killer_object_index,
	long victim_player_index,
	long damage_definition_index,
	boolean headshot,
	boolean melee,
	boolean backsmack)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	struct player_datum *victim = player_try_and_get(victim_player_index);

	if (!victim)
		return;
	campaign_medals_kill(killer_player_index, killer_object_index, victim->unit_index, victim_player_index,
		damage_definition_index, headshot, melee, backsmack);

	return;
}

static void campaign_medals_kill(
	long killer_player_index,
	long killer_object_index,
	long victim_unit_index,
	long victim_player_index,
	long damage_definition_index,
	boolean headshot,
	boolean melee,
	boolean backsmack)
{
	struct campaign_score_values const *values = &campaign_score_values;
	struct campaign_medals_player *state;
	struct player_datum *killer;
	long now = campaign_score_level_ticks();
	long wheelman;
	short weapon;
	short index;

	if (!campaign_medals_enabled())
		return;
	/* Wheelman: the vehicle's driver, a player, while another (a player, or
	an AI's gunner) kills from it */
	wheelman = campaign_medals_wheelman(killer_object_index);
	if (wheelman != NONE && wheelman != killer_player_index)
		campaign_medals_give(wheelman, _campaign_medal_wheelman);
	state = campaign_medals_player(killer_player_index);
	if (!state)
		return;
	killer = player_get(killer_player_index);

	if (!campaign_medals_first_kill)
	{
		campaign_medals_first_kill = TRUE;
		campaign_medals_give(killer_player_index, _campaign_medal_first_strike);
	}
	/* the multi-kill, within the window of the last kill */
	if (state->last_kill_ticks != NONE &&
		now - state->last_kill_ticks <= (long)(values->medal_multi_kill_seconds * TICKS_PER_SECOND))
	{
		state->multi_kill++;
	}
	else
	{
		state->multi_kill = 1;
	}
	state->last_kill_ticks = now;
	if (state->multi_kill >= 2)
	{
		campaign_medals_give(killer_player_index,
			(short)MIN(_campaign_medal_double_kill + state->multi_kill - 2, _campaign_medal_killionaire));
	}
	/* the spree, a medal every five kills */
	state->spree++;
	if (state->spree % CAMPAIGN_MEDALS_SPREE_STEP == 0 &&
		state->spree / CAMPAIGN_MEDALS_SPREE_STEP <= _campaign_medal_unfriggenbelievable - _campaign_medal_killing_spree + 1)
	{
		campaign_medals_give(killer_player_index,
			(short)(_campaign_medal_killing_spree + state->spree / CAMPAIGN_MEDALS_SPREE_STEP - 1));
	}
	/* the weapon's medal, and its spree */
	weapon = melee ? NONE : campaign_medals_weapon(damage_definition_index);
	switch (weapon)
	{
	case _campaign_medals_weapon_sniper:
		campaign_medals_give(killer_player_index, _campaign_medal_sniper_kill);
		campaign_medals_weapon_spree(killer_player_index, state, weapon, _campaign_medal_sniper_spree,
			_campaign_medal_sharpshooter);
		break;
	case _campaign_medals_weapon_shotgun:
		campaign_medals_weapon_spree(killer_player_index, state, weapon, _campaign_medal_shotgun_spree,
			_campaign_medal_open_season);
		break;
	case _campaign_medals_weapon_flame:
		campaign_medals_give(killer_player_index, _campaign_medal_incineration);
		break;
	case _campaign_medals_weapon_grenade_stick:
		campaign_medals_give(killer_player_index, _campaign_medal_grenade_stick);
		campaign_medals_weapon_spree(killer_player_index, state, weapon, _campaign_medal_stick_spree, NONE);
		break;
	case _campaign_medals_weapon_splatter:
		campaign_medals_give(killer_player_index, _campaign_medal_splatter);
		campaign_medals_weapon_spree(killer_player_index, state, weapon, _campaign_medal_splatter_spree,
			_campaign_medal_vehicular_manslaughter);
		break;
	default:
		break;
	}
	/* the kill's style */
	if (backsmack)
		campaign_medals_give(killer_player_index, _campaign_medal_assassin);
	else if (melee)
		campaign_medals_give(killer_player_index, _campaign_medal_beat_down);
	if (headshot)
		campaign_medals_give(killer_player_index, _campaign_medal_headshot);
	/* the killer's own state: dead (a grenade, a rocket on its way), or
	nearly (its shield down, its health low) */
	{
		struct unit_datum *unit = killer->unit_index != NONE ?
			(struct unit_datum *)object_try_and_get_and_verify_type(killer->unit_index, _object_mask_unit) : NULL;

		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			campaign_medals_give(killer_player_index, _campaign_medal_death_from_the_grave);
		else if (unit->object.shield_vitality <= 0.0f && unit->object.body_vitality > 0.0f &&
			unit->object.body_vitality <= values->medal_close_call_health)
		{
			campaign_medals_give(killer_player_index, _campaign_medal_close_call);
		}
	}
	/* Revenge, the player's last killer; Avenger, another player's (an AI's
unit, or a multiplayer game's player) */
	if (victim_player_index != NONE ? state->last_killer_player_index == victim_player_index :
		state->last_killer_object_index == victim_unit_index)
	{
		state->last_killer_object_index = NONE;
		state->last_killer_player_index = NONE;
		campaign_medals_give(killer_player_index, _campaign_medal_revenge);
	}
	for (index = 0; index < CAMPAIGN_MEDALS_MAXIMUM_PLAYERS; index++)
	{
		struct campaign_medals_player *other = &campaign_medals_players[index];

		if (other == state)
			continue;
		if (victim_player_index != NONE ? other->last_killer_player_index != victim_player_index :
			other->last_killer_object_index != victim_unit_index)
		{
			continue;
		}
		if (victim_player_index != NONE && !campaign_medals_avenged(other->player_index, killer_player_index))
			continue;
		other->last_killer_object_index = NONE;
		other->last_killer_player_index = NONE;
		campaign_medals_give(killer_player_index, _campaign_medal_avenger);
		break;
	}

	return;
}

void campaign_medals_player_died(
	long player_index,
	long killer_player_index,
	long killer_object_index)
{
	struct campaign_medals_player *state = campaign_medals_player(player_index);
	short weapon;

	if (!state)
		return;
	state->spree = 0;
	for (weapon = 0; weapon < NUMBER_OF_CAMPAIGN_MEDALS_WEAPONS; weapon++)
		state->weapon_sprees[weapon] = 0;
	state->multi_kill = 0;
	state->last_kill_ticks = NONE;
	state->player_index = player_index;
	/* (a multiplayer game's other player, killing them) */
	state->last_killer_player_index = killer_player_index != player_index ? killer_player_index : NONE;
	/* (another unit: not the player's own, nor its vehicle's fall) */
	if (killer_object_index != NONE && object_try_and_get_and_verify_type(killer_object_index, _object_mask_unit))
	{
		struct unit_datum *killer = (struct unit_datum *)object_get(killer_object_index);

		state->last_killer_object_index = killer->unit.player_index == NONE ? killer_object_index : NONE;
	}

	return;
}

void campaign_medals_level_won(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!campaign_medals_enabled())
		return;
	/* Perfection: won without dying, with enough kills */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (!player->quit_out_of_game && campaign_score_player_deaths(iterator.datum_index) == 0 &&
			campaign_score_player_kills(iterator.datum_index) >= campaign_score_values.medal_perfection_kills)
		{
			campaign_medals_give(iterator.datum_index, _campaign_medal_perfection);
		}
	}

	return;
}

void campaign_medals_awarded(
	short player_index,
	short medal)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	struct campaign_medals_popup *popup;

	if (player_index < 0 || player_index >= CAMPAIGN_MEDALS_MAXIMUM_PLAYERS || medal < 0 ||
		medal >= NUMBER_OF_CAMPAIGN_MEDALS)
	{
		return;
	}
	popup = &campaign_medals_popups[player_index][campaign_medals_popup_next[player_index]];
	popup->medal = medal;
	popup->start_milliseconds = system_milliseconds();
	campaign_medals_popup_next[player_index] = (short)((campaign_medals_popup_next[player_index] + 1) %
		CAMPAIGN_MEDALS_POPUPS);

	return;
}

long campaign_medals_bitmap(
	void)
{
	return tag_loaded(BITMAP_GROUP_TAG, CAMPAIGN_MEDALS_BITMAP);
}

void campaign_medals_draw_popups(
	long player_index)
{
	short absolute_index;
	long bitmap_index;
	unsigned long now = system_milliseconds();
	unsigned long lasting = (unsigned long)(campaign_score_values.medal_popup_seconds * 1000.0f);
	real scale;
	short width;
	short height;
	short shown = 0;
	short newest = NONE;
	short index;

	if (player_index == NONE || !campaign_medals_popups_shown())
		return;
	absolute_index = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	if (absolute_index >= CAMPAIGN_MEDALS_MAXIMUM_PLAYERS)
		return;
	bitmap_index = campaign_medals_bitmap();
	width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	scale = (real)height / 480.0f;
	(void)width;
	/* (the newest first: from the ring's last back) */
	for (index = 0; index < CAMPAIGN_MEDALS_POPUPS; index++)
	{
		short slot = (short)((campaign_medals_popup_next[absolute_index] - 1 - index + 2 * CAMPAIGN_MEDALS_POPUPS) %
			CAMPAIGN_MEDALS_POPUPS);
		struct campaign_medals_popup const *popup = &campaign_medals_popups[absolute_index][slot];
		real age;
		real alpha;
		real size;
		short column;
		short row;
		real middle_x;
		real middle_y;
		rectangle2d bounds;
		struct bitmap_data *bitmap;

		if (popup->medal == NONE || now - popup->start_milliseconds >= lasting)
			continue;
		if (newest == NONE)
			newest = popup->medal;
		age = (real)(now - popup->start_milliseconds) / 1000.0f;
		alpha = CAMPAIGN_MEDALS_POPUP_OPACITY *
			PIN((campaign_score_values.medal_popup_seconds - age) / CAMPAIGN_MEDALS_FADE_SECONDS, 0.0f, 1.0f);
		size = CAMPAIGN_MEDALS_POPUP_SIZE;
		if (age < CAMPAIGN_MEDALS_POP_SECONDS)
			size *= CAMPAIGN_MEDALS_POP_SCALE - (CAMPAIGN_MEDALS_POP_SCALE - 1.0f) * age / CAMPAIGN_MEDALS_POP_SECONDS;
		column = (short)(shown % CAMPAIGN_MEDALS_POPUPS_PER_ROW);
		row = (short)(shown / CAMPAIGN_MEDALS_POPUPS_PER_ROW);
		shown++;
		middle_x = (CAMPAIGN_MEDALS_POPUP_LEFT + CAMPAIGN_MEDALS_POPUP_SIZE / 2.0f +
			column * (CAMPAIGN_MEDALS_POPUP_SIZE + CAMPAIGN_MEDALS_POPUP_GAP)) * scale;
		middle_y = (CAMPAIGN_MEDALS_POPUP_MIDDLE - (CAMPAIGN_MEDALS_POPUP_SIZE + CAMPAIGN_MEDALS_POPUP_GAP) / 2.0f +
			row * (CAMPAIGN_MEDALS_POPUP_SIZE + CAMPAIGN_MEDALS_POPUP_GAP)) * scale;
		bounds.x0 = (short)(middle_x - size * scale / 2.0f);
		bounds.x1 = (short)(middle_x + size * scale / 2.0f);
		bounds.y0 = (short)(middle_y - size * scale / 2.0f);
		bounds.y1 = (short)(middle_y + size * scale / 2.0f);
		bitmap = bitmap_index != NONE ? bitmap_group_try_and_get_bitmap(bitmap_index, popup->medal) : NULL;
		if (bitmap)
			draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL, ((pixel32)(alpha * 255.0f + 0.5f) << 24) | 0x00FFFFFF, NULL, TRUE);
		if (shown == CAMPAIGN_MEDALS_POPUPS)
			break;
	}
	/* (the newest's name, beneath them; its name alone without the icons) */
	if (newest != NONE)
	{
		long font_index = hud_get_font_index();

		if (font_index != NONE)
		{
			short rows = (short)((shown + CAMPAIGN_MEDALS_POPUPS_PER_ROW - 1) / CAMPAIGN_MEDALS_POPUPS_PER_ROW);
			real_argb_color color;
			rectangle2d bounds;

			color.alpha = CAMPAIGN_MEDALS_POPUP_OPACITY;
			color.red = 1.0f;
			color.green = 0.92f;
			color.blue = 0.55f;
			bounds.x0 = (short)(CAMPAIGN_MEDALS_POPUP_LEFT * scale);
			bounds.x1 = (short)(width - bounds.x0);
			bounds.y0 = (short)((CAMPAIGN_MEDALS_POPUP_MIDDLE - (CAMPAIGN_MEDALS_POPUP_SIZE + CAMPAIGN_MEDALS_POPUP_GAP) +
				rows * (CAMPAIGN_MEDALS_POPUP_SIZE + CAMPAIGN_MEDALS_POPUP_GAP)) * scale);
			bounds.y1 = (short)(bounds.y0 + 32);
			/* (half the HUD's text, scaled about its top left) */
			rasterizer_text_set_scale(CAMPAIGN_MEDALS_NAME_SIZE, (real)bounds.x0, (real)bounds.y0);
			draw_string_set_draw_mode(font_index, NONE, 0, 0, &color);
			rasterizer_draw_string(&bounds, NULL, NULL, 0, campaign_medal_descriptions[newest].label);
			rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
		}
	}

	return;
}
