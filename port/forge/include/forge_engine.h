/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Runtime Engine Header - Shared across all Forge runtime modules.
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#ifndef FORGE_ENGINE_H
#define FORGE_ENGINE_H

#include <stdint.h>
#include "forge_cache_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Forge Release Version & Network Protocol Isolation (v2.4.1) */

#define FORGE_VERSION_STRING "2.4.2"
/* (this fork's own: Content Extended's Forge is 0x4628, and the two do not
play together. The browser lists one number alone: forge_get_network_version) */
#define FORGE_NETWORK_VERSION 0x4777
/* Standard (non-Forge) sessions use OpenCE's own HALO_PORT_NETWORK_VERSION
 * (port/linux/include/halo_port_limits.h), read in forge_get_network_version(),
 * so an upstream protocol bump never leaves Forge unable to join stock games. */
#define _forge_ai_team_friendly_human _forge_ai_team_friendly

/* an AI spawn's TEAM past the engine's own (_game_team_default..sentinel, kept
as they are): the players' teams, which forge_ai.c makes the match's as its
AI starts. In a team game RED and BLUE are the teams 0 and 1 the players are
on, and ALL PLAYERS the engine's unused team 6, which forge_ai_team_enmity
makes no player's enemy; in a free for all all three are ALL PLAYERS; in
Firefight (every player on _game_team_player) all three are that */
#define FORGE_AI_TEAM_ALL_PLAYERS 6
#define FORGE_AI_TEAM_RED 7
#define FORGE_AI_TEAM_BLUE 8

extern int g_forge_session_enabled;
/* turns a Forge session on where the rules allow it (a client only by Forge's own join); a session is turned off by assigning 0 */
void forge_session_start(int joined_at_forge_version, int local_request);
long forge_get_network_version(void);

