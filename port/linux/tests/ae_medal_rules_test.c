#include <stdio.h>
#include <string.h>
#include "ae_medal_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_medal_game game;
static struct ae_medal_out out;
static short teams[AE_MEDAL_RULES_PLAYERS];

static void new_game(int mode, int team_game)
{
	int index;

	memset(&game, 0, sizeof(game));
	memset(&out, 0, sizeof(out));
	ae_medal_rules_reset(&game, mode, team_game, 0, AE_MEDAL_BALL_NORMAL);
	for (index = 0; index < AE_MEDAL_RULES_PLAYERS; index++)
		teams[index] = (short)(team_game ? index % 2 : index);
}

/* a plain kill of the victim by the killer, with no damage facts */
static struct ae_medal_kill make_kill(int killer, int victim, long tick)
{
	struct ae_medal_kill kill;

	memset(&kill, 0, sizeof(kill));
	kill.tick = tick;
	kill.killer = killer;
	kill.victim = victim;
	kill.team_of = teams;
	kill.driver = -1;
	kill.betrayal = killer >= 0 && killer != victim && game.teams && teams[killer] == teams[victim];
	return kill;
}

static void do_kill(struct ae_medal_kill const *kill)
{
	ae_medal_rules_kill(&game, kill, &out);
}

static int count_of(int player, int medal)
{
	int index, count = 0;

	for (index = 0; index < out.count; index++)
		if (out.items[index].player == player && out.items[index].medal == medal)
			count++;
	return count;
}

static int has(int player, int medal)
{
	return count_of(player, medal) > 0;
}

/* how many medals the player has been given by the last call */
static int total(int player)
{
	int index, count = 0;

	for (index = 0; index < out.count; index++)
		if (out.items[index].player == player)
			count++;
	return count;
}

/* the position of the player's medal in the call's order, -1 for none */
static int position(int player, int medal)
{
	int index;

	for (index = 0; index < out.count; index++)
		if (out.items[index].player == player && out.items[index].medal == medal)
			return index;
	return -1;
}

/* player 0 kills, one by one, victims 2, 4, 6 ... (enemies in a team game: even and odd are teams, so use
free for all here), a tick apart; returns the call's result of the last */
/* the game's first kill is somebody else's (players 100 and 101), so that the next is not the first strike */
static void first_strike_used(void)
{
	struct ae_medal_kill kill = make_kill(100, 101, 1);

	do_kill(&kill);
}

static void kill_n(int killer, int first_victim, int count, long first_tick, long spacing)
{
	int index;

	for (index = 0; index < count; index++)
	{
		struct ae_medal_kill kill = make_kill(killer, first_victim + index, first_tick + index * spacing);

		do_kill(&kill);
	}
}

static void spree_test(void)
{
	static int const thresholds[] = { 5, 10, 15, 20, 25, 30, 35, 40 };
	static int const medals[] = {
		AE_MEDAL_KILLING_SPREE, AE_MEDAL_KILLING_FRENZY, AE_MEDAL_RUNNING_RIOT, AE_MEDAL_RAMPAGE,
		AE_MEDAL_UNTOUCHABLE_ROW_WIT_UNTOUCHABLE, AE_MEDAL_INVINCIBLE, AE_MEDAL_INCONCEIVABLE,
		AE_MEDAL_UNFRIGGENBELIEVABLE_ROW_WIT_UNFRIGGENBELIEVABLE };
	int index, kills;

	for (index = 0; index < 8; index++)
	{
		new_game(AE_MEDAL_MODE_SLAYER, 0);
		/* slow kills: a minute apart, so no multikill gets in the way */
		for (kills = 1; kills <= thresholds[index] + 1; kills++)
		{
			struct ae_medal_kill kill = make_kill(0, 1 + kills, kills * 3000L);

			do_kill(&kill);
			/* the medal comes on the kill that brings the count to its number: not before, not past it */
			CHECK(has(0, medals[index]) == (kills == thresholds[index]));
			CHECK(count_of(0, medals[index]) <= 1);
		}
	}
}

static void spree_reset_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill_n(0, 2, 4, 3000, 3000);
	/* a death ends the spree: five more kills make a spree again, not one earlier */
	kill = make_kill(5, 0, 20000);
	do_kill(&kill);
	CHECK(game.players[0].spree == 0);
	kill_n(0, 10, 4, 30000, 3000);
	CHECK(!has(0, AE_MEDAL_KILLING_SPREE));
	kill = make_kill(0, 20, 50000);
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_KILLING_SPREE));
	/* a suicide ends it too */
	kill = make_kill(0, 0, 60000);
	do_kill(&kill);
	CHECK(game.players[0].spree == 0);
}

static void multikill_test(void)
{
	static int const medals[] = {
		AE_MEDAL_DOUBLE_KILL, AE_MEDAL_TRIPLE_OF_EACH_KILL, AE_MEDAL_OVERKILL, AE_MEDAL_KILLTACULAR,
		AE_MEDAL_KILLTROCITY, AE_MEDAL_KILLIMANJARO, AE_MEDAL_KILLTASTROPHE, AE_MEDAL_KILLPOCALYPSE,
		AE_MEDAL_KILLIONAIRE };
	int number;

	for (number = 2; number <= 10; number++)
	{
		int kills;

		new_game(AE_MEDAL_MODE_SLAYER, 0);
		for (kills = 1; kills <= number + 1; kills++)
		{
			struct ae_medal_kill kill = make_kill(0, 1 + kills, 1000 + kills * 10L);

			do_kill(&kill);
			if (kills < number)
				CHECK(!has(0, medals[number - 2]));
			if (kills == number)
				CHECK(has(0, medals[number - 2]));
			if (kills == number + 1)
				CHECK(!has(0, medals[number - 2]));
		}
	}
	/* an eleventh in the window gives no multikill medal */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill_n(0, 2, 11, 1000, 10);
	CHECK(!has(0, AE_MEDAL_KILLIONAIRE) && game.players[0].multikill == 11);
}

static void multikill_window_test(void)
{
	struct ae_medal_kill kill;

	/* the second kill 120 ticks after the first is a double; 121 is not */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 2, 1000);
	do_kill(&kill);
	kill = make_kill(0, 3, 1120);
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_DOUBLE_KILL));
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 2, 1000);
	do_kill(&kill);
	kill = make_kill(0, 3, 1121);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_DOUBLE_KILL) && game.players[0].multikill == 1);
	/* the window is from the last kill: a chain of 120-tick gaps goes on */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill_n(0, 2, 3, 1000, 120);
	CHECK(has(0, AE_MEDAL_TRIPLE_OF_EACH_KILL));
	/* a death ends the multikill */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 2, 1000);
	do_kill(&kill);
	kill = make_kill(5, 0, 1010);
	do_kill(&kill);
	kill = make_kill(0, 3, 1020);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_DOUBLE_KILL));
}

static void weapon_run_test(void)
{
	static struct { int category; int five; int ten; } const runs[] = {
		{ AE_MEDAL_CATEGORY_SNIPER, AE_MEDAL_SNIPER_RIFLE_W_SPREE, AE_MEDAL_SHARPSHOOTER_RIFLE_W_SHARPSHOOTER },
		{ AE_MEDAL_CATEGORY_SHOTGUN, AE_MEDAL_SHOTGUN_SHOTGUN_SPREE, AE_MEDAL_OPEN_SEASON },
		{ AE_MEDAL_CATEGORY_VEHICLE, AE_MEDAL_SPLATTER_ROW_WIT_SPREE, AE_MEDAL_VEHICLE_ROW_WIT_MANSLAUGHTER },
	};
	int run, kills;

	for (run = 0; run < 3; run++)
	{
		new_game(AE_MEDAL_MODE_SLAYER, 0);
		for (kills = 1; kills <= 11; kills++)
		{
			struct ae_medal_kill kill = make_kill(0, 1 + kills, kills * 3000L);

			kill.category = runs[run].category;
			do_kill(&kill);
			CHECK(has(0, runs[run].five) == (kills == 5));
			CHECK(has(0, runs[run].ten) == (kills == 10));
		}
	}
	/* a kill of another category in between ends all the runs */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	for (kills = 1; kills <= 6; kills++)
	{
		struct ae_medal_kill kill = make_kill(0, 1 + kills, kills * 3000L);

		kill.category = kills == 3 ? 2 : AE_MEDAL_CATEGORY_SNIPER;
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_SNIPER_RIFLE_W_SPREE));
	}
	/* a death ends them too */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	for (kills = 1; kills <= 6; kills++)
	{
		struct ae_medal_kill kill = make_kill(0, 1 + kills, kills * 3000L);

		if (kills == 3)
		{
			struct ae_medal_kill death = make_kill(9, 0, kills * 3000L - 5);

			do_kill(&death);
		}
		kill.category = AE_MEDAL_CATEGORY_SNIPER;
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_SNIPER_RIFLE_W_SPREE));
	}
}

static void comeback_test(void)
{
	struct ae_medal_kill kill;
	int deaths;

	for (deaths = 1; deaths <= 4; deaths++)
	{
		int index;

		new_game(AE_MEDAL_MODE_SLAYER, 0);
		for (index = 0; index < deaths; index++)
		{
			kill = make_kill(9, 0, 100 + index * 10L);
			do_kill(&kill);
		}
		kill = make_kill(0, 2, 5000);
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_COMEBACK_KILL) == (deaths >= 3));
		/* only once: the next kill is a plain one */
		kill = make_kill(0, 3, 9000);
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_COMEBACK_KILL));
	}
	/* a kill in between ends the count */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(9, 0, 100);
	do_kill(&kill);
	kill = make_kill(9, 0, 110);
	do_kill(&kill);
	kill = make_kill(0, 2, 200);
	do_kill(&kill);
	kill = make_kill(9, 0, 300);
	do_kill(&kill);
	kill = make_kill(0, 3, 9000);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_COMEBACK_KILL));
}

static void revenge_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	/* 1 kills 0; 0 kills 1: revenge, once */
	kill = make_kill(1, 0, 100);
	do_kill(&kill);
	kill = make_kill(0, 1, 5000);
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_REVENGE));
	/* the field is spent: 1 dies to 0 again, no second revenge */
	kill = make_kill(1, 0, 9000);
	do_kill(&kill);
	kill = make_kill(0, 1, 12000);
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_REVENGE));
	kill = make_kill(0, 1, 15000);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_REVENGE));
	/* a third player's death is no revenge */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(1, 0, 100);
	do_kill(&kill);
	kill = make_kill(0, 2, 5000);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_REVENGE));
	/* a betrayal does not make a killer to take revenge on */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(2, 0, 100);
	CHECK(kill.betrayal);
	do_kill(&kill);
	kill = make_kill(0, 2, 5000);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_REVENGE));
}

static void avenger_test(void)
{
	struct ae_medal_kill kill;

	/* teams: even against odd. 1 kills 0 (they are enemies); 2 (0's teammate) kills 1: avenger */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(1, 0, 100);
	do_kill(&kill);
	kill = make_kill(2, 1, 5000);
	do_kill(&kill);
	CHECK(has(2, AE_MEDAL_AVENGER));
	/* the dead victim's own last victim is cleared when it died: 3 kills 1, no avenger for the next */
	kill = make_kill(2, 1, 6000);
	do_kill(&kill);
	CHECK(!has(2, AE_MEDAL_AVENGER));
	/* the killer himself is not an avenger (0 kills 1, who had killed 0) */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(1, 0, 100);
	do_kill(&kill);
	kill = make_kill(0, 1, 5000);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_AVENGER));
	/* free for all has no sides: no avenger */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(1, 0, 100);
	do_kill(&kill);
	kill = make_kill(2, 1, 5000);
	do_kill(&kill);
	CHECK(!has(2, AE_MEDAL_AVENGER));
	/* a betrayal clears the last victim: 1 betrays... teams even/odd, 2 betrays 4; 3 kills 2: not an avenger of 4 */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	game.players[2].last_victim = 7;
	kill = make_kill(2, 4, 100);
	CHECK(kill.betrayal);
	do_kill(&kill);
	CHECK(game.players[2].last_victim == -1);
}

