/*
AE_GAMETYPE_AMMO.C

INFINITE AMMO and BOTTOMLESS CLIP as the gametype's rules (ae_gametype_ammo.h;
the bits: ae_weapon_rules.h). Every machine reads the running game's variant,
the host's, so a network client's weapons play as the host's.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "items/equipment_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "tag_files/tag_files.h"

#include "ae_bomb_assault.h"
#include "ae_gametype_ammo.h"
#include "ae_weapon_rules.h"

int config_boolean(const char *name);

enum
{
	/* debug.weapon_log's shots a game */
	AE_AMMO_LOG_SHOTS = 400,
};

static struct
{
	long shots_logged;
	long last_tick;
	long grenades_logged;
} ae_ammo_log = { 0, -1, 0 };

static unsigned char ae_gametype_ammo_byte(void)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;

	return variant ? (unsigned char)variant->universal_variant.pack_loadout : 0;
}

boolean ae_gametype_infinite_ammo(void)
{
	return ae_weapon_infinite_ammo(ae_gametype_ammo_byte()) ? TRUE : FALSE;
}

boolean ae_gametype_bottomless_clip(void)
{
	return ae_weapon_bottomless_clip(ae_gametype_ammo_byte()) ? TRUE : FALSE;
}

boolean ae_gametype_skips_map_item(long definition_index)
{
	return ae_gametype_map_item_skipped(definition_index, TRUE);
}

boolean ae_gametype_map_item_skipped(long definition_index, boolean logged)
{
	unsigned char byte = ae_gametype_ammo_byte();
	boolean equipment;
	boolean grenade = FALSE;
	boolean skip;

	if (definition_index == NONE)
		return FALSE;
	/* (SEARCH AND DESTROY: nothing on the map but the bomb; the weapons of the dead stay) */
	if (ae_bomb_snd_on())
		return TRUE;
	equipment = object_definition_get(definition_index)->object.type == _object_type_equipment;
	if (equipment)
		grenade = equipment_definition_get(definition_index)->equipment.powerup_type == _equipment_powerup_grenade;
	skip = ae_map_item_skipped(byte, equipment, grenade) ? TRUE : FALSE;
	/* (debug.weapon_log: each grenade pickup the map's spawn reaches, the first few dozen a game) */
	if (logged && grenade && ae_ammo_log.grenades_logged < AE_AMMO_LOG_SHOTS && config_boolean("debug.weapon_log"))
	{
		char const *name = tag_get_name(definition_index);

		ae_ammo_log.grenades_logged++;
		error(_error_silent, "ae map grenades: tick %ld, %s pickup %s", game_time_get(),
			name ? tag_name_strip_path(name) : "?", skip ? "skipped" : "spawns");
	}
	return skip;
}

void ae_gametype_ammo_log_rules(unsigned char pack_loadout)
{
	error(_error_silent, "AE ammo: infinite ammo %s, bottomless clip %s",
		ae_weapon_infinite_ammo(pack_loadout) ? "on" : "off", ae_weapon_bottomless_clip(pack_loadout) ? "on" : "off");
	error(_error_silent, "AE map grenades: %s", ae_map_grenades_off(pack_loadout) ? "off" : "on");
}

void ae_gametype_ammo_fired(long weapon_index, short magazine_index)
{
	unsigned char byte = ae_gametype_ammo_byte();
	struct weapon_datum *weapon;
	struct weapon_definition *definition;
	struct weapon_magazine const *magazine;
	long tick;
	char const *name;

	if (!ae_weapon_infinite_ammo(byte) && !ae_weapon_bottomless_clip(byte))
		return;
	tick = game_time_get();
	/* (a new game: its time starts again) */
	if (tick < ae_ammo_log.last_tick)
		ae_ammo_log.shots_logged = 0;
	ae_ammo_log.last_tick = tick;
	if (ae_ammo_log.shots_logged >= AE_AMMO_LOG_SHOTS || !config_boolean("debug.weapon_log"))
		return;
	ae_ammo_log.shots_logged++;
	weapon = weapon_get(weapon_index);
	definition = weapon_definition_get(weapon->definition_index);
	magazine = magazine_index >= 0 && magazine_index < definition->weapon.magazines.count &&
		magazine_index < (short)NUMBEROF(weapon->weapon.magazines) ? &weapon->weapon.magazines[magazine_index] : NULL;
	name = tag_get_name(weapon->definition_index);
	error(_error_silent, "ae ammo: tick %ld, %s fired, %d loaded, %d in reserve", tick,
		name ? tag_name_strip_path(name) : "?", magazine ? (int)magazine->rounds_loaded : -1,
		magazine ? (int)magazine->rounds_total : -1);
}
