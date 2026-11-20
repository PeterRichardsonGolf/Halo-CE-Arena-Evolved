/*
AE_CAMPAIGN_MEDALS_RULES.C

The campaign's medals: see ae_campaign_medals_rules.h. Derived from AGPL-3.0-or-later code (README credits).

- multi-kills: Double Kill to Killionaire, kills within the multi-kill window of the one before;
- sprees, kills without dying: Killing Spree (5) to Unfriggenbelievable (40); and of a weapon's kills, 5 and 10: Shotgun Spree and
  Open Season, Sniper Spree and Sharpshooter, Splatter Spree and Vehicular Manslaughter, and Stick Spree (5 plasma grenades stuck);
- a kill's: Beat Down (melee), Assassin (melee from behind), Headshot, Sniper Kill, Grenade Stick, Splatter, Incineration (the
  flamethrower), Death from the Grave (the killer dead), Close Call, First Strike (the level's first kill), Revenge (the player's
  last killer killed), Avenger (another player's) and Wheelman (a passenger, an AI's gunner too, killing while the player drives);
- the level's: Perfection, won without dying, with enough kills.
*/

#include <string.h>

#include "ae_medals.h"
#include "ae_campaign_medals_rules.h"
#include "campaign_score_values.h"

/* a kill spree's medal: one every this many kills */
#define SPREE_STEP 5
/* a weapon's spree's two medals' kills */
#define WEAPON_SPREE_FIRST 5
#define WEAPON_SPREE_SECOND 10

enum { WEAPON_SNIPER = 0, WEAPON_SHOTGUN, WEAPON_FLAME, WEAPON_GRENADE_STICK, WEAPON_SPLATTER };

struct ae_campaign_medal_description const ae_campaign_medal_descriptions[AE_CAMPAIGN_MEDAL_COUNT] =
{
	{ "double_kill", "DOUBLE KILL!", AE_MEDAL_DOUBLE_KILL },
	{ "triple_kill", "TRIPLE KILL!", AE_MEDAL_TRIPLE_OF_EACH_KILL },
	{ "overkill", "OVERKILL!", AE_MEDAL_OVERKILL },
	{ "killtacular", "KILLTACULAR!", AE_MEDAL_KILLTACULAR },
	{ "killtrocity", "KILLTROCITY!", AE_MEDAL_KILLTROCITY },
	{ "killimanjaro", "KILLIMANJARO!", AE_MEDAL_KILLIMANJARO },
	{ "killtastrophe", "KILLTASTROPHE!", AE_MEDAL_KILLTASTROPHE },
	{ "killpocalypse", "KILLPOCALYPSE!", AE_MEDAL_KILLPOCALYPSE },
	{ "killionaire", "KILLIONAIRE!", AE_MEDAL_KILLIONAIRE },
	{ "killing_spree", "KILLING SPREE!", AE_MEDAL_KILLING_SPREE },
	{ "killing_frenzy", "KILLING FRENZY!", AE_MEDAL_KILLING_FRENZY },
	{ "running_riot", "RUNNING RIOT!", AE_MEDAL_RUNNING_RIOT },
	{ "rampage", "RAMPAGE!", AE_MEDAL_RAMPAGE },
	{ "untouchable", "UNTOUCHABLE!", AE_MEDAL_UNTOUCHABLE_ROW_WIT_UNTOUCHABLE },
	{ "invincible", "INVINCIBLE!", AE_MEDAL_INVINCIBLE },
	{ "inconceivable", "INCONCEIVABLE!", AE_MEDAL_INCONCEIVABLE },
	{ "unfriggenbelievable", "UNFRIGGENBELIEVABLE!", AE_MEDAL_UNFRIGGENBELIEVABLE_ROW_WIT_UNFRIGGENBELIEVABLE },
	{ "shotgun_spree", "SHOTGUN SPREE!", AE_MEDAL_SHOTGUN_SHOTGUN_SPREE },
	{ "open_season", "OPEN SEASON!", AE_MEDAL_OPEN_SEASON },
	{ "sniper_spree", "SNIPER SPREE!", AE_MEDAL_SNIPER_RIFLE_W_SPREE },
	{ "sharpshooter", "SHARPSHOOTER!", AE_MEDAL_SHARPSHOOTER_RIFLE_W_SHARPSHOOTER },
	{ "splatter_spree", "SPLATTER SPREE!", AE_MEDAL_SPLATTER_ROW_WIT_SPREE },
	{ "vehicular_manslaughter", "VEHICULAR MANSLAUGHTER!", AE_MEDAL_VEHICLE_ROW_WIT_MANSLAUGHTER },
	{ "stick_spree", "STICK SPREE!", -1 },
	{ "beat_down", "BEAT DOWN!", AE_MEDAL_BEAT_DOWN },
	{ "assassin", "ASSASSIN!", -1 },
	{ "headshot", "HEADSHOT!", AE_MEDAL_HEADSHOT },
	{ "sniper_kill", "SNIPER KILL!", AE_MEDAL_SNIPER_KILL },
	{ "grenade_stick", "GRENADE STICK!", AE_MEDAL_GRENADE_STICK },
	{ "splatter", "SPLATTER!", AE_MEDAL_SPLATTER_WITH_A__SPLATTER },
	{ "incineration", "INCINERATION!", -1 },
	{ "death_from_the_grave", "DEATH FROM THE GRAVE!", AE_MEDAL_KILL_FROM_THE_GRAVE },
	{ "close_call", "CLOSE CALL!", AE_MEDAL_CLOSE_CALL },
	{ "first_strike", "FIRST STRIKE!", AE_MEDAL_FIRST_STRIKE },
	{ "revenge", "REVENGE!", AE_MEDAL_REVENGE },
	{ "avenger", "AVENGER!", AE_MEDAL_AVENGER },
	{ "wheelman", "WHEELMAN!", AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN },
	{ "perfection", "PERFECTION!", AE_MEDAL_PERFECTION },
};

