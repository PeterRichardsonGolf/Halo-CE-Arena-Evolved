/*
AE_BOMB_ASSAULT.H

Arena Evolved's Bomb rules (ae_bomb_assault.c), on ChupathingyCE's Bomb
(game_engine_bomb.c), for a Bomb game that is not melee only (Grifball keeps
Chupa's):

- FLAG SPAWNS (game.bomb_flag_spawns, ON): the bomb spawns at one team's flag
  marker, and that team attacks: the other team can't pick it up. The teams
  swap each round (each score when a score doesn't start a new round), red
  first. Off: Chupa's bomb halfway between the goals, anyone's.
- DEFUSE (game.bomb_defuse_time, 3 s): a defender stands on the planted bomb
  this long to save it (Chupa's: a touch); leaving, or dying, starts it over.
- FUSE (game.bomb_fuse_time, 5 s): the planted bomb goes off this long after
  it is planted (Chupa's game.bomb_arm_time, 1 s, stays for Grifball).

The host decides (it places the bomb, approves pickups, saves and scores);
nothing new goes over the network.
*/

#ifndef __AE_BOMB_ASSAULT_H
#define __AE_BOMB_ASSAULT_H

/* AE_BOMB_SND_FLAG: the Bomb variant's SEARCH AND DESTROY bit (flags bit 7) */
#include "ae_bomb_snd_rules.h"

/* the team that attacks now (0 red, 1 blue): the bomb is theirs; NONE when the rule is off (or no Bomb game runs) */
long ae_bomb_assault_attacking_team(void);

/* game_engine_bomb.c's bomb_reset: where the bomb goes back to, the attacking team's flag marker; FALSE: the
game's own spawn */
boolean ae_bomb_assault_spawn(union real_point3d *point);

/* game_engine_bomb.c's pickup rule: whether the unit (a player's) may pick up the bomb; FALSE for a defender */
boolean ae_bomb_assault_may_carry(long unit_index);

/* game_engine_bomb.c's bomb_plant: the planted bomb's fuse in seconds (chupa_seconds: game.bomb_arm_time's) */
double ae_bomb_assault_fuse_time(double chupa_seconds);

