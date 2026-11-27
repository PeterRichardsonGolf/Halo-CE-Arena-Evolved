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
- A summary is what one player's report line needs from the host: medals (id and count, most first), their six
  deadliest weapons, best spree, multikills, headshot kills, the player they killed most and the one who killed them most.
- (rc2, wire version 3) The summary also carries the damage a player dealt and took (a full shield and body is 100 points,
  rounded whole; damage to a teammate or to oneself is taken, never dealt), the kinds of kill they made (melee, power weapon,
  grenade, assassination, splatter, stick), their kills as the flag carrier, the seconds they carried the flag, and the bomb
  plants and defuses. A version 2 (or 1) form has none of these: `extended` is 0 and the screens show a dash, never a zero.
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
	AE_PGCR_SUMMARY_WEAPONS = 6,
	AE_PGCR_SUMMARY_MEDALS = 32,
	/* the players a summary counts kills against and by: those it most traded kills with */
	AE_PGCR_SUMMARY_VERSUS = 8,
	/* a multikill's window, in ticks */
	AE_PGCR_MULTIKILL_TICKS = 120,
	/* the summary's wire form: version, then the fields (ae_pgcr_encode) */
	AE_PGCR_WIRE_VERSION = 3,
	/* version 2's weapons (the most an older reader accepts) */
	AE_PGCR_WIRE_V2_WEAPONS = 3,
	/* version 3's extension: damage dealt and taken (4 each), six kill kinds (2 each), kills as the carrier (2), carrier
	seconds (2), plants (1), defuses (1) */
	AE_PGCR_WIRE_EXTENSION = 8 + 12 + 4 + 2,
	/* the longest version 2 form (the block of the builds before version 3) */
	AE_PGCR_V2_MAXIMUM = 13 + AE_PGCR_WIRE_V2_WEAPONS * 6 + 1 + AE_PGCR_SUMMARY_MEDALS * 2 + 1 + AE_PGCR_SUMMARY_VERSUS * 3,
	AE_PGCR_WIRE_MAXIMUM = 13 + AE_PGCR_SUMMARY_WEAPONS * 6 + 1 + AE_PGCR_SUMMARY_MEDALS * 2 + 1 + AE_PGCR_SUMMARY_VERSUS * 3 +
		AE_PGCR_WIRE_EXTENSION,
	/* a game's ticks a second (the engine's) */
	AE_PGCR_TICKS_PER_SECOND = 30,
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
	/* (wire version 3) 1 when the counts below were sent: 0 for a version 2 or 1 summary, whose counts below are all 0 and
	must read as unknown, not as none */
	int extended;
	unsigned long damage_dealt, damage_taken;
	unsigned short melee, power, grenade, assassination, splatter, stick;
	unsigned short kills_as_flag_carrier, flag_carrier_seconds;
	unsigned char bomb_plants, bomb_defuses;
};

/* the kinds of a kill (ae_pgcr_kill_kinds_of's bits) */
enum
{
	AE_PGCR_KIND_MELEE = 1,
	AE_PGCR_KIND_POWER = 2,
	AE_PGCR_KIND_GRENADE = 4,
	AE_PGCR_KIND_ASSASSINATION = 8,
	AE_PGCR_KIND_SPLATTER = 16,
	AE_PGCR_KIND_STICK = 32
};

/* a bomb event for ae_pgcr_bomb */
enum { AE_PGCR_BOMB_PLANT = 1, AE_PGCR_BOMB_DEFUSE = 2 };

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
	/* (rc2) damage in points (a full shield and body is 100), kill kinds, the carrier's kills and ticks, bomb plants */
	float damage_dealt[AE_PGCR_PLAYERS];
	float damage_taken[AE_PGCR_PLAYERS];
	unsigned short kind[AE_PGCR_PLAYERS][6];
	unsigned short as_carrier[AE_PGCR_PLAYERS];
	long carrier_ticks[AE_PGCR_PLAYERS];
	unsigned char plants[AE_PGCR_PLAYERS];
	unsigned char defuses[AE_PGCR_PLAYERS];
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

/* damage that took this share of the victim's whole vitality (shield and body, 1.0 the lot) from the attacker (-1: no one):
the victim's taken from any source; the attacker's dealt unless it is themselves or `friendly` (a teammate). Never negative */
void ae_pgcr_damage(struct ae_pgcr_game *game, int attacker, int victim, float share, int friendly);
/* the kinds a killing blow is (AE_PGCR_KIND_ bits): melee is the melee category; a power weapon's is a kill with the sniper,
rocket, fuel rod, sword or hammer in hand, but not a bash, a grenade or a splat with one (the sword's blow is both melee and
power); a grenade is the grenade category or a grenade weapon; a stick is the stuck plasma grenade; a splatter is the vehicle
category (the medal's rule); an assassination is a melee kill from behind */
int ae_pgcr_kill_kinds_of(struct ae_medal_kill const *kill);
/* the kill's kinds counted for its killer, and a kill as the flag carrier; only a kill the rules count (an enemy's) */
void ae_pgcr_kill_kinds(struct ae_pgcr_game *game, struct ae_medal_kill const *kill);
/* a tick the slot carried the flag */
void ae_pgcr_carrier_tick(struct ae_pgcr_game *game, int slot);
/* a bomb planted or defused by the slot (AE_PGCR_BOMB_) */
void ae_pgcr_bomb(struct ae_pgcr_game *game, int slot, int what);

/* the slot's summary */
void ae_pgcr_summarize(struct ae_pgcr_game const *game, int slot, struct ae_pgcr_summary *summary);

/* the summary as bytes (little-endian), at most AE_PGCR_WIRE_MAXIMUM: the length */
int ae_pgcr_encode(struct ae_pgcr_summary const *summary, unsigned char *out, int size);
/* the same in an older version's form (1, 2 or 3): what an older host sends; version 2 and 1 keep the first three weapons and
have none of the rc2 counts */
int ae_pgcr_encode_version(struct ae_pgcr_summary const *summary, unsigned char *out, int size, int version);
/* bytes into a summary: 1 when they are a whole, sound one of this version (counts within their limits, medals known, the
rivals a slot or none), else 0 and the summary invalid. Versions 1 and 2 are read too (extended 0) */
int ae_pgcr_decode(unsigned char const *data, int length, struct ae_pgcr_summary *summary);

#endif