void ae_campaign_medals_reset(struct ae_campaign_medals_state *state)
{
	int slot;

	memset(state, 0, sizeof(*state));
	for (slot = 0; slot < AE_CAMPAIGN_MEDALS_PLAYERS; slot++)
	{
		state->players[slot].last_kill_ticks = -1;
		state->players[slot].last_killer_object = -1;
	}
}

int ae_campaign_medals_weapon(struct campaign_score_values const *values, char const *damage_name)
{
	char lower[128];
	size_t index;
	int weapon;

	if (!values || !damage_name)
		return -1;
	for (index = 0; index < sizeof(lower) - 1 && damage_name[index]; index++)
		lower[index] = (char)((damage_name[index] >= 'A' && damage_name[index] <= 'Z') ? damage_name[index] + 32 : damage_name[index]);
	lower[index] = 0;
	for (weapon = 0; weapon < AE_CAMPAIGN_MEDALS_WEAPONS; weapon++)
	{
		char const *const *words = values->medal_weapon_words[weapon];

		for (; words && *words; words++)
		{
			if (strstr(lower, *words))
				return weapon;
		}
	}
	return -1;
}

static void give(struct ae_campaign_medal_award *out, int most, int *count, int slot, int medal)
{
	if (*count < most)
	{
		out[*count].slot = slot;
		out[*count].medal = medal;
		(*count)++;
	}
}

static void weapon_spree(struct ae_campaign_medals_player *state, int weapon, int first, int second,
	struct ae_campaign_medal_award *out, int most, int *count, int slot)
{
	long kills = ++state->weapon_sprees[weapon];

	if (kills == WEAPON_SPREE_FIRST)
		give(out, most, count, slot, first);
	else if (kills == WEAPON_SPREE_SECOND && second >= 0)
		give(out, most, count, slot, second);
}

int ae_campaign_medals_kill(struct ae_campaign_medals_state *state, struct campaign_score_values const *values,
	struct ae_campaign_kill const *kill, struct ae_campaign_medal_award *out, int most)
{
	struct ae_campaign_medals_player *player;
	int count = 0, other, slot = kill->killer_slot;
	long window;

