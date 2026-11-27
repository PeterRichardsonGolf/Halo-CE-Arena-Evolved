/*
FORGE_MAP_SETTINGS.C

A Forge map's general settings (forge_map_settings.h): its display name,
sky, the players' bipeds and first-person hands, and its atmosphere.

Each tag is kept by its path, and found in the level's tag table when it is
wanted (forge_map_settings_tag): the map's own, the profile's in a Forge
session, the map's bake once saved (forge_baked_archive_make bakes them
with the placed objects' tags). A level of another map has none of them:
the settings are read anew as each level loads.
*/

#include "cseries.h"
#include "cseries/errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/biped_definitions.h"
#include "units/unit_control_data.h"
#include "items/weapons.h"
#include "render/render_cameras.h"
#include "rasterizer/rasterizer.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

#include "forge_engine.h"
void ae_forge_level_forget(void); /* AE hook */
#include "forge_map_settings.h"

/* ---------- externs */

extern void *cache_files_tag_instances(long *count);
extern boolean firefight_active(void);
extern long tag_injector_level_profile(void);
extern long tag_injector_level_baked_profile(void);
extern long port_tag_profiles_count(void);
extern long port_tag_profiles_tag_count(long profile);
extern unsigned long port_tag_profiles_tag_group(long profile, long tag);
extern const char *port_tag_profiles_tag_name(long profile, long tag);
extern const char *port_tag_profiles_tag_map(long profile, long tag);
extern long port_tag_profiles_tag_find(long profile, unsigned long group, const char *name);
extern void forge_network_send_text(long machine_index, char const *text);
extern void unit_network_add_weapon(long unit_index, long weapon_index, short slot);
extern void unit_network_forget_weapon(long unit_index, short slot);
extern boolean unit_can_hold_weapon_definition(long unit_index, long weapon_definition_index);
extern boolean unit_has_weapon_definition_index(long unit_index, long definition_index);

/* ---------- constants */

#define SKY_GROUP_TAG 'sky '
#define BIPED_GROUP_TAG 'bipd'
#define GBXMODEL_GROUP_TAG 'mod2'
#define MODEL_GROUP_TAG 'mode'
#define MAXIMUM_SKY_REFERENCES 16

/* ---------- structures */

struct settings_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name; /* AE hook */
	XPTR(void) base_address; /* AE hook */
	unsigned long unused[2];
};

struct atmosphere_preset
{
	char const *name;
	float color[3];
	float density;
	float start;
	float opaque;
	int indoors;
};

/* ---------- globals */

