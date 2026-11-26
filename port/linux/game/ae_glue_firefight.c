/*
AE_GLUE_FIREFIGHT.C

See ae_glue_firefight.h. Each call is a Firefight engine call (firefight.c, firefight_spawner.c: Content Evolved's), kept here so the
menus' screens include no game header.
*/

#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "ae_glue_firefight.h"
#include "ae_firefight_menus_rules.h"
#include "halo_firefight.h"

/* the rules' content byte is the engine's own: the rules file keeps the bits in step */
typedef char verify_content_mode[AE_FFM_MODE_INCLUDE_VALUE == _firefight_map_content_include_map &&
	AE_FFM_MODE_ONLY_VALUE == _firefight_map_content_only_map && AE_FFM_BIT_MODE == FIREFIGHT_MAP_CONTENT_MODE_MASK ? 1 : -1];
typedef char verify_content_kinds[FIREFIGHT_MAP_CONTENT_HAS_ENEMIES == 1 && FIREFIGHT_MAP_CONTENT_HAS_WEAPONS == 2 &&
	FIREFIGHT_MAP_CONTENT_HAS_VEHICLES == 4 ? 1 : -1];
typedef char verify_content_bits[FIREFIGHT_MAP_CONTENT_ENEMIES_CUSTOM == 0x04 && FIREFIGHT_MAP_CONTENT_WEAPONS_CUSTOM == 0x08 &&
	FIREFIGHT_MAP_CONTENT_VEHICLES_CUSTOM == 0x80 && FIREFIGHT_MAP_CONTENT_ENEMIES_OFF == 0x10 &&
	FIREFIGHT_MAP_CONTENT_WEAPONS_OFF == 0x20 && FIREFIGHT_MAP_CONTENT_VEHICLES_OFF == 0x40 ? 1 : -1];
typedef char verify_health_rules[AE_FFM_HEALTH_RULES == NUMBER_OF_FIREFIGHT_HEALTH_RULES ? 1 : -1];
typedef char verify_kinds[AE_FFM_CONTENT_KINDS == NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS ? 1 : -1];

/* (DEV MODE's staged scenes: a map of no file, "dev:firefight", with three enemies, six weapons, two vehicles and a player of its own
rules; its choices are kept in memory only) */
static struct
{
	char on[AE_FFG_DEV_KINDS][AE_FFG_DEV_TAGS];
	char loadout[2][AE_FFG_DEV_NAME];
	unsigned char health[3];
} dev_state;
static const char *const dev_tags[AE_FFG_DEV_KINDS][AE_FFG_DEV_TAGS] = {
	{ "characters\\dev\\elite major", "characters\\dev\\grunt heavy", "characters\\dev\\jackal sniper" },
	{ "weapons\\dev\\sniper rifle", "weapons\\dev\\rocket launcher", "weapons\\dev\\shotgun", "weapons\\dev\\plasma rifle",
		"weapons\\dev\\needler", "weapons\\dev\\fuel rod" },
	{ "vehicles\\dev\\ghost", "vehicles\\dev\\wraith" } };

static int dev_map(const char *map)
{
	static int started;

	if (!map || strcmp(map, AE_FFG_DEV_MAP))
		return 0;
	if (!started)
	{
		started = 1;
		memset(&dev_state, 0, sizeof(dev_state));
		memset(dev_state.on[0], 1, 3);
		memset(dev_state.on[1], 1, 6);
		dev_state.on[1][5] = 0;
		dev_state.on[2][0] = 1;
	}
	return 1;
}

static int dev_count(int kind)
{
	return kind == 0 ? 3 : kind == 1 ? 6 : kind == 2 ? 2 : 0;
}

unsigned char ae_ffg_map_kinds(const char *map)
{
	return dev_map(map) ? 7 : map && map[0] ? (unsigned char)firefight_map_content_kinds(map) : 0;
}

int ae_ffg_tag_count(const char *map, int kind)
{
	if (dev_map(map))
		return dev_count(kind);
	return map && map[0] ? firefight_map_content_tag_count(map, (short)kind) : 0;
}

const char *ae_ffg_tag_name(const char *map, int kind, int index)
{
	if (dev_map(map))
		return index >= 0 && index < dev_count(kind) ? dev_tags[kind][index] : NULL;
	return map && map[0] ? firefight_map_content_tag_name(map, (short)kind, (short)index) : NULL;
}

