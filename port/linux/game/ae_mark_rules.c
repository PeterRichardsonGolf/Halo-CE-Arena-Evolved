/*
AE_MARK_RULES.C

Pure rules for MARKS (ae_mark_rules.h). No engine includes.
*/

#include "ae_mark_rules.h"
#include "ae_wheel_rules.h"

#include <math.h>

#define AE_MARK_SAME_PLACE_SQUARED 0.25f	/* half a world unit */
#define AE_MARK_SAME_ENEMY_SQUARED 4.0f	/* an enemy marked and still within two world units */

int ae_mark_press_step(struct ae_mark_press *press, int down, int taken, long now, int *light)
{
	int mark = 0;

	if (down && !press->down)
	{
		press->since = now;
		press->taken = taken;
		press->held = 0;
	}
	else if (!down && press->down)
		mark = !press->taken && !press->held;
	press->down = down;
	if (down && !press->taken && now - press->since >= AE_MARK_HOLD_TICKS)
		press->held = 1;
	*light = !down || press->taken || press->held;
	return mark;
}

float ae_mark_alpha(long age)
{
	if (age < 0 || age >= AE_MARK_LIFE_TICKS)
		return 0.0f;
	if (age < AE_MARK_APPEAR_TICKS)
		return (float)(age + 1) / (float)AE_MARK_APPEAR_TICKS;
	if (age < AE_MARK_FADE_AT_TICKS)
		return 1.0f;
	return (float)(AE_MARK_LIFE_TICKS - age) / (float)(AE_MARK_LIFE_TICKS - AE_MARK_FADE_AT_TICKS);
}

float ae_mark_appear_scale(long age)
{
	float left;

	if (age < 0 || age >= AE_MARK_APPEAR_TICKS)
		return 1.0f;
	left = 1.0f - (float)(age + 1) / (float)AE_MARK_APPEAR_TICKS;
	return 1.0f + 0.35f * left * left;
}

int ae_mark_clamp(float *x, float *y, float radius_x, float radius_y, int off_screen)
{
	float across = *x / radius_x;
	float along = *y / radius_y;
	float reach = across * across + along * along;

	if (!off_screen && reach < 1.0f)
		return 0;
	if (reach <= 0.0f)
	{
		/* (straight behind: below the view) */
		*x = 0.0f;
		*y = radius_y;
		return 1;
	}
	reach = 1.0f / sqrtf(reach);
	*x *= reach;
	*y *= reach;
	return 1;
}

int ae_mark_same(int kind_a, long object_a, int kind_b, long object_b, float distance_squared)
{
	if (kind_a != kind_b)
		return 0;
	if (kind_a == AE_MARK_KIND_PLACE)
		return distance_squared < AE_MARK_SAME_PLACE_SQUARED;
	return object_a == object_b;
}

int ae_mark_host_allows(int kind, float distance_squared, int line_clear)
{
	if (kind < 0 || kind >= AE_MARK_KIND_COUNT || !(distance_squared <= AE_MARK_HOST_REACH * AE_MARK_HOST_REACH))
		return 0;
	return kind != AE_MARK_KIND_ENEMY || line_clear;
}

int ae_mark_host_line_wanted(int kind, float distance_squared)
{
	return kind == AE_MARK_KIND_ENEMY && distance_squared >= 0.0f && distance_squared <= AE_MARK_HOST_REACH * AE_MARK_HOST_REACH;
}

int ae_mark_spot_finite(float x, float y, float z)
{
	return __builtin_isfinite(x) && __builtin_isfinite(y) && __builtin_isfinite(z);
}

int ae_mark_spam_take(struct ae_mark_spam *spam, long now)
{
	int index, inside = 1;

	if (spam->blocked)
	{
		if (now < spam->blocked_until)
			return AE_MARK_SPAM_STILL_BLOCKED;
		spam->blocked = 0;
		spam->used = 0;
		spam->next = 0;
	}
	for (index = 0; index < spam->used; index++)
		inside += now - spam->times[index] < AE_MARK_SPAM_WINDOW_TICKS;
	if (inside >= AE_MARK_SPAM_LIMIT)
	{
		spam->blocked = 1;
		spam->blocked_until = now + AE_MARK_SPAM_BLOCK_TICKS;
		return AE_MARK_SPAM_NOW_BLOCKED;
	}
	spam->times[spam->next] = now;
	spam->next = (spam->next + 1) % AE_MARK_SPAM_LIMIT;
	if (spam->used < AE_MARK_SPAM_LIMIT)
		spam->used++;
	return inside == AE_MARK_SPAM_WARN_AT ? AE_MARK_SPAM_WARN : AE_MARK_SPAM_OK;
}