/* Engine function (source/main/main.h) used by several Forge modules */
char *main_get_map_name(void);
char const *forge_get_active_map_name(void);
void *forge_host_fopen(char const *path, char const *mode);
int forge_host_remove(char const *path);
/* Forge's own files and folders (Faber_data: beside the game on Windows,
under the data root on the other builds) through the Win32 calls the port's
file layer has on every build (port/linux/src/xbox_files.c): its
#if FORGE_HOST_FILES blocks, Windows' alone before Content Extended reached
the other builds */
#define FORGE_HOST_FILES 1
/* (the platform layer's log: halo.log, and the console) */
void platform_log(char const *format, ...);
/* a static of Forge's kept on the heap, made (zeroed) when first wanted, not
in the game's image, which on Android has no room for Forge's big tables:
FORGE_HEAP_STATIC(type, name) where it was defined, then
#define name FORGE_HEAP(name), and it is used as before */
#define FORGE_HEAP_STATIC(type, name) \
	static type *name##_storage; \
	static type *name##_get(void) \
	{ \
		if (!name##_storage && !(name##_storage = (type *)calloc(1, sizeof(type)))) \
		{ \
			platform_log("Forge: out of memory for " #name " (%lu bytes)", (unsigned long)sizeof(type)); \
			abort(); \
		} \
		return name##_storage; \
	}
#define FORGE_HEAP(name) (*(name##_storage ? name##_storage : name##_get()))
/* the Tag Injector's tag bundles (port/linux/src/port_tag_profiles.h), which
Forge reads custom tags from */
long port_tag_profiles_bundle_open(long profile, long tag, unsigned long *size);
int port_tag_profiles_bundle_read_at(long handle, unsigned long offset, void *buffer, unsigned long size);
void port_tag_profiles_bundle_close(long handle);

#ifdef _WIN32
__declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
#endif
/* a key's state as GetAsyncKeyState answers (0x8000 down), Windows' or, on
the other builds, the platform layer's last read of the keys (forge_hooks.c);
none while a chat box is typed in (forge_chat_typing) */
short forge_key_state(int vk);
int forge_chat_typing(void);
/* whether one of Forge's menus is open: the chat's hotkeys are then not the chat's */
int forge_menu_open(void);
/* the first controller's state as the game gets it (the Xbox's: its
buttons, its analog buttons A B X Y BLACK WHITE LT RT, its sticks with up
positive); 0 with none, or while a chat box is typed in */
struct forge_pad
{
	unsigned short buttons;
	unsigned char analog[8];
	short lx, ly, rx, ry;
};
int forge_pad_read(struct forge_pad *pad);

#ifndef VK_LBUTTON
#define VK_LBUTTON   0x01
#define VK_RBUTTON   0x02
#endif

#ifndef VK_BACK
#define VK_BACK      0x08
#define VK_TAB       0x09
#define VK_RETURN    0x0D
#define VK_SHIFT     0x10
#define VK_CONTROL   0x11
#define VK_ESCAPE    0x1B
#define VK_SPACE     0x20
#define VK_PRIOR     0x21
#define VK_NEXT      0x22
#define VK_END       0x23
#define VK_HOME      0x24
#define VK_LEFT      0x25
#define VK_UP        0x26
#define VK_RIGHT     0x27
#define VK_DOWN      0x28
#define VK_DELETE    0x2E
#define VK_F1        0x70
#define VK_F2        0x71
#define VK_F3        0x72
#define VK_F4        0x73
#define VK_F5        0x74
#define VK_F6        0x75
#define VK_OEM_PLUS  0xBB
#define VK_OEM_MINUS 0xBD
#define VK_OEM_4     0xDB
#define VK_OEM_6     0xDD
#endif

/* ---------- Forge Editor Modes */

enum forge_editor_mode
{
	_forge_mode_play = 0,         /* Normal Spartan / Player control */
	_forge_mode_3d_monitor = 1,   /* Mode 2: 3D In-Engine 343 Guilty Spark Monitor & Physics Beam */
	NUMBER_OF_FORGE_MODES
};

enum forge_ui_modal
{
	_forge_modal_none = 0,
	_forge_modal_spawn_palette,
	_forge_modal_object_properties,

	_forge_modal_save_copy_as,
	/* the map's general settings (the palette's button held:
	forge_map_settings_menu.inl) */
	_forge_modal_map_settings
};

enum forge_palette_category
{
	_forge_cat_weapons = 0,
	_forge_cat_equipment,
	_forge_cat_vehicles,
	_forge_cat_bipeds,
	_forge_cat_scenery,
	_forge_cat_barriers_devices,
	_forge_cat_lights,
	_forge_cat_spawns,
	_forge_cat_objectives,
	_forge_cat_ai_squads,
	/* Firefight's: its spawns, generators, drop areas and resupply racks
	(after the rest: the pack's palette file names categories by number) */
	_forge_cat_firefight,
	/* the teleporter nodes and every teleporter scenery (after Firefight for
	the same reason; the palette shows it after SPAWNS: forge_3d_mode.c) */
	_forge_cat_teleporters,
	/* particle emitters (scenery\emitters\ and the like: fire, sparks, steam),
	once left out; after the rest for the same reason */
	_forge_cat_effects,
	/* decals placed on the map's surfaces: blood, burns, bullet holes, signs */
	_forge_cat_decals,
	/* damage and kill volumes (forge_volumes.c): boxes, cylinders, spheres,
	and the map's own deadly trigger volumes */
	_forge_cat_volumes,
	NUMBER_OF_FORGE_PALETTE_CATEGORIES
};

/* where a palette tag comes from: the tabs that are split by it show a folder
of each (forge_catalog_category_split) */
enum forge_catalog_origin
{
	_forge_origin_builtin = 0,  /* Forge's own markers: spawns, objectives */
	_forge_origin_custom,       /* the game's props (tag_props.bin): the drop capsule, the ammo crate */
	_forge_origin_campaign,     /* the Forge pack's: every campaign map's and the stock multiplayer maps' */
	_forge_origin_map,          /* the map's own tags, baked in it */
	_forge_origin_profile,      /* the chosen Tag Injector profile's, and a saved map's bake */
	NUMBER_OF_FORGE_CATALOG_ORIGINS
};

#define _forge_cat_count NUMBER_OF_FORGE_PALETTE_CATEGORIES

enum forge_snap_angle_mode
{
	_forge_snap_off = 0,
	_forge_snap_15_deg,
	_forge_snap_45_deg,
	_forge_snap_90_deg,
	NUMBER_OF_FORGE_SNAP_MODES
};

#include "halo_firefight_route.h"

/* what a piece of a dropship route's line is, for its colour */
enum forge_route_piece
{
	_forge_route_piece_approach = 0,
	_forge_route_piece_departure,
	_forge_route_piece_blocked,
	_forge_route_piece_tail
};

/* ---------- Firefight Custom Spawn Marker ('ffsp' / netgame_flag type 15) */

#define FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN 15
/* (an objective, 'flag' as CTF's and KOTH's are: where Firefight puts its
generators, firefight_generators.c) */
#define FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR 16

enum forge_ff_spawn_type
{
	_forge_ff_spawn_ai_infantry_only = 0,   /* Standard enemy infantry wave spawn */
	_forge_ff_spawn_ai_and_vehicles = 1,    /* Enemy infantry + approved enemy vehicles */
	_forge_ff_spawn_enemy_vehicle_only = 2, /* Approved enemy vehicle spawn (Ghost/Wraith/Banshee/Scorpion) */
	_forge_ff_spawn_enemy_ghost_only = 3,   /* Approved Enemy Ghost spawn */
	_forge_ff_spawn_enemy_heavy_veh = 4,    /* Approved Enemy Wraith / Scorpion / Banshee spawn */
	_forge_ff_spawn_dropship_zone = 5,      /* Enemy drop point taking any drop; 8-10 one kind alone (halo_firefight_route.h) */
	_forge_ff_spawn_player_vehicle = 6,     /* Approved Player Vehicle Pad (Round 4 Warthog / Round 5 Scorpion) */
	NUMBER_OF_FORGE_FF_SPAWN_TYPES = 7,
	/* where the players spawn in Firefight, over the map's starting
	locations (halo_firefight_route.h's FF_PLAYER_SPAWN_TYPE) */
	_forge_ff_spawn_player_spawn = 14
};

enum forge_ff_round_filter
{
	_forge_ff_round_all = 0,       /* All Rounds */
	_forge_ff_round_1_only = 1,    /* Round 1 Only */
	_forge_ff_round_2_only = 2,    /* Round 2 Only */
	_forge_ff_round_3_only = 3,    /* Round 3 Only */
	_forge_ff_round_4_only = 4,    /* Round 4 Only */
	_forge_ff_round_5_only = 5,    /* Round 5 Only */
	_forge_ff_round_early_1_3 = 6, /* Rounds 1-3 (Early) */
	_forge_ff_round_late_4_5 = 7,  /* Rounds 4-5 (Late) */
	_forge_ff_round_bonus = 8,     /* Bonus Round Only */
	NUMBER_OF_FORGE_FF_ROUND_FILTERS = 9
};

enum forge_ff_wave_filter
{
	_forge_ff_wave_all = 0,        /* All Waves (1-3) */
	_forge_ff_wave_1_only = 1,     /* Wave 1 Only (Initial) */
	_forge_ff_wave_2_only = 2,     /* Wave 2 Only (Reinforcements) */
	_forge_ff_wave_3_boss = 3,     /* Wave 3 Only (Boss Wave) */
	NUMBER_OF_FORGE_FF_WAVE_FILTERS = 4
};

/* (bits 11-14 are a dropship route's, and a route's own points read the rest
differently: halo_firefight_route.h) */
#define FORGE_FF_PACK_USAGE(type, rnd, wav) \
	((int16_t)((((int)(type)) & 0x0F) | ((((int)(rnd)) & 0x0F) << 4) | ((((int)(wav)) & 0x07) << 8)))
#define FORGE_FF_UNPACK_TYPE(u)  (((int)(u)) & 0x0F)
#define FORGE_FF_UNPACK_ROUND(u) ((((int)(u)) >> 4) & 0x0F)
#define FORGE_FF_UNPACK_WAVE(u)  ((((int)(u)) >> 8) & 0x07)

/* ---------- Runtime Palette Catalog Entry */

struct forge_catalog_entry
{
	uint32_t group_tag;
	uint8_t category;
	uint8_t source_cache_id; /* 0 = forgeasset.map (universal), 1 = customforgeasset.map (SP only) */
	uint16_t pad;
	/* (enum forge_catalog_origin) where it comes from; and its keyword folder
	(forge_catalog_keyword_name, 0: none). A folder row (FORGE_GROUP_FOLDER)
	has the origin and keyword it opens */
	uint8_t origin;
	uint8_t keyword;
	/* a FIREFIGHT SPAWNS row's: the marker usage it places
	(FORGE_FF_PRESET_PATH); 0 for any other row */
	uint16_t preset;
	char tag_path[FORGE_TAG_PATH_MAX];
	char display_name[64];
};

/* FIREFIGHT's FIREFIGHT SPAWNS row, which opens its kinds of marker: each
a row of its own, 'ffsp' with a path under FORGE_FF_PRESET_PATH and its
usage in `preset`; what it places is "spawn\\firefight_spawn" */
#define FORGE_GROUP_FF_MENU 'ffmn'
#define FORGE_FF_MENU_PATH "spawn\\firefight_spawns"
#define FORGE_FF_PRESET_PATH "spawn\\firefight_spawn\\"
#define FORGE_FF_SPAWN_PATH "spawn\\firefight_spawn"

/* a palette folder's row: an origin's (keyword 0) or a keyword's within it */
#define FORGE_GROUP_FOLDER 'fold'
#define FORGE_FOLDER_PATH "forge_folder\\"

/* whether a tab is split into origins and keyword folders */
int forge_catalog_category_split(int category);
char const *forge_catalog_origin_name(int origin);
char const *forge_catalog_keyword_name(int keyword);

#define FORGE_MAX_CATALOG_ENTRIES 3072

/* ---------- Live Placed Instance Tracking */

struct forge_live_instance
{
	int32_t active;
	int32_t object_index;      /* Engine object datum index, or -1 if marker-only */
	int32_t actor_index;       /* Engine AI actor datum index, or -1 */
	int32_t edit_entry_index;  /* Index in edit_state */
	uint32_t group_tag;
	uint8_t source_cache_id;
	uint8_t is_ai_unit;
	uint8_t is_default_scenario_object; /* 1 if adopted from the base map's default scenario objects */
	uint8_t pad_inst;
	/* a placed vehicle made in every game, whatever the gametype's vehicles
	(Object Settings' ALWAYS SPAWN: game_engine_vehicle_always_spawn) */
	uint8_t always_spawn;
	/* a placed emitter's Object Settings DEAL DAMAGE off: its effects' damage
	parts do nothing (forge_volumes.c, effects.c); saved as OBJECT_NO_DAMAGE,
	sent as !FN: */
	uint8_t no_damage;
	/* a placed scenery's Object Settings COLLISION off: seen, but nothing
	collides with it (forge_3d_no_collision_*); saved as OBJECT_NO_COLLISION,
	sent as !FK: */
	uint8_t no_collision;
	uint16_t gametype_flags;   /* 0 = All Modes, bit0=CTF, bit1=Slayer, bit2=Oddball, bit3=KOTH, bit4=Race */
	int16_t netgame_flag_type; /* Scenario netgame_flag type (6=tele_from, 7=tele_to, 8=hill, 2=oddball, 0=ctf) or -1 */
	int16_t team_designator;   /* Reference/Channel ID or Team Index */
	/* a teleporter scenery's netgame flag (netgame_flag_type 6 or 7): where it
	stands from the object, up and turned (forge_tele_*) */
	float tele_dz, tele_dyaw;
	float orig_x, orig_y, orig_z; /* Original scenario position if is_default_scenario_object == 1 */
	float x, y, z;
	float yaw, pitch, roll;
	/* its size (0: as made, 1): a scenery's or an effect's, Object Settings'
	SCALE (forge_3d_scale_apply), saved as the OBJECT_SCALE line before its
	OBJECT line and sent as !FZ: */
	float scale;
	char tag_path[FORGE_TAG_PATH_MAX];
	char label[64];
};

/* (what Forge tracks on a map: the map's own objects it takes over, spawns,
flags and AI spawns too, and what is placed. Big maps (The Silent
Cartographer: Evolved) have more than 1024 of their own; the engine holds
HALO_PORT_MAXIMUM_OBJECTS_PER_MAP, 8192. About 380 bytes each, on the heap
with the rest of g_forge) */
#define FORGE_MAX_LIVE_INSTANCES 4096

/* ---------- 3D Snap Point Configuration Per Tag */

#define FORGE_MAX_SNAP_POINTS_PER_TAG 16
#define FORGE_MAX_SNAP_TAG_CONFIGS    512

enum forge_snap_normal_dir
{
	_forge_snap_dir_front = 0,  /* +X (1, 0, 0) */
	_forge_snap_dir_right = 1,  /* -Y (0, -1, 0) */
	_forge_snap_dir_back  = 2,  /* -X (-1, 0, 0) */
	_forge_snap_dir_left  = 3,  /* +Y (0, 1, 0) */
	_forge_snap_dir_up    = 4,  /* +Z (0, 0, 1) */
	_forge_snap_dir_down  = 5,  /* -Z (0, 0, -1) */
	NUMBER_OF_FORGE_SNAP_DIRS
};

enum forge_snap_preset_id
{
	_forge_snap_preset_custom       = 0,
	_forge_snap_preset_front_center = 1,
	_forge_snap_preset_back_center  = 2,
	_forge_snap_preset_left_center  = 3,
	_forge_snap_preset_right_center = 4,
	_forge_snap_preset_top_center   = 5,
	_forge_snap_preset_bot_center   = 6,
	_forge_snap_preset_front_base   = 7,
	_forge_snap_preset_back_base    = 8,
	_forge_snap_preset_left_base    = 9,
	_forge_snap_preset_right_base   = 10,
	NUMBER_OF_FORGE_SNAP_PRESETS
};

struct forge_snap_point
{
	float x, y, z;
	float nx, ny, nz;
	uint8_t normal_dir;
	uint8_t preset_id;
	uint16_t pad;
};

struct forge_tag_snap_config
{
	uint32_t group_tag;
	char tag_path[FORGE_TAG_PATH_MAX];
	int point_count;
	struct forge_snap_point points[FORGE_MAX_SNAP_POINTS_PER_TAG];
};

/* ---------- Global Runtime Forge State */

struct forge_runtime_state
{
	int initialized;
	enum forge_editor_mode mode;
	enum forge_ui_modal active_modal;
	enum forge_palette_category current_category;
	int h3_menu_stage; /* 0 = Halo 3 category list, 1 = Halo 3 item list */
	int palette_cursor;
	int palette_scroll;
	int props_cursor;
	enum forge_snap_angle_mode snap_mode;
	int ai_team_mode; /* _forge_ai_team_friendly_human or _forge_ai_team_enemy_covenant */
	int third_person_view; /* Toggleable via console `third_person` or F4 to inspect 343 Guilty Spark */

	/* Monitor 6DOF Free-Cam State */
	int monitor_cam_initialized;
	float cam_x, cam_y, cam_z;
	float cam_yaw, cam_pitch, cam_roll;
	float cam_speed;

	/* Physics Beam / Object Grab State */
	int32_t grabbed_object_index;
	int32_t grabbed_live_index;
	int grabbed_is_new_spawn;
	/* the grabbed instance is only having its settings changed: it stays
	where it is, and is let go when the settings close */
	int grabbed_for_settings;
	float grabbed_orig_x, grabbed_orig_y, grabbed_orig_z;
	float grabbed_orig_yaw, grabbed_orig_pitch, grabbed_orig_roll;
	float grab_distance;
	float grab_yaw, grab_pitch, grab_roll;
	int32_t hovered_object_index;
	int32_t hovered_live_index;
	char hovered_object_label[128];
	float ray_hit_x, ray_hit_y, ray_hit_z;
	int ray_hit_valid;

	/* 10-Point Linked KOTH Hill Circle Placement State */
	int koth_group_placing;
	int koth_group_hill_id;
	float koth_group_radius;
	int32_t koth_group_live_indices[10];

	/* The dropship zone whose freshly made route moves with it while it is held, or -1 */
	int32_t route_follow_live_index;

	/* Catalogs: 0 = map-local [MAP] (transient), 1 = stock cached_tags, 2 = custom cached_tags [C] */
	struct forge_catalog_entry catalog[FORGE_MAX_CATALOG_ENTRIES];
	int catalog_count;

	/* Save Map state: prompts for name on initial save, then quick-updates on subsequent saves */
	int has_saved_map_once;
	char save_map_name[64];
	int save_map_name_len;

	/* Status banner message */
	char status_message[128];
	uint32_t status_message_until_ms;

	/* Scenario Edit State & Live Instances */
	struct forge_scenario_edit_state edit_state;
	struct forge_live_instance live_instances[FORGE_MAX_LIVE_INSTANCES];
	int live_instance_count;

	/* Forge Controls & Controller State */
	int using_controller;
	int bind_vk[16];
	int bind_gp[16];
	float controller_look_sens;
	float controller_fly_speed;

	/* Per-Player Multiplayer Monitor vs. Spartan State & Orientation */
	uint8_t player_is_monitor[128];
	uint8_t player_has_monitor_pos[128];
	float player_monitor_pos[128][3];
	float player_monitor_yaw[128];
	float player_monitor_pitch[128];
	uint8_t player_has_render_pos[128];
	float player_monitor_render_pos[128][3];
	float player_monitor_render_yaw[128];
	float player_monitor_render_pitch[128];

	/* 3D Magnetic Snap Point Lock State */
	int snap_point_locked;
	float snap_lock_world_x, snap_lock_world_y, snap_lock_world_z;
	char snap_lock_label[64];
};

enum forge_bind_action
{
	FORGE_BIND_TOGGLE_MODE = 0,
	FORGE_BIND_PALETTE = 1,
	FORGE_BIND_THIRD_PERSON = 2,
	FORGE_BIND_QUICK_SAVE = 3,
	FORGE_BIND_ROTATE_HOLD = 4,
	FORGE_BIND_SNAP_FLOOR = 5,
	FORGE_BIND_SNAP_SURFACE = 6,
	FORGE_BIND_ROT_SNAP = 7,
	FORGE_BIND_OBJECT_OPTIONS = 8,
	FORGE_BIND_DELETE_OBJECT = 9,
	FORGE_BIND_COPY_OBJECT = 10,
	/* what the reticle is on: its settings, without picking it up */
	FORGE_BIND_LOOK_SETTINGS = 11,
	FORGE_BIND_COUNT = 12
};

enum forge_gamepad_button
{
	FORGE_GP_NONE = 0,
	FORGE_GP_A = 1,
	FORGE_GP_B = 2,
	FORGE_GP_X = 3,
	FORGE_GP_Y = 4,
	FORGE_GP_BLACK = 5,
	FORGE_GP_WHITE = 6,
	FORGE_GP_LTRIGGER = 7,
	FORGE_GP_RTRIGGER = 8,
	FORGE_GP_DPAD_UP = 9,
	FORGE_GP_DPAD_DOWN = 10,
	FORGE_GP_DPAD_LEFT = 11,
	FORGE_GP_DPAD_RIGHT = 12,
	FORGE_GP_START = 13,
	FORGE_GP_BACK = 14,
	FORGE_GP_LTHUMB = 15,
	FORGE_GP_RTHUMB = 16,
	FORGE_GP_COUNT = 17
};

#define FORGE_GP_RB     FORGE_GP_BLACK
#define FORGE_GP_LB     FORGE_GP_WHITE
#define FORGE_GP_LT     FORGE_GP_LTRIGGER
#define FORGE_GP_RT     FORGE_GP_RTRIGGER
#define FORGE_GP_LSTICK FORGE_GP_LTHUMB
#define FORGE_GP_RSTICK FORGE_GP_RTHUMB

/* Forge's state: on the heap, made (zeroed) when first wanted, not in the
game's image, which on Android has no room for its 1.5 MB
(forge_runtime_state_get) */
extern struct forge_runtime_state *g_forge_state;
struct forge_runtime_state *forge_runtime_state_get(void);
#define g_forge (*(g_forge_state ? g_forge_state : forge_runtime_state_get()))

void forge_controls_init_defaults(void);
void forge_controls_load(void);
void forge_controls_save(void);
char const *forge_vk_display_name(int vk);
char const *forge_gp_display_name(int gp);
char const *forge_gp_icon_token(int gp);
char const *forge_action_prompt(int action);
int forge_gp_button_down(int gp);
int forge_action_down(int action);
void forge_poll_controller_state(float *out_lx, float *out_ly, float *out_rx, float *out_ry, float *out_lt, float *out_rt);

/* ---------- Prototypes: forge_runtime_tags.c */

void forge_runtime_init(void);
void forge_runtime_reload_catalogs(void);
int forge_is_multiplayer_session(void);
int forge_catalog_entry_allowed(struct forge_catalog_entry const *entry);
long forge_resolve_engine_tag(uint32_t group_tag, char const *tag_path, uint32_t *out_actual_group);
uint32_t forge_get_base_map_tag_count(void);
int forge_ui_save_copy_as(char const *new_map_name);
void forge_trigger_save_map_action(void);
int forge_level_is_forges(char const *map_name);
void forge_load_saved_map_state_if_present(char const *map_name);
void forge_normalize_mcc_hud_tags(void);
int forge_is_mcc_hud_active(void);
void forge_set_status(char const *fmt, ...);
void forge_draw_screen_rect(short x0, short y0, short x1, short y1, uint32_t argb);
int forge_monitor_render_player_unit(long object_index, void const *lighting, unsigned long flags);
void forge_render_world_3d_overlays(void const *lighting);

/* ---------- Prototypes: forge_ai.c */

/* an AI spawn's marker: a live instance whose edit_entry_index is its
edit_state.ai_spawns entry, and tag_path its actor variant */
#define FORGE_GROUP_AI_SPAWN 'aisp'
/* the Actors tab's group rows, and a group's RANDOM row: their paths are these
and the group's number. A spawn placed from RANDOM keeps a real rank's tag, and
FORGE_AI_RANDOM_RANK, in gametype_flags' spare top bit, so saved maps and the
network carry it as they are */
#define FORGE_GROUP_AI_MENU 'aigp'
#define FORGE_AI_GROUP_PATH "forge_group\\"
#define FORGE_AI_RANDOM_PATH "forge_random\\"
#define FORGE_AI_RANDOM_RANK 0x8000u
#define FORGE_AI_KIND_OTHER (-1)
#define FORGE_AI_KIND_HIDDEN (-2)
int forge_ai_kind_of(char const *path);
char const *forge_ai_kind_name(int kind);
int forge_ai_kind_group(int kind);
int forge_ai_group_count(void);
char const *forge_ai_group_name(int group);
int forge_ai_row_is_random(struct forge_catalog_entry const *entry);
int forge_ai_row_group(struct forge_catalog_entry const *entry);
int forge_ai_tag_is_poor(char const *path);
/* the rows of an AI spawn's properties. Its panel's first page has UNIT
SETTINGS >, SPAWN SETTINGS > and GOAL SETTINGS >, then ROT SNAP, DROP TO FLOOR
and DELETE; each page opens on its rows and closes on < BACK:
	UNIT    AI, TEAM, LOADOUT, CLOAKED, GAME MODE, FLYING HEIGHT (a flying
	        unit's alone)
	SPAWN   AT START, RESPAWN (keep alive), RESPAWN DELAY, RESPAWN LIMIT,
	        SPAWN EFFECT
	GOAL    GOAL, then what it has of PATROL VOLUME or ROUTE (the target row),
	        PLAYER, RANGE and STOP DISTANCE (a team friendly to players), SPEED
	        and ALERT TIME */
enum forge_ai_spawn_row
{
	_forge_ai_row_variant = 0,
	_forge_ai_row_weapon,
	_forge_ai_row_team,
	_forge_ai_row_start_count,
	_forge_ai_row_respawn_alive,
	_forge_ai_row_respawn_delay,
	_forge_ai_row_respawn_total,
	_forge_ai_row_game_mode,
	_forge_ai_row_spawn_effect,
	_forge_ai_row_cloaked,
	_forge_ai_row_hover_height,
	/* (the pages, opened on a press) */
	_forge_ai_row_unit_page,
	_forge_ai_row_spawn_page,
	_forge_ai_row_goal_page,
	NUMBER_OF_FORGE_AI_SETTING_ROWS,
	_forge_ai_row_rot_snap = NUMBER_OF_FORGE_AI_SETTING_ROWS,
	_forge_ai_row_floor,
	_forge_ai_row_delete,
	_forge_ai_row_goal,
	_forge_ai_row_goal_target,
	_forge_ai_row_goal_who,
	_forge_ai_row_goal_start,
	_forge_ai_row_goal_follow,
	/* (SPEED: WALK or RUN, any goal but none) */
	_forge_ai_row_goal_speed,
	/* (ALERT TIME: how long the squad stays alert, any goal but none) */
	_forge_ai_row_goal_alert,
	/* (< BACK: any page's, to the first) */
	_forge_ai_row_goal_back,
	NUMBER_OF_FORGE_AI_ROWS
};

/* whether an AI spawn's row opens or closes a page (on a press, not a step) */
#define FORGE_AI_ROW_IS_PAGE_BUTTON(row) ((row) == _forge_ai_row_unit_page || (row) == _forge_ai_row_spawn_page || \
	(row) == _forge_ai_row_goal_page || (row) == _forge_ai_row_goal_back)

/* an AI spawn's GOAL (forge_ai_spawn_entry.goal): what its units do when they
are not fighting */
enum forge_ai_goal
{
	/* (as ever: they stand, and fight from around the spawn) */
	_forge_ai_goal_none = 0,
	/* roam the ground in an AI PATROL volume (goal_link its number), each unit
	from point to point of its own; they chase out to its PURSUIT RANGE */
	_forge_ai_goal_patrol,
	/* walk an AI route (goal_link its number): a leader walks it, the others
	follow the leader */
	_forge_ai_goal_route,
	/* GO TO PLAYER: go to the nearest player of goal_who within goal_start (0:
	anywhere), stopping goal_follow from it when their team is a players'
	(red, blue, all players); others go all the way, at a run */
	_forge_ai_goal_hunt,
	NUMBER_OF_FORGE_AI_GOALS
};
/* (FOLLOW PLAYER, a map's of before: read as GO TO PLAYER) */
#define FORGE_AI_GOAL_LEGACY_FOLLOW 4
/* (ALERT TIME, goal_alert's seconds: 0 is this) */
#define FORGE_AI_GOAL_ALERT_DEFAULT 12

/* whom GO TO PLAYER goes for (goal_who) */
enum
{
	_forge_ai_goal_who_any = 0,
	_forge_ai_goal_who_red,
	_forge_ai_goal_who_blue,
	NUMBER_OF_FORGE_AI_GOAL_WHO
};

/* goal_who's byte also holds the GOAL's SPEED: its low bits whom (above), its
top bit RUN (they go at a fight's pace out of one too; 0: WALK, the engine's
alert pace). Packed there, the saved |gw= and the !FJ: packet carry it as they
are */
#define FORGE_AI_GOAL_WHO_MASK 0x0F
#define FORGE_AI_GOAL_RUN_BIT 0x80
#define FORGE_AI_GOAL_WHO(value) ((int)((value) & FORGE_AI_GOAL_WHO_MASK) < NUMBER_OF_FORGE_AI_GOAL_WHO ? \
	(int)((value) & FORGE_AI_GOAL_WHO_MASK) : 0)