static struct atmosphere_preset const atmosphere_presets[NUMBER_OF_FORGE_ATMOSPHERES] =
{
	{ "MAP DEFAULT", { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 0 },
	{ "CLEAR", { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 1 },
	{ "LIGHT FOG", { 0.70f, 0.74f, 0.80f }, 0.45f, 10.0f, 160.0f, 0 },
	{ "HEAVY FOG", { 0.60f, 0.62f, 0.66f }, 0.95f, 2.0f, 50.0f, 1 },
	/* (Floodfection's: infection.c) */
	{ "FLOOD FOG", { 0.16f, 0.20f, 0.09f }, 1.0f, 3.0f, 55.0f, 1 },
	{ "CUSTOM", { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 0 },
};

static struct forge_map_settings settings;

/* the sky references of the scenario last changed, as the map had them, so
MAP DEFAULT gives them back; the level they are of (level_serial) */
static struct
{
	struct scenario *scenario;
	long serial;
	short count;
	long indices[MAXIMUM_SKY_REFERENCES];
	unsigned long groups[MAXIMUM_SKY_REFERENCES];
	/* (the sky it was set to, NONE: the map's) */
	long applied;
} sky_state = { NULL, -1, 0, { 0 }, { 0 }, NONE };

/* the test spawn (forge_map_settings_test_start): the slot the local player
spawns as (NONE: no test), what it waits for, the unit killed, and the copy
of the player standing before it that moves as in a mirror, the mirror's
plane (a point on it and its normal, level) */
enum
{
	_test_none = 0,
	_test_kill,
	_test_respawn,
	_test_mirroring
};

static struct
{
	int slot;
	int phase;
	long killed_unit;
	long mirror_unit;
	long mirrored_unit;
	real_point3d plane_point;
	real_vector3d plane_normal;
} test = { NONE, _test_none, NONE, NONE, NONE };

/* the far clip the game had before RENDER DISTANCE changed it (0: not
changed) */
static float render_distance_original;

/* a level's loading, each one counted; and the settings' changes */
static long level_serial;
static long settings_generation;

/* the tags the settings name, found in the level's table (each call of the
engine's hooks would look through all of it): for the level, the settings
and the table's size they were found for */
static struct
{
	long serial;
	long generation;
	long tag_count;
	long tags[NUMBER_OF_FORGE_MAP_KINDS][NUMBER_OF_FORGE_MAP_SLOTS];
} resolved = { -1, -1, -1 };

/* ---------- private code */

static void settings_clear(void)
{
	memset(&settings, 0, sizeof(settings));
	settings.atmosphere = _forge_atmosphere_map_default;
	settings.fog_color[0] = settings.fog_color[1] = settings.fog_color[2] = 0.7f;
	settings.fog_density = 0.5f;
	settings.fog_start = 5.0f;
	settings.fog_opaque = 120.0f;
	settings.sky_visibility = _forge_sky_auto;
}

/* the players' bipeds, hands and movements a level's profile plays (a
campaign profile's player, a multiplayer profile's teams:
forge_map_settings_players_set), under a Forge map's own of each slot */
static struct
{
	char biped[NUMBER_OF_FORGE_MAP_SLOTS][FORGE_MAP_SETTINGS_PATH_MAX];
	char hands[NUMBER_OF_FORGE_MAP_SLOTS][FORGE_MAP_SETTINGS_PATH_MAX];
	int movement[NUMBER_OF_FORGE_MAP_SLOTS];
	int link[NUMBER_OF_FORGE_MAP_SLOTS];
} players_layer;

/* the slot whose biped, hands and movement a slot plays, by the links of a
layer (a slot linked to another plays that one's, which may be linked on;
links that come back round play the slot's own) */
static int link_follow(int const *links, int slot)
{
	int at = slot;
	int step;

	for (step = 0; step < NUMBER_OF_FORGE_MAP_SLOTS; step++)
	{
		int link = links[at];

		if (link <= _forge_link_custom || link > NUMBER_OF_FORGE_MAP_SLOTS || link - 1 == at)
			return at;
		if (link - 1 == slot)
			return slot;
		at = link - 1;
	}
	return slot;
}

/* whether a movement setting moves the biped as a Spartan (the default) */
static int movement_spartan(int movement)
{
	return movement != _forge_movement_own;
}

/* a slot's movement: the Forge map's when it has a biped of its own for the
slot, else the profile's */
static int movement_of(int slot)
{
	int forge_slot = link_follow(settings.link, slot);

	if (settings.biped[forge_slot][0])
		return movement_spartan(settings.movement[forge_slot]) ? _forge_movement_spartan : _forge_movement_own;
	return movement_spartan(players_layer.movement[link_follow(players_layer.link, slot)]) ? _forge_movement_spartan :
		_forge_movement_own;
}

/* a model's group in the level: a gbxmodel the Tag Injector or Forge added
is made this build's model (tag_injector.c's bundle_models_convert), as the
map's own are */
static int group_matches(unsigned long wanted, unsigned long group)
{
	return group == wanted || (wanted == GBXMODEL_GROUP_TAG && group == MODEL_GROUP_TAG);
}

static char *settings_path(int kind, int slot)
{
	if (kind == _forge_map_kind_sky)
		return settings.sky;
	if (slot < 0 || slot >= NUMBER_OF_FORGE_MAP_SLOTS)
		return NULL;
	return kind == _forge_map_kind_biped ? settings.biped[slot] : kind == _forge_map_kind_hands ? settings.hands[slot] :
		NULL;
}

static unsigned long kind_group(int kind)
{
	return kind == _forge_map_kind_sky ? SKY_GROUP_TAG : kind == _forge_map_kind_biped ? BIPED_GROUP_TAG :
		GBXMODEL_GROUP_TAG;
}

/* a name's characters as a map list shows them: printable, no '|' (the
trailer's separator); its length */
static int display_name_clean(char *name)
{
	int read, written = 0;

	for (read = 0; name[read] && written < FORGE_MAP_DISPLAY_NAME_MAX; read++)
	{
		unsigned char character = (unsigned char)name[read];

		if (character >= ' ' && character < 0x7F && character != '|' && !(character == ' ' && !written))
			name[written++] = (char)character;
	}
	while (written > 0 && name[written - 1] == ' ')
		written--;
	name[written] = 0;
	return written;
}

static struct settings_tag_instance const *instance_of(long tag_index)
{
	long count = 0;
	struct settings_tag_instance const *instances =
		(struct settings_tag_instance const *)cache_files_tag_instances(&count);
	long absolute = tag_index == NONE ? NONE : DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index);

	if (!instances || absolute < 0 || absolute >= count || instances[absolute].tag_index != tag_index ||
		!instances[absolute].base_address)
	{
		return NULL;
	}
	return &instances[absolute];
}

static long tag_find(unsigned long group, char const *path)
{
	long count = 0;
	struct settings_tag_instance const *instances;
	long index;

	if (!path || !path[0])
		return NONE;
	instances = (struct settings_tag_instance const *)cache_files_tag_instances(&count);
	for (index = 0; instances && index < count; index++)
	{
		if (group_matches(group, (unsigned long)instances[index].group_tag) && instances[index].tag_index != NONE &&
			instances[index].name && instances[index].base_address && !_stricmp(XBOX_POINTER(char const, instances[index].name), path)) /* AE hook */
		{
			return instances[index].tag_index;
		}
	}
	return NONE;
}

/* a profile's hands in the level: the Tag Injector names a map's hands by
their path and the map's ("...\\fp#bigass"), and a level adds a hands tag
the profile names by its path alone, that the level has, so too
(tag_injector.c's level_hands_alias) */
static long profile_hands_level_tag(long profile, long tag)
{
	char const *name = port_tag_profiles_tag_name(profile, tag);
	char const *map = port_tag_profiles_tag_map(profile, tag);
	char alias[FORGE_MAP_SETTINGS_PATH_MAX + 64];
	long found;

	if (!name || !name[0])
		return NONE;
	if (strchr(name, '#') || !map || !map[0])
		return tag_find(GBXMODEL_GROUP_TAG, name);
	snprintf(alias, sizeof(alias), "%s#%s", name, map);
	found = tag_find(GBXMODEL_GROUP_TAG, alias);
	return found != NONE ? found : tag_find(GBXMODEL_GROUP_TAG, name);
}

long forge_map_settings_profile_tag(long profile, uint32_t group, char const *name)
{
	long found = profile == NONE ? NONE : port_tag_profiles_tag_find(profile, group, name);
	char const *hash = name ? strchr(name, '#') : NULL;
	long count;
	long tag;

	if (found != NONE || !hash || profile == NONE)
		return found;
	/* (a hands tag of a profile of before: its path alone, and the map) */
	count = port_tag_profiles_tag_count(profile);
	for (tag = 0; tag < count; tag++)
	{
		char const *entry = port_tag_profiles_tag_name(profile, tag);
		char const *map = port_tag_profiles_tag_map(profile, tag);

		if (port_tag_profiles_tag_group(profile, tag) == group && entry && map &&
			strlen(entry) == (size_t)(hash - name) && !_strnicmp(entry, name, (size_t)(hash - name)) &&
			!_stricmp(map, hash + 1))
		{
			return tag;
		}
	}
	return NONE;
}

static struct game_globals_first_person_interface *first_person_interface(void)
{
	struct game_globals *globals = scenario_get_game_globals();

	if (!globals || globals->first_person_interface.count <= 0)
		return NULL;
	return TAG_BLOCK_GET_ELEMENT(&globals->first_person_interface, 0, struct game_globals_first_person_interface);
}

/* ---- a biped's movement as a Spartan's (forge_map_movement) */

#define MAXIMUM_MOVEMENT_PATCHES NUMBER_OF_FORGE_MAP_SLOTS

/* the biped tags whose movement is the Spartan's now, each as it was (the
level they are of, and the settings they were made for) */
static struct
{
	long serial;
	long generation;
	short count;
	long tags[MAXIMUM_MOVEMENT_PATCHES];
	struct _biped_definition originals[MAXIMUM_MOVEMENT_PATCHES];
} movement_state = { -1, -1, 0 };

/* the map's own player biped: its multiplayer unit, else its player's */
static long spartan_biped(void)
{
	struct game_globals *globals = scenario_get_game_globals();
	long unit = NONE;

	if (!globals)
		return NONE;
	if (globals->multiplayer_information.count > 0)
		unit = TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0,
			struct game_globals_multiplayer_information)->unit.index;
	if (unit == NONE && globals->player_information.count > 0)
		unit = TAG_BLOCK_GET_ELEMENT(&globals->player_information, 0,
			struct game_globals_player_information)->player_unit.index;
	return unit != NONE && instance_of(unit) && (unsigned long)instance_of(unit)->group_tag == BIPED_GROUP_TAG ?
		unit : NONE;
}

/* a biped's movement made the Spartan's: how it moves (player physics,
turning, flying none), its slopes, jump and landings; its size, cameras,
crouching and the rest its own */
static void movement_copy(struct _biped_definition *biped, struct _biped_definition const *spartan)
{
	unsigned long const moving_flags = FLAG(_biped_turns_without_animating_bit) | FLAG(_biped_uses_player_physics_bit) |
		FLAG(_biped_flying_bit) | FLAG(_biped_rotate_while_airborne_bit) | FLAG(_biped_uses_old_player_physics_bit);

	biped->flags = (biped->flags & ~moving_flags) | (spartan->flags & moving_flags);
	biped->moving_turning_speed = spartan->moving_turning_speed;
	biped->stationary_turning_threshold = spartan->stationary_turning_threshold;
	biped->runtime_cosine_stationary_turning_threshold = spartan->runtime_cosine_stationary_turning_threshold;
	biped->maximum_slope_angle = spartan->maximum_slope_angle;
	biped->downhill_falloff_angle = spartan->downhill_falloff_angle;
	biped->downhill_cutoff_angle = spartan->downhill_cutoff_angle;
	biped->downhill_velocity_scale = spartan->downhill_velocity_scale;
	biped->uphill_falloff_angle = spartan->uphill_falloff_angle;
	biped->uphill_cutoff_angle = spartan->uphill_cutoff_angle;
	biped->uphill_velocity_scale = spartan->uphill_velocity_scale;
	biped->runtime_minimum_normal_k = spartan->runtime_minimum_normal_k;
	biped->runtime_downhill_k0 = spartan->runtime_downhill_k0;
	biped->runtime_downhill_k1 = spartan->runtime_downhill_k1;
	biped->runtime_uphill_k0 = spartan->runtime_uphill_k0;
	biped->runtime_uphill_k1 = spartan->runtime_uphill_k1;
	biped->jump_velocity = spartan->jump_velocity;
	biped->maximum_soft_landing_time = spartan->maximum_soft_landing_time;
	biped->maximum_hard_landing_time = spartan->maximum_hard_landing_time;
	biped->minimum_soft_landing_velocity = spartan->minimum_soft_landing_velocity;
	biped->minimum_hard_landing_velocity = spartan->minimum_hard_landing_velocity;
	biped->maximum_hard_landing_velocity = spartan->maximum_hard_landing_velocity;
	biped->death_hard_landing_velocity = spartan->death_hard_landing_velocity;
}

/* the bipeds' movements as the settings want them: each patched tag given
back, then each slot's SPARTAN biped patched (a tag two slots share moves
as a Spartan if either wants it). A level's own tags are new: none of the
last level's is given back */
static void movement_apply(void)
{
	long spartan;
	short index;
	int slot;

	if (movement_state.serial == level_serial && movement_state.generation == settings_generation)
		return;
	if (movement_state.serial == level_serial)
	{
		for (index = 0; index < movement_state.count; index++)
		{
			struct settings_tag_instance const *instance = instance_of(movement_state.tags[index]);

			if (instance)
				XBOX_POINTER(struct biped_definition, instance->base_address)->biped = movement_state.originals[index]; /* AE hook */
		}
	}
	movement_state.count = 0;
	movement_state.serial = level_serial;
	movement_state.generation = settings_generation;
	spartan = spartan_biped();
	for (slot = 0; spartan != NONE && slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		long biped = forge_map_settings_tag(_forge_map_kind_biped, slot);
		struct biped_definition *definition;

		if (movement_of(slot) != _forge_movement_spartan || biped == NONE || biped == spartan ||
			!instance_of(biped))
		{
			continue;
		}
		for (index = 0; index < movement_state.count && movement_state.tags[index] != biped; index++)
		{
		}
		if (index < movement_state.count || movement_state.count >= MAXIMUM_MOVEMENT_PATCHES)
			continue;
		definition = XBOX_POINTER(struct biped_definition, instance_of(biped)->base_address); /* AE hook */
		movement_state.tags[movement_state.count] = biped;
		movement_state.originals[movement_state.count] = definition->biped;
		movement_state.count++;
		movement_copy(&definition->biped, &XBOX_POINTER(struct biped_definition, instance_of(spartan)->base_address)->biped); /* AE hook */
		error(_error_silent, "forge map settings: %s moves as %s", tag_get_name(biped), tag_get_name(spartan));
	}
}

/* the scenario's sky references, the chosen sky's in place of each (the
map's own given back for none). The sky a cluster has is drawn and fogs by
its reference (scenario_get_sky), so a level's clusters with a sky all get
this one; those with none (indoors) stay so */
static void sky_apply(void)
{
	struct scenario *scenario = global_scenario_try_and_get();
	long wanted = forge_map_settings_tag(_forge_map_kind_sky, 0);
	short index;

	if (!scenario)
		return;
	if (sky_state.scenario != scenario || sky_state.serial != level_serial)
	{
		sky_state.scenario = scenario;
		sky_state.serial = level_serial;
		sky_state.count = (short)MIN(scenario->sky_references.count, MAXIMUM_SKY_REFERENCES);
		sky_state.applied = NONE;
		for (index = 0; index < sky_state.count; index++)
		{
			struct tag_reference *reference =
				TAG_BLOCK_GET_ELEMENT(&scenario->sky_references, index, struct tag_reference);

			sky_state.indices[index] = reference->index;
			sky_state.groups[index] = reference->group_tag;
		}
	}
	if (wanted == sky_state.applied)
		return;
	for (index = 0; index < sky_state.count; index++)
	{
		struct tag_reference *reference = TAG_BLOCK_GET_ELEMENT(&scenario->sky_references, index, struct tag_reference);

		reference->index = wanted != NONE ? wanted : sky_state.indices[index];
		reference->group_tag = wanted != NONE ? SKY_GROUP_TAG : sky_state.groups[index];
	}
	sky_state.applied = wanted;
	if (wanted != NONE)
		error(_error_silent, "forge map settings: sky %s over %d of the map's", settings.sky, (int)sky_state.count);
}

/* the player's slot: its team's in a team game, else free for all's (a
local player's test spawn's, during it) */
static int player_slot(long player_index)
{
	struct player_datum *player;

	if (test.slot != NONE && player_index != NONE && player_index == local_player_get_player_index(0))
		return test.slot;
	if (player_index == NONE || !game_engine_has_teams())
		return _forge_map_slot_ffa;
	player = player_get(player_index);
	if (!player)
		return _forge_map_slot_ffa;
	return player->team_index == 1 ? _forge_map_slot_blue : _forge_map_slot_red;
}

/* the host's word to its clients (-1: all of them) of a setting */
static void net_send_name(long machine_index)
{
	char text[64];

	snprintf(text, sizeof(text), "!FGN:%s", settings.display_name);
	forge_network_send_text(machine_index, text);
}

static void net_send_tag(long machine_index, int kind, int slot)
{
	char text[64];
	long tag = forge_map_settings_tag(kind, slot);

	snprintf(text, sizeof(text), "!FGT:%d:%d:%lX", kind, slot, (unsigned long)tag);
	forge_network_send_text(machine_index, text);
}

static void net_send_atmosphere(long machine_index)
{
	char text[64];

	snprintf(text, sizeof(text), "!FGA:%d:%02X%02X%02X:%d:%.1f:%.1f:%d:%d", settings.atmosphere,
		(unsigned)(PIN(settings.fog_color[0], 0.0f, 1.0f) * 255.0f + 0.5f),
		(unsigned)(PIN(settings.fog_color[1], 0.0f, 1.0f) * 255.0f + 0.5f),
		(unsigned)(PIN(settings.fog_color[2], 0.0f, 1.0f) * 255.0f + 0.5f),
		(int)(PIN(settings.fog_density, 0.0f, 1.0f) * 100.0f + 0.5f), settings.fog_start, settings.fog_opaque,
		settings.fog_indoors, settings.sky_visibility);
	forge_network_send_text(machine_index, text);
}

static void net_send_movement(long machine_index, int slot)
{
	char text[64];

	snprintf(text, sizeof(text), "!FGM:%d:%d", slot, settings.movement[slot]);
	forge_network_send_text(machine_index, text);
}

static void net_send_link(long machine_index, int slot)
{
	char text[64];

	snprintf(text, sizeof(text), "!FGL:%d:%d", slot, settings.link[slot]);
	forge_network_send_text(machine_index, text);
}

static void net_send_view(long machine_index)
{
	char text[64];

	snprintf(text, sizeof(text), "!FGV:%.0f:%.2f", settings.render_distance, settings.monitor_speed);
	forge_network_send_text(machine_index, text);
}

/* RENDER DISTANCE: the game's far clip moved out while the map sets one,
put back once it doesn't (each frame, from the fog hook) */
static void render_distance_apply(void)
{
	if (settings.render_distance > 0.0f)
	{
		if (render_distance_original <= 0.0f)
			render_distance_original = rasterizer_globals.far_clip_distance > 0.0f ?
				rasterizer_globals.far_clip_distance : 1024.0f;
		rasterizer_globals.far_clip_distance = settings.render_distance;
	}
	else if (render_distance_original > 0.0f)
	{
		rasterizer_globals.far_clip_distance = render_distance_original;
		render_distance_original = 0.0f;
	}
}

static int host_tells(void)
{
	return forge_is_multiplayer_session() && game_connection() == _game_connection_network_server;
}

/* ---------- public code */

struct forge_map_settings const *forge_map_settings_get(void)
{
	return &settings;
}

int forge_map_settings_any(void)
{
	int slot;

	if (settings.display_name[0] || settings.sky[0] || settings.atmosphere != _forge_atmosphere_map_default ||
		settings.render_distance > 0.0f || (settings.monitor_speed > 0.0f && settings.monitor_speed != 1.0f))
	{
		return 1;
	}
	for (slot = 0; slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		if (settings.biped[slot][0] || settings.hands[slot][0] || settings.movement[slot] || settings.link[slot])
			return 1;
	}
	return 0;
}

void forge_map_settings_players_set(int slot, int link, char const *biped, char const *hands, int movement)
{
	static char const *const names[NUMBER_OF_FORGE_MAP_SLOTS] = { "red", "blue", "ffa" };

	if (slot < 0 || slot >= NUMBER_OF_FORGE_MAP_SLOTS)
		return;
	snprintf(players_layer.biped[slot], sizeof(players_layer.biped[slot]), "%s", biped ? biped : "");
	snprintf(players_layer.hands[slot], sizeof(players_layer.hands[slot]), "%s", hands ? hands : "");
	players_layer.movement[slot] = PIN(movement, 0, NUMBER_OF_FORGE_MOVEMENTS - 1);
	players_layer.link[slot] = PIN(link, 0, NUMBER_OF_FORGE_MAP_SLOTS);
	settings_generation++;
	if (link > _forge_link_custom && link <= NUMBER_OF_FORGE_MAP_SLOTS)
		error(_error_silent, "forge map settings: the profile's %s players play %s's", names[slot], names[link - 1]);
	else if (biped && biped[0])
		error(_error_silent, "forge map settings: the profile's %s biped %s, hands %s, movement %s", names[slot], biped,
			hands && hands[0] ? hands : "(the map's)", movement_spartan(movement) ? "spartan" : "its own");
}

void forge_map_settings_players_from_options(char const *(*option)(void *context, char const *key), void *context,
	int campaign)
{
	static char const *const slot_names[NUMBER_OF_FORGE_MAP_SLOTS] = { "red", "blue", "ffa" };
	int slot;

	for (slot = 0; slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		char biped_key[32], hands_key[32], movement_key[32], link_key[32];
		char biped[FORGE_MAP_SETTINGS_PATH_MAX];
		char hands[FORGE_MAP_SETTINGS_PATH_MAX];
		char const *read;
		int link = _forge_link_custom;
		int movement;
		int other;

		snprintf(biped_key, sizeof(biped_key), campaign ? "player_biped" : "biped_%s", slot_names[slot]);
		snprintf(hands_key, sizeof(hands_key), campaign ? "player_hands" : "hands_%s", slot_names[slot]);
		snprintf(movement_key, sizeof(movement_key), campaign ? "player_movement" : "movement_%s", slot_names[slot]);
		/* (each copied as read: a share file's option gives every key in
		the same buffer) */
		read = option(context, biped_key);
		snprintf(biped, sizeof(biped), "%s", read ? read : "");
		read = option(context, hands_key);
		snprintf(hands, sizeof(hands), "%s", read ? read : "");
		read = option(context, movement_key);
		/* ("own": the biped's own; anything else, nothing too, a Spartan's) */
		movement = read && !strcmp(read, "own") ? _forge_movement_own : _forge_movement_spartan;
		/* (a team playing another's: link_red = "blue" or "ffa"; a
		campaign's player has none) */
		snprintf(link_key, sizeof(link_key), "link_%s", slot_names[slot]);
		read = campaign ? NULL : option(context, link_key);
		for (other = 0; read && other < NUMBER_OF_FORGE_MAP_SLOTS; other++)
		{
			if (other != slot && !strcmp(read, slot_names[other]))
				link = other + 1;
		}
		forge_map_settings_players_set(slot, link, biped, hands, movement);
	}
}

void forge_map_settings_level_loading(char const *text)
{
	char const *line;

	memset(&players_layer, 0, sizeof(players_layer));
	ae_forge_level_forget(); /* AE hook */
	settings_clear();
	level_serial++;
	settings_generation++;
	/* (the level's objects go with it) */
	test.slot = NONE;
	test.phase = _test_none;
	test.mirror_unit = test.mirrored_unit = test.killed_unit = NONE;
	sky_state.scenario = NULL;
	if (!text || !strstr(text, "MAP_SETTINGS|"))
		return;
	for (line = text; line && *line; line = strchr(line, '\n'), line = line ? line + 1 : NULL)
	{
		char value[FORGE_MAP_SETTINGS_PATH_MAX];
		int slot = 0;

		value[0] = 0;
		if (!strncmp(line, "MAP_NAME|", 9) && sscanf(line + 9, "%40[^\r\n]", settings.display_name) == 1)
		{
			display_name_clean(settings.display_name);
		}
		else if (!strncmp(line, "MAP_SKY|", 8) && sscanf(line + 8, "%255[^\r\n]", value) == 1)
		{
			snprintf(settings.sky, sizeof(settings.sky), "%s", value);
		}
		else if ((!strncmp(line, "MAP_BIPED|", 10) || !strncmp(line, "MAP_HANDS|", 10)) &&
			sscanf(line + 10, "%d|%255[^\r\n]", &slot, value) == 2 && slot >= 0 && slot < NUMBER_OF_FORGE_MAP_SLOTS)
		{
			snprintf(line[4] == 'B' ? settings.biped[slot] : settings.hands[slot], FORGE_MAP_SETTINGS_PATH_MAX, "%s",
				value);
		}
		else if (!strncmp(line, "MAP_LINK|", 9))
		{
			int link = 0;

			if (sscanf(line + 9, "%d|%d", &slot, &link) == 2 && slot >= 0 && slot < NUMBER_OF_FORGE_MAP_SLOTS)
				settings.link[slot] = PIN(link, 0, NUMBER_OF_FORGE_MAP_SLOTS);
		}
		else if (!strncmp(line, "MAP_MOVEMENT|", 13))
		{
			int movement = 0;

			if (sscanf(line + 13, "%d|%d", &slot, &movement) == 2 && slot >= 0 && slot < NUMBER_OF_FORGE_MAP_SLOTS)
				settings.movement[slot] = PIN(movement, 0, NUMBER_OF_FORGE_MOVEMENTS - 1);
		}
		else if (!strncmp(line, "MAP_RENDER_DISTANCE|", 20))
		{
			settings.render_distance = (float)atof(line + 20);
			settings.render_distance = settings.render_distance > 0.0f ? PIN(settings.render_distance, 256.0f, 16384.0f) : 0.0f;
		}
		else if (!strncmp(line, "MAP_MONITOR_SPEED|", 18))
		{
			settings.monitor_speed = (float)atof(line + 18);
			settings.monitor_speed = settings.monitor_speed > 0.0f ? PIN(settings.monitor_speed, 0.25f, 32.0f) : 0.0f;
		}
		else if (!strncmp(line, "MAP_ATMOSPHERE|", 15))
		{
			struct forge_map_settings read = settings;

			if (sscanf(line + 15, "%d|%f|%f|%f|%f|%f|%f|%d|%d", &read.atmosphere, &read.fog_color[0],
				&read.fog_color[1], &read.fog_color[2], &read.fog_density, &read.fog_start, &read.fog_opaque,
				&read.fog_indoors, &read.sky_visibility) == 9 &&
				read.atmosphere >= 0 && read.atmosphere < NUMBER_OF_FORGE_ATMOSPHERES)
			{
				settings = read;
				settings.sky_visibility = PIN(settings.sky_visibility, 0, NUMBER_OF_FORGE_SKY_VISIBILITIES - 1);
			}
		}
	}
	error(_error_silent, "forge map settings: name '%s', sky '%s', bipeds '%s' '%s' '%s', atmosphere %s",
		settings.display_name, settings.sky, settings.biped[0], settings.biped[1], settings.biped[2],
		atmosphere_presets[settings.atmosphere].name);
}

int forge_map_settings_save_lines(char *text, unsigned long size)
{
	int length = 0;
	int slot;

#define SETTINGS_LINE(...) \
	do { \
		int written = length < (int)size ? snprintf(text + length, size - (unsigned long)length, __VA_ARGS__) : 0; \
		if (written > 0 && length + written < (int)size) length += written; \
	} while (0)

	if (!text || !size)
		return 0;
	text[0] = 0;
	if (!forge_map_settings_any())
		return 0;
	SETTINGS_LINE("MAP_SETTINGS|1\r\n");
	if (settings.display_name[0])
		SETTINGS_LINE("MAP_NAME|%s\r\n", settings.display_name);
	if (settings.sky[0])
		SETTINGS_LINE("MAP_SKY|%s\r\n", settings.sky);
	for (slot = 0; slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		if (settings.biped[slot][0])
			SETTINGS_LINE("MAP_BIPED|%d|%s\r\n", slot, settings.biped[slot]);
		if (settings.hands[slot][0])
			SETTINGS_LINE("MAP_HANDS|%d|%s\r\n", slot, settings.hands[slot]);
		if (settings.movement[slot])
			SETTINGS_LINE("MAP_MOVEMENT|%d|%d\r\n", slot, settings.movement[slot]);
		if (settings.link[slot])
			SETTINGS_LINE("MAP_LINK|%d|%d\r\n", slot, settings.link[slot]);
	}
	if (settings.render_distance > 0.0f)
		SETTINGS_LINE("MAP_RENDER_DISTANCE|%.0f\r\n", settings.render_distance);
	if (settings.monitor_speed > 0.0f && settings.monitor_speed != 1.0f)
		SETTINGS_LINE("MAP_MONITOR_SPEED|%.2f\r\n", settings.monitor_speed);
	if (settings.atmosphere != _forge_atmosphere_map_default)
	{
		SETTINGS_LINE("MAP_ATMOSPHERE|%d|%.3f|%.3f|%.3f|%.3f|%.2f|%.2f|%d|%d\r\n", settings.atmosphere,
			settings.fog_color[0], settings.fog_color[1], settings.fog_color[2], settings.fog_density,
			settings.fog_start, settings.fog_opaque, settings.fog_indoors, settings.sky_visibility);
	}
#undef SETTINGS_LINE
	return length;
}

int forge_map_settings_tags(uint32_t *groups, char const **paths, int capacity)
{
	int count = 0;
	int slot;

	if (settings.sky[0] && count < capacity)
	{
		groups[count] = SKY_GROUP_TAG;
		paths[count++] = settings.sky;
	}
	for (slot = 0; slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		if (settings.biped[slot][0] && count < capacity)
		{
			groups[count] = BIPED_GROUP_TAG;
			paths[count++] = settings.biped[slot];
		}
		if (settings.hands[slot][0] && count < capacity)
		{
			groups[count] = GBXMODEL_GROUP_TAG;
			paths[count++] = settings.hands[slot];
		}
	}
	return count;
}

int forge_map_settings_can_edit(void)
{
	return game_connection() != _game_connection_network_client;
}

void forge_map_settings_set_display_name(char const *name)
{
	if (!forge_map_settings_can_edit())
		return;
	snprintf(settings.display_name, sizeof(settings.display_name), "%s", name ? name : "");
	display_name_clean(settings.display_name);
	if (host_tells())
		net_send_name(-1);
}

/* how many of the first parts of two paths (to each '\\') are the same */
static int path_folders_shared(char const *path, char const *other)
{
	int shared = 0;
	int index;

	for (index = 0; path[index] && other[index]; index++)
	{
		char a = path[index] >= 'A' && path[index] <= 'Z' ? (char)(path[index] - 'A' + 'a') : path[index];
		char b = other[index] >= 'A' && other[index] <= 'Z' ? (char)(other[index] - 'A' + 'a') : other[index];

		if (a != b)
			break;
		if (a == '\\')
			shared++;
	}
	return shared;
}

/* the hands of a profile's biped (a level tag's path) that the Tag Injector took out of the same map, the one whose path is most the
biped's (characters\\elite\\fp\\fp_arms for characters\\elite\\elite_mp): its path in `hands`, 1; 0 and nothing when the profile has no
such biped or no such hands */
static int hands_best(long profile, char const *biped_name, char *hands, int size)
{
	long entry = profile == NONE ? NONE : port_tag_profiles_tag_find(profile, BIPED_GROUP_TAG, biped_name);
	char const *map = entry == NONE ? NULL : port_tag_profiles_tag_map(profile, entry);
	long count = profile == NONE ? 0 : port_tag_profiles_tag_count(profile);
	char const *best = NULL;
	int best_shared = -1;
	long tag;

	for (tag = 0; map && map[0] && tag < count; tag++)
	{
		long level_tag;
		char const *name;
		int shared;

		if (port_tag_profiles_tag_group(profile, tag) != GBXMODEL_GROUP_TAG ||
			_stricmp(port_tag_profiles_tag_map(profile, tag), map) ||
			(level_tag = profile_hands_level_tag(profile, tag)) == NONE || !instance_of(level_tag))
		{
			continue;
		}
		name = XBOX_POINTER(char const, instance_of(level_tag)->name); /* AE hook */
		shared = path_folders_shared(biped_name, name);
		if (shared > best_shared)
		{
			best = name;
			best_shared = shared;
		}
	}
	if (best)
		snprintf(hands, (size_t)size, "%s", best);
	return best != NULL;
}

/* a biped's hands, chosen with it: of a profile's biped, the hands of its map that fit it best (hands_best); of another, or a
biped with none, the map's own */
static void hands_of_biped(int slot, long biped_tag)
{
	struct settings_tag_instance const *biped = instance_of(biped_tag);
	long profiles[2];
	int pass;

	settings.hands[slot][0] = 0;
	profiles[0] = tag_injector_level_baked_profile();
	profiles[1] = tag_injector_level_profile();
	for (pass = 0; biped && biped->name && pass < 2; pass++)
	{
		if (hands_best(profiles[pass], XBOX_POINTER(char const, biped->name), settings.hands[slot], sizeof(settings.hands[slot]))) /* AE hook */
			return;
	}
}

void forge_map_settings_players_body(long profile, char const *biped, char const *hands, int movement_own)
{
	char own[FORGE_MAP_SETTINGS_PATH_MAX];
	int slot;

	/* (no hands given: those of the biped's map that fit it best, else the campaign's own) */
	own[0] = 0;
	if (!hands)
		hands_best(profile, biped ? biped : "", own, (int)sizeof(own));
	for (slot = 0; slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		forge_map_settings_players_set(slot, _forge_link_custom, biped, hands ? hands : own,
			movement_own ? _forge_movement_own : _forge_movement_spartan);
	}
}

void forge_map_settings_set_tag(int kind, int slot, long tag_index)
{
	char *path = settings_path(kind, slot);
	struct settings_tag_instance const *instance = instance_of(tag_index);

	if (!path || !forge_map_settings_can_edit())
		return;
	if (instance && group_matches(kind_group(kind), (unsigned long)instance->group_tag) && instance->name)
		snprintf(path, FORGE_MAP_SETTINGS_PATH_MAX, "%s", XBOX_POINTER(char const, instance->name)); /* AE hook */
	else
		path[0] = 0;
	if (kind == _forge_map_kind_biped)
		hands_of_biped(slot, tag_index);
	settings_generation++;
	if (kind == _forge_map_kind_sky)
		sky_apply();
	if (host_tells())
	{
		net_send_tag(-1, kind, slot);
		if (kind == _forge_map_kind_biped)
			net_send_tag(-1, _forge_map_kind_hands, slot);
	}
}

void forge_map_settings_set_atmosphere(int atmosphere)
{
	struct atmosphere_preset const *preset;

	if (!forge_map_settings_can_edit() || atmosphere < 0 || atmosphere >= NUMBER_OF_FORGE_ATMOSPHERES)
		return;
	settings.atmosphere = atmosphere;
	preset = &atmosphere_presets[atmosphere];
	if (atmosphere != _forge_atmosphere_map_default && atmosphere != _forge_atmosphere_custom)
	{
		settings.fog_color[0] = preset->color[0];
		settings.fog_color[1] = preset->color[1];
		settings.fog_color[2] = preset->color[2];
		settings.fog_density = preset->density;
		settings.fog_start = preset->start;
		settings.fog_opaque = preset->opaque;
		settings.fog_indoors = preset->indoors;
	}
	if (host_tells())
		net_send_atmosphere(-1);
}

void forge_map_settings_set_movement(int slot, int movement)
{
	if (!forge_map_settings_can_edit() || slot < 0 || slot >= NUMBER_OF_FORGE_MAP_SLOTS)
		return;
	settings.movement[slot] = PIN(movement, 0, NUMBER_OF_FORGE_MOVEMENTS - 1);
	settings_generation++;
	if (host_tells())
		net_send_movement(-1, slot);
}

void forge_map_settings_set_link(int slot, int link)
{
	if (!forge_map_settings_can_edit() || slot < 0 || slot >= NUMBER_OF_FORGE_MAP_SLOTS || link == slot + 1)
		return;
	settings.link[slot] = PIN(link, 0, NUMBER_OF_FORGE_MAP_SLOTS);
	settings_generation++;
	if (host_tells())
		net_send_link(-1, slot);
}

int forge_map_settings_link_slot(int slot)
{
	return slot < 0 || slot >= NUMBER_OF_FORGE_MAP_SLOTS ? slot : link_follow(settings.link, slot);
}

void forge_map_settings_set_fog(float const color[3], float density, float start, float opaque, int indoors,
	int sky_visibility)
{
	if (!forge_map_settings_can_edit())
		return;
	settings.fog_color[0] = PIN(color[0], 0.0f, 1.0f);
	settings.fog_color[1] = PIN(color[1], 0.0f, 1.0f);
	settings.fog_color[2] = PIN(color[2], 0.0f, 1.0f);
	settings.fog_density = PIN(density, 0.0f, 1.0f);
	settings.fog_start = PIN(start, 0.0f, 2000.0f);
	settings.fog_opaque = PIN(opaque, settings.fog_start + 1.0f, 4000.0f);
	settings.fog_indoors = indoors != 0;
	settings.sky_visibility = PIN(sky_visibility, 0, NUMBER_OF_FORGE_SKY_VISIBILITIES - 1);
	/* (a preset's values changed are the player's own) */
	if (settings.atmosphere != _forge_atmosphere_map_default)
		settings.atmosphere = _forge_atmosphere_custom;
	if (host_tells())
		net_send_atmosphere(-1);
}

void forge_map_settings_set_render_distance(float distance)
{
	if (!forge_map_settings_can_edit())
		return;
	settings.render_distance = distance > 0.0f ? PIN(distance, 256.0f, 16384.0f) : 0.0f;
	if (host_tells())
		net_send_view(-1);
}

void forge_map_settings_set_monitor_speed(float speed)
{
	if (!forge_map_settings_can_edit())
		return;
	settings.monitor_speed = speed > 0.0f && speed != 1.0f ? PIN(speed, 0.25f, 32.0f) : 0.0f;
	if (host_tells())
		net_send_view(-1);
}

float forge_map_settings_monitor_speed(void)
{
	return settings.monitor_speed > 0.0f ? settings.monitor_speed : 1.0f;
}

void forge_map_settings_reset(void)
{
	if (!forge_map_settings_can_edit())
		return;
	settings_clear();
	settings_generation++;
	sky_apply();
	if (host_tells())
		forge_map_settings_net_send_all(-1);
}

int forge_map_settings_candidates(int kind, long *tags, int capacity)
{
	long count = 0;
	struct settings_tag_instance const *instances =
		(struct settings_tag_instance const *)cache_files_tag_instances(&count);
	unsigned long group = kind_group(kind);
	int found = 0;
	long index;

	if (kind == _forge_map_kind_hands)
	{
		/* (the map's own hands, then each a profile of the level has) */
		struct game_globals_first_person_interface *fp = first_person_interface();
		long profiles[2];
		int pass;

		if (fp && fp->hands.index != NONE && found < capacity)
			tags[found++] = fp->hands.index;
		profiles[0] = tag_injector_level_baked_profile();
		profiles[1] = tag_injector_level_profile();
		for (pass = 0; pass < 2; pass++)
		{
			long profile_count = profiles[pass] == NONE ? 0 : port_tag_profiles_tag_count(profiles[pass]);
			long tag;

			for (tag = 0; tag < profile_count && found < capacity; tag++)
			{
				long level_tag;
				int other;

				if (port_tag_profiles_tag_group(profiles[pass], tag) != GBXMODEL_GROUP_TAG)
					continue;
				level_tag = profile_hands_level_tag(profiles[pass], tag);
				for (other = 0; other < found && tags[other] != level_tag; other++)
				{
				}
				if (level_tag != NONE && other == found)
					tags[found++] = level_tag;
			}
		}
		return found;
	}
	for (index = 0; instances && index < count && found < capacity; index++)
	{
		if ((unsigned long)instances[index].group_tag == group && instances[index].tag_index != NONE &&
			instances[index].name && instances[index].base_address)
		{
			tags[found++] = instances[index].tag_index;
		}
	}
	return found;
}

void forge_map_settings_tag_label(long tag_index, char *text, unsigned long size)
{
	struct settings_tag_instance const *instance = instance_of(tag_index);
	char const *name = instance && instance->name ? XBOX_POINTER(char const, instance->name) : ""; /* AE hook */
	char const *leaf = strrchr(name, '\\');
	char const *parent_end = leaf;
	char const *parent = name;
	int index;

	if (!size)
		return;
	if (!instance)
	{
		snprintf(text, size, "MAP DEFAULT");
		return;
	}
	leaf = leaf ? leaf + 1 : name;
	/* (a map's hands: "FP (BIGASS)") */
	if (strchr(leaf, '#'))
	{
		char const *hash = strchr(leaf, '#');

		snprintf(text, size, "%.*s (%s)", (int)(hash - leaf), leaf, hash + 1);
	}
	/* (a leaf saying little, "fp" or "sky", with its folder's) */
	else if (parent_end && strlen(leaf) <= 3)
	{
		char const *cursor;

		for (cursor = name; cursor < parent_end; cursor++)
		{
			if (*cursor == '\\')
				parent = cursor + 1;
		}
		snprintf(text, size, "%.*s %s", (int)(parent_end - parent), parent, leaf);
	}
	else
	{
		snprintf(text, size, "%s", leaf);
	}
	for (index = 0; text[index]; index++)
	{
		if (text[index] == '_')
			text[index] = ' ';
		else if (text[index] >= 'a' && text[index] <= 'z')
			text[index] = (char)(text[index] - 'a' + 'A');
	}
}

long forge_map_settings_tag(int kind, int slot)
{
	long tag_count = 0;

	if (kind < 0 || kind >= NUMBER_OF_FORGE_MAP_KINDS || slot < 0 || slot >= NUMBER_OF_FORGE_MAP_SLOTS)
		return NONE;
	cache_files_tag_instances(&tag_count);
	if (resolved.serial != level_serial || resolved.generation != settings_generation ||
		resolved.tag_count != tag_count)
	{
		int each_kind, each_slot;

		resolved.serial = level_serial;
		resolved.generation = settings_generation;
		resolved.tag_count = tag_count;
		for (each_kind = 0; each_kind < NUMBER_OF_FORGE_MAP_KINDS; each_kind++)
		{
			for (each_slot = 0; each_slot < NUMBER_OF_FORGE_MAP_SLOTS; each_slot++)
			{
				char const *path;
				long found;

				/* (a slot linked to another: that one's; the Forge map's,
				else the profile's) */
				if (each_kind == _forge_map_kind_sky)
				{
					path = settings.sky;
				}
				else
				{
					path = settings_path(each_kind, link_follow(settings.link, each_slot));
					if (!path || !path[0])
					{
						int profile_slot = link_follow(players_layer.link, each_slot);

						path = each_kind == _forge_map_kind_biped ? players_layer.biped[profile_slot] :
							players_layer.hands[profile_slot];
					}
				}
				found = path && path[0] ? tag_find(kind_group(each_kind), path) : NONE;
				/* (a map's hands, "path#map", added by their path alone: the
				level had none of it, tag_injector.c's level_hands_alias) */
				if (found == NONE && each_kind == _forge_map_kind_hands && path && strchr(path, '#'))
				{
					char base[FORGE_MAP_SETTINGS_PATH_MAX];

					snprintf(base, sizeof(base), "%.*s", (int)(strchr(path, '#') - path), path);
					found = tag_find(GBXMODEL_GROUP_TAG, base);
				}
				resolved.tags[each_kind][each_slot] = found;
			}
		}
	}
	return resolved.tags[kind][kind == _forge_map_kind_sky ? 0 : slot];
}

char const *forge_map_settings_atmosphere_name(int atmosphere)
{
	return atmosphere >= 0 && atmosphere < NUMBER_OF_FORGE_ATMOSPHERES ? atmosphere_presets[atmosphere].name : "";
}

void forge_map_settings_net_send_all(long machine_index)
{
	int slot;

	if (!forge_map_settings_any() && machine_index >= 0)
		return;
	net_send_name(machine_index);
	net_send_tag(machine_index, _forge_map_kind_sky, 0);
	for (slot = 0; slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		net_send_tag(machine_index, _forge_map_kind_biped, slot);
		net_send_tag(machine_index, _forge_map_kind_hands, slot);
	}
	net_send_atmosphere(machine_index);
	net_send_view(machine_index);
	for (slot = 0; slot < NUMBER_OF_FORGE_MAP_SLOTS; slot++)
	{
		net_send_movement(machine_index, slot);
		net_send_link(machine_index, slot);
	}
}

int forge_map_settings_net_receive(char const *text, int from_client)
{
	if (strncmp(text, "!FG", 3))
		return 0;
	/* (the host's word alone) */
	if (from_client || game_connection() != _game_connection_network_client)
		return 1;
	if (!strncmp(text, "!FGN:", 5))
	{
		snprintf(settings.display_name, sizeof(settings.display_name), "%s", text + 5);
		display_name_clean(settings.display_name);
	}
	else if (!strncmp(text, "!FGT:", 5))
	{
		int kind = 0, slot = 0;
		unsigned long tag = 0;
		char *path;

		if (sscanf(text + 5, "%d:%d:%lX", &kind, &slot, &tag) == 3 && (path = settings_path(kind, slot)) != NULL)
		{
			struct settings_tag_instance const *instance = instance_of((long)tag);

			if (instance && group_matches(kind_group(kind), (unsigned long)instance->group_tag) && instance->name)
				snprintf(path, FORGE_MAP_SETTINGS_PATH_MAX, "%s", XBOX_POINTER(char const, instance->name)); /* AE hook */
			else
				path[0] = 0;
			settings_generation++;
			if (kind == _forge_map_kind_sky)
				sky_apply();
		}
	}
	else if (!strncmp(text, "!FGL:", 5))
	{
		int slot = 0, link = 0;

		if (sscanf(text + 5, "%d:%d", &slot, &link) == 2 && slot >= 0 && slot < NUMBER_OF_FORGE_MAP_SLOTS)
		{
			settings.link[slot] = PIN(link, 0, NUMBER_OF_FORGE_MAP_SLOTS);
			settings_generation++;
		}
	}
	else if (!strncmp(text, "!FGM:", 5))
	{
		int slot = 0, movement = 0;

		if (sscanf(text + 5, "%d:%d", &slot, &movement) == 2 && slot >= 0 && slot < NUMBER_OF_FORGE_MAP_SLOTS)
		{
			settings.movement[slot] = PIN(movement, 0, NUMBER_OF_FORGE_MOVEMENTS - 1);
			settings_generation++;
		}
	}
	else if (!strncmp(text, "!FGV:", 5))
	{
		float distance = 0.0f, speed = 0.0f;

		if (sscanf(text + 5, "%f:%f", &distance, &speed) == 2)
		{
			settings.render_distance = distance > 0.0f ? PIN(distance, 256.0f, 16384.0f) : 0.0f;
			settings.monitor_speed = speed > 0.0f ? PIN(speed, 0.25f, 32.0f) : 0.0f;
		}
	}
	else if (!strncmp(text, "!FGA:", 5))
	{
		int atmosphere = 0, density = 0, indoors = 0, sky = 0;
		unsigned int color = 0;
		float start = 0.0f, opaque = 0.0f;

		if (sscanf(text + 5, "%d:%6X:%d:%f:%f:%d:%d", &atmosphere, &color, &density, &start, &opaque, &indoors,
			&sky) == 7 && atmosphere >= 0 && atmosphere < NUMBER_OF_FORGE_ATMOSPHERES)
		{
			settings.atmosphere = atmosphere;
			settings.fog_color[0] = (float)((color >> 16) & 0xFF) / 255.0f;
			settings.fog_color[1] = (float)((color >> 8) & 0xFF) / 255.0f;
			settings.fog_color[2] = (float)(color & 0xFF) / 255.0f;
			settings.fog_density = (float)PIN(density, 0, 100) / 100.0f;
			settings.fog_start = start;
			settings.fog_opaque = opaque;
			settings.fog_indoors = indoors != 0;
			settings.sky_visibility = PIN(sky, 0, NUMBER_OF_FORGE_SKY_VISIBILITIES - 1);
		}
	}
	return 1;
}

/* (any game of the map: solo Forge is a single player game, its player
the globals' player unit, with no game engine) */
long forge_map_settings_player_biped(long player_index, long unit_definition_index)
{
	long biped;

	if (firefight_active())
		return unit_definition_index;
	movement_apply();
	biped = forge_map_settings_tag(_forge_map_kind_biped, player_slot(player_index));
	return biped != NONE ? biped : unit_definition_index;
}

long forge_map_settings_hands(long unit_index, long hands_index)
{
	struct unit_datum *unit;
	long hands;

	if (unit_index == NONE)
		return hands_index;
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!unit)
		return hands_index;
	hands = forge_map_settings_tag(_forge_map_kind_hands, player_slot(unit->unit.player_index));
	return hands != NONE ? hands : hands_index;
}

