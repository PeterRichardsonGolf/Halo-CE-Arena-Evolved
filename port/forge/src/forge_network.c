/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Collaborative P2P Forge Network Synchronization:
 * - Bidirectional Spawn (!FS: / !FH:), Transform (!FM:), Delete (!FD:) and Firefight marker usage (!FU:) replication
 * - AI spawn settings (!FA:) and GOAL (!FJ:), a placed scenery's SCALE (!FZ:), a volume's settings (!FW:),
 *   an emitter's DEAL DAMAGE (!FN:), a scenery's COLLISION (!FK:)
 * - Per-Player Monitor vs. Spartan mode & 3D Monitor flight state synchronization (!FT:)
 * - Late-join / full-session state synchronization (!FQ)
 * - Automatic client-side external tag & .ocetags faulting on object creation
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#include "cseries.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "game/game.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "items/items.h"
#include "scenario/scenario_definitions.h"
#include "scenario/scenario.h"
#include "structures/structure_bsp_definitions.h"
#include "tag_files/tag_groups.h"
#include "network_distributed.h"
#include "network_chat.h"

#include "ae_forge_rules.h" /* AE hook */
#include "ae_forge_net_rules.h" /* AE hook */
#include "forge_engine.h"
#include "forge_map_settings.h"
#include "forge_volumes.h"

/* (an AI spawn's settings and GOAL as packets: below) */
static int forge_ai_spawn_packet(int32_t live_idx, char *msg, size_t msg_size);
static int forge_ai_goal_packet(int32_t live_idx, char *msg, size_t msg_size);

extern unsigned long system_milliseconds(void);
extern void dispose_global_network_game_client(void);
extern void main_goto_main_menu(void);
extern void orig_network_objects_handle_changes(void const *entries, short count);
extern void *cache_files_tag_instances(long *count);

struct forge_net_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name; /* AE hook */
	XPTR(void) base_address; /* AE hook */
	unsigned long unused[2];
};

struct forge_chat_line_payload
{
	uint8_t player_index;
	uint8_t flags;
	uint8_t pad[2];
	char text[64];
};

struct forge_chat_packet
{
	struct distributed_message_header header;
	struct forge_chat_line_payload line;
};

struct forge_distributed_object_change
{
	byte change;
	byte flags;
	byte owner_player_index;
	byte forced_shader_permutation_index;
	long object_index;
	long definition_index;
	short owner_team_index;
	short variant_number;
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;
	real_vector3d translational_velocity;
	real_vector3d angular_velocity;
	real_rgb_color change_colors[4];
	byte region_permutations[8];
};

/* (by the engine's object index: every object the map can hold,
HALO_PORT_MAXIMUM_OBJECTS_PER_MAP. A map with many of its own puts placed
objects past 2048, which were not kept in step) */
#define FORGE_MAX_TRACKED_NET_OBJECTS 8192
#define FORGE_MAX_CLIENT_MACHINES     32

static unsigned long g_last_transform_send_ms = 0;
static unsigned long g_last_monitor_send_ms = 0;
static float g_last_monitor_sent_x = 0.0f;
static float g_last_monitor_sent_y = 0.0f;
static float g_last_monitor_sent_z = 0.0f;
static float g_last_monitor_sent_yaw = 0.0f;
static float g_last_monitor_sent_pitch = 0.0f;
static int g_client_requested_full_sync = 0;
static unsigned long g_client_join_ms = 0;

/* Maps absolute object index -> resolved local tag definition_index & Forge live_idx */
/* (about 1 MB: on the heap, FORGE_HEAP_STATIC, which the Android image has
no room for) */
struct forge_net_objects
{
	long def_index[FORGE_MAX_TRACKED_NET_OBJECTS];
	int32_t live_index[FORGE_MAX_TRACKED_NET_OBJECTS];
	uint8_t is_held[FORGE_MAX_TRACKED_NET_OBJECTS];
	uint8_t has_deferred_create[FORGE_MAX_TRACKED_NET_OBJECTS];
	struct forge_distributed_object_change deferred_create[FORGE_MAX_TRACKED_NET_OBJECTS];
};
FORGE_HEAP_STATIC(struct forge_net_objects, s_net_objects)
#define g_net_obj_def_index (FORGE_HEAP(s_net_objects).def_index)
#define g_net_obj_live_index (FORGE_HEAP(s_net_objects).live_index)
#define g_net_obj_is_held (FORGE_HEAP(s_net_objects).is_held)
#define g_net_obj_has_deferred_create (FORGE_HEAP(s_net_objects).has_deferred_create)
#define g_net_obj_deferred_create (FORGE_HEAP(s_net_objects).deferred_create)
/* (a change list is a wire short long: a slot for each) */
static uint8_t g_suppressed_change_slots[AE_FORGE_NET_CHANGES_MAX];

/* Host per-machine incremental full-sync cursor (-1 = idle, >= 0 = next live_idx to send) */
static int g_host_machine_known[FORGE_MAX_CLIENT_MACHINES];
static int g_host_machine_sync_cursor[FORGE_MAX_CLIENT_MACHINES];
static int g_host_machine_mon_synced[FORGE_MAX_CLIENT_MACHINES];

/* a machine's standing (new until it asks for the state), the identities of the session (the game's own for a machine: its address) with their rates,
budgets and the mark of being let go of, kept for the session so that a machine that leaves and comes back is the same player, who placed each instance
(identity + 1; 0 the host or the map), who holds it, when its slot was made and by whom */
static struct ae_forge_net_machine g_net_machine[FORGE_MAX_CLIENT_MACHINES];
static unsigned long g_net_identity_key[64];
static int g_net_identity_count;
static struct ae_forge_net_client g_net_identity_client[64];
/* when each identity was last seen, the rate and budget of the machines with no identity (the table full of players that cannot be let go of), when each
machine last sent a line (the senders the relay is shared among), and the streams waiting for the relay's budget (the latest line of each) */
static unsigned long g_net_identity_seen_ms[64];
static struct ae_forge_net_client g_net_identityless_client[FORGE_MAX_CLIENT_MACHINES];
static unsigned long g_net_machine_line_ms[FORGE_MAX_CLIENT_MACHINES];
static struct ae_forge_net_pending_table g_net_pending;
static uint8_t g_net_machine_viewer[FORGE_MAX_CLIENT_MACHINES];
static struct ae_forge_net_bucket g_net_feed_bucket[FORGE_MAX_CLIENT_MACHINES];
static uint8_t g_net_machine_fq_seen[FORGE_MAX_CLIENT_MACHINES];
static uint8_t g_net_instance_owner[FORGE_MAX_LIVE_INSTANCES];
static uint8_t g_net_held_by[FORGE_MAX_LIVE_INSTANCES];
static uint8_t g_net_instance_creator[FORGE_MAX_LIVE_INSTANCES];
static unsigned long g_net_instance_created_ms[FORGE_MAX_LIVE_INSTANCES];
static struct ae_forge_net_relay g_net_relay;
/* the log lines of what clients did: a few a second */
static struct ae_forge_net_bucket g_net_log_bucket;
/* a joiner's: the lines put by (its creates over the rate, the compare not done), the host's base tags (R1) and what came of the compare, when the state was
asked for, and a leave asked for */
static struct ae_forge_net_ring g_net_ring;
static struct ae_forge_net_bucket g_net_joiner_creates;
static int g_net_ring_overflow;
static int g_net_host_tags_known;
static unsigned int g_net_host_tags_count, g_net_host_tags_fingerprint;
static int g_net_compare_state;
static unsigned long g_net_state_asked_ms;
static char g_net_leave_message[160];

static void forge_net_leave_tick(void);
static void forge_net_instance_released(int32_t idx);
static void forge_net_machine_left(long machine_index);
static void forge_net_pending_tick(unsigned long now);
static int forge_net_team_kind(uint32_t group_tag);
static int forge_net_flag_type(uint32_t group_tag, char const *tag_path);
static int forge_net_level_ready(void);
static int forge_net_from_client(long machine_index, char const *text);
static uint8_t g_net_fq_pending[FORGE_MAX_CLIENT_MACHINES];
static void forge_net_joiner_tick(unsigned long now);
static unsigned int forge_base_tag_count(void);
static unsigned int forge_base_tag_fingerprint(void);

uint32_t forge_catalog_entry_hash(uint32_t group_tag, char const *tag_path)
{
	uint32_t h = 2166136261u;
	char const *p;

	h = (h ^ ((group_tag >> 24) & 0xFF)) * 16777619u;
	h = (h ^ ((group_tag >> 16) & 0xFF)) * 16777619u;
	h = (h ^ ((group_tag >> 8) & 0xFF)) * 16777619u;
	h = (h ^ (group_tag & 0xFF)) * 16777619u;

	if (tag_path)
	{
		for (p = tag_path; *p; p++)
		{
			unsigned char c = (unsigned char)*p;
			if (c == '/')
				c = '\\';
			else if (c >= 'A' && c <= 'Z')
				c = (unsigned char)(c - 'A' + 'a');
			h = (h ^ c) * 16777619u;
		}
	}
	return h ? h : 1u;
}

static struct forge_catalog_entry const *forge_find_catalog_entry_mode(uint32_t hash, int any_table_tag);

struct forge_catalog_entry const *forge_find_catalog_entry_by_hash(uint32_t hash)
{
	return forge_find_catalog_entry_mode(hash, 1);
}

/* (a client's spawn asks for the palette's tags alone: not any tag the level's table has, which a host's own lines may name) */
static struct forge_catalog_entry const *forge_find_catalog_entry_mode(uint32_t hash, int any_table_tag)
{
	static struct forge_catalog_entry s_fallback_entry;
	long tag_count = 0;
	struct forge_net_tag_instance const *instances;
	int i;

	if (!g_forge.initialized)
		forge_runtime_init();
	else if (g_forge.catalog_count == 0)
		forge_runtime_reload_catalogs();

	for (i = 0; i < g_forge.catalog_count; i++)
	{
		if (forge_catalog_entry_hash(g_forge.catalog[i].group_tag, g_forge.catalog[i].tag_path) == hash)
			return &g_forge.catalog[i];
	}
	/* Fallback 1: check active live instances (e.g. copied base-map objects or virtual markers) */
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		if (inst->active && inst->tag_path[0] &&
			forge_catalog_entry_hash(inst->group_tag, inst->tag_path) == hash)
		{
			memset(&s_fallback_entry, 0, sizeof(s_fallback_entry));
			s_fallback_entry.group_tag = inst->group_tag;
			snprintf(s_fallback_entry.tag_path, sizeof(s_fallback_entry.tag_path), "%s", inst->tag_path);
			s_fallback_entry.source_cache_id = inst->source_cache_id;
			return &s_fallback_entry;
		}
	}
	if (!any_table_tag)
		return NULL;
	/* Fallback 2: check all loaded map tag instances (e.g. map-local mp_warthog/ghost_mp when stock catalog deduplicated them) */
	instances = (struct forge_net_tag_instance const *)cache_files_tag_instances(&tag_count);
	if (instances && tag_count > 0)
	{
		long t;
		for (t = 0; t < tag_count; t++)
		{
			struct forge_net_tag_instance const *ti = &instances[t];
			uint32_t grp;
			if (ti->tag_index == NONE || !ti->name || !XBOX_POINTER(char const, ti->name)[0] || !ti->base_address) /* AE hook */
				continue;
			grp = (uint32_t)ti->group_tag;
			if (grp != 'scen' && grp != 'vehi' && grp != 'weap' &&
				grp != 'eqip' && grp != 'bipd' && grp != 'mach' &&
				grp != 'lifi' && grp != 'actv')
			{
				continue;
			}
			if (forge_catalog_entry_hash(grp, XBOX_POINTER(char const, ti->name)) == hash) /* AE hook */
			{
				memset(&s_fallback_entry, 0, sizeof(s_fallback_entry));
				s_fallback_entry.group_tag = grp;
				snprintf(s_fallback_entry.tag_path, sizeof(s_fallback_entry.tag_path), "%s", ti->name);
				s_fallback_entry.source_cache_id = 3;
				return &s_fallback_entry;
			}
		}
	}
	return NULL;
}

static void forge_send_packet_to_host(char const *text)
{
	struct forge_chat_packet pkt;
	if (!text || !text[0])
		return;
	memset(&pkt, 0, sizeof(pkt));
	pkt.line.player_index = 0;
	pkt.line.flags = 0;
	snprintf(pkt.line.text, sizeof(pkt.line.text), "%s", text);
	distributed_send(&pkt, _distributed_message_chat_up, 1, sizeof(pkt), _distributed_to_host_reliably);
}

static void forge_send_packet_to_machine(long machine_index, char const *text)
{
	struct forge_chat_packet pkt;
	if (!text || !text[0] || machine_index < 0)
		return;
	memset(&pkt, 0, sizeof(pkt));
	pkt.line.player_index = 0;
	pkt.line.flags = 0;
	snprintf(pkt.line.text, sizeof(pkt.line.text), "%s", text);
	distributed_send_to_machine_reliably(machine_index, &pkt, _distributed_message_chat_down, 1, sizeof(pkt));
}

