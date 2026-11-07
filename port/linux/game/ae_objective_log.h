/*
AE_OBJECTIVE_LOG.H

The objective log (debug.objective_log: off | events | snap | full; HALO_OBJECTIVE_LOG; debug.bot_events implies full): one
schema, "ae obj:" lines in debug.txt, for every objective mode, from the host only. The format and every event's keys:
ae_objective_log_rules.h. Read by tools/ae_test/obj_report.py. Player slots only, never names.

- events: flag take / drop / return / capture, ball take / drop, the hill's moves and who is in it, the territories, the
  bomb's grabs / plants / defuses / blasts, the rounds, a bot's job changes and the objective it does not reach, game
  start and end.
- snap: + one line a second of the score, the clock and where the objective is.
- full: + one line a second for every living player (a person and a bot alike): where, health, what it does and why.

One writer (ae_objective_log.c); the engine files call it in one-line AE hooks (ae_hooks.txt), the AE files directly.
The existing log lines ("bot event:", "bomb:", "ae territories:", ...) are unchanged; these only add.
*/
#ifndef AE_OBJECTIVE_LOG_H
#define AE_OBJECTIVE_LOG_H

#include "cseries.h"
#include "ae_objective_log_rules.h"

/* the level now (AE_OBJ_OFF with the setting off); read once a game */
int ae_obj_level(void);

/* (game_engine_update, the host's each tick) the watchers: game start and end, the flags, balls, hill and bomb as they
change, the one-a-second state and the people's lines */
void ae_obj_tick(void);

/* (game_engine_ctf.c) a capture: the player (a player datum index) scored for its team; a flag returned (touched by a
player of its team, or NONE for its time running out) */
void ae_obj_ctf_capture(long player_index, long team_index);
void ae_obj_ctf_return(long weapon_index, long player_index);

/* (game_engine_bomb.c's playtest log) the bomb's event the log names ("grab", "drop", "plant", "save", "score",
"blast kills", ...) with the player (a player datum index, NONE for none); a round ending */
void ae_obj_bomb_player(char const *what, long player_index);
void ae_obj_bomb_round(long round, int timed_out, long score_red, long score_blue);

/* (ae_bomb_assault.c) a defuse by that player begins, is let go (held_ticks) or is done; an AE SND round begins (the round's end is
ae_obj_bomb_round's, whose reason is the pending one) */
void ae_obj_defuse(int kind, long player_index, long held_ticks);
enum { AE_OBJ_DEFUSE_START, AE_OBJ_DEFUSE_CANCEL, AE_OBJ_DEFUSE_DONE };
void ae_obj_snd_round(long round, long attackers, long score_red, long score_blue);
/* the reason the coming round end has (wipe | time | defuse; none: the blast), told by the code that decides it */
void ae_obj_snd_pending(char const *why);

/* (ae_h2_king.c) a territory taken; a territory's occupancy or contest changed */
void ae_obj_terr_capture(int territory, long hill, int team, int from_team, long score_red, long score_blue);
void ae_obj_terr_state(int territory, long hill, int owner, int contested, long occupants_red, long occupants_blue,
	long progress);

/* (game_engine_bomb.c's playtest log, every line) the bomb's send home ("reset: ...") */
void ae_obj_bomb_log(char const *message);

/* (ae_round.c, ae_round_zone.c) the elimination's rounds, downs and outs (the spot where), revives and the zone's phases */
void ae_obj_elim_round_start(long round, long max_rounds, int switched, long wins_red, long wins_blue);
void ae_obj_elim_round_end(long round, long winner, long wins_red, long wins_blue);
void ae_obj_elim_down(long player_index, boolean out, boolean zone, real_point3d const *spot);
void ae_obj_elim_revive(long player_index, long by, long revives);
void ae_obj_zone_phase(long round, int phase, double radius, long left_seconds);

/* (game_engine_race.c) a flag touched by that flag's team index; a lap (its number and ticks) */
void ae_obj_race_flag(long player_index, long flag_team);
void ae_obj_race_lap(long player_index, long lap, long lap_ticks);

/* ---------- for the bots' lines (ae_bots_objlog.inc) and the people's */

struct ae_obj_ps_words
{
	char const *job, *role, *goal, *state, *seat, *order;
	long fireteam;
};

/* emit a line of the current game's mode (the level is the caller's to check) */
struct ae_obj_fields;
void ae_obj_emit(char const *ev, struct ae_obj_fields const *fields);
/* "p b team x y z" of a player (a player datum index): the slot, bot or not, team, where */
void ae_obj_who(struct ae_obj_fields *fields, long player_index);
/* where the player's objective is (the enemy flag, the ball, the hill, the territory, the bomb or its goal) as a name
and its distance (-1: none) */
void ae_obj_objective_of(long player_index, char const **name, double *distance);
/* a player's line (ev=ps); words NULL: a person's */
void ae_obj_ps(long player_index, struct ae_obj_ps_words const *words);
/* the log's names of the game, for the stuck bot's words */
char const *ae_obj_mode_word(void);
boolean ae_obj_player_is_bot(long player_index);

#endif