int forge_map_settings_fog(short local_player_index, int indoors, struct render_fog *fog)
{
	float start, opaque;
	int hidden;

	(void)local_player_index;
	/* (the sky's, each frame: what the level's scenario holds is set by the
	first frame of each level) */
	if (settings.sky[0] || sky_state.applied != NONE || sky_state.serial != level_serial)
		sky_apply();
	movement_apply();
	render_distance_apply();
	if (settings.atmosphere == _forge_atmosphere_map_default || (indoors && !settings.fog_indoors))
		return 0;
	if (settings.atmosphere == _forge_atmosphere_clear || settings.fog_density <= 0.0f)
	{
		fog->atmospheric_maximum_density = 0.0f;
		fog->atmospheric_minimum_distance = 0.0f;
		fog->atmospheric_maximum_distance = 0.0f;
		return settings.sky_visibility == _forge_sky_hidden;
	}
	start = settings.fog_start;
	opaque = settings.fog_opaque;
	/* (closer in a cluster without a sky, as Floodfection's) */
	if (indoors)
	{
		start *= 0.2f;
		opaque *= 0.3f;
	}
	fog->atmospheric_color.red = settings.fog_color[0];
	fog->atmospheric_color.green = settings.fog_color[1];
	fog->atmospheric_color.blue = settings.fog_color[2];
	fog->atmospheric_maximum_density = settings.fog_density;
	fog->atmospheric_minimum_distance = start;
	fog->atmospheric_maximum_distance = MAX(opaque, start + 0.0001f);
	hidden = settings.sky_visibility == _forge_sky_hidden ||
		(settings.sky_visibility == _forge_sky_auto && settings.fog_density >= 0.99f);
	return hidden;
}

