/*
AE_BOTS_COMBAT.C

The engine's side of the bots' fight by level (ae_bots_combat.h): the two skill settings, what a bot reads of a
weapon, a grenade or its target, its own state, and the minute line. The choices are ae_bots_combat_rules.c's.
*/

#include "cseries.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "tag_files/tag_groups.h"
#include "items/weapon_definitions.h"

#include "bots.h"
#include "ae_burst_rifle.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"
#include "ae_bots_combat.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* port/linux/src/port_config.c */
const char *config_string(char const *name);
unsigned long config_changes(void);
long config_integer(char const *name);
void platform_log(char const *format, ...);

/* ---------- the settings */

static int level_named(char const *name)
{
	static char const *const names[AE_BOTS_LEVELS] = { "recruit", "easy", "normal", "heroic", "legendary", "demon" };
	int level;

	for (level = 0; name && level < AE_BOTS_LEVELS; level++)
	{
		if (!strcmp(name, names[level]))
			return level;
	}
	/* ("same", and anything else) */
	return -1;
}

long ae_bots_trace_level(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static long level;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		level = MAX(config_integer("debug.bot_trace"), 0);
	}
	return level;
}

int ae_bots_combat_setting_level(boolean movement)
{
	static unsigned long read_at = (unsigned long)-1;
	static int shooting = -1, moving = -1;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		shooting = level_named(config_string("bots.shooting_skill"));
		moving = level_named(config_string("bots.movement_skill"));
	}
	return movement ? moving : shooting;
}

/* ---------- weapons, grenades, targets */

/* (the last weapon asked about: its tag index is another weapon's on the next map, so ae_bots_combat_reset forgets it) */
static long precision_definition = NONE;
static boolean precision_answer;

boolean ae_bots_combat_precision_weapon(long definition_index)
{
	char const *name;

	if (definition_index == NONE)
		return FALSE;
	if (definition_index == precision_definition)
		return precision_answer;
	name = tag_get_name(definition_index);
	precision_definition = definition_index;
	/* (and the carbine pack's, a head-shot rifle as the BR: ae_weapon_slots.h) */
	precision_answer = ae_burst_rifle_definition(definition_index) ||
		ae_weapon_pack_definition_slot(definition_index) == AE_WEAPON_SLOT_CARBINE ||
		(name && ((strstr(name, "pistol") && !strstr(name, "plasma")) || strstr(name, "sniper")));
	return precision_answer;
}

boolean ae_bots_combat_scoped_rifle(long definition_index)
{
	short slot;

	if (definition_index == NONE)
		return FALSE;
	/* (the packs' two precision rifles; the sniper rifle keeps the DESCOPE REFLEX) */
	slot = ae_weapon_pack_definition_slot(definition_index);
	return (slot == AE_WEAPON_SLOT_BATTLE_RIFLE || slot == AE_WEAPON_SLOT_CARBINE) &&
		weapon_definition_get(definition_index)->weapon.zoom_level_count > 0;
}

real ae_bots_combat_grenade_speed(long unit_index)
{
	struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	real speed;

	if (!object)
		return 0.6f;
	speed = unit_definition_get(object->definition_index)->unit.grenade_velocity / (real)TICKS_PER_SECOND;
	return speed > 0.05f ? speed : 0.6f;
}

boolean ae_bots_combat_back_turned(long unit_index, real_point3d const *point)
{
	struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	real dx, dy, length, forward_length;

	if (!object)
		return FALSE;
	dx = point->x - object->object.position.x;
	dy = point->y - object->object.position.y;
	length = (real)sqrt(dx * dx + dy * dy);
	forward_length = (real)sqrt(object->object.forward.i * object->object.forward.i +
		object->object.forward.j * object->object.forward.j);
	if (length < 0.01f || forward_length < 0.01f)
		return FALSE;
	/* (cos 114.6 degrees: 'facing away') */
	return (dx * object->object.forward.i + dy * object->object.forward.j) / (length * forward_length) < -0.416f;
}

/* ---------- a bot's state */

static struct ae_bots_combat_bot combat_bots[BOTS_BRAINS];