static void forge_broadcast_packet_to_clients(char const *text, long exclude_machine_index)
{
	long machines[FORGE_MAX_CLIENT_MACHINES];
	short count, i;

	if (!text || !text[0])
		return;
	count = distributed_client_machines(machines, FORGE_MAX_CLIENT_MACHINES);
	for (i = 0; i < count; i++)
	{
		if (machines[i] != exclude_machine_index)
			forge_send_packet_to_machine(machines[i], text);
	}
}

/* a packet of Forge's from the host to one client, or to all of them (-1);
a client's goes to the host (forge_map_settings.c) */
void forge_network_send_text(long machine_index, char const *text)
{
	if (!forge_is_multiplayer_session())
		return;
	if (game_connection() == _game_connection_network_client)
		forge_send_packet_to_host(text);
	else if (game_connection() == _game_connection_network_server)
	{
		if (machine_index >= 0)
			forge_send_packet_to_machine(machine_index, text);
		else
			forge_broadcast_packet_to_clients(text, -1);
	}
}

void forge_network_reset_session(void)
{
	int i;
	g_last_transform_send_ms = 0;
	g_last_monitor_send_ms = 0;
	g_last_monitor_sent_x = 0.0f;
	g_last_monitor_sent_y = 0.0f;
	g_last_monitor_sent_z = 0.0f;
	g_last_monitor_sent_yaw = 0.0f;
	g_last_monitor_sent_pitch = 0.0f;
	g_client_requested_full_sync = 0;
	g_client_join_ms = system_milliseconds();
	{
		int machine;

		for (machine = 0; machine < FORGE_MAX_CLIENT_MACHINES; machine++)
			ae_forge_net_rules_machine_reset(&g_net_machine[machine]);
	}
	memset(g_net_identity_key, 0, sizeof(g_net_identity_key));
	g_net_identity_count = 0;
	memset(g_net_identity_client, 0, sizeof(g_net_identity_client));
	memset(g_net_identity_seen_ms, 0, sizeof(g_net_identity_seen_ms));
	memset(g_net_identityless_client, 0, sizeof(g_net_identityless_client));
	memset(g_net_machine_line_ms, 0, sizeof(g_net_machine_line_ms));
	memset(&g_net_pending, 0, sizeof(g_net_pending));
	memset(g_net_feed_bucket, 0, sizeof(g_net_feed_bucket));
	memset(g_net_machine_fq_seen, 0, sizeof(g_net_machine_fq_seen));
	memset(g_net_instance_owner, 0, sizeof(g_net_instance_owner));
	memset(g_net_held_by, 0, sizeof(g_net_held_by));
	memset(g_net_instance_creator, 0, sizeof(g_net_instance_creator));
	memset(g_net_instance_created_ms, 0, sizeof(g_net_instance_created_ms));
	memset(&g_net_relay, 0, sizeof(g_net_relay));
	memset(&g_net_log_bucket, 0, sizeof(g_net_log_bucket));
	memset(&g_net_joiner_creates, 0, sizeof(g_net_joiner_creates));
	ae_forge_net_rules_ring_reset(&g_net_ring);
	g_net_ring_overflow = 0;
	g_net_host_tags_known = 0;
	g_net_host_tags_count = g_net_host_tags_fingerprint = 0;
	g_net_compare_state = AE_FORGE_NET_COMPARE_WAIT;
	g_net_state_asked_ms = 0;
	g_net_leave_message[0] = 0;

	memset(g_forge.player_is_monitor, 0, sizeof(g_forge.player_is_monitor));
	memset(g_forge.player_has_monitor_pos, 0, sizeof(g_forge.player_has_monitor_pos));
	memset(g_forge.player_monitor_pos, 0, sizeof(g_forge.player_monitor_pos));
	memset(g_forge.player_monitor_yaw, 0, sizeof(g_forge.player_monitor_yaw));
	memset(g_forge.player_monitor_pitch, 0, sizeof(g_forge.player_monitor_pitch));
	memset(g_forge.player_has_render_pos, 0, sizeof(g_forge.player_has_render_pos));
	memset(g_forge.player_monitor_render_pos, 0, sizeof(g_forge.player_monitor_render_pos));
	memset(g_forge.player_monitor_render_yaw, 0, sizeof(g_forge.player_monitor_render_yaw));
	memset(g_forge.player_monitor_render_pitch, 0, sizeof(g_forge.player_monitor_render_pitch));

	for (i = 0; i < FORGE_MAX_TRACKED_NET_OBJECTS; i++)
	{
		g_net_obj_def_index[i] = NONE;
		g_net_obj_live_index[i] = -1;
		g_net_obj_is_held[i] = 0;
		g_net_obj_has_deferred_create[i] = 0;
	}
	for (i = 0; i < FORGE_MAX_CLIENT_MACHINES; i++)
	{
		g_host_machine_known[i] = 0;
		g_host_machine_sync_cursor[i] = -1;
		g_host_machine_mon_synced[i] = 0;
	}
}

static int forge_object_is_network_shared(long obj_idx)
{
	struct object_datum *obj;
	if (obj_idx == NONE)
		return 0;
	obj = (struct object_datum *)object_try_and_get_and_verify_type(obj_idx, (unsigned long)-1);
	if (!obj)
		return 0;
	return TEST_FLAG(_object_mask_unit | _object_mask_item, obj->object.type) ? 1 : 0;
}

static void forge_format_host_spawn_packet(
	char *out_buf,
	size_t out_size,
	int32_t live_idx,
	uint32_t group_tag,
	char const *tag_path,
	long obj_idx,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator)
{
	uint32_t hash = forge_catalog_entry_hash(group_tag, tag_path);
	unsigned int obj_enc = 0u;
	int yaw_cr = (int)(yaw * 100.0f);
	int pitch_cr = (int)(pitch * 100.0f);
	int roll_cr = (int)(roll * 100.0f);

	if (obj_idx != NONE && forge_object_is_network_shared(obj_idx))
	{
		struct object_datum *obj = (struct object_datum *)object_try_and_get_and_verify_type(obj_idx, (unsigned long)-1);
		if (obj)
		{
			unsigned int base_enc = (unsigned int)((DATUM_INDEX_TO_ABSOLUTE_INDEX(obj_idx) + 1) & 0xFFFu);
			unsigned int weap0_enc = 0u;
			if (TEST_FLAG(_object_mask_unit, obj->object.type))
			{
				struct unit_datum *unit = (struct unit_datum *)obj;
				if (unit->unit.weapon_object_indices[0] != NONE &&
					forge_object_is_network_shared(unit->unit.weapon_object_indices[0]))
				{
					weap0_enc = (unsigned int)((DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.weapon_object_indices[0]) + 1) & 0xFFFu);
				}
			}
			obj_enc = base_enc | (weap0_enc << 12);
		}
	}
	if (live_idx >= 0 && g_forge.grabbed_live_index == live_idx)
	{
		obj_enc |= (1u << 24);
	}

	/* (a far object's line, at two places, can be past the packet's 63
	letters, and cut short it is dropped: fewer places then, which the
	reader's %f takes alike) */
	{
		static char const *const s_formats[] = {
			"!FH:%X:%08X:%X:%.2f:%.2f:%.2f:%d:%d:%d:%d",
			"!FH:%X:%08X:%X:%.1f:%.1f:%.1f:%d:%d:%d:%d",
			"!FH:%X:%08X:%X:%.0f:%.0f:%.0f:%d:%d:%d:%d"
		};
		int f;

		for (f = 0; f < 3; f++)
		{
			int length = snprintf(
				out_buf,
				out_size,
				s_formats[f],
				(unsigned int)live_idx,
				(unsigned int)hash,
				obj_enc,
				x, y, z,
				yaw_cr, pitch_cr, roll_cr,
				(int)team_designator);

			if (length >= 0 && (size_t)length < out_size)
				break;
		}
	}
}

void forge_network_send_monitor_mode(int is_monitor, float x, float y, float z, float yaw, float pitch)
{
	char msg[64];
	int local_p_abs;
	int yaw_cr = (int)(yaw * 100.0f);
	int pitch_cr = (int)(pitch * 100.0f);

	if (!forge_is_multiplayer_session())
		return;

	local_p_abs = forge_get_local_player_abs_index();
	if (local_p_abs < 0 || local_p_abs >= 128)
		local_p_abs = 0;

	g_last_monitor_send_ms = system_milliseconds();
	g_last_monitor_sent_x = x;
	g_last_monitor_sent_y = y;
	g_last_monitor_sent_z = z;
	g_last_monitor_sent_yaw = yaw;
	g_last_monitor_sent_pitch = pitch;

	snprintf(
		msg,
		sizeof(msg),
		"!FT:%d:%d:%.2f:%.2f:%.2f:%d:%d",
		local_p_abs,
		is_monitor ? 1 : 0,
		x, y, z,
		yaw_cr, pitch_cr);

	if (game_connection() == _game_connection_network_client)
	{
		forge_send_packet_to_host(msg);
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_broadcast_packet_to_clients(msg, -1);
	}
}

/* a slot made now: its maker's alone for a moment, and no one's owner but the host's until a client's spawn says otherwise; the last occupant's marks go
(any place the host makes a slot calls it, a placed thing, an adopted one, an actor's marker) */
void forge_network_slot_made(int32_t live_idx)
{
	if (live_idx >= 0 && live_idx < FORGE_MAX_LIVE_INSTANCES && game_connection() == _game_connection_network_server)
	{
		g_net_instance_owner[live_idx] = 0;
		g_net_held_by[live_idx] = 0;
		g_net_instance_creator[live_idx] = 0;
		g_net_instance_created_ms[live_idx] = system_milliseconds() | 1ul;
	}
}

