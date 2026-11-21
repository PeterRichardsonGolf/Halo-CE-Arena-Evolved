/*
AE_PARTY_TEAMS_RULES.H

Arena Evolved: a split-screen party plays on one team (owner 2026-11-09: "when a player joins in a splitscreen party that
they can play as team and dont get separated"). A party is the players of one machine; each bot is a party of its own.
The host decides (the gametype's KEEP PARTIES TOGETHER, ON unless a gametype says OFF; game.keep_parties_together turns
it off for a host); a client sends team NONE as ever and takes the host's pick, so nothing on the wire changes. Pure
(port/linux/tests/ae_party_teams_rules_test.c); the game's side is ae_party_teams.c.

- JOIN: the team the party is on already; a new party takes the smaller team with room for all its players (a team has
  at most half the game's players); with no room, the smaller team where the bots would give way; else -1, the old
  per-player pick.
- BALANCE (pregame AUTO TEAM BALANCE): while the teams differ by more than one, the party on the bigger team whose move
  leaves them closest moves, whole.
- DEAL (SHUFFLE TEAMS): parties dealt largest first, each onto the smaller team.
- DEATH BALANCE: only a player alone on their team moves, as before.
In-game CHANGE TEAMS and the host's override stay per player.
*/

#ifndef __AE_PARTY_TEAMS_RULES_H
#define __AE_PARTY_TEAMS_RULES_H

enum { AE_PARTY_PLAYERS_MAX = 128 };

struct ae_party_player
{
	int party;	/* the machine's number; a bot's own */
	int team;	/* 0 or 1 */
	int bot;	/* 1 for a bot (the lobby's bots give way to people) */
};

/* the team (0 or 1) for a player joining as one of party_size players of party (this one counted), or -1 for the old
pick; team_cap is the most players a team holds */
int ae_party_join_team(struct ae_party_player const *players, int count, int party, int party_size, int team_cap);
/* AUTO TEAM BALANCE: moves whole parties until the teams differ by one or no move helps; the number of players moved */
int ae_party_balance(struct ae_party_player *players, int count);
/* SHUFFLE TEAMS: teams[i] (0 or 1) for count players of parties party[i], from seed */
void ae_party_deal(int const *party, int count, unsigned long seed, signed char *teams);
/* the people of party on team (bots never counted: a LAN bot shares its host's machine) */
int ae_party_group_size(struct ae_party_player const *players, int count, int party, int team);
/* the sender (players[sender]) moved from old_team to new_team: the other people of its party who were on old_team follow
(set to new_team); the number moved. A change that is not a team change moves nobody */
int ae_party_follow(struct ae_party_player *players, int count, int sender, int old_team, int new_team);
/* death balance: whether the dead player may move (alone in their party) from a team of own players to one of other */
int ae_party_death_may_move(int party_size, int own, int other);

#endif
