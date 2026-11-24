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

/* FORMATION: each member of a FOLLOW or a REGROUP has a free spot of its own near the player, AE_SQUAD_SPOT_RADII rings out
(3, 4 and 5 world units), behind and to the sides of where the player faces (90 degrees or more off it: never in front of
the aim), a floor within AE_SQUAD_SPOT_STEP of the player's height, a line from the player, at least AE_SQUAD_SPOT_SPACING
from another member's spot and AE_SQUAD_SPOT_LEADER_GAP from the player; the spots are chosen again every
AE_SQUAD_SPOT_REFRESH_TICKS (3 s) or when the player moved AE_SQUAD_SPOT_MOVED from where they were chosen */
#define AE_SQUAD_SPOT_RING_FIRST 3.0f
#define AE_SQUAD_SPOT_RING_STEP 1.0f
#define AE_SQUAD_SPOT_RADII 3
#define AE_SQUAD_SPOT_ANGLES 7
#define AE_SQUAD_SPOT_STEP 1.0f
#define AE_SQUAD_SPOT_SPACING 1.5f
#define AE_SQUAD_SPOT_LEADER_GAP 2.0f
#define AE_SQUAD_SPOT_REFRESH_TICKS 90
#define AE_SQUAD_SPOT_MOVED 3.0f
#define AE_SQUAD_SPOT_CANDIDATES (AE_SQUAD_SPOT_RADII * AE_SQUAD_SPOT_ANGLES)

/* the spots a member of number slot (0 up) tries, best first, as offsets from the player facing that way (radians): the near ring
first, its own side first (the first member behind-left, the second behind-right, the third behind, the fourth to the side);
the count (at most most) */
int ae_bots_squad_spot_candidates(long slot, float facing, float *dx, float *dy, int most);
/* whether an offset from the player is behind or beside their facing (90 degrees or more off it) */
int ae_bots_squad_spot_behind(float facing, float dx, float dy);
/* whether a spot is too near another member's spot or the player (world units) */
int ae_bots_squad_spot_crowded(float to_other_spot, float to_player);
/* whether the spots are chosen again: this long since (ticks), the player moved this far since */
int ae_bots_squad_spot_refresh(long since, float player_moved);

/* TEAM LAYER: the squad's state over its order. FORMED: every member in position (ae_bots_squad_in_position: under a FOLLOW within
AE_SQUAD_IN_POSITION_FOLLOW of its spot, where the walk to it stops; under a REGROUP within AE_SQUAD_IN_POSITION_REGROUP);
MOVING: the player walks; GATHERING: members on their way to their spots; REGROUPING: a REGROUP until every member is in, given
up after AE_SQUAD_REGROUP_TIMEOUT_TICKS (10 s); HOLDING: a point order (DEFEND, WATCH, GOING, ATTACK). A member out of position
for AE_SQUAD_KEEP_UP_TICKS (30 s) says it is catching up, once every 30 s */
enum { AE_SQUAD_TEAM_NONE, AE_SQUAD_TEAM_FORMED, AE_SQUAD_TEAM_MOVING, AE_SQUAD_TEAM_GATHERING, AE_SQUAD_TEAM_REGROUPING,
	AE_SQUAD_TEAM_HOLDING };
enum { AE_SQUAD_ORDER_FOLLOW, AE_SQUAD_ORDER_REGROUP, AE_SQUAD_ORDER_HOLD };
#define AE_SQUAD_IN_POSITION_FOLLOW 3.0f
#define AE_SQUAD_IN_POSITION_REGROUP 2.0f
#define AE_SQUAD_REGROUP_TIMEOUT_TICKS 300
#define AE_SQUAD_KEEP_UP_TICKS 900
int ae_bots_squad_in_position(int order, float from_spot);
int ae_bots_squad_team_state(int order, int player_moving, int members, int in_position);
char const *ae_bots_squad_team_state_name(int state);
int ae_bots_squad_regroup_timed_out(long waited);
int ae_bots_squad_keep_up_due(long out_of_position, long since_said);

