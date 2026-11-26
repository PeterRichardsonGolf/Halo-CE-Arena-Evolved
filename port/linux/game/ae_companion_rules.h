/*
AE_COMPANION_RULES.H

The AI squad's entry into a campaign level (ae_companion.c), as rules with no game state.

ENTRY: the squad is not in the level's intro. Each member appears once the human player has control and is on foot,
behind them (out of their view), on walkable ground reached from them in a clear line. The candidates are tried in
order, nearest first; a member's own side of the player first (odd slots left, even right).

INTRO RIDE: on a level of the allow-list the squad rides the intro's vehicle in its seats instead, and gets out to the
ground beside it with the player. The list holds the levels whose intro the sweep passes; a DEV/config switch forces
the ride on any level, for the sweep itself.
*/

#ifndef AE_COMPANION_RULES_H
#define AE_COMPANION_RULES_H

#define AE_COMPANION_ENTRY_CANDIDATES 8
/* the floor under a spot: at most this steep (its normal's up part), within this height of the player's feet, and with
this much room above it */
#define AE_COMPANION_ENTRY_FLOOR_UP 0.7f
#define AE_COMPANION_ENTRY_FLOOR_RISE 1.5f
#define AE_COMPANION_ENTRY_HEADROOM 1.2f
/* a spot this near another player's body is taken (the squad's rest spacing, ae_bots_squad_rules.h) */
#define AE_COMPANION_ENTRY_APART 1.3f

/* the candidate spots for a member (its slot 0..3) as offsets from the player's feet along the ground, for a player
facing (facing_i, facing_j) (any length; none: along x): writes up to AE_COMPANION_ENTRY_CANDIDATES pairs, returns the
count */
int ae_companion_entry_candidates(float facing_i, float facing_j, int slot, float offsets[][2]);
/* whether an offset from the player lies out of their view: more than 100 degrees from where they face */
int ae_companion_entry_unseen(float facing_i, float facing_j, float offset_i, float offset_j);
/* whether a floor is one to stand on: its normal's up part, its height and the player's, the room above it */
int ae_companion_entry_floor(float normal_k, float floor_z, float player_z, float headroom);

/* CATCH-UP, the last resort: a member far from the player (past AE_COMPANION_CATCH_UP_FAR) that has come no nearer for
AE_COMPANION_CATCH_UP_TICKS (no walk or jump takes it there), and that the player does not see, is put behind them as at its
entry; never a member in a seat. distance from the player, ticks_without_gain since it last came 1.5 nearer, unseen whether
the player cannot see it, seated whether it sits in a vehicle */
#define AE_COMPANION_CATCH_UP_FAR 20.0f
#define AE_COMPANION_CATCH_UP_TICKS 240
#define AE_COMPANION_CATCH_UP_GAIN 1.5f
int ae_companion_catch_up(float distance, long ticks_without_gain, int unseen, int seated);
/* STUCK INDOORS, the last resort under a roof: a member cut off from the player by a barrier, or farther than
AE_COMPANION_STUCK_FAR, that has not come AE_COMPANION_STUCK_GAIN nearer for AE_COMPANION_STUCK_TICKS while the
player does not see it (AE_COMPANION_STUCK_SEEN_TICKS while they do), is put behind the player as at its entry; never one
nearer than AE_COMPANION_STUCK_NEAR, in a seat or in a fight */
#define AE_COMPANION_STUCK_NEAR 2.5f
#define AE_COMPANION_STUCK_FAR 6.0f
#define AE_COMPANION_STUCK_GAIN 1.0f
#define AE_COMPANION_STUCK_TICKS 150
#define AE_COMPANION_STUCK_SEEN_TICKS 300
int ae_companion_stuck_warp(int indoors, int cut_off, float distance, long ticks_stuck, int unseen, int seated, int fighting);
/* the stuck window starts again (TRUE) when the member is no longer under a roof, is within AE_COMPANION_STUCK_NEAR, was neither
cut off in this window (cut_off_seen) nor is past AE_COMPANION_STUCK_FAR, or is AE_COMPANION_STUCK_GAIN nearer than the nearest it
came in the window (window_nearest); a member hopping in place against a barrier keeps its window */
int ae_companion_stuck_restarts(int indoors, int cut_off_seen, float distance, float window_nearest);