/* ---------- the test spawn */

static void test_reflect_point(real_point3d const *point, real_point3d *result)
{
	real distance = (point->x - test.plane_point.x) * test.plane_normal.i +
		(point->y - test.plane_point.y) * test.plane_normal.j + (point->z - test.plane_point.z) * test.plane_normal.k;

	result->x = point->x - 2.0f * distance * test.plane_normal.i;
	result->y = point->y - 2.0f * distance * test.plane_normal.j;
	result->z = point->z - 2.0f * distance * test.plane_normal.k;
}

static void test_reflect_vector(real_vector3d const *vector, real_vector3d *result)
{
	real along = vector->i * test.plane_normal.i + vector->j * test.plane_normal.j + vector->k * test.plane_normal.k;

	result->i = vector->i - 2.0f * along * test.plane_normal.i;
	result->j = vector->j - 2.0f * along * test.plane_normal.j;
	result->k = vector->k - 2.0f * along * test.plane_normal.k;
}

static struct unit_datum *test_unit(long unit_index)
{
	return unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
}

static void test_mirror_delete(void)
{
	if (test_unit(test.mirror_unit))
	{
		unit_delete_all_weapons(test.mirror_unit);
		object_delete(test.mirror_unit);
	}
	test.mirror_unit = NONE;
	test.mirrored_unit = NONE;
}

