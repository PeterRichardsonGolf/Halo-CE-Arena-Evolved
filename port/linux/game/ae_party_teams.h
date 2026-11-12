/*
AE_PARTY_TEAMS.H

A split-screen party on one team (ae_party_teams_rules.h): the game's side. The host's choice: the gametype's KEEP PARTIES
TOGETHER (ON unless the gametype says OFF: the AE block's AE_EXT_MATCH_SPLIT_PARTIES) and the setting
game.keep_parties_together (true, false turns it off for this host). Off is the game's own play, a player at a time.
*/

#ifndef AE_PARTY_TEAMS_H
#define AE_PARTY_TEAMS_H

struct network_game;
struct network_player;

/* whether this machine, as the host, keeps a machine's players on one team */
int ae_party_teams_on(void);
/* whether a lobby player is a bot (a local game's, on a machine of its own, or a LAN game's, a controller of the host's) */
int ae_party_teams_player_is_bot(struct network_game const *game, struct network_player const *player);
/* network_game_server_add_player_to_game: the team for the joining player, old_team (the smaller) when the rule has none */
int ae_party_teams_join(struct network_game const *game, struct network_player const *player, int old_team);
/* pregame AUTO TEAM BALANCE: 1 when whole parties handled it (changed set when a team changed), 0 for the old balance */
int ae_party_teams_balance_lobby(struct network_game *game, int *changed);
/* the host is about to take a player's new settings: their team in the game's record now (ae_party_teams_follow compares) */
void ae_party_teams_note_player(struct network_game const *game, struct network_player const *player);
/* the host took a player's new settings: when their team changed, the other people of their machine who were on their old
team follow (a name or colour change moves nobody) */
void ae_party_teams_follow(struct network_game *game, struct network_player const *player);
/* the number of people of a lobby slot's machine on its team (1 when parties are not kept): a switch moves them all */
int ae_party_teams_group_size(struct network_game const *game, long slot);
/* death balance: whether the dead player (a player datum's index) may move to the other team */
int ae_party_teams_death_may_move(long player_index);
/* SHUFFLE TEAMS: teams for the people of machines machine[i], parties kept when on (else the plain deal) */
void ae_party_teams_deal(int const *machine, int count, unsigned long seed, signed char *teams);

#endif