int ae_mark_reply_allowed(int verb, int mark_active, int same_team, int mark_kind, int has_object, int already_said)
{
	if (!mark_active || !same_team)
		return 0;
	switch (verb)
	{
	case AE_CALL_OK:
		return !already_said;
	case AE_CALL_NEVER_MIND:
		return already_said != 0;
	case AE_CALL_DIBS:
		return !already_said && mark_kind == AE_MARK_KIND_ITEM && has_object;
	case AE_CALL_CANCEL:
		return 1;
	}
	return 0;
}

long ae_mark_life_after_ok(long life_ticks, long born, long now)
{
	long life = life_ticks + AE_MARK_OK_TICKS;

	if (now - born >= life_ticks)
		return life_ticks;
	return life > AE_MARK_LIFE_MOST_TICKS ? AE_MARK_LIFE_MOST_TICKS : life;
}

int ae_mark_old_build_kind(int entry)
{
	switch (entry)
	{
	case AE_WHEEL_ENEMY:
		return AE_MARK_KIND_ENEMY;
	case AE_WHEEL_ATTACK:
	case AE_WHEEL_DEFEND:
	case AE_WHEEL_GOING:
	case AE_WHEEL_WATCH:
	case AE_WHEEL_WEAPON:
	case AE_WHEEL_RIDE:
		return AE_MARK_KIND_PLACE;
	}
	return -1;
}

long ae_mark_effective_age(long age, long life_ticks)
{
	long shift = life_ticks - AE_MARK_LIFE_TICKS;

	if (age < 0 || shift <= 0 || age < AE_MARK_FADE_AT_TICKS)
		return age;
	/* (after the fade's start it holds full till the life's own fade begins) */
	return age < AE_MARK_FADE_AT_TICKS + shift ? AE_MARK_FADE_AT_TICKS - 1 : age - shift;
}

int ae_mark_repeats_after(int same, int previous)
{
	if (!same)
		return 0;
	return previous >= 2 ? 2 : previous + 1;
}

int ae_mark_double_free(long counted, long now)
{
	long since = now - (counted - 1);

	return counted > 0 && since >= 0 && since <= AE_MARK_DOUBLE_FREE_TICKS;
}

int ae_mark_candidate_better(int tier_a, float part_a, int tier_b, float part_b)
{
	if (tier_a <= 0)
		return 0;
	if (tier_b <= 0)
		return 1;
	/* (a body lies where its player dropped the weapon: against an objective, a vehicle or a pickup it counts only when nearer) */
	if (tier_a != tier_b)
	{
		if ((tier_a == AE_TIER_BODY && (tier_b == AE_TIER_NAMED || tier_b == AE_TIER_PICKUP)) ||
			(tier_b == AE_TIER_BODY && (tier_a == AE_TIER_NAMED || tier_a == AE_TIER_PICKUP)))
		{
			return part_a < part_b;
		}
		return tier_a < tier_b;
	}
	return part_a < part_b;
}

int ae_mark_press_verb(int own_marker, int said_ok, int item_mark, int dibs_taken)
{
	if (own_marker)
		return AE_CALL_CANCEL;
	if (said_ok)
		return AE_CALL_NEVER_MIND;
	return item_mark && !dibs_taken ? AE_CALL_DIBS : AE_CALL_OK;
}

int ae_mark_press_is_own(int kind_a, long object_a, int kind_b, long object_b, float distance_squared)
{
	if (!ae_mark_same(kind_a, object_a, kind_b, object_b, distance_squared))
		return 0;
	return kind_a != AE_MARK_KIND_ENEMY || distance_squared < AE_MARK_SAME_ENEMY_SQUARED;
}

