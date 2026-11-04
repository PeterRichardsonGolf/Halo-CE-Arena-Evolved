/*
AE_DEV_CMD.H

DEV MODE's commands in the game (the grammar: ae_dev_cmd_rules.h). A line comes from the console, the telnet
console, a TEST RUN set-up line or a script; it is checked at once (help, a usage line, the host-only gate answer
straight away) and the rest wait for the next game tick, where objects are made as the game makes its own. Every
command logs one line, "dev cmd <source>: <line> -> ok <what>" or "-> error <why>", and answers on the console it
came from.

Host only (ae_dev_cmd_rules.h's gate): a network game's client refuses every command but help, whatever its own
system.dev_mode. In a network game the host hosts, each command that changes the game is said in the chat to every
player, AE or not.
*/

#ifndef AE_DEV_CMD_H
#define AE_DEV_CMD_H

enum ae_dev_cmd_source
{
	AE_DEV_CMD_FROM_CONSOLE = 0,
	AE_DEV_CMD_FROM_TELNET,
	AE_DEV_CMD_FROM_TEST_RUN,
	AE_DEV_CMD_FROM_SCRIPT,
	AE_DEV_CMD_SOURCES
};

/* 1: the line was a dev command (answered, or queued for the next tick); 0: it is not one, the script evaluator takes
it. A command that changes the game, with no game running (the menus, a screen test), is answered "no game running" at
once, never queued */
int ae_dev_cmd_line(const char *line, int source);
/* (hs.c's hs_compile_and_evaluate_command, so the console, the telnet console, init.txt and the server's commands,
before the script evaluator's own commands) ae_dev_cmd_line for the source ae_dev_cmd_evaluate_from set (the console
otherwise); when it was a dev command, *succeeded is whether it was taken: 0 for one refused or failed at once */
int ae_dev_cmd_hs_line(const char *line, int *succeeded);
/* (telnet_console.c, console.c's init.txt, network_test.c's cues) hs_compile_and_evaluate with a dev command's line
logged as coming from source (AE_DEV_CMD_FROM_TELNET, AE_DEV_CMD_FROM_SCRIPT) */
int ae_dev_cmd_evaluate_from(const char *expression, int source);

/* (the game tick, on this machine) runs the commands queued since the last tick */
void ae_dev_cmd_tick(void);
/* (game_engine_initialize_for_new_map) a new game: a reset not finished and the rules kept are the last game's, never
this one's (whatever its clock says); with DEV MODE off, the settings bots and config changed go back */
void ae_dev_cmd_new_game(void);

/* nothing queued or under way (a reset, a respawn waited for) and the first local player alive: a TEST RUN's setup
lines may go */
int ae_dev_cmd_settled(void);
/* commands are waiting for the game's tick */
int ae_dev_cmd_waiting(void);

/* (console.c's Tab) adds the dev command names (with DEV MODE on) that start with prefix to the script console's count
names in items, up to maximum, sorted again as the console's list is (by name, any case); the new count */
int ae_dev_cmd_completions(const char *prefix, const char **items, int count, int maximum);

/* gives a weapon (its definition) to a player's unit as the dev wheel does: the one in hand dropped when two are
held. 1 when it is held. (Called in the game tick) */
int ae_dev_cmd_give_weapon(long player_index, long definition_index);
/* (game_engine.c's game_engine_player_killed, on the host) the player's death is a reset's: nobody's kill, nothing
counted, no message, as a team change's. TRUE once for each player a reset killed */
int ae_dev_cmd_quiet_death(long dead_player_index);

/* (game_engine.c's end-of-game test and the clocks) the ticks a time command added to this game's time limit: 0 when
none did, or DEV MODE is off */
long ae_dev_cmd_time_extra(void);

/* TRUE while a dev command (or the wheel) makes an object: the map's remaps and the weapon set leave it be */
int ae_dev_cmd_making(void);

/* (a TEST RUN's next map load and its end) the settings bots and config changed put back as they were before; reset
does it too */
void ae_dev_cmd_settings_back(void);

#endif
