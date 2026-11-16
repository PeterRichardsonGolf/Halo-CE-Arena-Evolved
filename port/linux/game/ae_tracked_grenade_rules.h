/*
AE_TRACKED_GRENADE_RULES.H

port: weapon pack (0.7.6). The pure rules of a pack weapon whose trigger tracks the projectile it fired (the
trigger's "tracks fired projectile" flag, which the game's trigger state machine has, _trigger_tracking, but nothing
ever filled in, and the weapon's busy time after a shot reads to it as the trigger let go): the GRENADE LAUNCHER
pack's hold-to-delay, release-to-detonate. While the trigger is held the grenade's fuse does not run; letting go sets
it off where it is. A tap (let go sooner than the minimum hold) leaves
the grenade to its fuse, as the tags have it. No engine includes: port/linux/tests/ae_tracked_grenade_rules_test.c
compiles these alone; ae_tracked_grenade.c applies them in the game.
*/

#ifndef __AE_TRACKED_GRENADE_RULES_H
#define __AE_TRACKED_GRENADE_RULES_H

/* a release sets the grenade off only after it has been held this long since the shot (0.2 s at 30 ticks a second):
a quicker tap is a plain shot */
#define AE_TRACKED_GRENADE_MIN_HOLD_TICKS 6
/* the fuse is held back for at most this long (10 s): then it runs again even with the trigger still held */
#define AE_TRACKED_GRENADE_MAX_HOLD_TICKS 300

/* whether a shot's projectile is tracked: the weapon is a pack's own tag (a stock or map weapon never is, whatever its
trigger's flag), its trigger has "tracks fired projectile", and it is the shot's first projectile */
int ae_tracked_grenade_tracks(int pack_own_weapon, int trigger_tracks, long projectile_in_shot);

/* whether a tracked row still names what it named at the shot: the projectile handle names a projectile of the tag
recorded then (else the row is stale: forgotten, the projectile runs as its tags have it). A row of another map never
gets here: a new map forgets every row (ae_tracked_grenade_new_map) */
int ae_tracked_grenade_same_projectile(int projectile_there, long projectile_definition, long recorded_definition);

/* whether the weapon the row names still counts as the shooter: there, and of the tag recorded at the shot (else the
grenade is treated as let go) */
int ae_tracked_grenade_same_weapon(int weapon_there, long weapon_definition, long recorded_definition);

/* whether the shooter still holds the trigger: the weapon is there, in its unit's hands (its current weapon: a weapon
dropped, put away or whose unit is gone keeps its last control, which is no longer anyone's input) and its primary
trigger is down */
int ae_tracked_grenade_trigger_held(int weapon_there, int in_hand, int trigger_down);

/* the owner a projectile keeps: its shooter's object while that still exists, else none (-1). A grenade left to its
fuse, or one that never comes to rest, can outlive the unit that shot it (a dead unit is deleted for the respawn), and
its owner handle then names nothing: the damage code's walk up the owner's parents asserts on it. The kill credit is the
projectile's own owner player, which stays */
long ae_tracked_grenade_owner(long owner_index, int owner_there);

/* what a tracked grenade does this tick (its own update): */
enum
{
	AE_TRACKED_GRENADE_RUN = 0,	/* as its tags have it (not tracked, or no longer: forgotten) */
	AE_TRACKED_GRENADE_HOLD,	/* the trigger still held: its fuse waits */
	AE_TRACKED_GRENADE_DETONATE,	/* let go after the minimum hold: set off now (then forgotten) */
	AE_TRACKED_GRENADE_FUSE,	/* its fuse runs now, from the first bounce (ae_tracked_grenade_fuse_runs) */
};

/* a projectile tag's "detonation timer starts" value for after the first bounce (the game's enum, which is private to
its projectile file) */
#define AE_TRACKED_GRENADE_STARTS_AFTER_FIRST_BOUNCE 1

/* whether the fuse of a pack's own grenade runs from its first bounce. The game's after-the-first-bounce timer starts
only once the grenade has come to rest, and one that creeps on never does (it then lives until it hits something). Only a
pack's own projectile tag that asks for the first bounce; a stock grenade keeps the game's way. tick_action is this
tick's ae_tracked_grenade_tick_action (or RUN for an untracked grenade): a held grenade waits, a let-go one is set off,
and neither is changed. An impact detonation is the collision's own and is not touched */
int ae_tracked_grenade_fuse_runs(int tick_action, int pack_own_projectile, long timer_starts, int collided_once);

/* this tick's action for a tracked grenade: weapon_there (the weapon that fired it still exists), trigger_held (that
weapon's primary trigger is down: its owner's input, the host's relay of it for another machine's player), held_ticks
(ticks since the shot; negative, a clock gone back, is let go). Held past the cap: its fuse runs again (forgotten). A
release sooner than the minimum is a tap: its fuse runs (forgotten). The weapon gone counts as let go */
int ae_tracked_grenade_tick_action(int weapon_there, int trigger_held, long held_ticks);

#endif /* __AE_TRACKED_GRENADE_RULES_H */