#define FORGE_AI_GOAL_RUNS(value) (((value) & FORGE_AI_GOAL_RUN_BIT) != 0)

/* the rows an AI spawn's panel shows (the goal's page's while it is open),
how many */
int forge_ai_spawn_rows(int32_t live_idx, int *rows);
/* a spawn's goal, as the network has it (!FJ:) */
void forge_ai_spawn_goal_apply(int32_t live_idx, int goal, int link, int who, int start, int follow, int alert);
/* the spawns' lines to their goals, in Forge mode: those of a spawn held or
pointed at, and those of all the spawns linked to a route or AI PATROL volume
held or pointed at */
void forge_ai_goal_links_render(void (*draw_line)(float const *from, float const *to));

int32_t forge_ai_spawn_unit_live(
	char const *actor_variant_path,
	float x, float y, float z, float yaw,
	int16_t team_designator,
	int32_t *out_unit_index);
/* AI is a multiplayer map's alone: never a campaign level's, or any solo map's */
int forge_ai_map_editable(void);
/* a multiplayer map with encounters of its own */
int forge_ai_map_supported(void);
int forge_ai_palette_entry_allowed(struct forge_catalog_entry const *entry);
/* the campaign tags a save names: the spawns' */
void forge_ai_saved_tags_collect(void);
/* which campaign map has a tag (forge_runtime_tags.c); 0: none does */
int forge_campaign_tag_map(uint32_t group_tag, char const *path, char *map, size_t map_size);
/* the tags a saved map brings in, read as its tags load */
int forge_ai_saved_actor_count(void);
unsigned long forge_ai_saved_actor_group(int index);
void forge_ai_saved_clear(void);
char const *forge_ai_saved_actor_path(int index);
char const *forge_ai_saved_actor_map(int index);
void forge_ai_saved_parse(char const *text);
void forge_ai_markers_scan(void);
int forge_ai_spawn_entry_new(int32_t live_idx, char const *variant_path, float x, float y, float z, float yaw, int16_t team);
void forge_ai_spawn_moved(struct forge_live_instance const *inst);
void forge_ai_spawn_deleted(struct forge_live_instance const *inst);
/* a spawn's settings as they are, and put back (the properties' UNDO) */
int forge_ai_spawn_settings_get(int32_t live_idx, struct forge_ai_spawn_entry *settings);
void forge_ai_spawn_settings_set(int32_t live_idx, struct forge_ai_spawn_entry const *settings);
void forge_ai_spawn_copy_settings(int source_entry, int32_t live_idx);
void forge_ai_spawn_row_text(int32_t live_idx, int row, char *label, size_t label_size, char *value, size_t value_size);
/* a row stepped: 0, or FORGE_AI_ROUTE_PRESS_PAGE_OPENED/CLOSED (forge_ai_routes.h)
for a page opened or closed */
int forge_ai_spawn_row_step(int32_t live_idx, int row, int dir);
/* the page row (UNIT/SPAWN/GOAL SETTINGS >) the last BACK came back to */
int forge_ai_spawn_page_return_row(void);
/* a page closed (back/Esc): 1 when one was open, the page row then forge_ai_spawn_page_return_row */
int forge_ai_spawn_page_back(int32_t live_idx);
/* the title of the panel's page */
char const *forge_ai_spawn_panel_title(int32_t live_idx);
void forge_ai_spawn_label(struct forge_live_instance const *inst, char *text, size_t text_size);
void forge_ai_spawn_net_apply(
	int32_t live_idx,
	uint32_t variant_hash,
	uint32_t weapon_hash,
	int team,
	int unit_count,
	int respawn_alive,
	int respawn_delay_seconds,
	int respawn_total,
	unsigned int gametype_flags,
	int detached,
	int spawn_effect);
