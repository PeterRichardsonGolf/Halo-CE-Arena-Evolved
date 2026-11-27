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
/* (the campaign squad: an enemy called out stays the squad's first target until it is dead or the next call names another) */
#define AE_SQUAD_FOCUS_STANDS_SECONDS 600
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
/* whether the player went into another room (cluster) of the level since the spots were chosen: they are chosen again at once
(-1: no room known) */
int ae_bots_squad_room_changed(long was, long now);

/* DOOR STACK: at a door (a machine of the door kind, which opens by itself for a body within its activation radius) the squad
stacks on the player's side at places found about the door (PLACES below); on GO they go in nearest first and clear the room by its
clear list. A DEFEND or SMART within AE_SQUAD_DOOR_NEAR
of a door stacks there; with every door on, the player still AE_SQUAD_DOOR_AUTO_TICKS within AE_SQUAD_DOOR_AUTO_NEAR of a shut or open
door the level has not locked does. A grenade goes in first only when an enemy in the room was seen or heard within
AE_SQUAD_DOOR_HEARD_TICKS, the thrower has one and the player is AE_SQUAD_GRENADE_SAFE or more from the blast, the entry
AE_SQUAD_GRENADE_WAIT_TICKS after the throw; a door still shut AE_SQUAD_DOOR_LOCKED_TICKS after the first man came into its
reach is locked; inside, no enemy seen for AE_SQUAD_CLEAR_TICKS is clear; the stack waits AE_SQUAD_STACK_TIMEOUT_TICKS at most
for all to be in place */
#define AE_SQUAD_DOOR_NEAR 3.0f
#define AE_SQUAD_DOOR_AUTO_NEAR 4.0f
#define AE_SQUAD_DOOR_AUTO_TICKS 60
#define AE_SQUAD_DOOR_HEARD_TICKS 300
#define AE_SQUAD_GRENADE_WAIT_TICKS 18
#define AE_SQUAD_GRENADE_SAFE 4.0f
#define AE_SQUAD_DOOR_LOCKED_TICKS 30
#define AE_SQUAD_CLEAR_TICKS 90
#define AE_SQUAD_STACK_TIMEOUT_TICKS 300
enum { AE_SQUAD_DOOR_NONE, AE_SQUAD_DOOR_STACKING, AE_SQUAD_DOOR_STACKED, AE_SQUAD_DOOR_GRENADE, AE_SQUAD_DOOR_ENTERING,
	AE_SQUAD_DOOR_IN };
/* the side of a door a point is on (dx, dy from its centre): +1 the way it faces (or in its plane), -1 behind it */
int ae_bots_squad_door_side(float forward_x, float forward_y, float dx, float dy);
/* whether the room behind a door opens to its left (the longer of two rays along the frame from inside it) */
int ae_bots_squad_door_open_left(float left_ray, float right_ray);
/* the members by distance to the door, nearest first, one under 0 (dead, seated) left out: order gets their indices; the count */
int ae_bots_squad_entry_order(float const *distances, int count, int *order);
/* whether a grenade goes in first (ticks since an enemy in the room was seen or heard, under 0 never); only on the player's
ATTACK (ordered), never when the player walked into or through the door (the player is in the doorway) */
int ae_bots_squad_entry_grenade(long ticks_since_seen_or_heard, int has_grenade, float player_to_blast, int ordered);
/* whether the squad's order changed since the stack took it (a new serial is a new order, even of the same kind) */
int ae_bots_squad_door_new_order(long serial_at_stack, long serial_now);
/* whether a stack the squad took on its own (EVERY DOOR) is let go: the player on the squad's side, AE_SQUAD_DOOR_LEFT or more
from the door */
#define AE_SQUAD_DOOR_LEFT 8.0f
int ae_bots_squad_door_left(float player_to_door, int player_on_squad_side);
/* a door the level keeps shut (locked) or without power (it does not move) is shut for good for the squad */
int ae_bots_squad_door_shut_for_good(int locked, float power);