	if (slot < 0 || slot >= AE_CAMPAIGN_MEDALS_PLAYERS)
		return 0;
	player = &state->players[slot];
	/* Wheelman: the vehicle's driver, a player, while another (a player, or an AI's gunner) kills from it */
	if (kill->wheelman_slot >= 0 && kill->wheelman_slot < AE_CAMPAIGN_MEDALS_PLAYERS && kill->wheelman_slot != slot)
		give(out, most, &count, kill->wheelman_slot, AE_CAMPAIGN_MEDAL_WHEELMAN);
	if (!state->first_kill)
	{
		state->first_kill = 1;
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_FIRST_STRIKE);
	}
	/* the multi-kill, within the window of the last kill */
	window = (long)(values->medal_multi_kill_seconds * 30.0f);
	if (player->last_kill_ticks >= 0 && kill->ticks - player->last_kill_ticks <= window)
		player->multi_kill++;
	else
		player->multi_kill = 1;
	player->last_kill_ticks = kill->ticks;
	if (player->multi_kill >= 2)
	{
		long step = player->multi_kill - 2;

		give(out, most, &count, slot,
			(int)(AE_CAMPAIGN_MEDAL_DOUBLE_KILL + (step > AE_CAMPAIGN_MEDAL_KILLIONAIRE ? AE_CAMPAIGN_MEDAL_KILLIONAIRE : step)));
	}
	/* the spree, a medal every five kills */
	player->spree++;
	if (player->spree % SPREE_STEP == 0 &&
		player->spree / SPREE_STEP <= AE_CAMPAIGN_MEDAL_UNFRIGGENBELIEVABLE - AE_CAMPAIGN_MEDAL_KILLING_SPREE + 1)
		give(out, most, &count, slot, (int)(AE_CAMPAIGN_MEDAL_KILLING_SPREE + player->spree / SPREE_STEP - 1));
	/* the weapon's medal, and its spree (a melee blow has none) */
	switch (kill->melee ? -1 : kill->weapon)
	{
	case WEAPON_SNIPER:
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_SNIPER_KILL);
		weapon_spree(player, WEAPON_SNIPER, AE_CAMPAIGN_MEDAL_SNIPER_SPREE, AE_CAMPAIGN_MEDAL_SHARPSHOOTER, out, most, &count, slot);
		break;
	case WEAPON_SHOTGUN:
		weapon_spree(player, WEAPON_SHOTGUN, AE_CAMPAIGN_MEDAL_SHOTGUN_SPREE, AE_CAMPAIGN_MEDAL_OPEN_SEASON, out, most, &count, slot);
		break;
	case WEAPON_FLAME:
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_INCINERATION);
		break;
	case WEAPON_GRENADE_STICK:
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_GRENADE_STICK);
		weapon_spree(player, WEAPON_GRENADE_STICK, AE_CAMPAIGN_MEDAL_STICK_SPREE, -1, out, most, &count, slot);
		break;
	case WEAPON_SPLATTER:
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_SPLATTER);
		weapon_spree(player, WEAPON_SPLATTER, AE_CAMPAIGN_MEDAL_SPLATTER_SPREE, AE_CAMPAIGN_MEDAL_VEHICULAR_MANSLAUGHTER, out,
			most, &count, slot);
		break;
	default:
		break;
	}
	/* the kill's style */
	if (kill->backsmack)
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_ASSASSIN);
	else if (kill->melee)
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_BEAT_DOWN);
	if (kill->headshot)
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_HEADSHOT);
	/* the killer's own state: dead, or nearly */
	if (kill->killer_dead)
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_DEATH_FROM_THE_GRAVE);
	else if (kill->killer_nearly_dead)
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_CLOSE_CALL);
	/* Revenge: the unit that killed this player last, killed */
	if (player->last_killer_object >= 0 && player->last_killer_object == kill->victim_unit)
	{
		player->last_killer_object = -1;
		give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_REVENGE);
	}
	/* Avenger: the unit that killed another player last, killed (in the campaign every player is the others' ally) */
	for (other = 0; other < AE_CAMPAIGN_MEDALS_PLAYERS; other++)
	{
		if (other != slot && state->players[other].last_killer_object >= 0 &&
			state->players[other].last_killer_object == kill->victim_unit)
		{
			state->players[other].last_killer_object = -1;
			give(out, most, &count, slot, AE_CAMPAIGN_MEDAL_AVENGER);
			break;
		}
	}
	return count;
}

void ae_campaign_medals_player_died(struct ae_campaign_medals_state *state, int slot, long killer_object)
{
	struct ae_campaign_medals_player *player;
	int weapon;

	if (slot < 0 || slot >= AE_CAMPAIGN_MEDALS_PLAYERS)
		return;
	player = &state->players[slot];
	player->spree = 0;
	for (weapon = 0; weapon < AE_CAMPAIGN_MEDALS_WEAPONS; weapon++)
		player->weapon_sprees[weapon] = 0;
	player->multi_kill = 0;
	player->last_kill_ticks = -1;
	player->last_killer_object = killer_object;
}

int ae_campaign_medals_perfection(struct campaign_score_values const *values, int deaths, int kills)
{
	return deaths == 0 && kills >= values->medal_perfection_kills;
}