static void killjoy_test(void)
{
	struct ae_medal_kill kill;
	int spree;

	for (spree = 4; spree <= 6; spree++)
	{
		int index;

		new_game(AE_MEDAL_MODE_SLAYER, 0);
		for (index = 0; index < spree; index++)
		{
			kill = make_kill(1, 10 + index, 1000 + index * 3000L);
			do_kill(&kill);
		}
		kill = make_kill(0, 1, 100000);
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_KILLJOY) == (spree >= 5));
		CHECK(game.players[1].spree == 0);
	}
}

static void assist_test(void)
{
	struct ae_medal_kill kill;

	/* teams: victim 1 (odd), killer 0, attackers 2 (teammate of killer), 3 (odd: the victim's own team), 4 */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(0, 1, 1000);
	kill.attacker_count = 4;
	kill.attackers[0].player = 0;
	kill.attackers[0].tick = 990;
	kill.attackers[0].damage = 100.0f;
	/* 40 of 100 is not past the share; 41 is */
	kill.attackers[1].player = 2;
	kill.attackers[1].tick = 900;
	kill.attackers[1].damage = 40.0f;
	kill.attackers[2].player = 4;
	kill.attackers[2].tick = 900;
	kill.attackers[2].damage = 41.0f;
	kill.attackers[3].player = 3;
	kill.attackers[3].tick = 900;
	kill.attackers[3].damage = 99.0f;
	do_kill(&kill);
	CHECK(!has(2, AE_MEDAL_ASSIST));
	CHECK(has(4, AE_MEDAL_ASSIST));
	/* the victim's teammate never assists, nor does the killer */
	CHECK(!has(3, AE_MEDAL_ASSIST) && !has(0, AE_MEDAL_ASSIST));
	/* assists come first */
	CHECK(position(4, AE_MEDAL_ASSIST) == 0);
	CHECK(game.players[4].assists == 1);
	/* the window: damage dealt 179 ticks before counts, 180 does not */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(0, 1, 1000);
	kill.attacker_count = 3;
	kill.attackers[0].player = 0;
	kill.attackers[0].tick = 990;
	kill.attackers[0].damage = 100.0f;
	kill.attackers[1].player = 2;
	kill.attackers[1].tick = 1000 - 179;
	kill.attackers[1].damage = 90.0f;
	kill.attackers[2].player = 4;
	kill.attackers[2].tick = 1000 - 180;
	kill.attackers[2].damage = 90.0f;
	do_kill(&kill);
	CHECK(has(2, AE_MEDAL_ASSIST) && !has(4, AE_MEDAL_ASSIST));
	/* a betrayal or a suicide gives no assist */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(2, 0, 1000);
	kill.attacker_count = 2;
	kill.attackers[0].player = 2;
	kill.attackers[0].tick = 990;
	kill.attackers[0].damage = 100.0f;
	kill.attackers[1].player = 1;
	kill.attackers[1].tick = 990;
	kill.attackers[1].damage = 100.0f;
	do_kill(&kill);
	CHECK(out.count == 0);
	/* one attacker in two records is one assist */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(0, 1, 1000);
	kill.attacker_count = 3;
	kill.attackers[0].player = 0;
	kill.attackers[0].tick = 990;
	kill.attackers[0].damage = 100.0f;
	kill.attackers[1].player = 2;
	kill.attackers[1].tick = 990;
	kill.attackers[1].damage = 90.0f;
	kill.attackers[2].player = 2;
	kill.attackers[2].tick = 990;
	kill.attackers[2].damage = 90.0f;
	do_kill(&kill);
	CHECK(count_of(2, AE_MEDAL_ASSIST) == 1);
}

static void grave_and_state_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	kill.killer_dead = 1;
	kill.victim_invisible = 1;
	kill.victim_reloading = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_KILL_FROM_THE_GRAVE) && has(0, AE_MEDAL_I_SEE_YOU) && has(0, AE_MEDAL_RELOAD_THIS));
	/* and the order: grave, I see you, reload this */
	CHECK(position(0, AE_MEDAL_KILL_FROM_THE_GRAVE) < position(0, AE_MEDAL_I_SEE_YOU));
	CHECK(position(0, AE_MEDAL_I_SEE_YOU) < position(0, AE_MEDAL_RELOAD_THIS));
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_KILL_FROM_THE_GRAVE) && !has(0, AE_MEDAL_I_SEE_YOU) && !has(0, AE_MEDAL_RELOAD_THIS));
	/* snapshot: a sniper category kill by a killer who was not zoomed */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	kill.killer_snapshot = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_SNAPSHOT));
	kill = make_kill(0, 2, 9000);
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_SNAPSHOT));
	kill = make_kill(0, 3, 19000);
	kill.category = 2;
	kill.killer_snapshot = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_SNAPSHOT));
	/* close call, protector (teams), wheelman (teams) */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(0, 1, 1000);
	kill.killer_close_call = 1;
	kill.protected_teammate = 1;
	kill.driver = 2;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_CLOSE_CALL) && has(0, AE_MEDAL_PROTECTOR) && has(2, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN));
	/* a driver on the enemy team gets nothing; the killer's own driving is no wheelman */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(0, 1, 1000);
	kill.driver = 3;
	do_kill(&kill);
	CHECK(!has(3, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN));
	kill = make_kill(0, 1, 5000);
	kill.driver = 0;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN));
	/* free for all: no protector, no wheelman */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	kill.protected_teammate = 1;
	kill.driver = 2;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_PROTECTOR) && !has(2, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN));
}

static void extermination_test(void)
{
	struct ae_medal_kill kill;
	int multi;

	for (multi = 3; multi <= 5; multi++)
	{
		int index;

		new_game(AE_MEDAL_MODE_SLAYER, 1);
		for (index = 1; index <= multi; index++)
		{
			kill = make_kill(0, 1 + 2 * index, 1000 + index * 10L);
			kill.victim_team_survivors = index < multi ? 3 : 0;
			do_kill(&kill);
		}
		CHECK(has(0, AE_MEDAL_EXTERMINATION) == (multi >= 4));
	}
	/* a survivor on the team: none */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill_n(0, 3, 3, 1000, 10);
	kill = make_kill(0, 9, 1040);
	kill.victim_team_survivors = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_EXTERMINATION));
	/* free for all: none */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill_n(0, 3, 3, 1000, 10);
	kill = make_kill(0, 9, 1040);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_EXTERMINATION));
}

static void weapon_medals_test(void)
{
	static struct { int weapon; int category; int medal; int headshot; } const cases[] = {
		{ AE_MEDAL_WEAPON_SNIPER, AE_MEDAL_CATEGORY_SNIPER, AE_MEDAL_SNIPER_KILL, 1 },
		{ AE_MEDAL_WEAPON_OTHER, AE_MEDAL_CATEGORY_VEHICLE, AE_MEDAL_SPLATTER_WITH_A__SPLATTER, 0 },
		{ AE_MEDAL_WEAPON_OTHER, AE_MEDAL_CATEGORY_MELEE, AE_MEDAL_MELEE_KILL, 0 },
		{ AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK, 3, AE_MEDAL_GRENADE_STICK, 0 },
		{ AE_MEDAL_WEAPON_BALL, AE_MEDAL_CATEGORY_MELEE, AE_MEDAL_BALL_KILL, 0 },
		{ AE_MEDAL_WEAPON_ROCKET, 4, AE_MEDAL_ROCKET_KILL, 0 },
		{ AE_MEDAL_WEAPON_NEEDLER_SUPERCOMBINE, 11, AE_MEDAL_SUPERCOMBINE_USING_A_SUPERCOMBINE, 0 },
		{ AE_MEDAL_WEAPON_OTHER, AE_MEDAL_CATEGORY_MOUNTED, AE_MEDAL_VEHICLE_A_VEHIC_KILL, 0 },
		{ AE_MEDAL_WEAPON_FRAG_GRENADE, 3, AE_MEDAL_GRENADE_KILL, 0 },
		{ AE_MEDAL_WEAPON_PLASMA_GRENADE, 3, AE_MEDAL_PLASMA_KILL, 0 },
		{ AE_MEDAL_WEAPON_FLAG, AE_MEDAL_CATEGORY_MELEE, AE_MEDAL_FLAG_KILL, 0 },
		{ AE_MEDAL_WEAPON_FUEL_ROD, 4, AE_MEDAL_FUEL_ROD_CANNON_KILL, 0 },
		{ AE_MEDAL_WEAPON_OTHER, 2, AE_MEDAL_HEADSHOT, 1 },
	};
	static int const weapon_medals[] = {
		AE_MEDAL_SNIPER_KILL, AE_MEDAL_SPLATTER_WITH_A__SPLATTER, AE_MEDAL_MELEE_KILL, AE_MEDAL_BEAT_DOWN,
		AE_MEDAL_GRENADE_STICK, AE_MEDAL_BALL_KILL, AE_MEDAL_ROCKET_KILL, AE_MEDAL_SUPERCOMBINE_USING_A_SUPERCOMBINE,
		AE_MEDAL_VEHICLE_A_VEHIC_KILL, AE_MEDAL_GRENADE_KILL, AE_MEDAL_PLASMA_KILL, AE_MEDAL_FLAG_KILL,
		AE_MEDAL_FUEL_ROD_CANNON_KILL, AE_MEDAL_HEADSHOT };
	int index, other;

	for (index = 0; index < (int)(sizeof(cases) / sizeof(cases[0])); index++)
	{
		struct ae_medal_kill kill;

		new_game(AE_MEDAL_MODE_SLAYER, 0);
		first_strike_used();
		kill = make_kill(0, 1, 1000);
		kill.weapon = cases[index].weapon;
		kill.category = cases[index].category;
		kill.headshot = cases[index].headshot;
		do_kill(&kill);
		CHECK(has(0, cases[index].medal));
		/* and no other weapon medal but the ones it comes with: the melee of a ball or a flag is a melee kill too */
		for (other = 0; other < (int)(sizeof(weapon_medals) / sizeof(weapon_medals[0])); other++)
		{
			int allowed = weapon_medals[other] == cases[index].medal;

			if (cases[index].medal == AE_MEDAL_BALL_KILL || cases[index].medal == AE_MEDAL_FLAG_KILL)
				allowed |= weapon_medals[other] == AE_MEDAL_MELEE_KILL;
			CHECK(allowed || !has(0, weapon_medals[other]));
		}
		/* a weapon's kill is no plain KILL */
		CHECK(!has(0, AE_MEDAL_KILL));
	}
	/* a sniper's body shot is no medal but the kill; a sniper's headshot is not also a HEADSHOT */
	{
		struct ae_medal_kill kill;

		new_game(AE_MEDAL_MODE_SLAYER, 0);
		first_strike_used();
		kill = make_kill(0, 1, 1000);
		kill.weapon = AE_MEDAL_WEAPON_SNIPER;
		kill.category = AE_MEDAL_CATEGORY_SNIPER;
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_SNIPER_KILL) && !has(0, AE_MEDAL_HEADSHOT) && has(0, AE_MEDAL_KILL));
		new_game(AE_MEDAL_MODE_SLAYER, 0);
		kill = make_kill(0, 1, 1000);
		kill.weapon = AE_MEDAL_WEAPON_SNIPER;
		kill.category = AE_MEDAL_CATEGORY_SNIPER;
		kill.headshot = 1;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_SNIPER_KILL) && !has(0, AE_MEDAL_HEADSHOT));
	}
	/* a plasma grenade that is not stuck is not a stick, and the needler's own needles are no supercombine */
	{
		struct ae_medal_kill kill;

		new_game(AE_MEDAL_MODE_SLAYER, 0);
		first_strike_used();
		kill = make_kill(0, 1, 1000);
		kill.weapon = AE_MEDAL_WEAPON_NEEDLER;
		kill.category = 11;
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_SUPERCOMBINE_USING_A_SUPERCOMBINE) && has(0, AE_MEDAL_KILL));
	}
}