/* the copy: of the player's unit's biped and colors, a few steps before it,
facing it */
static void test_mirror_create(long unit_index)
{
	struct unit_datum *unit = test_unit(unit_index);
	struct object_placement_data placement;
	real length;
	long mirror;
	short color;

	if (!unit)
		return;
	test.plane_normal.i = unit->object.forward.i;
	test.plane_normal.j = unit->object.forward.j;
	test.plane_normal.k = 0.0f;
	length = (real)sqrt(test.plane_normal.i * test.plane_normal.i + test.plane_normal.j * test.plane_normal.j);
	if (length < 0.001f)
	{
		test.plane_normal.i = 1.0f;
		test.plane_normal.j = 0.0f;
		length = 1.0f;
	}
	test.plane_normal.i /= length;
	test.plane_normal.j /= length;
	test.plane_point.x = unit->object.position.x + test.plane_normal.i * 1.75f;
	test.plane_point.y = unit->object.position.y + test.plane_normal.j * 1.75f;
	test.plane_point.z = unit->object.position.z;
	object_placement_data_new(&placement, unit->definition_index, NONE);
	test_reflect_point(&unit->object.position, &placement.position);
	test_reflect_vector(&unit->object.forward, &placement.forward);
	placement.up.i = 0.0f;
	placement.up.j = 0.0f;
	placement.up.k = 1.0f;
	for (color = 0; color < NUMBER_OF_OBJECT_CHANGE_COLORS; color++)
		placement.change_colors[color] = unit->object.base_change_colors[color];
	mirror = object_new(&placement);
	if (mirror == NONE || !test_unit(mirror))
	{
		error(_error_silent, "forge test spawn: the copy of %s could not be made",
			tag_get_name(unit->definition_index));
		if (mirror != NONE)
			object_delete(mirror);
		return;
	}
	unit_set_actively_controlled(mirror, TRUE);
	SET_FLAG(unit_get(mirror)->unit.flags, _unit_ignored_by_actors_bit, TRUE);
	test.mirror_unit = mirror;
	test.mirrored_unit = unit_index;
}