/* game_engine_bomb.c's bomb_plant_update, each tick a living defender is on the planted bomb (each defender's
call counts once a tick): whether the bomb is saved now */
boolean ae_bomb_assault_defused(long player_index);
/* game_engine_bomb.c's bomb_engine_weapon_update: whether a loose bomb resets after the gametype's untouched time
(FALSE in SEARCH AND DESTROY: a dropped bomb lies where it fell for anyone to pick up); and whether a loose bomb is
out of reach there (in a pit: ae_bomb_snd_rules.h), so it resets at once. The host's */
boolean ae_bomb_assault_untouched_resets(void);
boolean ae_bomb_assault_out_of_reach(long weapon_index);
/* another player's defuse under way and that player still alive within radius of the bomb: this one waits (one
defuser at a time, never faster with more) */
boolean ae_bomb_assault_defuse_held_by_other(long player_index, union real_point3d const *bomb_position, real radius);
/* game_engine_bomb.c, a round's start: whether the announcer says "play ball" (Grifball's; AE's Bomb has no
ball); and a grab: whether the grab's tone plays (Grifball's; AE's bomb spawns at the attackers' flag, so it was a
gong at every round's start) */
boolean ae_bomb_assault_play_ball(void);
/* SEARCH AND DESTROY (the gametype's: the Bomb variant's AE_BOMB_SND_FLAG, the seeded AE SND's; or config
game.bomb_snd, OFF, a test override for any Bomb game; a Bomb game not melee only, with FLAG SPAWNS), classic
competitive rules on Bomb's FLAG SPAWNS: one life a round (the dead wait for the next); a round goes to the attackers when the bomb goes off or
every defender is dead, to the defenders when it is defused, every attacker is dead before it is planted, or the round's
time (game.bomb_snd_round_time, 150 s) runs out with none planted; the plant game.bomb_snd_plant_time (5 s), the
planted bomb's fuse game.bomb_snd_fuse_time (45 s) and its defuse game.bomb_snd_defuse_time (5 s); the sides swap
every game.bomb_snd_round_switch rounds (3), and game.bomb_snd_rounds_to_win (4) wins the game (to tune after
play); every round starts with a countdown (game.bomb_snd_countdown, 6 s: the strategy
time) when the gametype has its PRE-GAME COUNTDOWN. A round won is a point */
boolean ae_bomb_snd_on(void);
/* (a dead player's respawn wait in SEARCH AND DESTROY: till the next round, which sets it to a tick) */
#define AE_BOMB_SND_NO_RESPAWN_TICKS (30 * 60 * 30)
/* game_engine_bomb.c's bomb_start_round: the dead back for the new round */
void ae_bomb_snd_round_started(void);
/* game_engine_bomb.c's update, the host's, each tick: a round won by elimination or the time */
void ae_bomb_snd_update(void);
/* game_engine_bomb.c, every machine's update: SEARCH AND DESTROY's planted bomb beeps, faster as its fuse runs out */
void ae_bomb_snd_beeps(void);
/* ticks since the planted bomb last beeped on this machine (NONE: no fuse running): the bomb's beacon flashes with it
(ae_bomb_pack.c) */
long ae_bomb_snd_beep_age(void);
/* the blast (game_engine_bomb.c): where it goes off (SEARCH AND DESTROY: at the planted bomb; elsewhere, or
with no bomb object, at the goal) and the radius everyone in it dies (SEARCH AND DESTROY: the lethal core, where
its 200 down to 20 damage, over 9 wu (made bigger by the owner from ~4.3 wu: a 5 wu core), is 100 or
more; the rest of it ae_bomb_pack_blast's) */
union real_point3d const *ae_bomb_snd_blast_point(union real_point3d const *goal);
real ae_bomb_snd_blast_radius(real chupa_radius);
/* SEARCH AND DESTROY's whole blast radius (its falloff's reach, ae_bomb_pack_blast's) */
real ae_bomb_snd_blast_reach(void);
/* game_engine_bomb.c (an AE exporter): the round to that team (a point), the next round started unless that won the
game */
void ae_bomb_snd_round_won(long team_index);
/* game_engine_bomb.c's bomb_award_score: the goal's countdown before the blast (chupa_ticks: Chupa's 3 s); AE's rules
have none, the planted bomb's fuse being its countdown (the clock and the blast at 0 together; Grifball keeps it) */
long ae_bomb_assault_goal_countdown(long chupa_ticks);
/* game_engine_bomb.c's arming (the carrier in the enemy goal): its ticks (chupa_ticks: the gametype's arm time), SEARCH
AND DESTROY's game.bomb_snd_plant_time */
long ae_bomb_assault_plant_ticks(long chupa_ticks);
/* game_engine_bomb.c's update, every machine's, each tick: SEARCH AND DESTROY's game.bomb_snd_rounds_to_win over the
gametype's score to win */
void ae_bomb_snd_score_to_win(long *score_to_win);
/* game_engine_bomb.c's bomb_plant: the longest fuse (chupa_maximum: Chupa's 10 s; SEARCH AND DESTROY's longer) */
double ae_bomb_assault_fuse_maximum(double chupa_maximum);

/* ---------- the HUD's (HUD STYLE AE: ae_strip_engine.c's round strip, ae_bomb_hud.c's PLANTING / DEFUSING bar) and
the countdown each round */

/* Chupa's Bomb state as the HUD reads it (game_engine_bomb.c fills it: an AE exporter). plant_ticks and
plant_goal_team are the host's alone (a client has 0 and NONE: ae_bomb_assault_fuse_ticks has its fuse) */
struct ae_bomb_state
{
	boolean running;
	long weapon_index;          /* the bomb (NONE: none) */
	long carrier;               /* its carrier's player index (NONE: none) */
	long scores[2];             /* rounds (goals) won, red and blue */
	long score_to_win;          /* 0: none */
	long round;                 /* from 1 */
	long round_ticks;           /* Chupa's round clock: ticks left, NONE with no limit */
	long round_start_time;      /* game time (a client's: when the host's state said a new round began) */
	long arm_ticks;             /* the carrier's ticks in the goal (every machine's, from the host's state) */
	long arm_ticks_needed;      /* the gametype's arm time in ticks (0: plants at a touch) */
	long plant_ticks;           /* the host's: the planted bomb's ticks to its score (0: none planted) */
	long plant_goal_team;       /* the host's: the goal it is planted in (the defenders'), NONE */
	long hold_ticks;            /* after a goal, the ticks before play goes on */
	long countdown_ticks;       /* the goal's countdown before the blast (BOMB_COUNTDOWN_TICKS) */
	long blast_pause_ticks;     /* the blast's pause after it (BOMB_BLAST_PAUSE_TICKS) */
	real save_radius;           /* a defender this near the planted bomb defuses it (BOMB_SAVE_RADIUS) */
};
/* game_engine_bomb.c (an AE exporter) */
void ae_bomb_state_get(struct ae_bomb_state *state);
/* game_engine_bomb.c's game_engine_bomb_rasterize: HUD STYLE AE draws the round strip and the PLANTING / DEFUSING bar
in place of Chupa's round clock, team rounds, large countdown and arming meter (classic keeps Chupa's) */
boolean ae_bomb_assault_hud_replaced(void);