/* THE PLAYER'S TRACK: the spots the human walked (a crumb each time they are AE_COMPANION_TRACK_STEP from the last, on their
feet, oldest first, AE_COMPANION_TRACK_MOST kept). On an object's ground (a bridge deck, a lift) a member walks the track in
single file: from the crumb nearest it (within AE_COMPANION_TRACK_ON in its plane, AE_COMPANION_TRACK_RISE in height; a newer one on a
later stretch of the track, no more than AE_COMPANION_TRACK_NEWER farther, wins) to the
next newer one once within AE_COMPANION_TRACK_REACH of it. track holds count crumbs (x, y, z): the crumb to walk to, -1 when
the member is not on the track (it then goes straight to the player) */
#define AE_COMPANION_TRACK_MOST 48
#define AE_COMPANION_TRACK_STEP 0.75f
#define AE_COMPANION_TRACK_ON 3.0f
#define AE_COMPANION_TRACK_RISE 1.5f
#define AE_COMPANION_TRACK_REACH 0.9f
#define AE_COMPANION_TRACK_NEWER 0.4f
int ae_companion_track_step(float const track[][3], int count, float x, float y, float z);
/* the same from crumb least on: a member never goes back to a crumb older than the one it last walked from (a track that
doubles back passes its older crumbs again) */
int ae_companion_track_step_from(float const track[][3], int count, float x, float y, float z, int least);
/* the track's cues, a byte a crumb (flags, beside track): JUMPED, the human left the ground after this crumb (a barrier jumped);
CROUCHED, the human crouched at it. A crumb is added (1) once the human is AE_COMPANION_TRACK_STEP from the last; a step of more
than AE_COMPANION_TRACK_GAP or a rise of AE_COMPANION_TRACK_RISE starts the track again; full, the oldest goes. airborne_since:
the human was in the air since the last crumb (the last one is marked JUMPED); 0 when no crumb was added */
#define AE_COMPANION_TRACK_JUMPED 1
#define AE_COMPANION_TRACK_CROUCHED 2
/* a crumb the player dropped from: the track goes on down a drop of AE_COMPANION_TRACK_DROP_KEPT or less (one a member may take) */
#define AE_COMPANION_TRACK_DROPPED 4
#define AE_COMPANION_TRACK_DROP_KEPT 3.0f
#define AE_COMPANION_TRACK_GAP 4.0f
int ae_companion_track_push(float track[][3], unsigned char flags[], int *count, float x, float y, float z, int airborne_since,
	int crouched);
/* what a member walking to crumb goal (ae_companion_track_step) does there: the jump of the crumb it just reached (goal - 1,
within AE_COMPANION_TRACK_CUE of it in plan), the crouch of that crumb or of the goal */
#define AE_COMPANION_TRACK_CUE 1.2f
int ae_companion_track_cue(float const track[][3], unsigned char const flags[], int count, int goal, float x, float y);
/* the crumb an entry (or a catch-up) on an object's ground puts a member at: the newest crumb at least AE_COMPANION_TRACK_BEHIND
from the player (px, py) along the track, skipping `taken` of them already used; -1 none */
#define AE_COMPANION_TRACK_BEHIND 2.5f
int ae_companion_track_behind(float const track[][3], int count, float px, float py, int taken);
/* NARROW GROUND (a bridge deck, a ledge, a beam): the floor falls away (no floor within AE_COMPANION_NARROW_DROP below) a step
out (AE_COMPANION_NARROW_PROBE) on two opposite sides. drops: the eight directions, 45 degrees apart, from +x turning left,
1 where it falls away. On narrow ground (or an object's) the squad keeps to the player's track in single file, and an entry
takes one of the deck spots below */
#define AE_COMPANION_NARROW_PROBE 1.2f
#define AE_COMPANION_NARROW_DROP 1.0f
int ae_companion_narrow(int const drops[8]);
/* the entry's spots on narrow ground: 1.5 and 2.5 out in each of the eight directions, the ones furthest behind the player
(facing (facing_i, facing_j)) first; writes AE_COMPANION_DECK_CANDIDATES pairs, returns the count. A deck spot's floor is
within AE_COMPANION_DECK_RISE of the player's feet */
#define AE_COMPANION_DECK_CANDIDATES 16
#define AE_COMPANION_DECK_RISE 0.5f
int ae_companion_deck_candidates(float facing_i, float facing_j, float offsets[][2]);

/* whether an entry spot's floor will hold the member: with the player on an object's ground, only a floor that is an object
(the structure under a bridge deck lies below it, and a member put there falls through the deck) */
int ae_companion_entry_floor_kind(int player_on_object, int floor_is_object);

