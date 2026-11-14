#include <stdio.h>
#include <string.h>
#include "ae_party_teams_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void set(struct ae_party_player *p, int party, int team, int bot)
{
	p->party = party;
	p->team = team;
	p->bot = bot;
}

static int count_team(struct ae_party_player const *p, int count, int team)
{
	int index, n = 0;

	for (index = 0; index < count; index++)
		n += p[index].team == team;
	return n;
}

int main(void)
{
	struct ae_party_player p[AE_PARTY_PLAYERS_MAX] = { { 0, 0, 0 } };
	int party[AE_PARTY_PLAYERS_MAX];
	signed char teams[AE_PARTY_PLAYERS_MAX];
	int index, seed;

	/* join: an empty game, the first player takes red as the old pick does */
	CHECK(ae_party_join_team(p, 0, 0, 1, 8) == 0);
	/* the party's own team, whatever the sizes */
	set(&p[0], 5, 1, 0);
	set(&p[1], 7, 0, 0);
	set(&p[2], 8, 0, 0);
	CHECK(ae_party_join_team(p, 3, 5, 2, 8) == 1);
	/* a new party: the smaller team */
	CHECK(ae_party_join_team(p, 3, 9, 2, 8) == 1);
	/* a pair joining even teams stays together on the first (the old pick split it) */
	set(&p[0], 1, 0, 0);
	set(&p[1], 2, 1, 0);
	CHECK(ae_party_join_team(p, 2, 3, 2, 8) == 0);
	/* the party's team is full: the old pick */
	for (index = 0; index < 4; index++)
		set(&p[index], 4, 0, 0);
	CHECK(ae_party_join_team(p, 4, 4, 2, 4) == -1);
	/* no room on the smaller team for all, room on the other */
	for (index = 0; index < 3; index++)
		set(&p[index], 10 + index, 0, 0);
	set(&p[3], 20, 1, 0);
	set(&p[4], 21, 1, 0);
	CHECK(ae_party_join_team(p, 5, 30, 3, 4) == -1);
	CHECK(ae_party_join_team(p, 5, 30, 2, 4) == 1);
	/* a team of 3 and one of 2, cap 4: three players fit neither, two fit the smaller */
	/* bots give way: both teams full of bots, the people's room is the answer */
	for (index = 0; index < 4; index++)
		set(&p[index], 100 + index, 0, 1);
	for (index = 4; index < 7; index++)
		set(&p[index], 100 + index, 1, 1);
	set(&p[7], 50, 1, 0);
	CHECK(ae_party_join_team(p, 8, 60, 2, 4) == 0);
	/* the party's members counted by their party, bots never in it */
	set(&p[0], 5, 1, 1);
	CHECK(ae_party_join_team(p, 1, 5, 1, 8) == 0);
	/* a bad count or no list: the old pick */
	CHECK(ae_party_join_team(NULL, 2, 0, 1, 8) == -1);
	CHECK(ae_party_join_team(p, -1, 0, 1, 8) == -1);
	CHECK(ae_party_join_team(p, AE_PARTY_PLAYERS_MAX + 1, 0, 1, 8) == -1);

	/* balance: singles move as the old balance did, the bigger team's last */
	for (index = 0; index < 4; index++)
		set(&p[index], index, 0, 0);
	CHECK(ae_party_balance(p, 4) == 2);
	CHECK(count_team(p, 4, 0) == 2 && count_team(p, 4, 1) == 2);
	CHECK(p[0].team == 0 && p[1].team == 0 && p[2].team == 1 && p[3].team == 1);
	/* a pair and two singles on red, none on blue: the pair moves whole, which evens them */
	set(&p[0], 1, 0, 0);
	set(&p[1], 1, 0, 0);
	set(&p[2], 2, 0, 0);
	set(&p[3], 3, 0, 0);
	CHECK(ae_party_balance(p, 4) == 2);
	CHECK(p[0].team == 1 && p[1].team == 1 && p[2].team == 0 && p[3].team == 0);
	/* a pair and a single on red, nobody on blue: the pair would swap the problem, the single moves */
	set(&p[0], 1, 0, 0);
	set(&p[1], 1, 0, 0);
	set(&p[2], 2, 0, 0);
	CHECK(ae_party_balance(p, 3) == 1);
	CHECK(p[0].team == 0 && p[1].team == 0 && p[2].team == 1);
	/* one pair alone on a team of two against none: no move helps, the pair is not split */
	set(&p[0], 1, 0, 0);
	set(&p[1], 1, 0, 0);
	CHECK(ae_party_balance(p, 2) == 0);
	CHECK(p[0].team == 0 && p[1].team == 0);
	/* a party of three on a team 3 v 0: nothing moves; 4 v 0 with a pair among them moves the pair */
	set(&p[0], 1, 0, 0);
	set(&p[1], 1, 0, 0);
	set(&p[2], 2, 0, 0);
	set(&p[3], 2, 0, 0);
	CHECK(ae_party_balance(p, 4) == 2);
	CHECK(count_team(p, 4, 0) == 2 && p[0].team == p[1].team && p[2].team == p[3].team && p[0].team != p[2].team);
	/* already within one: untouched */
	set(&p[0], 1, 0, 0);
	set(&p[1], 2, 1, 0);
	set(&p[2], 3, 0, 0);
	CHECK(ae_party_balance(p, 3) == 0);
	/* a bot is a party of its own: the pair moves whole, the bots stay */
	set(&p[0], 1, 0, 0);
	set(&p[1], 1, 0, 0);
	set(&p[2], 1001, 0, 1);
	set(&p[3], 1002, 0, 1);
	CHECK(ae_party_balance(p, 4) == 2);
	CHECK(p[0].team == 1 && p[1].team == 1 && p[2].team == 0 && p[3].team == 0);
	CHECK(ae_party_balance(NULL, 3) == 0);

	/* deal: pairs stay together, the teams even where the parties allow */
	for (seed = 1; seed < 400; seed++)
	{
		int ones;

		party[0] = 1; party[1] = 1; party[2] = 2; party[3] = 2; party[4] = 3; party[5] = 4; party[6] = 5; party[7] = 6;
		memset(teams, 9, sizeof(teams));
		ae_party_deal(party, 8, (unsigned long)seed * 2654435761ul, teams);
		CHECK(teams[0] == teams[1] && teams[2] == teams[3]);
		for (ones = 0, index = 0; index < 8; index++)
		{
			CHECK(teams[index] == 0 || teams[index] == 1);
			ones += teams[index];
		}
		CHECK(ones == 4);
	}
	/* singles alone: as even as the plain deal, an odd one out either side */
	for (seed = 1; seed < 100; seed++)
	{
		int ones = 0;

		for (index = 0; index < 7; index++)
			party[index] = index;
		ae_party_deal(party, 7, (unsigned long)seed * 7919ul, teams);
		for (index = 0; index < 7; index++)
			ones += teams[index];
		CHECK(ones == 3 || ones == 4);
	}
	/* a party of four against singles: the four on one team, the singles on the other */
	for (index = 0; index < 4; index++)
		party[index] = 9;
	for (index = 4; index < 8; index++)
		party[index] = index;
	ae_party_deal(party, 8, 12345ul, teams);
	CHECK(teams[0] == teams[1] && teams[1] == teams[2] && teams[2] == teams[3]);
	CHECK(teams[4] == teams[5] && teams[5] == teams[6] && teams[6] == teams[7] && teams[4] != teams[0]);
	ae_party_deal(party, 0, 1ul, teams);
	ae_party_deal(NULL, 4, 1ul, teams);

	/* death balance: only a player alone in their party, two or more ahead */
	CHECK(ae_party_death_may_move(1, 4, 2));
	CHECK(!ae_party_death_may_move(2, 4, 2));
	CHECK(!ae_party_death_may_move(1, 3, 2));
	CHECK(ae_party_death_may_move(0, 5, 1));

	/* group size: the people of a party on a team, a LAN bot sharing the host's machine never counted */
	set(&p[0], 0, 0, 0);
	set(&p[1], 0, 0, 1);
	set(&p[2], 0, 1, 0);
	set(&p[3], 7, 0, 0);
	CHECK(ae_party_group_size(p, 4, 0, 0) == 1);
	CHECK(ae_party_group_size(p, 4, 0, 1) == 1);
	CHECK(ae_party_group_size(p, 4, 7, 0) == 1);
	CHECK(ae_party_group_size(NULL, 4, 0, 0) == 0);
	/* death balance with a LAN bot teammate: the lone host human is alone in the party, so may move */
	CHECK(ae_party_death_may_move(ae_party_group_size(p, 2, 0, 0), 3, 1));
	set(&p[1], 0, 0, 0);
	CHECK(!ae_party_death_may_move(ae_party_group_size(p, 2, 0, 0), 3, 1));

	/* follow: a team change moves the siblings who were on the sender's old team, nobody else */
	set(&p[0], 4, 1, 0);	/* sender, already on its new team */
	set(&p[1], 4, 0, 0);	/* sibling on the old team: follows */
	set(&p[2], 4, 1, 0);	/* sibling already on the new team */
	set(&p[3], 5, 0, 0);	/* another machine */
	set(&p[4], 4, 0, 1);	/* a LAN bot on the same machine: never moved */
	CHECK(ae_party_follow(p, 5, 0, 0, 1) == 1);
	CHECK(p[1].team == 1 && p[2].team == 1 && p[3].team == 0 && p[4].team == 0);
	/* a name or colour change (old team == new team): nobody moves */
	set(&p[0], 4, 0, 0);
	set(&p[1], 4, 1, 0);
	CHECK(ae_party_follow(p, 2, 0, 0, 0) == 0 && p[1].team == 1);
	/* a sibling on the other team is not pulled back by a change of a third's */
	set(&p[0], 4, 1, 0);
	set(&p[1], 4, 1, 0);
	set(&p[2], 4, 0, 0);
	CHECK(ae_party_follow(p, 3, 0, 0, 1) == 1 && p[1].team == 1 && p[2].team == 1);
	set(&p[0], 4, 0, 0);
	set(&p[1], 4, 1, 0);
	CHECK(ae_party_follow(p, 2, 0, 1, 0) == 1 && p[1].team == 0);
	/* a bot sender, a bad index or team: nobody */
	set(&p[0], 4, 1, 1);
	set(&p[1], 4, 0, 0);
	CHECK(ae_party_follow(p, 2, 0, 0, 1) == 0 && p[1].team == 0);
	CHECK(ae_party_follow(p, 2, 5, 0, 1) == 0 && ae_party_follow(p, 2, 1, 0, 2) == 0 && ae_party_follow(NULL, 2, 1, 0, 1) == 0);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
