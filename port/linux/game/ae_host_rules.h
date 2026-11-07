/*
AE_HOST_RULES.H

CREATE GAME's HOST RULES panel (0.7.5): the rules a host keeps for the games this PC hosts, as rows over config
keys the settings already have (network.text_chat, voice, team changing, the client timeout, the votes). This is
the model, pure C for the unit test (port/linux/tests/ae_host_rules_test.c): the rows, their words, which value a
key's text is, and what a pick writes. The screen (ae_screen_create_game.c) reads and writes the config.
*/

#ifndef AE_HOST_RULES_H
#define AE_HOST_RULES_H

/* the rows, in the order the panel lists them */
enum
{
	AE_HR_TEXT_CHAT, AE_HR_LOBBY_VOICE, AE_HR_GAME_VOICE, AE_HR_VOICE_RANGE,
	AE_HR_TEAM_CHANGING, AE_HR_CLIENT_TIMEOUT,
	AE_HR_VOTEKICK, AE_HR_VOTES_NEEDED, AE_HR_VOTE_END,
	AE_HR_ROWS
};
enum { AE_HR_SECTION_CHAT, AE_HR_SECTION_PLAYERS, AE_HR_SECTION_VOTES, AE_HR_SECTIONS };

/* a custom value: no word of the row's list is it */
enum { AE_HR_CUSTOM = -1 };

char const *ae_host_rule_label(int row);
int ae_host_rule_section(int row);
char const *ae_host_rule_section_name(int section);

/* the words a row offers (its list; VOTES NEEDED's four) */
int ae_host_rule_count(int row);
char const *ae_host_rule_word(int row, int index);

/* the row's value: which word the config's text is. text is what the config holds ("on", "true", "15.0"); for
VOTES NEEDED it is the threshold ("dynamic" or "static") and percent the network.votekick_percent. AE_HR_CUSTOM
when no word is it (a number between the list's, a percent that is not 50, 60 or 75). Numbers go to the nearest
word when nearest is set (a step from a custom value) */
int ae_host_rule_index(int row, char const *text, long percent, int nearest);

/* the row's value as shown: its word, or "n SECONDS" / "n M" / "CUSTOM (n %)" for a custom value */
void ae_host_rule_value_text(int row, char const *text, long percent, char *out, int size);

/* what picking word index writes: up to two config settings (name, value text); returns how many */
struct ae_host_rule_write { char const *name; char value[16]; };
int ae_host_rule_writes(int row, int index, struct ae_host_rule_write writes[2]);

/* the config setting a row reads (VOTES NEEDED: the threshold; its percent is network.votekick_percent) */
char const *ae_host_rule_setting(int row);

/* the row's help; locked: TEXT CHAT held OFF in a PUBLIC game. The line ends "Kept for the games this PC hosts." */
char const *ae_host_rule_help(int row, int locked);

/* HOST RULES...'s summary: "TEAMS BALANCED, VOICE ON", with ", CHAT OFF" when text chat is off or held. The text of
network.text_chat, network.team_change and network.voice_mode, locked as above */
void ae_host_rules_summary(char const *chat, char const *team, char const *voice, int locked, char *out, int size);

#endif
