#include <stdio.h>
#include <string.h>
#include "ae_lan_bots_rules.h"

static int failures;
static int checks;
#define CAP(mask) ae_lan_bots_capacity(0, (mask), 0)
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
	CHECK(ae_lan_bots_target(0, 1, 1, 16, CAP(0xE)) == 0);
	CHECK(ae_lan_bots_target(3, 0, 1, 16, CAP(0xE)) == 0);
	CHECK(ae_lan_bots_target(-2, 1, 1, 16, CAP(0xE)) == 0);
	/* the setting, up to three (bots.count goes to 15 for local games) */
	CHECK(ae_lan_bots_target(1, 1, 1, 16, CAP(0xE)) == 1);
	CHECK(ae_lan_bots_target(3, 1, 1, 16, CAP(0xE)) == 3);
	CHECK(ae_lan_bots_target(15, 1, 1, 16, CAP(0xE)) == 3);
	/* as many as the free controllers: one human on the host leaves three, two leave two */
	CHECK(ae_lan_bots_target(3, 1, 2, 16, CAP(0xC)) == 2);
	CHECK(ae_lan_bots_target(3, 1, 4, 16, CAP(0x0)) == 0);
	/* one slot of the game always left free for a machine that joins */
	CHECK(ae_lan_bots_target(3, 1, 1, 4, CAP(0xE)) == 2);
	CHECK(ae_lan_bots_target(3, 1, 14, 16, CAP(0xE)) == 1);
	CHECK(ae_lan_bots_target(3, 1, 15, 16, CAP(0xE)) == 0);
	CHECK(ae_lan_bots_target(3, 1, 16, 16, CAP(0xE)) == 0);
	CHECK(ae_lan_bots_target(3, 1, 1, 2, CAP(0xE)) == 0);
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

/* debug.network_bots: the host's free controllers, then four on each virtual machine, up to 127 */
static void test_capacity(void)
{
	CHECK(ae_lan_bots_capacity(0, 0xE, 31) == 3);
	CHECK(ae_lan_bots_capacity(0, 0xF, 0) == 3);
	CHECK(ae_lan_bots_capacity(1, 0xE, 0) == 3);
	CHECK(ae_lan_bots_capacity(1, 0xE, 3) == 15);
	CHECK(ae_lan_bots_capacity(1, 0x0, 4) == 16);
	CHECK(ae_lan_bots_capacity(1, 0xE, -1) == 3);
	CHECK(ae_lan_bots_capacity(1, 0xE, 31) == 127);
	CHECK(ae_lan_bots_capacity(1, 0xE, 100) == 127);
	CHECK(AE_NET_BOTS_MACHINES == 32);
	/* a BTB lobby of 16: one person, fifteen asked for, one slot left free: fourteen */
	CHECK(ae_lan_bots_target(15, 1, 1, 16, ae_lan_bots_capacity(1, 0xE, 3)) == 14);
	CHECK(ae_lan_bots_target(15, 1, 2, 16, ae_lan_bots_capacity(1, 0xE, 3)) == 13);
	CHECK(ae_lan_bots_target(127, 1, 1, 128, ae_lan_bots_capacity(1, 0xE, 32)) == 126);
	CHECK(ae_lan_bots_target(40, 1, 1, 128, ae_lan_bots_capacity(1, 0xE, 2)) == 11);
}

