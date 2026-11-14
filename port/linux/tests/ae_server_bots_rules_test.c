#include <stdio.h>
#include <string.h>
#include "ae_server_bots_rules.h"
#include "ae_lan_bots_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define MIN AE_SERVER_BOTS_FILL_MINIMUM
#define EMPTY AE_SERVER_BOTS_FILL_EMPTY_SLOTS
#define OFF AE_SERVER_BOTS_FILL_OFF

static void test_parse(void)
{
	CHECK(ae_server_bots_fill_parse("off") == OFF);
	CHECK(ae_server_bots_fill_parse("minimum") == MIN);
	CHECK(ae_server_bots_fill_parse("empty_slots") == EMPTY);
	CHECK(ae_server_bots_fill_parse("auto") == AE_SERVER_BOTS_FILL_AUTO);
	CHECK(ae_server_bots_fill_parse("all") == -1);
	CHECK(ae_server_bots_fill_parse(NULL) == -1);
	CHECK(ae_server_bots_skill_valid("normal") && ae_server_bots_skill_valid("demon") && ae_server_bots_skill_valid("recruit"));
	CHECK(ae_server_bots_skill_valid("elite") && !ae_server_bots_skill_valid("elit") && !ae_server_bots_skill_valid("") && !ae_server_bots_skill_valid(NULL));
	/* auto: minimum on an AE entry, off on a stock one */
	CHECK(ae_server_bots_fill_resolved(AE_SERVER_BOTS_FILL_AUTO, 1) == MIN);
	CHECK(ae_server_bots_fill_resolved(AE_SERVER_BOTS_FILL_AUTO, 0) == OFF);
	CHECK(ae_server_bots_fill_resolved(EMPTY, 0) == EMPTY);
	CHECK(ae_server_bots_fill_resolved(MIN, 0) == MIN);
	CHECK(ae_server_bots_fill_resolved(OFF, 1) == OFF);
}

/* fill per mode */
static void test_fill(void)
{
	/* off, or a gametype bots do not play: none */
	CHECK(ae_server_bots_wanted(OFF, 8, 1, 0, 1, 16, 0, 1) == 0);
	CHECK(ae_server_bots_wanted(MIN, 8, 1, 0, 1, 16, 0, 0) == 0);
	/* minimum: one human, minimum 8 -> 7 bots; 3 humans -> 5; at or past the minimum -> 0 */
	CHECK(ae_server_bots_wanted(MIN, 8, 1, 0, 1, 16, 0, 1) == 7);
	CHECK(ae_server_bots_wanted(MIN, 8, 3, 0, 1, 16, 0, 1) == 5);
	CHECK(ae_server_bots_wanted(MIN, 8, 8, 0, 1, 16, 0, 1) == 0);
	CHECK(ae_server_bots_wanted(MIN, 8, 12, 0, 1, 16, 0, 1) == 0);
	/* empty slots: up to the game's most players */
	CHECK(ae_server_bots_wanted(EMPTY, 8, 1, 0, 1, 16, 0, 1) == 15);
	CHECK(ae_server_bots_wanted(EMPTY, 8, 16, 0, 1, 16, 0, 1) == 0);
	/* never above the room: a minimum past the most players */
	CHECK(ae_server_bots_wanted(MIN, 20, 1, 0, 1, 16, 0, 1) == 15);
	CHECK(ae_server_bots_wanted(MIN, 8, 1, 0, 1, 4, 0, 1) == 3);
}

