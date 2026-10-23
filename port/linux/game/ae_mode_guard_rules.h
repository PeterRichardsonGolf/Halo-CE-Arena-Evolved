/*
AE_MODE_GUARD_RULES.H

Bug 139's rules, pure (ae_mode_guard.c applies them to the loaded map): what a gametype's mode needs from a map's
netgame flags and spawns. A mode that misses a flag it needs is played as Slayer (owner 2026-10-17: fix the map with a
layout file first; a map still missing them plays Slayer with a message, never a crash); a mode with no spawn of its
own game type spawns players at the map's spawns of every game type.
*/

#ifndef __AE_MODE_GUARD_RULES_H
#define __AE_MODE_GUARD_RULES_H

/* the modes, numbered as game_engine.h's game engines */
enum
{
	AE_MODE_GUARD_CTF = 1,
	AE_MODE_GUARD_SLAYER = 2,
	AE_MODE_GUARD_ODDBALL = 3,
	AE_MODE_GUARD_KING = 4,
	AE_MODE_GUARD_RACE = 5,
	/* (Bomb, the builds with it: its goals are the CTF flags of both teams, game_engine_bomb.c) */
	AE_MODE_GUARD_BOMB = 6
};

/* what a map has: its netgame flags of each kind (the CTF flags per team) */
struct ae_mode_guard_map
{
	int ctf_flags[2];
	int hills;
	int ball_spawns;
	int race_flags;
};

/* what the mode misses on the map, for the log ("red CTF flag", "hill" ...), NULL when nothing: Slayer and
unknown modes need no flag */
const char *ae_mode_guard_missing(long mode, struct ae_mode_guard_map const *map);

/* the mode's name for the message ("CTF", "KING OF THE HILL" ...), "" for an unknown mode */
const char *ae_mode_guard_mode_name(long mode);

/* the score to win of the Slayer a mode the map cannot play falls back to: AE's Slayer's (50 kills a team, 25 a player),
not the mode's own (KING OF THE HILL's 5 was minutes on the hill: 5 kills ended the game in seconds, Terminal 10-20) */
long ae_mode_guard_slayer_score(int teams);

/* whether players spawn at every spawn, whatever its game types: the map has spawns, none of them the mode's */
int ae_mode_guard_spawn_anywhere(int spawns_for_mode, int spawns);

#endif
