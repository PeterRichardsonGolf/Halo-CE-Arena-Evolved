/*
AE_REJOIN_RULES.H

REJOIN AFTER A DROP (rc6, network.rejoin_after_drop): the pure rules of ae_rejoin.c (unit test:
port/linux/tests/ae_rejoin_rules_test.c).

A guest whose connection to the server or host ends in a game with no reason from the host (no kick, ban, restart or
shutdown notice) does not get the lost-connection error at once: the engine leaves the game as before, and once the
menus are up the same join is made again (the same address, name, invite), up to AE_REJOIN_TRIES times within
AE_REJOIN_WINDOW_MS. Only then the error shows.

The host side: a join whose machine name is the same as a machine in the game that has been silent for
AE_REJOIN_STALE_MS or more, from the same machine (the same hardware id when both told one, else the same address),
replaces that machine: it is dropped first, so the rejoin table (ae_server_bots_rules.h) holds its slot, team and
statistics for the new one.
*/

#ifndef AE_REJOIN_RULES_H
#define AE_REJOIN_RULES_H

enum
{
	AE_REJOIN_TRIES = 3,
	/* the first try this long after the drop (the host's old machine silent past AE_REJOIN_STALE_MS by then) */
	AE_REJOIN_FIRST_MS = 2500,
	/* a failed try's next one this long after it */
	AE_REJOIN_GAP_MS = 2000,
	/* every try within this of the drop (an older host keeps the old machine up to its 15 s silence limit) */
	AE_REJOIN_WINDOW_MS = 30000,
	/* the host: a machine silent this long may be replaced by its own rejoin */
	AE_REJOIN_STALE_MS = 2000,
	/* the text sizes */
	AE_REJOIN_TEXT = 160,
};

/* whether a lost connection is rejoined: the setting on, the join armed (this machine a guest in a multiplayer game
of a remote host, by a join that can be made again), the error a lost connection or a host gone, and no reason given
by the host (a kick, ban, restart or shutdown notice) */
int ae_rejoin_wanted(int enabled, int armed, int lost_error, int host_gave_reason);

/* the time of the next try: the first AE_REJOIN_FIRST_MS after the drop, a later one AE_REJOIN_GAP_MS after the last
failed (times in ms of one clock, unsigned: a clock that wraps is read by differences) */
unsigned long ae_rejoin_next_try(unsigned long lost_at, int tries_done, unsigned long last_failed_at);

/* whether the rejoin gives up now: no try left and none going on, or the window past */
int ae_rejoin_give_up(unsigned long now, unsigned long lost_at, int tries_done, int trying);

/* whether a try may start now (its time come and the window not past) */
int ae_rejoin_try_due(unsigned long now, unsigned long lost_at, unsigned long next_try);

/* the dialog's words while rejoining: "The connection to the server was lost. Rejoining <where>... (try n of 3)";
where empty: no name. dedicated: a dedicated server, else a player's host */
void ae_rejoin_body(char *out, int size, int dedicated, char const *where, int try_number);

/* the host's replace rule. same_name: the joining machine's name is the old one's; hardware ids "" when not told;
addresses 0 when not known; silent_ms: how long the host has not heard the old machine */
int ae_rejoin_replaces(int same_name, char const *joining_hardware_id, char const *old_hardware_id,
	unsigned long joining_address, unsigned long old_address, unsigned long silent_ms);

/* two machine names (16-bit characters) are the same up to their ends (or length characters): 0 for an empty name */
int ae_rejoin_same_name(unsigned short const *a, unsigned short const *b, int length);

/* a test drop (debug.network_test_drop): "<seconds>:reset" or "<seconds>:silent". 1 with its seconds and silent
(1: the stream left open, the host hears nothing; 0: closed), 0 for an empty or bad text */
int ae_rejoin_test_drop_parse(char const *text, double *seconds, int *silent);

#endif
