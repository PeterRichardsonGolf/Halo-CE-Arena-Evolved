/*
AE_JOINER_NOTICE_RULES.H

The words of a host's chat line for a machine that joins without Arena
Evolved (ae_joiner_notice.c says it). Pure C, for the unit test
(port/linux/tests/ae_joiner_notice_rules_test.c).
*/

#ifndef AE_JOINER_NOTICE_RULES_H
#define AE_JOINER_NOTICE_RULES_H

/* where Arena Evolved is (its releases) */
#define AE_JOINER_NOTICE_LINK "github.com/PeterRichardsonGolf/Halo-CE-Arena-Evolved"

/* the line for a machine, written to text: name its first player's (UTF-8; "A player" when NULL or empty), kind
delta_peer_game_machine_build's answer (1 a Delta build named build, 0 OpenCE's, -1 not known: "another build").
Returns the text's length, 0 (text empty) when it does not fit */
int ae_joiner_notice_text(char *text, int size, char const *name, int kind, char const *build);

#endif