int ae_ffg_tag_on(const char *map, int kind, const char *tag_name)
{
	if (dev_map(map))
	{
		int index;

		for (index = 0; tag_name && index < dev_count(kind); index++)
			if (!strcmp(tag_name, dev_tags[kind][index]))
				return dev_state.on[kind][index];
		return 0;
	}
	return map && map[0] && tag_name && firefight_map_content_tag_on(map, (short)kind, tag_name) != 0;
}

void ae_ffg_tag_set(const char *map, int kind, const char *tag_name, int on)
{
	if (dev_map(map))
	{
		int index;

		for (index = 0; tag_name && index < dev_count(kind); index++)
			if (!strcmp(tag_name, dev_tags[kind][index]))
				dev_state.on[kind][index] = on != 0;
		return;
	}
	if (map && map[0] && tag_name)
		firefight_map_content_tag_set(map, (short)kind, tag_name, on != 0);
}

const char *ae_ffg_loadout_get(const char *map, int slot)
{
	if (dev_map(map))
		return slot >= 0 && slot < 2 && dev_state.loadout[slot][0] ? dev_state.loadout[slot] : NULL;
	return map && map[0] ? firefight_map_loadout_get(map, (short)slot) : NULL;
}

void ae_ffg_loadout_set(const char *map, int slot, const char *tag_name)
{
	if (dev_map(map))
	{
		if (slot >= 0 && slot < 2)
		{
			strncpy(dev_state.loadout[slot], tag_name ? tag_name : "", AE_FFG_DEV_NAME - 1);
			dev_state.loadout[slot][AE_FFG_DEV_NAME - 1] = 0;
		}
		return;
	}
	if (map && map[0])
		firefight_map_loadout_set(map, (short)slot, tag_name);
}

unsigned char ae_ffg_health_detect(const char *map)
{
	if (dev_map(map))
		return 1;
	return map && map[0] ? (unsigned char)firefight_map_health_detect(map) : 0;
}

void ae_ffg_health_get(const char *map, unsigned char rules[3])
{
	byte kept[NUMBER_OF_FIREFIGHT_HEALTH_RULES] = { 0, 0, 0 };
	int rule;

	if (dev_map(map))
	{
		for (rule = 0; rule < 3; rule++)
			rules[rule] = dev_state.health[rule];
		return;
	}
	if (map && map[0])
		firefight_map_health_rules_get(map, kept);
	for (rule = 0; rule < 3; rule++)
		rules[rule] = kept[rule];
}

void ae_ffg_health_set(const char *map, unsigned char const rules[3])
{
	byte kept[NUMBER_OF_FIREFIGHT_HEALTH_RULES];
	int rule;

	if (dev_map(map))
	{
		for (rule = 0; rule < 3; rule++)
			dev_state.health[rule] = rules[rule];
		return;
	}
	for (rule = 0; rule < 3; rule++)
		kept[rule] = rules[rule];
	if (map && map[0])
		firefight_map_health_rules_set(map, kept);
}

int ae_ffg_vehicles_shown(const char *map, unsigned char content)
{
	boolean stock = FALSE, own = FALSE;

	if (dev_map(map))
		return 1;
	if (!map || !map[0])
		return 0;
	firefight_map_has_vehicles(map, &stock, &own);
	return stock || (own && (ae_ffg_map_kinds(map) & FIREFIGHT_MAP_CONTENT_HAS_VEHICLES) &&
		(content & FIREFIGHT_MAP_CONTENT_MODE_MASK) != _firefight_map_content_vanilla &&
		!(content & FIREFIGHT_MAP_CONTENT_VEHICLES_OFF));
}

int ae_ffg_content_saved(const char *map, unsigned char *content)
{
	byte saved = 0;

	if (dev_map(map))
		return 0;
	if (!map || !map[0] || !firefight_map_content_saved(map, &saved))
		return 0;
	*content = saved;
	return 1;
}

void ae_ffg_content_save(const char *map, unsigned char content)
{
	if (map && map[0] && !dev_map(map) && ae_ffg_map_kinds(map))
		firefight_map_content_save(map, content);
}

int ae_ffg_scale_get(void)
{
	return firefight_server_player_scale_percent_get();
}

void ae_ffg_scale_set(int percent)
{
	firefight_server_player_scale_percent_set((short)percent);
}