/* PLACES at a door, found at runtime: grid points AE_SQUAD_PLACE_STEP apart within AE_SQUAD_PLACE_REACH of
the stack point; a point is walkable with a floor AE_SQUAD_PLACE_DROP or less below, flat (normal z AE_SQUAD_PLACE_FLAT or more),
AE_SQUAD_PLACE_HEADROOM of room above and a clear chest-high line from the stack point */
#define AE_SQUAD_PLACE_STEP 0.25f
#define AE_SQUAD_PLACE_REACH 1.6f
#define AE_SQUAD_PLACE_DROP 0.6f
#define AE_SQUAD_PLACE_FLAT 0.7f
#define AE_SQUAD_PLACE_HEADROOM 0.6f
#define AE_SQUAD_PLACES 256
int ae_bots_squad_grid_walkable(float floor_drop, float normal_z, float headroom, int line_clear);
/* a stack place skipped: within AE_SQUAD_STACK_NEAR of the stack point, on the room's side of the door, or within the door's reach
+ AE_SQUAD_STACK_PAD (a Halo door opens for a body there) */
#define AE_SQUAD_STACK_NEAR 0.3f
#define AE_SQUAD_STACK_PAD 0.6f
int ae_bots_squad_stack_skip(float to_stack_point, int room_side, float to_door, float reach);
/* a stack place's score: +1 a wall close beside it (it hugs the wall), -10 no line to the stack point, -2 the room sees it through
the door, -2 a known foe near it */
float ae_bots_squad_stack_score(int wall_beside, int line_to_stack, int room_sees, int foe_near);
/* the men's stack places, man by man: the place of least distance/2 - score x AE_SQUAD_STACK_SCORE_UNIT; a place taken blocks all
within AE_SQUAD_STACK_APART. places: x, y, score; men: x, y; slot_of_man gets each man's place (-1: none, he holds where he is);
the count placed */
#define AE_SQUAD_STACK_SCORE_UNIT 0.5f
#define AE_SQUAD_STACK_APART 0.45f
int ae_bots_squad_stack_assign(float const (*places)[3], int place_count, float const (*men)[2], int man_count, int *slot_of_man);
/* the clearing order: by distance to the door, AE_SQUAD_CLEAR_FAR_BONUS less for one already on the room's side, a tie to the nearer
his own place; under 0 left out; order gets the indices, the count returned */
#define AE_SQUAD_CLEAR_FAR_BONUS 0.5f
int ae_bots_squad_clear_order(float const *to_door, int const *room_side, float const *to_place, int count, int *order);
/* the room's clear list: the nearest point not visited (-1 none left); a member within AE_SQUAD_CLEAR_VISIT visits a point (the
count newly visited) */
#define AE_SQUAD_CLEAR_VISIT 0.6f
#define AE_SQUAD_CLEAR_POINTS 48
int ae_bots_squad_clear_next(float const (*points)[2], int const *visited, int count, float x, float y);
int ae_bots_squad_clear_visit(float const (*points)[2], int *visited, int count, float x, float y);
/* a member walking with the player keeps out of a door's reach + AE_SQUAD_DOOR_KEEP_OUT unless the player went through it */
#define AE_SQUAD_DOOR_KEEP_OUT 0.3f
int ae_bots_squad_door_keep_out(float to_door, float reach, int player_through);
/* EVERY DOOR's door: ahead of the player, within AE_SQUAD_DOOR_AHEAD_ANGLE of the facing and AE_SQUAD_DOOR_AUTO_NEAR */
#define AE_SQUAD_DOOR_AHEAD_ANGLE 0.785f
int ae_bots_squad_door_ahead(float angle_off_facing, float distance);

/* JUMP UNDER A LOW CEILING (a doorway's tunnel with pipes in it): a jump with a lip ahead (ae_bots_squad_ceiling_lip) would strike
it: no jump, a crouched walk on into the tunnel (CREEP); with none the jump goes (GO); from its first tick in the air, AE_SQUAD_JUMP_CROUCH_TICKS long, it
presses on crouched (AIR, the player's crouch jump). ticks_since_jump: under 0 none */
#define AE_SQUAD_JUMP_LIP_AHEAD 0.6f
#define AE_SQUAD_JUMP_CROUCH_TICKS 15
enum { AE_SQUAD_JUMP_NONE, AE_SQUAD_JUMP_CREEP, AE_SQUAD_JUMP_GO, AE_SQUAD_JUMP_AIR };
int ae_bots_squad_jump_plan(int want_jump, int lip_ahead, long ticks_since_jump);
/* a lip ahead: the ceiling AE_SQUAD_JUMP_LIP_AHEAD ahead is lower than the one here and under AE_SQUAD_LOW_CEILING (heights above
the feet) */
#define AE_SQUAD_LOW_CEILING 1.4f
int ae_bots_squad_ceiling_lip(float here, float ahead);
/* a crouched walk at a lip (CREEP) that gained under AE_SQUAD_CREEP_GAIN in AE_SQUAD_CREEP_GIVE_UP_TICKS (2/3 s) is given up: what
it walks into is no tunnel but a barrier the player crouch jumped (the lip measured over it), and the jump goes. creep_ticks:
under 0 no crouched walk */
#define AE_SQUAD_CREEP_GIVE_UP_TICKS 20
#define AE_SQUAD_CREEP_GAIN 0.3f
int ae_bots_squad_creep_gave_up(long creep_ticks, float moved);

/* RUN-UP at a barrier on the player's track: a member stopped against it backs off AE_SQUAD_RUNUP_BACK_TICKS, then runs at it
and jumps once AE_SQUAD_RUNUP_SPEED fast or AE_SQUAD_RUNUP_JUMP_TICKS into the run, pressing on and crouching in the air (the
player's crouch jump); the run ends AE_SQUAD_RUNUP_RUN_TICKS after it began */
#define AE_SQUAD_RUNUP_BACK_TICKS 12
#define AE_SQUAD_RUNUP_RUN_TICKS 30
#define AE_SQUAD_RUNUP_JUMP_TICKS 10
#define AE_SQUAD_RUNUP_SPEED 1.5f
enum { AE_SQUAD_RUNUP_DONE, AE_SQUAD_RUNUP_BACK, AE_SQUAD_RUNUP_RUN };
int ae_bots_squad_runup_phase(long ticks_since_start);
int ae_bots_squad_runup_jump(float speed, long run_ticks);
/* whether a member keeps its last goal on the player's track: the new one AE_SQUAD_GOAL_BACK or more farther from the player (a
crumb behind it: walking back would undo its way) */
#define AE_SQUAD_GOAL_BACK 0.5f
int ae_bots_squad_goal_keeps(float new_to_player, float old_to_player);
/* whether the squad stacks at a door on its own, shut or open */
int ae_bots_squad_door_auto(int every_door, long player_still_ticks, float player_to_door, int locked);
/* whether a door is locked: still shut (its position near 0) this long after the first man came into its reach */
int ae_bots_squad_door_locked(long ticks_first_in_radius, float door_position);

