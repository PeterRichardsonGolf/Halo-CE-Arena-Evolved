/*
AE_PGCR_RULES.H

The post-game report's numbers (ae_pgcr_stats.c counts them on the host, ae_pgcr_data.h keeps them for the screen), as pure
code: no engine headers (port/linux/tests/ae_pgcr_rules_test.c runs them). Players are slots (the absolute player index).

- Accuracy counts rounds: a trigger pull is one shot, a shotgun shell (all its pellets) is one, a burst is its rounds. A hit
  is a shot that landed on another player with a gun (bullet, sniper, shotgun, needle, plasma, flame, an explosive round):
  one hit for every tick a player's rounds land, so a shell's pellets are one hit; never more hits than shots.
- A kill of an enemy counts for the killer: the pair table (who killed whom), headshots, the kills of the weapon (the
  killing damage's tag), the spree (kills without dying) and the multikills (a kill within 4 seconds of the one before).
  A betrayal or a suicide counts for no one. Any death ends the dead player's spree and chain.
- A summary is what one player's report line needs from the host: medals (id and count, most first), their three
  deadliest weapons, best spree, multikills, headshot kills, the player they killed most and the one who killed them most.
*/

#ifndef __AE_PGCR_RULES_H
#define __AE_PGCR_RULES_H

#include "ae_medal_rules.h"

enum
{
	AE_PGCR_PLAYERS = 128,
	/* the weapons (damage tags) a player's kills are counted by */
	AE_PGCR_TRACKED_WEAPONS = 12,
	/* a summary's lists */
	AE_PGCR_SUMMARY_WEAPONS = 3,
	AE_PGCR_SUMMARY_MEDALS = 32,
	/* the players a summary counts kills against and by: those it most traded kills with */
	AE_PGCR_SUMMARY_VERSUS = 8,
	/* a multikill's window, in ticks */
	AE_PGCR_MULTIKILL_TICKS = 120,
	/* the summary's wire form: version, then the fields (ae_pgcr_encode) */
	AE_PGCR_WIRE_VERSION = 2,
	AE_PGCR_WIRE_MAXIMUM = 13 + AE_PGCR_SUMMARY_WEAPONS * 6 + 1 + AE_PGCR_SUMMARY_MEDALS * 2 + 1 + AE_PGCR_SUMMARY_VERSUS * 3,
	/* no player (killed most / by) */
	AE_PGCR_NO_PLAYER = 255,
};

struct ae_pgcr_weapon
{
	/* the killing damage's tag (a datum index), NONE for none */
	long tag;
	unsigned short kills;
};

struct ae_pgcr_medal
{
	unsigned char id;
	unsigned char count;
};

/* the kills between the summary's player and another: theirs of that player, and that player's of them */
struct ae_pgcr_versus
{
	unsigned char slot;
	unsigned char killed;
	unsigned char killed_by;
};

/* one player's report line from the host */
struct ae_pgcr_summary
{
	int valid;
	/* the slot it is for */
	int slot;
	unsigned short best_spree;
	unsigned short multikills;
	unsigned short headshots;
	/* the rivals: a slot or AE_PGCR_NO_PLAYER, and the kills */
	unsigned char killed_most;
	unsigned char killed_most_count;
	unsigned char killed_by_most;
	unsigned char killed_by_most_count;
	int weapon_count;
	struct ae_pgcr_weapon weapons[AE_PGCR_SUMMARY_WEAPONS];
	int medal_count;
	struct ae_pgcr_medal medals[AE_PGCR_SUMMARY_MEDALS];
	/* (wire version 2) the players they traded the most kills with, most first; a version 1 summary has none */
	int versus_count;
	struct ae_pgcr_versus versus[AE_PGCR_SUMMARY_VERSUS];
};

/* the host's counts for the game being played */
struct ae_pgcr_game
{
	unsigned char kills[AE_PGCR_PLAYERS][AE_PGCR_PLAYERS];
	unsigned char medals[AE_PGCR_PLAYERS][AE_MEDAL_COUNT];
	unsigned short headshots[AE_PGCR_PLAYERS];
	unsigned short spree[AE_PGCR_PLAYERS];
	unsigned short best_spree[AE_PGCR_PLAYERS];
	unsigned short chain[AE_PGCR_PLAYERS];
	long last_kill_tick[AE_PGCR_PLAYERS];
	unsigned short multikills[AE_PGCR_PLAYERS];
	int weapon_count[AE_PGCR_PLAYERS];
	struct ae_pgcr_weapon weapons[AE_PGCR_PLAYERS][AE_PGCR_TRACKED_WEAPONS];
	long shots_fired[AE_PGCR_PLAYERS];
	long shots_hit[AE_PGCR_PLAYERS];
	long last_hit_tick[AE_PGCR_PLAYERS];
};

void ae_pgcr_reset(struct ae_pgcr_game *game);

/* a round fired by the slot: the shots it has fired now (0 for a slot out of range) */
long ae_pgcr_shot(struct ae_pgcr_game *game, int slot);
/* whether a damage of this category (ae_medal_rules.h's) is a gun's: one that can be a hit */
int ae_pgcr_category_is_gun(int category);
/* damage from the attacker's gun on the victim at the tick: 1 when it is a new hit (a shot of theirs that landed), 0 when
it is not a hit (no attacker, themselves, a grenade, a blow, a vehicle, the same tick as the last hit, or no shots left) */
int ae_pgcr_hit(struct ae_pgcr_game *game, int attacker, int victim, int category, long tick);
/* a medal the slot earned */
void ae_pgcr_medal(struct ae_pgcr_game *game, int slot, int medal);
/* a player died (any death): their spree and chain end */
void ae_pgcr_death(struct ae_pgcr_game *game, int victim);
/* the killer killed the victim (the kill the rules count: not a betrayal, not themselves), a headshot or not, with this
damage's tag (NONE if unknown), at the tick */
void ae_pgcr_kill(struct ae_pgcr_game *game, int killer, int victim, int headshot, long tag, long tick);

/* the slot's summary */
void ae_pgcr_summarize(struct ae_pgcr_game const *game, int slot, struct ae_pgcr_summary *summary);

/* the summary as bytes (little-endian), at most AE_PGCR_WIRE_MAXIMUM: the length */
int ae_pgcr_encode(struct ae_pgcr_summary const *summary, unsigned char *out, int size);
/* bytes into a summary: 1 when they are a whole, sound one of this version (counts within their limits, medals known, the
rivals a slot or none), else 0 and the summary invalid */
int ae_pgcr_decode(unsigned char const *data, int length, struct ae_pgcr_summary *summary);

#endif
