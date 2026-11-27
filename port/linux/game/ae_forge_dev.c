/*
AE_FORGE_DEV.C

DEV MODE's `forge` command (ae_forge_dev.h).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "ai/actors.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "memory/data.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "saved games/player_profile.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "tag_files/tag_files.h"
#include "ae_forge_dev.h"
#include "ae_forge_maps.h"
#include "../src/port_config.h"
#include "ae_forge_rules.h"
#include "ae_forge_net_rules.h"
#include "ae_map_share.h"
#include "opence_features.h"
#include "tag_pack.h"

#if OPENCE_ENABLE_FORGE

#include "../../forge/include/forge_engine.h"
#include "../../forge/include/forge_map_settings.h"
#include "../../forge/include/forge_volumes.h"
#include "../../forge/include/forge_ai_routes.h"

extern int g_forge_session_enabled;
void ae_forge_dev_select_instance(int best);
void forge_network_send_text(long machine_index, char const *text);
void network_test_restart(void);
unsigned long system_milliseconds(void);

/* the saved map the session was started on, to be marked saved once Forge is up */
static char resave_name[64];
/* the instance the last `forge place` or `forge copy` made (-1: none yet), for `forge gametype last` */
static int last_made = -1;

/* the modes the console asked for, in turn (0 Spartan, 1 Monitor): a `forge mode play` right after a `forge mode monitor` and the edits
between them is not the one that replaces the other */
enum { MODE_QUEUE_SIZE = 4 };
static int mode_asked[MODE_QUEUE_SIZE];
static int mode_asked_count;
/* the tick the Spartan's mode was last taken (the AI's units are made from their markers then), and the tick `forge kill ai` last killed */
static long play_since = -1, killed_at = -1;
/* the Firefight markers placed this session: each stands a step to the left of the last */
static int ff_placed;

/* the edits asked for (ae_forge_dev_edit), carried out one at a time at the Forge updates once they can be */
enum { QUEUE_SIZE = 8, QUEUE_WAIT_UPDATES = 7200, AI_STAND_TICKS = 90 };
static struct
{
	int op, number, has_position;
	char tag[64];
	float position[3], yaw;
	long waited;
} queue[QUEUE_SIZE];
static int queued;

/* a session's start: nothing of a session before it is asked for or remembered (an edit queued for another map's object would run on
this one's, and the instance placed last is another map's) */
static void session_reset(void)
{
	mode_asked_count = 0;
	play_since = killed_at = -1;
	ff_placed = 0;
	queued = 0;
	last_made = -1;
	memset(queue, 0, sizeof(queue));
}

/* the level path of a name: a path or a Custom Edition map's name as it is, else the campaign level of that leaf, else
the multiplayer map's */
static void level_path(char const *name, char *path, int size)
{
	short level;

	if (strchr(name, '\\') || strchr(name, '@'))
	{
		snprintf(path, (size_t)size, "%s", name);
		return;
	}
	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
	{
		char const *solo = main_get_solo_level_name(level);
		char const *leaf = solo ? strrchr(solo, '\\') : NULL;

		if (solo && !_stricmp(leaf ? leaf + 1 : solo, name))
		{
			snprintf(path, (size_t)size, "%s", solo);
			return;
		}
	}
	snprintf(path, (size_t)size, "levels\\test\\%s\\%s", name, name);
}

int ae_forge_dev_start(char const *map, char *text, int size)
{
	char path[128];

	if (game_in_progress() && !main_menu_is_active())
	{
		snprintf(text, (size_t)size, "forge: from the menus only (leave this game first)");
		return 0;
	}
	if (!tag_pack_ready("watanoge_forge"))
	{
		struct tag_pack_state state;

		tag_pack_request("watanoge_forge");
		tag_pack_start_now();
		tag_pack_state("watanoge_forge", &state);
		if (state.failed && !state.writing)
		{
			snprintf(text, (size_t)size, "forge: its tags were not built from the game's maps: %s", state.reason);
			return 0;
		}
		snprintf(text, (size_t)size, "forge: its tags are being built from the game's maps (the main menu says when): ask again");
		return -1;
	}
	level_path(map, path, (int)sizeof(path));
	forge_session_start(0, 1);
	forge_runtime_init();
	session_reset();
	dispose_global_network_game_client();
	dispose_global_network_game_server();
	network_game_accept_remote_connections(FALSE);
	custom_edition_maps_look_again();
	player_ui_set_single_player_local_player_controller(0, 0);
	main_set_map_name(path);
	main_disallow_persistent_storage();
	main_set_difficulty(1);
	game_connection_set(0);
	main_menu_switch_to_single_player();
	player_ui_remember_player1_profile(TRUE);
	snprintf(text, (size_t)size, "forge: a session starts on %s", path);
	return 1;
}


/* the level's ground under a point, looking down from z `top`: where a thing set on it rests (the editor's DROP TO FLOOR sets 0.08 above the
surface); `fallback` when there is none under it */
static float ground_below(float x, float y, float top, float fallback)
{
	real_point3d from;
	real_vector3d down;
	struct collision_result hit;

	from.x = x;
	from.y = y;
	from.z = top;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -200.0f;
	memset(&hit, 0, sizeof(hit));
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit), &from, &down, NONE, &hit))
		return hit.point.z + 0.08f;
	return fallback;
}

/* the distance the level leaves free from a point along a direction on the level (30 at the most) */
static float free_distance(float x, float y, float z, float yaw)
{
	real_point3d from;
	real_vector3d along;
	struct collision_result hit;

	from.x = x;
	from.y = y;
	from.z = z;
	along.i = 30.0f * (float)cos((double)yaw);
	along.j = 30.0f * (float)sin((double)yaw);
	along.k = 0.0f;
	memset(&hit, 0, sizeof(hit));
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit), &from, &along, NONE, &hit))
		return 30.0f * hit.t;
	return 30.0f;
}


/* the way along the ground from a camera place that leaves the most free (30 at the most) with level ground in sight 5 and 9 ahead and nothing low
between: 1 and the yaw and the distance free, else 0 */
static int open_way(float cam_x, float cam_y, float cam_z, float *best_yaw, float *best_free)
{
	float floor_here = ground_below(cam_x, cam_y, cam_z, cam_z - 0.65f);
	int step, found = 0;

	*best_free = -1.0f;
	for (step = 0; step < 24; step++)
	{
		float yaw = (float)step * (6.2831853f / 24.0f);
		float low = free_distance(cam_x, cam_y, floor_here + 0.6f, yaw);
		float high = free_distance(cam_x, cam_y, floor_here + 1.5f, yaw);
		float free_here = low < high ? low : high;
		int clear = free_here >= 14.0f, at;

		/* (what is placed 5 and 9 ahead stands on level ground the camera sees, nothing low between) */
		for (at = 0; at < 2 && clear; at++)
		{
			float distance = at ? 9.0f : 5.0f;
			float ahead_x = cam_x + distance * (float)cos((double)yaw), ahead_y = cam_y + distance * (float)sin((double)yaw);
			float floor_z = ground_below(ahead_x, ahead_y, cam_z, cam_z - 100.0f);
			real_point3d from;
			real_vector3d to;
			struct collision_result hit;

			from.x = cam_x;
			from.y = cam_y;
			from.z = cam_z;
			to.i = ahead_x - from.x;
			to.j = ahead_y - from.y;
			to.k = floor_z + 0.6f - from.z;
			memset(&hit, 0, sizeof(hit));
			clear = fabsf(floor_z - floor_here) <= 1.0f &&
				!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit), &from, &to, NONE, &hit);
		}
		if (clear && free_here > *best_free + 0.01f)
		{
			*best_free = free_here;
			*best_yaw = yaw;
			found = 1;
		}
	}
	return found;
}