/* the copy's weapons the player's, slot for slot, the one in hand the
player's, their rounds, and its grenades (as the firing range's mirror) */
static void test_mirror_equip(long unit_index)
{
	struct unit_datum *unit = test_unit(unit_index);
	struct unit_datum *mirror = test_unit(test.mirror_unit);
	short slot;

	if (!unit || !mirror)
		return;
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long held = unit->unit.weapon_object_indices[slot];
		long copied = mirror->unit.weapon_object_indices[slot];
		long wanted = held == NONE ? NONE : weapon_get(held)->definition_index;
		struct object_placement_data placement;
		long copy;

		if (wanted == (copied == NONE ? NONE : weapon_get(copied)->definition_index))
			continue;
		if (copied != NONE)
		{
			unit_network_forget_weapon(test.mirror_unit, slot);
			object_delete(copied);
		}
		if (wanted == NONE)
			continue;
		object_placement_data_new(&placement, wanted, NONE);
		placement.position = mirror->object.position;
		copy = object_new(&placement);
		if (copy != NONE)
			unit_network_add_weapon(test.mirror_unit, copy, slot);
	}
	if (unit->unit.current_weapon_index >= 0 && unit->unit.current_weapon_index < MAXIMUM_WEAPONS_PER_UNIT &&
		(mirror->unit.current_weapon_index == NONE ||
		mirror->unit.weapon_object_indices[mirror->unit.current_weapon_index] == NONE))
	{
		mirror->unit.desired_weapon_index = unit->unit.current_weapon_index;
	}
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long held = unit->unit.weapon_object_indices[slot];
		long copied = mirror->unit.weapon_object_indices[slot];

		if (held != NONE && copied != NONE)
		{
			struct weapon_datum *from = weapon_get(held);
			struct weapon_datum *to = weapon_get(copied);

			memcpy(to->weapon.magazines, from->weapon.magazines, sizeof(to->weapon.magazines));
			to->weapon.heat = from->weapon.heat;
		}
	}
	memcpy(mirror->unit.grenade_counts, unit->unit.grenade_counts, sizeof(mirror->unit.grenade_counts));
}