/* !FA:'s effect field: the spawn effect, CLOAKED and HEIGHT in one number (a
peer from before them reads the effect alone) */
#define FORGE_AI_PACKED_EFFECT_MASK 0x0F
#define FORGE_AI_PACKED_CLOAKED 0x10
#define FORGE_AI_PACKED_HEIGHT_SHIFT 5
int forge_ai_spawn_effect_packed(struct forge_ai_spawn_entry const *entry);
/* a spawn's units past its first as they start: how many it has laid out (0: one
at its point), and where unit `index` stands. Forge's preview draws them */
int forge_ai_spawn_grid_count(int32_t live_idx);
int forge_ai_spawn_grid_point(int32_t live_idx, int index, union real_point3d *point);
/* where a flying unit of a spawn with a HEIGHT takes to (unit `index`, 0 the
first, at the marker): 0 for a spawn with none, or not of flyers. Forge mode
draws them there, a line down to where they stand; they start there */
int forge_ai_spawn_hover_point(int32_t live_idx, int index, union real_point3d *point);
/* a spawn effect shown here, and told to the clients */
void forge_ai_spawn_effect_play(int effect, union real_point3d const *point);
/* a spawn's model out of reach of all but Forge's user */
void forge_ai_preview_protect(long unit_index);
void forge_network_send_spawn_effect(int effect, union real_point3d const *point);
void forge_ai_spawn_net_settings(
	int32_t live_idx,
	uint32_t *variant_hash,
	uint32_t *weapon_hash,
	struct forge_ai_spawn_entry const **settings);