/* whether the squad rides the intro of a level (its scenario's name, the path's last part: "b30"); forced: the switch
that rides every level's intro */
int ae_companion_intro_ride(char const *level, int forced);

/* WHERE A SQUAD MAY BE: only in a campaign game this machine hosts (a solo scenario, the game's AI running, no game engine
and so no multiplayer game, not the menus, not a client). A multiplayer lobby, whose engine has not started yet, is not one */
int ae_companion_may_ask(int scenario_solo, int ai_ready, int engine_running, int menu_active, int network_client, int has_server,
	int dedicated_server);
/* the squad's count for the game: a test's or a level start's count for the next level (next_level, -1 none) while the menu's
AI SQUAD (setting) is still what it was when that count was given (next_level_setting), else a DEV MODE count (override, -1
none), else the menu's; 0 to most */
long ae_companion_wanted_count(long next_level, long next_level_setting, long override, long setting, long most);
/* a player leaves the squad's game only when the squad added it itself and no longer wants it (a bot of the same machine slot
the squad did not add is never removed) */
int ae_companion_may_remove(int squad_added_it, int wanted);
/* a squad player of the network game with no player of the game for AE_COMPANION_REJOIN_TICKS (a checkpoint revert took it
away) is made again */
#define AE_COMPANION_REJOIN_TICKS 60
int ae_companion_rejoin_wanted(int no_player, long missing_ticks);
/* PLAYER DOWN (game.companions_player_down): whether a living player keeps the game going and is someone a downed player comes
back beside. CHECKPOINT (the default): the humans only, so with every human down the level goes back to its checkpoint;
RESPAWN: the squad's members too */
int ae_companion_player_down_counts(int is_squad_member, int respawn_mode);
/* the setting's word: 1 for "respawn", 0 for anything else ("checkpoint", the default) */
int ae_companion_player_down_respawn(char const *word);

/* THE WARP, LAST: a member is put beside the player only when its walk to him failed and the track made no progress (the path
left shrinking AE_COMPANION_PROGRESS, or the crumb it walks to moving on) for AE_COMPANION_WARP_UNSEEN_TICKS unseen or
AE_COMPANION_WARP_SEEN_TICKS seen; at once, unseen, for a way no walk can take (the player teleported, a lift left, a door shut
for good between them, a drop over the edge rule), with the reason */
#define AE_COMPANION_PROGRESS 0.3f
#define AE_COMPANION_WARP_UNSEEN_TICKS 150
#define AE_COMPANION_WARP_SEEN_TICKS 300
enum { AE_COMPANION_WARP_NONE, AE_COMPANION_WARP_TELEPORT, AE_COMPANION_WARP_LIFT, AE_COMPANION_WARP_LOCKED, AE_COMPANION_WARP_DROP };
int ae_companion_progress(float path_left_was, float path_left_now, int crumb_was, int crumb_now);
/* progress too: a walk of AE_COMPANION_WALKED from where the member last made progress (a goal moving as fast as it walks: the
player walking on, it behind him) */
#define AE_COMPANION_WALKED 1.0f
int ae_companion_walked(float moved);
int ae_companion_warp_due(int path_failed, int track_progress, long ticks_no_progress, int seen);
int ae_companion_warp_now(int teleported, int lift_gone, int door_locked_between, int drop_over_rule);
/* TRAIL LINKS: a step that starts the track again (AE_COMPANION_TRACK_GAP across, AE_COMPANION_TRACK_RISE up or down) is kept as
a link from the old trail's end to the new one's start: a drop, a rise, a teleport (or a vehicle's ride). A drop the player took
with no damage may be followed to its height + AE_COMPANION_DROP_SPARE; a teleport is a move of more than AE_COMPANION_TELEPORT in a
tick */
#define AE_COMPANION_DROP_SPARE 0.2f
#define AE_COMPANION_TELEPORT 4.0f
enum { AE_COMPANION_LINK_NONE, AE_COMPANION_LINK_DROP, AE_COMPANION_LINK_RISE, AE_COMPANION_LINK_TELEPORT, AE_COMPANION_LINK_VEHICLE };
int ae_companion_link_kind(float across, float up, int in_vehicle);
int ae_companion_drop_allowed(float link_drop, int player_hurt, float drop);
int ae_companion_teleported(float moved_in_a_tick);
/* a member farther than AE_COMPANION_WARP_NEAR from the player may be warped: nearer it is with him, however long it stands */
#define AE_COMPANION_WARP_NEAR 4.0f
int ae_companion_warp_far(float distance);

#endif