/* (solo) the player's unit put in place of one of the slot's biped, where
it stands, with the weapons it had that the biped can hold (one holds a
weapon its animations have: unit_can_hold_weapon_definition) and its
grenades: a solo player's death reverts the game to its checkpoint, so the
test spawns no other way. The new unit, NONE when none could be made */
static long test_swap(long unit_index)
{
	struct unit_datum *unit = test_unit(unit_index);
	long biped = forge_map_settings_tag(_forge_map_kind_biped, test.slot);
	long weapons[MAXIMUM_WEAPONS_PER_UNIT];
	short current;
	struct object_placement_data placement;
	long swapped;
	short color;
	short slot;
	int refused = 0;
	int given = 0;

	if (!unit)
		return NONE;
	if (biped == NONE)
		biped = spartan_biped();
	if (biped == NONE)
		return NONE;
	movement_apply();
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long held = unit->unit.weapon_object_indices[slot];

		weapons[slot] = held == NONE ? NONE : weapon_get(held)->definition_index;
	}
	current = unit->unit.current_weapon_index;
	object_placement_data_new(&placement, biped, NONE);
	placement.position = unit->object.position;
	placement.forward = unit->object.forward;
	placement.up = unit->object.up;
	for (color = 0; color < NUMBER_OF_OBJECT_CHANGE_COLORS; color++)
		placement.change_colors[color] = unit->object.base_change_colors[color];
	swapped = object_new(&placement);
	if (swapped == NONE || !test_unit(swapped))
	{
		if (swapped != NONE)
			object_delete(swapped);
		forge_set_status("Test spawn: %s could not be made", tag_get_name(biped));
		return NONE;
	}
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon;

		if (weapons[slot] == NONE || unit_has_weapon_definition_index(swapped, weapons[slot]))
			continue;
		if (!unit_can_hold_weapon_definition(swapped, weapons[slot]))
		{
			refused++;
			continue;
		}
		object_placement_data_new(&placement, weapons[slot], swapped);
		weapon = object_new(&placement);
		if (weapon != NONE && !unit_add_weapon_to_inventory(swapped, weapon, FALSE))
			object_delete(weapon);
		else if (weapon != NONE)
			given++;
	}
	if (current >= 0 && current < MAXIMUM_WEAPONS_PER_UNIT)
		test_unit(swapped)->unit.desired_weapon_index = current;
	memcpy(test_unit(swapped)->unit.grenade_counts, unit->unit.grenade_counts,
		sizeof(test_unit(swapped)->unit.grenade_counts));
	players_set_local_player_unit(0, swapped);
	unit_delete_all_weapons(unit_index);
	object_delete(unit_index);
	if (refused)
		forge_set_status("Test spawn: %s cannot hold %d of your weapons (no animations for them)",
			tag_name_strip_path(tag_get_name(biped)), refused);
	else if (!given && test_unit(swapped)->unit.weapon_object_indices[0] == NONE)
		forge_set_status("Test spawn: %s, unarmed", tag_name_strip_path(tag_get_name(biped)));
	return swapped;
}

