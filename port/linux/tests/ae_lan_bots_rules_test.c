#include <stdio.h>
#include "ae_lan_bots_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* only a LAN lobby being set up, of a gametype bots play, with a person of the host's machine in it */
static void test_eligible(void)
{
	int pregame, local, internet, played, person;

	for (pregame = 0; pregame < 2; pregame++)
		for (local = 0; local < 2; local++)
			for (internet = 0; internet < 2; internet++)
				for (played = 0; played < 2; played++)
					for (person = 0; person < 2; person++)
						CHECK(ae_lan_bots_eligible(pregame, local, internet, played, person) ==
							(pregame && !local && !internet && played && person));
}

/* a person's controller and a pad that wants to join are never a bot's */
static void test_free_controllers(void)
{
	CHECK(ae_lan_bots_free_controllers(0x1, 0x0) == 0xE);
	CHECK(ae_lan_bots_free_controllers(0x1, 0x4) == 0xA);
	CHECK(ae_lan_bots_free_controllers(0x3, 0xC) == 0x0);
	/* (bits past the four controllers are not slots) */
	CHECK(ae_lan_bots_free_controllers(0x10, 0x20) == 0xF);
}

static void test_target(void)
{
	/* off, or not eligible: none */
	CHECK(ae_lan_bots_target(0, 1, 1, 16, 0xE) == 0);
	CHECK(ae_lan_bots_target(3, 0, 1, 16, 0xE) == 0);
	CHECK(ae_lan_bots_target(-2, 1, 1, 16, 0xE) == 0);
	/* the setting, up to three (bots.count goes to 15 for local games) */
	CHECK(ae_lan_bots_target(1, 1, 1, 16, 0xE) == 1);
	CHECK(ae_lan_bots_target(3, 1, 1, 16, 0xE) == 3);
	CHECK(ae_lan_bots_target(15, 1, 1, 16, 0xE) == 3);
	/* as many as the free controllers: one human on the host leaves three, two leave two */
	CHECK(ae_lan_bots_target(3, 1, 2, 16, 0xC) == 2);
	CHECK(ae_lan_bots_target(3, 1, 4, 16, 0x0) == 0);
	/* one slot of the game always left free for a machine that joins */
	CHECK(ae_lan_bots_target(3, 1, 1, 4, 0xE) == 2);
	CHECK(ae_lan_bots_target(3, 1, 14, 16, 0xE) == 1);
	CHECK(ae_lan_bots_target(3, 1, 15, 16, 0xE) == 0);
	CHECK(ae_lan_bots_target(3, 1, 16, 16, 0xE) == 0);
	CHECK(ae_lan_bots_target(3, 1, 1, 2, 0xE) == 0);
}

/* the highest free controller no bot holds, so that people's pads 2 and 3 meet a bot as late as can be */
static void test_pick_controller(void)
{
	CHECK(ae_lan_bots_pick_controller(0xE, 0x0) == 3);
	CHECK(ae_lan_bots_pick_controller(0xE, 0x8) == 2);
	CHECK(ae_lan_bots_pick_controller(0xE, 0xC) == 1);
	CHECK(ae_lan_bots_pick_controller(0xE, 0xE) == -1);
	/* the host's person on pad 2 (controller 1): controller 0 is free */
	CHECK(ae_lan_bots_pick_controller(0xD, 0xC) == 0);
	CHECK(ae_lan_bots_pick_controller(0x0, 0x0) == -1);
}

static void test_team(void)
{
	/* against: the other team of the people's */
	CHECK(ae_lan_bots_team(1, 0, 5, 0) == 1);
	CHECK(ae_lan_bots_team(1, 1, 0, 5) == 0);
	CHECK(ae_lan_bots_team(1, -1, 0, 0) == 1);
	/* even: the smaller team, red when even */
	CHECK(ae_lan_bots_team(0, 0, 1, 0) == 1);
	CHECK(ae_lan_bots_team(0, 0, 1, 1) == 0);
	CHECK(ae_lan_bots_team(0, 1, 2, 3) == 0);
	CHECK(ae_lan_bots_team(0, 1, 3, 2) == 1);
}

int main(void)
{
	test_eligible();
	test_free_controllers();
	test_target();
	test_pick_controller();
	test_team();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
