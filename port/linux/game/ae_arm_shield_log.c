/*
AE_ARM_SHIELD_LOG.C

ae_arm_shield_log.h says what it logs.
*/

#include "cseries.h"
#include "game/game.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "units/units.h"
#include "../src/port_config.h"

#include "ae_arm_shield_log.h"
#include "ae_hit_feedback.h"
#include "ae_shield_effects_rules.h"
#include "network_shield_effects.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
/* (overshield_effect.c) */
boolean overshield_effect_active(long object_index);

#define LOCAL_PLAYERS 4

void ae_arm_shield_log(
	long unit_index,
	short local_player_index)
{
	static unsigned long logged_at[LOCAL_PLAYERS];
	struct unit_datum *unit;
	struct object_definition *definition;
	unsigned long now;
	int charging;
	short shader_type = NONE;
	short effects;

	if (local_player_index < 0 || local_player_index >= LOCAL_PLAYERS)
		return;
	unit = unit_try_and_get(unit_index);
	if (!unit)
		return;
	now = system_milliseconds() | 1;
	charging = TEST_FLAG(unit->object.damage_flags, _object_shield_over_charging_bit) != 0;
	if (!ae_arm_shield_log_due(unit->object.shield_vitality, charging, now, logged_at[local_player_index]))
		return;
	logged_at[local_player_index] = now;
	definition = object_definition_get(unit->definition_index);
	if (definition && definition->object.modifier_shader.index != NONE)
		shader_type = shader_definition_get(definition->object.modifier_shader.index)->base.type;
	effects = network_shield_effects();
	platform_log("arm shield: player %d shield %.2f%s: display.viewmodel_shield %d, display.overshield_effect %d, "
		"shield effects %s (%s), hit feedback %s, overshield effect %s, modifier shader type %d",
		(int)local_player_index, (double)unit->object.shield_vitality, charging ? " (charging)" : "",
		config_boolean("display.viewmodel_shield") != 0, config_boolean("display.overshield_effect") != 0,
		ae_shield_effects_name(effects), effects == NONE ? "this machine's settings" : "the host's",
		ae_hit_feedback_active() ? "on (no overshield effect)" : "off",
		overshield_effect_active(unit_index) ? "on" : "off", (int)shader_type);
}
