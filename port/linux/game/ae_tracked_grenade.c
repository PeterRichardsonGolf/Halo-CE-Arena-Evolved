/*
AE_TRACKED_GRENADE.C

Hold-to-delay, release-to-detonate for a pack weapon whose trigger tracks its projectile (ae_tracked_grenade.h).
*/

#include <string.h>

#include "cseries.h"
#include "real_math.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "game/game.h"
#include "items/projectile_definitions.h"
#include "items/projectiles.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "units/units.h"

#include "ae_weapon_pack.h"
#include "ae_tracked_grenade.h"
#include "ae_tracked_grenade_rules.h"

/* the grenades tracked now: a few at a time (a grenade is forgotten once its weapon tracks another, or it is gone) */
#define TRACKED 32
/* projectiles.c's _projectile_collided_once_bit (its flags are its own file's): for the log line */
#define COLLIDED_ONCE_BIT 2
/* the first releases on a map are logged, then no more */
#define LOGGED 16

static struct tracked
{
	long projectile_index;
	long weapon_index;
	long weapon_definition_index;
	long projectile_definition_index;
	long fired_tick;
} tracked[TRACKED];
static int next_entry;
static int logged;
static long logged_map_tick = -1;

static boolean ready_done = FALSE;

/* every entry empty (NONE, not 0, which may be an object's index), the log's count back to 0 */
static void clear(void)
{
	int index;

	for (index = 0; index < TRACKED; index++)
	{
		tracked[index].projectile_index = NONE;
		tracked[index].weapon_index = NONE;
		tracked[index].weapon_definition_index = NONE;
		tracked[index].projectile_definition_index = NONE;
		tracked[index].fired_tick = 0;
	}
	next_entry = 0;
	logged = 0;
	logged_map_tick = -1;
	ready_done = TRUE;
}

static void ready(void)
{
	if (!ready_done)
		clear();
}

void ae_tracked_grenade_new_map(void)
{
	clear();
}

static struct tracked *find(long projectile_index)
{
	int index;

	ready();
	if (projectile_index == NONE)
		return NULL;
	for (index = 0; index < TRACKED; index++)
	{
		if (tracked[index].projectile_index == projectile_index)
			return &tracked[index];
	}
	return NULL;
}

/* a log line, the first few of a map (a new map is a clock that went back) */
static boolean log_this(void)
{
	long now = game_time_get();

	if (now < logged_map_tick)
		logged = 0;
	logged_map_tick = now;
	return (boolean)(logged++ < LOGGED);
}

void ae_tracked_grenade_fired(long weapon_index, short trigger_index, long projectile_in_shot, long projectile_object_index)
{
	struct weapon_datum *weapon = weapon_try_and_get(weapon_index);
	struct weapon_definition *definition;
	struct weapon_trigger_definition *trigger;
	struct tracked *entry;
	struct projectile_datum *projectile = (struct projectile_datum *)object_try_and_get_and_verify_type(
		projectile_object_index, _object_mask_projectile);

	ready();
	if (!weapon || !projectile)
		return;
	definition = weapon_definition_get(weapon->definition_index);
	if (trigger_index < 0 || trigger_index >= definition->weapon.triggers.count)
		return;
	trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, trigger_index, struct weapon_trigger_definition);
	if (!ae_tracked_grenade_tracks(ae_weapon_pack_tag_is_own(weapon->definition_index),
			TEST_FLAG(trigger->flags, _weapon_trigger_tracks_projectile_bit) ? 1 : 0, projectile_in_shot))
	{
		return;
	}
	/* (the weapon's last grenade is forgotten: a weapon tracks one) */
	for (entry = tracked; entry < tracked + TRACKED; entry++)
	{
		if (entry->weapon_index == weapon_index)
			entry->projectile_index = NONE;
	}
	entry = &tracked[next_entry];
	next_entry = (next_entry + 1) % TRACKED;
	entry->projectile_index = projectile_object_index;
	entry->weapon_index = weapon_index;
	entry->weapon_definition_index = weapon->definition_index;
	entry->projectile_definition_index = projectile->definition_index;
	entry->fired_tick = game_time_get();
	weapon->weapon.tracked_object_index = projectile_object_index;
}

/* whether the weapon is its unit's current weapon (every handle tried, none asserted) */
static int in_hand(long weapon_index, struct weapon_datum *weapon)
{
	struct unit_datum *unit = unit_try_and_get(weapon->object.parent_object_index);
	short current;

	if (!unit)
		return 0;
	current = unit->unit.current_weapon_index;
	return current >= 0 && current < MAXIMUM_WEAPONS_PER_UNIT && unit->unit.weapon_object_indices[current] == weapon_index;
}

