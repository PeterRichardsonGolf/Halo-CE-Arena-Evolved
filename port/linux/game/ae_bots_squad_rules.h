/*
AE_BOTS_SQUAD_RULES.H

Arena Evolved: the numbers of a campaign AI squad member's follow and its choice of an enemy (ae_bots_squad.inc). Pure numbers
only (port/linux/tests/ae_bots_squad_rules_test.c).

SINGLE FILE (on a deck, ae_companion.h): a member stands a settle distance behind the player on their track (a step further for each
member after the first, so they stand in file along the way they came); it sets off again once the player is that distance and a
margin away, and walks until it is back within the settle distance. Off a deck the squad is a fireteam: its follow, slots and
leash are the fireteam's (ae_fireteam_rules.h).
*/

#ifndef __AE_BOTS_SQUAD_RULES_H
#define __AE_BOTS_SQUAD_RULES_H

/* world units: the first member's settle distance, each next member's step further, the margin past it that sets a member off */
#define AE_SQUAD_SETTLE 3.5f
#define AE_SQUAD_SETTLE_STEP 0.8f
#define AE_SQUAD_GO_MARGIN 2.5f
/* an enemy further than this from the player is left to the player: a member only shoots what the player is in reach of */
#define AE_SQUAD_ENGAGE_REACH 45.0f
/* an enemy the player called out is fought first for this long; its score is lowered by the bonus (a near one is still fought
first when the called one is far) */
#define AE_SQUAD_FOCUS_SECONDS 8
#define AE_SQUAD_FOCUS_BONUS 25.0f
/* a deck (ae_companion.h) is held this long after it was last seen (3 s), so its far end or a wider piece does not flip it */
#define AE_SQUAD_DECK_HOLD_TICKS 90
/* THE EDGE: a member never takes a step whose floor drops more than this (world units), on a deck more than the second */
#define AE_SQUAD_EDGE_DROP 3.0f
#define AE_SQUAD_DECK_DROP 1.0f
/* a member carried faster than this (world units a second) has where its speed takes it probed */
#define AE_SQUAD_CARRY_SPEED 1.5f
/* ON THE TRACK: a member walking the player's track that moved less than this (world units) in AE_SQUAD_TRACK_STUCK_TICKS
(half a second) jumps on along the track, where the player jumped over something */
#define AE_SQUAD_TRACK_STUCK_MOVE 0.3f
#define AE_SQUAD_TRACK_STUCK_TICKS 15
/* ... unless a squad mate or a player stands closer than this (world units): the one ahead in single file stopped */
#define AE_SQUAD_TRACK_CLOSE 1.5f
/* DRY, a member whose weapons are all empty (its swap found none loaded): first as a player would, a loaded weapon on the ground
within AE_SQUAD_PICKUP_REACH of it and AE_SQUAD_PICKUP_LEASH of the player is gone for (given up after
AE_SQUAD_PICKUP_GIVE_UP_TICKS, 10 s); it melees and throws grenades meanwhile as its fight does. Only with none in reach for
AE_SQUAD_DRY_RECOVER_TICKS (15 s), and no enemy seen for AE_SQUAD_REFILL_QUIET_TICKS (3 s), its weapon in hand gets one
magazine back every AE_SQUAD_REFILL_STEP_TICKS (5 s) */
#define AE_SQUAD_PICKUP_REACH 15.0f
/* ... in a fight only this near (a multiplayer bot's fetch in a fight) */
#define AE_SQUAD_PICKUP_REACH_FIGHTING 8.0f
#define AE_SQUAD_PICKUP_LEASH 20.0f
#define AE_SQUAD_PICKUP_GIVE_UP_TICKS 300
#define AE_SQUAD_DRY_RECOVER_TICKS 450
#define AE_SQUAD_REFILL_QUIET_TICKS 90
#define AE_SQUAD_REFILL_STEP_TICKS 150
/* the player moving faster than this (world units a second) is kept pace with: a fighting member walks with them, shooting */
#define AE_SQUAD_PACE_SPEED 1.0f
/* ticks (30 a second) between a member's looks for an enemy, and of the grace an enemy lost from sight is still fought */
#define AE_SQUAD_LOOK_TICKS 3

/* the distance a member of number slot (0 up) stands from the player */
float ae_bots_squad_settle(long slot);

/* whether a member walks to the player this tick: moving now (it walked last tick) and not yet within its settle distance, or
standing and the player past the settle distance and the margin */
int ae_bots_squad_follow_wants_move(float distance, int moving, long slot);

/* the score of an enemy to fight (the least is fought): near, ahead of the member, and the one it already fights favoured */
float ae_bots_squad_foe_score(float distance, float bearing, int current);

/* the deck held (AE_SQUAD_DECK_HOLD_TICKS): seen now, it holds until now + the hold (until is set); not seen, it holds while now
is before until. 1 when the member is on a deck this tick */
int ae_bots_squad_deck_held(int seen, long now, long *until);

/* whether a member walking the track jumps this check: it walked the whole check (AE_SQUAD_TRACK_STUCK_TICKS) and moved less
than AE_SQUAD_TRACK_STUCK_MOVE */
int ae_bots_squad_track_jump(int walked_whole_check, float moved);

/* DRY: whether a weapon on the ground is gone for: that far (world units) from the member and from the player, in a fight or not */
int ae_bots_squad_pickup_wanted(float from_member, float from_player, int fighting);
/* DRY: whether the weapon in hand gets a magazine back this tick: no pickup in reach this long, no enemy seen this long, and
the last step this long ago (ticks) */
int ae_bots_squad_refill_step(long no_pickup_ticks, long quiet_ticks, long since_step);

/* whether an enemy that far from the player is one the member fights */
int ae_bots_squad_foe_in_reach(float distance_from_player);

#endif