void ae_bots_combat_reset(void)
{
	csmemset(combat_bots, 0, sizeof(combat_bots));
	precision_definition = NONE;
}

struct ae_bots_combat_bot *ae_bots_combat_bot_state(long bot_index, long unit_index)
{
	struct ae_bots_combat_bot *bot;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		bot_index = 0;
	bot = &combat_bots[bot_index];
	if (!bot->known || bot->unit_index != unit_index)
	{
		csmemset(bot, 0, sizeof(*bot));
		bot->known = TRUE;
		bot->unit_index = unit_index;
		ae_bots_combat_retreat_reset(&bot->retreat);
		bot->share_tick = -TICKS_PER_SECOND * 10;
		bot->last_fight_tick = -TICKS_PER_SECOND * 10;
		bot->melee_ready_tick = NONE;
		bot->prenade_seen_tick = NONE;
		bot->pace_since = NONE;
	}
	return bot;
}

/* ---------- halo.log */

static struct
{
	long counts[AE_BOTS_COMBAT_COUNTERS];
	long last_report_tick;
	boolean started;
} combat_log;

void ae_bots_combat_count(int counter)
{
	if (counter >= 0 && counter < AE_BOTS_COMBAT_COUNTERS)
		combat_log.counts[counter]++;
}

void ae_bots_combat_report(long now, char const *shooting, char const *movement)
{
	/* (a new game's ticks start over: so do the counts) */
	if (!combat_log.started || now < combat_log.last_report_tick)
	{
		csmemset(&combat_log, 0, sizeof(combat_log));
		combat_log.started = TRUE;
		combat_log.last_report_tick = now;
		return;
	}
	if (now - combat_log.last_report_tick < TICKS_PER_SECOND * 60)
		return;
	combat_log.last_report_tick = now;
	platform_log("bots: combat tick %ld (shooting %s, movement %s): grenades %ld, blows %ld (%ld held back), "
		"head aims %ld, retreats %ld, charges %ld, jumps %ld, halts %ld, sightings shared %ld, shots paced %ld, "
		"zoomed again %ld, grenades at last seen %ld, high ground picks %ld, looks out of vertical view %ld, heads seen past a ledge %ld, "
		"holds of fire for a teammate %ld, grenades held for a teammate %ld", now, shooting,
		movement,
		combat_log.counts[AE_BOTS_COMBAT_GRENADES], combat_log.counts[AE_BOTS_COMBAT_MELEES],
		combat_log.counts[AE_BOTS_COMBAT_MELEES_DECLINED], combat_log.counts[AE_BOTS_COMBAT_HEAD_AIMS],
		combat_log.counts[AE_BOTS_COMBAT_RETREATS], combat_log.counts[AE_BOTS_COMBAT_CHARGES],
		combat_log.counts[AE_BOTS_COMBAT_JUMPS], combat_log.counts[AE_BOTS_COMBAT_HALTS],
		combat_log.counts[AE_BOTS_COMBAT_SHARES], combat_log.counts[AE_BOTS_COMBAT_PACED],
		combat_log.counts[AE_BOTS_COMBAT_RESCOPES], combat_log.counts[AE_BOTS_COMBAT_PRENADES],
		combat_log.counts[AE_BOTS_COMBAT_HIGH_GROUND], combat_log.counts[AE_BOTS_COMBAT_OUT_OF_VERTICAL_VIEW],
		combat_log.counts[AE_BOTS_COMBAT_HEADS_SEEN], combat_log.counts[AE_BOTS_COMBAT_FRIENDLY_FIRE_HELD],
		combat_log.counts[AE_BOTS_COMBAT_FRIENDLY_GRENADES_HELD]);
}

void ae_bots_combat_event(long bot_index, long now, char const *skill_name, char const *event, long target,
	real distance, char const *extra)
{
	platform_log("bot event: t=%ld bot=%ld skill=%s ev=%s target=%ld dist=%.1f%s%s", now, bot_index + 1, skill_name,
		event, target, distance, extra && extra[0] ? " " : "", extra ? extra : "");
}
