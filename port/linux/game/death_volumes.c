/*
DEATH_VOLUMES.C

(port) The trigger volumes a map kills in, for those that must keep out of
them: the bots (bots.c: their searches' walkable surfaces in one closed,
items and roaming places in one left be, a step into one not taken),
Firefight's ordnance drops (firefight_spawner.c: no drop in one, nor on the
ground under one) and the AI's straight-line fallback (actor_moving.c).

A volume is deadly
- when its name says so ("kill", "death", "dead"), or
- learnt: an AI unit died of falling damage inside it (ai_falls.c's
  trails: a volume whose script hurts as a fall does).

Worked out once a level, when first asked (the scenario's scripts are
loaded by then), and forgotten at the level's end (ai_falls_level_end).
debug.txt has the level's deadly volumes ("death volumes:").
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

#include "death_volumes.h"

#include <string.h>

#define DEATH_VOLUMES_MAXIMUM 256
#define DEATH_VOLUMES_LONGS (DEATH_VOLUMES_MAXIMUM / 32)

static struct
{
	boolean known;
	struct scenario *scenario;
	short volume_count;
	short deadly_count;
	unsigned long deadly[DEATH_VOLUMES_LONGS];
	short deadly_indices[DEATH_VOLUMES_MAXIMUM];
} death_volumes;

static boolean death_volumes_name_deadly(char const *name)
{
	static char const *const words[] = { "kill", "death", "dead" };
	char lower[TAG_STRING_LENGTH + 1];
	short index;

	for (index = 0; index < TAG_STRING_LENGTH && name[index]; index++)
		lower[index] = (char)(name[index] >= 'A' && name[index] <= 'Z' ? name[index] - 'A' + 'a' : name[index]);
	lower[index] = 0;
	for (index = 0; index < (short)NUMBEROF(words); index++)
	{
		if (strstr(lower, words[index]))
			return TRUE;
	}
	return FALSE;
}

static void death_volumes_index(void)
{
	short index;

	death_volumes.deadly_count = 0;
	for (index = 0; index < death_volumes.volume_count; index++)
	{
		if (BIT_VECTOR_TEST_FLAG(death_volumes.deadly, index))
			death_volumes.deadly_indices[death_volumes.deadly_count++] = index;
	}
}

static boolean death_volumes_ready(void)
{
	struct scenario *scenario = global_scenario_try_and_get();
	unsigned long scripted[DEATH_VOLUMES_LONGS];
	char names[512];
	int used = 0;
	short index, scripted_count;

	if (!scenario)
		return FALSE;
	if (death_volumes.known && death_volumes.scenario == scenario)
		return death_volumes.deadly_count > 0;
	csmemset(&death_volumes, 0, sizeof(death_volumes));
	death_volumes.known = TRUE;
	death_volumes.scenario = scenario;
	death_volumes.volume_count = (short)MIN(scenario->trigger_volumes.count, DEATH_VOLUMES_MAXIMUM);
	if (death_volumes.volume_count <= 0)
		return FALSE;
	/* (the scripts' kill volumes are not read in this build: by name, or learnt) */
	csmemset(scripted, 0, sizeof(scripted));
	scripted_count = 0;
	names[0] = 0;
	for (index = 0; index < death_volumes.volume_count; index++)
	{
		struct scenario_trigger_volume const *volume =
			TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, index, struct scenario_trigger_volume);
		boolean by_script = BIT_VECTOR_TEST_FLAG(scripted, index);
		boolean by_name = death_volumes_name_deadly(volume->name);

		if (!by_script && !by_name)
			continue;
		BIT_VECTOR_SET_FLAG(death_volumes.deadly, index, TRUE);
		if (used < (int)sizeof(names) - 48)
			used += snprintf(names + used, sizeof(names) - (size_t)used, "%s\"%s\" (%s)", used ? ", " : "", volume->name,
				by_script && by_name ? "script, name" : by_script ? "script" : "name");
	}
	death_volumes_index();
	error(_error_silent, "death volumes: %d of %d trigger volumes kill (%d by the scripts)%s%s", (int)death_volumes.deadly_count,
		(int)death_volumes.volume_count, (int)scripted_count, death_volumes.deadly_count ? ": " : "", names);
	return death_volumes.deadly_count > 0;
}

short death_volume_at(real_point3d const *point)
{
	short index;

	if (!death_volumes_ready())
		return NONE;
	for (index = 0; index < death_volumes.deadly_count; index++)
	{
		if (scenario_trigger_volume_test_point(death_volumes.deadly_indices[index], point))
			return death_volumes.deadly_indices[index];
	}
	return NONE;
}

boolean death_volume_column(real_point3d const *point, real height)
{
	real_point3d sample = *point;
	real risen;

	if (!death_volumes_ready())
		return FALSE;
	for (risen = 0.0f; risen <= height; risen += 0.5f)
	{
		sample.z = point->z + risen;
		if (death_volume_at(&sample) != NONE)
			return TRUE;
	}
	return FALSE;
}

boolean death_volume_is_deadly(short volume_index)
{
	return death_volumes_ready() && volume_index >= 0 && volume_index < death_volumes.volume_count &&
		BIT_VECTOR_TEST_FLAG(death_volumes.deadly, volume_index);
}

boolean death_volumes_any(void)
{
	return death_volumes_ready();
}

void death_volume_learn(short volume_index, char const *why)
{
	struct scenario *scenario = global_scenario_try_and_get();

	death_volumes_ready();
	if (!scenario || volume_index < 0 || volume_index >= death_volumes.volume_count ||
		BIT_VECTOR_TEST_FLAG(death_volumes.deadly, volume_index))
	{
		return;
	}
	BIT_VECTOR_SET_FLAG(death_volumes.deadly, volume_index, TRUE);
	death_volumes_index();
	error(_error_silent, "death volumes: \"%s\" learnt to kill (%s)",
		TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, volume_index, struct scenario_trigger_volume)->name, why);
}

void death_volumes_level_end(void)
{
	csmemset(&death_volumes, 0, sizeof(death_volumes));
}
