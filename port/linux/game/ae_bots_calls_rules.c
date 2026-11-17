/*
AE_BOTS_CALLS_RULES.C

How often a bot may make a wheel call, as pure rules (ae_bots_calls_rules.h): no engine includes.
*/

#include <string.h>

#include "ae_bots_calls_rules.h"
#include "ae_wheel_rules.h"

int ae_bots_call_allowed(struct ae_bots_call_team const *team, long bot_last_tick, int low_level, int team_has_human, int alive,
	int on_foot_or_driver, int is_driver, int entry, float x, float y, float z, long now)
{
	long gap = AE_BOTS_CALL_BOT_GAP_TICKS * (low_level ? 2 : 1);
	int i;

	if (!team_has_human || !alive || !on_foot_or_driver)
		return 0;
	if (entry < 0 || entry >= AE_WHEEL_COUNT)
		return 0;
	if (entry == AE_WHEEL_RIDE && !is_driver)
		return 0;
	if (bot_last_tick != 0 && now - bot_last_tick < gap)
		return 0;
	if (team->last_tick != 0 && now - team->last_tick < AE_BOTS_CALL_TEAM_GAP_TICKS)
		return 0;
	for (i = 0; i < AE_BOTS_CALL_RECENT; i++)
	{
		struct ae_bots_call_recent const *recent = &team->recent[i];
		float dx = recent->x - x, dy = recent->y - y, dz = recent->z - z;

		if (recent->tick != 0 && recent->entry == entry && now - recent->tick < AE_BOTS_CALL_REPEAT_TICKS &&
			dx * dx + dy * dy + dz * dz < AE_BOTS_CALL_REPEAT_REACH * AE_BOTS_CALL_REPEAT_REACH)
		{
			return 0;
		}
	}
	return 1;
}

void ae_bots_call_made(struct ae_bots_call_team *team, int entry, float x, float y, float z, long now)
{
	struct ae_bots_call_recent *recent = &team->recent[team->next];

	recent->entry = entry;
	recent->x = x;
	recent->y = y;
	recent->z = z;
	recent->tick = now;
	team->next = (team->next + 1) % AE_BOTS_CALL_RECENT;
	team->last_tick = now;
}

void ae_bots_call_team_reset(struct ae_bots_call_team *team)
{
	memset(team, 0, sizeof(*team));
}