/* TEAM LAYER: the squad's state over its order. FORMED: every member in position (ae_bots_squad_in_position: under a FOLLOW within
AE_SQUAD_IN_POSITION_FOLLOW of its spot, where the walk to it stops; under a REGROUP within AE_SQUAD_IN_POSITION_REGROUP);
MOVING: the player walks; GATHERING: members on their way to their spots; REGROUPING: a REGROUP until every member is in, given
up after AE_SQUAD_REGROUP_TIMEOUT_TICKS (10 s); HOLDING: a point order (DEFEND, WATCH, GOING, ATTACK). A member out of position
for AE_SQUAD_KEEP_UP_TICKS (30 s) says it is catching up, once every 30 s */
enum { AE_SQUAD_TEAM_NONE, AE_SQUAD_TEAM_FORMED, AE_SQUAD_TEAM_MOVING, AE_SQUAD_TEAM_GATHERING, AE_SQUAD_TEAM_REGROUPING,
	AE_SQUAD_TEAM_HOLDING };
enum { AE_SQUAD_ORDER_FOLLOW, AE_SQUAD_ORDER_REGROUP, AE_SQUAD_ORDER_HOLD };
#define AE_SQUAD_IN_POSITION_FOLLOW 0.8f
#define AE_SQUAD_IN_POSITION_REGROUP 0.8f
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
/* (a vehicle whose up leans past this is upturned: no seat is taken, a member within AE_SQUAD_MOUNT_FLIP of it rights it) */
#define AE_SQUAD_MOUNT_UPRIGHT 0.4f
#define AE_SQUAD_MOUNT_FLIP 3.0f
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
/* whether a member takes a foe at a bearing (radians, off its aim) for a target: on foot one inside half its field of view, or
within AE_SQUAD_NEAR_VIEW units, or that hurt it, or the one it fights, or the one the player called out. A seated gunner takes one at
any bearing: its vehicle turns under it, and its turret turns all the way round */
#define AE_SQUAD_NEAR_VIEW 8.0f
int ae_bots_squad_foe_in_view(float bearing, float half_view, float distance, int hurt, int current, int focused, int seated_gunner);
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
/* WATCH WHILE MOVING: indoors out of a fight a member watches an opening (a doorway, a side room, the corridor on) instead of
the way it walks. AE_SQUAD_WATCH_PROBES level rays from its eye, AE_SQUAD_WATCH_RANGE long, every AE_SQUAD_WATCH_TICKS; an opening is
a ray AE_SQUAD_WATCH_OPEN or longer and no shorter than either neighbour; none more than AE_SQUAD_WATCH_BEHIND off its way (its
walk, or back down the player's track for the last member when the player stands), none within AE_SQUAD_WATCH_ARC of where the
player looks or of a mate's opening; of the rest the one nearest its way, the longer on a tie. lengths[i] is probe i's ray at
first_yaw + i turns of a full circle / count; taken holds the mates' watched yaws; the probe picked, -1 none */
#define AE_SQUAD_WATCH_PROBES 12
#define AE_SQUAD_WATCH_RANGE 6.0f
#define AE_SQUAD_WATCH_OPEN 2.0f
#define AE_SQUAD_WATCH_ARC 0.61f
#define AE_SQUAD_WATCH_BEHIND 2.36f
#define AE_SQUAD_WATCH_TICKS 15
int ae_bots_squad_watch_pick(int count, float const *lengths, float first_yaw, float way_yaw, float player_yaw, float const *taken,
	int taken_count);

/* THE SQUAD'S LOG: its counters since the level's start, written once a second as four short lines, "squad <group>: name value
...": path (searches reached / partial / failed, track fallbacks, door keep-out steps, edge holds, unsticks), slots (re-plans,
swaps while the player stands, mode switches, ticks in the player's way, ticks blocking him, oscillations, the closest pair), warps
(one per reason) and cost (rays in the busiest second, the busiest tick's milliseconds, grid points, grid ticks, the ticks' 99th
percentile). A distance or milliseconds count is kept in hundredths and written as x.xx; the cost's are since the squad's last
reset (a test's setup) */
enum ae_squad_stat
{
	AE_SQUAD_STAT_REACHED, AE_SQUAD_STAT_PARTIAL, AE_SQUAD_STAT_FAILED, AE_SQUAD_STAT_TRACK,
	AE_SQUAD_STAT_KEEPOUT, AE_SQUAD_STAT_EDGE, AE_SQUAD_STAT_UNSTICK,
	AE_SQUAD_STAT_REPLANS, AE_SQUAD_STAT_SWAPS, AE_SQUAD_STAT_MODES, AE_SQUAD_STAT_STRIP, AE_SQUAD_STAT_BLOCKED,
	AE_SQUAD_STAT_OSCILLATIONS, AE_SQUAD_STAT_CLOSEST_PAIR,
	AE_SQUAD_STAT_WARP_STUCK, AE_SQUAD_STAT_WARP_CATCH_UP, AE_SQUAD_STAT_WARP_STRANDED, AE_SQUAD_STAT_WARP_TELEPORT,
	AE_SQUAD_STAT_WARP_LIFT, AE_SQUAD_STAT_WARP_DROP, AE_SQUAD_STAT_WARP_LOCKED,
	AE_SQUAD_STAT_RAYS, AE_SQUAD_STAT_MS_MAX, AE_SQUAD_STAT_GRID_POINTS, AE_SQUAD_STAT_GRID_TICKS, AE_SQUAD_STAT_MS_P99,
	AE_SQUAD_STAT_COUNT
};
/* the group a counter is written in: 0 path, 1 slots, 2 warps, 3 cost */
int ae_bots_squad_stat_group(int stat);
/* the line of a group ("path", "slots", "warps", "cost") into out; its length, 0 for an unknown group */
int ae_bots_squad_stats_line(char *out, int size, char const *group, long const *values);