/* humans 0: idle without bots_alone, bot-only games with it */
static void test_alone(void)
{
	CHECK(ae_server_bots_wanted(MIN, 8, 0, 0, 1, 16, 0, 1) == 0);
	CHECK(ae_server_bots_wanted(EMPTY, 8, 0, 0, 1, 16, 0, 1) == 0);
	CHECK(ae_server_bots_wanted(MIN, 8, 0, 0, 1, 16, 1, 1) == 8);
	CHECK(ae_server_bots_wanted(EMPTY, 8, 0, 0, 1, 16, 1, 1) == 16);
	/* (a minimum of one is two bots alone: a game needs an opponent) */
	CHECK(ae_server_bots_wanted(MIN, 1, 0, 0, 1, 16, 1, 1) == 2);
	/* (with a human the plain rule) */
	CHECK(ae_server_bots_wanted(MIN, 1, 1, 0, 1, 16, 1, 1) == 0);
	CHECK(ae_server_bots_wanted(OFF, 8, 0, 0, 1, 16, 1, 1) == 0);
}

/* teams: the bots wanted plus the humans, evened by the lobby's/in-game rule */
static void test_team_evenness(void)
{
	int bots;

	/* 3 humans, minimum 7: 4 bots, 7 players: the game's rule adds one for even teams */
	bots = ae_server_bots_wanted(MIN, 7, 3, 0, 1, 16, 0, 1);
	CHECK(bots == 4);
	CHECK(ae_lan_bots_ingame_target(bots, 1, 3, 16, 127, 1, 0) == 5);
	/* an even total stays */
	bots = ae_server_bots_wanted(MIN, 8, 3, 0, 1, 16, 0, 1);
	CHECK(ae_lan_bots_ingame_target(bots, 1, 3, 16, 127, 1, 0) == 5);
	/* no teams: no extra */
	bots = ae_server_bots_wanted(MIN, 7, 3, 0, 1, 16, 0, 1);
	CHECK(ae_lan_bots_ingame_target(bots, 1, 3, 16, 127, 0, 0) == 4);
	/* the bots join the smaller team */
	CHECK(ae_lan_bots_team(0, 0, 3, 1) == 1);
	CHECK(ae_lan_bots_team(0, 0, 1, 3) == 0);
}

/* a human joins: the surplus bot leaves at its next death, from the larger team */
static void test_replace_at_death(void)
{
	int target = ae_server_bots_wanted(MIN, 8, 2, 0, 1, 16, 0, 1);
	int surplus;

	CHECK(target == 6);
	/* 7 bots there, 6 wanted: one surplus */
	surplus = ae_server_bots_surplus(7, target);
	CHECK(surplus == 1);
	CHECK(ae_server_bots_surplus(6, target) == 0);
	CHECK(ae_server_bots_surplus(4, target) == 0);
	/* it leaves on a death only, from the team the game's rule names (red 0, blue 1) */
	CHECK(!ae_server_bots_death_leaves(0, 1, 0, 0));
	CHECK(ae_server_bots_death_leaves(1, 1, 0, 0));
	CHECK(!ae_server_bots_death_leaves(1, 1, 1, 0));
	CHECK(!ae_server_bots_death_leaves(1, 1, 1, -1));
	/* no teams: any dead bot */
	CHECK(ae_server_bots_death_leaves(1, 0, 1, -1));
	/* the larger team's bot: red has 5 players with a bot, blue 3 */
	CHECK(ae_lan_bots_leaving_team(1, 2, 2, 5, 3) == 0);
	CHECK(ae_lan_bots_leaving_team(1, 2, 2, 3, 5) == 1);
	/* two humans joining one after the other: the surplus counts both */
	CHECK(ae_server_bots_surplus(7, ae_server_bots_wanted(MIN, 8, 3, 0, 1, 16, 0, 1)) == 2);
}