/* the start of a team game with odd players: one more bot, so the teams are even */
static void test_start_extra(void)
{
	/* 1 person + 14 bots of 16: the fifteenth bot, 8 against 8 */
	CHECK(ae_lan_bots_start_extra(1, 0, 14, 1, 16, 127, 0) == 1);
	/* 2 people + 13 bots: the fourteenth */
	CHECK(ae_lan_bots_start_extra(1, 0, 13, 2, 16, 127, 0) == 1);
	/* even already, FFA, "against", no bots, no room, no seat: none */
	CHECK(ae_lan_bots_start_extra(1, 0, 13, 1, 16, 127, 0) == 0);
	CHECK(ae_lan_bots_start_extra(0, 0, 14, 1, 16, 127, 0) == 0);
	CHECK(ae_lan_bots_start_extra(1, 1, 14, 1, 16, 127, 0) == 0);
	CHECK(ae_lan_bots_start_extra(1, 0, 0, 1, 16, 127, 0) == 0);
	CHECK(ae_lan_bots_start_extra(1, 0, 15, 2, 16, 127, 0) == 0);
	CHECK(ae_lan_bots_start_extra(1, 0, 14, 1, 15, 127, 0) == 0);
	CHECK(ae_lan_bots_start_extra(1, 0, 14, 1, 16, 14, 0) == 0);
	/* (review) host + 14 bots of 16, a machine joined as the countdown ended, its player not added yet: the
	free slot is that player's, no bot takes it */
	CHECK(ae_lan_bots_start_extra(1, 0, 14, 1, 16, 127, 1) == 0);
	CHECK(ae_lan_bots_start_extra(1, 0, 13, 2, 16, 127, 2) == 0);
}

static void test_pick_machine(void)
{
	int counts[AE_NET_BOTS_MACHINES] = { 4, 3, 4 };
	int index;

	CHECK(ae_lan_bots_pick_machine(counts, 0) == 0);
	CHECK(ae_lan_bots_pick_machine(counts, 1) == 1);
	CHECK(ae_lan_bots_pick_machine(counts, 3) == 1);
	counts[1] = 4;
	CHECK(ae_lan_bots_pick_machine(counts, 3) == 3);
	for (index = 0; index < AE_NET_BOTS_MACHINES; index++)
		counts[index] = 4;
	CHECK(ae_lan_bots_pick_machine(counts, AE_NET_BOTS_MACHINES) == -1);
	counts[AE_NET_BOTS_MACHINES - 1] = 0;
	CHECK(ae_lan_bots_pick_machine(counts, AE_NET_BOTS_MACHINES) == AE_NET_BOTS_MACHINES - 1);
}

static void test_name_number(void)
{
	CHECK(ae_lan_bots_name_number(0, 15) == 0);
	CHECK(ae_lan_bots_name_number(14, 15) == 0);
	CHECK(ae_lan_bots_name_number(15, 15) == 2);
	CHECK(ae_lan_bots_name_number(29, 15) == 2);
	CHECK(ae_lan_bots_name_number(30, 15) == 3);
	CHECK(ae_lan_bots_name_number(126, 15) == 9);
	CHECK(ae_lan_bots_name_number(5, 0) == 0);
}

/* "BOT " before a network game's bot's name, eleven characters at most, its number kept */
static void test_name(void)
{
	char name[12];

	ae_lan_bots_name("Johnson", 0, 1, name, sizeof(name));
	CHECK(!strcmp(name, "BOT Johnson"));
	ae_lan_bots_name("Fitzgerald", 0, 1, name, sizeof(name));
	CHECK(!strcmp(name, "BOT Fitzger"));
	ae_lan_bots_name("Johnson", 2, 1, name, sizeof(name));
	CHECK(!strcmp(name, "BOT Johnso2"));
	ae_lan_bots_name("Kojo", 9, 1, name, sizeof(name));
	CHECK(!strcmp(name, "BOT Kojo9"));
	/* untagged (the switch off): the name as bots.c has it */
	ae_lan_bots_name("Fitzgerald", 0, 0, name, sizeof(name));
	CHECK(!strcmp(name, "Fitzgerald"));
	ae_lan_bots_name("Fitzgerald", 3, 0, name, sizeof(name));
	CHECK(!strcmp(name, "Fitzgerald3"));
	ae_lan_bots_name("Johnson", 0, 1, name, 5);
	CHECK(!strcmp(name, "BOT "));
}