/* entering Forge mode stops the AI and shows it as models on its markers,
leaving it plays the AI from them (the host's: it runs the AI) */
void forge_ai_forge_mode_changed(int forge_mode);
void forge_ai_preview_update(int32_t live_idx);
/* the unit an actor is, to draw; NONE when the level has not got it */
long forge_ai_actor_unit_definition(char const *path);
/* how a tag's object looks with no object made of it */
void forge_model_look_get(long object_definition, char const *actor_path, float colors[4][3], int colored[4],
	short *forced_shader);

/* the spawns' units as they play (the host's, each tick, in Forge and in a match
of the map): CLOAKED kept on, HEIGHT kept over the ground */
void forge_ai_units_update(void);

/* ---------- Prototypes: forge_3d_mode.c */

void forge_3d_update(float dt);
void forge_3d_apply_monitor_camera(short local_player_index);
int forge_3d_fill_live_observer_result(void *out_observer_result);
int forge_is_main_menu_active(void);
void forge_3d_draw_hud(short local_player_index, short window_x0, short window_y0, short window_x1, short window_y1, long font_index, short line_height);
int32_t forge_3d_spawn_object_at_slot(
	int32_t forced_live_idx,
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	int broadcast_net);
int32_t forge_3d_spawn_object_at(
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	int broadcast_net);
void forge_3d_move_instance(int32_t live_idx, float x, float y, float z, float yaw, float pitch, float roll, int broadcast_net);
void forge_3d_move_instance_ext(int32_t live_idx, float x, float y, float z, float yaw, float pitch, float roll, int is_held, int broadcast_net);
void forge_3d_delete_instance(int32_t live_idx, int broadcast_net);
void forge_3d_marker_usage_apply(int32_t live_idx, int16_t usage);
/* ---------- Teleporters: the 'tele' nodes and teleporter scenery. Each is a
SENDER (netgame flag 6), a RECEIVER (7) or, scenery only, OFF, on a channel
0..15. On the network, as !FU's usage and a scenery's !FH/!FS team: the
channel, with the role (0 off, 1 sender, 2 receiver) at bit 8 and
FORGE_TELE_USAGE_SET, which a palette's team (0) or a node's channel has not */
#define FORGE_TELE_CHANNELS 16
#define FORGE_TELE_USAGE_SET 0x400
#define FORGE_TELE_USAGE(role, channel) ((int16_t)(FORGE_TELE_USAGE_SET | ((role) << 8) | ((channel) & 0xFF)))
int forge_teleporter_scenery_tag(uint32_t group_tag, char const *tag_path);
int forge_3d_teleporter_net_usage(int32_t live_idx);
void forge_3d_dropship_routes_render(void (*draw_piece)(float const *from, float const *to, int kind));
int forge_is_unit_in_monitor_mode(long unit_index);
int forge_get_local_player_abs_index(void);
void forge_render_begin_monitor_visibility(void);
void forge_render_end_monitor_visibility(void);
void forge_apply_host_player_monitor_state(int player_abs_idx, int is_monitor, float x, float y, float z, float yaw, float pitch);
int32_t forge_3d_adopt_or_find_object_instance(long object_index);
void forge_init_map_scenario_entities(void);
void forge_3d_reset_session_tracking(void);
void forge_cancel_or_release_held_item(void);
int forge_has_held_item_to_cancel(void);
/* the undo history (moves, turns, properties, placings; UNDO/REDO in MAP
SETTINGS, ctrl+z/ctrl+y on a desktop): a step undone (`redo` FALSE) or
redone, TRUE when one was; how many each way; emptied with a level; an
instance moved to another slot */
int forge_3d_history_step(int redo);
int forge_3d_history_steps(int redo);
void forge_3d_history_clear(void);
void forge_3d_history_remap(int32_t from_idx, int32_t to_idx);

