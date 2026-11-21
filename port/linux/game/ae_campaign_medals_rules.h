/*
AE_CAMPAIGN_MEDALS_RULES.H

The campaign's medals (its score's, ae_campaign_score.c): which a kill, a spree or a won level earns. Pure: the engine's facts
come in as numbers (ae_campaign_medals_kill's input), and the medals won come out as (player slot, medal) pairs.

The medals follow the campaign's own list (ae_campaign_medal_descriptions: its key, its label and the number of the medal of
ae_medal_ids.h it is, -1 for those AE's table has no match for). The points of a medal are campaign_score_values.json's
"medals" (the campaign's score; the multiplayer medals' points are ae_medal_ids.h's own, and stay there). Derived from
AGPL-3.0-or-later code of the campaign score this file's rules come from; see the README's credits.
*/

#ifndef __AE_CAMPAIGN_MEDALS_RULES_H
#define __AE_CAMPAIGN_MEDALS_RULES_H

struct campaign_score_values;

/* the campaign's medals, in ae_campaign_medal_descriptions' order */
enum
{
	/* (kills within the multi-kill window of each other) */
	AE_CAMPAIGN_MEDAL_DOUBLE_KILL = 0,
	AE_CAMPAIGN_MEDAL_TRIPLE_KILL,
	AE_CAMPAIGN_MEDAL_OVERKILL,
	AE_CAMPAIGN_MEDAL_KILLTACULAR,
	AE_CAMPAIGN_MEDAL_KILLTROCITY,
	AE_CAMPAIGN_MEDAL_KILLIMANJARO,
	AE_CAMPAIGN_MEDAL_KILLTASTROPHE,
	AE_CAMPAIGN_MEDAL_KILLPOCALYPSE,
	AE_CAMPAIGN_MEDAL_KILLIONAIRE,
	/* (kills without dying: 5, 10, ... 40) */
	AE_CAMPAIGN_MEDAL_KILLING_SPREE,
	AE_CAMPAIGN_MEDAL_KILLING_FRENZY,
	AE_CAMPAIGN_MEDAL_RUNNING_RIOT,
	AE_CAMPAIGN_MEDAL_RAMPAGE,
	AE_CAMPAIGN_MEDAL_UNTOUCHABLE,
	AE_CAMPAIGN_MEDAL_INVINCIBLE,
	AE_CAMPAIGN_MEDAL_INCONCEIVABLE,
	AE_CAMPAIGN_MEDAL_UNFRIGGENBELIEVABLE,
	/* (kills of a weapon's without dying: 5 and 10) */
	AE_CAMPAIGN_MEDAL_SHOTGUN_SPREE,
	AE_CAMPAIGN_MEDAL_OPEN_SEASON,
	AE_CAMPAIGN_MEDAL_SNIPER_SPREE,
	AE_CAMPAIGN_MEDAL_SHARPSHOOTER,
	AE_CAMPAIGN_MEDAL_SPLATTER_SPREE,
	AE_CAMPAIGN_MEDAL_VEHICULAR_MANSLAUGHTER,
	AE_CAMPAIGN_MEDAL_STICK_SPREE,
	/* (a kill's) */
	AE_CAMPAIGN_MEDAL_BEAT_DOWN,
	AE_CAMPAIGN_MEDAL_ASSASSIN,
	AE_CAMPAIGN_MEDAL_HEADSHOT,
	AE_CAMPAIGN_MEDAL_SNIPER_KILL,
	AE_CAMPAIGN_MEDAL_GRENADE_STICK,
	AE_CAMPAIGN_MEDAL_SPLATTER,
	AE_CAMPAIGN_MEDAL_INCINERATION,
	AE_CAMPAIGN_MEDAL_DEATH_FROM_THE_GRAVE,
	AE_CAMPAIGN_MEDAL_CLOSE_CALL,
	AE_CAMPAIGN_MEDAL_FIRST_STRIKE,
	AE_CAMPAIGN_MEDAL_REVENGE,
	AE_CAMPAIGN_MEDAL_AVENGER,
	AE_CAMPAIGN_MEDAL_WHEELMAN,
	/* (the level's) */
	AE_CAMPAIGN_MEDAL_PERFECTION,
	AE_CAMPAIGN_MEDAL_COUNT
};

struct ae_campaign_medal_description
{
	char const *key;
	char const *label;
	/* the number of ae_medal_ids.h's medal of the same name (AE_MEDAL_*), -1 for none */
	int ae_medal;
};

extern struct ae_campaign_medal_description const ae_campaign_medal_descriptions[AE_CAMPAIGN_MEDAL_COUNT];

enum
{
	/* the players a medal state is kept for (the slots of a game's players) */
	AE_CAMPAIGN_MEDALS_PLAYERS = 16,
	/* the most medals one kill can earn (a multi-kill, a spree, a weapon, a style, a state, Revenge, Avenger, Wheelman) */
	AE_CAMPAIGN_MEDALS_MOST_PER_KILL = 16,
	/* the weapons the medals tell apart: sniper, shotgun, flame, grenade stick, splatter */
	AE_CAMPAIGN_MEDALS_WEAPONS = 5
};

struct ae_campaign_medals_player
{
	/* the kills without dying, all and each weapon's */
	long spree;
	long weapon_sprees[AE_CAMPAIGN_MEDALS_WEAPONS];
	/* the multi-kill: its kills, and the last's time (the level's ticks; -1: none) */
	long multi_kill;
	long last_kill_ticks;
	/* the unit that killed the player last (-1: none), for Revenge and Avenger */
	long last_killer_object;
};

struct ae_campaign_medals_state
{
	struct ae_campaign_medals_player players[AE_CAMPAIGN_MEDALS_PLAYERS];
	/* whether the level's first kill was made (First Strike) */
	int first_kill;
};

struct ae_campaign_medal_award
{
	int slot;
	int medal;
};

/* everything of one kill the medals tell from */
struct ae_campaign_kill
{
	/* the killing player's slot (0 up), and the unit it killed */
	int killer_slot;
	long victim_unit;
	/* the unit that did it (its player's own, or an AI gunner's: -1 none) */
	long killer_object;
	/* the level's ticks now, and the weapon of the killing blow (-1: none the medals tell apart; 0 up, the weapons above) */
	long ticks;
	int weapon;
	int headshot;
	int melee;
	int backsmack;
	/* the slot of the player driving the vehicle the killer rides (-1: none, or the killer drives) */
	int wheelman_slot;
	/* the killer's unit: gone or dead (Death from the Grave), or its shield down with health left and at most the
	values' Close Call health (Close Call) */
	int killer_dead;
	int killer_nearly_dead;
};

void ae_campaign_medals_reset(struct ae_campaign_medals_state *state);

/* the weapon (0 up; -1: none) whose words the damage effect's name (any case) has, by the values' words; a NULL name is none */
int ae_campaign_medals_weapon(struct campaign_score_values const *values, char const *damage_name);

/* a kill: the medals it earns, up to `most` pairs into out, the number given. A slot out of range earns nothing */
int ae_campaign_medals_kill(struct ae_campaign_medals_state *state, struct campaign_score_values const *values,
	struct ae_campaign_kill const *kill, struct ae_campaign_medal_award *out, int most);

/* a player died (slot), killed by that unit (-1: none): the sprees end, the killer is remembered */
void ae_campaign_medals_player_died(struct ae_campaign_medals_state *state, int slot, long killer_object);

/* the level won: Perfection for a player who did not die and killed at least the values' Perfection kills */
int ae_campaign_medals_perfection(struct campaign_score_values const *values, int deaths, int kills);

#endif