/* ORDERS, the SMART tap in a campaign game, by what it is on, in this order: an enemy is fought first (FOCUS); a tap within
AE_SQUAD_SMART_CANCEL_REACH of the squad's own point order cancels it (CANCEL: follow again); a vehicle or turret with a free
seat is manned, the gunner's seat first, by the nearest member within AE_SQUAD_SMART_MOUNT_REACH (MOUNT); a weapon on the
ground is taken by the nearest member within AE_SQUAD_SMART_TAKE_REACH (TAKE); else the place is gone to (MOVE, the existing
GOING). A SMART tap on a vehicle a member was sent to lets it go: the member gets out. The member that takes it on says "on it"; with none that can, no answer */
enum { AE_SQUAD_SMART_NONE, AE_SQUAD_SMART_FOCUS, AE_SQUAD_SMART_CANCEL, AE_SQUAD_SMART_MOUNT, AE_SQUAD_SMART_TAKE, AE_SQUAD_SMART_MOVE };
#define AE_SQUAD_SMART_CANCEL_REACH 2.5f
#define AE_SQUAD_SMART_MOUNT_REACH 30.0f
#define AE_SQUAD_SMART_TAKE_REACH 25.0f
/* a member sent to a vehicle gets in within this distance of it (world units), and gives up after this long (ticks, 20 s) */
#define AE_SQUAD_MOUNT_ENTER 2.5f
/* the seat is taken within AE_SQUAD_MOUNT_BOARD of the vehicle: twice the walk's stop, room for a vehicle's width */
#define AE_SQUAD_MOUNT_BOARD 5.0f
#define AE_SQUAD_MOUNT_GIVE_UP_TICKS 600
int ae_bots_squad_smart_pick(int on_enemy, int has_point_order, float to_order_point, int on_vehicle_with_seat, int on_weapon);
/* whether the nearest member that far (world units) takes on the pick */
int ae_bots_squad_smart_in_reach(int pick, float member_distance);

/* COVER: a member fighting under FOLLOW with the player standing still takes a spot of cover from the enemy it fights. Spots
on two rings about it (AE_SQUAD_COVER_RING_NEAR and _FAR, AE_SQUAD_COVER_ANGLES each), with a floor within AE_SQUAD_SPOT_STEP of its
own, within AE_SQUAD_COVER_LEASH of the player and AE_SQUAD_SPOT_SPACING of no other member, are rated by the enemy's lines to them:
at AE_SQUAD_COVER_LOW (a crouch's body) and at AE_SQUAD_COVER_HIGH (the eyes) above the floor. Exposure: 0 neither line (full
cover), 0.5 the eyes only (half cover: it shoots over it), 0.75 the body only, 1 both (open). It leaves a spot of exposure
AE_SQUAD_COVER_LEAVE or more for one of AE_SQUAD_COVER_TAKE or less (two thresholds: no flicker), half cover first, then the
nearest; checked every AE_SQUAD_COVER_CHECK_TICKS */
#define AE_SQUAD_COVER_RING_NEAR 2.0f
#define AE_SQUAD_COVER_RING_FAR 3.5f
#define AE_SQUAD_COVER_ANGLES 8
#define AE_SQUAD_COVER_LEASH 8.0f
#define AE_SQUAD_COVER_LOW 0.5f
#define AE_SQUAD_COVER_HIGH 1.3f
#define AE_SQUAD_COVER_TAKE 0.5f
#define AE_SQUAD_COVER_LEAVE 0.75f
#define AE_SQUAD_COVER_CHECK_TICKS 45
#define AE_SQUAD_COVER_ARRIVED 0.8f
float ae_bots_squad_cover_exposure(int body_seen, int eyes_seen);
int ae_bots_squad_cover_leaves(float exposure_here);
int ae_bots_squad_cover_takes(float exposure);
/* a spot's value, lower is better: half cover first, then the shorter walk (world units) */
float ae_bots_squad_cover_value(float exposure, float walk);
/* A member in a gunner's seat (a Warthog's chain gun) fires as a multiplayer bot gunner does (ae_bots_ut_gunner): the trigger
held while its aim is within the cone on its target, atan(AE_SQUAD_GUNNER_CONE_SIZE / distance) plus AE_SQUAD_GUNNER_CONE_DEGREES
(pitch: one and a half of it), the target seen within AE_SQUAD_GUNNER_SEEN_TICKS and for AE_SQUAD_GUNNER_REACT_TICKS; no bursts */
#define AE_SQUAD_GUNNER_CONE_SIZE 0.6f
#define AE_SQUAD_GUNNER_CONE_DEGREES 3.0f
#define AE_SQUAD_GUNNER_SEEN_TICKS 3
#define AE_SQUAD_GUNNER_REACT_TICKS 7
/* a member in a gunner's seat takes targets AE_SQUAD_GUNNER_REACH_SCALE times as far as its level's sight (heroic 42: 63) and as far
from the player, as a human gunner hits with the chain gun; on foot as before */
#define AE_SQUAD_GUNNER_REACH_SCALE 1.5f
float ae_bots_squad_gunner_reach(float sight, int seated_gunner);
/* the cone (radians) at a distance (world units) */
float ae_bots_squad_gunner_cone(float distance);
/* whether the gunner holds the trigger: its aim off the target (radians), the distance, ticks since seen and since first seen */
int ae_bots_squad_gunner_holds(float yaw_off, float pitch_off, float distance, long seen_ago, long seen_for);