/* ---------- Prototypes: forge_network.c */

uint32_t forge_catalog_entry_hash(uint32_t group_tag, char const *tag_path);
struct forge_catalog_entry const *forge_find_catalog_entry_by_hash(uint32_t hash);
void forge_network_reset_session(void);
void forge_network_sync_tick(void);
void forge_network_send_monitor_mode(int is_monitor, float x, float y, float z, float yaw, float pitch);
void forge_network_send_spawn_ext(
	int32_t live_idx,
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	long host_obj_idx,
	long host_def_idx);
void forge_network_send_transform_ext(
	int32_t live_idx,
	long obj_idx,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int is_held,
	int immediate);
void forge_network_send_delete_ext(int32_t live_idx, long obj_idx);
void forge_network_send_marker_usage(int32_t live_idx, int16_t usage);
/* a placed vehicle's ALWAYS SPAWN (!FV:), and its setting here
(forge_3d_mode.c's Object Settings row) */
void forge_network_send_always_spawn(int32_t live_idx, int on);
void forge_3d_always_spawn_apply(int32_t live_idx, int on);
/* a placed scenery's or effect's size (SCALE): its object, its record, and
the other machines' (!FZ:) */
#define FORGE_SCALE_MIN 0.1f
#define FORGE_SCALE_MAX 10.0f
void forge_network_send_scale(int32_t live_idx, float scale);
void forge_3d_scale_apply(int32_t live_idx, float scale);
/* the size the next object spawned is made at (0: as made): a saved map's
OBJECT_SCALE line, a copy's */
extern float g_forge_next_spawn_scale;
/* the next object spawned is a vehicle set to ALWAYS SPAWN (read and cleared
by forge_3d_spawn_object_at_slot) */
extern int g_forge_next_spawn_always;
void forge_network_send_ai_spawn(int32_t live_idx);
/* what `forge status` says of the base tags against the host's (ae_forge_net_rules.h) */
void forge_network_tags_status(char *text, int size);
void forge_network_editors_status(char *text, int size);
/* a live slot is made on the host: its marks of who placed, held and made it are the host's afresh, not the last occupant's */
void forge_network_slot_made(int32_t live_idx);
int forge_network_intercept_chat_packet(long machine_index, char const *text, int is_from_client_to_host);
void forge_network_pre_handle_object_changes(void const *entries, short count);
void forge_network_post_handle_object_changes(void const *entries, short count);
void forge_runtime_on_map_unload(void);
void forge_runtime_markers_preload(void);
int forge_window_is_focused(void);
short forge_key_state(int vk);