static void melee_test(void)
{
	struct ae_medal_kill kill;

	/* a melee from behind: beat down, not a melee kill */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.killed_instantly = 1;
	kill.from_behind = 1;
	kill.side_effect = AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_BEAT_DOWN) && !has(0, AE_MEDAL_MELEE_KILL));
	/* the same blow from the front is a melee kill */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.killed_instantly = 1;
	kill.side_effect = AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_BEAT_DOWN) && has(0, AE_MEDAL_MELEE_KILL));
	/* one that was not lethal to the unsuspecting, or not an instant kill: a melee kill */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.killed_instantly = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_BEAT_DOWN) && has(0, AE_MEDAL_MELEE_KILL));
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.side_effect = AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_BEAT_DOWN) && has(0, AE_MEDAL_MELEE_KILL));
}

static void order_test(void)
{
	static int const order[] = {
		AE_MEDAL_KILLJOY, AE_MEDAL_OVERKILL, AE_MEDAL_KILL_FROM_THE_GRAVE, AE_MEDAL_I_SEE_YOU, AE_MEDAL_RELOAD_THIS,
		AE_MEDAL_SNAPSHOT, AE_MEDAL_REVENGE, AE_MEDAL_AVENGER, AE_MEDAL_SNIPER_KILL, AE_MEDAL_EXTERMINATION,
		AE_MEDAL_CLOSE_CALL, AE_MEDAL_PROTECTOR };
	struct ae_medal_kill kill;
	int index, last = -1;

	/* teams: even against odd. Player 1 kills 0, then 6 to 14 (a spree of six, the last a teammate of 0's); player 0
	kills 3, 5 and 7, and then 1 in the window with everything on */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(1, 0, 100);
	do_kill(&kill);
	for (index = 0; index < 5; index++)
	{
		kill = make_kill(1, 6 + 2 * index, 1000 + index * 3000L);
		do_kill(&kill);
	}
	for (index = 0; index < 3; index++)
	{
		kill = make_kill(0, 3 + 2 * index, 20000 + index * 10L);
		do_kill(&kill);
	}
	kill = make_kill(0, 1, 20030);
	kill.attacker_count = 2;
	kill.attackers[0].player = 0;
	kill.attackers[0].tick = 20020;
	kill.attackers[0].damage = 100.0f;
	kill.attackers[1].player = 4;
	kill.attackers[1].tick = 20020;
	kill.attackers[1].damage = 90.0f;
	kill.weapon = AE_MEDAL_WEAPON_SNIPER;
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	kill.headshot = 1;
	kill.victim_invisible = 1;
	kill.victim_reloading = 1;
	kill.killer_dead = 1;
	kill.killer_snapshot = 1;
	kill.driver = 2;
	kill.victim_team_survivors = 0;
	kill.killer_close_call = 1;
	kill.protected_teammate = 0;
	do_kill(&kill);
	CHECK(has(4, AE_MEDAL_ASSIST) && position(4, AE_MEDAL_ASSIST) == 0);
	CHECK(has(2, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN));
	for (index = 0; index < (int)(sizeof(order) / sizeof(order[0])); index++)
	{
		int where = position(0, order[index]);

		/* (protector is for a killer who is alive; this one is dead) */
		if (order[index] == AE_MEDAL_PROTECTOR)
		{
			CHECK(where < 0);
			continue;
		}
		CHECK(where >= 0 && where > last);
		last = where;
	}
	/* the wheelman comes after the weapon's medal and before the extermination */
	CHECK(position(2, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN) > position(0, AE_MEDAL_SNIPER_KILL));
	CHECK(position(2, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN) < position(0, AE_MEDAL_EXTERMINATION));
	/* with the killer alive, the protector comes after the close call, and no kill medal on top of the others */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(0, 1, 100);
	kill.killer_close_call = 1;
	kill.protected_teammate = 1;
	do_kill(&kill);
	CHECK(position(0, AE_MEDAL_CLOSE_CALL) < position(0, AE_MEDAL_PROTECTOR));
	CHECK(position(0, AE_MEDAL_PROTECTOR) < position(0, AE_MEDAL_FIRST_STRIKE));
	CHECK(!has(0, AE_MEDAL_KILL));
}

static void order_counters_test(void)
{
	struct ae_medal_kill kill;
	int index;

	/* a fifth kill within the window: spree, then multikill (killtacular), then runs, then comeback */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	for (index = 0; index < 4; index++)
	{
		kill = make_kill(9, 0, 100 + index * 10L);
		do_kill(&kill);
	}
	for (index = 0; index < 5; index++)
	{
		kill = make_kill(0, 2 + index, 1000 + index * 10L);
		kill.category = AE_MEDAL_CATEGORY_SNIPER;
		if (index == 4)
		{
			game.players[0].sniper_run = 0;
			game.players[0].sniper_run = 4;
			game.players[0].deaths_since_kill = 3;
		}
		do_kill(&kill);
	}
	CHECK(has(0, AE_MEDAL_KILLING_SPREE) && has(0, AE_MEDAL_KILLTACULAR) && has(0, AE_MEDAL_SNIPER_RIFLE_W_SPREE) &&
		has(0, AE_MEDAL_COMEBACK_KILL));
	CHECK(position(0, AE_MEDAL_KILLING_SPREE) < position(0, AE_MEDAL_KILLTACULAR));
	CHECK(position(0, AE_MEDAL_KILLTACULAR) < position(0, AE_MEDAL_SNIPER_RIFLE_W_SPREE));
	CHECK(position(0, AE_MEDAL_SNIPER_RIFLE_W_SPREE) < position(0, AE_MEDAL_COMEBACK_KILL));
}

static void first_strike_and_kill_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill = make_kill(0, 1, 1000);
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_FIRST_STRIKE) && !has(0, AE_MEDAL_KILL));
	kill = make_kill(2, 3, 2000);
	do_kill(&kill);
	CHECK(!has(2, AE_MEDAL_FIRST_STRIKE) && has(2, AE_MEDAL_KILL) && total(2) == 1);
	/* a first kill that is a betrayal or a suicide does not use the first strike */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill = make_kill(0, 2, 1000);
	CHECK(kill.betrayal);
	do_kill(&kill);
	CHECK(out.count == 0);
	kill = make_kill(3, 3, 1500);
	do_kill(&kill);
	CHECK(out.count == 0);
	kill = make_kill(-1, 3, 1600);
	do_kill(&kill);
	CHECK(out.count == 0);
	kill = make_kill(0, 1, 2000);
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_FIRST_STRIKE));
}

static void betrayal_test(void)
{
	struct ae_medal_kill kill;
	int index;

	/* betrayals do not count for the spree, the multikill, anything */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	for (index = 0; index < 8; index++)
	{
		kill = make_kill(0, 2 + 2 * index, 1000 + index * 5L);
		do_kill(&kill);
		CHECK(out.count == 0);
	}
	CHECK(game.players[0].spree == 0 && game.players[0].multikill == 0 && game.players[0].kills == 0);
	/* but the dead player's spree is over */
	kill = make_kill(1, 2, 3000);
	do_kill(&kill);
	kill = make_kill(2, 4, 3010);
	kill.betrayal = 1;
	do_kill(&kill);
	kill = make_kill(0, 1, 3020);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_KILLJOY));
	/* a betrayal in the middle of a spree does not break it, nor add to it */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	kill_n(0, 1, 1, 1000, 1);
	game.players[0].spree = 3;
	kill = make_kill(0, 2, 20000);
	do_kill(&kill);
	CHECK(game.players[0].spree == 3);
	/* the betrayed victim has died: their deaths count and their spree ends */
	CHECK(game.players[2].deaths == 1);
	/* no assist on a betrayal, no killjoy for the betrayer */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	for (index = 0; index < 5; index++)
	{
		kill = make_kill(2, 1 + 2 * index, 1000 + index * 3000L);
		do_kill(&kill);
	}
	kill = make_kill(4, 2, 30000);
	kill.betrayal = 1;
	do_kill(&kill);
	CHECK(!has(4, AE_MEDAL_KILLJOY));
}

static void flag_test(void)
{
	/* captures: 1 flag captured; 2 also the runner; 3 also the champion; 4 only the capture */
	int capture;

	new_game(AE_MEDAL_MODE_CTF, 1);
	for (capture = 1; capture <= 4; capture++)
	{
		out.count = 0;
		ae_medal_rules_flag(&game, 0, AE_MEDAL_FLAG_CAPTURE, &out);
		CHECK(has(0, AE_MEDAL_FLAG_CAPTURED));
		CHECK(has(0, AE_MEDAL_FLAG_RUNNER) == (capture == 2));
		CHECK(has(0, AE_MEDAL_FLAG_CHAMPION) == (capture == 3));
	}
	out.count = 0;
	ae_medal_rules_flag(&game, 1, AE_MEDAL_FLAG_GRAB, &out);
	CHECK(has(1, AE_MEDAL_FLAG_TAKEN) && out.count == 1);
	out.count = 0;
	ae_medal_rules_flag(&game, 1, AE_MEDAL_FLAG_RETURN, &out);
	CHECK(has(1, AE_MEDAL_FLAG_RETURNED) && out.count == 1);
	/* assault has none of them */
	memset(&game, 0, sizeof(game));
	ae_medal_rules_reset(&game, AE_MEDAL_MODE_CTF, 1, 1, AE_MEDAL_BALL_NORMAL);
	out.count = 0;
	ae_medal_rules_flag(&game, 0, AE_MEDAL_FLAG_CAPTURE, &out);
	ae_medal_rules_flag(&game, 0, AE_MEDAL_FLAG_GRAB, &out);
	ae_medal_rules_flag(&game, 0, AE_MEDAL_FLAG_RETURN, &out);
	CHECK(out.count == 0);
	/* the flag carrier's kill */
	{
		struct ae_medal_kill kill;

		new_game(AE_MEDAL_MODE_CTF, 1);
		kill = make_kill(0, 1, 1000);
		kill.victim_carried_flag = 1;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_FLAG_CARRIER_KILL) && !has(0, AE_MEDAL_KILL));
		/* a betrayal on the carrier: nothing */
		kill = make_kill(2, 4, 2000);
		kill.victim_carried_flag = 1;
		do_kill(&kill);
		CHECK(out.count == 0);
		/* not in Slayer, where no flag is */
		new_game(AE_MEDAL_MODE_SLAYER, 1);
		kill = make_kill(0, 1, 1000);
		kill.victim_carried_flag = 1;
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_FLAG_CARRIER_KILL));
	}
}

