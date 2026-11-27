/*
SOCIAL.H

This fork's social side (social.c, social_chat.c): the install's account on
the community server, the records of its players' matches and levels sent to
it, and the fork's chat (social_chat.h).
*/

#ifndef __SOCIAL_H
#define __SOCIAL_H

struct community_run;
struct firefight_emblem_info;
struct server_json;
struct server_json_document;

/* (main.c's main loop) each frame, after community_update: the account asked
for, a match over recorded, the records not sent yet sent again */
void social_update(void);

/* social.enabled, with community.enabled: the account is asked for, its
records sent, and the fork's chat online */
boolean social_enabled(void);

/* the account's code ("XXXX-XXXX"), "" until the server says it */
char const *social_account_code(void);
/* (social_menu.c's NEW CODE) the account's code from an answer of the
server's ({data: {account: {code}}}): FALSE for none in it */
boolean social_account_code_take(char const *answer);

/* the account's multiplayer totals, as the server last said them (as the
game starts, and after each record): FALSE until it has */
boolean social_account_totals(long *matches, long *kills, long *deaths, long *assists);

/* the server's last answer to what the game is doing: the friends online,
and the friend requests waiting */
void social_presence_counts(long *friends_online, long *requests_waiting);

/* (firefight_stats.c) a game's totals and XP added: sent with the account
(POST /account), which other players' cards read */
void social_firefight_changed(void);

/* the server's word on the account's rank and emblem, from POST /account:
whether it has answered, and whether it knows of them (one that does not
needs updating: nothing is asked of the player then) */
boolean social_account_known(void);
boolean social_account_ranks_served(void);
/* the emblem it has for the account: FALSE for none chosen yet (out may be
NULL) */
boolean social_account_emblem(struct firefight_emblem_info *out_emblem);
/* the account's rank and emblem as an answer of the server's says them, kept
and brought to the profile ({data: {account}}: POST /account/setup's...) */
void social_account_answer(char const *answer);
void social_account_synced(char const *answer);
/* the server's rank and emblem brought to the profile (a new one, on a device
of an account set up elsewhere) */
void social_account_apply(void);
/* (the main menu) an account set up whose past records were not counted yet:
counted into its rank, the XP card showing it */
void social_rank_catch_up(void);
/* (firefight_stats.c) the emblem chosen: sent as the choosing stops, once the
account has one on the server (the first goes with POST /account/setup) */
void social_emblem_changed(void);
/* an emblem in a request, under key */
void social_emblem_json(struct server_json *json, char const *key, struct firefight_emblem_info const *emblem);
/* a player the server said (a friend, a card, a player of the room): its rank
and emblem, at token, kept to show by its code and name */
void social_person_note(struct server_json_document const *document, int token, char const *code, char const *name);

/* the name the account goes by: player 1's profile's (ASCII, '?' for what it
has not), "" for none */
void social_player_name(char *name, long size);

/* the name the account is known by in the chat on every device: the one it
picked in Social (CHAT NAME), else player 1's profile's (as social_player_name) */
void social_account_name(char *name, long size);
/* whether the account picked one (else the profile's is used) */
boolean social_account_name_picked(void);
/* the colour the chat shows the name in (0 to 1 each): FALSE for none picked */
boolean social_account_name_color(real *red, real *green, real *blue);
/* the name and colour from an answer of the server's to POST /account/name
({data: {account: {displayName, nameColor}}}) */
void social_account_name_take(char const *answer);

/* asks the community server for path as the account (the install's key, no
profile's): a GET, or with body (JSON text) a POST. A ticket for
community_net_poll, 0 when it cannot be asked */
int social_send(char const *path, char const *body);

/* (ui_widget_event_handler_functions.c's network_game_remove_local_player)
a player of this machine quit the multiplayer game being played: if it is
left before its end, it is recorded as a quit */
void social_match_quit(void);

/* (ui_widget.c's display_error_when_main_menu_loaded) why the game being
left ends (_error_network_connection_lost: the network lost;
_error_network_server_shut_down: its host ended it) */
void social_match_error(short error_code);

/* (campaign_score.c) a campaign level won by a player of this machine (its
absolute index, NONE for none), the run as the leaderboards have it, the
level's tag name (levels\a10\a10), and its profile's name */
void social_level_won(short player_index, struct community_run const *run, char const *level_name,
	wchar_t const *name);

/* ---------- the chat's /invite (social_chat.c) */

enum
{
	SOCIAL_GAME_INVITE_LINK_SIZE = 168,
	SOCIAL_GAME_INVITE_KEY_SIZE = 33,
	SOCIAL_GAME_INVITE_LABEL_SIZE = 64,
};

/* the network game this machine hosts, as an invite in a chat line shows
it and joins it: its invite link (else, a public game with a password, its
listing's host key, which the joiner's server browser finds and asks for
the password), its game's name, gametype, map, the map's kind (Custom
Edition, MCC, Halo PC) and players */
struct social_game_invite
{
	char link[SOCIAL_GAME_INVITE_LINK_SIZE];
	char host_key[SOCIAL_GAME_INVITE_KEY_SIZE];
	char game[SOCIAL_GAME_INVITE_LABEL_SIZE];
	char gametype[SOCIAL_GAME_INVITE_LABEL_SIZE];
	char map[SOCIAL_GAME_INVITE_LABEL_SIZE];
	char kind[16];
	long players;
	long maximum;
};

/* the game hosted, as an invite (one to a single player: its link past the
password; to everyone: a password's game's host key instead). FALSE with
why (a chat notice's text) when there is none to give: not in a network
game, not its host, not an internet game */
boolean social_game_invite_make(boolean to_one_player, struct social_game_invite *invite, char *why, long why_size);

/* whether this machine is in a game (campaign, a network game or its
lobby, a replay): another's game is joined from the menus alone */
boolean social_in_any_game(void);

#endif