/* where the staged scene is looked at from, and which way (the Monitor's camera, or the local player's unit on foot): 0 when there is none */
static int viewer(float *x, float *y, float *z, float *yaw)
{
	if (g_forge.mode == _forge_mode_3d_monitor)
	{
		*x = g_forge.cam_x;
		*y = g_forge.cam_y;
		*z = g_forge.cam_z;
		*yaw = g_forge.cam_yaw;
		return 1;
	}
	{
		long player_index = local_player_get_player_index(0);
		long unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
		real_point3d eye;
		real_vector3d aim;

		if (unit_index == NONE || !object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
			return 0;
		unit_get_camera_position(unit_index, &eye);
		unit_get_aiming_vector(unit_index, &aim);
		*x = eye.x;
		*y = eye.y;
		*z = eye.z;
		*yaw = (float)atan2((double)aim.j, (double)aim.i);
		return 1;
	}
}

/* a thing at x y z seen from the viewer, as one line of the log: where it is, whether it rests on the ground or hovers over it (0.35 over it at the origin; a vehicle's origin rides 1.2 on its suspension), how far it is
from the viewer, which way (0 degrees ahead, positive to the left) and whether it is in view (the level's walls leave it in sight,
within 40 degrees of ahead), out of view, or walled off */
static void where_say(char const *what, float x, float y, float z, float rest_height)
{
	float vx, vy, vz, vyaw, ground, bearing, distance;
	real_point3d from;
	real_vector3d to;
	struct collision_result hit;
	int sight;

	ground = ground_below(x, y, z + 0.5f, z - 100.0f) - 0.08f;
	if (!viewer(&vx, &vy, &vz, &vyaw))
	{
		platform_log("forge dev: where %s: at %.2f %.2f %.2f, %s %.2f above the ground, no viewer", what, x, y, z, z - ground <= rest_height ? "rests" : "hovers", z - ground);
		return;
	}
	bearing = (float)atan2((double)(y - vy), (double)(x - vx)) - vyaw;
	while (bearing > 3.14159265f)
		bearing -= 6.2831853f;
	while (bearing < -3.14159265f)
		bearing += 6.2831853f;
	distance = (float)sqrt((double)((x - vx) * (x - vx) + (y - vy) * (y - vy)));
	from.x = vx;
	from.y = vy;
	from.z = vz;
	to.i = x - vx;
	to.j = y - vy;
	to.k = z + 0.6f - vz;
	memset(&hit, 0, sizeof(hit));
	sight = !collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit), &from, &to, NONE, &hit);
	platform_log("forge dev: where %s: at %.2f %.2f %.2f, %s %.2f above the ground, %.2f from you, bearing %.0f degrees, %s", what, x, y, z,
		z - ground <= rest_height ? "rests" : "hovers", z - ground, distance, bearing * 57.29578f,
		!sight ? "walled off" : fabsf(bearing) <= 0.70f ? "in view" : "out of view");
}

/* the tick the Spartan's mode was last taken (the AI's units are made from their markers then) */

/* whether the AI placed is not yet on its feet: in the Spartan's mode, with spawns that start units, no living actor yet or the mode only
just taken */
static int ai_pending(void)
{
	struct data_iterator iterator;
	struct actor_datum *actor;
	int each, placed = 0, standing = 0;

	if (g_forge.mode == _forge_mode_3d_monitor)
		return 0;
	for (each = 0; each < g_forge.edit_state.ai_spawn_count; each++)
		placed += g_forge.edit_state.ai_spawns[each].active && g_forge.edit_state.ai_spawns[each].unit_count > 0;
	if (!placed || (killed_at >= 0 && game_time_get() - killed_at < 300))
		return 0;
	if (actor_data)
	{
		data_iterator_new(&iterator, actor_data);
		while ((actor = (struct actor_datum *)data_iterator_next(&iterator)) != NULL)
		{
			struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(actor->meta.unit_index, _object_mask_unit);

			standing += unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit);
		}
	}
	return !standing || play_since < 0 || game_time_get() - play_since < AI_STAND_TICKS;
}

/* a hosted or joined Forge game, started as the network test starts one from the main menu */
extern int putenv(char *string);

static int net_start(char const *value, char const *join_address, char const *start_seconds, char *text, int size)
{
	if (game_in_progress() && !main_menu_is_active())
	{
		snprintf(text, (size_t)size, "forge: from the menus only (leave this game first)");
		return 0;
	}
	if (!tag_pack_ready("watanoge_forge"))
	{
		tag_pack_request("watanoge_forge");
		tag_pack_start_now();
		snprintf(text, (size_t)size, "forge: its tags are being built from the game's maps (the main menu says when): ask again");
		return 0;
	}
	forge_session_start(join_address != NULL, 1);
	forge_runtime_init();
	session_reset();
	custom_edition_maps_look_again();
	if (join_address)
	{
		static char environment[96];

		snprintf(environment, sizeof(environment), "HALO_NETWORK_TEST_JOIN_ADDRESS=%s", join_address);
		putenv(environment);
	}
	if (!config_set("debug.network_test", value) || !config_set("debug.network_test_local_players", "1") ||
		!config_set("debug.ae_test_screen", "0") || !config_set("debug.network_test_start", start_seconds))
	{
		snprintf(text, (size_t)size, "forge: the settings could not be set");
		return 0;
	}
	network_test_restart();
	if (!main_menu_is_active())
		main_goto_main_menu();
	return 1;
}

int ae_forge_dev_host(char const *map, char const *variant, char *text, int size)
{
	char value[160];

	snprintf(value, sizeof(value), "host:%s:%s", map, variant);
	if (!net_start(value, NULL, "25", text, size))
		return 0;
	snprintf(text, (size_t)size, "forge: a game of %s hosted as %s starts from the main menu, its start 25 seconds after (version %d)", map, variant, (int)forge_get_network_version());
	return 1;
}

int ae_forge_dev_join(char const *address, char *text, int size)
{
	if (!net_start("join", address, "0", text, size))
		return 0;
	snprintf(text, (size_t)size, "forge: the game at %s is joined from the main menu (version %d)", address, (int)forge_get_network_version());
	return 1;
}

