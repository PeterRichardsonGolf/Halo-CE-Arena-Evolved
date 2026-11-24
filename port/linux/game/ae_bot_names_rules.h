/*
AE_BOT_NAMES_RULES.H

bot_names.txt as pure rules (port/linux/tests/ae_bot_names_rules_test.c): one name a line, blank lines and lines
whose first character is # ignored, spaces and tabs around a name dropped, only printable ASCII kept (a name is
what the player-name field holds), each name clipped to AE_BOT_NAMES_LENGTH characters. Duplicates stay.
*/

#ifndef __AE_BOT_NAMES_RULES_H
#define __AE_BOT_NAMES_RULES_H

/* a player name's characters (the network player's name holds 11 and a NUL) */
#define AE_BOT_NAMES_LENGTH 11
/* the most names read */
#define AE_BOT_NAMES_MAXIMUM 128

/* the names in text, into names (each ended): how many (0 for an empty file or one of comments only) */
int ae_bot_names_parse(char const *text, char names[][AE_BOT_NAMES_LENGTH + 1], int maximum);

/* bot number bot_index's (0 up) name out of count names: the name itself for the first count bots, then the names
again with a number as the built-in list does ("Ann 2"); the base is clipped to leave room for it */
void ae_bot_names_pick(char const names[][AE_BOT_NAMES_LENGTH + 1], int count, long bot_index, char *name, int size);

#endif
