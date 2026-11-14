/*
AE_DEV_EVENTS.H

DEV MODE's event stream: one JSON object a line, for the agents that judge a TEST RUN's expect lines
(tools/ae_test/expect.py) and, later, watch a test live. Only while DEV MODE is on (ae_dev_mode_on): off, nothing is
opened and the calls return at a flag's test.

The file is <config folder>/dev/events-<YYYYmmdd-HHMMSS>.jsonl, opened at the first event of a run of the game and
flushed at each line. As it opens, the folder's older streams beyond the newest 50 are removed (that exact name
pattern only: ae_dev_events_rules.h), with one log line saying how many. Every line has "t" (milliseconds since the file opened), "tick" (the game's time in ticks, -1
out of a game) and "ev" (the kind), then the kind's own members:

	cmd        source, line, ok (true/false), text: a dev command's answer (ae_dev_cmd.c)
	game_start map, engine; game_over
	spawn      player, x, y, z; death: player
	kill       victim, killer (-1: none)
	ttk        killer, victim, weapon, seconds, hits, head: a kill's time from the killer's first damage to the victim (the ttk
	           command, ae_dev_ttk.c)
	weapon     player, weapon (tag path), loaded, total: the weapon in hand changed
	ammo       player, weapon, loaded, total: the rounds in hand went up (a pickup of ammunition, a reload)
	score      player, individual, team: a player's score or their team's changed
	object_new kind ("vehicle", or "item": a weapon or equipment on the map, not held), tag, id, x, y, z;
	           object_gone kind, tag, id (an item taken is gone); vehicle_dead tag, id
	hud        text: a HUD message shown
	log        text: a line the game wrote to debug.txt (error(): "ae needler: ...")
	round_start round, switched (true/false): a round mode's round began (ae_round.c; ELIMINATION)
	round_end  round, winner (a side: 0 red, 1 blue, a player without teams; -1 a tie), red, blue (the wins)
	down       player, x, y, z, zone (false): a player went down at a revive spot; out: the same with no spot (zone true:
	           the danger zone's death)
	revived    player, by (-1: a dev command), revives (this round)
	zone       phase (0 wait, 1 closing, 2 wait, 3 closing, 4 closed), radius: the danger zone reached a phase
	round_wait player, round: a player who joined during a round waits for the next; match_over rounds, best,
	           runner_up
	test       list, test, state: "load" (its setup applied), "ready" (up), "start" (after the reset, its setup lines
	           go: the judged window opens), "setup" (they have run), "setup_failed", "verdict" (with verdict)
*/

#ifndef AE_DEV_EVENTS_H
#define AE_DEV_EVENTS_H

#include <stddef.h>

/* whether events are written now (DEV MODE on) */
int ae_dev_events_on(void);

/* one line of kind ev: members is "" or the kind's own JSON members, with no leading comma ("\"player\":3") */
void ae_dev_event(const char *ev, const char *members_format, ...);

/* text as a JSON string's inside (quotes, backslashes, control characters escaped), cut to fit size */
void ae_dev_event_text(char *out, size_t size, const char *text);

/* (each game tick, from ae_dev_cmd_tick) the state's changes as events: spawns, deaths, the weapon in hand, scores,
vehicles and items made and gone, the game's start and end */
void ae_dev_events_tick(void);

/* (errors.c's error(), every line it writes to debug.txt) the line as a "log" event, its end of line cut off */
void ae_dev_events_log(char const *line);

/* the open stream's path, "" before the first event */
const char *ae_dev_events_path(void);

#endif