int ae_forge_dev_ready(char *text, int size)
{
	return ae_forge_rules_ready(g_forge_session_enabled, g_forge.initialized && game_in_progress() && !main_menu_is_active(), queued,
		mode_asked_count, g_forge_session_enabled && g_forge.initialized && ai_pending(), text, size);
}

int ae_forge_dev_mode(int monitor, char *text, int size)
{
	if (!g_forge_session_enabled)
	{
		snprintf(text, (size_t)size, "forge: no session (forge start <map> first)");
		return 0;
	}
	if (mode_asked_count >= MODE_QUEUE_SIZE)
		mode_asked_count = MODE_QUEUE_SIZE - 1;
	mode_asked[mode_asked_count++] = monitor != 0;
	snprintf(text, (size_t)size, "forge: the %s's mode is asked for", monitor ? "Monitor" : "Spartan");
	return 1;
}

int ae_forge_dev_status(char *text, int size)
{
	char tags[120], editors[120];

	forge_network_tags_status(tags, (int)sizeof(tags));
	forge_network_editors_status(editors, (int)sizeof(editors));
	snprintf(text, (size_t)size, "forge: session %s, mode %s, %d placed, base map %lu tags, network version %ld; %s%s",
		g_forge_session_enabled ? "on" : "off", g_forge.mode == _forge_mode_3d_monitor ? "Monitor" : "Spartan",
		g_forge.live_instance_count, (unsigned long)forge_get_base_map_tag_count(), forge_get_network_version(), tags, editors);
	return 1;
}

/* a tag path as the palette names it, for comparing: lower case, one kind of slash */
static void tag_key(char const *path, char *key, int size)
{
	int at;

	for (at = 0; path[at] && at < size - 1; at++)
		key[at] = path[at] == '/' ? '\\' : (path[at] >= 'A' && path[at] <= 'Z' ? (char)(path[at] + 32) : path[at]);
	key[at] = 0;
}

static struct forge_catalog_entry const *catalog_find(char const *tag)
{
	char wanted[96], have[96];
	int entry;

	tag_key(tag, wanted, (int)sizeof(wanted));
	for (entry = 0; entry < g_forge.catalog_count; entry++)
	{
		tag_key(g_forge.catalog[entry].tag_path, have, (int)sizeof(have));
		if (!strcmp(wanted, have))
			return &g_forge.catalog[entry];
	}
	return NULL;
}

static int instance_live(int number)
{
	return number >= 0 && number < g_forge.live_instance_count && g_forge.live_instances[number].active;
}

/* MAP SETTINGS > PLAYER BIPEDS (or HANDS) of a team: the level's tag whose last name has what was asked for; `other`, a biped other than
the player's own (an elite's, if the level has one) */
static void settings_pick(int op, char const *asked, int slot)
{
	int kind = op == AE_FORGE_DEV_BIPED ? _forge_map_kind_biped : _forge_map_kind_hands;
	char const *what = op == AE_FORGE_DEV_BIPED ? "biped" : "hands";
	char const *team = slot == 0 ? "red" : slot == 1 ? "blue" : "ffa";
	long tags[256];
	int count = forge_map_settings_candidates(kind, tags, 256);
	int other = !_stricmp(asked, "other");
	long own = NONE, chosen = NONE;
	int each, pass;

	if (other && op == AE_FORGE_DEV_BIPED && local_player_get_player_index(0) != NONE)
	{
		struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(
			player_get(local_player_get_player_index(0))->unit_index, _object_mask_unit);

		own = unit ? unit->definition_index : NONE;
	}
	for (pass = 0; pass < (other ? 2 : 1) && chosen == NONE; pass++)
	{
		for (each = 0; each < count && chosen == NONE; each++)
		{
			char const *name = tag_get_name(tags[each]);

			if (other ? tags[each] != own && (pass == 1 || ae_forge_rules_tag_matches(name, "elite")) :
				ae_forge_rules_tag_matches(name, asked))
				chosen = tags[each];
		}
	}
	if (chosen == NONE)
	{
		platform_log("forge dev: the level has no %s with %s in its name (%d to choose from)", what, asked, count);
		for (each = 0; each < count && each < 8; each++)
			platform_log("forge dev: %s: %s", what, tag_get_name(tags[each]));
		return;
	}
	forge_map_settings_set_tag(kind, slot, chosen);
	chosen = forge_map_settings_tag(kind, slot);
	platform_log("forge dev: %s %s is %s%s", team, what, chosen != NONE ? tag_get_name(chosen) : "the map's own",
		forge_map_settings_can_edit() ? "" : " (only the host sets it)");
}

