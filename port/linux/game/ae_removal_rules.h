/*
AE_REMOVAL_RULES.H

The words of a removal (0.7.9): the reason the host's notice gave ("You were removed: <why>", network_distributed.c) as a
title and one plain sentence, for the error that follows the lost connection (ae_glue_errors.c) and the lobby's removed
page (ae_screen_server_lobby.c). Pure C, for the unit test (port/linux/tests/ae_removal_rules_test.c).

The reasons the server writes (and what an older server wrote, which still reads):
  away for N minute[s] (AFK)                 (older: away too long (AFK))
  kicked by the host[ (rejoin in M:SS)]
  voted out by the other players[ (rejoin in M:SS)]
  banned for N minute[s] / banned for N hours / banned from this server
  your game ran too fast
Anything else keeps the old words: "Removed" and "You were removed from the server: <why>."
*/

#ifndef AE_REMOVAL_RULES_H
#define AE_REMOVAL_RULES_H

enum
{
	AE_REMOVAL_OTHER,
	AE_REMOVAL_AFK,
	AE_REMOVAL_KICKED,
	AE_REMOVAL_VOTED,
	AE_REMOVAL_BANNED,
	AE_REMOVAL_SPEED
};

struct ae_removal_words
{
	int kind;
	char title[48];
	char body[200];
};

/* the words of a reason; the kind (AE_REMOVAL_*) */
int ae_removal_words_of(char const *why, struct ae_removal_words *words);

/* the wait the server's text carries ("(rejoin in 1:58)": 118), or -1 when it names none */
long ae_removal_wait_seconds(char const *why);

#endif