/* CUT OFF: a member AE_SQUAD_CUT_OFF_GAP or more from the player with the line to them blocked at AE_SQUAD_CUT_OFF_HEIGHT above
the feet (a waist-high barrier, a doorway's pipes the player jumped) walks the player's own track, its stops jumped (the
track jump), as on a deck; the line is tested every AE_SQUAD_CUT_OFF_CHECK_TICKS. A formation slot whose line from the player
is blocked so is pulled to the player's side, AE_SQUAD_CUT_OFF_PULL short of the block */
#define AE_SQUAD_CUT_OFF_GAP 2.0f
#define AE_SQUAD_CUT_OFF_HEIGHT 0.4f
#define AE_SQUAD_CUT_OFF_CHECK_TICKS 10
#define AE_SQUAD_CUT_OFF_PULL 1.0f
/* indoors also a line AE_SQUAD_CUT_OFF_LOW above the feet, and both faces of the level (a barrier seen from its far side) */
#define AE_SQUAD_CUT_OFF_LOW 0.2f
int ae_bots_squad_cut_off(int line_blocked, float distance);
/* whether a squad mate or a player stops a track jump: within AE_SQUAD_TRACK_CLOSE and ahead on the way to the player (nearer the
player by AE_SQUAD_TRACK_AHEAD: the one in front stopped); one beside or behind does not (two members side by side at a barrier) */
#define AE_SQUAD_TRACK_AHEAD 0.3f
int ae_bots_squad_track_blocked(float to_other, float other_to_player, float me_to_player);

/* TRACK FOLLOW: under FOLLOW with the player indoors (a roof within AE_SQUAD_ROOF straight up, tested every
AE_SQUAD_ROOF_CHECK_TICKS) a member on the player's track (a crumb within AE_COMPANION_TRACK_ON) walks the track in single file,
not to a formation spot: the first AE_SQUAD_TRACK_GAP_FIRST behind the player, each next one AE_SQUAD_TRACK_GAP_NEXT further,
AE_SQUAD_TRACK_GO_MARGIN more before one standing sets off; outdoors, or off the track, the formation as before */
#define AE_SQUAD_ROOF 10.0f
#define AE_SQUAD_ROOF_CHECK_TICKS 30
#define AE_SQUAD_TRACK_GAP_FIRST 2.5f
#define AE_SQUAD_TRACK_GAP_NEXT 1.5f
#define AE_SQUAD_TRACK_GO_MARGIN 1.0f
int ae_bots_squad_indoors(int roof_hit, float roof_height);
int ae_bots_squad_track_follows(int indoors, int on_track, int following);
int ae_bots_squad_track_wants_move(float distance, int moving, long slot);
/* how far a member on the track is out of its single file's place (the team layer's in position): 0 where it stands still */
float ae_bots_squad_track_off(float distance, long slot);
/* PERCHED: a member on the track standing AE_SQUAD_PERCH_RISE or more above the player (on a barrier's top, the pipes) and
AE_SQUAD_PERCH_NEAR or more from them is not in its place: it walks on along the track, off the barrier */
#define AE_SQUAD_PERCH_RISE 0.15f
#define AE_SQUAD_PERCH_NEAR 1.5f
int ae_bots_squad_perched(float above_player, float distance);

#endif