/* (every projectile, tracked or not: see ae_tracked_grenade_owner) the owner handle of a projectile whose shooter is gone
is cleared; its owner player (the kill credit) is kept */
static void forget_gone_owner(long projectile_index)
{
	struct projectile_datum *projectile = (struct projectile_datum *)object_try_and_get_and_verify_type(
		projectile_index, _object_mask_projectile);

	if (projectile)
	{
		projectile->object.owner_object_index = ae_tracked_grenade_owner(projectile->object.owner_object_index,
			object_try_and_get_and_verify_type(projectile->object.owner_object_index, _object_mask_all) != NULL);
	}
}

/* a pack's own grenade that asks for its fuse after the first bounce has it from the first bounce */
static int fuse_from_first_bounce(long projectile_index, int action)
{
	struct projectile_datum *projectile = (struct projectile_datum *)object_try_and_get_and_verify_type(
		projectile_index, _object_mask_projectile);

	if (!projectile)
		return action;
	return ae_tracked_grenade_fuse_runs(action, ae_weapon_pack_tag_is_own(projectile->definition_index),
		projectile_definition_get(projectile->definition_index)->projectile.detonation_timer_starts,
		TEST_FLAG(projectile->projectile.flags, COLLIDED_ONCE_BIT) ? 1 : 0) ? AE_TRACKED_GRENADE_FUSE : action;
}

static int tracked_action(long projectile_index);

int ae_tracked_grenade_tick(long projectile_index)
{
	forget_gone_owner(projectile_index);
	return fuse_from_first_bounce(projectile_index, tracked_action(projectile_index));
}

static int tracked_action(long projectile_index)
{
	struct tracked *entry;

	entry = find(projectile_index);
	struct weapon_datum *weapon;
	struct projectile_datum *projectile;
	long held;
	int action;

	if (!entry)
		return AE_TRACKED_GRENADE_RUN;
	/* (the row must still be this grenade and this weapon: the same salted handles, of the same tags, as at the shot.
	The weapon's own tracked_object_index is not asked: the game's trigger clears it once the weapon's busy time after
	the shot reads as the trigger let go, a tick or two in) */
	projectile = (struct projectile_datum *)object_try_and_get_and_verify_type(projectile_index, _object_mask_projectile);
	if (!ae_tracked_grenade_same_projectile(projectile != NULL, projectile ? projectile->definition_index : NONE,
			entry->projectile_definition_index))
	{
		entry->projectile_index = NONE;
		return AE_TRACKED_GRENADE_RUN;
	}
	weapon = weapon_try_and_get(entry->weapon_index);
	if (!ae_tracked_grenade_same_weapon(weapon != NULL, weapon ? weapon->definition_index : NONE,
			entry->weapon_definition_index))
	{
		weapon = NULL;
	}
	held = game_time_get() - entry->fired_tick;
	action = ae_tracked_grenade_tick_action(weapon != NULL, ae_tracked_grenade_trigger_held(weapon != NULL,
		weapon ? in_hand(entry->weapon_index, weapon) : 0,
		weapon && TEST_FLAG(weapon->weapon.control_flags, _weapon_control_primary_trigger_bit) ? 1 : 0), held);
	if (action == AE_TRACKED_GRENADE_HOLD)
		return action;
	entry->projectile_index = NONE;
	if (log_this())
	{
		long owner = projectile->object.owner_player_index;

		if (action == AE_TRACKED_GRENADE_DETONATE)
		{
			error(_error_silent, "ae tracked grenade: player %ld let go after %ld ticks: set off at %.2f %.2f %.2f, %s, "
				"%.1f wu/s", owner == NONE ? -1L : DATUM_INDEX_TO_ABSOLUTE_INDEX(owner), held,
				(double)projectile->object.position.x, (double)projectile->object.position.y,
				(double)projectile->object.position.z,
				TEST_FLAG(projectile->projectile.flags, COLLIDED_ONCE_BIT) ? "after a bounce" : "in the air, no bounce yet",
				(double)(magnitude3d(&projectile->object.translational_velocity) * TICKS_PER_SECOND));
		}
		else
		{
			error(_error_silent, "ae tracked grenade: player %ld %s after %ld ticks: left to its fuse",
				owner == NONE ? -1L : DATUM_INDEX_TO_ABSOLUTE_INDEX(owner),
				held > AE_TRACKED_GRENADE_MAX_HOLD_TICKS ? "held past the cap" : "let go (a tap)", held);
		}
	}
	return action;
}