int ae_marks_walls_of(int ball_melee_byte)
{
	return (ball_melee_byte & AE_MARKS_WALLS_BIT) != 0;
}

int ae_marks_walls_set(int ball_melee_byte, int on)
{
	return on ? ball_melee_byte | AE_MARKS_WALLS_BIT : ball_melee_byte & ~AE_MARKS_WALLS_BIT;
}

int ae_mark_rule_flags(int marks_on)
{
	return marks_on ? 0 : 1;
}

void ae_mark_rule_read(int pad_byte, int *marks_on)
{
	*marks_on = !(pad_byte & 1);
}

int ae_mark_sight_due(long tested_at, long now)
{
	return tested_at == -1 || now < tested_at || now - tested_at >= AE_MARK_SIGHT_TEST_TICKS;
}

int ae_mark_sight_held(long seen_at, long now)
{
	return seen_at != -1 && now >= seen_at && now - seen_at < AE_MARK_SIGHT_HOLD_TICKS;
}

int ae_mark_place_item_id(int item_class, int side)
{
	return AE_MARK_PLACE_ITEM_FIRST + item_class * 3 + side;
}

int ae_mark_place_name(int id, wchar_t *name, int size)
{
	static char const *const items[AE_MARK_PLACE_ITEM_CLASSES] = { "ROCKETS", "SNIPER", "SHOTGUN", "OS", "CAMO" };
	static char const *const sides[3] = { "", "RED ", "BLUE " };
	char const *parts[2] = { "", "" };
	int length = 0;
	int part;

	name[0] = 0;
	if (id == AE_MARK_PLACE_RED_BASE)
	{
		parts[0] = "RED BASE";
	}
	else if (id == AE_MARK_PLACE_BLUE_BASE)
	{
		parts[0] = "BLUE BASE";
	}
	else if (id >= AE_MARK_PLACE_ITEM_FIRST && id < AE_MARK_PLACE_ITEM_FIRST + AE_MARK_PLACE_ITEM_CLASSES * 3)
	{
		parts[0] = sides[(id - AE_MARK_PLACE_ITEM_FIRST) % 3];
		parts[1] = items[(id - AE_MARK_PLACE_ITEM_FIRST) / 3];
	}
	else
	{
		return 0;
	}
	for (part = 0; part < 2; part++)
	{
		char const *letter;

		for (letter = parts[part]; *letter && length < size - 1; letter++)
			name[length++] = (wchar_t)*letter;
	}
	name[length] = 0;
	return 1;
}

int ae_mark_place_pick(struct ae_mark_place_point const *points, int count, float x, float y, float z)
{
	float best = 0.0f;
	int best_id = AE_MARK_PLACE_NONE;
	int best_base = 0;
	int index;

	for (index = 0; index < count; index++)
	{
		float dx = points[index].x - x;
		float dy = points[index].y - y;
		float dz = points[index].z - z;
		float distance = dx * dx + dy * dy + dz * dz;
		int base = points[index].id < AE_MARK_PLACE_ITEM_FIRST;
		float range = base ? AE_MARK_PLACE_BASE_REACH : AE_MARK_PLACE_ITEM_REACH;

		if (distance > range * range || (best_id != AE_MARK_PLACE_NONE && (best_base > base ||
			(best_base == base && distance >= best))))
		{
			continue;
		}
		best = distance;
		best_id = points[index].id;
		best_base = base;
	}
	return best_id;
}

int ae_mark_serial_next(int serial)
{
	return (serial + 1) & AE_MARK_SERIAL_MASK;
}

int ae_mark_reply_is_for(int verb, int reply_serial, int mark_serial, long ends, long now)
{
	if (verb == AE_CALL_CANCEL)
		return 1;
	return reply_serial == mark_serial && now < ends;
}

int ae_mark_wire_counts(int repeats, int serial)
{
	return (repeats > 2 ? 2 : repeats) | ((serial & AE_MARK_SERIAL_MASK) << 2);
}

int ae_mark_wire_repeats(int counts)
{
	int repeats = counts & 3;

	return repeats > 2 ? 2 : repeats;
}

int ae_mark_wire_serial(int counts)
{
	return (counts >> 2) & AE_MARK_SERIAL_MASK;
}