static void ball_test(void)
{
	static struct { long ticks; int medal; } const holds[] = {
		{ AE_MEDAL_BALL_HOLDER_TICKS, AE_MEDAL_BALL_HOLDER }, { AE_MEDAL_BALL_KEEPER_TICKS, AE_MEDAL_BALL_KEEPER },
		{ AE_MEDAL_BALL_MASTER_TICKS, AE_MEDAL_BALL_MASTER }, { AE_MEDAL_BALL_HOG_TICKS, AE_MEDAL_BALL_HOG },
		{ AE_MEDAL_MAGIC_HANDS_TICKS, AE_MEDAL_MAGIC_HANDS } };
	int index;
	long tick;

	for (index = 0; index < 5; index++)
	{
		int seen = 0;

		new_game(AE_MEDAL_MODE_ODDBALL, 0);
		for (tick = 1; tick <= 1900; tick++)
		{
			out.count = 0;
			ae_medal_rules_ball_tick(&game, 3, 1, &out);
			if (has(3, holds[index].medal))
			{
				CHECK(tick == holds[index].ticks);
				seen++;
			}
		}
		CHECK(seen == 1);
	}
	/* dropping it starts over: 299 ticks, a tick without, then 299 more is nothing */
	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	for (tick = 1; tick <= 299; tick++)
		ae_medal_rules_ball_tick(&game, 3, 1, &out);
	ae_medal_rules_ball_tick(&game, 3, 0, &out);
	out.count = 0;
	for (tick = 1; tick <= 299; tick++)
		ae_medal_rules_ball_tick(&game, 3, 1, &out);
	CHECK(!has(3, AE_MEDAL_BALL_HOLDER));
	/* first touch: the first pick up of the game, a normal ball, once */
	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	out.count = 0;
	ae_medal_rules_ball_tick(&game, 3, 1, &out);
	CHECK(has(3, AE_MEDAL_FIRST_TOUCH));
	out.count = 0;
	ae_medal_rules_ball_tick(&game, 3, 1, &out);
	ae_medal_rules_ball_tick(&game, 3, 0, &out);
	ae_medal_rules_ball_tick(&game, 4, 1, &out);
	CHECK(!has(4, AE_MEDAL_FIRST_TOUCH));
	/* the juggernaut ball has no hold medals, no first touch */
	memset(&game, 0, sizeof(game));
	ae_medal_rules_reset(&game, AE_MEDAL_MODE_ODDBALL, 0, 0, AE_MEDAL_BALL_JUGGERNAUT);
	out.count = 0;
	for (tick = 1; tick <= 400; tick++)
		ae_medal_rules_ball_tick(&game, 3, 1, &out);
	CHECK(out.count == 0);
}

static void ball_kill_test(void)
{
	struct ae_medal_kill kill;
	int index;

	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	kill = make_kill(0, 1, 1000);
	kill.victim_carried_ball = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_BALL_CARRIER_KILL) && !has(0, AE_MEDAL_KILLED_JUGGERNAUT));
	/* juggernaut: the juggernaut's death, and the juggernaut's own kills */
	memset(&game, 0, sizeof(game));
	ae_medal_rules_reset(&game, AE_MEDAL_MODE_ODDBALL, 0, 0, AE_MEDAL_BALL_JUGGERNAUT);
	kill = make_kill(0, 1, 1000);
	kill.victim_carried_ball = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_KILLED_JUGGERNAUT) && !has(0, AE_MEDAL_BALL_CARRIER_KILL));
	for (index = 1; index <= 11; index++)
	{
		kill = make_kill(0, 1 + index, 5000 + index * 3000L);
		kill.killer_carried_ball = 1;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_JUGGERNAUT_SPREE) == (index == 5));
		CHECK(has(0, AE_MEDAL_IMMOVABLE_OBJECT) == (index == 10));
	}
	/* the juggernaut's death starts the count again */
	kill = make_kill(5, 0, 90000);
	do_kill(&kill);
	CHECK(game.players[0].juggernaut_kills == 0);
	/* smooth with the moves: a normal ball in hand, a triple, the ball's third kill */
	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	for (index = 1; index <= 3; index++)
	{
		kill = make_kill(0, 1 + index, 1000 + index * 10L);
		kill.killer_carried_ball = 1;
		kill.weapon = AE_MEDAL_WEAPON_BALL;
		kill.category = AE_MEDAL_CATEGORY_MELEE;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_SMOOTH_WITH_TH_MOVES) == (index == 3));
	}
	/* with the ball's blows only twice, no */
	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	for (index = 1; index <= 3; index++)
	{
		kill = make_kill(0, 1 + index, 1000 + index * 10L);
		kill.killer_carried_ball = 1;
		kill.weapon = index == 2 ? AE_MEDAL_WEAPON_OTHER : AE_MEDAL_WEAPON_BALL;
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_SMOOTH_WITH_TH_MOVES));
	}
}

static void hill_test(void)
{
	struct ae_medal_kill kill;
	int index;
	long tick;

	/* hail to the king: five kills in a row from the hill */
	new_game(AE_MEDAL_MODE_KING, 0);
	for (index = 1; index <= 6; index++)
	{
		kill = make_kill(0, 1 + index, 1000 + index * 3000L);
		kill.killer_on_hill = 1;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_HAIL_TO_THE_KING) == (index == 5));
		CHECK(has(0, AE_MEDAL_HILL_DEFENSE));
		CHECK(!has(0, AE_MEDAL_HILL_OFFENSE));
	}
	/* a kill off the hill resets; a death resets */
	new_game(AE_MEDAL_MODE_KING, 0);
	for (index = 1; index <= 6; index++)
	{
		kill = make_kill(0, 1 + index, 1000 + index * 3000L);
		kill.killer_on_hill = index != 3;
		kill.victim_on_hill = 1;
		do_kill(&kill);
		CHECK(!has(0, AE_MEDAL_HAIL_TO_THE_KING));
		CHECK(has(0, AE_MEDAL_HILL_OFFENSE));
	}
	/* the first point of the game: the first on the hill */
	new_game(AE_MEDAL_MODE_KING, 0);
	out.count = 0;
	ae_medal_rules_hill_tick(&game, 2, 1, 0, 0, &out);
	CHECK(out.count == 0);
	ae_medal_rules_hill_tick(&game, 3, 1, 1, 0, &out);
	CHECK(has(3, AE_MEDAL_FIRST_POIINT));
	out.count = 0;
	ae_medal_rules_hill_tick(&game, 4, 1, 1, 0, &out);
	ae_medal_rules_hill_tick(&game, 3, 1, 1, 0, &out);
	CHECK(out.count == 0);
	/* held to the hill's move on foot: dominance; in a vehicle: vehicle dominance */
	for (index = 0; index < 4; index++)
	{
		new_game(AE_MEDAL_MODE_KING, 0);
		out.count = 0;
		for (tick = 0; tick < 100; tick++)
		{
			int in_vehicle = index == 1 || (index == 2 && tick == 50);

			ae_medal_rules_hill_tick(&game, 3, 1, 1, in_vehicle, &out);
			if (index == 3 && tick == 60)
				ae_medal_rules_hill_tick(&game, 3, 1, 0, 0, &out);
		}
		out.count = 0;
		ae_medal_rules_hill_moved(&game, &out);
		CHECK(has(3, AE_MEDAL_HILL_DOMINANCE) == (index == 0));
		CHECK(has(3, AE_MEDAL_VEHICLE_DOMINANCE) == (index == 1));
		CHECK(!has(3, AE_MEDAL_HILL_DOMINANCE) || !has(3, AE_MEDAL_VEHICLE_DOMINANCE));
		if (index >= 2)
			CHECK(out.count == 0);
	}
	/* death ends it too, and a held hill given once */
	new_game(AE_MEDAL_MODE_KING, 0);
	ae_medal_rules_hill_tick(&game, 3, 1, 1, 0, &out);
	ae_medal_rules_hill_tick(&game, 3, 0, 1, 0, &out);
	out.count = 0;
	ae_medal_rules_hill_moved(&game, &out);
	CHECK(out.count == 0);
	new_game(AE_MEDAL_MODE_KING, 0);
	ae_medal_rules_hill_tick(&game, 3, 1, 1, 0, &out);
	out.count = 0;
	ae_medal_rules_hill_moved(&game, &out);
	CHECK(has(3, AE_MEDAL_HILL_DOMINANCE));
	out.count = 0;
	ae_medal_rules_hill_moved(&game, &out);
	CHECK(out.count == 0);
}

static void race_test(void)
{
	int count;

	new_game(AE_MEDAL_MODE_RACE, 0);
	/* pole position: the first checkpoint by anyone, once */
	out.count = 0;
	ae_medal_rules_checkpoint(&game, 2, 100, &out);
	CHECK(has(2, AE_MEDAL_POLE_POSITION));
	out.count = 0;
	ae_medal_rules_checkpoint(&game, 3, 110, &out);
	CHECK(out.count == 0);
	/* redlined: the tenth checkpoint within 1800 ticks: 1799 yes, 1800 no; the ninth is not it, the eleventh no longer */
	new_game(AE_MEDAL_MODE_RACE, 0);
	for (count = 1; count <= 9; count++)
		ae_medal_rules_checkpoint(&game, 5, 100 + count, &out);
	out.count = 0;
	ae_medal_rules_checkpoint(&game, 5, 1799, &out);
	CHECK(has(5, AE_MEDAL_REDLINED_IT));
	out.count = 0;
	ae_medal_rules_checkpoint(&game, 5, 1799, &out);
	CHECK(!has(5, AE_MEDAL_REDLINED_IT));
	new_game(AE_MEDAL_MODE_RACE, 0);
	for (count = 1; count <= 9; count++)
		ae_medal_rules_checkpoint(&game, 5, 100 + count, &out);
	out.count = 0;
	ae_medal_rules_checkpoint(&game, 5, 1800, &out);
	CHECK(!has(5, AE_MEDAL_REDLINED_IT));
}