/* the game in progress: no slot kept free, a person takes a bot's place, a bot comes back when the person leaves */
static void test_ingame_target(void)
{
	/* BTB 16, bots.count 15: the host alone and 15 bots; a client joins: 14; two: 13; the client leaves: 14 again */
	CHECK(ae_lan_bots_ingame_target(15, 1, 1, 16, 127, 1, 0) == 15);
	CHECK(ae_lan_bots_ingame_target(15, 1, 2, 16, 127, 1, 0) == 14);
	CHECK(ae_lan_bots_ingame_target(15, 1, 3, 16, 127, 1, 0) == 13);
	/* odd players in a team game: one more (2 people and 7 bots are 9: 8 bots) */
	CHECK(ae_lan_bots_ingame_target(7, 1, 2, 128, 127, 1, 0) == 8);
	CHECK(ae_lan_bots_ingame_target(7, 1, 2, 128, 127, 0, 0) == 7);
	CHECK(ae_lan_bots_ingame_target(7, 1, 2, 128, 127, 1, 1) == 7);
	/* the game full of people, none asked, not eligible, no seat: none */
	CHECK(ae_lan_bots_ingame_target(15, 1, 16, 16, 127, 1, 0) == 0);
	CHECK(ae_lan_bots_ingame_target(0, 1, 2, 16, 127, 1, 0) == 0);
	CHECK(ae_lan_bots_ingame_target(15, 0, 2, 16, 127, 1, 0) == 0);
	CHECK(ae_lan_bots_ingame_target(15, 1, 2, 16, 3, 1, 0) == 3);
	/* a full game stays full with odd max: 15 slots, 2 people, 13 bots */
	CHECK(ae_lan_bots_ingame_target(15, 1, 2, 15, 127, 1, 0) == 13);
}

/* the game in progress's seats: those there, the free ones, four on each virtual machine it may still add */
static void test_ingame_capacity(void)
{
	/* a big game: 15 bots on the host's three and three virtual machines, no free seat: 29 more machines */
	CHECK(ae_lan_bots_ingame_capacity(15, 0, 29) == 127);
	CHECK(ae_lan_bots_ingame_capacity(15, 0, 4) == 31);
	CHECK(ae_lan_bots_ingame_capacity(15, 0, 0) == 15);
	CHECK(ae_lan_bots_ingame_capacity(13, 2, 0) == 15);
	CHECK(ae_lan_bots_ingame_capacity(0, 3, 1) == 7);
	/* (no more virtual machines than the bots take, no more than AE_NET_BOTS_MAXIMUM) */
	CHECK(ae_lan_bots_ingame_capacity(0, 0, 1000) == AE_NET_BOTS_MAXIMUM);
	CHECK(ae_lan_bots_ingame_capacity(-1, -1, -1) == 0);
	/* with ae_lan_bots_ingame_target: bots.count 31, host alone, max 128: 31 */
	CHECK(ae_lan_bots_ingame_target(31, 1, 1, 128, ae_lan_bots_ingame_capacity(15, 0, 29), 0, 0) == 31);
	/* ... max 16 (a full game): 15 */
	CHECK(ae_lan_bots_ingame_target(31, 1, 1, 16, ae_lan_bots_ingame_capacity(15, 0, 29), 0, 0) == 15);
}

/* four at a time towards the target, either way */
static void test_ingame_step(void)
{
	CHECK(ae_lan_bots_ingame_step(15, 31) == 4);
	CHECK(ae_lan_bots_ingame_step(28, 31) == 3);
	CHECK(ae_lan_bots_ingame_step(31, 31) == 0);
	CHECK(ae_lan_bots_ingame_step(31, 20) == -4);
	CHECK(ae_lan_bots_ingame_step(22, 20) == -2);
	CHECK(ae_lan_bots_ingame_step(0, 1) == 1);
	/* 15 to 31: four steps; 31 to 20: three */
	{
		int bots = 15, steps = 0;

		while (ae_lan_bots_ingame_step(bots, 31))
			bots += ae_lan_bots_ingame_step(bots, 31), steps++;
		CHECK(bots == 31 && steps == 4);
		steps = 0;
		while (ae_lan_bots_ingame_step(bots, 20))
			bots += ae_lan_bots_ingame_step(bots, 20), steps++;
		CHECK(bots == 20 && steps == 3);
	}
}