int forge_get_readable_donor_map_path(char const *map_path, char *out_path, size_t out_sz);

/* ---------- 3D Snap Point Persistence */

void forge_snap_points_load_all(void);
int forge_snap_points_save_all(void);
struct forge_tag_snap_config *forge_snap_points_get_for_tag(uint32_t group_tag, char const *tag_path, int create_if_missing);
int forge_snap_points_count_for_tag(uint32_t group_tag, char const *tag_path);
void forge_snap_points_auto_generate(uint32_t group_tag, char const *tag_path, int mode);
void forge_snap_points_apply_preset_to_point(struct forge_snap_point *pt, int preset_id, int normal_dir);
void forge_snap_points_fill_normal_from_dir(struct forge_snap_point *pt, int normal_dir);
/* the box the presets and layouts measure: set by the Tag Injector's preview,
or read from a stock tag's map (0: not one, or it cannot be read) */
void forge_snap_points_bounds_set(float const bounds_min[3], float const bounds_max[3]);
int forge_snap_points_stock_tag(uint32_t group_tag, char const *tag_path, char *map_path, size_t map_path_size);
int forge_snap_points_read_stock_bounds(uint32_t group_tag, char const *tag_path);
/* Object Properties' SNAP POINTS row (forge_hooks.c) */
int forge_snap_tag_layout_step(uint32_t group_tag, char const *tag_path, int dir);
void forge_snap_tag_layout_text(uint32_t group_tag, char const *tag_path, char *text, size_t text_size);

#undef GetAsyncKeyState
#define GetAsyncKeyState(vk) forge_key_state(vk)

#ifdef __cplusplus
}
#endif

#endif /* FORGE_ENGINE_H */
