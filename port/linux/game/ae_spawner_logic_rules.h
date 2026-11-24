/*
AE_SPAWNER_LOGIC_RULES.H

port: AE weapon spawners (0.7.8; design section 3.2). The state machine of one spawner, pure: the engine
(ae_spawners.c) feeds it what it sees each tick (does the spawner's weapon still exist, is it in someone's hands, is
it out of play) and does what the step says (spawn now, keep the weapon on its spot, log a line). No engine includes;
port/linux/tests/ae_spawner_logic_rules_test.c compiles this alone.

	static   a fixed clock from the delay: a spawn every period, taken or not (CE's own way); no state
	pickup   the weapon stays on its spot until someone takes it; the next one `period` after that
	expired  taken: RED until the weapon is out of play (gone, or empty and not held); the next one `period` after
	trigger  only when its channel fires (ae_spawner_logic_fire), or a test asks

A weapon that leaves its spot untaken (purged, destroyed, its ammo walked off: a pickup that leaves nothing behind)
restarts the count as a pickup would. Ticks: 30 a second; the floor on a period is the rules' 5 s.

A weapon set picks one of two rule sets (`rules ae|reference`, ae_weaponset_rules.h) where they differ:
	a static POWER pad's beat   ae: always spawns on its clock; reference: skipped when its weapon was taken less than
	                            15 s before it (AE_SPAWNER_RULE_STATIC_SKIP on the machine)
	an expired rack's release   ae: its weapon gone, or empty and not held; reference: gone, or empty even while held
	                            (ae_spawner_out_of_play)
	a taken POWER pad           ae: RED shows; reference: never shown red (ae_spawner_shown_red)
*/

#ifndef __AE_SPAWNER_LOGIC_RULES_H
#define __AE_SPAWNER_LOGIC_RULES_H

#include "ae_pure_types.h"
#include "ae_spawner_rules.h"

#define AE_SPAWNER_TICKS_PER_SECOND 30
/* a static spawner's weapon counts as "on its spot" for the item timers this long after its clock (as CE's timers) */
#define AE_SPAWNER_STATIC_ON_SPOT_TICKS (20 * AE_SPAWNER_TICKS_PER_SECOND)
/* the reference rules: a static POWER pad skips a beat that comes this soon after its weapon was taken */
#define AE_SPAWNER_STATIC_SKIP_TICKS (15 * AE_SPAWNER_TICKS_PER_SECOND)

/* a machine's rule bits (the engine sets them after ae_spawner_logic_begin; a reset keeps them) */
#define AE_SPAWNER_RULE_STATIC_SKIP 1

enum ae_spawner_state
{
	AE_SPAWNER_WAITING = 0,	/* nothing on the spot; a countdown runs */
	AE_SPAWNER_READY,	/* the weapon sits on its spot */
	AE_SPAWNER_RED,	/* taken; the next one waits for this one to leave play (expired logic) */
	AE_SPAWNER_DORMANT	/* this game spawns nothing here (no map weapons, a mode without items, a set's mute row) */
};

/* what the engine reports about the spawner's weapon this tick */
struct ae_spawner_sight
{
	boolean exists;	/* the object the spawner made still exists */
	boolean held;	/* it is attached to a unit (picked up) */
	boolean out_of_play;	/* gone, or empty and not held (expired logic's release) */
};

/* what the step tells the engine to do */
enum ae_spawner_action
{
	AE_SPAWNER_DO_NOTHING = 0,
	AE_SPAWNER_DO_SPAWN,	/* make the next weapon now */
	AE_SPAWNER_DO_KEEP,	/* the weapon is on its spot: hold its purge clock */
	AE_SPAWNER_DO_LOG_TAKEN,	/* log "taken" (pickup: next in period; expired: red until out of play) */
	AE_SPAWNER_DO_LOG_LOST,	/* log "lost its weapon untaken: next in period" */
	AE_SPAWNER_DO_LOG_EXPIRED,	/* log "expired (out of play): next in period" */
	AE_SPAWNER_DO_LOG_SKIPPED	/* a static beat skipped (AE_SPAWNER_RULE_STATIC_SKIP): log it, nothing spawns */
};

struct ae_spawner_machine
{
	signed char logic;	/* enum ae_spawner_logic */
	signed char state;	/* enum ae_spawner_state */
	long period, delay;	/* ticks; period at least the floor */
	boolean has_object;	/* the spawner's weapon was made and not yet released */
	boolean taken;
	long next_at;	/* the tick of the next spawn while waiting (pickup, expired, trigger); static: one spawn asked for */
	long made_at;	/* the tick of the last spawn (-1 none) */
	unsigned char rules;	/* AE_SPAWNER_RULE_* bits */
	long taken_at;	/* static: the tick its last weapon was taken (-1 none) */
	long skipped_at;	/* static: the beat last skipped (-1 none) */
};

/* a fresh spawner of a logic: static starts its clock at the delay; the others spawn at the delay too (next_at =
delay) */
void ae_spawner_logic_begin(struct ae_spawner_machine *s, int logic, long period_ticks, long delay_ticks);

/* one tick, at `now`: the action for the engine; `s` moves on. The engine calls ae_spawner_logic_made after a spawn it
performed (with whether an object came out), so a failed spawn waits a period instead of trying every tick */
int ae_spawner_logic_step(struct ae_spawner_machine *s, long now, struct ae_spawner_sight const *sight);
void ae_spawner_logic_made(struct ae_spawner_machine *s, long now, boolean object_made);

/* a trigger spawner's channel fired (or a test asked): spawn on the next step, whatever sits there */
void ae_spawner_logic_fire(struct ae_spawner_machine *s);

/* a static spawner's weapon was picked up at `now` (the engine watches the weapon it made) */
void ae_spawner_logic_static_taken(struct ae_spawner_machine *s, long now);

/* whether a spawner's weapon is out of play (an expired spawner's release): gone; or empty and, under the ae rules, not
held (the reference rules: empty even in someone's hands) */
int ae_spawner_out_of_play(int exists, int held, int empty, int reference);

/* whether a RED spawner shows red: always under the ae rules; under the reference rules a POWER pad never does */
int ae_spawner_shown_red(int red, int power, int reference);

/* a set switch mid-game: the new logic and times from the next spawn; what is on the map stays, a running count
keeps its remaining time (capped to the new period) */
void ae_spawner_logic_retime(struct ae_spawner_machine *s, int logic, long period_ticks, long delay_ticks, long now);

/* the item timers' view: ticks to the next spawn (1..period) and whether the weapon is on its spot now */
long ae_spawner_logic_ticks_left(struct ae_spawner_machine const *s, long now);
boolean ae_spawner_logic_on_spot(struct ae_spawner_machine const *s, long now);

/* the game's clock went back (a new game on the same map): everything starts over */
void ae_spawner_logic_reset(struct ae_spawner_machine *s);
/* a round's reset (the map back as at the game's start): the spot starts over from now, its start delay counted from
now (a trigger waits for its channel) */
void ae_spawner_logic_restart(struct ae_spawner_machine *s, long now);

#endif /* __AE_SPAWNER_LOGIC_RULES_H */