static void end_test(void)
{
	struct ae_medal_end end;
	struct ae_medal_kill kill;
	int index;

	/* last strike: the last to get a kill; triple double */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	memset(&end, 0, sizeof(end));
	end.used[0] = end.used[1] = end.used[2] = 1;
	end.winner[0] = 1;
	for (index = 0; index < 10; index++)
	{
		kill = make_kill(0, 1, 1000 + index * 3000L);
		kill.headshot = 1;
		kill.attacker_count = 2;
		kill.attackers[0].player = 0;
		kill.attackers[0].tick = kill.tick - 5;
		kill.attackers[0].damage = 100.0f;
		kill.attackers[1].player = 2;
		kill.attackers[1].tick = kill.tick - 5;
		kill.attackers[1].damage = 90.0f;
		do_kill(&kill);
	}
	kill = make_kill(1, 2, 90000);
	do_kill(&kill);
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(has(1, AE_MEDAL_LAST_STRIKE) && !has(0, AE_MEDAL_LAST_STRIKE));
	/* two kills in one tick: the later one is the last */
	{
		static struct ae_medal_game saved;
		struct ae_medal_kill same = make_kill(0, 2, 90000);

		saved = game;
		do_kill(&same);
		out.count = 0;
		ae_medal_rules_game_end(&game, &end, &out);
		CHECK(has(0, AE_MEDAL_LAST_STRIKE) && !has(1, AE_MEDAL_LAST_STRIKE));
		game = saved;
		out.count = 0;
		ae_medal_rules_game_end(&game, &end, &out);
	}
	/* the last striker gone from the game: the last kill of those still in it */
	{
		struct ae_medal_end left = end;

		left.used[1] = 0;
		out.count = 0;
		ae_medal_rules_game_end(&game, &left, &out);
		CHECK(has(0, AE_MEDAL_LAST_STRIKE) && !has(1, AE_MEDAL_LAST_STRIKE));
		out.count = 0;
	}
	CHECK(has(0, AE_MEDAL_TRIPLE_DOUBLE) == 0);
	CHECK(game.players[0].kills == 10 && game.players[0].headshots == 10 && game.players[2].assists == 10);
	game.players[0].assists = 10;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(has(0, AE_MEDAL_TRIPLE_DOUBLE));
	game.players[0].headshots = 9;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(!has(0, AE_MEDAL_TRIPLE_DOUBLE));
	game.players[0].headshots = 10;
	game.players[0].assists = 9;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(!has(0, AE_MEDAL_TRIPLE_DOUBLE));
	game.players[0].assists = 10;
	game.players[0].kills = 9;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(!has(0, AE_MEDAL_TRIPLE_DOUBLE));

	/* perfection: Slayer, first place, 15 kills, no deaths: 14, 15 and a death */
	for (index = 14; index <= 16; index++)
	{
		new_game(AE_MEDAL_MODE_SLAYER, 0);
		memset(&end, 0, sizeof(end));
		end.used[0] = end.used[1] = 1;
		end.winner[0] = 1;
		game.players[0].kills = index;
		out.count = 0;
		ae_medal_rules_game_end(&game, &end, &out);
		CHECK(has(0, AE_MEDAL_PERFECTION) == (index >= 15));
	}
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	memset(&end, 0, sizeof(end));
	end.used[0] = end.used[1] = 1;
	end.winner[0] = 1;
	game.players[0].kills = 15;
	game.players[0].deaths = 1;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(!has(0, AE_MEDAL_PERFECTION));
	/* not the winner */
	end.winner[0] = 0;
	game.players[0].deaths = 0;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(!has(0, AE_MEDAL_PERFECTION));
	/* not in other modes */
	new_game(AE_MEDAL_MODE_KING, 0);
	end.winner[0] = 1;
	game.players[0].kills = 20;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(!has(0, AE_MEDAL_PERFECTION) && !has(0, AE_MEDAL_STEAKTACULAR));

	/* steaktacular, free for all: top kills minus second 20 */
	for (index = 19; index <= 21; index++)
	{
		new_game(AE_MEDAL_MODE_SLAYER, 0);
		memset(&end, 0, sizeof(end));
		end.used[0] = end.used[1] = end.used[2] = 1;
		game.players[0].kills = index + 5;
		game.players[1].kills = 5;
		game.players[2].kills = 1;
		out.count = 0;
		ae_medal_rules_game_end(&game, &end, &out);
		CHECK(has(0, AE_MEDAL_STEAKTACULAR) == (index >= 20));
		CHECK(!has(1, AE_MEDAL_STEAKTACULAR));
	}
	/* teams: every player of the team that leads by 20 points */
	for (index = 19; index <= 21; index++)
	{
		new_game(AE_MEDAL_MODE_SLAYER, 1);
		memset(&end, 0, sizeof(end));
		end.used[0] = end.used[1] = end.used[2] = end.used[3] = 1;
		end.team[0] = end.team[2] = 0;
		end.team[1] = end.team[3] = 1;
		end.leading_team = 1;
		end.team_lead = index;
		out.count = 0;
		ae_medal_rules_game_end(&game, &end, &out);
		CHECK(has(1, AE_MEDAL_STEAKTACULAR) == (index >= 20) && has(3, AE_MEDAL_STEAKTACULAR) == (index >= 20));
		CHECK(!has(0, AE_MEDAL_STEAKTACULAR) && !has(2, AE_MEDAL_STEAKTACULAR));
	}

	/* race: first place */
	new_game(AE_MEDAL_MODE_RACE, 0);
	memset(&end, 0, sizeof(end));
	end.used[0] = end.used[1] = 1;
	end.winner[1] = 1;
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(has(1, AE_MEDAL_FIRST_PLACE) && !has(0, AE_MEDAL_FIRST_PLACE));
	/* and only in a race */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	out.count = 0;
	ae_medal_rules_game_end(&game, &end, &out);
	CHECK(!has(1, AE_MEDAL_FIRST_PLACE));
}

static void state_test(void)
{
	struct ae_medal_kill kill;

	/* a slot's new player has nothing of the last */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	kill_n(0, 2, 4, 3000, 3000);
	CHECK(game.players[0].spree == 4);
	ae_medal_rules_clear_player(&game, 0);
	CHECK(game.players[0].spree == 0 && game.players[0].last_killer == -1 && game.players[0].last_victim == -1);
	/* numbers from nowhere do nothing */
	kill = make_kill(0, 5000, 1000);
	do_kill(&kill);
	kill = make_kill(5000, 0, 1000);
	do_kill(&kill);
	kill = make_kill(-7, -3, 1000);
	do_kill(&kill);
	CHECK(out.count == 0);
	ae_medal_rules_flag(&game, 9999, AE_MEDAL_FLAG_CAPTURE, &out);
	ae_medal_rules_ball_tick(&game, -1, 1, &out);
	ae_medal_rules_checkpoint(&game, 128, 1, &out);
	ae_medal_rules_hill_tick(&game, 500, 1, 1, 0, &out);
	ae_medal_rules_clear_player(&game, -1);
	ae_medal_rules_clear_player(&game, 9999);
	CHECK(out.count == 0);
	/* an attacker count past the record's size is cut */
	kill = make_kill(0, 1, 1000);
	kill.attacker_count = 99;
	do_kill(&kill);
	/* each call's output starts empty */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	out.count = 7;
	kill = make_kill(0, 1, 1000);
	do_kill(&kill);
	CHECK(out.count > 0 && out.count <= AE_MEDAL_RULES_OUT && out.count < 7);
}

static void weapon_path_test(void)
{
	CHECK(ae_medal_rules_weapon(NULL) == AE_MEDAL_WEAPON_NONE);
	CHECK(ae_medal_rules_weapon("") == AE_MEDAL_WEAPON_OTHER);
	CHECK(ae_medal_rules_weapon("weapons\\sniper rifle\\sniper bullet") == AE_MEDAL_WEAPON_SNIPER);
	CHECK(ae_medal_rules_weapon("weapons/Sniper Rifle/sniper bullet") == AE_MEDAL_WEAPON_SNIPER);
	CHECK(ae_medal_rules_weapon("weapons\\frag grenade\\explosion") == AE_MEDAL_WEAPON_FRAG_GRENADE);
	CHECK(ae_medal_rules_weapon("weapons\\frag grenade\\shock wave") == AE_MEDAL_WEAPON_FRAG_GRENADE);
	CHECK(ae_medal_rules_weapon("weapons\\plasma grenade\\explosion") == AE_MEDAL_WEAPON_PLASMA_GRENADE);
	CHECK(ae_medal_rules_weapon("weapons\\plasma grenade\\shock wave") == AE_MEDAL_WEAPON_PLASMA_GRENADE);
	CHECK(ae_medal_rules_weapon("weapons\\plasma grenade\\attached") == AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK);
	CHECK(ae_medal_rules_weapon("weapons\\rocket launcher\\explosion") == AE_MEDAL_WEAPON_ROCKET);
	CHECK(ae_medal_rules_weapon("weapons\\rocket launcher\\melee") == AE_MEDAL_WEAPON_ROCKET);
	/* the needler: only its combined blast is the supercombine (its explosion and shock wave); its needles'
	impact, its detonation damage on a unit it is in, its trigger and its melee are not */
	CHECK(ae_medal_rules_weapon("weapons\\needler\\explosion") == AE_MEDAL_WEAPON_NEEDLER_SUPERCOMBINE);
	CHECK(ae_medal_rules_weapon("weapons\\needler\\shock wave") == AE_MEDAL_WEAPON_NEEDLER_SUPERCOMBINE);
	CHECK(ae_medal_rules_weapon("weapons\\needler\\impact damage") == AE_MEDAL_WEAPON_NEEDLER);
	CHECK(ae_medal_rules_weapon("weapons\\needler\\detonation damage") == AE_MEDAL_WEAPON_NEEDLER);
	CHECK(ae_medal_rules_weapon("weapons\\needler\\trigger") == AE_MEDAL_WEAPON_NEEDLER);
	CHECK(ae_medal_rules_weapon("weapons\\needler\\melee") == AE_MEDAL_WEAPON_NEEDLER);
	/* the fuel rod and the plasma cannon, either spelling */
	CHECK(ae_medal_rules_weapon("weapons\\fuel rod\\explosion") == AE_MEDAL_WEAPON_FUEL_ROD);
	CHECK(ae_medal_rules_weapon("weapons\\fuel_rod\\explosion") == AE_MEDAL_WEAPON_FUEL_ROD);
	CHECK(ae_medal_rules_weapon("weapons\\plasma cannon\\explosion") == AE_MEDAL_WEAPON_FUEL_ROD);
	CHECK(ae_medal_rules_weapon("weapons\\plasma_cannon\\bolt") == AE_MEDAL_WEAPON_FUEL_ROD);
	CHECK(ae_medal_rules_weapon("weapons\\flag\\melee") == AE_MEDAL_WEAPON_FLAG);
	CHECK(ae_medal_rules_weapon("weapons\\ball\\melee") == AE_MEDAL_WEAPON_BALL);
	CHECK(ae_medal_rules_weapon("weapons\\pistol\\bullet") == AE_MEDAL_WEAPON_OTHER);
	CHECK(ae_medal_rules_weapon("globals\\vehicle_collision") == AE_MEDAL_WEAPON_OTHER);
	/* a map with its own tag names has none */
	CHECK(ae_medal_rules_weapon("levels\\test\\damage\\thing") == AE_MEDAL_WEAPON_OTHER);
}

/* ---------- batch 2: kill facts, objective events */

static struct ae_medal_kill facts_kill(int killer, int victim, long tick)
{
	struct ae_medal_kill kill = make_kill(killer, victim, tick);

	kill.victim_stand_distance = -1.0f;
	return kill;
}