/* THE SQUAD'S PATH DATA: a door closes the searches' walkable surfaces under it only while the level keeps it shut (locked or
without power); one that opens for a body is open to them, so a path goes through a doorway. A failed search never learns a
machine in the way (a door, a pod); scenery it learns for AE_SQUAD_LEARN_EXPIRY_TICKS (10 s), then its surfaces open again */
#define AE_SQUAD_LEARN_EXPIRY_TICKS 300
int ae_bots_squad_door_closes_paths(int is_door, int locked);
/* whether a failed search may learn the object in the way: never a machine; scenery until *expires (game ticks) */
int ae_bots_squad_learn(int is_machine, long now, long *expires);
/* what a closure record wants now: 1 close its surfaces, -1 open them again, 0 nothing (a door by its lock; scenery by its
time, expires 0 for never) */
int ae_bots_squad_closure_due(int door, int locked, int closed, long expires, long now);

/* A SQUAD WALK: a path the search gives is FAILED when it found no surface at either end, ends more than AE_SQUAD_PATH_END_OFF from the
goal and more than half the straight way (reached or not: a goal beside a shut door is on a surface closed to the searches) or is more than AE_SQUAD_PATH_DETOUR times the straight way (that at least AE_SQUAD_PATH_STRAIGHT_LEAST);
a failed path sends the member along the player's track. A member arrives within AE_SQUAD_ARRIVE of its goal and stands until the goal is
more than AE_SQUAD_SET_OFF away; its goal moves when the spot moved AE_SQUAD_GOAL_MOVES (a storey up or down, more than
AE_SQUAD_STOREY, counts AE_SQUAD_STOREY_WEIGHT times); it searches again when the goal moved AE_SQUAD_RESEARCH or into another cluster */
enum { AE_SQUAD_PATH_OK, AE_SQUAD_PATH_FAILED };
#define AE_SQUAD_PATH_END_OFF 0.5f
#define AE_SQUAD_PATH_DETOUR 3.0f
#define AE_SQUAD_PATH_STRAIGHT_LEAST 0.5f
#define AE_SQUAD_ARRIVE 0.35f
#define AE_SQUAD_SET_OFF 0.8f
#define AE_SQUAD_GOAL_MOVES 0.05f
#define AE_SQUAD_STOREY 0.3f
#define AE_SQUAD_STOREY_WEIGHT 10.0f
#define AE_SQUAD_RESEARCH 0.5f
int ae_bots_squad_path_verdict(int reached, float end_to_goal, float path_length, float straight, int surfaces_found);
int ae_bots_squad_walk_state(int walking, float to_goal);
float ae_bots_squad_goal_shift(float dx, float dy, float dz);
int ae_bots_squad_goal_moves(float shift);
int ae_bots_squad_research(float goal_moved, int cluster_changed);

/* THE WALKABLE GRID: points AE_SQUAD_GRID_STEP apart flooded out from a centre (the player's feet): a neighbour joins when its
floor is within AE_SQUAD_GRID_RISE up or down and the line between them at knee height (AE_SQUAD_GRID_KNEE) is clear; each point
keeps its flood distance (the walk, not the straight line). A point's walkability is kept by its cell's key (its bsp, x and y
by the step, z by AE_SQUAD_GRID_STOREY) */
#define AE_SQUAD_GRID_STEP 0.25f
#define AE_SQUAD_GRID_RISE 0.3f
#define AE_SQUAD_GRID_KNEE 0.3f
#define AE_SQUAD_GRID_CLEAR 0.25f
#define AE_SQUAD_GRID_STOREY 0.5f
#define AE_SQUAD_GRID_POINTS 512
#define AE_SQUAD_GRID_A_TICK 32
int ae_bots_squad_grid_step_ok(float dz, int knee_clear);
unsigned long ae_bots_squad_grid_key(int bsp, float x, float y, float z);
/* a cell's whole-number coordinates (floor of x / step and so on) */
void ae_bots_squad_grid_cell(float x, float y, float z, long *ix, long *iy, long *iz);