/* a dropped human's slot: an unrated game's bot holds it, a rated game's stays empty; the rejoin gets team and slot */
static void test_rejoin(void)
{
	struct ae_server_rejoin_table table;
	int index;

	ae_server_rejoin_clear(&table);
	CHECK(ae_server_rejoin_held(&table, 1) == 0 && ae_server_rejoin_held(&table, 0) == 0);
	/* 8 humans of minimum 8, one drops: unrated, a bot takes the slot at once even though the minimum is met */
	CHECK(ae_server_bots_wanted(MIN, 8, 7, 0, 1, 16, 0, 1) == 1);
	index = ae_server_rejoin_drop(&table, "Goat", 0x0A000030, 1, 1);
	CHECK(index == 0);
	CHECK(ae_server_rejoin_held(&table, 1) == 1);
	/* 3 humans of minimum 2: dropping one makes 2: the hold still asks for a bot */
	CHECK(ae_server_bots_wanted(MIN, 2, 2, 1, 1, 16, 0, 1) == 1);
	CHECK(ae_server_bots_wanted(MIN, 2, 2, 0, 1, 16, 0, 1) == 0);
	/* rated: no bot, the slot is out of the room */
	CHECK(ae_server_bots_wanted(MIN, 2, 2, 1, 0, 16, 0, 1) == 0);
	CHECK(ae_server_bots_wanted(EMPTY, 2, 2, 1, 0, 16, 0, 1) == 13);
	/* the same name from another machine is another human; the same name and address is the one back */
	CHECK(ae_server_rejoin_find(&table, "Goat", 0x0A000031) == -1);
	CHECK(ae_server_rejoin_find(&table, "Other", 0x0A000030) == -1);
	index = ae_server_rejoin_find(&table, "Goat", 0x0A000030);
	CHECK(index == 0 && table.entries[index].team == 1 && table.entries[index].bot_holds);
	ae_server_rejoin_take(&table, index);
	CHECK(ae_server_rejoin_held(&table, 1) == 0);
	CHECK(ae_server_rejoin_find(&table, "Goat", 0x0A000030) == -1);
	/* dropping twice replaces; the table fills up and says so */
	CHECK(ae_server_rejoin_drop(&table, "A", 1, 0, 1) == 0);
	CHECK(ae_server_rejoin_drop(&table, "A", 1, 1, 0) == 0);
	CHECK(ae_server_rejoin_held(&table, 1) == 0 && ae_server_rejoin_held(&table, 0) == 1);
	for (index = 1; index < AE_SERVER_REJOIN_MAXIMUM; index++)
	{
		char name[16];

		snprintf(name, sizeof(name), "P%d", index);
		CHECK(ae_server_rejoin_drop(&table, name, (unsigned long)index, 0, 1) == index);
	}
	CHECK(ae_server_rejoin_drop(&table, "Late", 99, 0, 1) == -1);
	/* a long name is cut the same way both times */
	ae_server_rejoin_clear(&table);
	CHECK(ae_server_rejoin_drop(&table, "ABCDEFGHIJKLMNOP", 5, 0, 1) == 0);
	CHECK(ae_server_rejoin_find(&table, "ABCDEFGHIJKLMNOP", 5) == 0);
}

static void test_human_room(void)
{
	/* a server of 8 seats, 8 bots (min 7 became 8/8 for even teams): the person who joins is let in, a bot gives way */
	CHECK(ae_server_bots_human_fits(0, 8) == 1);
	CHECK(ae_server_bots_human_needs_bot_gone(0, 8) == 1);
	/* a free seat: no bot goes */
	CHECK(ae_server_bots_human_fits(1, 7) == 1);
	CHECK(ae_server_bots_human_needs_bot_gone(1, 7) == 0);
	/* people only, no seat: full stays full */
	CHECK(ae_server_bots_human_fits(0, 0) == 0);
	CHECK(ae_server_bots_human_needs_bot_gone(0, 0) == 0);
	/* the wanted count follows: with the human in, 8 seats hold 7 bots (minimum 7, 1 human) */
	CHECK(ae_server_bots_wanted(AE_SERVER_BOTS_FILL_MINIMUM, 7, 1, 0, 1, 8, 0, 1) == 6);
	CHECK(ae_server_bots_surplus(8, 6) == 2);
}

int main(void)
{
	test_parse();
	test_fill();
	test_alone();
	test_team_evenness();
	test_replace_at_death();
	test_rejoin();
	test_human_room();
	printf("%d checks, %d failures\n", checks, failures);
	return failures != 0;
}
