/*
AE_BOTS_POWER_RULES.H

Arena Evolved: the bots go for power items as a player who knows the timers does (owner 2026-10-20, after HEROIC on
Temple: "would be interesting to see how they were if they picked up power weapons and powerups consistently. I don't
think I have seen them pick up a sniper or OV on that map"). Pure (port/linux/tests/ae_bots_power_rules_test.c).

The items are item_timers.c's power entries (rockets, sniper, shotgun, overshield, camouflage, fuel rod, flamethrower: the kinds below are in
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
	AE_BOTS_POWER_FUEL_ROD,
	AE_BOTS_POWER_FLAMETHROWER,
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

/* MOUNTING a stand (0.7.4; Temple's overshield and camouflage: a stand 0.75 units high, its top 1.0 across with a steep
bevelled rim out to 1.5, and a base that stops a bot 1.0 from its centre. The bots took about a quarter of their
spawns: they met the base's corners on the diagonals, and their jump came every 20 ticks wherever they were, aimed at
the path's last step, over the stand and 0.6 to its side). A bot near a power item that lies a step above its feet (a
rise a crouch jump clears), on a stand whose face steps up along the lane (a ramp or stairs are walked), goes to a
staging point 2.0 from the item on the one of eight axes (45 degrees apart) with the most top to land on (Temple's
altars are a diagonal ridge 0.8 across: along it there is room, across it there is not; the bot's own side among the
like), runs at the item along that axis, and jumps once, 1.5 to 1.8 from it, in the lane and moving at it. A bot runs 0.07 a tick and its jump takes
20 ticks to the top, which is then over the item (from 1.2 the bot meets the rim low in its jump, loses its run on the
slope, comes down on it and slides off); there it crouches, as in any jump of a bot's, to the top of the stand. A jump
that comes down on the floor turns the axis a quarter on for the next. */
#define AE_BOTS_POWER_MOUNT_RANGE 3.5f
#define AE_BOTS_POWER_MOUNT_STAGING 2.0f
#define AE_BOTS_POWER_MOUNT_JUMP_ACROSS 1.8f
#define AE_BOTS_POWER_MOUNT_JUMP_NEAREST 1.5f
#define AE_BOTS_POWER_MOUNT_LANE 0.3f
#define AE_BOTS_POWER_MOUNT_RISE_MIN 0.3f
#define AE_BOTS_POWER_MOUNT_RISE_MAX 1.0f
/* the rise in 0.25 units that makes a stand's face (a ramp or stairs rise less) */
#define AE_BOTS_POWER_MOUNT_STEP 0.3f
/* whether an item this far above the bot's feet, this far off across, is one to mount */
int ae_bots_power_mount_applies(float rise, float across);
/* the staging point for a bot at (from) and an item at (item): the axis nearest to the bot, turned a quarter turn
shift times */
void ae_bots_power_mount_point(float item_x, float item_y, float from_x, float from_y, int shift, float *x, float *y);
/* whether the bot (position relative to the item, x and y, and the axis's unit vector) is in the lane to run in: out on
the axis's side by at least 1.0 and within 0.3 of the axis */
int ae_bots_power_mount_in_lane(float x, float y, float axis_x, float axis_y);
/* whether the bot jumps now: 1.5 to 1.8 from the item, its velocity (x, y per tick) at the item
(within 18 degrees) and at a walk's speed or more */
int ae_bots_power_mount_jump(float across, float velocity_x, float velocity_y, float to_item_x, float to_item_y);

#endif