/* FOLLOW SLOTS: under FOLLOW each member takes a slot about the player on the walkable grid. His facing is the way he moves
(faster than AE_SQUAD_MOVING), else his aim once it turned AE_SQUAD_AIM_TURN for AE_SQUAD_AIM_HELD_TICKS. A slot is in the ring
AE_SQUAD_RING_IN-AE_SQUAD_RING_OUT from him; moving, behind him and behind his lead point (the facing's dot with the way to the
slot under -AE_SQUAD_BEHIND); standing, behind him; its walk passes no nearer him than AE_SQUAD_PATH_CLEAR; out of every door's
reach. Its cost: its distance from AE_SQUAD_RING_PREFER, AE_SQUAD_ROOM_COST in another room or off his floor (more than
AE_SQUAD_FLOOR_OFF up or down), AE_SQUAD_THREAT_COST by a known threat. Men take slots by the least total cost (each man's
AE_SQUAD_SLOT_CHOICES cheapest tried together), no two within AE_SQUAD_SLOT_APART, a kept slot AE_SQUAD_SLOT_KEEP cheaper. The
slots are planned again when one fails, or, on the ground, when he moved AE_SQUAD_REPLAN_MOVED and changed room or
AE_SQUAD_REPLAN_TICKS went */
#define AE_SQUAD_SLOTS_MAX AE_SQUAD_GRID_POINTS
#define AE_SQUAD_MOVING 0.5f
#define AE_SQUAD_AIM_TURN 1.047f
#define AE_SQUAD_AIM_HELD_TICKS 30
#define AE_SQUAD_RING_IN 0.7f
#define AE_SQUAD_RING_OUT 1.8f
#define AE_SQUAD_RING_PREFER 1.1f
#define AE_SQUAD_BEHIND 0.3f
#define AE_SQUAD_PATH_CLEAR 0.5f
#define AE_SQUAD_ROOM_COST 4.2f
#define AE_SQUAD_FLOOR_OFF 0.6f
#define AE_SQUAD_THREAT_COST 1.7f
#define AE_SQUAD_SLOT_CHOICES 6
#define AE_SQUAD_SLOT_APART 0.8f
#define AE_SQUAD_SLOT_KEEP 0.3f
#define AE_SQUAD_REPLAN_MOVED 1.0f
#define AE_SQUAD_REPLAN_TICKS 90
float ae_bots_squad_leader_yaw(float vx, float vy, float aim_yaw, float last_yaw, int aim_held_ticks);
int ae_bots_squad_slot_ok(float to_player, float behind_player, float behind_lead, float path_near_player, int in_door_reach,
	int moving);
float ae_bots_squad_slot_cost(float to_player, int other_room, int off_floor, int threat_point_near);
/* cost[man][slot]; where[slot] its x, y; kept[man] the slot it holds (-1 none); slot_of_man[man] (-1 none): 1 when every man has one */
int ae_bots_squad_slot_assign(float const cost[][AE_SQUAD_SLOTS_MAX], float const (*where)[2], int men, int slots,
	int const *kept, int *slot_of_man);
int ae_bots_squad_replan(float moved, int cluster_changed, int ticks_since, int slot_invalid, int airborne);

/* PRECEDENCE: a member is in one mode a tick, the first that applies: in a seat, mounting, fetching, at a door, in cover, on a
deck (or cut off), at its follow slot, on the player's track. A change waits until the new mode was wanted AE_SQUAD_MODE_HOLD_TICKS
(1.5 s), except into a seat, a mount or a door. A step aside (unstick, sidestep, yield, door keep-out) is taken only when no wall
is within AE_SQUAD_OVERRIDE_WALL that way. A heading turned back more than AE_SQUAD_OSCILLATION_TURN within
AE_SQUAD_OSCILLATION_TICKS out of a fight is an oscillation (counted). In single file indoors members keep AE_SQUAD_FILE_GAP apart */
enum { AE_SQUAD_MODE_SEAT, AE_SQUAD_MODE_MOUNT, AE_SQUAD_MODE_FETCH, AE_SQUAD_MODE_DOOR, AE_SQUAD_MODE_COVER, AE_SQUAD_MODE_DECK,
	AE_SQUAD_MODE_SLOT, AE_SQUAD_MODE_TRACK, AE_SQUAD_MODES };
#define AE_SQUAD_MODE_HOLD_TICKS 45
#define AE_SQUAD_OVERRIDE_WALL 0.5f
#define AE_SQUAD_OSCILLATION_TURN 2.618f
#define AE_SQUAD_OSCILLATION_TICKS 30
#define AE_SQUAD_FILE_GAP 0.8f
int ae_bots_squad_mode_pick(int wanted, int current, long held_ticks);
/* wall_ahead: the distance to the first wall that way (large for none) */
int ae_bots_squad_override_ok(float wall_ahead);
int ae_bots_squad_oscillation(float heading_before, float heading_now, long ticks_between, int fighting);

/* MAKE WAY: a member in the player's way (AE_SQUAD_STRIP_AHEAD ahead of him or less, within AE_SQUAD_STRIP_HALF across his way,
AE_SQUAD_STRIP_HALF_HELD when he is held up, moving under half what his stick asks; him moving or pushing faster than
AE_SQUAD_MOVING) steps to the side with AE_SQUAD_WAY_ROOM of room (the roomier), else walks on ahead of him.
FILE REVERSAL: when he turns back more than AE_SQUAD_REVERSAL_DEGREES in a corridor narrower than AE_SQUAD_REVERSAL_WIDTH, the
members on his new way ahead lead: none tries to pass him to get behind */
#define AE_SQUAD_STRIP_AHEAD 1.5f
#define AE_SQUAD_STRIP_HALF 0.3f
#define AE_SQUAD_STRIP_HALF_HELD 0.55f
#define AE_SQUAD_WAY_ROOM 0.6f
#define AE_SQUAD_REVERSAL_DEGREES 120.0f
#define AE_SQUAD_REVERSAL_WIDTH 1.6f
enum { AE_SQUAD_WAY_LEFT, AE_SQUAD_WAY_RIGHT, AE_SQUAD_WAY_AHEAD };
int ae_bots_squad_in_strip(float along, float across, float player_speed, int held);
int ae_bots_squad_make_way(float room_left, float room_right);
/* the side kept (AE_SQUAD_WAY_LEFT, RIGHT or AHEAD) unless the kept side's room fell under AE_SQUAD_WAY_ROOM: then the side
ae_bots_squad_make_way picks now */
int ae_bots_squad_make_way_kept(int kept, float room_left, float room_right);