static void test_end(void)
{
	test_mirror_delete();
	test.slot = NONE;
	test.phase = _test_none;
	test.killed_unit = NONE;
}

int forge_map_settings_test_start(int slot)
{
	if (slot < 0 || slot >= NUMBER_OF_FORGE_MAP_SLOTS || game_connection() == _game_connection_network_client ||
		local_player_get_player_index(0) == NONE)
	{
		return 0;
	}
	test_end();
	test.slot = slot;
	test.phase = _test_kill;
	error(_error_silent, "forge test spawn: %s biped %s", slot == _forge_map_slot_red ? "red" :
		slot == _forge_map_slot_blue ? "blue" : "ffa", settings.biped[slot][0] ? settings.biped[slot] : "(the map's)");
	return 1;
}

int forge_map_settings_test_active(void)
{
	return test.slot != NONE;
}

void forge_map_settings_test_tick(int forge_mode)
{
	long player_index;
	long unit_index;

	if (test.slot == NONE)
		return;
	player_index = local_player_get_player_index(0);
	unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
	/* (back in Forge mode: the test is over) */
	if (forge_mode && test.phase != _test_kill)
	{
		test_end();
		return;
	}
	switch (test.phase)
	{
	case _test_kill:
		/* (once out of Forge mode, on foot: killed, to spawn again) */
		if (!forge_mode && test_unit(unit_index) && test_unit(unit_index)->object.parent_object_index == NONE)
		{
			test.killed_unit = unit_index;
			test.phase = _test_respawn;
			/* (a network game's host: killed, the game spawns it again as the
			biped; solo: its unit swapped) */
			if (game_connection() == _game_connection_local)
			{
				if (test_swap(unit_index) == NONE)
					test_end();
			}
			else
			{
				unit_kill(unit_index);
			}
		}
		break;
	case _test_respawn:
		if (unit_index != NONE && unit_index != test.killed_unit && test_unit(unit_index) &&
			!TEST_FLAG(test_unit(unit_index)->object.damage_flags, _object_dead_bit))
		{
			test_mirror_create(unit_index);
			test.phase = _test_mirroring;
		}
		break;
	case _test_mirroring:
		/* (the player dead again, or the copy: a new copy as the player
		spawns again) */
		if (unit_index == NONE || unit_index != test.mirrored_unit || !test_unit(test.mirror_unit) ||
			TEST_FLAG(test_unit(test.mirror_unit)->object.damage_flags, _object_dead_bit))
		{
			if (unit_index != test.mirrored_unit || !test_unit(test.mirror_unit))
			{
				test_mirror_delete();
				test.killed_unit = unit_index != NONE && test_unit(unit_index) &&
					TEST_FLAG(test_unit(unit_index)->object.damage_flags, _object_dead_bit) ? unit_index : NONE;
				test.phase = _test_respawn;
			}
			break;
		}
		test_mirror_equip(unit_index);
		break;
	}
}

void forge_map_settings_player_control(long unit_index, struct unit_control_data const *control)
{
	struct unit_control_data mirrored;
	struct unit_datum *unit;
	struct unit_datum *mirror;
	real_point3d place;
	real dx, dy;

	if (test.phase != _test_mirroring || unit_index != test.mirrored_unit || !(unit = test_unit(unit_index)) ||
		!(mirror = test_unit(test.mirror_unit)) || TEST_FLAG(mirror->object.damage_flags, _object_dead_bit))
	{
		return;
	}
	/* (the player's controls turned about the mirror's plane: the way it
	faces and aims reflected, its sideways throttle the other way; it picks
	up nothing) */
	mirrored = *control;
	mirrored.control_flags &= (word)~FLAG(_unit_control_action_bit);
	test_reflect_vector(&control->facing_vector, &mirrored.facing_vector);
	test_reflect_vector(&control->aiming_vector, &mirrored.aiming_vector);
	test_reflect_vector(&control->looking_vector, &mirrored.looking_vector);
	mirrored.throttle.j = -control->throttle.j;
	unit_control(test.mirror_unit, &mirrored);
	/* (put back where it should be when it drifts, or the floor differs) */
	test_reflect_point(&unit->object.position, &place);
	dx = mirror->object.position.x - place.x;
	dy = mirror->object.position.y - place.y;
	if (dx * dx + dy * dy > 0.75f * 0.75f)
	{
		real_vector3d forward;

		test_reflect_vector(&unit->object.forward, &forward);
		object_set_position(test.mirror_unit, &place, &forward, &unit->object.up);
	}
}

int forge_map_settings_damage_ignored(long owner_object_index, long object_index)
{
	(void)object_index;
	/* (the copy's shots hurt no one: it fires back as the player fires) */
	return test.mirror_unit != NONE && owner_object_index == test.mirror_unit;
}