static void role_test(void)
{
	CHECK(ae_medal_rules_role(NULL) == AE_MEDAL_ROLE_NONE);
	CHECK(ae_medal_rules_role("weapons\\pistol\\pistol") == AE_MEDAL_ROLE_PISTOL);
	CHECK(ae_medal_rules_role("weapons\\plasma pistol\\plasma pistol") == AE_MEDAL_ROLE_PLASMA_PISTOL);
	CHECK(ae_medal_rules_role("Weapons/Plasma_Pistol/plasma_pistol") == AE_MEDAL_ROLE_PLASMA_PISTOL);
	CHECK(ae_medal_rules_role("weapons\\assault rifle\\assault rifle") == AE_MEDAL_ROLE_ASSAULT_RIFLE);
	CHECK(ae_medal_rules_role("weapons\\plasma rifle\\plasma rifle") == AE_MEDAL_ROLE_PLASMA_RIFLE);
	CHECK(ae_medal_rules_role("weapons\\smg\\smg") == AE_MEDAL_ROLE_SMG);
	CHECK(ae_medal_rules_role("weapons\\battle rifle\\battle rifle") == AE_MEDAL_ROLE_BATTLE_RIFLE);
	CHECK(ae_medal_rules_role("weapons\\carbine\\carbine") == AE_MEDAL_ROLE_CARBINE);
	CHECK(ae_medal_rules_role("weapons\\dmr\\dmr") == AE_MEDAL_ROLE_DMR);
	CHECK(ae_medal_rules_role("weapons\\evo rifle\\evo rifle") == AE_MEDAL_ROLE_EVO_RIFLE);
	CHECK(ae_medal_rules_role("weapons\\shotgun\\shotgun") == AE_MEDAL_ROLE_SHOTGUN);
	CHECK(ae_medal_rules_role("weapons\\sniper rifle\\sniper rifle") == AE_MEDAL_ROLE_SNIPER);
	CHECK(ae_medal_rules_role("weapons\\rocket launcher\\rocket launcher") == AE_MEDAL_ROLE_ROCKET);
	CHECK(ae_medal_rules_role("weapons\\plasma_cannon\\plasma_cannon") == AE_MEDAL_ROLE_FUEL_ROD);
	CHECK(ae_medal_rules_role("weapons\\fuel rod\\fuel rod") == AE_MEDAL_ROLE_FUEL_ROD);
	CHECK(ae_medal_rules_role("weapons\\energy sword\\energy sword") == AE_MEDAL_ROLE_SWORD);
	CHECK(ae_medal_rules_role("weapons\\gravity hammer\\hammer") == AE_MEDAL_ROLE_HAMMER);
	CHECK(ae_medal_rules_role("weapons\\needler\\needler") == AE_MEDAL_ROLE_NEEDLER);
	CHECK(ae_medal_rules_role("weapons\\flag\\flag") == AE_MEDAL_ROLE_OTHER);
	CHECK(ae_medal_rules_role("") == AE_MEDAL_ROLE_OTHER);
	/* the registry's medal_role words (ae_weapon_registry.h) */
	CHECK(ae_medal_rules_role_of_word("battle_rifle") == AE_MEDAL_ROLE_BATTLE_RIFLE && ae_medal_rules_role_of_word("EVO_RIFLE") == AE_MEDAL_ROLE_EVO_RIFLE);
	CHECK(ae_medal_rules_role_of_word("assault_rifle") == AE_MEDAL_ROLE_ASSAULT_RIFLE && ae_medal_rules_role_of_word("fuel_rod") == AE_MEDAL_ROLE_FUEL_ROD);
	CHECK(ae_medal_rules_role_of_word("melee") == AE_MEDAL_ROLE_OTHER && ae_medal_rules_role_of_word("hammer") == AE_MEDAL_ROLE_HAMMER);
	CHECK(ae_medal_rules_role_of_word("none") == AE_MEDAL_ROLE_NONE && ae_medal_rules_role_of_word("nonsense") == AE_MEDAL_ROLE_NONE &&
		ae_medal_rules_role_of_word(NULL) == AE_MEDAL_ROLE_NONE && ae_medal_rules_role_of_word("smgs") == AE_MEDAL_ROLE_NONE);
	CHECK(ae_medal_rules_role_is_power(AE_MEDAL_ROLE_SNIPER) && ae_medal_rules_role_is_power(AE_MEDAL_ROLE_ROCKET));
	CHECK(ae_medal_rules_role_is_power(AE_MEDAL_ROLE_FUEL_ROD) && ae_medal_rules_role_is_power(AE_MEDAL_ROLE_SWORD));
	CHECK(ae_medal_rules_role_is_power(AE_MEDAL_ROLE_HAMMER));
	CHECK(!ae_medal_rules_role_is_power(AE_MEDAL_ROLE_PISTOL) && !ae_medal_rules_role_is_power(AE_MEDAL_ROLE_NONE));
	CHECK(!ae_medal_rules_role_is_power(AE_MEDAL_ROLE_SHOTGUN) && !ae_medal_rules_role_is_power(AE_MEDAL_ROLE_NEEDLER));
}

static void yard_sale_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.victim_power_weapon = 1;
	kill.victim_inventory_full = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_YARD_SALE));
	kill = facts_kill(0, 2, 200);
	kill.victim_power_weapon = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_YARD_SALE));
	kill = facts_kill(0, 3, 300);
	kill.victim_inventory_full = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_YARD_SALE));
	/* a suicide or a betrayal is no sale */
	kill = facts_kill(4, 4, 400);
	kill.victim_power_weapon = 1;
	kill.victim_inventory_full = 1;
	do_kill(&kill);
	CHECK(!has(4, AE_MEDAL_YARD_SALE));
}

static void melee_style_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	/* a blow from behind that kills outright: the plain one is a beat down */
	kill = facts_kill(0, 1, 100);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.from_behind = 1;
	kill.killed_instantly = 1;
	kill.side_effect = AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_BEAT_DOWN) && !has(0, AE_MEDAL_SNEAK_KING) && !has(0, AE_MEDAL_ACHILLES_SPINE));
	/* camouflaged victim: SNEAK KING in its place */
	kill = facts_kill(0, 2, 200);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.from_behind = 1;
	kill.killed_instantly = 1;
	kill.side_effect = AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING;
	kill.victim_invisible = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_SNEAK_KING) && !has(0, AE_MEDAL_BEAT_DOWN) && !has(0, AE_MEDAL_MELEE_KILL));
	/* overshield on: ACHILLES SPINE, even camouflaged */
	kill.victim = 3;
	kill.tick = 300;
	kill.victim_overshield = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_ACHILLES_SPINE) && !has(0, AE_MEDAL_SNEAK_KING) && !has(0, AE_MEDAL_BEAT_DOWN));
	/* not from behind: neither */
	kill = facts_kill(0, 4, 400);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.victim_invisible = 1;
	kill.victim_overshield = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_MELEE_KILL) && !has(0, AE_MEDAL_SNEAK_KING) && !has(0, AE_MEDAL_ACHILLES_SPINE));
	/* the ninja: it comes, and the beat down gives way to the plain melee kill */
	kill = facts_kill(0, 5, 500);
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	kill.from_behind = 1;
	kill.killed_instantly = 1;
	kill.side_effect = AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING;
	kill.ninja = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_NINJA) && !has(0, AE_MEDAL_BEAT_DOWN) && has(0, AE_MEDAL_MELEE_KILL));
}

static void last_shot_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.category = AE_MEDAL_CATEGORY_BULLET;
	kill.killer_magazine_empty = 1;
	kill.killer_role = AE_MEDAL_ROLE_PISTOL;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_LAST_SHOT));
	/* a rounds left: no */
	kill.victim = 2;
	kill.killer_magazine_empty = 0;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_LAST_SHOT));
	/* a power weapon, a shotgun, a melee blow: no */
	kill.victim = 3;
	kill.killer_magazine_empty = 1;
	kill.killer_role = AE_MEDAL_ROLE_SNIPER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_LAST_SHOT));
	kill.victim = 4;
	kill.killer_role = AE_MEDAL_ROLE_SHOTGUN;
	kill.category = AE_MEDAL_CATEGORY_SHOTGUN;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_LAST_SHOT));
	kill.victim = 5;
	kill.killer_role = AE_MEDAL_ROLE_PISTOL;
	kill.category = AE_MEDAL_CATEGORY_MELEE;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_LAST_SHOT));
}

static void grenade_pair_test(void)
{
	struct ae_medal_kill kill;

	/* hail mary: a grenade over 11.7 units at the kill */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.weapon = AE_MEDAL_WEAPON_FRAG_GRENADE;
	kill.category = AE_MEDAL_CATEGORY_GRENADE;
	kill.distance = 11.8f;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_HAIL_MARY));
	kill.victim = 2;
	kill.tick = 1000;
	kill.distance = 11.7f;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_HAIL_MARY));
	kill.victim = 3;
	kill.tick = 2000;
	kill.weapon = AE_MEDAL_WEAPON_PLASMA_GRENADE;
	kill.distance = 30.0f;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_HAIL_MARY));
	kill.victim = 4;
	kill.tick = 3000;
	kill.weapon = AE_MEDAL_WEAPON_OTHER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_HAIL_MARY));
	kill.victim = 5;
	kill.tick = 4000;
	kill.weapon = AE_MEDAL_WEAPON_FRAG_GRENADE;
	kill.distance = 0.0f;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_HAIL_MARY));

	/* cluster luck: two grenade kills within 3 ticks */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.weapon = AE_MEDAL_WEAPON_FRAG_GRENADE;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_CLUSTER_LUCK));
	kill.victim = 2;
	kill.tick = 103;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_CLUSTER_LUCK));
	/* the pair is spent: the next kill at once starts a new one */
	kill.victim = 3;
	kill.tick = 104;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_CLUSTER_LUCK));
	kill.victim = 4;
	kill.tick = 107;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_CLUSTER_LUCK));
	/* 4 ticks apart is no pair */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.weapon = AE_MEDAL_WEAPON_PLASMA_GRENADE;
	do_kill(&kill);
	kill.victim = 2;
	kill.tick = 104;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_CLUSTER_LUCK));
	/* a death in between ends the pair */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.weapon = AE_MEDAL_WEAPON_FRAG_GRENADE;
	do_kill(&kill);
	kill = facts_kill(7, 0, 101);
	do_kill(&kill);
	kill = facts_kill(0, 2, 102);
	kill.weapon = AE_MEDAL_WEAPON_FRAG_GRENADE;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_CLUSTER_LUCK));
	/* a grenade and a sniper shot are not a pair */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.weapon = AE_MEDAL_WEAPON_FRAG_GRENADE;
	do_kill(&kill);
	kill = facts_kill(0, 2, 101);
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_CLUSTER_LUCK) && !has(0, AE_MEDAL_QUIGLEY));
}

static void quigley_autopilot_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_QUIGLEY));
	kill.victim = 2;
	kill.tick = 103;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_QUIGLEY));
	kill.victim = 3;
	kill.tick = 200;
	do_kill(&kill);
	kill.victim = 4;
	kill.tick = 204;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_QUIGLEY));

	/* autopilot engaged: a sniper kill of a driver of a moving vehicle */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	kill.victim_driving = 1;
	kill.victim_vehicle_speed = 1.5f;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_AUTOPILOT_ENGAGED));
	kill.victim = 2;
	kill.tick = 1000;
	kill.victim_vehicle_speed = 1.0f;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_AUTOPILOT_ENGAGED));
	kill.victim = 3;
	kill.tick = 2000;
	kill.victim_vehicle_speed = 8.0f;
	kill.victim_driving = 0;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_AUTOPILOT_ENGAGED));
	kill.victim = 4;
	kill.tick = 3000;
	kill.victim_driving = 1;
	kill.category = AE_MEDAL_CATEGORY_BULLET;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_AUTOPILOT_ENGAGED));
}

static void fumble_test(void)
{
	struct ae_medal_kill kill;
	long to_win = 10 * 60 * 30;

	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.victim_carried_ball = 1;
	kill.victim_score = to_win * 95 / 100;
	kill.victim_score_to_win = to_win;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_FUMBLE) && has(0, AE_MEDAL_BALL_CARRIER_KILL));
	kill.victim = 2;
	kill.tick = 200;
	kill.victim_score = to_win * 95 / 100 - 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_FUMBLE));
	kill.victim = 3;
	kill.tick = 300;
	kill.victim_score = to_win;
	kill.victim_carried_ball = 0;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_FUMBLE));
	kill.victim = 4;
	kill.tick = 400;
	kill.victim_carried_ball = 1;
	kill.victim_score_to_win = 0;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_FUMBLE));
	/* the other balls are not scored by a share of the time to win */
	{
		int ball_type;

		for (ball_type = AE_MEDAL_BALL_REVERSE_TAG; ball_type <= AE_MEDAL_BALL_JUGGERNAUT; ball_type++)
		{
			ae_medal_rules_reset(&game, AE_MEDAL_MODE_ODDBALL, 0, 0, ball_type);
			first_strike_used();
			kill = facts_kill(0, 1, 100);
			kill.victim_carried_ball = 1;
			kill.victim_score = to_win;
			kill.victim_score_to_win = to_win;
			do_kill(&kill);
			CHECK(!has(0, AE_MEDAL_FUMBLE));
		}
	}
}

