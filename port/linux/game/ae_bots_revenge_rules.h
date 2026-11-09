/*
AE_BOTS_REVENGE_RULES.H

Arena Evolved: REVENGE (owner 2026-10-24: "if they get killed multiple times over and over by a player they will try to
find them and kill them"). Pure (port/linux/tests/ae_bots_revenge_rules_test.c); levels as ae_bots_combat_rules.h's.

- A bot killed AE_BOTS_REVENGE_DEATHS times in a row by the same enemy player, with no kill of that player between,
  takes that player as its NEMESIS for AE_BOTS_REVENGE_SECONDS (from the last of those deaths) or until it kills them.
  A death to someone else starts a new count on them; a suicide, a fall or a teammate's kill counts for nobody and
  breaks nothing. Killing the player who killed it resets the count.
- While it has a nemesis, the bot picks the nemesis first of the enemies it sees (a target score taken off) and, not
  fighting, goes after them instead of roaming, power items or regrouping (objectives and vehicles still first).
- HOW, BY LEVEL (owner's pick: "hunt, by level"): RECRUIT, EASY and NORMAL chase RECKLESSLY (straight to where the
  nemesis is, and in the fight with them more confident than it should be: it pushes in, it does not back off so soon:
  tilt, easier to farm); HEROIC chases straight too but keeps its head in the fight (a little bonus only); LEGENDARY
  and DEMON hunt SMART: they go to a spot near the nemesis (AE_BOTS_REVENGE_SMART_NEAR .. _FAR units off, higher
  ground preferred) and hold the angle, with their own fight rules unchanged.
*/

#ifndef __AE_BOTS_REVENGE_RULES_H
#define __AE_BOTS_REVENGE_RULES_H

#define AE_BOTS_REVENGE_DEATHS 3
#define AE_BOTS_REVENGE_SECONDS 60
/* the target score taken off the nemesis (bots.c's score: distance + bearing * 4; the one it fights already has 6) */
#define AE_BOTS_REVENGE_TARGET_SCORE 8.0f
/* a smart hunter's spot: this far from the nemesis at least, and at most */
#define AE_BOTS_REVENGE_SMART_NEAR 8.0f
#define AE_BOTS_REVENGE_SMART_FAR 18.0f

enum
{
	AE_BOTS_REVENGE_RECKLESS = 1,
	AE_BOTS_REVENGE_DIRECT,
	AE_BOTS_REVENGE_SMART
};

/* (players are the game's 32-bit player handles, -1 for none: a valid one is negative as an int too, so only -1 means
nobody) */
struct ae_bots_revenge
{
	/* who killed it last and how many times in a row */
	int killer;
	int streak;
	/* the player it hunts and until when (ticks) */
	int nemesis;
	int until_tick;
};

void ae_bots_revenge_clear(struct ae_bots_revenge *state);
/* the bot's death to killer (-1: nobody's, a suicide or a teammate: counts nothing); 1 when it takes a new nemesis */
int ae_bots_revenge_death(struct ae_bots_revenge *state, int killer, int now, int ticks_per_second);
/* the bot's kill of victim; 1 when that was its nemesis (revenge done) */
int ae_bots_revenge_kill(struct ae_bots_revenge *state, int victim);
/* the nemesis now, -1 for none (one whose time is up is let go) */
int ae_bots_revenge_nemesis(struct ae_bots_revenge *state, int now);
/* AE_BOTS_REVENGE_RECKLESS / _DIRECT / _SMART for a level (0 RECRUIT .. 5 DEMON) */
int ae_bots_revenge_style(int level);
/* the confidence added in a fight with the nemesis (the tilt): RECKLESS 0.3, DIRECT 0.1, SMART 0 */
float ae_bots_revenge_confidence(int level);

#endif