/* COUNTDOWN EACH ROUND: with the gametype's PRE-GAME COUNTDOWN on and AE's rules (not Grifball), every round after the
first starts with the same countdown (game_engine.c's game_engine_pregame_countdown_ticks_left, _covers): its ticks
left now (0: none), and whether a tick is one of the current round's countdown */
long ae_bomb_round_countdown_ticks_left(void);
boolean ae_bomb_round_countdown_covers(long time);
/* the countdown screen's round line (ae_hud_style_draw.c, HUD STYLE AE): a Bomb game with AE's rules, its round (from
1) and the attacking team (NONE: none) */
boolean ae_bomb_round_line(long *round, long *attackers);

/* the planted bomb's ticks to its score, every machine's (the host's own; a client's from the host's state, below),
0 when none is planted */
long ae_bomb_assault_fuse_ticks(void);
/* the defuse under way, 0..1 (0: none), every machine's */
real ae_bomb_assault_defuse_progress(void);
/* SEARCH AND DESTROY as the host plays it (a client: from the host's state; ae_bomb_snd_on is this machine's
setting) */
boolean ae_bomb_snd_shown(void);
/* SEARCH AND DESTROY's round clock: ticks left before a plant (from the round's countdown's end), NONE when not
SEARCH AND DESTROY */
long ae_bomb_snd_round_ticks_left(void);

/* Chupa's bomb state message's two spare bytes (struct bomb_network_state's pad, which a Chupa host sends as 0 and a
Chupa client skips), written by the host and read by a client: [0] the planted bomb's fuse left in half seconds (0:
none planted), [1] bit 7 SEARCH AND DESTROY on, bits 0-6 the defuse's progress in 127ths (0: none) */
void ae_bomb_assault_write_state(unsigned char spare[2]);
void ae_bomb_assault_read_state(unsigned char const spare[2]);

/* SEARCH AND DESTROY's bomb is CARRIED, not held (owner 2026-10-28): its carrier keeps his
weapons. weapons.c's weapon_must_be_readied says FALSE for it (no forced switch to it, no drop on the trigger), units.c's
weapon cycle passes it by, and it shows on the carrier's back in place of his stowed rifle (ae_backpack_weapons.c).
Every machine's (a client: its bomb from the host's state) */
boolean ae_bomb_snd_carried_bomb(long weapon_index);
/* whether a unit (NONE: none) carries SEARCH AND DESTROY's bomb: the HUD's "YOU HAVE THE BOMB" and its bomb tile */
boolean ae_bomb_snd_unit_carries(long unit_index);
/* units.c's unit_drop_item (AE hook), after its throw: SEARCH AND DESTROY's bomb is not thrown, it falls straight down
where its carrier was, onto the floor under him (a trace down, no throw, no bounce) */
void ae_bomb_snd_drop_straight_down(long item_index, long unit_index);
/* players.c's weapon pickup (AE hook): the player who dropped the bomb by hand (BOMB DROP) picks it up again only
after letting go of the action button that dropped it */
boolean ae_bomb_snd_pickup_blocked(long player_index);
/* players.c's update (AE hook), every player's every tick on the machine that runs them: the action button HELD this
tick, as the weapon swap's flag says (set while it is held past the map's minimum_weapon_swap_ticks, the hold that
picks up a weapon; the action flag itself is one tick a press). BOMB DROP's hold reads it */
void ae_bomb_snd_note_action(long player_index, boolean held);
/* the pickup prompt's name of SEARCH AND DESTROY's bomb ("Hold X to pick up the bomb"; the ball has none in the map's
strings): hud.c gives the bomb AE_BOMB_SND_ITEM_NAME in place of its icon text index (any other object keeps
name_index), and hud_messaging.c draws AE's text for it (any other index the list's string) */
#define AE_BOMB_SND_ITEM_NAME 0x7E00
short ae_bomb_snd_item_name(long object_index, short name_index);
wchar_t const *ae_bomb_snd_item_text(long string_list_index, short string_index);

#endif
