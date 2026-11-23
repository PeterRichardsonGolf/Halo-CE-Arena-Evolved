#include <stdio.h>
#include "ae_vehicle_sides_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* game types as Sidewinder's starting locations have them: CTF (1), race (5), "all except CTF and race" (14) */
#define CTF { 1, 0, 0, 0 }
#define RACE { 5, 0, 0, 0 }
#define NORMAL { 14, 0, 0, 0 }
#define ALL { 12, 0, 0, 0 }

/* Sidewinder's own (a sample of its 64): red's and blue's CTF starts at each base, its race starts (team 0) lining
both bases, slayer starts (team 0) between them */
static struct ae_vehicle_sides_start const sidewinder[] =
{
	{ -31.7f, -33.1f, 0.6f, 0, CTF }, { -31.3f, -44.0f, -3.8f, 0, CTF }, { -28.1f, -31.6f, -3.8f, 0, CTF },
	{ -36.0f, -31.5f, -3.8f, 0, CTF }, { -30.9f, -28.5f, -3.8f, 0, CTF }, { -28.4f, -29.4f, -3.8f, 0, CTF },
	{ 31.1f, -47.8f, -3.8f, 1, CTF }, { 31.6f, -36.9f, -3.8f, 1, CTF }, { 32.2f, -32.3f, -3.8f, 1, CTF },
	{ 30.8f, -32.5f, -3.8f, 1, CTF }, { 29.1f, -32.3f, -3.8f, 1, CTF }, { 26.8f, -33.4f, -3.8f, 1, CTF },
	{ 38.9f, -32.0f, -3.8f, 0, RACE }, { 36.7f, -32.1f, -3.8f, 0, RACE }, { 31.7f, -32.1f, -3.8f, 0, RACE },
	{ 29.2f, -32.2f, -3.8f, 0, RACE }, { 26.8f, -32.2f, -3.7f, 0, RACE }, { 24.5f, -32.3f, -3.7f, 0, RACE },
	{ 21.5f, -32.6f, -3.8f, 0, RACE }, { -25.1f, -27.6f, -3.8f, 0, RACE }, { -31.7f, -27.6f, -3.8f, 0, RACE },
	{ -11.8f, -31.0f, -3.8f, 0, NORMAL }, { -5.9f, -12.7f, 0.2f, 0, NORMAL }, { 4.1f, -12.7f, 0.2f, 0, NORMAL },
	{ 52.6f, 25.3f, 0.2f, 0, NORMAL },
};

/* its eight vehicle placements (warthog, ghosts, scorpion at each base) and their bases: 0 red, 1 blue */
static struct { float x, y, z; short team; } const sidewinder_vehicles[] =
{
	{ 30.1f, -30.8f, -3.7f, 1 }, { -31.9f, -26.7f, -3.8f, 0 },
	{ 23.4f, -30.7f, -3.7f, 1 }, { 36.8f, -30.7f, -3.8f, 1 }, { -38.3f, -25.9f, -3.6f, 0 }, { -25.5f, -26.2f, -3.8f, 0 },
	{ -32.0f, -20.6f, -3.8f, 0 }, { 30.5f, -24.4f, -3.4f, 1 },
};

static short pick(struct ae_vehicle_sides_start const *starts, int count, float x, float y, float z, short fallback)
{
	struct ae_vehicle_sides_pick picked;
	int index;

	ae_vehicle_sides_begin(&picked, fallback);
	for (index = 0; index < count; index++)
		ae_vehicle_sides_consider(&picked, &starts[index], x, y, z);
	return picked.team;
}

/* the port's own rule (game_engine.c game_engine_nearest_team): the nearest start of any game type */
static short nearest_any(struct ae_vehicle_sides_start const *starts, int count, float x, float y, float z)
{
	float best = -1.0f;
	short team = 0;
	int index;

	for (index = 0; index < count; index++)
	{
		float dx = starts[index].x - x, dy = starts[index].y - y, dz = starts[index].z - z;
		float distance = dx * dx + dy * dy + dz * dz;

		if ((starts[index].team == 0 || starts[index].team == 1) && (best < 0.0f || distance < best))
		{
			best = distance;
			team = starts[index].team;
		}
	}
	return team;
}

int main(void)
{
	int index, blue_before = 0, blue_after = 0;

	/* Sidewinder: every placement goes to its own base's team (four each), where the nearest start of any game type
	gave all eight to red (the bug: CUSTOM counts placed red's over the map, none of blue's) */
	for (index = 0; index < COUNT(sidewinder_vehicles); index++)
	{
		float x = sidewinder_vehicles[index].x, y = sidewinder_vehicles[index].y, z = sidewinder_vehicles[index].z;
		short team = pick(sidewinder, COUNT(sidewinder), x, y, z, nearest_any(sidewinder, COUNT(sidewinder), x, y, z));

		CHECK(team == sidewinder_vehicles[index].team);
		blue_before += nearest_any(sidewinder, COUNT(sidewinder), x, y, z);
		blue_after += team;
	}
	CHECK(blue_before == 0);
	CHECK(blue_after == 4);

	/* a start's team counts only when a CTF game uses it: CTF or ALL, red or blue */
	{
		struct ae_vehicle_sides_start start = { 0.0f, 0.0f, 0.0f, 1, CTF };

		CHECK(ae_vehicle_sides_start_has_team(&start));
		start.game_types[0] = 12;
		CHECK(ae_vehicle_sides_start_has_team(&start));
		start.game_types[0] = 2;
		CHECK(!ae_vehicle_sides_start_has_team(&start));
		start.game_types[3] = 1;
		CHECK(ae_vehicle_sides_start_has_team(&start));
		start.team = 2;
		CHECK(!ae_vehicle_sides_start_has_team(&start));
		start.team = -1;
		CHECK(!ae_vehicle_sides_start_has_team(&start));
	}

	/* a map with no CTF start keeps the side given (the port's own answer); none at all, the same */
	{
		static struct ae_vehicle_sides_start const no_ctf[] =
		{
			{ 10.0f, 0.0f, 0.0f, 0, NORMAL }, { -10.0f, 0.0f, 0.0f, 1, RACE },
		};

		CHECK(pick(no_ctf, COUNT(no_ctf), 9.0f, 0.0f, 0.0f, 1) == 1);
		CHECK(pick(no_ctf, COUNT(no_ctf), 9.0f, 0.0f, 0.0f, 0) == 0);
		CHECK(pick(no_ctf, 0, 9.0f, 0.0f, 0.0f, 1) == 1);
	}

	/* ALL starts carry their team; the nearest wins, height too */
	{
		static struct ae_vehicle_sides_start const all[] =
		{
			{ 0.0f, 0.0f, 10.0f, 0, ALL }, { 0.0f, 0.0f, -3.0f, 1, ALL }, { 1.0f, 0.0f, 0.0f, 0, RACE },
		};

		CHECK(pick(all, COUNT(all), 0.0f, 0.0f, 0.0f, 0) == 1);
		CHECK(pick(all, COUNT(all), 0.0f, 0.0f, 8.0f, 1) == 0);
	}

	if (failures)
		return 1;
	printf("ae_vehicle_sides_rules_test: all passed\n");
	return 0;
}