static void flag_kill_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_CTF, 1);
	first_strike_used();
	/* player 0 (team 0) kills player 1 (team 1), who carries the flag, flag in hand too */
	kill = facts_kill(0, 1, 100);
	kill.victim_carried_flag = 1;
	kill.killer_carried_flag = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_FLAG_JOUST) && has(0, AE_MEDAL_FLAG_CARRIER_KILL));
	kill = facts_kill(0, 1, 200);
	kill.victim_carried_flag = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_FLAG_JOUST));
	kill = facts_kill(0, 1, 300);
	kill.killer_carried_flag = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_FLAG_JOUST));
	/* stopped short: the carrier killed within 2 units of the stand it scores at */
	kill = facts_kill(0, 1, 400);
	kill.victim_carried_flag = 1;
	kill.victim_stand_distance = 1.9f;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_STOPPED_SHORT));
	kill = facts_kill(0, 1, 500);
	kill.victim_carried_flag = 1;
	kill.victim_stand_distance = 2.0f;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_STOPPED_SHORT));
	kill = facts_kill(0, 1, 600);
	kill.victim_carried_flag = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_STOPPED_SHORT));
	kill = facts_kill(0, 1, 700);
	kill.victim_stand_distance = 0.5f;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_STOPPED_SHORT));
	/* none of it in assault, where the flag is the bomb's */
	memset(&game, 0, sizeof(game));
	ae_medal_rules_reset(&game, AE_MEDAL_MODE_CTF, 1, 1, AE_MEDAL_BALL_NORMAL);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.victim_carried_flag = 1;
	kill.killer_carried_flag = 1;
	kill.victim_stand_distance = 0.5f;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_FLAG_JOUST) && !has(0, AE_MEDAL_STOPPED_SHORT));
}

static void goal_line_test(void)
{
	new_game(AE_MEDAL_MODE_CTF, 1);
	out.count = 0;
	ae_medal_rules_flag_at(&game, 0, AE_MEDAL_FLAG_RETURN, 1.9f, &out);
	CHECK(has(0, AE_MEDAL_FLAG_RETURNED) && has(0, AE_MEDAL_GOAL_LINE_STAND));
	out.count = 0;
	ae_medal_rules_flag_at(&game, 0, AE_MEDAL_FLAG_RETURN, 2.0f, &out);
	CHECK(has(0, AE_MEDAL_FLAG_RETURNED) && !has(0, AE_MEDAL_GOAL_LINE_STAND));
	out.count = 0;
	ae_medal_rules_flag_at(&game, 0, AE_MEDAL_FLAG_RETURN, -1.0f, &out);
	CHECK(has(0, AE_MEDAL_FLAG_RETURNED) && !has(0, AE_MEDAL_GOAL_LINE_STAND));
	/* a grab or a capture near a stand is not one */
	out.count = 0;
	ae_medal_rules_flag_at(&game, 0, AE_MEDAL_FLAG_GRAB, 0.5f, &out);
	CHECK(has(0, AE_MEDAL_FLAG_TAKEN) && !has(0, AE_MEDAL_GOAL_LINE_STAND));
	/* the old call is the same with no distance */
	out.count = 0;
	ae_medal_rules_flag(&game, 0, AE_MEDAL_FLAG_RETURN, &out);
	CHECK(out.count == 1 && has(0, AE_MEDAL_FLAG_RETURNED));
}

/* a gun kill with the weapon the role names, as player 0 of a free for all */
static void gun_kill(int victim, long tick, int role, int category)
{
	struct ae_medal_kill kill = facts_kill(0, victim, tick);

	kill.killer_role = role;
	kill.category = category;
	do_kill(&kill);
}

static void proficiency_test(void)
{
	static struct { int medal; int role; int category; } const guns[] = {
		{ AE_MEDAL_GUNSLINGER, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET },
		{ AE_MEDAL_GUNSLINGER, AE_MEDAL_ROLE_PLASMA_PISTOL, AE_MEDAL_CATEGORY_PLASMA },
		{ AE_MEDAL_RIFLEMAN, AE_MEDAL_ROLE_ASSAULT_RIFLE, AE_MEDAL_CATEGORY_BULLET },
		{ AE_MEDAL_RIFLEMAN, AE_MEDAL_ROLE_SMG, AE_MEDAL_CATEGORY_BULLET },
		{ AE_MEDAL_RIFLEMAN, AE_MEDAL_ROLE_BATTLE_RIFLE, AE_MEDAL_CATEGORY_BULLET },
		{ AE_MEDAL_RIFLEMAN, AE_MEDAL_ROLE_EVO_RIFLE, AE_MEDAL_CATEGORY_BULLET },
		{ AE_MEDAL_RIFLEMAN, AE_MEDAL_ROLE_DMR, AE_MEDAL_CATEGORY_BULLET },
		{ AE_MEDAL_RIFLEMAN, AE_MEDAL_ROLE_CARBINE, AE_MEDAL_CATEGORY_BULLET },
		{ AE_MEDAL_RIFLEMAN, AE_MEDAL_ROLE_PLASMA_RIFLE, AE_MEDAL_CATEGORY_PLASMA },
	};
	int index, count;

	for (index = 0; index < (int)(sizeof(guns) / sizeof(guns[0])); index++)
	{
		new_game(AE_MEDAL_MODE_SLAYER, 0);
		first_strike_used();
		for (count = 1; count <= 7; count++)
		{
			gun_kill(10 + count, count * 3000L, guns[index].role, guns[index].category);
			CHECK(has(0, guns[index].medal) == (count == 5));
		}
	}
	/* the pistol's and the rifle's kills are counted apart: two of one, three of the other are nothing */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	gun_kill(11, 3000, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET);
	gun_kill(12, 6000, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET);
	gun_kill(13, 9000, AE_MEDAL_ROLE_ASSAULT_RIFLE, AE_MEDAL_CATEGORY_BULLET);
	gun_kill(14, 12000, AE_MEDAL_ROLE_ASSAULT_RIFLE, AE_MEDAL_CATEGORY_BULLET);
	gun_kill(15, 15000, AE_MEDAL_ROLE_ASSAULT_RIFLE, AE_MEDAL_CATEGORY_BULLET);
	CHECK(!has(0, AE_MEDAL_GUNSLINGER) && !has(0, AE_MEDAL_RIFLEMAN));
	/* a death does not reset the count: five in the game */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 4; count++)
		gun_kill(10 + count, count * 3000L, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET);
	do_kill(&(struct ae_medal_kill){ .tick = 14000, .killer = 50, .victim = 0, .team_of = teams, .driver = -1 });
	gun_kill(20, 15000, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET);
	CHECK(has(0, AE_MEDAL_GUNSLINGER));
	/* a melee blow with a pistol in hand, a grenade with a rifle in hand: not a gun's kill */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 5; count++)
		gun_kill(10 + count, count * 3000L, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_MELEE);
	CHECK(!has(0, AE_MEDAL_GUNSLINGER));
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 5; count++)
		gun_kill(10 + count, count * 3000L, AE_MEDAL_ROLE_ASSAULT_RIFLE, AE_MEDAL_CATEGORY_GRENADE);
	CHECK(!has(0, AE_MEDAL_RIFLEMAN));
	/* the shotgun by its damage */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		gun_kill(10 + count, count * 3000L, AE_MEDAL_ROLE_SHOTGUN, AE_MEDAL_CATEGORY_SHOTGUN);
		CHECK(has(0, AE_MEDAL_SCATTERGUNNER) == (count == 5));
	}
	/* the bomber: a rocket or a fuel rod's damage, or the plasma cannon's */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		struct ae_medal_kill kill = facts_kill(0, 10 + count, count * 3000L);

		kill.weapon = count % 2 ? AE_MEDAL_WEAPON_ROCKET : AE_MEDAL_WEAPON_FUEL_ROD;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_BOMBER) == (count == 5));
	}
	/* the grenadier: any grenade */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		struct ae_medal_kill kill = facts_kill(0, 10 + count, count * 3000L);

		kill.weapon = count % 3 == 0 ? AE_MEDAL_WEAPON_PLASMA_GRENADE : count % 3 == 1 ? AE_MEDAL_WEAPON_FRAG_GRENADE :
			AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_GRENADIER) == (count == 5));
	}
	/* the boxer: melee, a beat down too */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		struct ae_medal_kill kill = facts_kill(0, 10 + count, count * 3000L);

		kill.category = AE_MEDAL_CATEGORY_MELEE;
		kill.from_behind = count % 2;
		kill.killed_instantly = count % 2;
		kill.side_effect = count % 2 ? AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING : 0;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_BOXER) == (count == 5));
	}
}

static void vehicle_proficiency_test(void)
{
	int count;
	struct ae_medal_kill kill;

	/* driver: kills as the driver of a ground vehicle */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		kill = facts_kill(0, 10 + count, count * 3000L);
		kill.category = AE_MEDAL_CATEGORY_VEHICLE;
		kill.killer_seat = AE_MEDAL_SEAT_DRIVER;
		kill.killer_vehicle = AE_MEDAL_VEHICLE_GROUND;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_DRIVER) == (count == 5) && !has(0, AE_MEDAL_PILOT) && !has(0, AE_MEDAL_GUNNER));
	}
	/* pilot: the same in a flying one */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		kill = facts_kill(0, 10 + count, count * 3000L);
		kill.category = count % 2 ? AE_MEDAL_CATEGORY_MOUNTED : AE_MEDAL_CATEGORY_VEHICLE;
		kill.killer_seat = AE_MEDAL_SEAT_DRIVER;
		kill.killer_vehicle = AE_MEDAL_VEHICLE_AIR;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_PILOT) == (count == 5) && !has(0, AE_MEDAL_DRIVER));
	}
	/* gunner: kills from the gunner's seat */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		kill = facts_kill(0, 10 + count, count * 3000L);
		kill.category = AE_MEDAL_CATEGORY_MOUNTED;
		kill.killer_seat = AE_MEDAL_SEAT_GUNNER;
		kill.killer_vehicle = AE_MEDAL_VEHICLE_GROUND;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_GUNNER) == (count == 5) && !has(0, AE_MEDAL_DRIVER));
	}
	/* a gun of the vehicle's whatever its damage is called: a ghost's bolts are plasma */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		kill = facts_kill(0, 10 + count, count * 3000L);
		kill.category = AE_MEDAL_CATEGORY_PLASMA;
		kill.killer_seat = AE_MEDAL_SEAT_DRIVER;
		kill.killer_vehicle = AE_MEDAL_VEHICLE_GROUND;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_DRIVER) == (count == 5));
	}
	/* a driver who strikes or throws a grenade is not driving the kill */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		kill = facts_kill(0, 10 + count, count * 3000L);
		kill.category = count % 2 ? AE_MEDAL_CATEGORY_MELEE : AE_MEDAL_CATEGORY_GRENADE;
		kill.weapon = count % 2 ? AE_MEDAL_WEAPON_OTHER : AE_MEDAL_WEAPON_FRAG_GRENADE;
		kill.killer_seat = count % 3 ? AE_MEDAL_SEAT_DRIVER : AE_MEDAL_SEAT_GUNNER;
		kill.killer_vehicle = AE_MEDAL_VEHICLE_GROUND;
		do_kill(&kill);
	}
	CHECK(!has(0, AE_MEDAL_DRIVER) && !has(0, AE_MEDAL_GUNNER));
}