/* one queued edit, when its conditions hold: 1 done (or refused, said in the log), 0 not yet */
static int edit_run(int index)
{
	int op = queue[index].op, number = queue[index].number;
	struct forge_catalog_entry const *entry;
	int32_t made;

	if (!g_forge.initialized)
		return 0;
	if (op == AE_FORGE_DEV_SNAP)
	{
		g_forge.snap_mode = number == 15 ? _forge_snap_15_deg : number == 45 ? _forge_snap_45_deg :
			number == 90 ? _forge_snap_90_deg : _forge_snap_off;
		platform_log("forge dev: rotation snap %d degrees", number);
		return 1;
	}
	if (op == AE_FORGE_DEV_LIST)
	{
		int each, shown = 0;

		for (each = 0; each < g_forge.live_instance_count && shown < 24; each++)
		{
			struct forge_live_instance const *inst = &g_forge.live_instances[each];

			if (inst->active && !inst->is_default_scenario_object)
			{
				platform_log("forge dev: #%d %s at %.2f %.2f %.2f", each, inst->tag_path, inst->x, inst->y, inst->z);
				shown++;
			}
		}
		platform_log("forge dev: %d placed in all, %d listed", g_forge.live_instance_count, shown);
		return 1;
	}
	if (op == AE_FORGE_DEV_SAVE)
	{
		int saved = forge_ui_save_copy_as(queue[index].tag);

		/* (the map lists are made again: the saved map is a map to host from now on) */
		if (saved)
			custom_edition_maps_look_again();
		platform_log("forge dev: save %s: %s", queue[index].tag, saved ? "saved" : "not saved");
		return 1;
	}
	if (op == AE_FORGE_DEV_GAMETYPE)
	{
		static char const *const kinds[] = { "all", "ctf", "slayer", "oddball", "koth", "race" };
		struct forge_live_instance *inst;
		int kind;

		if (number < 0)
			number = last_made;
		if (!instance_live(number))
		{
			platform_log("forge dev: #%d is not placed", number);
			return 1;
		}
		inst = &g_forge.live_instances[number];
		for (kind = 0; kind < 6 && _stricmp(queue[index].tag, kinds[kind]); kind++)
		{
		}
		if (inst->group_tag == 'spwn' || inst->group_tag == 'tele' || inst->group_tag == 'flag' || inst->group_tag == 'ffsp')
		{
			platform_log("forge dev: #%d is a marker of its own (its gametype is the marker's)", number);
			return 1;
		}
		inst->gametype_flags = (uint16_t)(kind == 0 ? 0 : 1 << (kind - 1));
		if (!inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
			inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
			g_forge.edit_state.placed_objects[inst->edit_entry_index].gametype_flags = inst->gametype_flags;
		if (inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
			inst->edit_entry_index < (int)g_forge.edit_state.default_edit_count)
			g_forge.edit_state.default_edits[inst->edit_entry_index].gametype_flags = inst->gametype_flags;
		platform_log("forge dev: #%d %s is for %s", number, inst->tag_path, kinds[kind < 6 ? kind : 0]);
		return 1;
	}
	if (op == AE_FORGE_DEV_BIPED || op == AE_FORGE_DEV_HANDS)
	{
		settings_pick(op, queue[index].tag, number);
		return 1;
	}
	if (op == AE_FORGE_DEV_FOG)
	{
		forge_map_settings_set_atmosphere(number);
		platform_log("forge dev: atmosphere %s", forge_map_settings_atmosphere_name(number));
		return 1;
	}
	if (op == AE_FORGE_DEV_TESTSPAWN)
	{
		/* MAP SETTINGS' TEST SPAWN: the player spawns as the team's biped once out of the Monitor's mode */
		platform_log("forge dev: test spawn %s", forge_map_settings_test_start(number) ? "begun (leave the Monitor's mode)" :
			"could not begin");
		return 1;
	}
	if (op == AE_FORGE_DEV_SEND)
	{
		/* a line as this machine's own (a joined machine's goes to the host) */
		char line[AE_FORGE_NET_TEXT_SIZE + 1];
		int each;

		for (each = 0; queue[index].tag[each] && each < AE_FORGE_NET_TEXT_SIZE; each++)
			line[each] = queue[index].tag[each] >= 'a' && queue[index].tag[each] <= 'z' ? (char)(queue[index].tag[each] - 32) : queue[index].tag[each];
		line[each] = 0;
		forge_network_send_text(-1, line);
		platform_log("forge dev: sent %s", line);
		return 1;
	}
	if (op == AE_FORGE_DEV_MOVE)
	{
		if (!instance_live(number))
		{
			platform_log("forge dev: #%d is not placed", number);
			return 1;
		}
		forge_3d_move_instance_ext(number, queue[index].position[0], queue[index].position[1], queue[index].position[2], g_forge.live_instances[number].yaw,
			g_forge.live_instances[number].pitch, g_forge.live_instances[number].roll, 0, 1);
		platform_log("forge dev: moved #%d to %.2f %.2f %.2f", number, queue[index].position[0], queue[index].position[1], queue[index].position[2]);
		return 1;
	}
	if (op == AE_FORGE_DEV_AMMO)
	{
		/* the weapon in hand's ammunition to a percentage, once the Spartan is on foot */
		long player_index = local_player_get_player_index(0);
		struct unit_datum *unit = player_index == NONE ? NULL :
			(struct unit_datum *)object_try_and_get_and_verify_type(player_get(player_index)->unit_index, _object_mask_unit);
		short slot = unit ? unit->unit.current_weapon_index : NONE;
		struct weapon_datum *weapon = slot >= 0 && slot < MAXIMUM_WEAPONS_PER_UNIT && unit->unit.weapon_object_indices[slot] != NONE ?
			(struct weapon_datum *)object_try_and_get_and_verify_type(unit->unit.weapon_object_indices[slot], _object_mask_weapon) : NULL;
		struct weapon_definition *definition;
		short magazine;

		if (g_forge.mode == _forge_mode_3d_monitor || !weapon)
			return 0;
		definition = weapon_definition_get(weapon->definition_index);
		for (magazine = 0; magazine < definition->weapon.magazines.count && magazine < 2; magazine++)
		{
			struct weapon_magazine_definition *magazine_definition = TAG_BLOCK_GET_ELEMENT(
				&definition->weapon.magazines, magazine, struct weapon_magazine_definition);

			weapon->weapon.magazines[magazine].rounds_total = (short)((long)magazine_definition->rounds_total_maximum * number / 100);
			weapon->weapon.magazines[magazine].rounds_loaded = (short)MIN((long)weapon->weapon.magazines[magazine].rounds_total,
				(long)magazine_definition->rounds_loaded_maximum * number / 100);
		}
		platform_log("forge dev: the weapon in hand has %d percent of its ammunition", number);
		return 1;
	}
	if (op == AE_FORGE_DEV_KILL)
	{
		/* every AI unit dies: once the Spartan's mode is on and the AI stands */
		struct data_iterator iterator;
		struct actor_datum *actor;
		int killed = 0;

		if (g_forge.mode == _forge_mode_3d_monitor || ai_pending() || !actor_data)
			return 0;
		data_iterator_new(&iterator, actor_data);
		while ((actor = (struct actor_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (object_try_and_get_and_verify_type(actor->meta.unit_index, _object_mask_unit))
			{
				unit_kill(actor->meta.unit_index);
				killed++;
			}
		}
		killed_at = game_time_get();
		platform_log("forge dev: %d AI units killed", killed);
		return 1;
	}
	if (op == AE_FORGE_DEV_WHERE)
	{
		/* where something stands: an instance (-1: the one made last, -3: all that were placed) when the edits before it are done, or the AI's
		units (-2) once the Spartan's mode is on and the AI stands */
		if (number == -2)
		{
			struct data_iterator iterator;
			struct actor_datum *actor;
			int alive = 0, each = 0;

			if (g_forge.mode == _forge_mode_3d_monitor || ai_pending() || !actor_data)
				return 0;
			data_iterator_new(&iterator, actor_data);
			while ((actor = (struct actor_datum *)data_iterator_next(&iterator)) != NULL)
			{
				struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(actor->meta.unit_index, _object_mask_unit);

				if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
				{
					char what[40];
					real_point3d at;

					object_get_origin(actor->meta.unit_index, &at);
					snprintf(what, sizeof(what), "ai unit %d", each);
					where_say(what, at.x, at.y, at.z, 0.35f);
					alive++;
				}
				each++;
			}
			platform_log("forge dev: where ai: %d alive", alive);
			return 1;
		}
		{
			int first = number == -3 ? 0 : number < 0 ? last_made : number;
			int end = number == -3 ? g_forge.live_instance_count : first + 1;
			int each, said = 0;

			if (number != -3 && !instance_live(first))
			{
				platform_log("forge dev: where #%d: is not placed", first);
				return 1;
			}
			for (each = first; each < end && said < 24; each++)
			{
				struct forge_live_instance const *inst = &g_forge.live_instances[each];
				struct object_datum *object;
				char what[100];

				/* (all: what was placed, not the level's own) */
				if (!inst->active || (number == -3 && inst->is_default_scenario_object))
					continue;
				object = inst->object_index == NONE ? NULL : (struct object_datum *)object_try_and_get_and_verify_type(inst->object_index, (unsigned long)-1);
				snprintf(what, sizeof(what), "#%d %s", each, inst->tag_path);
				/* (a marker has no object: its place is the instance's) */
				if (object)
					where_say(what, object->object.position.x, object->object.position.y, object->object.position.z,
						object->object.type == _object_type_vehicle ? 1.2f : 0.35f);
				else
					where_say(what, inst->x, inst->y, inst->z, 0.35f);
				said++;
			}
			if (number == -3)
				platform_log("forge dev: where all: %d placed", said);
		}
		return 1;
	}
	if (g_forge.mode != _forge_mode_3d_monitor || !g_forge.monitor_cam_initialized || g_forge.catalog_count <= 0)
		return 0;
	if (op == AE_FORGE_DEV_FACE)
	{
		/* the Monitor turned to the way the level leaves the most free along the ground, with level ground in sight 5 and 9 ahead, so that a
		scene placed ahead of it is not in a wall's face; where no way is clear from where it flies, it is moved to the nearest place with one
		(rings of 6 points, 8 world units apart) */
		float x = g_forge.cam_x, y = g_forge.cam_y, yaw, free_distance_here = -1.0f;
		int ring, point, found = open_way(x, y, g_forge.cam_z, &yaw, &free_distance_here);

		for (ring = 1; !found && ring <= 8; ring++)
		{
			for (point = 0; !found && point < 6 * ring; point++)
			{
				float angle = (float)point * (6.2831853f / (float)(6 * ring));
				float try_x = g_forge.cam_x + 8.0f * (float)ring * (float)cos((double)angle);
				float try_y = g_forge.cam_y + 8.0f * (float)ring * (float)sin((double)angle);
				float floor_z = ground_below(try_x, try_y, g_forge.cam_z + 2.0f, g_forge.cam_z - 100.0f);
				real_point3d inside;

				inside.x = try_x;
				inside.y = try_y;
				inside.z = floor_z + 1.0f;
				if (floor_z > g_forge.cam_z - 50.0f && scenario_leaf_index_from_point(&inside) != NONE &&
					open_way(try_x, try_y, floor_z + 0.57f, &yaw, &free_distance_here))
				{
					x = try_x;
					y = try_y;
					g_forge.cam_x = x;
					g_forge.cam_y = y;
					g_forge.cam_z = floor_z + 0.57f;
					found = 1;
				}
			}
		}
		if (found)
			g_forge.cam_yaw = yaw;
		g_forge.cam_pitch = 0.0f;
		if (!found)
			platform_log("forge dev: facing: no clear way ahead anywhere near");
		else
			platform_log("forge dev: facing %.0f degrees at %.2f %.2f, the level leaves %.1f free ahead", yaw * 57.29578f, x, y, free_distance_here);
		return 1;
	}
	if (op == AE_FORGE_DEV_VOLUME)
	{
		/* a volume of a kind, where the Monitor's feet would stand (or at x y z, the feet there) */
		static int const effects[AE_FORGE_VOLUME_KINDS] = { _forge_volume_gravity, _forge_volume_insta_kill,
			_forge_volume_constant_damage, _forge_volume_apply_damage, _forge_volume_one_way, _forge_volume_ai_patrol };
		int kind = ae_forge_rules_volume_kind(queue[index].tag);
		struct ae_forge_volume_made made;
		struct forge_volume settings;
		int32_t made_idx;
		float x = queue[index].has_position ? queue[index].position[0] : g_forge.cam_x;
		float y = queue[index].has_position ? queue[index].position[1] : g_forge.cam_y;
		float feet = queue[index].has_position ? queue[index].position[2] : g_forge.cam_z - 0.65f;

		if (!ae_forge_rules_volume_made(kind, &made))
		{
			platform_log("forge dev: volume: no kind %s", queue[index].tag);
			return 1;
		}
		made_idx = forge_3d_spawn_object_at(FORGE_GROUP_VOLUME, FORGE_VOLUME_BOX_PATH, 1, x, y, feet + made.up, 0.0f, 0.0f, 0.0f, 0, 1);
		if (made_idx < 0 || !forge_volume_get(made_idx))
		{
			platform_log("forge dev: the volume could not be placed");
			return 1;
		}
		settings = *forge_volume_get(made_idx);
		settings.shape = _forge_volume_box;
		settings.effect = (uint8_t)effects[kind];
		settings.damage = number >= 0 ? (float)number : made.value_default;
		settings.size[0] = made.size[0];
		settings.size[1] = made.size[1];
		settings.size[2] = made.size[2];
		if (kind == AE_FORGE_VOLUME_PATROL)
		{
			/* (an AI PATROL volume has a number of its own, which an AI spawn's goal names) */
			int number_free = forge_volume_patrol_free_number();

			if (!number_free)
			{
				platform_log("forge dev: no free AI patrol number");
				return 1;
			}
			settings.flags = (uint8_t)((settings.flags & ~FORGE_VOLUME_PATROL_MASK) | (number_free << FORGE_VOLUME_PATROL_SHIFT));
		}
		forge_volume_set(made_idx, &settings, 1);
		last_made = (int)made_idx;
		platform_log("forge dev: volume %s as #%d at %.2f %.2f %.2f, %.2f x %.2f x %.2f, %.0f", queue[index].tag, (int)made_idx, x, y,
			feet + made.up, made.size[0], made.size[1], made.size[2], settings.damage);
		return 1;
	}
	if (op == AE_FORGE_DEV_FF)
	{
		/* a Firefight marker or prop, ahead of the Monitor (a marker 9 world units, the drop area 5 and 2.5 to the side of the rack's line, the rack 1.2), each marker beside the last */
		int kind = ae_forge_rules_ff_unpack_kind(number), round = ae_forge_rules_ff_unpack_round(number);
		int wave = ae_forge_rules_ff_unpack_wave(number);
		/* (a row of markers spreads across the view, the first ahead, the next to its left and right in turn, 2.5 apart; each 9 ahead, the
		props nearer) */
		float left = kind == AE_FORGE_FF_DROP ? 2.5f : 2.5f * (float)((ff_placed + 1) / 2) * (ff_placed % 2 ? 1.0f : -1.0f);
		/* (the rack a step ahead, within its reach, so that the player who flies out of the Monitor stands by it and sees it) */
		float ahead = kind == AE_FORGE_FF_RACK ? 1.2f : kind == AE_FORGE_FF_DROP ? 5.0f : 9.0f;
		float x = g_forge.cam_x + ahead * (float)cos((double)g_forge.cam_yaw) - left * (float)sin((double)g_forge.cam_yaw);
		float y = g_forge.cam_y + ahead * (float)sin((double)g_forge.cam_yaw) + left * (float)cos((double)g_forge.cam_yaw);
		float feet = ground_below(x, y, g_forge.cam_z, g_forge.cam_z - 0.65f);
		int32_t placed = -1;

		if (kind == AE_FORGE_FF_RACK || kind == AE_FORGE_FF_DROP)
		{
			struct forge_catalog_entry const *prop = catalog_find(kind == AE_FORGE_FF_RACK ? "firefight\\props\\weapon_rack_resupply" :
				"firefight\\props\\drop_area_pod");

			if (!prop)
			{
				int each, firefight = 0;

				for (each = 0; each < g_forge.catalog_count; each++)
					firefight += !_strnicmp(g_forge.catalog[each].tag_path, "firefight", 9);
				platform_log("forge dev: the palette has no %s (%d entries, %d of the game's props)", kind == AE_FORGE_FF_RACK ? "rack" : "drop area",
					g_forge.catalog_count, firefight);
				return 1;
			}
			placed = forge_3d_spawn_object_at(prop->group_tag, prop->tag_path, prop->source_cache_id, x, y, feet, g_forge.cam_yaw, 0.0f, 0.0f, 0, 1);
		}
		else if (kind == AE_FORGE_FF_GENERATOR)
			placed = forge_3d_spawn_object_at('flag', "objective\\firefight_generator", 1, x, y, feet, g_forge.cam_yaw, 0.0f, 0.0f, 0, 1);
		else
		{
			static int const types[AE_FORGE_FF_KINDS] = { _forge_ff_spawn_ai_infantry_only, _forge_ff_spawn_ai_and_vehicles,
				_forge_ff_spawn_enemy_vehicle_only, _forge_ff_spawn_enemy_ghost_only, _forge_ff_spawn_enemy_heavy_veh, FF_DROP_SPAWN_TYPE_ANY,
				_forge_ff_spawn_player_vehicle, _forge_ff_spawn_player_spawn, 0, 0, 0 };

			placed = forge_3d_spawn_object_at('ffsp', "spawn\\firefight_spawn", 1, x, y, feet, g_forge.cam_yaw, 0.0f, 0.0f,
				FORGE_FF_PACK_USAGE(types[kind], round, wave), 1);
		}
		if (placed >= 0)
		{
			last_made = (int)placed;
			if (kind != AE_FORGE_FF_RACK)
				ff_placed++;
		}
		platform_log("forge dev: firefight kind %d round %d wave %d as #%d at %.2f %.2f %.2f", kind, round, wave, (int)placed, x, y, feet);
		return 1;
	}
	if (op == AE_FORGE_DEV_AI)
	{
		/* an AI spawn of the actor, ahead of the Monitor, on a team, with a goal, kept alive by respawning */
		int team = ae_forge_rules_ai_unpack_team(number), goal = ae_forge_rules_ai_unpack_goal(number);
		int respawn = ae_forge_rules_ai_unpack_respawn(number), each, count = 0;
		struct forge_catalog_entry const *found = NULL;
		struct forge_ai_spawn_entry settings;
		int numbers[FORGE_VOLUME_PATROL_MAX + 1];
		/* (a hunter stands farther, in sight, so that it is seen coming) */
		float ahead = goal == _forge_ai_goal_hunt ? 20.0f : 5.0f;
		float x = g_forge.cam_x + ahead * (float)cos((double)g_forge.cam_yaw);
		float y = g_forge.cam_y + ahead * (float)sin((double)g_forge.cam_yaw);
		int32_t spawned;

		if (!forge_ai_map_editable())
		{
			platform_log("forge dev: this map has no AI");
			return 1;
		}
		for (each = 0; each < g_forge.catalog_count && !found; each++)
		{
			struct forge_catalog_entry const *row = &g_forge.catalog[each];

			if (row->group_tag == FORGE_GROUP_AI_SPAWN && !forge_ai_row_is_random(row) &&
				ae_forge_rules_tag_matches(row->tag_path, queue[index].tag))
				found = row;
		}
		if (!found)
		{
			platform_log("forge dev: the palette has no AI spawn with %s in its name", queue[index].tag);
			return 1;
		}
		spawned = forge_3d_spawn_object_at(FORGE_GROUP_AI_SPAWN, found->tag_path, found->source_cache_id, x, y,
			ground_below(x, y, g_forge.cam_z, g_forge.cam_z - 0.65f), g_forge.cam_yaw, 0.0f, 0.0f, (int16_t)team, 1);
		if (spawned < 0 || !forge_ai_spawn_settings_get(spawned, &settings))
		{
			platform_log("forge dev: the AI spawn could not be placed");
			return 1;
		}
		settings.goal = (uint8_t)goal;
		if (goal == _forge_ai_goal_patrol || goal == _forge_ai_goal_route)
		{
			count = goal == _forge_ai_goal_patrol ? forge_volumes_patrol_numbers(numbers, FORGE_VOLUME_PATROL_MAX) :
				forge_ai_route_numbers(numbers, FORGE_VOLUME_PATROL_MAX);
			settings.goal_link = (uint8_t)ae_forge_rules_ai_link(0, numbers, count);
			if (!settings.goal_link)
			{
				platform_log("forge dev: no %s to go by: the goal is none", goal == _forge_ai_goal_patrol ? "patrol volume" : "route");
				settings.goal = _forge_ai_goal_none;
			}
		}
		if (respawn)
		{
			settings.respawn_alive = 1;
			settings.respawn_delay_seconds = (int16_t)respawn;
			settings.respawn_total = 0;
		}
		forge_ai_spawn_settings_set(spawned, &settings);
		last_made = (int)spawned;
		platform_log("forge dev: ai %s as #%d at %.2f %.2f, team %d, goal %d (%d), respawn %d", found->tag_path, (int)spawned, x, y, team,
			settings.goal, settings.goal_link, respawn);
		return 1;
	}
	if (op == AE_FORGE_DEV_ROUTE)
	{
		/* a route of `number` points, 5 world units apart, in a line across the view 8 ahead of the Monitor (seen from the side, not end on),
		the first to the Monitor's right */
		int32_t previous, point = -1;
		int each;
		float lateral = -2.5f * (float)(number - 1);
		float first_x = g_forge.cam_x + 8.0f * (float)cos((double)g_forge.cam_yaw) - lateral * (float)sin((double)g_forge.cam_yaw);
		float first_y = g_forge.cam_y + 8.0f * (float)sin((double)g_forge.cam_yaw) + lateral * (float)cos((double)g_forge.cam_yaw);

		previous = forge_3d_spawn_object_at(FORGE_GROUP_AI_ROUTE, FORGE_AI_ROUTE_PATH, 1, first_x, first_y,
			ground_below(first_x, first_y, g_forge.cam_z, g_forge.cam_z - 0.65f), g_forge.cam_yaw, 0.0f, 0.0f, 0, 1);
		for (each = 1; previous >= 0 && each < number; each++)
		{
			int16_t usage = forge_ai_route_copy_usage(previous);

			if (!usage)
				break;
			{
				float side = lateral + 5.0f * (float)each;
				float px = g_forge.cam_x + 8.0f * (float)cos((double)g_forge.cam_yaw) - side * (float)sin((double)g_forge.cam_yaw);
				float py = g_forge.cam_y + 8.0f * (float)sin((double)g_forge.cam_yaw) + side * (float)cos((double)g_forge.cam_yaw);

				point = forge_3d_spawn_object_at(FORGE_GROUP_AI_ROUTE, FORGE_AI_ROUTE_PATH, 1, px, py,
					ground_below(px, py, g_forge.cam_z, g_forge.cam_z - 0.65f), g_forge.cam_yaw, 0.0f, 0.0f, usage, 1);
			}
			if (point < 0)
				break;
			previous = point;
		}
		platform_log("forge dev: route of %d points%s", each, previous >= 0 ? "" : " (not made)");
		return 1;
	}
	if (op == AE_FORGE_DEV_PLACE)
	{
		float x, y, z, yaw;
		char other[96];

		entry = catalog_find(queue[index].tag);
		if (!entry && ae_forge_rules_tag_alias(queue[index].tag, other, (int)sizeof(other)))
			entry = catalog_find(other);
		if (!entry)
		{
			int each, local = 0, vehicles = 0;

			for (each = 0; each < g_forge.catalog_count; each++)
			{
				local += g_forge.catalog[each].source_cache_id == 0;
				vehicles += g_forge.catalog[each].group_tag == 'vehi';
			}
			platform_log("forge dev: place: the palette has no %s (%d entries, %d of the map's own, %d vehicles)", queue[index].tag,
				g_forge.catalog_count, local, vehicles);
			return 1;
		}
		yaw = queue[index].has_position ? queue[index].yaw * 0.017453293f : g_forge.cam_yaw;
		x = queue[index].has_position ? queue[index].position[0] : g_forge.cam_x + 5.0f * (float)cos((double)g_forge.cam_yaw);
		y = queue[index].has_position ? queue[index].position[1] : g_forge.cam_y + 5.0f * (float)sin((double)g_forge.cam_yaw);
		/* (with no position it rests on the ground there, as one a player drops does; a position given is where it stands) */
		z = queue[index].has_position ? queue[index].position[2] : ground_below(x, y, g_forge.cam_z, g_forge.cam_z);
		made = forge_3d_spawn_object_at(entry->group_tag, entry->tag_path, entry->source_cache_id, x, y, z, yaw, 0.0f, 0.0f, 0, 1);
		platform_log("forge dev: %s %s as #%d at %.2f %.2f %.2f", made >= 0 ? "placed" : "could not place", entry->tag_path,
			(int)made, x, y, z);
		if (made >= 0)
			last_made = (int)made;
		return 1;
	}
	if (!instance_live(number))
	{
		platform_log("forge dev: #%d is not placed", number);
		return 1;
	}
	if (op == AE_FORGE_DEV_COPY)
	{
		struct forge_live_instance const *from = &g_forge.live_instances[number];

		made = forge_3d_spawn_object_at(from->group_tag, from->tag_path, from->source_cache_id, from->x + 3.0f, from->y,
			from->z + 0.3f, from->yaw, from->pitch, from->roll, from->team_designator, 1);
		if (made >= 0)
			g_forge.live_instances[made].gametype_flags = g_forge.live_instances[number].gametype_flags;
		platform_log("forge dev: %s #%d as #%d", made >= 0 ? "copied" : "could not copy", number, (int)made);
	}
	else if (op == AE_FORGE_DEV_DELETE)
	{
		forge_3d_delete_instance(number, 1);
		platform_log("forge dev: deleted #%d, %d placed now", number, g_forge.live_instance_count);
	}
	else if (op == AE_FORGE_DEV_GRAB)
	{
		ae_forge_dev_select_instance(number);
		platform_log("forge dev: holding #%d %s", number, g_forge.live_instances[number].tag_path);
	}
	return 1;
}

/* the seconds spent in a session, written to CAREER's FORGE TIME every ten seconds (a frame's gap over a second is a stall, not time) */
static void time_update(void)
{
	static unsigned long last_ms, pending_ms;
	unsigned long now = system_milliseconds(), gap = now - last_ms;

	if (last_ms && gap < 1000)
		pending_ms += gap;
	last_ms = now;
	if (pending_ms >= 10000)
	{
		char value[24];

		snprintf(value, sizeof(value), "%ld", (long)config_integer("forge.seconds") + (long)(pending_ms / 1000));
		config_write("forge.seconds", value);
		pending_ms %= 1000;
	}
}

static void edit_update(void)
{
	time_update();
	if (resave_name[0] && g_forge.initialized)
	{
		snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", resave_name);
		g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
		g_forge.has_saved_map_once = 1;
		resave_name[0] = 0;
	}
	if (!queued)
		return;
	if (edit_run(0))
		queue[0].waited = -1;
	else if (++queue[0].waited > QUEUE_WAIT_UPDATES)
	{
		platform_log("forge dev: op %d dropped (the Monitor never flew)", queue[0].op);
		queue[0].waited = -1;
	}
	if (queue[0].waited < 0)
	{
		memmove(&queue[0], &queue[1], sizeof(queue[0]) * (size_t)(queued - 1));
		queued--;
	}
}

int ae_forge_dev_edit(int op, char const *tag, int number, int has_position, float const *position, float yaw_degrees,
	char *text, int size)
{
	if (!g_forge_session_enabled)
	{
		snprintf(text, (size_t)size, "forge: no session (forge start <map> first)");
		return 0;
	}
	if (queued >= QUEUE_SIZE)
	{
		snprintf(text, (size_t)size, "forge: %d edits are waiting already", queued);
		return 0;
	}
	memset(&queue[queued], 0, sizeof(queue[queued]));
	queue[queued].op = op;
	queue[queued].number = number;
	queue[queued].has_position = has_position;
	queue[queued].yaw = yaw_degrees;
	if (tag)
		snprintf(queue[queued].tag, sizeof(queue[queued].tag), "%s", tag);
	if (has_position && position)
		memcpy(queue[queued].position, position, sizeof(queue[queued].position));
	queued++;
	snprintf(text, (size_t)size, "forge: edit %d asked for (done when the Monitor flies: see the log's forge dev lines)", queued);
	return 1;
}

int ae_forge_dev_play(char const *name, char const *gametype, char *text, int size)
{
	char value[160];
	struct ae_forge_map const *map;

	ae_forge_maps_scan();
	map = ae_forge_maps_get(ae_forge_maps_find(name));
	if (!map)
	{
		snprintf(text, (size_t)size, "forge play: no saved map %s (forge save <name> first)", name);
		return 0;
	}
	if (!map->level[0])
	{
		snprintf(text, (size_t)size, "forge play: %s is a map this game does not play", name);
		return 0;
	}
	snprintf(value, sizeof(value), "local:%s:%s", map->level, gametype);
	if (!config_set("debug.network_test", value) || !config_set("debug.network_test_local_players", "1") ||
		!config_set("debug.network_test_gametype", gametype) || !config_set("debug.ae_test_screen", "0") ||
		!config_set("debug.network_test_start", "0"))
	{
		snprintf(text, (size_t)size, "forge play: the settings could not be set");
		return 0;
	}
	network_test_restart();
	if (!main_menu_is_active())
		main_goto_main_menu();
	snprintf(text, (size_t)size, "forge play: %s as %s starts from the main menu", map->level, gametype);
	return 1;
}

void ae_forge_dev_resave(char const *file)
{
	snprintf(resave_name, sizeof(resave_name), "%s", file ? file : "");
}

int ae_forge_dev_gametype_plays(unsigned flags)
{
	extern struct game_variant game_variant_global;

	if (g_forge_session_enabled || !game_engine_running())
		return 1;
	return ae_forge_rules_gametype_plays(flags, (int)game_variant_global.game_engine_index);
}

int ae_forge_dev_save_map(void)
{
	if (!g_forge_session_enabled || !g_forge.initialized)
		return 0;
	forge_trigger_save_map_action();
	return 1;
}

int ae_forge_dev_saved(char *name, int size)
{
	name[0] = 0;
	if (!g_forge_session_enabled || !g_forge.has_saved_map_once || !g_forge.save_map_name[0])
		return 0;
	snprintf(name, (size_t)size, "%s", g_forge.save_map_name);
	return 1;
}

int ae_forge_dev_key_taken(int vk)
{
	int binds[FORGE_BIND_COUNT], action, vk_bind, gp;

	for (action = 0; action < FORGE_BIND_COUNT; action++)
	{
		vk_bind = gp = 0;
		ae_forge_binds_get(action, &vk_bind, &gp);
		binds[action] = vk_bind;
	}
	return ae_forge_rules_key_taken(g_forge_session_enabled != 0, vk, binds, FORGE_BIND_COUNT);
}

int ae_forge_binds_get(int action, int *vk, int *gp)
{
	if (action < 0 || action >= FORGE_BIND_COUNT)
		return 0;
	*vk = g_forge.bind_vk[action];
	*gp = g_forge.bind_gp[action];
	return 1;
}

void ae_forge_binds_set(int action, int vk, int gp)
{
	if (action >= 0 && action < FORGE_BIND_COUNT)
	{
		g_forge.bind_vk[action] = vk;
		g_forge.bind_gp[action] = gp;
	}
}

float ae_forge_pad_get(int which)
{
	return which ? g_forge.controller_fly_speed : g_forge.controller_look_sens;
}

void ae_forge_pad_set(int which, float value)
{
	if (which)
		g_forge.controller_fly_speed = value;
	else
		g_forge.controller_look_sens = value;
}

void ae_forge_binds_load(void)
{
	forge_controls_load();
}

void ae_forge_binds_save(void)
{
	forge_controls_save();
}

void ae_forge_binds_defaults(void)
{
	forge_controls_init_defaults();
}

void ae_forge_bind_names(int vk, int gp, char *key, int key_size, char *pad, int pad_size)
{
	snprintf(key, (size_t)key_size, "%s", forge_vk_display_name(vk));
	snprintf(pad, (size_t)pad_size, "%s", forge_gp_display_name(gp));
}

int ae_forge_dev_session(void)
{
	return g_forge_session_enabled != 0;
}

int ae_forge_dev_mode_toggle(int mode)
{
	edit_update();
	/* (a mode already taken is no toggle) */
	while (mode_asked_count && (mode_asked[0] != 0) == (mode != 0))
	{
		memmove(&mode_asked[0], &mode_asked[1], sizeof(mode_asked[0]) * (size_t)(mode_asked_count - 1));
		mode_asked_count--;
	}
	if (!mode_asked_count)
		return 0;
	/* (the edits asked for first: a volume or a spawn is made while the Monitor flies, before it leaves) */
	if (!mode_asked[0])
	{
		/* (`forge kill ai`, `forge where ai` and `forge ff ammo` wait for the Spartan's mode themselves) */
		int each, edits = 0;

		for (each = 0; each < queued; each++)
			edits += queue[each].op != AE_FORGE_DEV_KILL && queue[each].op != AE_FORGE_DEV_AMMO &&
				!(queue[each].op == AE_FORGE_DEV_WHERE && queue[each].number == -2);
		if (edits)
			return 0;
	}
	if (!mode_asked[0])
		play_since = game_time_get();
	memmove(&mode_asked[0], &mode_asked[1], sizeof(mode_asked[0]) * (size_t)(mode_asked_count - 1));
	mode_asked_count--;
	return 1;
}

#else

int ae_forge_dev_host(char const *map, char const *variant, char *text, int size)
{
	(void)map;
	(void)variant;
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

int ae_forge_dev_join(char const *address, char *text, int size)
{
	(void)address;
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

int ae_forge_dev_ready(char *text, int size)
{
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

int ae_forge_dev_start(char const *map, char *text, int size)
{
	(void)map;
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

int ae_forge_dev_mode(int monitor, char *text, int size)
{
	(void)monitor;
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

int ae_forge_dev_status(char *text, int size)
{
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

int ae_forge_dev_mode_toggle(int mode)
{
	(void)mode;
	return 0;
}

int ae_forge_dev_edit(int op, char const *tag, int number, int has_position, float const *position, float yaw_degrees,
	char *text, int size)
{
	(void)op;
	(void)tag;
	(void)number;
	(void)has_position;
	(void)position;
	(void)yaw_degrees;
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

int ae_forge_binds_get(int action, int *vk, int *gp)
{
	(void)action;
	*vk = *gp = 0;
	return 0;
}

void ae_forge_binds_set(int action, int vk, int gp)
{
	(void)action;
	(void)vk;
	(void)gp;
}

float ae_forge_pad_get(int which)
{
	(void)which;
	return 1.0f;
}

void ae_forge_pad_set(int which, float value)
{
	(void)which;
	(void)value;
}

void ae_forge_binds_load(void)
{
}

void ae_forge_binds_save(void)
{
}

void ae_forge_binds_defaults(void)
{
}

void ae_forge_bind_names(int vk, int gp, char *key, int key_size, char *pad, int pad_size)
{
	(void)vk;
	(void)gp;
	snprintf(key, (size_t)key_size, "-");
	snprintf(pad, (size_t)pad_size, "-");
}

int ae_forge_dev_session(void)
{
	return 0;
}

int ae_forge_dev_play(char const *name, char const *gametype, char *text, int size)
{
	(void)name;
	(void)gametype;
	snprintf(text, (size_t)size, "forge: this build has no Forge");
	return 0;
}

void ae_forge_dev_resave(char const *file)
{
	(void)file;
}

int ae_forge_dev_gametype_plays(unsigned flags)
{
	(void)flags;
	return 1;
}

int ae_forge_dev_save_map(void)
{
	return 0;
}

int ae_forge_dev_saved(char *name, int size)
{
	(void)size;
	name[0] = 0;
	return 0;
}

int ae_forge_dev_key_taken(int vk)
{
	(void)vk;
	return 0;
}

#endif