/* YIELD: a squadmate within AE_SQUAD_YIELD_NEAR ahead of the member's heading (dx, dy, dz from the member, AE_SQUAD_YIELD_RISE in
height), moving (velocity per tick over AE_SQUAD_YIELD_MOVING): walking on away (over half its speed along the heading) the
member slows behind it (AE_SQUAD_YIELD_SLOW), else it backs off (AE_SQUAD_YIELD_BACK); otherwise AE_SQUAD_YIELD_NONE */
#define AE_SQUAD_YIELD_NEAR 1.1f
#define AE_SQUAD_YIELD_RISE 1.5f
#define AE_SQUAD_YIELD_MOVING 0.02f
#define AE_SQUAD_YIELD_THROTTLE 0.4f
enum { AE_SQUAD_YIELD_NONE, AE_SQUAD_YIELD_SLOW, AE_SQUAD_YIELD_BACK };
int ae_bots_squad_yield(float dx, float dy, float dz, float heading, float velocity_x, float velocity_y);
int ae_bots_squad_file_reversal(float turn_degrees, float corridor_width);

/* DOOR KEEP-OUT as a filter: a follow slot, a track step, a stack place or a cover place within any door's reach +
AE_SQUAD_DOOR_KEEP_OUT is out unless the member's walk goes through that door. The player is through a door once
AE_SQUAD_THROUGH past its plane, and back once AE_SQUAD_THROUGH the other way. A stack place in the same eighth of a turn from
the door's side as from the stack point scores AE_SQUAD_SECTOR_SAME. The squad is stacked only with every member within
AE_SQUAD_STACK_IN_PLACE of a place AE_SQUAD_PLACE_REACH or less from the stack point */
#define AE_SQUAD_THROUGH 0.9f
#define AE_SQUAD_SECTOR_SAME (-2)
#define AE_SQUAD_STACK_IN_PLACE 0.5f
int ae_bots_squad_in_any_door_reach(float to_door, float reach, int route_crosses);
int ae_bots_squad_player_through(float past_plane, int was);
int ae_bots_squad_stack_sector(float yaw_to_door_side, float yaw_to_stack_point);
int ae_bots_squad_stacked(int in_place, int members, float farthest_place_to_stack_point);

/* A FIGHT ON NARROW GROUND: indoors where the way is narrow a member fights within AE_SQUAD_FIGHT_NARROW of the player (else the
fireteam's AE_SQUAD_FIGHT_WIDE); it fires only on a line passing AE_SQUAD_FIRE_CLEAR or more from him.
PACE: a member runs when more than AE_SQUAD_PACE_FAR_SLOT from its slot or AE_SQUAD_PACE_FAR_PLAYER from the player; nearer, it
keeps the player's own pace plus AE_SQUAD_PACE_PULL for each unit it is off its arrival, AE_SQUAD_PACE_LEAST at least, none once
arrived (AE_SQUAD_ARRIVE) */
#define AE_SQUAD_FIGHT_NARROW 3.0f
#define AE_SQUAD_FIGHT_WIDE 6.0f
#define AE_SQUAD_FIRE_CLEAR 0.4f
#define AE_SQUAD_PACE_FAR_SLOT 1.35f
#define AE_SQUAD_PACE_FAR_PLAYER 2.7f
#define AE_SQUAD_PACE_PULL 0.5f
#define AE_SQUAD_PACE_LEAST 0.3f
/* a player's run on foot, world units a second (a pace of 1) */
#define AE_SQUAD_PLAYER_RUN 2.2f
float ae_bots_squad_fight_leash(int narrow, int indoors);
int ae_bots_squad_fire_line_ok(float nearest_player);
float ae_bots_squad_throttle(float player_speed, float max_speed, float to_slot, float to_player);

/* ONE AT A TIME: a crumb of the player's track where he jumped is jumped within AE_SQUAD_CUE_JUMP_NEAR of it, not on the way to
it; a member within AE_SQUAD_CROSSING_CLEAR of that crumb takes the crossing, from that crumb to the next one. One member
crosses there at a time: another waits until the one crossing has come within AE_SQUAD_CROSSING_LANDED of the crossing's far
end (measured along it), while it stays within AE_SQUAD_CROSSING_NEAR of the takeoff, AE_SQUAD_CROSSING_TICKS at most */
#define AE_SQUAD_CUE_JUMP_NEAR 0.6f
#define AE_SQUAD_CROSSING_CLEAR 1.0f
#define AE_SQUAD_CROSSING_LANDED 0.3f
#define AE_SQUAD_CROSSING_NEAR 3.0f
#define AE_SQUAD_CROSSING_TICKS 90
int ae_bots_squad_cue_jump_now(float to_crumb);
int ae_bots_squad_crossing_waits(int another_crosses, float crosser_along, float crossing_length, float crosser_to_takeoff,
	long crossing_ticks);