static void passenger_test(void)
{
	struct ae_medal_kill kill;

	/* driveby: the second of a double kill, from a passenger's seat */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.killer_seat = AE_MEDAL_SEAT_PASSENGER;
	kill.killer_vehicle = AE_MEDAL_VEHICLE_GROUND;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_DRIVEBY));
	kill.victim = 2;
	kill.tick = 130;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_DRIVEBY) && has(0, AE_MEDAL_DOUBLE_KILL));
	kill.victim = 3;
	kill.tick = 160;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_DRIVEBY));
	/* the second kill on foot: no */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.killer_seat = AE_MEDAL_SEAT_PASSENGER;
	do_kill(&kill);
	kill = facts_kill(0, 2, 130);
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_DRIVEBY));
	/* a driver's double kill: no */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.killer_seat = AE_MEDAL_SEAT_DRIVER;
	do_kill(&kill);
	kill.victim = 2;
	kill.tick = 130;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_DRIVEBY));
	/* pineapple express: a grenade kill from a passenger's seat */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.weapon = AE_MEDAL_WEAPON_FRAG_GRENADE;
	kill.killer_seat = AE_MEDAL_SEAT_PASSENGER;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_PINEAPPLE_EXPRESS));
	kill.victim = 2;
	kill.tick = 1000;
	kill.killer_seat = AE_MEDAL_SEAT_DRIVER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_PINEAPPLE_EXPRESS));
	kill.victim = 3;
	kill.tick = 2000;
	kill.killer_seat = AE_MEDAL_SEAT_GUNNER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_PINEAPPLE_EXPRESS));
	kill.victim = 4;
	kill.tick = 3000;
	kill.killer_seat = AE_MEDAL_SEAT_NONE;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_PINEAPPLE_EXPRESS));
	kill.victim = 5;
	kill.tick = 4000;
	kill.weapon = AE_MEDAL_WEAPON_OTHER;
	kill.killer_seat = AE_MEDAL_SEAT_PASSENGER;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_PINEAPPLE_EXPRESS));
}

static void protector_batch_test(void)
{
	struct ae_medal_kill kill;
	int count;

	/* team game: even slots one team, odd the other. Player 0 protects 2 against victims 1, 3, 5 ... */
	new_game(AE_MEDAL_MODE_SLAYER, 1);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		kill = facts_kill(0, 1 + 2 * count, count * 3000L);
		kill.protected_teammate = 1;
		kill.distance = count == 2 ? 10.4f : 10.3f;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_PROTECTOR));
		CHECK(has(0, AE_MEDAL_BODYGUARD) == (count == 5));
		CHECK(has(0, AE_MEDAL_GUARDIAN_ANGEL) == (count == 2));
	}
	/* a protector kill while dead, or in a game with no teams, counts for none of it */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 6; count++)
	{
		kill = facts_kill(0, 10 + count, count * 3000L);
		kill.protected_teammate = 1;
		kill.distance = 30.0f;
		do_kill(&kill);
	}
	CHECK(!has(0, AE_MEDAL_BODYGUARD) && !has(0, AE_MEDAL_GUARDIAN_ANGEL));
	/* wingman: the tenth assist, once */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	for (count = 1; count <= 12; count++)
	{
		kill = facts_kill(1, 10 + count, count * 3000L);
		kill.attacker_count = 2;
		kill.attackers[0].player = 1;
		kill.attackers[0].tick = count * 3000L - 10;
		kill.attackers[0].damage = 0.6f;
		kill.attackers[1].player = 0;
		kill.attackers[1].tick = count * 3000L - 20;
		kill.attackers[1].damage = 0.4f;
		do_kill(&kill);
		CHECK(has(0, AE_MEDAL_ASSIST));
		CHECK(has(0, AE_MEDAL_WINGMAN) == (count == 10));
	}
}

static void record_flags_test(void)
{
	struct ae_medal_kill kill;

	new_game(AE_MEDAL_MODE_SLAYER, 0);
	first_strike_used();
	kill = facts_kill(0, 1, 100);
	kill.no_scope = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_NO_SCOPE) && !has(0, AE_MEDAL_QUICK_DRAW));
	kill = facts_kill(0, 2, 200);
	kill.quick_draw = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_QUICK_DRAW) && !has(0, AE_MEDAL_NO_SCOPE));
	/* a sniper kill out of the scope: NO SCOPE, not SNAPSHOT as well */
	kill = facts_kill(0, 3, 300);
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	kill.killer_snapshot = 1;
	kill.no_scope = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_NO_SCOPE) && !has(0, AE_MEDAL_SNAPSHOT));
	kill = facts_kill(0, 4, 400);
	kill.category = AE_MEDAL_CATEGORY_SNIPER;
	kill.killer_snapshot = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_SNAPSHOT) && !has(0, AE_MEDAL_NO_SCOPE));
	kill = facts_kill(0, 3, 300);
	kill.reversal = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_REVERSAL));
	kill = facts_kill(0, 4, 400);
	kill.nade_shot = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_NADE_SHOT));
	kill = facts_kill(0, 5, 500);
	kill.turned_360 = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_THREE_SIXTY));
	kill = facts_kill(0, 6, 600);
	kill.off_the_rack = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_OFF_THE_RACK));
	/* perfect takes the headshot's place, not the sniper's */
	kill = facts_kill(0, 7, 700);
	kill.headshot = 1;
	kill.perfect = 1;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_PERFECT) && !has(0, AE_MEDAL_HEADSHOT));
	kill = facts_kill(0, 8, 800);
	kill.headshot = 1;
	do_kill(&kill);
	CHECK(!has(0, AE_MEDAL_PERFECT) && has(0, AE_MEDAL_HEADSHOT));
	/* the other medals of a kill with a perfect on it come as they did */
	kill = facts_kill(0, 9, 900);
	kill.headshot = 1;
	kill.perfect = 1;
	kill.weapon = AE_MEDAL_WEAPON_OTHER;
	do_kill(&kill);
	CHECK(has(0, AE_MEDAL_PERFECT) && !has(0, AE_MEDAL_KILL));
	/* none of them for a suicide */
	kill = facts_kill(3, 3, 1000);
	kill.no_scope = 1;
	kill.perfect = 1;
	kill.headshot = 1;
	do_kill(&kill);
	CHECK(out.count == 0);
}

static void hill_guardian_test(void)
{
	long tick;
	int fired;

	new_game(AE_MEDAL_MODE_KING, 0);
	fired = 0;
	for (tick = 1; tick <= 1900; tick++)
	{
		out.count = 0;
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
		if (has(2, AE_MEDAL_HILL_GUARDIAN))
		{
			fired++;
			CHECK(tick == 900 || tick == 1800);
		}
	}
	CHECK(fired == 2);
	/* leaving the hill resets it */
	new_game(AE_MEDAL_MODE_KING, 0);
	for (tick = 1; tick <= 899; tick++)
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
	ae_medal_rules_hill_guardian_tick(&game, 2, 1, 0, 0, &out);
	out.count = 0;
	for (tick = 1; tick <= 899; tick++)
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
	CHECK(!has(2, AE_MEDAL_HILL_GUARDIAN));
	/* dying counts as leaving */
	new_game(AE_MEDAL_MODE_KING, 0);
	for (tick = 1; tick <= 899; tick++)
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
	ae_medal_rules_hill_guardian_tick(&game, 2, 0, 1, 0, &out);
	out.count = 0;
	ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
	CHECK(!has(2, AE_MEDAL_HILL_GUARDIAN));
	/* a contest pauses the time: it neither counts nor resets */
	new_game(AE_MEDAL_MODE_KING, 0);
	for (tick = 1; tick <= 450; tick++)
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
	for (tick = 1; tick <= 1000; tick++)
	{
		out.count = 0;
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 1, &out);
		CHECK(!has(2, AE_MEDAL_HILL_GUARDIAN));
	}
	for (tick = 451; tick <= 899; tick++)
	{
		out.count = 0;
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
		CHECK(!has(2, AE_MEDAL_HILL_GUARDIAN));
	}
	out.count = 0;
	ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
	CHECK(has(2, AE_MEDAL_HILL_GUARDIAN));
	/* only in King; another player's time is their own */
	new_game(AE_MEDAL_MODE_SLAYER, 0);
	for (tick = 1; tick <= 1000; tick++)
	{
		out.count = 0;
		ae_medal_rules_hill_guardian_tick(&game, 2, 1, 1, 0, &out);
		CHECK(!has(2, AE_MEDAL_HILL_GUARDIAN));
	}
}

static void straight_balling_test(void)
{
	long tick;
	int ball_type;

	for (ball_type = 0; ball_type < 3; ball_type++)
	{
		int seen = 0;

		memset(&game, 0, sizeof(game));
		ae_medal_rules_reset(&game, AE_MEDAL_MODE_ODDBALL, 0, 0, ball_type);
		for (tick = 1; tick <= 2000; tick++)
		{
			out.count = 0;
			ae_medal_rules_ball_tick(&game, 3, 1, &out);
			if (has(3, AE_MEDAL_STRAIGHT_BALLING))
			{
				CHECK(tick == 1800);
				seen++;
			}
		}
		CHECK(seen == 1);
	}
	/* a drop in between starts over */
	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	for (tick = 1; tick <= 1799; tick++)
		ae_medal_rules_ball_tick(&game, 3, 1, &out);
	ae_medal_rules_ball_tick(&game, 3, 0, &out);
	out.count = 0;
	for (tick = 1; tick <= 1799; tick++)
	{
		ae_medal_rules_ball_tick(&game, 3, 1, &out);
		CHECK(!has(3, AE_MEDAL_STRAIGHT_BALLING));
	}
	/* one medal for a hold, however long it runs: no repeat at 120 s or after */
	new_game(AE_MEDAL_MODE_ODDBALL, 0);
	{
		int given = 0;

		for (tick = 1; tick <= 3 * AE_MEDAL_STRAIGHT_BALLING_TICKS + 5; tick++)
		{
			out.count = 0;
			ae_medal_rules_ball_tick(&game, 3, 1, &out);
			given += has(3, AE_MEDAL_STRAIGHT_BALLING) ? 1 : 0;
		}
		CHECK(given == 1);
		/* a drop for one tick: the next hold has its own 60 s, and gives its own one medal */
		ae_medal_rules_ball_tick(&game, 3, 0, &out);
		given = 0;
		for (tick = 1; tick <= 2 * AE_MEDAL_STRAIGHT_BALLING_TICKS; tick++)
		{
			out.count = 0;
			ae_medal_rules_ball_tick(&game, 3, 1, &out);
			given += has(3, AE_MEDAL_STRAIGHT_BALLING) ? 1 : 0;
		}
		CHECK(given == 1);
	}
}

int main(void)
{
	weapon_path_test();
	spree_test();
	spree_reset_test();
	multikill_test();
	multikill_window_test();
	weapon_run_test();
	comeback_test();
	revenge_test();
	avenger_test();
	killjoy_test();
	assist_test();
	grave_and_state_test();
	extermination_test();
	weapon_medals_test();
	melee_test();
	order_test();
	order_counters_test();
	first_strike_and_kill_test();
	betrayal_test();
	flag_test();
	ball_test();
	ball_kill_test();
	hill_test();
	race_test();
	end_test();
	state_test();
	role_test();
	yard_sale_test();
	melee_style_test();
	last_shot_test();
	grenade_pair_test();
	quigley_autopilot_test();
	fumble_test();
	flag_kill_test();
	goal_line_test();
	proficiency_test();
	vehicle_proficiency_test();
	passenger_test();
	protector_batch_test();
	record_flags_test();
	hill_guardian_test();
	straight_balling_test();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_medal_rules_test: ok\n");
	return 0;
}
