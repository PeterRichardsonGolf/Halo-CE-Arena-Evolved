/*
AE_BOTS_POWER_RULES.H

Arena Evolved: the bots go for power items as a player who knows the timers does (owner 2026-10-20, after HEROIC on
Temple: "would be interesting to see how they were if they picked up power weapons and powerups consistently. I don't
think I have seen them pick up a sniper or OV on that map"). Pure (port/linux/tests/ae_bots_power_rules_test.c).

The items are item_timers.c's power entries (rockets, sniper, shotgun, overshield, camouflage: the kinds below are in
its item_timer_class order), with each one's time to its next spawn. How far a bot goes for one and how early, by
level (world units; a biped runs about 2.25 a second):

  RECRUIT 8, EASY 12, NORMAL 25: only an item on the map;
  HEROIC 45, there up to 5 s before it spawns; LEGENDARY 70, 10 s; DEMON anywhere, 15 s.

In a fight, only one within 8 units. Never one the bot holds, one it found taken (until it spawns again), or one a
teammate bot is already going for.
*/

#ifndef __AE_BOTS_POWER_RULES_H
#define __AE_BOTS_POWER_RULES_H

enum
{
	AE_BOTS_POWER_ROCKETS = 0,
	AE_BOTS_POWER_SNIPER,
	AE_BOTS_POWER_SHOTGUN,
	AE_BOTS_POWER_OVERSHIELD,
	AE_BOTS_POWER_CAMO,
	AE_BOTS_POWER_KINDS
};

#define AE_BOTS_POWER_RUN_SPEED 2.25f
#define AE_BOTS_POWER_FIGHTING_REACH 8.0f

struct ae_bots_power_view
{
	int level;		/* AE_BOTS_LEVEL_RECRUIT .. AE_BOTS_LEVEL_DEMON */
	int kind;		/* AE_BOTS_POWER_* */
	float distance;		/* from the bot to the spawn point */
	float seconds_left;	/* to its next spawn; ignored while up */
	int up;			/* the item lies there now */
	int known_taken;	/* the bot saw the spot empty since its last spawn */
	int fighting;
	int holds_it;		/* the bot holds that weapon (or wears that powerup) */
	int teammate_going;	/* another bot of its team is going for it */
};

/* how far a level goes for a power item (world units) */
float ae_bots_power_reach(int level);
/* how many seconds before a spawn a level wants to be there (0: only an item on the map) */
float ae_bots_power_lead_seconds(int level);
/* how much an item is worth to a bot (rockets and the overshield the most, then the sniper, the camouflage, the
shotgun) */
float ae_bots_power_value(int kind);
/* how much the bot wants to go for it now: 0 for not, else higher for better and nearer */
float ae_bots_power_want(struct ae_bots_power_view const *view);

#endif