/* THE SQUAD'S COST: each tick's milliseconds counted in AE_SQUAD_COST_BUCKETS tenths of a millisecond (the last for more); the
99th percentile of the ticks counted, in hundredths of a millisecond, 0 for none */
#define AE_SQUAD_COST_BUCKETS 100
long ae_bots_squad_cost_p99(long const *histogram);

/* an oscillation that is not the player's own turn back: none within AE_SQUAD_PLAYER_TURN_TICKS of him turning back */
#define AE_SQUAD_PLAYER_TURN_TICKS 90
int ae_bots_squad_oscillation_after(float heading_before, float heading_now, long ticks_between, int fighting,
	long ticks_since_player_turned);

/* AN ORDER'S POINT (the campaign squad: ATTACK, DEFEND, WATCH and MOVE stand until the player's next order; the member is off the
player's leash). One member is the POINT MAN (the one nearest the point when the order came): its place is the point itself. The
others have places in a wedge behind it, AE_SQUAD_ORDER_SPACING apart (ae_bots_squad_order_offset: rank 0 the point man, 1 back and
left, 2 back and right, 3 behind; dx, dy for a way of advance in radians), and on the way they walk to those places about the point
man as he is, so the squad moves as one; the point man stands for the others while one is more than AE_SQUAD_ORDER_STRING behind
its place, AE_SQUAD_ORDER_WAIT_TICKS an order at most (ae_bots_squad_order_waits). Out of a fight it walks to its place and stands there
(AE_SQUAD_ORDER_ARRIVE); in a fight it fights inside the order's radius about its place (AE_SQUAD_ORDER_RADIUS, an ATTACK's
AE_SQUAD_ORDER_ATTACK_RADIUS: its own moves, cover to cover) and walks back in, shooting, when it is outside it */
#define AE_SQUAD_ORDER_SPACING 1.4f
#define AE_SQUAD_ORDER_ARRIVE 0.6f
/* (a member the walk leaves standing, or with no way on, this near its place is at it) */
#define AE_SQUAD_ORDER_NEAR 1.5f
#define AE_SQUAD_ORDER_RADIUS 6.0f
#define AE_SQUAD_ORDER_ATTACK_RADIUS 10.0f
enum { AE_SQUAD_ORDER_STAND, AE_SQUAD_ORDER_WALK, AE_SQUAD_ORDER_FIGHT };
#define AE_SQUAD_ORDER_STRING 7.0f
#define AE_SQUAD_ORDER_WAIT_TICKS 240
void ae_bots_squad_order_offset(int rank, float way, float *dx, float *dy);
int ae_bots_squad_order_waits(float worst_behind, long waited_ticks, int fighting);
float ae_bots_squad_order_radius(int attack);
/* walking: the member was walking to its place last tick (it stops inside half the arrive distance, starts past the whole) */
int ae_bots_squad_order_step(int fighting, float to_place, float radius, int walking);
/* whether a member is on the player's leash (the pull to the player: the track, the cut-off walk, the catch-up warps): only with
no point order and no vehicle it was sent to */
int ae_bots_squad_on_leash(int point_order, int sent_to_vehicle);

/* THE WARP, LAST: a member's standing still is a stall only while its walk to the player failed lately, it is cut off or it walks
his track; never while it holds a cover place or stands at its slot, which it does on purpose, nor on an order away from the
player (go here, attack, defend, watch), whose walk a warp to him would undo */
int ae_bots_squad_stalled(int walk_failed_lately, int cut_off, int on_track, int in_cover, int at_slot, int on_order);

/* REPORT: a line of the squad's counts is written when its text differs from the one last written, when
AE_SQUAD_REPORT_QUIET_TICKS (30 s) passed since then, or when the game's time is before it (a new level); last_tick under 0: never
written */
#define AE_SQUAD_REPORT_QUIET_TICKS 900
int ae_bots_squad_report_due(char const *last, char const *text, long last_tick, long now);