void forge_network_sync_tick(void)
{
	unsigned long now;

	if (!forge_is_multiplayer_session())
		return;

	forge_net_leave_tick();
	now = system_milliseconds();

	/* Broadcast local Monitor flight position/aiming at 20 Hz when moving/aiming, or 2.5 Hz when stationary */
	if (g_forge.mode == _forge_mode_3d_monitor)
	{
		float dx = g_forge.cam_x - g_last_monitor_sent_x;
		float dy = g_forge.cam_y - g_last_monitor_sent_y;
		float dz = g_forge.cam_z - g_last_monitor_sent_z;
		float dyaw = g_forge.cam_yaw - g_last_monitor_sent_yaw;
		float dpitch = g_forge.cam_pitch - g_last_monitor_sent_pitch;
		int moved = (dx * dx + dy * dy + dz * dz > 0.0004f) ||
					(dyaw * dyaw + dpitch * dpitch > 0.0001f);
		unsigned long min_interval = moved ? 50u : 400u;
		if (now - g_last_monitor_send_ms >= min_interval)
		{
			forge_network_send_monitor_mode(1, g_forge.cam_x, g_forge.cam_y, g_forge.cam_z, g_forge.cam_yaw, g_forge.cam_pitch);
		}
	}

	if (game_connection() == _game_connection_network_client)
	{
		forge_net_joiner_tick(now);
		if (!g_client_requested_full_sync && g_forge.monitor_cam_initialized && (now - g_client_join_ms >= 250))
		{
			g_client_requested_full_sync = 1;
			g_net_state_asked_ms = now ? now : 1;
			forge_send_packet_to_host("!FQ");
			if (g_forge.mode == _forge_mode_3d_monitor)
				forge_network_send_monitor_mode(1, g_forge.cam_x, g_forge.cam_y, g_forge.cam_z, g_forge.cam_yaw, g_forge.cam_pitch);
		}
	}
	else if (game_connection() == _game_connection_network_server)
	{
		long machines[FORGE_MAX_CLIENT_MACHINES];
		uint8_t active_mask[FORGE_MAX_CLIENT_MACHINES];
		short count, i;
		int m;

		memset(active_mask, 0, sizeof(active_mask));
		count = distributed_client_machines(machines, FORGE_MAX_CLIENT_MACHINES);
		for (i = 0; i < count; i++)
		{
			long midx = machines[i];
			if (midx >= 0 && midx < FORGE_MAX_CLIENT_MACHINES)
			{
				active_mask[midx] = 1;
				if (!g_host_machine_known[midx])
				{
					char tags[40];

					g_host_machine_known[midx] = 1;
					memset(&g_net_feed_bucket[midx], 0, sizeof(g_net_feed_bucket[midx]));
					g_host_machine_sync_cursor[midx] = 0;
					g_host_machine_mon_synced[midx] = 0;
					/* (the host's base tags before any other line: the joiner compares them before it makes a thing) */
					snprintf(tags, sizeof(tags), "!FB:%X:%X", forge_base_tag_count(), forge_base_tag_fingerprint());
					forge_send_packet_to_machine(midx, tags);
				}
			}
		}

		for (m = 0; m < FORGE_MAX_CLIENT_MACHINES; m++)
		{
			int packets_sent = 0, feed_lines;
			if (!active_mask[m])
			{
				if (g_host_machine_known[m] || g_net_machine[m].identity >= 0)
					forge_net_machine_left(m);
				g_host_machine_known[m] = 0;
				g_host_machine_sync_cursor[m] = -1;
				g_host_machine_mon_synced[m] = 0;
				continue;
			}

			/* Step 1: Sync all active Monitor players to newly joined client machine */
			if (!g_host_machine_mon_synced[m])
			{
				int p_abs;
				for (p_abs = 0; p_abs < 128; p_abs++)
				{
					if (g_forge.player_is_monitor[p_abs])
					{
						char msg[64];
						snprintf(
							msg,
							sizeof(msg),
							"!FT:%d:1:%.2f:%.2f:%.2f:%d:%d",
							p_abs,
							g_forge.player_monitor_pos[p_abs][0],
							g_forge.player_monitor_pos[p_abs][1],
							g_forge.player_monitor_pos[p_abs][2],
							(int)(g_forge.player_monitor_yaw[p_abs] * 100.0f),
							(int)(g_forge.player_monitor_pitch[p_abs] * 100.0f));
						forge_send_packet_to_machine(m, msg);
						packets_sent++;
					}
				}
				g_host_machine_mon_synced[m] = 1;
			}

			/* a request for the state that came before the level was ready is answered now */
			if (g_net_fq_pending[m] && forge_net_level_ready())
			{
				g_net_fq_pending[m] = 0;
				forge_net_from_client(m, "!FQ");
			}

			/* Step 2: the feed of the live instances to a newly joined machine: so many lines a second whatever the frame rate (150, the joiner takes 180) */
			feed_lines = ae_forge_net_rules_feed_allowance(&g_net_feed_bucket[m], now);
			packets_sent = 0;
			while (g_host_machine_sync_cursor[m] >= 0 &&
				   g_host_machine_sync_cursor[m] < g_forge.live_instance_count &&
				   packets_sent < feed_lines)
			{
				int32_t idx = g_host_machine_sync_cursor[m]++;
				struct forge_live_instance const *inst = &g_forge.live_instances[idx];
				char msg[64];

				if (!inst->active)
				{
					if (inst->is_default_scenario_object)
					{
						snprintf(msg, sizeof(msg), "!FD:%d:-1", (int)idx);
						forge_send_packet_to_machine(m, msg);
						packets_sent++;
					}
				}
				/* (a map's own volume: each machine adopted it, so where it is now) */
				else if (forge_volume_get(idx) && forge_volume_get(idx)->map_index >= 0)
				{
					snprintf(msg, sizeof(msg), "!FM:%d:-1:%.2f:%.2f:%.2f:%d:%d:%d:0", (int)idx, inst->x, inst->y, inst->z,
						(int)(inst->yaw * 100.0f), (int)(inst->pitch * 100.0f), (int)(inst->roll * 100.0f));
					forge_send_packet_to_machine(m, msg);
					packets_sent++;
				}
				else if (!inst->is_default_scenario_object)
				{
					int tele_usage = forge_3d_teleporter_net_usage(idx);

					/* (a teleporter's scenery: its role and channel as its team) */
					forge_format_host_spawn_packet(
						msg,
						sizeof(msg),
						idx,
						inst->group_tag,
						inst->tag_path,
						inst->object_index,
						inst->x, inst->y, inst->z,
						inst->yaw, inst->pitch, inst->roll,
						(tele_usage >= 0 && inst->group_tag != 'tele') ? (int16_t)tele_usage : inst->team_designator);
					forge_send_packet_to_machine(m, msg);
					packets_sent++;
					/* (and its SCALE: the joiner makes the scenery itself) */
					if (inst->scale > 0.0f)
					{
						snprintf(msg, sizeof(msg), "!FZ:%d:%d", (int)idx, (int)(inst->scale * 1000.0f + 0.5f));
						forge_send_packet_to_machine(m, msg);
						packets_sent++;
					}
				}
				else if (inst->edit_entry_index >= 0)
				{
					int obj_abs = (inst->object_index != NONE && forge_object_is_network_shared(inst->object_index))
									  ? (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(inst->object_index)
									  : -1;
					int is_held = (g_forge.grabbed_live_index == idx) ? 1 : 0;
					snprintf(
						msg,
						sizeof(msg),
						"!FM:%d:%d:%.2f:%.2f:%.2f:%d:%d:%d:%d",
						(int)idx,
						obj_abs,
						inst->x, inst->y, inst->z,
						(int)(inst->yaw * 100.0f),
						(int)(inst->pitch * 100.0f),
						(int)(inst->roll * 100.0f),
						is_held);
					forge_send_packet_to_machine(m, msg);
					packets_sent++;
				}
				/* (an AI spawn's settings and GOAL) */
				if (inst->active && inst->group_tag == FORGE_GROUP_AI_SPAWN)
				{
					if (forge_ai_spawn_packet(idx, msg, sizeof(msg)))
					{
						forge_send_packet_to_machine(m, msg);
						packets_sent++;
					}
					if (forge_ai_goal_packet(idx, msg, sizeof(msg)))
					{
						forge_send_packet_to_machine(m, msg);
						packets_sent++;
					}
				}
				/* (a volume's shape, size and effect; an emitter's DEAL DAMAGE off) */
				if (inst->active && forge_volume_packet(idx, msg, sizeof(msg)))
				{
					forge_send_packet_to_machine(m, msg);
					packets_sent++;
				}
				if (inst->active && inst->no_damage)
				{
					snprintf(msg, sizeof(msg), "!FN:%d:1", (int)idx);
					forge_send_packet_to_machine(m, msg);
					packets_sent++;
				}
				/* (a scenery's COLLISION off) */
				if (inst->active && inst->no_collision)
				{
					snprintf(msg, sizeof(msg), "!FK:%d:1", (int)idx);
					forge_send_packet_to_machine(m, msg);
					packets_sent++;
				}
				/* (a map's own teleporter, as it is set now) */
				if (inst->active && inst->is_default_scenario_object && forge_3d_teleporter_net_usage(idx) >= 0)
				{
					snprintf(msg, sizeof(msg), "!FU:%d:%d", (int)idx, forge_3d_teleporter_net_usage(idx));
					forge_send_packet_to_machine(m, msg);
					packets_sent++;
				}
			}

			ae_forge_net_rules_feed_spent(&g_net_feed_bucket[m], packets_sent);
			if (g_host_machine_sync_cursor[m] >= g_forge.live_instance_count)
				g_host_machine_sync_cursor[m] = -1;
		}
		forge_net_pending_tick(now);
	}
}

void forge_network_send_spawn_ext(
	int32_t live_idx,
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	long host_obj_idx,
	long host_def_idx)
{
	char msg[64];
	uint32_t hash;

	(void)source_cache_id;
	if (!forge_is_multiplayer_session() || !tag_path || !tag_path[0])
		return;

	hash = forge_catalog_entry_hash(group_tag, tag_path);

	forge_network_slot_made(live_idx);

	if (host_obj_idx != NONE && forge_object_is_network_shared(host_obj_idx))
	{
		int obj_abs = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(host_obj_idx);
		if (obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
		{
			if (host_def_idx != NONE)
				g_net_obj_def_index[obj_abs] = host_def_idx;
			if (live_idx >= 0)
				g_net_obj_live_index[obj_abs] = live_idx;
		}
	}

	if (game_connection() == _game_connection_network_client)
	{
		snprintf(
			msg,
			sizeof(msg),
			"!FS:%08X:%.2f:%.2f:%.2f:%d:%d:%d:%d",
			(unsigned int)hash,
			x, y, z,
			(int)(yaw * 100.0f),
			(int)(pitch * 100.0f),
			(int)(roll * 100.0f),
			(int)team_designator);
		forge_send_packet_to_host(msg);
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_format_host_spawn_packet(
			msg,
			sizeof(msg),
			live_idx,
			group_tag,
			tag_path,
			host_obj_idx,
			x, y, z,
			yaw, pitch, roll,
			team_designator);
		forge_broadcast_packet_to_clients(msg, -1);
	}
}

void forge_network_send_transform_ext(
	int32_t live_idx,
	long obj_idx,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int is_held,
	int immediate)
{
	unsigned long now;
	char msg[64];
	int obj_abs;

	if (!forge_is_multiplayer_session())
		return;

	now = system_milliseconds();
	if (!immediate && (now - g_last_transform_send_ms < 50))
		return;
	g_last_transform_send_ms = now;

	obj_abs = (obj_idx != NONE && forge_object_is_network_shared(obj_idx))
				  ? (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(obj_idx)
				  : -1;
	if (obj_abs < 0 && obj_idx == NONE && live_idx >= 0 && live_idx < g_forge.live_instance_count)
	{
		uint32_t grp = g_forge.live_instances[live_idx].group_tag;
		if (grp == 'vehi' || grp == 'weap' || grp == 'eqip' || grp == 'bipd' || grp == 'actv')
		{
			int k;
			for (k = 0; k < FORGE_MAX_TRACKED_NET_OBJECTS; k++)
			{
				if (g_net_obj_live_index[k] == live_idx)
				{
					obj_abs = k;
					break;
				}
			}
		}
	}
	if (obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
	{
		g_net_obj_is_held[obj_abs] = is_held ? 1 : 0;
	}

	snprintf(
		msg,
		sizeof(msg),
		"!FM:%d:%d:%.2f:%.2f:%.2f:%d:%d:%d:%d",
		(int)live_idx,
		obj_abs,
		x, y, z,
		(int)(yaw * 100.0f),
		(int)(pitch * 100.0f),
		(int)(roll * 100.0f),
		is_held ? 1 : 0);

	if (game_connection() == _game_connection_network_client)
	{
		forge_send_packet_to_host(msg);
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_broadcast_packet_to_clients(msg, -1);
	}
}

void forge_network_send_delete_ext(int32_t live_idx, long obj_idx)
{
	char msg[64];
	int obj_abs;
	int k;

	if (!forge_is_multiplayer_session())
		return;

	/* (a host's own deletion gives back what a client placed there) */
	if (game_connection() == _game_connection_network_server)
		forge_net_instance_released(live_idx);
	obj_abs = (obj_idx != NONE && forge_object_is_network_shared(obj_idx))
				  ? (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(obj_idx)
				  : -1;
	for (k = 0; k < FORGE_MAX_TRACKED_NET_OBJECTS; k++)
	{
		if (g_net_obj_live_index[k] == live_idx)
		{
			if (obj_abs < 0)
				obj_abs = k;
			g_net_obj_live_index[k] = -1;
			g_net_obj_is_held[k] = 0;
		}
	}
	if (obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
	{
		g_net_obj_live_index[obj_abs] = -1;
		g_net_obj_is_held[obj_abs] = 0;
	}
	snprintf(msg, sizeof(msg), "!FD:%d:%d", (int)live_idx, obj_abs);

	if (game_connection() == _game_connection_network_client)
	{
		forge_send_packet_to_host(msg);
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_broadcast_packet_to_clients(msg, -1);
	}
}

void forge_network_send_marker_usage(int32_t live_idx, int16_t usage)
{
	char msg[64];

	if (!forge_is_multiplayer_session())
		return;

	snprintf(msg, sizeof(msg), "!FU:%d:%d", (int)live_idx, (int)usage);
	if (game_connection() == _game_connection_network_client)
	{
		forge_send_packet_to_host(msg);
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_broadcast_packet_to_clients(msg, -1);
	}
}

void forge_network_send_always_spawn(int32_t live_idx, int on)
{
	char msg[64];

	if (!forge_is_multiplayer_session())
		return;

	snprintf(msg, sizeof(msg), "!FV:%d:%d", (int)live_idx, on ? 1 : 0);
	if (game_connection() == _game_connection_network_client)
	{
		forge_send_packet_to_host(msg);
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_broadcast_packet_to_clients(msg, -1);
	}
}

/* a placed scenery's or effect's SCALE, in thousandths */
void forge_network_send_scale(int32_t live_idx, float scale)
{
	char msg[64];

	if (!forge_is_multiplayer_session())
		return;

	snprintf(msg, sizeof(msg), "!FZ:%d:%d", (int)live_idx, (int)(scale * 1000.0f + 0.5f));
	if (game_connection() == _game_connection_network_client)
	{
		forge_send_packet_to_host(msg);
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_broadcast_packet_to_clients(msg, -1);
	}
}

static int32_t forge_resolve_target_live_idx(int live_idx, int obj_abs);

static void forge_network_send_text_both_ways(char const *msg)
{
	if (!forge_is_multiplayer_session())
		return;
	if (game_connection() == _game_connection_network_client)
		forge_send_packet_to_host(msg);
	else if (game_connection() == _game_connection_network_server)
		forge_broadcast_packet_to_clients(msg, -1);
}

void forge_network_send_no_damage(int32_t live_idx, int no_damage)
{
	char msg[64];

	snprintf(msg, sizeof(msg), "!FN:%d:%d", (int)live_idx, no_damage ? 1 : 0);
	forge_network_send_text_both_ways(msg);
}

void forge_network_send_no_collision(int32_t live_idx, int no_collision)
{
	char msg[64];

	snprintf(msg, sizeof(msg), "!FK:%d:%d", (int)live_idx, no_collision ? 1 : 0);
	forge_network_send_text_both_ways(msg);
}

void forge_network_send_volume(int32_t live_idx)
{
	char msg[64];

	if (forge_volume_packet(live_idx, msg, sizeof(msg)))
		forge_network_send_text_both_ways(msg);
}

/* a !FW: packet's fields read and applied (TRUE when it is one) */
/* an AI spawn's settings: everything but where it stands, which !FM: carries.
(the chat line's 64 bytes hold it, the actor and weapon by their hashes) */
static int forge_ai_spawn_packet(int32_t live_idx, char *msg, size_t msg_size)
{
	struct forge_ai_spawn_entry const *entry = NULL;
	uint32_t variant_hash = 0, weapon_hash = 0;

	forge_ai_spawn_net_settings(live_idx, &variant_hash, &weapon_hash, &entry);
	if (!entry)
		return 0;
	snprintf(msg, msg_size, "!FA:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X",
		(unsigned int)live_idx,
		(unsigned int)variant_hash,
		(unsigned int)weapon_hash,
		(unsigned int)entry->team & 0xFFFFu,
		(unsigned int)entry->unit_count,
		(unsigned int)entry->respawn_alive,
		(unsigned int)entry->respawn_delay_seconds,
		(unsigned int)entry->respawn_total,
		(unsigned int)entry->gametype_flags,
		(unsigned int)entry->detached,
		(unsigned int)forge_ai_spawn_effect_packed(entry));
	return 1;
}

/* an AI spawn's GOAL (forge_ai_spawn_entry.goal*): a packet of its own, so a
peer of before reads its !FA: as ever */
static int forge_ai_goal_packet(int32_t live_idx, char *msg, size_t msg_size)
{
	struct forge_ai_spawn_entry const *entry = NULL;
	uint32_t variant_hash = 0, weapon_hash = 0;

	forge_ai_spawn_net_settings(live_idx, &variant_hash, &weapon_hash, &entry);
	if (!entry)
		return 0;
	/* (ALERT TIME last: a peer of before reads the six it knows) */
	snprintf(msg, msg_size, "!FJ:%X:%X:%X:%X:%X:%X:%X", (unsigned int)live_idx, (unsigned int)entry->goal,
		(unsigned int)entry->goal_link, (unsigned int)entry->goal_who, (unsigned int)entry->goal_start,
		(unsigned int)entry->goal_follow, (unsigned int)entry->goal_alert);
	return 1;
}

/* (the host's: the AI is its own, so only a unit's appearing has to be told) */
void forge_network_send_spawn_effect(int effect, union real_point3d const *point)
{
	char msg[64];

	if (game_connection() != _game_connection_network_server)
		return;
	snprintf(msg, sizeof(msg), "!FE:%X:%.2f:%.2f:%.2f", (unsigned int)effect, point->x, point->y, point->z);
	forge_broadcast_packet_to_clients(msg, -1);
}

void forge_network_send_ai_spawn(int32_t live_idx)
{
	char msg[64];
	char goal[64];

	if (!forge_is_multiplayer_session() || !forge_ai_spawn_packet(live_idx, msg, sizeof(msg)) ||
		!forge_ai_goal_packet(live_idx, goal, sizeof(goal)))
	{
		return;
	}
	if (game_connection() == _game_connection_network_client)
	{
		/* (one the host has not answered for yet says so once it has) */
		if ((g_forge.live_instances[live_idx].pad_inst & 0x80u) == 0)
		{
			forge_send_packet_to_host(msg);
			forge_send_packet_to_host(goal);
		}
	}
	else if (game_connection() == _game_connection_network_server)
	{
		forge_broadcast_packet_to_clients(msg, -1);
		forge_broadcast_packet_to_clients(goal, -1);
	}
}

static long forge_find_live_object_by_abs_index(int obj_abs)
{
	if (obj_abs < 0 || !object_header_data)
		return NONE;
	if (obj_abs < object_header_data->count)
	{
		long cand;
		for (cand = data_next_index(object_header_data, NONE);
			 cand != NONE;
			 cand = data_next_index(object_header_data, cand))
		{
			if ((int)DATUM_INDEX_TO_ABSOLUTE_INDEX(cand) == obj_abs)
				return cand;
		}
	}
	return NONE;
}

static int32_t forge_resolve_target_live_idx(int live_idx, int obj_abs)
{
	if (g_forge.live_instance_count == 0)
		forge_init_map_scenario_entities();

	if (obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
	{
		int32_t mapped_idx = g_net_obj_live_index[obj_abs];
		if (mapped_idx >= 0 && mapped_idx < g_forge.live_instance_count && g_forge.live_instances[mapped_idx].active)
			return mapped_idx;
	}
	if (live_idx >= 0 && live_idx < g_forge.live_instance_count && g_forge.live_instances[live_idx].active)
		return live_idx;
	if (obj_abs >= 0)
	{
		long obj_idx = forge_find_live_object_by_abs_index(obj_abs);
		if (obj_idx != NONE && forge_object_is_network_shared(obj_idx))
		{
			int32_t adopted_idx = forge_3d_adopt_or_find_object_instance(obj_idx);
			if (adopted_idx >= 0)
			{
				if (live_idx >= 0 && live_idx < FORGE_MAX_LIVE_INSTANCES && adopted_idx != live_idx &&
					(live_idx >= g_forge.live_instance_count || !g_forge.live_instances[live_idx].active))
				{
					if (live_idx >= g_forge.live_instance_count)
					{
						memset(
							&g_forge.live_instances[g_forge.live_instance_count],
							0,
							(size_t)(live_idx - g_forge.live_instance_count + 1) * sizeof(struct forge_live_instance));
						g_forge.live_instance_count = live_idx + 1;
					}
					g_forge.live_instances[live_idx] = g_forge.live_instances[adopted_idx];
					memset(&g_forge.live_instances[adopted_idx], 0, sizeof(struct forge_live_instance));
					g_forge.live_instances[adopted_idx].object_index = NONE;
					g_forge.live_instances[adopted_idx].actor_index = NONE;
					g_forge.live_instances[adopted_idx].edit_entry_index = -1;
					adopted_idx = live_idx;
				}
				if (obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
					g_net_obj_live_index[obj_abs] = adopted_idx;
				return adopted_idx;
			}
		}
	}
	if (live_idx >= 0 && live_idx < g_forge.live_instance_count)
		return live_idx;
	return -1;
}

/* Firefight markers look alike, and a dropship route's points are told apart by
their usage alone: a host's spawn links only to the one it was asked for */
static int forge_spawn_usage_matches(struct forge_live_instance const *cand, int16_t team_designator)
{
	return cand->group_tag != 'ffsp' || cand->team_designator == team_designator;
}

static void forge_client_configure_linked_object(int32_t live_idx, long obj_idx, int is_held)
{
	struct object_datum *obj;

	if (obj_idx == NONE)
		return;

	object_set_automatic_deactivation(obj_idx, FALSE);
	object_activate(obj_idx);

	obj = object_get(obj_idx);
	if (obj)
	{
		SET_FLAG(obj->object.flags, _object_no_collisions_bit, is_held ? TRUE : FALSE);
		if (TEST_FLAG(_object_mask_unit, obj->object.type))
		{
			if (is_held)
				SET_FLAG(obj->object.flags, _object_at_rest_bit, TRUE);
			else
				SET_FLAG(obj->object.flags, _object_at_rest_bit, FALSE);
			unit_set_enterable_by_player(obj_idx, TRUE);
			unit_set_controllable(obj_idx, TRUE);
		}
		else if (!is_held)
		{
			SET_FLAG(obj->object.flags, _object_at_rest_bit, FALSE);
		}
	}

	if (live_idx >= 0 && live_idx < g_forge.live_instance_count && g_forge.live_instances[live_idx].active)
	{
		g_forge.live_instances[live_idx].object_index = obj_idx;
		if (g_forge.grabbed_live_index == live_idx)
			g_forge.grabbed_object_index = obj_idx;
		if (g_forge.live_instances[live_idx].group_tag == FORGE_GROUP_AI_SPAWN)
			forge_ai_preview_protect(obj_idx);
	}
}

static void forge_client_handle_host_spawn(
	int host_live_idx,
	uint32_t hash,
	int obj_abs,
	int weap0_abs,
	int host_is_held,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator)
{
	struct forge_catalog_entry const *ce = forge_find_catalog_entry_by_hash(hash);
	uint32_t actual_grp;
	long def_idx = NONE;
	long weap0_def_idx = NONE;
	int32_t match_idx = -1;
	int matched_client_unlinked = 0;
	int matched_existing = 0;
	/* (a spawn this machine placed: the host has only its defaults) */
	int link_ai_settings = 0;
	int is_net_obj;
	int i;

	if (!ce)
	{
		platform_log("Forge: the host placed a tag this machine has not got (hash %08X): it is not shown",
			(unsigned int)hash);
		return;
	}
	/* (an AI spawn's team is one of forge_ai.c's: the host's line is no engine index of this machine's) */
	if (!ae_forge_net_rules_instance_team_ok(forge_net_team_kind(ce->group_tag), forge_net_flag_type(ce->group_tag, ce->tag_path), team_designator))
		return;

	/* Ensure default scenario entities (spawns, flags, teleporters) are initialized at indices 0..N-1
	 * before inserting any host-replicated objects, even if !FH: arrives on Frame 0 before forge_3d_update()! */
	if (g_forge.live_instance_count == 0)
		forge_init_map_scenario_entities();

	actual_grp = ce->group_tag;
	if (ce->group_tag != 'spwn' && ce->group_tag != 'tele' && ce->group_tag != 'flag' && ce->group_tag != 'ffsp' &&
		ce->group_tag != FORGE_GROUP_AI_SPAWN)
	{
		/* Fault in external Xbox campaign tags or .ocetags packages on the client before object creation! */
		def_idx = forge_resolve_engine_tag(ce->group_tag, ce->tag_path, &actual_grp);
	}

	/* Only units and items share lower-1024 obj_abs indices across the network (network_objects.c).
	 * Non-networked objects (scen, mach, lifi) are spawned locally on each machine at independent indices. */
	is_net_obj = (actual_grp == 'vehi' || actual_grp == 'weap' || actual_grp == 'eqip' ||
				  actual_grp == 'bipd' || ce->group_tag == 'actv');
	if (!is_net_obj)
	{
		obj_abs = -1;
		weap0_abs = -1;
	}

	if (is_net_obj && obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
	{
		if (def_idx != NONE)
			g_net_obj_def_index[obj_abs] = def_idx;
		g_net_obj_is_held[obj_abs] = host_is_held ? 1 : 0;
	}

	if (is_net_obj && weap0_abs >= 0 && weap0_abs < FORGE_MAX_TRACKED_NET_OBJECTS && def_idx != NONE && actual_grp == 'vehi')
	{
		struct unit_definition const *udef = (struct unit_definition const *)tag_get('unit', def_idx);
		if (udef && udef->unit.initial_weapons.count > 0 && udef->unit.initial_weapons.address)
		{
			struct tag_reference const *iw_ref = XBOX_POINTER(struct tag_reference const, udef->unit.initial_weapons.address); /* AE hook */
			if (iw_ref->index != NONE)
			{
				weap0_def_idx = iw_ref->index;
				g_net_obj_def_index[weap0_abs] = weap0_def_idx;
			}
		}
	}

	/* 1. If the Client is currently holding a locally-created unlinked instance (pad_inst & 0x80) of this tag, match it first */
	if (g_forge.grabbed_live_index >= 0 && g_forge.grabbed_live_index < g_forge.live_instance_count)
	{
		struct forge_live_instance const *cand = &g_forge.live_instances[g_forge.grabbed_live_index];
		if (cand->active && !cand->is_default_scenario_object &&
			(cand->pad_inst & 0x80u) != 0 &&
			cand->group_tag == ce->group_tag && _stricmp(cand->tag_path, ce->tag_path) == 0 &&
			forge_spawn_usage_matches(cand, team_designator))
		{
			match_idx = g_forge.grabbed_live_index;
			matched_client_unlinked = 1;
		}
	}

	/* 2. Check if host_live_idx points to a locally-created unlinked instance of this tag */
	if (match_idx < 0 && host_live_idx >= 0 && host_live_idx < g_forge.live_instance_count)
	{
		struct forge_live_instance const *cand = &g_forge.live_instances[host_live_idx];
		if (cand->active && !cand->is_default_scenario_object &&
			(cand->pad_inst & 0x80u) != 0 &&
			cand->group_tag == ce->group_tag && _stricmp(cand->tag_path, ce->tag_path) == 0 &&
			forge_spawn_usage_matches(cand, team_designator))
		{
			match_idx = host_live_idx;
			matched_client_unlinked = 1;
		}
	}

	/* 3. Scan for any locally-created unlinked Client instance (pad_inst & 0x80) of this tag */
	if (match_idx < 0)
	{
		for (i = g_forge.live_instance_count - 1; i >= 0; i--)
		{
			struct forge_live_instance const *cand = &g_forge.live_instances[i];
			if (!cand->active || cand->is_default_scenario_object ||
				(cand->pad_inst & 0x80u) == 0)
				continue;
			if (cand->group_tag == ce->group_tag && _stricmp(cand->tag_path, ce->tag_path) == 0 &&
				forge_spawn_usage_matches(cand, team_designator))
			{
				match_idx = i;
				matched_client_unlinked = 1;
				break;
			}
		}
	}

	if (matched_client_unlinked && match_idx >= 0 && match_idx < g_forge.live_instance_count)
	{
		g_forge.live_instances[match_idx].pad_inst &= (uint8_t)~0x80u;
		link_ai_settings = g_forge.live_instances[match_idx].group_tag == FORGE_GROUP_AI_SPAWN;
		if (host_live_idx >= 0 && host_live_idx < FORGE_MAX_LIVE_INSTANCES && match_idx != host_live_idx)
		{
			if (host_live_idx >= g_forge.live_instance_count)
			{
				memset(
					&g_forge.live_instances[g_forge.live_instance_count],
					0,
					(size_t)(host_live_idx - g_forge.live_instance_count + 1) * sizeof(struct forge_live_instance));
				g_forge.live_instance_count = host_live_idx + 1;
			}
			if (!g_forge.live_instances[host_live_idx].active)
			{
				g_forge.live_instances[host_live_idx] = g_forge.live_instances[match_idx];
				memset(&g_forge.live_instances[match_idx], 0, sizeof(struct forge_live_instance));
				g_forge.live_instances[match_idx].object_index = NONE;
				g_forge.live_instances[match_idx].actor_index = NONE;
				g_forge.live_instances[match_idx].edit_entry_index = -1;
				if (g_forge.grabbed_live_index == match_idx)
					g_forge.grabbed_live_index = host_live_idx;
				if (g_forge.hovered_live_index == match_idx)
					g_forge.hovered_live_index = host_live_idx;
				forge_3d_history_remap(match_idx, host_live_idx);
				match_idx = host_live_idx;
			}
		}
		if (link_ai_settings)
			forge_network_send_ai_spawn(match_idx);
	}

	/* 4. Check if already linked to obj_abs, at host_live_idx, or at the exact position */
	if (match_idx < 0)
	{
		long existing_obj = is_net_obj ? forge_find_live_object_by_abs_index(obj_abs) : NONE;
		if (is_net_obj && obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
		{
			int32_t mapped = g_net_obj_live_index[obj_abs];
			if (mapped >= 0 && mapped < g_forge.live_instance_count)
			{
				struct forge_live_instance const *cand = &g_forge.live_instances[mapped];
				if (cand->active && !cand->is_default_scenario_object &&
					cand->group_tag == ce->group_tag && _stricmp(cand->tag_path, ce->tag_path) == 0)
				{
					match_idx = mapped;
					matched_existing = 1;
				}
			}
		}
		if (match_idx < 0 && existing_obj != NONE)
		{
			for (i = g_forge.live_instance_count - 1; i >= 0; i--)
			{
				struct forge_live_instance const *cand = &g_forge.live_instances[i];
				if (cand->active && !cand->is_default_scenario_object && cand->object_index == existing_obj)
				{
					match_idx = i;
					matched_existing = 1;
					break;
				}
			}
		}
		if (match_idx < 0 && host_live_idx >= 0 && host_live_idx < g_forge.live_instance_count)
		{
			struct forge_live_instance const *cand = &g_forge.live_instances[host_live_idx];
			if (cand->active && !cand->is_default_scenario_object &&
				cand->group_tag == ce->group_tag && _stricmp(cand->tag_path, ce->tag_path) == 0)
			{
				match_idx = host_live_idx;
				matched_existing = 1;
			}
		}
		if (match_idx < 0)
		{
			for (i = g_forge.live_instance_count - 1; i >= 0; i--)
			{
				struct forge_live_instance const *cand = &g_forge.live_instances[i];
				float dx, dy, dz;
				if (!cand->active || cand->is_default_scenario_object)
					continue;
				if (is_net_obj && cand->object_index != NONE)
					continue;
				if (cand->group_tag != ce->group_tag || _stricmp(cand->tag_path, ce->tag_path) != 0)
					continue;
				dx = cand->x - x;
				dy = cand->y - y;
				dz = cand->z - z;
				if (dx * dx + dy * dy + dz * dz < 0.04f)
				{
					match_idx = i;
					matched_existing = 1;
					break;
				}
			}
		}
	}

	if (match_idx < 0)
	{
		match_idx = forge_3d_spawn_object_at_slot(
			host_live_idx,
			ce->group_tag,
			ce->tag_path,
			ce->source_cache_id,
			x, y, z,
			yaw, pitch, roll,
			team_designator,
			0);
	}
	else if (matched_existing && match_idx != g_forge.grabbed_live_index)
	{
		forge_3d_move_instance_ext(
			match_idx,
			x, y, z,
			yaw, pitch, roll,
			host_is_held,
			0);
	}

	if (is_net_obj && obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
	{
		long existing_obj;
		int effective_held = host_is_held;

		if (match_idx >= 0)
		{
			int k;
			for (k = 0; k < FORGE_MAX_TRACKED_NET_OBJECTS; k++)
			{
				if (g_net_obj_live_index[k] == match_idx)
					g_net_obj_live_index[k] = -1;
			}
			g_net_obj_live_index[obj_abs] = match_idx;
			if (matched_client_unlinked)
			{
				effective_held = (g_forge.grabbed_live_index == match_idx) ? 1 : 0;
				g_net_obj_is_held[obj_abs] = (uint8_t)effective_held;
			}
		}

		/* If _distributed_message_object_changes arrived BEFORE !FH:, replay deferred creation now! */
		existing_obj = forge_find_live_object_by_abs_index(obj_abs);
		if (existing_obj == NONE && g_net_obj_has_deferred_create[obj_abs] && def_idx != NONE)
		{
			struct forge_distributed_object_change re_change = g_net_obj_deferred_create[obj_abs];
			g_net_obj_has_deferred_create[obj_abs] = 0;
			re_change.definition_index = def_idx;
			orig_network_objects_handle_changes(&re_change, 1);
			existing_obj = forge_find_live_object_by_abs_index(obj_abs);
		}

		if (weap0_abs >= 0 && weap0_abs < FORGE_MAX_TRACKED_NET_OBJECTS &&
			forge_find_live_object_by_abs_index(weap0_abs) == NONE &&
			g_net_obj_has_deferred_create[weap0_abs] && weap0_def_idx != NONE)
		{
			struct forge_distributed_object_change re_weap = g_net_obj_deferred_create[weap0_abs];
			g_net_obj_has_deferred_create[weap0_abs] = 0;
			re_weap.definition_index = weap0_def_idx;
			orig_network_objects_handle_changes(&re_weap, 1);
		}

		if (existing_obj != NONE && match_idx >= 0 && match_idx < g_forge.live_instance_count)
		{
			forge_client_configure_linked_object(match_idx, existing_obj, effective_held);
			if (matched_client_unlinked)
			{
				struct forge_live_instance const *inst = &g_forge.live_instances[match_idx];
				forge_3d_move_instance_ext(
					match_idx,
					inst->x, inst->y, inst->z,
					inst->yaw, inst->pitch, inst->roll,
					effective_held,
					0);
				forge_network_send_transform_ext(
					match_idx,
					existing_obj,
					inst->x, inst->y, inst->z,
					inst->yaw, inst->pitch, inst->roll,
					effective_held,
					1);
			}
		}
	}
	else if (matched_client_unlinked && match_idx >= 0 && match_idx < g_forge.live_instance_count)
	{
		/* Non-networked object (scen, mach, lifi, ctrl, spwn, tele, flag, ffsp) spawned by Client and now linked to host_live_idx:
		 * immediately send Client's current held/placed transform so Host updates from initial spawn position! */
		struct forge_live_instance const *inst = &g_forge.live_instances[match_idx];
		int effective_held = (g_forge.grabbed_live_index == match_idx) ? 1 : 0;
		forge_network_send_transform_ext(
			match_idx,
			inst->object_index,
			inst->x, inst->y, inst->z,
			inst->yaw, inst->pitch, inst->roll,
			effective_held,
			1);
	}
}

/* ---------- what a peer's line may do (ae_forge_net_rules.h) */

extern int config_boolean(char const *name);

static void forge_net_limits(struct ae_forge_net_limits *limits)
{
	ae_forge_net_rules_limits(limits, FORGE_MAX_LIVE_INSTANCES, FORGE_MAX_TRACKED_NET_OBJECTS);
}

/* the tag table as the level's tags left it (the map's, the props', the online game's actors: forge_level_tags in randomizer.c, the same steps on
every machine, before Forge adds a tag of its own or the menus theirs): its count and the hash of each tag's group and name, taken once a level */
static unsigned int g_net_level_count, g_net_level_fingerprint;

void forge_network_note_level_tags(void)
{
	long tag_count = 0, t;
	struct forge_net_tag_instance const *instances = (struct forge_net_tag_instance const *)cache_files_tag_instances(&tag_count);
	unsigned int h = 2166136261u;

	for (t = 0; instances && t < tag_count; t++)
	{
		char const *name = instances[t].name ? XBOX_POINTER(char const, instances[t].name) : "";

		h = (h ^ (unsigned int)instances[t].group_tag) * 16777619u;
		for (; *name; name++)
			h = (h ^ (unsigned char)*name) * 16777619u;
	}
	g_net_level_count = instances ? (unsigned int)tag_count : 0;
	g_net_level_fingerprint = h;
}

static unsigned int forge_base_tag_count(void)
{
	return g_net_level_count;
}

static unsigned int forge_base_tag_fingerprint(void)
{
	return g_net_level_fingerprint;
}

static void forge_net_send_refusal(long machine_index, int reason)
{
	char msg[16];

	snprintf(msg, sizeof(msg), "!FX:%d", reason);
	if (machine_index >= 0)
		forge_send_packet_to_machine(machine_index, msg);
}

/* the object of the instance a client's line names, as the netcode shares it (-1: none) */
static int forge_net_instance_object_abs(int32_t idx)
{
	struct forge_live_instance const *inst = &g_forge.live_instances[idx];

	if (inst->object_index != NONE && forge_object_is_network_shared(inst->object_index))
		return (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(inst->object_index);
	return -1;
}

/* whether a vehicle has a rider in a seat */
static int forge_net_vehicle_occupied(long vehicle_index)
{
	struct object_iterator iterator;
	struct unit_datum *unit;

	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((unit = (struct unit_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (iterator.index != vehicle_index && unit->object.parent_object_index == vehicle_index && unit->unit.parent_seat_index != NONE)
			return 1;
	}
	return 0;
}

/* the modes a spawn serves, as the rules take them: one mode's bit, or 0 for all of them */
static int forge_net_spawn_modes(struct forge_live_instance const *inst)
{
	unsigned int flags = (unsigned int)inst->gametype_flags & 0x1Fu;

	return (flags == 0x1u || flags == 0x2u || flags == 0x4u || flags == 0x8u || flags == 0x10u) ? (int)flags : 0;
}

/* a spawn's team as it is placed (0 red, 1 blue, anything else the free-for-all spawn 2) */
static int forge_net_spawn_team(struct forge_live_instance const *inst)
{
	return (inst->team_designator == 0 || inst->team_designator == 1) ? inst->team_designator : 2;
}

/* the class a game needs one of that this instance is in (a spawn of its team, a flag of its type, a capture flag of its team too), and which modes the
instance and the others of its class serve: other than itself */
static void forge_net_class_masks(int32_t idx, int *own_mask, int *others_mask)
{
	struct forge_live_instance const *inst = &g_forge.live_instances[idx];
	int32_t each;

	*own_mask = *others_mask = 0;
	if (inst->group_tag != 'spwn' && inst->group_tag != 'flag')
		return;
	*own_mask = inst->group_tag == 'spwn' ? forge_net_spawn_modes(inst) : 1;
	for (each = 0; each < g_forge.live_instance_count; each++)
	{
		struct forge_live_instance const *other = &g_forge.live_instances[each];

		if (each == idx || !other->active || other->group_tag != inst->group_tag)
			continue;
		if (inst->group_tag == 'spwn')
		{
			if (forge_net_spawn_team(other) == forge_net_spawn_team(inst))
				*others_mask |= forge_net_spawn_modes(other) ? forge_net_spawn_modes(other) : AE_FORGE_NET_MODES_ALL;
		}
		else if (other->netgame_flag_type == inst->netgame_flag_type &&
			(inst->netgame_flag_type != 0 || other->team_designator == inst->team_designator))
			*others_mask |= 1;
	}
	if (*own_mask == 0)
		*own_mask = AE_FORGE_NET_MODES_ALL;
}

/* the kind of team an instance's group takes (ae_forge_net_rules.h), and a flag's type from its tag, as forge_3d_spawn_object_at reads it */
static int forge_net_team_kind(uint32_t group_tag)
{
	if (group_tag == 'spwn')
		return AE_FORGE_NET_TEAM_SPAWN;
	if (group_tag == 'flag')
		return AE_FORGE_NET_TEAM_FLAG;
	if (group_tag == FORGE_GROUP_AI_SPAWN || group_tag == 'actv')
		return AE_FORGE_NET_TEAM_AI;
	return AE_FORGE_NET_TEAM_OTHER;
}

static int forge_net_flag_type(uint32_t group_tag, char const *tag_path)
{
	if (group_tag != 'flag')
		return 0;
	return strstr(tag_path, "firefight_generator") ? FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR :
		strstr(tag_path, "oddball") ? 2 : strstr(tag_path, "koth") ? 8 : strstr(tag_path, "race") ? 3 : 0;
}

/* whether a point is inside the level's world box (the box of its structure) */
static int forge_net_in_level(float x, float y, float z)
{
	struct structure_bsp const *bsp = global_structure_bsp_get();

	if (!bsp)
		return 0;
	return x >= bsp->world_bounds.x0 && x <= bsp->world_bounds.x1 && y >= bsp->world_bounds.y0 && y <= bsp->world_bounds.y1 &&
		z >= bsp->world_bounds.z0 && z <= bsp->world_bounds.z1;
}

/* the facts the rules decide an edit by (ae_forge_net_rules.h) */
static void forge_net_facts(int32_t idx, struct ae_forge_net_facts *facts)
{
	struct forge_live_instance const *inst;

	memset(facts, 0, sizeof(*facts));
	facts->object_abs = -1;
	if (idx < 0 || idx >= g_forge.live_instance_count || idx >= FORGE_MAX_LIVE_INSTANCES || !g_forge.live_instances[idx].active)
		return;
	inst = &g_forge.live_instances[idx];
	facts->active = 1;
	facts->map_own = inst->is_default_scenario_object ? 1 : 0;
	facts->kind = inst->group_tag == 'spwn' ? AE_FORGE_NET_KIND_SPAWN : inst->group_tag == 'flag' ? AE_FORGE_NET_KIND_FLAG : AE_FORGE_NET_KIND_OTHER;
	facts->object_abs = forge_net_instance_object_abs(idx);
	if (inst->object_index != NONE)
	{
		struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(inst->object_index, (unsigned long)-1);

		if (object)
		{
			facts->parented = object->object.parent_object_index != NONE;
			if (TEST_FLAG(_object_mask_vehicle, object->object.type))
			{
				facts->kind = AE_FORGE_NET_KIND_VEHICLE;
				facts->occupied = forge_net_vehicle_occupied(inst->object_index);
			}
			else if (TEST_FLAG(_object_mask_item, object->object.type))
				facts->kind = AE_FORGE_NET_KIND_ITEM;
		}
	}
	facts->held_by = g_net_held_by[idx];
	facts->owner = g_net_instance_owner[idx];
	facts->creator = g_net_instance_creator[idx];
	facts->created_ms = g_net_instance_created_ms[idx];
	forge_net_class_masks(idx, &facts->class_own_mask, &facts->class_others_mask);
	facts->host_holds = g_forge.grabbed_live_index == idx;
	facts->dest_in_level = 1;
}

/* a log line of what a client did: so many a second */
static int forge_net_log_ok(void)
{
	return ae_forge_net_rules_bucket_take(&g_net_log_bucket, system_milliseconds(), 4.0, 8.0);
}

/* whether a machine of the session is on an identity now */
static int forge_net_identity_in_use(int slot)
{
	int each;

	for (each = 0; each < FORGE_MAX_CLIENT_MACHINES; each++)
	{
		if (g_net_machine[each].identity == slot)
			return 1;
	}
	return 0;
}

/* an identity's slot, made for a machine at its first line (the game's own mark of who it is: the machine's address; a machine with none resolved has its
own by its place). The table full: the identity seen longest ago that is not let go of, has no instance, holds nothing and is on no machine gives its slot
up. None of those: -1, and the machine may join and pose but not edit (forge status says so). Machines behind one address share an identity: one budget, one
mark of being let go of */
static int forge_net_identity(long machine_index)
{
	struct ae_forge_net_machine *machine = &g_net_machine[machine_index];
	unsigned long now = system_milliseconds();

	if (machine->identity < 0)
	{
		unsigned long key = ae_forge_net_rules_identity_key((unsigned long)distributed_machine_real_address(machine_index), machine_index);
		int slot = ae_forge_net_rules_identity(g_net_identity_key, &g_net_identity_count, 64, key);

		if (slot < 0)
		{
			unsigned char pinned[64];
			int each;

			for (each = 0; each < g_net_identity_count; each++)
				pinned[each] = g_net_identity_client[each].dropped || g_net_identity_client[each].instances > 0 ||
					ae_forge_net_rules_count_held(g_net_held_by, FORGE_MAX_LIVE_INSTANCES, each + 1) > 0 || forge_net_identity_in_use(each);
			slot = ae_forge_net_rules_identity_evict(g_net_identity_seen_ms, pinned, g_net_identity_count, now);
			if (slot >= 0)
			{
				memset(&g_net_identity_client[slot], 0, sizeof(g_net_identity_client[slot]));
				g_net_identity_key[slot] = key;
			}
		}
		machine->identity = slot;
	}
	if (machine->identity >= 0)
		g_net_identity_seen_ms[machine->identity] = now ? now : 1;
	return machine->identity;
}

/* the live actors an instance costs the AI budget: an AI spawn's (its most alive at once), a placed actor's one, anything else none */
static int forge_net_ai_cost(int32_t idx)
{
	struct forge_live_instance const *inst = &g_forge.live_instances[idx];
	struct forge_ai_spawn_entry const *entry = NULL;
	uint32_t variant_hash = 0, weapon_hash = 0;

	if (inst->group_tag == 'actv')
		return 1;
	if (inst->group_tag != FORGE_GROUP_AI_SPAWN)
		return 0;
	forge_ai_spawn_net_settings(idx, &variant_hash, &weapon_hash, &entry);
	return entry ? ae_forge_net_rules_ai_live(entry->unit_count, entry->respawn_alive) : 0;
}

/* a client's identity's instance of this live index given back: its budget falls, the owner and holder marks go */
static void forge_net_instance_released(int32_t idx)
{
	if (idx < 0 || idx >= FORGE_MAX_LIVE_INSTANCES)
		return;
	if (g_net_instance_owner[idx] && g_net_instance_owner[idx] <= 64)
	{
		int is_ai = forge_net_ai_cost(idx) > 0;

		ae_forge_net_rules_client_removed(&g_net_identity_client[g_net_instance_owner[idx] - 1], is_ai, forge_net_ai_cost(idx));
	}
	g_net_instance_owner[idx] = 0;
	g_net_held_by[idx] = 0;
}

/* the live actors an identity has alive in the AI spawns it placed (other than `except`), and the session's (the same for every spawn) */
static void forge_net_ai_live(int identity, int32_t except, int *owned, int *session)
{
	int32_t each;

	*owned = *session = 0;
	for (each = 0; each < g_forge.live_instance_count && each < FORGE_MAX_LIVE_INSTANCES; each++)
	{
		int live;

		if (each == except || !g_forge.live_instances[each].active)
			continue;
		live = forge_net_ai_cost(each);
		*session += live;
		if (g_net_instance_owner[each] == identity + 1)
			*owned += live;
	}
}

static int forge_net_session_instances(void)
{
	int total = 0, each;

	for (each = 0; each < g_net_identity_count; each++)
		total += g_net_identity_client[each].instances;
	return total;
}

/* the machines in this session that have asked for the state, and their count */
static int forge_net_synced_machines(void)
{
	int each, total = 0;

	for (each = 0; each < FORGE_MAX_CLIENT_MACHINES; each++)
		total += g_net_machine[each].state == AE_FORGE_NET_MACHINE_SYNCED;
	return total;
}

/* the level is in: Forge's state is up and the map's own entities are in its table */
static int forge_net_level_ready(void)
{
	return g_forge.initialized && g_forge.live_instance_count > 0;
}

/* the instance a client's line names, if it may touch it: by its live index alone, and every refusal of the rules (ae_forge_net_rules_client_touch:
the object it names, the map's own, held, carried, occupied, the last of its class, a fresh slot) says no. -1 if none */
static int32_t forge_net_client_target(struct ae_forge_net_record const *record, int identity, unsigned long now)
{
	struct ae_forge_net_facts facts;
	int32_t idx = record->live_idx;
	int why;

	if (idx < 0 || idx >= g_forge.live_instance_count)
		return -1;
	forge_net_facts(idx, &facts);
	facts.sender_holds = ae_forge_net_rules_count_held(g_net_held_by, FORGE_MAX_LIVE_INSTANCES, identity + 1);
	if (record->tag == AE_FORGE_NET_FM)
		facts.dest_in_level = forge_net_in_level(record->x, record->y, record->z);
	why = ae_forge_net_rules_client_touch(&facts, record->tag, record->obj_abs, record->held, identity + 1, config_boolean("forge.clients_edit_map"), now);
	if (why != AE_FORGE_NET_TOUCH_OK)
	{
		if (why != AE_FORGE_NET_TOUCH_INACTIVE && forge_net_log_ok())
			platform_log("Forge: a client's line about instance %d is refused (%d): object %d", (int)idx, why, record->obj_abs);
		return -1;
	}
	return idx;
}

/* an AI spawn's settings from a checked line */
static int forge_net_apply_fa(struct ae_forge_net_record const *record, int32_t target)
{
	forge_ai_spawn_net_apply(target, record->hash, record->weapon_hash, record->team, record->unit_count, record->respawn_alive, record->respawn_delay,
		record->respawn_total, (unsigned int)record->gametypes, record->detached, record->spawn_effect);
	return 1;
}

static int forge_net_apply_fj(struct ae_forge_net_record const *record, int32_t target)
{
	forge_ai_spawn_goal_apply(target, record->goal, record->link, record->who, record->start, record->follow, record->alert);
	return 1;
}

static int forge_net_apply_fw(struct ae_forge_net_record const *record, int32_t target)
{
	float size[3];

	size[0] = (float)record->size[0] / 100.0f;
	size[1] = (float)record->size[1] / 100.0f;
	size[2] = (float)record->size[2] / 100.0f;
	forge_volume_apply(target, record->shape, record->effect, (float)record->damage, size, record->flags);
	return 1;
}

/* a client's edit counted and allowed: the host's setting that it alone edits */
static int forge_net_client_edit_ok(long machine_index, int identity, int tag)
{
	if (!ae_forge_net_rules_is_edit(tag))
		return 1;
	if (!ae_forge_net_rules_may_edit(1, config_boolean("forge.host_only_edit")))
	{
		if (!g_net_identity_client[identity].told)
		{
			g_net_identity_client[identity].told = 1;
			forge_net_send_refusal(machine_index, 2);
		}
		return 0;
	}
	return 1;
}

/* the senders the relay is shared among: the machines that sent a line within the last second (not those that asked for the state: one line from each
would else make the share small for all) */
static int forge_net_active_senders(unsigned long now)
{
	return ae_forge_net_rules_active_senders(g_net_machine_line_ms, FORGE_MAX_CLIENT_MACHINES, now);
}

static int forge_net_recipients(void)
{
	long machines[FORGE_MAX_CLIENT_MACHINES];
	short count = distributed_client_machines(machines, FORGE_MAX_CLIENT_MACHINES);

	return count > 1 ? count - 1 : 1;
}

/* a line of a stream (a Monitor's pose, a held instance's moves) sent on to the other machines: at once when the relay's budget allows, else the latest
of its key waits for it (the older lines of it are dropped, the line that lets go never) */
static void forge_net_relay_stream(long key, long sender, char const *line, int final, unsigned long now)
{
	if (ae_forge_net_rules_relay_ok(&g_net_relay, (int)sender, forge_net_recipients(), forge_net_active_senders(now), now))
	{
		ae_forge_net_rules_pending_clear(&g_net_pending, key);
		forge_broadcast_packet_to_clients(line, sender);
		return;
	}
	ae_forge_net_rules_pending_put(&g_net_pending, key, (int)sender, line, final);
}

/* each tick of the host: the streams waiting for the relay's budget go out as it allows */
static void forge_net_pending_tick(unsigned long now)
{
	char line[AE_FORGE_NET_TEXT_SIZE + 1];
	int sender, sent = 0;

	while (sent < 64 && ae_forge_net_rules_pending_flush(&g_net_pending, &g_net_relay, forge_net_recipients(), forge_net_active_senders(now), now, line, &sender))
	{
		forge_broadcast_packet_to_clients(line, sender);
		sent++;
	}
}

/* a line from a client to the host */
static int forge_net_from_client(long machine_index, char const *text)
{
	struct ae_forge_net_limits limits;
	struct ae_forge_net_record record;
	struct ae_forge_net_client *client;
	unsigned long now = system_milliseconds();
	int tag, identity, drop = 0;
	int32_t target_idx;
	char relay[64];

	if (machine_index < 0 || machine_index >= FORGE_MAX_CLIENT_MACHINES)
		return 1;
	forge_net_limits(&limits);
	tag = ae_forge_net_rules_parse(text, &limits, &record);
	if (tag == AE_FORGE_NET_NONE)
	{
		if (forge_net_log_ok())
		{
			char shown[24];

			ae_forge_net_rules_printable(text, shown, (int)sizeof(shown));
			platform_log("Forge: a line from machine %ld was refused (%d): %s", machine_index, record.reason, shown);
		}
		return 1;
	}
	if (!ae_forge_net_rules_client_may_say(tag))
		return 1;
	/* (nothing from a machine until the level is in and it has asked for the state; a request that came too soon is kept, one flag a machine, and answered
	when the level is in) */
	if (tag == AE_FORGE_NET_FQ && !forge_net_level_ready())
	{
		g_net_fq_pending[machine_index] = 1;
		return 1;
	}
	if (!ae_forge_net_rules_machine_accepts(&g_net_machine[machine_index], tag, forge_net_level_ready()))
		return 1;
	identity = forge_net_identity(machine_index);
	if (identity < 0)
	{
		/* (no room for another identity: the machine is in the game and may pose, but edits nothing) */
		g_net_machine_viewer[machine_index] = 1;
		if (!ae_forge_net_rules_identityless_may(tag))
			return 1;
		client = &g_net_identityless_client[machine_index];
	}
	else
		client = &g_net_identity_client[identity];
	if (client->dropped)
		return 1;
	if (!ae_forge_net_rules_client_line(client, tag, now, &drop))
	{
		if (drop)
		{
			client->dropped = 1;
			forge_net_send_refusal(machine_index, 4);
			platform_log("Forge: machine %ld is over its rate: its lines are no longer taken", machine_index);
		}
		return 1;
	}
	if (!forge_net_client_edit_ok(machine_index, identity, tag))
		return 1;
	if (ae_forge_net_rules_is_edit(tag) || tag == AE_FORGE_NET_FT)
		g_net_machine_line_ms[machine_index] = now ? now : 1;
	/* what the host sends on is shared out: a one-time edit that cannot be sent on is not applied either, so that no machine differs. A stream (a move, a
	pose) is applied and sent on when the budget allows, the latest of it after that */
	if (ae_forge_net_rules_is_edit(tag) && tag != AE_FORGE_NET_FM)
	{
		if (!ae_forge_net_rules_relay_ok(&g_net_relay, (int)machine_index, forge_net_recipients(), forge_net_active_senders(now), now))
		{
			forge_net_send_refusal(machine_index, 4);
			return 1;
		}
	}

	switch (tag)
	{
	case AE_FORGE_NET_FX:
		if (forge_net_log_ok())
			platform_log("Forge: machine %ld says: %s", machine_index, record.effect == 1 ? "its tags are not the host's" : "a refusal");
		return 1;

	case AE_FORGE_NET_FQ:
		/* the state asked for: the host's tags first (R1), then the settings. The feed began as the machine appeared: the first request restarts nothing */
		{
			char msg[40];

			snprintf(msg, sizeof(msg), "!FB:%X:%X", forge_base_tag_count(), forge_base_tag_fingerprint());
			forge_send_packet_to_machine(machine_index, msg);
			ae_forge_net_rules_machine_synced(&g_net_machine[machine_index]);
			if (g_net_machine_fq_seen[machine_index])
			{
				g_host_machine_known[machine_index] = 1;
				g_host_machine_sync_cursor[machine_index] = 0;
				g_host_machine_mon_synced[machine_index] = 0;
			}
			g_net_machine_fq_seen[machine_index] = 1;
			forge_map_settings_net_send_all(machine_index);
		}
		return 1;

	case AE_FORGE_NET_FT:
	{
		int p_abs = record.player;
		float yaw = (float)record.yaw / 100.0f, pitch = (float)record.pitch / 100.0f;

		/* Verify or resolve the player index belonging to this client machine */
		if (!distributed_machine_has_player(machine_index, (short)p_abs))
		{
			short cand;

			p_abs = -1;
			for (cand = 0; cand < 16; cand++)
			{
				if (distributed_machine_has_player(machine_index, cand))
				{
					p_abs = cand;
					break;
				}
			}
		}
		if (p_abs >= 0 && p_abs < 128)
		{
			forge_apply_host_player_monitor_state(p_abs, record.on, record.x, record.y, record.z, yaw, pitch);
			if (ae_forge_net_rules_format_ft(relay, (int)sizeof(relay), p_abs, record.on, record.x, record.y, record.z, record.yaw, record.pitch))
				forge_net_relay_stream(1000000L + machine_index, machine_index, relay, 0, now);
		}
		return 1;
	}

	case AE_FORGE_NET_FS:
	{
		struct forge_catalog_entry const *ce = forge_find_catalog_entry_mode(record.hash, 0);
		int is_ai, owned, session;
		int32_t made;

		if (!ce)
			return 1;
		is_ai = ce->group_tag == FORGE_GROUP_AI_SPAWN || ce->group_tag == 'actv';
		if (!ae_forge_net_rules_instance_team_ok(forge_net_team_kind(ce->group_tag), forge_net_flag_type(ce->group_tag, ce->tag_path), record.team))
			return 1;
		if ((ce->group_tag == 'spwn' || ce->group_tag == 'flag') && !forge_net_in_level(record.x, record.y, record.z))
			return 1;
		if (is_ai)
		{
			forge_net_ai_live(identity, -1, &owned, &session);
			if (!ae_forge_net_rules_ai_budget_ok(owned, 1, session))
			{
				forge_net_send_refusal(machine_index, 3);
				return 1;
			}
		}
		if (!ae_forge_net_rules_client_instance(client, forge_net_session_instances()))
		{
			forge_net_send_refusal(machine_index, 3);
			return 1;
		}
		if (is_ai && !ae_forge_net_rules_client_ai(client, 1))
		{
			/* (the instance counted is given back) */
			ae_forge_net_rules_client_removed(client, 0, 0);
			forge_net_send_refusal(machine_index, 3);
			return 1;
		}
		/* Host spawns authoritatively and broadcasts !FH: to all Clients (broadcast_net = 1) */
		made = forge_3d_spawn_object_at(ce->group_tag, ce->tag_path, ce->source_cache_id, record.x, record.y, record.z, (float)record.yaw / 100.0f,
			(float)record.pitch / 100.0f, (float)record.roll / 100.0f, (int16_t)record.team, 1);
		if (made >= 0 && made < FORGE_MAX_LIVE_INSTANCES)
		{
			g_net_instance_owner[made] = (uint8_t)(identity + 1);
			g_net_instance_creator[made] = (uint8_t)(identity + 1);
			g_net_instance_created_ms[made] = now ? now : 1;
			if (forge_net_log_ok())
				platform_log("Forge: machine %ld placed #%d (%s)", machine_index, (int)made, ce->tag_path);
		}
		else
			ae_forge_net_rules_client_removed(client, is_ai, is_ai ? 1 : 0);
		return 1;
	}

	default:
		break;
	}

	/* the edits of an instance: the host checks what the client may touch, applies from the record and relays its own line, never the sender's text */
	target_idx = forge_net_client_target(&record, identity, now);
	if (target_idx < 0)
		return 1;
	switch (tag)
	{
	case AE_FORGE_NET_FA:
	{
		int owned, session;

		forge_net_ai_live(identity, target_idx, &owned, &session);
		if (!ae_forge_net_rules_ai_budget_ok(owned, ae_forge_net_rules_ai_live(record.unit_count, record.respawn_alive), session))
		{
			forge_net_send_refusal(machine_index, 3);
			return 1;
		}
		if (!ae_forge_net_rules_ai_team_ok(record.team))
			return 1;
		forge_net_apply_fa(&record, target_idx);
		if (forge_ai_spawn_packet(target_idx, relay, sizeof(relay)))
			forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	}
	case AE_FORGE_NET_FJ:
		forge_net_apply_fj(&record, target_idx);
		if (forge_ai_goal_packet(target_idx, relay, sizeof(relay)))
			forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	case AE_FORGE_NET_FV:
		forge_3d_always_spawn_apply(target_idx, record.on);
		snprintf(relay, sizeof(relay), "!FV:%d:%d", (int)target_idx, record.on ? 1 : 0);
		forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	case AE_FORGE_NET_FW:
		forge_net_apply_fw(&record, target_idx);
		if (forge_volume_packet(target_idx, relay, sizeof(relay)))
			forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	case AE_FORGE_NET_FN:
		forge_3d_no_damage_apply(target_idx, record.on);
		snprintf(relay, sizeof(relay), "!FN:%d:%d", (int)target_idx, record.on ? 1 : 0);
		forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	case AE_FORGE_NET_FK:
		forge_3d_no_collision_apply(target_idx, record.on);
		snprintf(relay, sizeof(relay), "!FK:%d:%d", (int)target_idx, record.on ? 1 : 0);
		forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	case AE_FORGE_NET_FZ:
		forge_3d_scale_apply(target_idx, (float)record.thousandths / 1000.0f);
		snprintf(relay, sizeof(relay), "!FZ:%d:%d", (int)target_idx, record.thousandths);
		forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	case AE_FORGE_NET_FU:
		forge_3d_marker_usage_apply(target_idx, (int16_t)record.usage);
		snprintf(relay, sizeof(relay), "!FU:%d:%d", (int)target_idx, record.usage);
		forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	case AE_FORGE_NET_FM:
	{
		int host_obj_abs = forge_net_instance_object_abs(target_idx);

		/* the held mark follows who set it: released with the line that lets go (and when its holder leaves) */
		g_net_held_by[target_idx] = record.held ? (uint8_t)(identity + 1) : 0;
		if (host_obj_abs >= 0)
			g_net_obj_is_held[host_obj_abs] = record.held ? 1 : 0;
		forge_3d_move_instance_ext(target_idx, record.x, record.y, record.z, (float)record.yaw / 100.0f, (float)record.pitch / 100.0f,
			(float)record.roll / 100.0f, record.held, 0);
		if (forge_net_log_ok())
			platform_log("Forge: machine %ld moved #%d to %.2f %.2f %.2f", machine_index, (int)target_idx, record.x, record.y, record.z);
		if (ae_forge_net_rules_format_fm(relay, (int)sizeof(relay), (int)target_idx, host_obj_abs, record.x, record.y, record.z, record.yaw, record.pitch,
			record.roll, record.held))
			forge_net_relay_stream((long)target_idx, machine_index, relay, record.held ? 0 : 1, now);
		return 1;
	}
	case AE_FORGE_NET_FD:
	{
		int host_obj_abs = forge_net_instance_object_abs(target_idx);
		int k;

		for (k = 0; k < FORGE_MAX_TRACKED_NET_OBJECTS; k++)
		{
			if (g_net_obj_live_index[k] == target_idx)
			{
				g_net_obj_live_index[k] = -1;
				g_net_obj_is_held[k] = 0;
			}
		}
		if (host_obj_abs >= 0 && host_obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
		{
			g_net_obj_live_index[host_obj_abs] = -1;
			g_net_obj_is_held[host_obj_abs] = 0;
		}
		forge_net_instance_released(target_idx);
		ae_forge_net_rules_pending_clear(&g_net_pending, (long)target_idx);
		forge_3d_delete_instance(target_idx, 0);
		if (forge_net_log_ok())
			platform_log("Forge: machine %ld deleted #%d", machine_index, (int)target_idx);
		snprintf(relay, sizeof(relay), "!FD:%d:%d", (int)target_idx, host_obj_abs);
		forge_broadcast_packet_to_clients(relay, machine_index);
		return 1;
	}
	default:
		return 1;
	}
}

/* a line from the host to a joiner: from_ring when it was put by and is taken again now */
static int forge_net_from_host(char const *text, int from_ring)
{
	struct ae_forge_net_limits limits;
	struct ae_forge_net_record record;
	int tag;
	int32_t target_idx;
	unsigned long now = system_milliseconds();

	forge_net_limits(&limits);
	tag = ae_forge_net_rules_parse(text, &limits, &record);
	if (tag == AE_FORGE_NET_NONE)
	{
		if (forge_net_log_ok())
		{
			char shown[24];

			ae_forge_net_rules_printable(text, shown, (int)sizeof(shown));
			platform_log("Forge: a line was refused (%d): %s", record.reason, shown);
		}
		return 1;
	}
	if (!ae_forge_net_rules_host_may_say(tag))
		return 1;

	switch (tag)
	{
	case AE_FORGE_NET_FB:
		/* (the host's base tags: R1) */
		{
			int same = record.base_count == forge_base_tag_count() && record.fingerprint == forge_base_tag_fingerprint();
			int result = ae_forge_net_rules_tags_compare(1, record.base_count, record.fingerprint, forge_base_tag_count(), forge_base_tag_fingerprint());

			g_net_host_tags_known = 1;
			g_net_host_tags_count = record.base_count;
			g_net_host_tags_fingerprint = record.fingerprint;
			g_net_compare_state = ae_forge_net_rules_compare_state(g_net_compare_state, 1, same, g_net_state_asked_ms, now);
			if (forge_net_log_ok())
				platform_log("Forge: the host's base tags: %u (fingerprint %08X), this machine's %u (%08X): %s", record.base_count, record.fingerprint,
					forge_base_tag_count(), forge_base_tag_fingerprint(), same ? "the same, plays" : "different");
			if (!same)
			{
				ae_forge_net_rules_tags_message(result, record.base_count, forge_base_tag_count(), g_net_leave_message, (int)sizeof(g_net_leave_message));
				forge_send_packet_to_host("!FX:1");
			}
			else
				g_net_leave_message[0] = 0;
		}
		return 1;

	case AE_FORGE_NET_FX:
		forge_set_status("The host: %s", record.effect == 2 ? "editing is the host's alone" :
			record.effect == 3 ? "your budget of objects is full" : record.effect == 4 ? "too many edits: slow down" : "a refusal");
		return 1;

	case AE_FORGE_NET_FT:
		if (record.player != forge_get_local_player_abs_index())
			forge_apply_host_player_monitor_state(record.player, record.on, record.x, record.y, record.z, (float)record.yaw / 100.0f,
				(float)record.pitch / 100.0f);
		return 1;

	case AE_FORGE_NET_FE:
	{
		union real_point3d point;

		point.x = record.x;
		point.y = record.y;
		point.z = record.z;
		forge_ai_spawn_effect_play(record.effect, &point);
		return 1;
	}

	default:
		break;
	}

	/* what makes things waits: until the host's tags are compared (or the host says none), and in turn (its creates at the joiner's rate; a line put by
	keeps the lines behind it) */
	if (!from_ring)
	{
		if (!ae_forge_net_rules_apply_allowed(g_net_compare_state) || g_net_ring.count > 0 ||
			(tag == AE_FORGE_NET_FH && !ae_forge_net_rules_joiner_create(&g_net_joiner_creates, now)))
		{
			if (!ae_forge_net_rules_ring_push(&g_net_ring, text))
				g_net_ring_overflow = 1;
			return 1;
		}
	}

	switch (tag)
	{
	case AE_FORGE_NET_FGN: case AE_FORGE_NET_FGT: case AE_FORGE_NET_FGL: case AE_FORGE_NET_FGM: case AE_FORGE_NET_FGV: case AE_FORGE_NET_FGA:
		forge_map_settings_net_receive(text, 0);
		return 1;
	case AE_FORGE_NET_FA:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0 && ae_forge_net_rules_ai_team_ok(record.team))
			forge_net_apply_fa(&record, target_idx);
		return 1;
	case AE_FORGE_NET_FJ:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0)
			forge_net_apply_fj(&record, target_idx);
		return 1;
	case AE_FORGE_NET_FU:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0)
			forge_3d_marker_usage_apply(target_idx, (int16_t)record.usage);
		return 1;
	case AE_FORGE_NET_FV:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0)
			forge_3d_always_spawn_apply(target_idx, record.on);
		return 1;
	case AE_FORGE_NET_FZ:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0)
			forge_3d_scale_apply(target_idx, (float)record.thousandths / 1000.0f);
		return 1;
	case AE_FORGE_NET_FW:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0)
			forge_net_apply_fw(&record, target_idx);
		return 1;
	case AE_FORGE_NET_FN:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0)
			forge_3d_no_damage_apply(target_idx, record.on);
		return 1;
	case AE_FORGE_NET_FK:
		target_idx = forge_resolve_target_live_idx(record.live_idx, -1);
		if (target_idx >= 0)
			forge_3d_no_collision_apply(target_idx, record.on);
		return 1;
	case AE_FORGE_NET_FH:
	{
		unsigned int base_enc = record.object_code & 0xFFFu;
		unsigned int weap0_enc = (record.object_code >> 12) & 0xFFFu;
		int host_is_held = (record.object_code & (1u << 24)) ? 1 : 0;

		forge_client_handle_host_spawn(record.live_idx, record.hash, base_enc > 0 ? (int)(base_enc - 1) : -1,
			weap0_enc > 0 ? (int)(weap0_enc - 1) : -1, host_is_held, record.x, record.y, record.z, (float)record.yaw / 100.0f,
			(float)record.pitch / 100.0f, (float)record.roll / 100.0f, (int16_t)record.team);
		return 1;
	}
	case AE_FORGE_NET_FM:
		target_idx = forge_resolve_target_live_idx(record.live_idx, record.obj_abs);
		if (record.obj_abs >= 0 && record.obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
			g_net_obj_is_held[record.obj_abs] = record.held ? 1 : 0;
		if (target_idx >= 0 && target_idx != g_forge.grabbed_live_index)
			forge_3d_move_instance_ext(target_idx, record.x, record.y, record.z, (float)record.yaw / 100.0f, (float)record.pitch / 100.0f,
				(float)record.roll / 100.0f, record.held, 0);
		return 1;
	case AE_FORGE_NET_FD:
		target_idx = forge_resolve_target_live_idx(record.live_idx, record.obj_abs);
		if (target_idx >= 0)
		{
			int k;

			for (k = 0; k < FORGE_MAX_TRACKED_NET_OBJECTS; k++)
			{
				if (g_net_obj_live_index[k] == target_idx)
				{
					g_net_obj_live_index[k] = -1;
					g_net_obj_is_held[k] = 0;
				}
			}
			if (record.obj_abs >= 0 && record.obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
			{
				g_net_obj_live_index[record.obj_abs] = -1;
				g_net_obj_is_held[record.obj_abs] = 0;
			}
			forge_3d_delete_instance(target_idx, 0);
		}
		return 1;
	default:
		return 1;
	}
}

int forge_network_intercept_chat_packet(long machine_index, char const *text_in, int is_from_client_to_host)
{
	char text[AE_FORGE_NET_TEXT_SIZE + 1];
	int from_client = is_from_client_to_host && game_connection() == _game_connection_network_server;
	int from_host = !is_from_client_to_host && game_connection() == _game_connection_network_client;

	if (!text_in || text_in[0] != '!' || text_in[1] != 'F')
		return 0;

	/* a line is Forge's only in a game that started as a Forge session; in any other it is ordinary chat */
	if (!ae_forge_rules_line_accepted(g_forge_session_enabled, text_in)) /* AE hook */
		return 0; /* AE hook */

	/* its 64 bytes must hold their NUL (the line is parsed as text); else it is nothing, and nothing of chat either */
	if (!ae_forge_net_rules_text(text_in, text))
		return 1;
	if (from_client)
		return forge_net_from_client(machine_index, text);
	if (from_host)
		return forge_net_from_host(text, 0);
	return 1;
}

/* a machine left the game: what it held is let go of where it stands (the instance is not left ghosted, no collision, in its hand), and it is new again for the
next player on its slot; its identity (budgets, the mark of being let go of) stays for the session */
static void forge_net_machine_left(long machine_index)
{
	int identity = g_net_machine[machine_index].identity, each, held;
	int list[256];

	held = identity >= 0 ? ae_forge_net_rules_held_list(g_net_held_by, FORGE_MAX_LIVE_INSTANCES, identity + 1, list, (int)(sizeof(list) / sizeof(list[0]))) : 0;
	for (each = 0; each < held; each++)
	{
		int32_t idx = list[each];
		struct forge_live_instance const *inst = &g_forge.live_instances[idx];

		g_net_held_by[idx] = 0;
		if (idx < g_forge.live_instance_count && inst->active)
		{
			int obj_abs = forge_net_instance_object_abs(idx);

			if (obj_abs >= 0)
				g_net_obj_is_held[obj_abs] = 0;
			/* (each one's release goes to the clients at once: the throttle that spaces a drag's lines would pass the first only) */
			forge_3d_move_instance_ext(idx, inst->x, inst->y, inst->z, inst->yaw, inst->pitch, inst->roll, 0, 0);
			forge_network_send_transform_ext(idx, inst->object_index, inst->x, inst->y, inst->z, inst->yaw, inst->pitch, inst->roll, 0, 1);
		}
	}
	/* (what it held is let go of to every machine at once: the throttle that spaces a drag's lines would pass the first of them only) */
	if (identity >= 0)
		ae_forge_net_rules_client_machine_left(&g_net_identity_client[identity]);
	memset(&g_net_identityless_client[machine_index], 0, sizeof(g_net_identityless_client[machine_index]));
	memset(&g_net_feed_bucket[machine_index], 0, sizeof(g_net_feed_bucket[machine_index]));
	g_net_machine_line_ms[machine_index] = 0;
	g_net_machine_viewer[machine_index] = 0;
	g_net_fq_pending[machine_index] = 0;
	ae_forge_net_rules_pending_clear(&g_net_pending, 1000000L + machine_index);
	ae_forge_net_rules_machine_reset(&g_net_machine[machine_index]);
	g_net_machine_fq_seen[machine_index] = 0;
}

/* a joiner's each tick: the compare timed out (the host says none), the lines put by taken again in turn, a full ring asked for again */
static void forge_net_joiner_tick(unsigned long now)
{
	int before = g_net_compare_state, taken = 0;
	char line[AE_FORGE_NET_TEXT_SIZE + 1];

	g_net_compare_state = ae_forge_net_rules_compare_state(g_net_compare_state, 0, 0, g_net_state_asked_ms, now);
	if (before != g_net_compare_state && g_net_compare_state == AE_FORGE_NET_COMPARE_NOT_COMPARED)
		platform_log("Forge: the host said no base tags: not compared");
	while (taken < 200 && g_net_ring.count > 0 && ae_forge_net_rules_apply_allowed(g_net_compare_state) &&
		ae_forge_net_rules_joiner_create(&g_net_joiner_creates, now))
	{
		if (!ae_forge_net_rules_ring_pop(&g_net_ring, line))
			break;
		forge_net_from_host(line, 1);
		taken++;
	}
	if (g_net_ring_overflow && g_net_state_asked_ms && (unsigned int)now - (unsigned int)g_net_state_asked_ms >= 61000u)
	{
		g_net_ring_overflow = 0;
		g_net_state_asked_ms = now ? now : 1;
		forge_send_packet_to_host("!FQ");
	}
}

/* a joiner whose tags are not the host's leaves: asked for by the line, done at the next tick (not in the middle of the message that said so) */
static void forge_net_leave_tick(void)
{
	if (!g_net_leave_message[0] || game_connection() != _game_connection_network_client)
		return;
	platform_log("%s", g_net_leave_message);
	forge_set_status("%s", g_net_leave_message);
	g_net_leave_message[0] = 0;
	/* (the game is left for the main menu: the client let go of) */
	dispose_global_network_game_client();
	main_goto_main_menu();
}

/* what `forge status` says of the players on a host: the identities in use, and the machines that cannot edit as the table is full of ones that cannot be
let go of (they join and pose) */
void forge_network_editors_status(char *text, int size)
{
	int each, viewers = 0;

	text[0] = 0;
	if (game_connection() != _game_connection_network_server)
		return;
	for (each = 0; each < FORGE_MAX_CLIENT_MACHINES; each++)
		viewers += g_net_machine_viewer[each] && g_net_machine[each].identity < 0;
	snprintf(text, (size_t)size, "; %d of 64 player slots used%s", g_net_identity_count,
		viewers ? ", and machines that join and pose but cannot edit (no free slot)" : "");
}

/* what `forge status` says of the host's base tags */
void forge_network_tags_status(char *text, int size)
{
	if (game_connection() != _game_connection_network_client)
		ae_forge_net_rules_tags_message(AE_FORGE_NET_TAGS_SAME, 0, forge_base_tag_count(), text, size);
	else
		ae_forge_net_rules_tags_message(ae_forge_net_rules_tags_compare(g_net_host_tags_known, g_net_host_tags_count, g_net_host_tags_fingerprint,
			forge_base_tag_count(), forge_base_tag_fingerprint()), g_net_host_tags_count, forge_base_tag_count(), text, size);
}

void forge_network_pre_handle_object_changes(void const *entries, short count)
{
	struct forge_distributed_object_change *changes = (struct forge_distributed_object_change *)entries;
	uint32_t base_tag_count = forge_get_base_map_tag_count();
	short i;

	count = (short)ae_forge_net_rules_changes_count(count);
	memset(g_suppressed_change_slots, 0, (size_t)count);
	if (!changes || count <= 0)
		return;

	for (i = 0; i < count; i++)
	{
		if (changes[i].change == 0 /* _object_change_create */ && changes[i].object_index != NONE)
		{
			int obj_abs = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(changes[i].object_index);
			if (obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
			{
				g_net_obj_deferred_create[obj_abs] = changes[i];
				g_net_obj_has_deferred_create[obj_abs] = 1;
				if (g_net_obj_def_index[obj_abs] != NONE)
				{
					changes[i].definition_index = g_net_obj_def_index[obj_abs];
				}
				else if (base_tag_count > 0 &&
						 changes[i].definition_index != NONE &&
						 (uint32_t)DATUM_INDEX_TO_ABSOLUTE_INDEX(changes[i].definition_index) >= base_tag_count)
				{
					/* Dynamic tag from Host not yet resolved on Client via !FH: — defer creation cleanly */
					g_suppressed_change_slots[i] = 1;
					changes[i].change = 0xFF;
				}
			}
		}
		else if (changes[i].change == 1 /* _object_change_delete */ && changes[i].object_index != NONE)
		{
			int obj_abs = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(changes[i].object_index);
			if (obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
			{
				g_net_obj_has_deferred_create[obj_abs] = 0;
			}
		}
	}
}

void forge_network_post_handle_object_changes(void const *entries, short count)
{
	struct forge_distributed_object_change *changes = (struct forge_distributed_object_change *)entries;
	short i;

	count = (short)ae_forge_net_rules_changes_count(count);
	if (!changes || count <= 0)
		return;

	for (i = 0; i < count; i++)
	{
		if (g_suppressed_change_slots[i])
		{
			changes[i].change = 0;
			continue;
		}
		if (changes[i].change == 0 /* _object_change_create */ && changes[i].object_index != NONE)
		{
			int obj_abs = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(changes[i].object_index);
			if (obj_abs >= 0 && obj_abs < FORGE_MAX_TRACKED_NET_OBJECTS)
			{
				long existing_obj = forge_find_live_object_by_abs_index(obj_abs);
				if (existing_obj != NONE)
				{
					int32_t live_idx = g_net_obj_live_index[obj_abs];
					int is_held = g_net_obj_is_held[obj_abs] ? 1 : 0;
					g_net_obj_has_deferred_create[obj_abs] = 0;
					if (live_idx >= 0 && g_forge.grabbed_live_index == live_idx)
						is_held = 1;
					forge_client_configure_linked_object(live_idx, existing_obj, is_held);
				}
			}
		}
	}
}