/* the wait before a step: short after the bots' own step the same way, else the two seconds the game settles */
static void test_ingame_wait(void)
{
	CHECK(ae_lan_bots_ingame_wait(4, 4) == AE_LAN_BOTS_STEP_TICKS);
	CHECK(ae_lan_bots_ingame_wait(1, 3) == AE_LAN_BOTS_STEP_TICKS);
	CHECK(ae_lan_bots_ingame_wait(-4, -2) == AE_LAN_BOTS_STEP_TICKS);
	/* the first step (the game started, or a person joined or left) */
	CHECK(ae_lan_bots_ingame_wait(0, 4) == AE_LAN_BOTS_SETTLE_TICKS);
	CHECK(ae_lan_bots_ingame_wait(0, -1) == AE_LAN_BOTS_SETTLE_TICKS);
	/* turning from leaving to joining (a seat let go is taken only once its player quit everywhere), and back */
	CHECK(ae_lan_bots_ingame_wait(-4, 4) == AE_LAN_BOTS_SETTLE_TICKS);
	CHECK(ae_lan_bots_ingame_wait(4, -4) == AE_LAN_BOTS_SETTLE_TICKS);
	CHECK(AE_LAN_BOTS_STEP_TICKS < AE_LAN_BOTS_SETTLE_TICKS);
	/* 16 bots after the start: 2 s, then three more steps */
	CHECK(AE_LAN_BOTS_SETTLE_TICKS + 3 * AE_LAN_BOTS_STEP_TICKS <= 10 * 30);
}

/* the bot that gives way: the larger team's, so the person joins the team it leaves (the host puts a joiner on
the smaller team, red when even) and the teams stay even */
static void test_leaving_team(void)
{
	CHECK(ae_lan_bots_leaving_team(1, 7, 7, 8, 8) == 0);
	CHECK(ae_lan_bots_leaving_team(1, 7, 7, 7, 8) == 1);
	CHECK(ae_lan_bots_leaving_team(1, 0, 7, 8, 8) == 1);
	CHECK(ae_lan_bots_leaving_team(1, 3, 0, 5, 8) == 0);
	CHECK(ae_lan_bots_leaving_team(1, 0, 0, 8, 8) == -1);
	CHECK(ae_lan_bots_leaving_team(0, 3, 3, 8, 8) == -1);
}

static void test_teams(void)
{
	int teams[4], chosen[4] = { -1, -1, -1, -1 };

	/* (none chosen: as ae_lan_bots_team one by one, the smaller team first) */
	ae_lan_bots_teams(4, NULL, 0, 0, 1, 0, teams);
	CHECK(teams[0] == 1 && teams[1] == 0 && teams[2] == 1 && teams[3] == 0);
	ae_lan_bots_teams(4, chosen, 0, 0, 1, 0, teams);
	CHECK(teams[0] == 1 && teams[1] == 0 && teams[2] == 1 && teams[3] == 0);
	/* (a bot's chosen team stays, and counts before the others: the others even up around it) */
	chosen[0] = 0;
	ae_lan_bots_teams(4, chosen, 0, 0, 1, 0, teams);
	CHECK(teams[0] == 0 && teams[1] == 1 && teams[2] == 1 && teams[3] == 0);
	chosen[0] = 1;
	chosen[3] = 1;
	ae_lan_bots_teams(4, chosen, 0, 0, 1, 0, teams);
	CHECK(teams[0] == 1 && teams[3] == 1 && teams[1] == 0 && teams[2] == 0);
	/* (against: the others take the people's other team, a chosen one still its own) */
	chosen[0] = 0;
	chosen[3] = -1;
	ae_lan_bots_teams(4, chosen, 1, 0, 1, 0, teams);
	CHECK(teams[0] == 0 && teams[1] == 1 && teams[2] == 1 && teams[3] == 1);
	ae_lan_bots_teams(0, chosen, 0, 0, 0, 0, teams);
}

int main(void)
{
	test_teams();
	test_eligible();
	test_free_controllers();
	test_target();
	test_pick_controller();
	test_team();
	test_capacity();
	test_start_extra();
	test_pick_machine();
	test_name_number();
	test_name();
	test_ingame_target();
	test_leaving_team();
	test_ingame_capacity();
	test_ingame_step();
	test_ingame_wait();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