/* OUTCOME, a member's own numbers for the test lists: its gap to the player each tick counted in AE_SQUAD_GAP_BUCKETS half
units (the last for more), whose 90th percentile is the bucket's upper edge in hundredths of a unit (0 with none); a tick
stuck is one it wants to walk, slower than AE_SQUAD_STUCK_SPEED (units a second), out of a fight */
#define AE_SQUAD_GAP_BUCKETS 64
#define AE_SQUAD_STUCK_SPEED 0.3f
long ae_bots_squad_gap_p90(long const *histogram);
int ae_bots_squad_stuck_tick(int wants_move, float speed, int fighting);
/* a member at a lip (run-ups, crouched walks) AE_SQUAD_LIP_PATIENCE_TICKS (12 s) with its way to the player not shrinking:
its walk to and fro and its standing are no progress, so THE WARP, LAST takes it to the player (a ledge it cannot jump);
lip_ticks: under 0 none */
#define AE_SQUAD_LIP_PATIENCE_TICKS 360
int ae_bots_squad_lip_patience_over(long lip_ticks);
/* a door for the squad: a machine of the door kind, or a platform the level names a door (its tag's name has "door" in it: a10
builds its doors as platforms); a lift or a gear is none */
#define AE_SQUAD_MACHINE_DOOR 0
#define AE_SQUAD_MACHINE_PLATFORM 1
int ae_bots_squad_machine_is_door(int kind, char const *tag_name);
/* a track crumb in a door's reach that the member gained nothing on (AE_SQUAD_CRUMB_GAIN closer) for AE_SQUAD_CRUMB_STALL_TICKS
(2 s) is one it cannot reach (a door it cannot pass): it takes the next crumb on; never a crumb the player jumped at, nor one on
open ground or a barrier's top (the crossing's own rules) */
#define AE_SQUAD_CRUMB_STALL_TICKS 60
#define AE_SQUAD_CRUMB_GAIN 0.2f
int ae_bots_squad_crumb_unreachable(int jump_cue, int at_door, long ticks_no_gain);
/* LINE UP: a member that gained nothing on a track crumb for AE_SQUAD_LINEUP_TICKS (not one the player jumped at) and stands more
than AE_SQUAD_LINEUP_OFF off the player's line to it (from the crumb before it, AE_SQUAD_LINEUP_LEAST long or more) walks first to
the point on that line AE_SQUAD_LINEUP_BACK before the crumb (x, y), for AE_SQUAD_LINEUP_MOST_TICKS at most, then on to the
crumb; once a crumb. 1 when it lines up */
#define AE_SQUAD_LINEUP_TICKS 30
#define AE_SQUAD_LINEUP_MOST_TICKS 45
#define AE_SQUAD_LINEUP_OFF 0.25f
#define AE_SQUAD_LINEUP_LEAST 0.3f
#define AE_SQUAD_LINEUP_BACK 0.6f
int ae_bots_squad_lineup_point(float from_x, float from_y, float crumb_x, float crumb_y, float at_x, float at_y, float *x, float *y);

/* AUTOMATIC FORMATIONS: the squad's formation picked from what the player does and the width of the space about him, in order: a
fight, an entry through a door, a stack at one, a halt (he stood AE_SQUAD_FORMATION_HALT_TICKS), then by the width: under
AE_SQUAD_FILE_WIDE a file, under AE_SQUAD_STAGGER_WIDE a tight stagger, else a stagger (indoors) or a wedge (outdoors). A new pick
is taken once it held AE_SQUAD_FORMATION_HOLD_TICKS; a fight, an entry and a stack at once */
enum
{
	AE_SQUAD_FORMATION_FIGHT,
	AE_SQUAD_FORMATION_ENTRY,
	AE_SQUAD_FORMATION_STACK,
	AE_SQUAD_FORMATION_HALT,
	AE_SQUAD_FORMATION_FILE,
	AE_SQUAD_FORMATION_TIGHT,
	AE_SQUAD_FORMATION_STAGGER,
	AE_SQUAD_FORMATION_WEDGE,
	AE_SQUAD_FORMATIONS
};
#define AE_SQUAD_FORMATION_HALT_TICKS 60
#define AE_SQUAD_FORMATION_HOLD_TICKS 30
#define AE_SQUAD_FILE_WIDE 1.5f
#define AE_SQUAD_STAGGER_WIDE 3.0f
int ae_bots_squad_formation_pick(int fighting, int entering, int stacking, long still_ticks, float width, int outdoors);
/* AUTOMATIC FORMATIONS OFF (game.companions_formations false): the squad makes no formation of its own. Under a FOLLOW the
picked formation is none (AE_SQUAD_FORMATIONS: the follow ring); DOOR STACK's EVERY DOOR is taken as ON ORDER (a stack only at a
door the player marks); under an order each member walks to its own place: the others do not keep theirs about the point man,
and he does not stand for them */
int ae_bots_squad_formation_wanted(int formations_on, int picked);
int ae_bots_squad_door_every(int formations_on, int every_setting);
int ae_bots_squad_moves_as_one(int formations_on);
int ae_bots_squad_formation_hold(int current, int wanted, long wanted_ticks);
/* a formation's spot for the member of a rank (0 to 2, entry order): along, behind the player on his heading; across, to his left
(+) or right (-); kept AE_SQUAD_SIDE_ROOM inside the width's walls. A halt's spots are AE_SQUAD_HALT_NEAR or more from him, a
file's AE_SQUAD_FILE_APART apart on his line, a tight stagger's and a stagger's two on both sides AE_SQUAD_TIGHT_APART or
AE_SQUAD_STAGGER_APART behind him and the third twice that on his line, a wedge's on his rear quarters AE_SQUAD_WEDGE_OUT out; a fight, an entry or a stack have
none (0) */
#define AE_SQUAD_HALT_NEAR 1.5f
#define AE_SQUAD_FILE_APART 1.2f
#define AE_SQUAD_TIGHT_APART 1.0f
#define AE_SQUAD_STAGGER_APART 1.5f
#define AE_SQUAD_WEDGE_OUT 2.0f
#define AE_SQUAD_SIDE_ROOM 0.4f
int ae_bots_squad_formation_spot(int formation, int rank, float width, float *along, float *across);
char const *ae_bots_squad_formation_name(int formation);
/* the slots' ring under a formation: a halt's from AE_SQUAD_HALT_NEAR less 0.2; out to its farthest spot (in a wide space) and 0.6;
a formation without spots keeps the follow ring */
void ae_bots_squad_formation_ring(int formation, float *ring_in, float *ring_out);
/* the follow slot's test (ae_bots_squad_slot_ok) with the ring given */
int ae_bots_squad_slot_ok_ring(float to_player, float behind_player, float behind_lead, float path_near_player, int in_door_reach,
	int moving, float ring_in, float ring_out);

#endif
