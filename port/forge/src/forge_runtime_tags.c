/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Runtime Tag Catalog, Cached Tag Folder System (Faber_data/cached_tags/),
 * Dynamic Runtime Tag Table Expansion, Multiplayer Asset Guard, Custom Tag Extraction,
 * Standalone Map Baking, Dynamic Network Version & 343 Guilty Spark Hardware Model Renderer.
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#include "cseries.h"
#include "cseries/errors.h"
#include "halo_port_limits.h" /* HALO_PORT_NETWORK_VERSION: OpenCE's protocol for standard sessions */
#include "game/game.h"
#include "ae_forge_rules.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "models/models.h"
#include "models/model_definitions.h"
#include "physics/physics_definitions.h"
#include "items/weapon_definitions.h"
#include "items/projectile_definitions.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "game/players.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_geometry.h"
#include "rasterizer/rasterizer_model_types.h"
#include "rasterizer/rasterizer_models.h"
#include "rasterizer/rasterizer_swizzle.h"
#include "render/render.h"
#include "render/render_cameras_internal.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_groups.h"
extern void *custom_edition_sounds_encode(void const *samples, long size, short compression, boolean big_endian_pcm, /* AE hook */
	long channels, long rate, long encoded_rate, unsigned long *encoded_bytes); /* AE hook */
#include "tag_files/tag_files.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "cache/cache_files.h"
#include "custom_edition_cache.h"
#include "mcc_cache.h"
#include "mcc_maps.h"
#include "mcc_package.h"
#include "tag_schema.h"
#include "social_creator.h"
#include <xtl.h>
#include "rasterizer/xbox/rasterizer_xbox.h"
#include "rasterizer/xbox/rasterizer_xbox_pixel_shader.h"
#include "zlib_prefixed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "forge_engine.h"
#include "forge_map_settings.h"
#include "forge_volumes.h"
#include "forge_ai_routes.h"

#undef fopen
#undef fclose
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef remove
#ifndef HALO_64BIT /* AE hook */
#undef malloc
#undef calloc
#undef free
#undef realloc
#endif /* AE hook */

extern unsigned long system_milliseconds(void);
extern void rasterizer_set_frustum_z(real z_near, real z_far);
extern void WINAPI D3DDevice_GetViewport(D3DVIEWPORT8 *viewport);
extern void *cache_files_tag_instances(long *count);
extern void cache_files_set_tag_instances(void *instances, long count);
extern void progress_bar_begin(boolean skip_frame_capture);
extern void progress_bar_display(real progress);
extern void progress_bar_end(void);
extern void progress_bar_eachframe(void);
extern void shell_idle(void);
extern void sound_render(void);
extern void main_present_frame(void);
extern long halo_screen_width(void);
extern void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
extern char *main_get_multiplayer_map_name(void);

void *forge_host_fopen(char const *path, char const *mode)
{
	char norm[1024];
	char host_path[1024];
	char d_xbox[1024];
	size_t i;
	FILE *f;

	if (!path || !path[0] || !mode)
		return NULL;

	for (i = 0; path[i] && i + 1 < sizeof(norm); i++)
		norm[i] = (path[i] == '\\') ? '/' : path[i];
	norm[i] = '\0';

	if ((norm[0] == 'd' || norm[0] == 'D' ||
		 norm[0] == 'h' || norm[0] == 'H' ||
		 norm[0] == 'z' || norm[0] == 'Z' ||
		 norm[0] == 't' || norm[0] == 'T' ||
		 norm[0] == 'u' || norm[0] == 'U') &&
		norm[1] == ':' && norm[2] == '/')
	{
		platform_translate_path(path, host_path, sizeof(host_path));
		return fopen(host_path, mode);
	}

	if (norm[0] == '/' ||
		(((norm[0] >= 'A' && norm[0] <= 'Z') || (norm[0] >= 'a' && norm[0] <= 'z')) && norm[1] == ':'))
	{
		return fopen(norm, mode);
	}

	snprintf(d_xbox, sizeof(d_xbox), "d:\\%s", norm);
	platform_translate_path(d_xbox, host_path, sizeof(host_path));
	f = fopen(host_path, mode);
	if (f)
		return f;

	return fopen(norm, mode);
}

int forge_host_remove(char const *path)
{
	char norm[1024];
	char host_path[1024];
	char d_xbox[1024];
	size_t i;

	if (!path || !path[0])
		return -1;

	for (i = 0; path[i] && i + 1 < sizeof(norm); i++)
		norm[i] = (path[i] == '\\') ? '/' : path[i];
	norm[i] = '\0';

	if ((norm[0] == 'd' || norm[0] == 'D' ||
		 norm[0] == 'h' || norm[0] == 'H' ||
		 norm[0] == 'z' || norm[0] == 'Z' ||
		 norm[0] == 't' || norm[0] == 'T' ||
		 norm[0] == 'u' || norm[0] == 'U') &&
		norm[1] == ':' && norm[2] == '/')
	{
		platform_translate_path(path, host_path, sizeof(host_path));
		return remove(host_path);
	}

	if (norm[0] == '/' ||
		(((norm[0] >= 'A' && norm[0] <= 'Z') || (norm[0] >= 'a' && norm[0] <= 'z')) && norm[1] == ':'))
	{
		return remove(norm);
	}

	snprintf(d_xbox, sizeof(d_xbox), "d:\\%s", norm);
	platform_translate_path(d_xbox, host_path, sizeof(host_path));
	if (remove(host_path) == 0)
		return 0;

	return remove(norm);
}

char const *forge_get_active_map_name(void)
{
	static char s_active_map[256];
	struct scenario *scen = global_scenario_try_and_get();
	char const *ce_loaded;
	char const *loaded_leaf;
	char const *mp_map;
	char const *sp_map;

	if (!scen || scen->type == 2 /* _scenario_type_main_menu */)
		return "levels\\ui\\ui";

	ce_loaded = custom_edition_cache_loaded_path();
	if (ce_loaded && ce_loaded[0])
	{
		char const *slash = strrchr(ce_loaded, '\\');
		char const *slash2 = strrchr(ce_loaded, '/');
		char const *stem;
		char clean_stem[128];
		char *dot;
		if (slash2 && (!slash || slash2 > slash))
			slash = slash2;
		stem = slash ? (slash + 1) : ce_loaded;
		snprintf(clean_stem, sizeof(clean_stem), "%s", stem);
		dot = strrchr(clean_stem, '.');
		if (dot && _stricmp(dot, ".map") == 0)
			*dot = '\0';
		if (clean_stem[0])
		{
			snprintf(s_active_map, sizeof(s_active_map), "custom_maps\\%s", clean_stem);
			return s_active_map;
		}
	}

	/* (an MCC map's level: its own name, mcc_maps\<package>\<name>) */
	if (mcc_cache_level_name())
	{
		snprintf(s_active_map, sizeof(s_active_map), "%s", mcc_cache_level_name());
		return s_active_map;
	}

	loaded_leaf = cache_file_loaded_map_name();
	mp_map = main_get_multiplayer_map_name();
	sp_map = main_get_map_name();

	if (loaded_leaf && loaded_leaf[0] && _stricmp(loaded_leaf, "ui") != 0)
	{
		if (mp_map && mp_map[0])
		{
			char const *ms = strrchr(mp_map, '\\');
			char const *ms2 = strrchr(mp_map, '/');
			char const *mleaf;
			if (ms2 && (!ms || ms2 > ms))
				ms = ms2;
			mleaf = ms ? (ms + 1) : mp_map;
			if (_stricmp(mleaf, loaded_leaf) == 0)
				return mp_map;
		}
		if (sp_map && sp_map[0])
		{
			char const *ss = strrchr(sp_map, '\\');
			char const *ss2 = strrchr(sp_map, '/');
			char const *sleaf;
			if (ss2 && (!ss || ss2 > ss))
				ss = ss2;
			sleaf = ss ? (ss + 1) : sp_map;
			if (_stricmp(sleaf, loaded_leaf) == 0)
				return sp_map;
		}
		return loaded_leaf;
	}

	if (scen->type == 1 /* _scenario_type_multiplayer */ && mp_map && mp_map[0])
		return mp_map;

	return (sp_map && sp_map[0]) ? sp_map : "levels\\ui\\ui";
}

#define fopen(p, m) ((FILE *)forge_host_fopen((p), (m)))
#define remove(p) forge_host_remove((p))
extern long bitmap_format_to_d3d_format(short format, word flags);
extern long bitmap_format_to_d3d_linear_format(short format, word flags);
extern void models_fix_transparent_part_links(void);
extern boolean custom_edition_cache_tags_loaded(void);
/* the default palette's tags: a tag pack (port/assets/tag_packs), read a tag
at a time (tag_import.h) */
#define FORGE_PACK "watanoge_forge"
struct tag_import_donor;
extern struct tag_import_donor *tag_import_pack_open(char const *pack, char const *map);
extern void tag_import_pack_close(struct tag_import_donor *donor);
extern unsigned char *tag_import_pack_tags(struct tag_import_donor *donor, unsigned long *size);
extern long tag_import_pack_read(struct tag_import_donor *donor, long offset, long size, void *buffer);
extern long tag_import_tag_count(struct tag_import_donor *donor);
extern long tag_import_tag_group(struct tag_import_donor *donor, long absolute);
extern char const *tag_import_tag_name(struct tag_import_donor *donor, long absolute);
extern void *tag_import_tag_data(struct tag_import_donor *donor, long absolute, unsigned long bytes);
extern int tag_pack_ready(char const *name);
extern void tag_pack_request(char const *name);
extern unsigned long tag_pack_content(char const *name);
extern long port_tag_packs_find(const char *name);
extern long port_tag_packs_map_count(long pack);
extern const char *port_tag_packs_map(long pack, long index);
static struct tag_import_donor *s_forge_pack_donor;
static int s_forge_pack_donor_map = -1;

static void forge_pack_donor_close(void)
{
	if (s_forge_pack_donor)
		tag_import_pack_close(s_forge_pack_donor);
	s_forge_pack_donor = NULL;
	s_forge_pack_donor_map = -1;
}

extern long port_tag_profiles_baked_build(const long *profiles, const long *tags, long count, const char *archive);
extern long port_tag_profiles_baked_build_local(const long *profiles, const long *tags, long count, const char *archive);
extern int port_tag_profiles_tag_mcc_own(long profile, long tag);
extern const char *port_tag_profiles_folder(long profile);
extern unsigned long port_tag_profiles_tag_size(long profile, long tag);
extern unsigned long port_tag_profiles_tag_signature(long profile, long tag);
extern int port_tag_profiles_baked_unpack(const char *archive, const char *id);
extern int port_tag_profiles_baked_present(const char *id);
extern long port_tag_profiles_baked_open(const char *id);
extern void port_tag_profiles_baked_scratch_path(char *path, unsigned long size);
extern long port_tag_profiles_tag_find(long profile, unsigned long group, const char *name);
extern long port_tag_profiles_scan(void);
extern long port_tag_profiles_count(void);
extern long port_tag_profiles_find(const char *folder);
extern long port_tag_profiles_tag_count(long profile);
extern unsigned long port_tag_profiles_tag_group(long profile, long tag);
extern const char *port_tag_profiles_tag_name(long profile, long tag);
extern int port_tag_profiles_tag_enabled(long profile, long tag);
extern const char *port_tag_profiles_tag_option(long profile, long tag, const char *key);
extern void halo_custom_edition_texels_channels(void const *address, unsigned char order);
extern boolean orig_mod_sounds_read(struct sound_permutation const *permutation, void *buffer);
extern void orig_render_model(
	long model_index,
	real level_of_detail_pixels,
	struct real_matrix4x3 const *node_matrices,
	char const *region_permutation_indices,
	union real_rgb_color const *change_colors,
	real const *function_values,
	struct render_lighting const *lighting,
	union real_point3d const *centroid,
	real radius,
	struct render_model_effect const *model_effect,
	long unique_identifier,
	short forced_shader_permutation_index,
	unsigned long flags);

struct forge_progress_bar_mode_mirror
{
	long state;
	boolean capture_frame;
	unsigned char padding0[3];
	IDirect3DTexture8 *texture0;
	IDirect3DTexture8 *texture1;
	boolean active;
	unsigned char padding1[3];
};
extern struct forge_progress_bar_mode_mirror progress_bar_mode;
extern real last_t;

static int s_forge_loading_active = 0;
static float s_forge_loading_map_base = 0.0f;
static float s_forge_loading_map_span = 0.0f;
static unsigned long s_forge_loading_last_ms = 0;

static void forge_xbox_loading_screen_render_frame(float progress)
{
	struct rasterizer_frame_begin_parameters frame_parameters;
	struct rasterizer_window_begin_parameters window_parameters;

	if (!global_d3d_device)
		return;
	if (progress < 0.001f)
		progress = 0.001f;
	if (progress > 1.0f)
		progress = 1.0f;

	shell_idle();
	sound_render();

	memset(&frame_parameters, 0, sizeof(frame_parameters));
	rasterizer_frame_begin(&frame_parameters);
	rasterizer_windows_begin();

	memset(&window_parameters, 0, sizeof(window_parameters));
	window_parameters.camera.position.x = 0.0f;
	window_parameters.camera.position.y = 0.0f;
	window_parameters.camera.position.z = 0.0f;
	window_parameters.camera.forward.i = 0.0f;
	window_parameters.camera.forward.j = 0.0f;
	window_parameters.camera.forward.k = 1.0f;
	window_parameters.camera.up.i = 0.0f;
	window_parameters.camera.up.j = 1.0f;
	window_parameters.camera.up.k = 0.0f;
	window_parameters.camera.mirrored = FALSE;
	window_parameters.camera.vertical_field_of_view = 1.1f;
	window_parameters.camera.viewport_bounds.x0 = 0;
	window_parameters.camera.viewport_bounds.y0 = 0;
	window_parameters.camera.viewport_bounds.x1 = (short)halo_screen_width();
	window_parameters.camera.viewport_bounds.y1 = 480;
	window_parameters.camera.window_bounds = window_parameters.camera.viewport_bounds;
	window_parameters.camera.z_near = 0.01f;
	window_parameters.camera.z_far = 1.0f;
	render_camera_build_frustum(&window_parameters.camera, NULL, &window_parameters.frustum, TRUE);
	window_parameters.rasterizer_target = 0;

	rasterizer_window_begin(&window_parameters);
	progress_bar_display(progress);
	rasterizer_window_end();
	rasterizer_windows_end();
	rasterizer_frame_end();
	main_present_frame();
	s_forge_loading_last_ms = system_milliseconds();
}

struct forge_cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name; /* AE hook */
	XPTR(void) base_address; /* AE hook */
	unsigned long unused[2];
};

struct forge_runtime_state *g_forge_state;

struct forge_runtime_state *forge_runtime_state_get(void)
{
	if (!g_forge_state)
	{
		g_forge_state = (struct forge_runtime_state *)calloc(1, sizeof(*g_forge_state));
		if (!g_forge_state)
		{
			platform_log("Forge: out of memory for its state (%lu bytes)", (unsigned long)sizeof(*g_forge_state));
			abort();
		}
	}
	return g_forge_state;
}
int g_forge_session_enabled = 0;

/* the one place a Forge session is turned on (every other assignment of the flag turns it off): see ae_forge_rules_session_may_start */
void forge_session_start(int joined_at_forge_version, int local_request)
{
	extern struct network_game_client *global_network_game_client_get(void);
	extern struct network_game_server *global_network_game_server_get(void);
	int is_client = game_connection() == _game_connection_network_client ||
		(global_network_game_client_get() != NULL && global_network_game_server_get() == NULL);

	if (ae_forge_rules_session_may_start(is_client, joined_at_forge_version, local_request))
		g_forge_session_enabled = 1;
}

long forge_get_network_version(void)
{
	return g_forge_session_enabled ? FORGE_NETWORK_VERSION : HALO_PORT_NETWORK_VERSION;
}

void forge_set_status(char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(g_forge.status_message, sizeof(g_forge.status_message), fmt, ap);
	va_end(ap);
	g_forge.status_message_until_ms = (uint32_t)system_milliseconds() + 6000u;
}

void forge_draw_screen_rect(short x0, short y0, short x1, short y1, uint32_t argb)
{
	if (!global_d3d_device || x1 <= x0 || y1 <= y0)
		return;

	_rasterizer_debug_immediate_begin_screenspace();
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_BLENDOP, D3DBLENDOP_ADD);
	pixel_shader.final_combiner_inputs_efg = 0x1400;
	rasterizer_set_pixel_shader(&pixel_shader);

	IDirect3DDevice8_Begin(global_d3d_device, D3DPT_TRIANGLEFAN);
	IDirect3DDevice8_SetVertexDataColor(global_d3d_device, 9, argb);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x0, y0);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x1, y0);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x1, y1);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x0, y1);
	IDirect3DDevice8_End(global_d3d_device);
	_rasterizer_debug_immediate_end_screenspace();
}

int forge_is_multiplayer_session(void)
{
	short conn = game_connection();
	return (conn == _game_connection_network_client || conn == _game_connection_network_server) ? 1 : 0;
}

static int is_allowed_forge_light_or_device_tag(uint32_t grp, char const *tag_path)
{
	static struct
	{
		uint32_t grp;
		char const *path;
	} const s_allowed[] = {
		/* Functioning Beam Emitters ('mach', animated emitter + beam effect + hum sound + light) */
		{ 'mach', "levels\\a30\\devices\\beam emitter\\beam emitter" },
		{ 'mach', "levels\\a30\\devices\\beam emitter\\beam emitter red" },
		/* 12 Light Fixtures ('lifi') with 3D fixture models, lens flares, and dynamic illumination */
		{ 'lifi', "levels\\a50\\devices\\light_ceiling\\bright_light_ceiling" },
		{ 'lifi', "levels\\a50\\devices\\light_ceiling\\light_ceiling" },
		{ 'lifi', "levels\\a50\\devices\\light_ceiling_large\\light_ceiling_large" },
		{ 'lifi', "levels\\a50\\devices\\light_ground\\light_ground" },
		{ 'lifi', "levels\\b30\\devices\\light fixtures\\light fixture ceiling1\\light fixture ceiling1" },
		{ 'lifi', "levels\\b30\\devices\\light fixtures\\light fixture floor1\\light fixture floor1" },
		{ 'lifi', "levels\\b30\\devices\\light fixtures\\light fixture wall1\\light fixture wall1" },
		{ 'lifi', "levels\\b40\\devices\\b40_spotlight\\b40_spotlight" },
		{ 'lifi', "levels\\c10\\devices\\ground_bright\\ground_bright" },
		{ 'lifi', "levels\\test\\carousel\\devices\\car_spotlight\\car_spotlight" },
		{ 'lifi', "levels\\test\\damnation\\devices\\dam_spotlight\\dam_spotlight" },
		{ 'lifi', "levels\\test\\damnation\\devices\\light_ceiling\\mp bright_light_ceiling" },
		/* 5 Dynamic Point Light Scenery tags ('scen') with 3D meshes and colored dynamic lights */
		{ 'scen', "levels\\c10\\scenery\\cyan point light\\light cyan dynamic" },
		{ 'scen', "levels\\c10\\scenery\\green point light\\light green dynamic" },
		{ 'scen', "levels\\c10\\scenery\\orange point light\\light orange dynamic" },
		{ 'scen', "levels\\c10\\scenery\\red point light\\light red dynamic" },
		{ 'scen', "levels\\b40\\scenery\\b40_ctorch\\ctorch" }
	};
	size_t i;

	if (!tag_path || !tag_path[0])
		return 0;
	if ((grp == 0 || grp == 'mach') &&
		(strstr(tag_path, "door") != NULL || strstr(tag_path, "Door") != NULL || strstr(tag_path, "DOOR") != NULL))
	{
		if (strstr(tag_path, "doorblinker") == NULL && strstr(tag_path, "door_blast_collision") == NULL)
			return 1;
	}
	for (i = 0; i < sizeof(s_allowed) / sizeof(s_allowed[0]); i++)
	{
		if ((grp == 0 || grp == s_allowed[i].grp) &&
			_stricmp(tag_path, s_allowed[i].path) == 0)
		{
			return 1;
		}
	}
	return 0;
}

static int is_forge_door_tag(uint32_t grp, char const *tag_path)
{
	if (!tag_path || !tag_path[0])
		return 0;
	if ((grp == 0 || grp == 'mach') &&
		(strstr(tag_path, "door") != NULL || strstr(tag_path, "Door") != NULL || strstr(tag_path, "DOOR") != NULL))
	{
		if (strstr(tag_path, "doorblinker") == NULL && strstr(tag_path, "door_blast_collision") == NULL)
			return 1;
	}
	return 0;
}

static int is_forge_light_beacon_emitter_tag(uint32_t grp, char const *tag_path)
{
	char lower_buf[260];
	size_t i;

	if (!tag_path || !tag_path[0])
		return 0;
	if (is_forge_door_tag(grp, tag_path))
		return 0;
	if (grp == 'lifi')
		return 1;
	if (grp != 0 && grp != 'scen' && grp != 'mach')
		return 0;

	for (i = 0; tag_path[i] && i + 1 < sizeof(lower_buf); i++)
	{
		char ch = tag_path[i];
		if (ch >= 'A' && ch <= 'Z')
			ch = (char)(ch - 'A' + 'a');
		lower_buf[i] = ch;
	}
	lower_buf[i] = '\0';

	if (strstr(lower_buf, "light") != NULL ||
		strstr(lower_buf, "spotlight") != NULL ||
		strstr(lower_buf, "beacon") != NULL ||
		strstr(lower_buf, "emitter") != NULL ||
		strstr(lower_buf, "ctorch") != NULL ||
		strstr(lower_buf, "torch") != NULL)
	{
		return 1;
	}
	return 0;
}

/* a scenery that is a teleporter's: one placed sends or receives (forge_3d_mode.c's forge_tele_*).
The stock one is scenery\teleporter_base\teleporter_base; its shield is an effect, left out */
int forge_teleporter_scenery_tag(uint32_t group_tag, char const *tag_path)
{
	char lower_buf[260];
	size_t i;

	if ((group_tag != 'scen' && group_tag != 'mach') || !tag_path || !tag_path[0])
		return 0;
	for (i = 0; tag_path[i] && i + 1 < sizeof(lower_buf); i++)
	{
		char ch = tag_path[i];

		lower_buf[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
	}
	lower_buf[i] = '\0';
	return strstr(lower_buf, "teleporter") != NULL && strstr(lower_buf, "shield") == NULL;
}

static void forge_lowercase(char const *in, char *out, size_t size)
{
	size_t i;

	for (i = 0; in && in[i] && i + 1 < size; i++)
		out[i] = (in[i] >= 'A' && in[i] <= 'Z') ? (char)(in[i] - 'A' + 'a') : in[i];
	out[size ? i : 0] = '\0';
}

/* a particle emitter: a scenery whose attachments make fire, sparks, steam
and the like (the EFFECTS tab), which the scenery list leaves out */
static int is_forge_particle_emitter_tag(uint32_t grp, char const *tag_path)
{
	static char const *const s_emitters[] = {
		"scenery\\emitters\\",
		"_emitter",
		"\\grav_lift_particles\\",
		"rocks_smoldering",
		"scenery\\tubewire\\",
	};
	char lower_buf[260];
	size_t k;

	if (grp != 'scen' || !tag_path || !tag_path[0])
		return 0;
	forge_lowercase(tag_path, lower_buf, sizeof(lower_buf));
	for (k = 0; k < sizeof(s_emitters) / sizeof(s_emitters[0]); k++)
	{
		if (strstr(lower_buf, s_emitters[k]) != NULL)
			return 1;
	}
	return 0;
}

/* an effect placed as itself (an effect spot: Forge plays it again and again
where it's put, forge_3d_mode.c's forge_effect_spots_update): the blood and
the Flood's gore, which no emitter scenery makes */
int is_forge_effect_spot_tag(uint32_t grp, char const *tag_path)
{
	/* (those that show something played as they are: particles, or the bits
	they throw. The blood collisions, flood bit death and the infection
	form's deaths are a decal, a sound or damage alone: DECALS has those) */
	static char const *const s_spots[] = {
		"effects\\blood aoe elite",
		"effects\\blood aoe flood",
		"effects\\blood aoe human",
		"effects\\blood aoe hunter",
		"effects\\flood bit skin explode",
		"effects\\flood bit limb explode",
		"effects\\flood bit h-head explode",
		"effects\\debris floodcombat scabs",
		"characters\\floodcarrier\\effects\\body destroyed",
		"characters\\flood_infection\\body depleted",
		"characters\\flood_captain\\punchspray",
	};
	char lower_buf[260];
	size_t k;

	if (grp != 'effe' || !tag_path || !tag_path[0])
		return 0;
	forge_lowercase(tag_path, lower_buf, sizeof(lower_buf));
	for (k = 0; k < sizeof(s_spots) / sizeof(s_spots[0]); k++)
	{
		if (!strcmp(lower_buf, s_spots[k]))
			return 1;
	}
	return 0;
}

/* ---------- The palette's origins and keyword folders

SCENERY, VEHICLES, WEAPONS, EQUIPMENT and EFFECTS show a folder of each origin
the tab has tags of, and in each the tags that share a keyword (two at least)
in a folder of their own */

int forge_catalog_category_split(int category)
{
	return category == _forge_cat_scenery || category == _forge_cat_vehicles ||
		category == _forge_cat_weapons || category == _forge_cat_equipment ||
		category == _forge_cat_effects || category == _forge_cat_decals;
}

char const *forge_catalog_origin_name(int origin)
{
	switch (origin)
	{
	case _forge_origin_custom:   return "CUSTOM";
	case _forge_origin_campaign: return "CAMPAIGN";
	case _forge_origin_map:      return "MAP";
	case _forge_origin_profile:  return "TAG PROFILE";
	default:                     return "FORGE";
	}
}

/* (a tab's keywords, the first a tag's path has deciding its folder) */
static struct
{
	uint8_t category;
	char const *word;
	char const *folder;
} const s_catalog_keywords[] = {
	{ _forge_cat_scenery, "tree", "TREES" },
	{ _forge_cat_scenery, "boulder", "ROCKS" },
	{ _forge_cat_scenery, "rock", "ROCKS" },
	{ _forge_cat_scenery, "shrub", "PLANTS" },
	{ _forge_cat_scenery, "bush", "PLANTS" },
	{ _forge_cat_scenery, "plant", "PLANTS" },
	{ _forge_cat_scenery, "fern", "PLANTS" },
	{ _forge_cat_scenery, "vine", "PLANTS" },
	{ _forge_cat_scenery, "lilypad", "PLANTS" },
	{ _forge_cat_scenery, "crashed", "WRECKS" },
	{ _forge_cat_scenery, "scenery\\vehicles\\", "WRECKS" },
	{ _forge_cat_scenery, "vehicles\\", "VEHICLE PROPS" },
	{ _forge_cat_scenery, "barricade", "BARRICADES" },
	{ _forge_cat_scenery, "beacon", "BEACONS" },
	{ _forge_cat_scenery, "scenery\\c_", "COVENANT PROPS" },
	{ _forge_cat_scenery, "metal", "COVENANT PROPS" },
	{ _forge_cat_scenery, "scenery\\h_", "HUMAN PROPS" },
	{ _forge_cat_vehicles, "warthog", "WARTHOGS" },
	{ _forge_cat_vehicles, "ghost", "GHOSTS" },
	{ _forge_cat_vehicles, "banshee", "BANSHEES" },
	{ _forge_cat_vehicles, "scorpion", "SCORPIONS" },
	{ _forge_cat_vehicles, "pelican", "PELICANS" },
	{ _forge_cat_vehicles, "dropship", "SPIRITS" },
	{ _forge_cat_vehicles, "lifepod", "LIFEPODS" },
	{ _forge_cat_vehicles, "turret", "TURRETS" },
	{ _forge_cat_vehicles, "wraith", "WRAITHS" },
	{ _forge_cat_weapons, "plasma", "COVENANT" },
	{ _forge_cat_weapons, "needler", "COVENANT" },
	{ _forge_cat_weapons, "energy sword", "COVENANT" },
	{ _forge_cat_weapons, "fuel rod", "COVENANT" },
	{ _forge_cat_equipment, "grenade", "GRENADES" },
	{ _forge_cat_equipment, "ammo", "AMMO" },
	{ _forge_cat_equipment, "powerups\\", "POWERUPS" },
	{ _forge_cat_effects, "glowingdrip", "FLOOD" },
	{ _forge_cat_effects, "blood", "BLOOD" },
	{ _forge_cat_effects, "flood", "FLOOD" },
	{ _forge_cat_effects, "fire", "FIRE" },
	{ _forge_cat_effects, "flame", "FIRE" },
	{ _forge_cat_effects, "smolder", "FIRE" },
	{ _forge_cat_effects, "spark", "SPARKS" },
	{ _forge_cat_effects, "steam", "SMOKE & STEAM" },
	{ _forge_cat_effects, "smoke", "SMOKE & STEAM" },
	{ _forge_cat_effects, "condensation", "SMOKE & STEAM" },
	{ _forge_cat_effects, "dust", "SMOKE & STEAM" },
	{ _forge_cat_effects, "tubewire", "SMOKE & STEAM" },
	{ _forge_cat_effects, "water", "WATER" },
	{ _forge_cat_effects, "spray", "WATER" },
	{ _forge_cat_effects, "drip", "WATER" },
	{ _forge_cat_effects, "bubble", "WATER" },
	{ _forge_cat_decals, "blood", "BLOOD" },
	{ _forge_cat_decals, "glowingdrip", "BLOOD" },
	{ _forge_cat_decals, "burn", "BURNS & SCORCHES" },
	{ _forge_cat_decals, "char", "BURNS & SCORCHES" },
	{ _forge_cat_decals, "glow", "BURNS & SCORCHES" },
	{ _forge_cat_decals, "bullet holes", "BULLET HOLES" },
	{ _forge_cat_decals, "treadmark", "TRACKS" },
	{ _forge_cat_decals, "decals-environment", "SIGNS & SYMBOLS" },
};

/* (keywords are numbered from 1 by their folder's first row) */
char const *forge_catalog_keyword_name(int keyword)
{
	return keyword > 0 && keyword <= (int)(sizeof(s_catalog_keywords) / sizeof(s_catalog_keywords[0])) ?
		s_catalog_keywords[keyword - 1].folder : "";
}

static uint8_t forge_catalog_keyword_of(int category, char const *tag_path)
{
	char lower_buf[260];
	size_t k;

	if (!forge_catalog_category_split(category))
		return 0;
	forge_lowercase(tag_path, lower_buf, sizeof(lower_buf));
	for (k = 0; k < sizeof(s_catalog_keywords) / sizeof(s_catalog_keywords[0]); k++)
	{
		if (s_catalog_keywords[k].category == category && strstr(lower_buf, s_catalog_keywords[k].word) != NULL)
		{
			size_t first = 0;

			/* (a folder's number is its first row's) */
			while (s_catalog_keywords[first].category != s_catalog_keywords[k].category ||
				strcmp(s_catalog_keywords[first].folder, s_catalog_keywords[k].folder))
			{
				first++;
			}
			return (uint8_t)(first + 1);
		}
	}
	return 0;
}

static int is_excluded_scenery_or_effect_tag(char const *tag_path)
{
	static char const *const s_excluded_substrings[] = {
		"emitter",
		"particle",
		"effect",
		"explosion",
		"exhaust",
		"lightning",
		"point light",
		"lens flare",
		"dynamic light",
		"nomodel",
		"hologram",
		"holo control",
		"doorblinker",
		"nutblocker",
		"door_blast_collision",
		"countdown timer",
		"temp critical",
		"teleporter_shield",
		"smoke",
		"spark",
		"steam",
		"flame",
		"flare",
		"blood_pool",
		"floor_arrow",
		"cutscene_",
		"lifepod_atmosphere_entry",
		"\\devices\\",
		"scenery\\cortana_chip\\cortana_chip",
		"scenery\\baton\\baton",
		"scenery\\tubewire\\tubewire",
		"scenery\\debris\\rocks_smoldering\\rocks_smoldering",
		"levels\\b40\\scenery\\b40_cpanel\\b40_cpanel",
		"scenery\\helmet_chip\\helmet_chip",
		"scenery\\lockpick\\lockpick",
		"levels\\c10\\scenery\\c10_smallplant\\c10_smallplant",
		"levels\\c10\\scenery\\c10_lilypad\\c10_lilypad",
		"levels\\c10\\scenery\\c10_lilypadbunch\\c10_lilypadbunch",
		"levels\\c10\\scenery\\c10_lilypadcluster\\c10_lilypadcluster",
		"levels\\c10\\scenery\\c10_plantclump\\c10_plantclump",
		"levels\\c10\\scenery\\c10_lilypadbunchb\\c10_lilypadbunchb",
		"levels\\c10\\scenery\\c10_lilypadclusterb\\c10_lilypadclusterb",
		"scenery\\baton\\baton dynamic",
		"scenery\\index\\index",
		"scenery\\index_hold\\index_hold",
		"levels\\c40\\scenery\\c40_snowbushsmall_dark\\c40_snowbushsmall_dark",
		"scenery\\hilltop\\hilltop",
		"scenery\\captain_implant\\captain_implant"
	};
	char lower_buf[260];
	size_t i, k;

	if (!tag_path || !tag_path[0])
		return 1;
	if (is_allowed_forge_light_or_device_tag(0, tag_path))
		return 0;
	if (_strnicmp(tag_path, "cinematics\\", 11) == 0)
		return 1;

	for (i = 0; tag_path[i] && i + 1 < sizeof(lower_buf); i++)
	{
		char ch = tag_path[i];
		if (ch >= 'A' && ch <= 'Z')
			ch = (char)(ch - 'A' + 'a');
		lower_buf[i] = ch;
	}
	lower_buf[i] = '\0';

	for (k = 0; k < sizeof(s_excluded_substrings) / sizeof(s_excluded_substrings[0]); k++)
	{
		if (strstr(lower_buf, s_excluded_substrings[k]) != NULL)
			return 1;
	}
	return 0;
}

static int is_excluded_weapon_or_equipment_tag(char const *tag_path)
{
	static char const *const s_excluded_weap_eqip[] = {
		"gravity rifle",
		"gravity_rifle",
		"full-spectrum",
		"full spectrum",
		"full_spectrum",
		"plasma_cannon",
		"plasma cannon",
		"fuel rod",
		"fuel_rod",
		"double speed",
		"double_speed",
		"target_laser",
		"target laser",
		"energy sword",
		"energy_sword",
		"\\flag",
		"flag\\",
		"\\ball",
		"ball\\"
	};
	char lower_buf[260];
	size_t i, k;

	if (!tag_path || !tag_path[0])
		return 1;

	for (i = 0; tag_path[i] && i + 1 < sizeof(lower_buf); i++)
	{
		char ch = tag_path[i];
		if (ch >= 'A' && ch <= 'Z')
			ch = (char)(ch - 'A' + 'a');
		lower_buf[i] = ch;
	}
	lower_buf[i] = '\0';

	for (k = 0; k < sizeof(s_excluded_weap_eqip) / sizeof(s_excluded_weap_eqip[0]); k++)
	{
		if (strstr(lower_buf, s_excluded_weap_eqip[k]) != NULL)
			return 1;
	}
	return 0;
}

int forge_catalog_entry_allowed(struct forge_catalog_entry const *entry)
{
	if (!entry)
		return 0;
	/* (the Actors tab decides its own, a Tag Injector actor's among them) */
	if (entry->category == _forge_cat_ai_squads)
		return forge_ai_palette_entry_allowed(entry);
	/* (a folder's row: forge_catalog_folders_rebuild made it of tags allowed) */
	if (entry->group_tag == FORGE_GROUP_FOLDER)
		return 1;
	/* (VOLUMES: Forge's own shapes; FIREFIGHT's spawns' row and its kinds) */
	if (entry->category == _forge_cat_volumes)
		return entry->group_tag == FORGE_GROUP_VOLUME;
	if (entry->group_tag == FORGE_GROUP_FF_MENU || entry->group_tag == 'ffsp')
		return 1;
	/* (EFFECTS: the particle emitters, which the scenery's rules leave out) */
	if (entry->category == _forge_cat_effects)
		return entry->source_cache_id == 2 || is_forge_particle_emitter_tag(entry->group_tag, entry->tag_path) ||
			is_forge_effect_spot_tag(entry->group_tag, entry->tag_path);
	/* (DECALS: any decal) */
	if (entry->category == _forge_cat_decals)
		return entry->group_tag == 'deca';
	if (entry->source_cache_id == 2)
		return 1;
	/* Allow map-local tags (source_cache_id == 0) populated by forge_SyncScenarioTagsWithLoadedMap() */
	if (entry->source_cache_id == 0)
	{
		if (entry->group_tag == 'scen' || entry->group_tag == 'vehi' ||
		    entry->group_tag == 'weap' || entry->group_tag == 'eqip' ||
		    entry->group_tag == 'garb' || entry->group_tag == 'mach' ||
		    entry->group_tag == 'ctrl' || entry->group_tag == 'lifi' ||
		    entry->group_tag == FORGE_GROUP_AI_SPAWN)
		{
			return 1;
		}
	}
	/* Disable default stock bipeds, AI characters, and control devices */
	if (entry->category == _forge_cat_bipeds ||
	    entry->category == _forge_cat_ai_squads ||
	    entry->group_tag == 'bipd' ||
	    entry->group_tag == 'actv' ||
	    entry->group_tag == 'ctrl')
	{
		return 0;
	}
	/* Doors have their own dedicated DOORS category (_forge_cat_barriers_devices) */
	if (entry->category == _forge_cat_barriers_devices)
	{
		return is_forge_door_tag(entry->group_tag, entry->tag_path);
	}
	if (is_forge_door_tag(entry->group_tag, entry->tag_path))
	{
		return (entry->category == _forge_cat_barriers_devices) ? 1 : 0;
	}
	/* Lights, beacons, and emitters have their own dedicated LIGHTS category (_forge_cat_lights) */
	if (entry->category == _forge_cat_lights)
	{
		if (!is_forge_light_beacon_emitter_tag(entry->group_tag, entry->tag_path))
			return 0;
	}
	else if (is_forge_light_beacon_emitter_tag(entry->group_tag, entry->tag_path))
	{
		return 0;
	}
	if (entry->group_tag == 'mach' || entry->group_tag == 'lifi')
	{
		if (entry->source_cache_id == 2)
			return 1;
		return is_allowed_forge_light_or_device_tag(entry->group_tag, entry->tag_path);
	}
	/* Exclude gravity rifle, full spectrum vision, fuel rod (plasma_cannon), and double speed */
	if ((entry->group_tag == 'weap' || entry->group_tag == 'eqip' ||
	     entry->category == _forge_cat_weapons || entry->category == _forge_cat_equipment) &&
	    is_excluded_weapon_or_equipment_tag(entry->tag_path))
	{
		return 0;
	}
	/* Exclude all emitters, particle effects, point lights, lens flares, and device scenery props */
	if (entry->group_tag == 'scen' && is_excluded_scenery_or_effect_tag(entry->tag_path))
	{
		return 0;
	}
	/* Allow standard vehicles and Pelican in the vehicle spawn list */
	if (entry->group_tag == 'vehi' || entry->category == _forge_cat_vehicles)
	{
		static char const *const s_allowed_vehicles[] = {
			"vehicles\\warthog\\warthog",
			"vehicles\\warthog\\mp_warthog",
			"vehicles\\rwarthog\\rwarthog",
			"vehicles\\ghost\\ghost",
			"vehicles\\banshee\\banshee",
			"vehicles\\banshee\\banshee_mp",
			"vehicles\\scorpion\\scorpion",
			"vehicles\\pelican\\pelican",
			"vehicles\\c_gun_turret\\c_gun_turret",
			"vehicles\\c_gun_turret\\c_gun_turret_mp",
			"vehicles\\c gun turret\\c gun turret",
			"vehicles\\c gun turret\\c gun turret_mp"
		};
		int v, allowed = 0;
		for (v = 0; v < (int)(sizeof(s_allowed_vehicles) / sizeof(s_allowed_vehicles[0])); v++)
		{
			if (_stricmp(entry->tag_path, s_allowed_vehicles[v]) == 0)
			{
				allowed = 1;
				break;
			}
		}
		if (!allowed)
			return 0;
	}
	return 1;
}

static void make_short_display_name(char const *tag_path, char const *prefix, char *out, size_t out_sz)
{
	char const *leaf = tag_path;
	char const *p;
	for (p = tag_path; *p; p++)
	{
		if (*p == '\\' || *p == '/')
			leaf = p + 1;
	}
	/* (object_port_wreck_look: not the vehicles of those names) */
	if (_stricmp(tag_path, "scenery\\vehicles\\ghost\\ghost") == 0)
		leaf = "ghost wreck";
	else if (_stricmp(tag_path, "scenery\\vehicles\\wraith\\wraith") == 0)
		leaf = "wraith wreck";
	if (prefix && (_stricmp(prefix, "[c]") == 0))
		snprintf(out, out_sz, "[C] %s", leaf);
	else if (prefix && (_strnicmp(prefix, "[c] ", 4) == 0))
		snprintf(out, out_sz, "[C] %s", prefix + 4);
	else if (prefix && (_stricmp(prefix, "[map]") == 0))
		snprintf(out, out_sz, "[MAP] %s", leaf);
	else if (prefix && (_strnicmp(prefix, "[map] ", 6) == 0))
		snprintf(out, out_sz, "[MAP] %s", prefix + 6);
	else if (prefix && prefix[0] == '[')
		snprintf(out, out_sz, "%s", prefix);
	else if (prefix && prefix[0])
		snprintf(out, out_sz, "%s %s", prefix, leaf);
	else
		snprintf(out, out_sz, "%s", leaf);
}

static void add_catalog_entry(
	uint32_t group_tag,
	uint8_t category,
	uint8_t source_cache_id,
	char const *tag_path,
	char const *display_prefix);

/* the origin the entries now added are of (enum forge_catalog_origin) */
static uint8_t s_catalog_origin = _forge_origin_builtin;

/* a rank's name, its group's row if it is the first, and RANDOM once the group
has a choice */
static int s_catalog_row_replaced = -1;

static void add_ai_group_rows(struct forge_catalog_entry *rank, int kind)
{
	int group = forge_ai_kind_group(kind);
	int ranks = 0, has_group = 0, i;
	uint8_t category = rank->category;
	char path[64];

	if (kind >= 0)
		snprintf(rank->display_name, sizeof(rank->display_name), "%s", forge_ai_kind_name(kind));
	for (i = 0; i < g_forge.catalog_count; i++)
	{
		struct forge_catalog_entry const *row = &g_forge.catalog[i];

		if (row->group_tag == FORGE_GROUP_AI_MENU && forge_ai_row_group(row) == group)
			has_group = 1;
		else if (row->group_tag == FORGE_GROUP_AI_SPAWN && !forge_ai_row_is_random(row) && forge_ai_row_group(row) == group)
			ranks++;
	}
	/* (source 1, as the spawns' rows: 0 is a map's own, which each map's load
	strips, and they stand for no tag of any map) */
	if (!has_group)
	{
		int before = g_forge.catalog_count;

		snprintf(path, sizeof(path), "%s%d", FORGE_AI_GROUP_PATH, group);
		add_catalog_entry(FORGE_GROUP_AI_MENU, category, 1, path, NULL);
		if (g_forge.catalog_count > before)
		{
			snprintf(g_forge.catalog[before].display_name, sizeof(g_forge.catalog[before].display_name), "%s",
				forge_ai_group_name(group));
		}
	}
	if (ranks >= 2 && kind >= 0)
	{
		int before = g_forge.catalog_count;

		snprintf(path, sizeof(path), "%s%d", FORGE_AI_RANDOM_PATH, group);
		add_catalog_entry(FORGE_GROUP_AI_SPAWN, category, 1, path, NULL);
		if (g_forge.catalog_count > before)
		{
			snprintf(g_forge.catalog[before].display_name, sizeof(g_forge.catalog[before].display_name), "RANDOM");
		}
	}
}

static void add_catalog_entry(
	uint32_t group_tag,
	uint8_t category,
	uint8_t source_cache_id,
	char const *tag_path,
	char const *display_prefix)
{
	int i;
	int ai_kind = FORGE_AI_KIND_HIDDEN;
	struct forge_catalog_entry *e;

	if (!tag_path || !tag_path[0] || g_forge.catalog_count >= FORGE_MAX_CATALOG_ENTRIES)
		return;
	/* (a teleporter's scenery, wherever it is found, in TELEPORTERS: placed, it is one) */
	if (category == _forge_cat_scenery && forge_teleporter_scenery_tag(group_tag, tag_path))
		category = _forge_cat_teleporters;

	/* (an actor's rank once, whatever its weapon: forge_ai.c's kinds) */
	if (group_tag == FORGE_GROUP_AI_SPAWN && strncmp(tag_path, FORGE_AI_RANDOM_PATH, strlen(FORGE_AI_RANDOM_PATH)))
	{
		ai_kind = forge_ai_kind_of(tag_path);
		if (ai_kind == FORGE_AI_KIND_HIDDEN)
			return;
		if (ai_kind >= 0)
		{
			for (i = 0; i < g_forge.catalog_count; i++)
			{
				if (g_forge.catalog[i].group_tag == group_tag &&
					forge_ai_kind_of(g_forge.catalog[i].tag_path) == ai_kind)
				{
					/* (armed if one of the rank's tags is) */
					if (forge_ai_tag_is_poor(g_forge.catalog[i].tag_path) && !forge_ai_tag_is_poor(tag_path))
					{
						snprintf(g_forge.catalog[i].tag_path, sizeof(g_forge.catalog[i].tag_path), "%s", tag_path);
						s_catalog_row_replaced = i;
					}
					return;
				}
			}
		}
	}

	for (i = 0; i < g_forge.catalog_count; i++)
	{
		if (g_forge.catalog[i].group_tag == group_tag &&
		    g_forge.catalog[i].category == category &&
		    _stricmp(g_forge.catalog[i].tag_path, tag_path) == 0 &&
		    /* (a split tab has it once in each origin's folder) */
		    (!forge_catalog_category_split(category) || g_forge.catalog[i].origin == s_catalog_origin ||
		     /* (the props' and a profile's are one tag of the level) */
		     (s_catalog_origin != _forge_origin_campaign && s_catalog_origin != _forge_origin_map &&
		      g_forge.catalog[i].origin != _forge_origin_campaign && g_forge.catalog[i].origin != _forge_origin_map)))
		{
			if (source_cache_id == 2 && g_forge.catalog[i].source_cache_id != 2)
			{
				g_forge.catalog[i].source_cache_id = 2;
				make_short_display_name(tag_path, display_prefix, g_forge.catalog[i].display_name, sizeof(g_forge.catalog[i].display_name));
			}
			return;
		}
	}

	e = &g_forge.catalog[g_forge.catalog_count++];
	e->group_tag = group_tag;
	e->category = category;
	e->source_cache_id = source_cache_id;
	e->pad = 0;
	e->origin = s_catalog_origin;
	e->keyword = forge_catalog_keyword_of(category, tag_path);
	e->preset = 0;
	snprintf(e->tag_path, sizeof(e->tag_path), "%s", tag_path);
	make_short_display_name(tag_path, display_prefix, e->display_name, sizeof(e->display_name));
	if (ai_kind != FORGE_AI_KIND_HIDDEN)
		add_ai_group_rows(e, ai_kind);
}

static uint16_t forge_pack_map_mark(char const *name);

static void scan_cached_tags_manifest(char const *manifest_path)
{
	FILE *f = fopen(manifest_path, "r");
	char line[512];
	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
	{
		char grp_str[16];
		int cat_id = 0;
		int src_id = 0;
		char tag_path[256];
		char custom_label[128];
		char prefix_buf[140];
		uint32_t grp = 0;
		char *nl;
		char *bar;

		nl = strchr(line, '\r');
		if (nl) *nl = '\0';
		nl = strchr(line, '\n');
		if (nl) *nl = '\0';
		if (line[0] == '#' || line[0] == '\0')
			continue;

		custom_label[0] = '\0';
		if (sscanf(line, "%4s|%d|%d|%255[^\n]", grp_str, &cat_id, &src_id, tag_path) == 4)
		{
			bar = strchr(tag_path, '|');
			if (bar)
			{
				*bar = '\0';
				snprintf(custom_label, sizeof(custom_label), "%s", bar + 1);
			}
			if (strlen(grp_str) == 4 && tag_path[0] != '\0')
			{
				grp = ((uint32_t)(uint8_t)grp_str[0] << 24) |
				      ((uint32_t)(uint8_t)grp_str[1] << 16) |
				      ((uint32_t)(uint8_t)grp_str[2] << 8) |
				      ((uint32_t)(uint8_t)grp_str[3]);
				if (grp == 'ctrl' && src_id != 2)
					continue;
				if ((grp == 'mach' || grp == 'lifi') &&
					src_id != 2 && src_id != 1 &&
					!is_allowed_forge_light_or_device_tag(grp, tag_path))
				{
					continue;
				}
				if (src_id != 2 && grp == 'scen' && is_excluded_scenery_or_effect_tag(tag_path) &&
					!is_forge_particle_emitter_tag(grp, tag_path))
				{
					continue;
				}
				if (src_id != 2 && grp == 'vehi' && _stricmp(tag_path, "vehicles\\wraith\\wraith") == 0)
					continue;
				if (src_id != 2 && (grp == 'weap' || grp == 'eqip') && is_excluded_weapon_or_equipment_tag(tag_path))
					continue;

				/* Map group tag directly to the separated WEAPONS vs EQUIPMENT vs VEHICLES vs SCENERY vs DOORS vs LIGHTS tabs */
				if (grp == 'effe')
				{
					if (!is_forge_effect_spot_tag(grp, tag_path))
						continue;
					cat_id = _forge_cat_effects;
				}
				else if (grp == 'deca')
					cat_id = _forge_cat_decals;
				else if (grp == 'weap')
					cat_id = _forge_cat_weapons;
				else if (grp == 'eqip' || grp == 'garb')
					cat_id = _forge_cat_equipment;
				else if (grp == 'vehi')
					cat_id = _forge_cat_vehicles;
				else if (is_forge_door_tag(grp, tag_path) || grp == 'ctrl')
					cat_id = _forge_cat_barriers_devices;
				else if (is_forge_particle_emitter_tag(grp, tag_path))
					cat_id = _forge_cat_effects;
				else if (is_forge_light_beacon_emitter_tag(grp, tag_path))
					cat_id = _forge_cat_lights;
				else if (grp == 'scen' || grp == 'mach' || grp == 'lifi')
					cat_id = _forge_cat_scenery;
				else if (grp == 'bipd')
					cat_id = _forge_cat_bipeds;
				else if (grp == 'actv' || grp == FORGE_GROUP_AI_SPAWN)
					cat_id = _forge_cat_ai_squads;
				else
					continue;

				if (src_id == 2)
				{
					if (custom_label[0])
					{
						if (_strnicmp(custom_label, "[c] ", 4) == 0)
							snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_label + 4);
						else if (_strnicmp(custom_label, "[c]", 3) == 0)
							snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_label + 3);
						else
							snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_label);
					}
					else
					{
						snprintf(prefix_buf, sizeof(prefix_buf), "[C]");
					}
					add_catalog_entry(grp, (uint8_t)cat_id, 2, tag_path, prefix_buf);
				}
				else
				{
					int before = g_forge.catalog_count;

					add_catalog_entry(
						grp,
						(uint8_t)cat_id,
						src_id ? (uint8_t)src_id : 1u,
						tag_path,
						src_id ? (grp == 'actv' ? "[CUSTOM AI]" : "[CUSTOM]") : (grp == 'actv' ? "[AI]" : ""));
					/* (the pack's map it is first in) */
					if (g_forge.catalog_count > before)
						g_forge.catalog[before].pad = forge_pack_map_mark(custom_label);
					else if (s_catalog_row_replaced >= 0)
						g_forge.catalog[s_catalog_row_replaced].pad = forge_pack_map_mark(custom_label);
					s_catalog_row_replaced = -1;
				}
			}
		}
	}
	fclose(f);
}

/* the palette of the pack: the tags of its maps a Forge map may place, each
with the map it is first in. Reading every map's tags takes a while, so what
is found is kept in PALETTE_FILE, whose first line is the pack's content */
#define FORGE_PACK_PALETTE_FILE "Faber_data/cached_tags/pack_palette.txt"

extern int forge_campaign_actor_wanted(char const *name);

static void forge_pack_palette_add(struct tag_import_donor *donor, char const *map, FILE *out)
{
	long count = tag_import_tag_count(donor);
	long i;

	for (i = 0; i < count; i++)
	{
		uint32_t grp = (uint32_t)tag_import_tag_group(donor, i);
		char const *tname = tag_import_tag_name(donor, i);
		char grp_str[5];
		int cat_id = -1;

		if (!tname || !tname[0])
			continue;
		/* (an effect spot, a decal: no object nor model; before the scenery's
		rules, which leave out every path with "effect" in it) */
		if (grp == 'effe' || grp == 'deca')
		{
			if (grp == 'deca' || is_forge_effect_spot_tag(grp, tname))
				fprintf(out, "%s|%d|0|%s|%s\n", grp == 'deca' ? "deca" : "effe",
					grp == 'deca' ? (int)_forge_cat_decals : (int)_forge_cat_effects, tname, map);
			continue;
		}
		if (is_excluded_scenery_or_effect_tag(tname) && !is_forge_particle_emitter_tag(grp, tname))
			continue;
		/* (an actor is no object, and places as a spawn: forge_ai.c) */
		if (grp == 'actv')
		{
			if (forge_campaign_actor_wanted(tname))
				fprintf(out, "aisp|%d|0|%s|%s\n", (int)_forge_cat_ai_squads, tname, map);
			continue;
		}
		if (grp == 'weap')
		{
			if (!is_excluded_weapon_or_equipment_tag(tname))
				cat_id = _forge_cat_weapons;
		}
		else if (grp == 'eqip')
		{
			if (!is_excluded_weapon_or_equipment_tag(tname))
				cat_id = _forge_cat_equipment;
		}
		else if (grp == 'vehi')
		{
			if (_stricmp(tname, "vehicles\\wraith\\wraith") != 0)
				cat_id = _forge_cat_vehicles;
		}
		else if (is_forge_door_tag(grp, tname))
			cat_id = _forge_cat_barriers_devices;
		else if (is_forge_particle_emitter_tag(grp, tname))
			cat_id = _forge_cat_effects;
		else if (is_forge_light_beacon_emitter_tag(grp, tname) &&
				 (grp == 'scen' || is_allowed_forge_light_or_device_tag(grp, tname)))
			cat_id = _forge_cat_lights;
		else if (grp == 'scen')
			cat_id = _forge_cat_scenery;
		else if ((grp == 'mach' || grp == 'lifi') && is_allowed_forge_light_or_device_tag(grp, tname))
			cat_id = _forge_cat_scenery;
		if (cat_id < 0)
			continue;

		/* (an object with a model, or a light of the list) */
		if (!is_allowed_forge_light_or_device_tag(grp, tname))
		{
			uint8_t const *object = (uint8_t const *)tag_import_tag_data(donor, i, 0x38);
			int32_t mod_idx = object ? *(int32_t const *)(object + 0x34) : -1;

			if (mod_idx == -1 || (mod_idx & 0xFFFF) >= count)
				continue;
		}
		grp_str[0] = (char)((grp >> 24) & 0xFF);
		grp_str[1] = (char)((grp >> 16) & 0xFF);
		grp_str[2] = (char)((grp >> 8) & 0xFF);
		grp_str[3] = (char)(grp & 0xFF);
		grp_str[4] = '\0';
		fprintf(out, "%s|%d|0|%s|%s\n", grp_str, cat_id, tname, map);
		if (grp == 'mach' && _stricmp(tname, "levels\\a30\\devices\\beam emitter\\beam emitter") == 0)
			fprintf(out, "mach|%d|0|%s red|%s\n", cat_id, tname, map);
	}
}

/* PALETTE_FILE made, or kept when it is of this pack */
static int forge_pack_palette_ready(void)
{
	long pack = port_tag_packs_find(FORGE_PACK);
	unsigned long content = tag_pack_content(FORGE_PACK);
	char first[64];
	char want[64];
	FILE *f;
	long map;

	if (pack < 0 || !tag_pack_ready(FORGE_PACK))
		return 0;
	/* (v2: with the particle emitters; v3 Keyes' Flood form and the blood and
	Flood effects; v4 those effects again, which v3 left out, and the decals;
	v5 the effects that show nothing left out) */
	snprintf(want, sizeof(want), "# forge palette %08lx v5\n", content);
	f = fopen(FORGE_PACK_PALETTE_FILE, "r");
	if (f)
	{
		int same = fgets(first, sizeof(first), f) && !strcmp(first, want);

		fclose(f);
		if (same)
			return 1;
	}
#if FORGE_HOST_FILES
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);
#endif
	f = fopen(FORGE_PACK_PALETTE_FILE, "w");
	if (!f)
	{
		platform_log("Forge: could not write %s", FORGE_PACK_PALETTE_FILE);
		return 0;
	}
	fputs(want, f);
	for (map = 0; map < port_tag_packs_map_count(pack); map++)
	{
		char const *name = port_tag_packs_map(pack, map);
		struct tag_import_donor *donor = tag_import_pack_open(FORGE_PACK, name);

		if (!donor)
			continue;
		forge_pack_palette_add(donor, name, f);
		tag_import_pack_close(donor);
	}
	fclose(f);
	return 1;
}

/* the map index of the pack a palette line names, plus one (the catalog
entry's pad), 0 when the line has none */
static uint16_t forge_pack_map_mark(char const *name)
{
	long pack = port_tag_packs_find(FORGE_PACK);
	long map;

	for (map = 0; name && name[0] && pack >= 0 && map < port_tag_packs_map_count(pack); map++)
	{
		if (!_stricmp(port_tag_packs_map(pack, map), name))
			return (uint16_t)(map + 1);
	}
	return 0;
}

/* forge_runtime_tags: whether the pack is wanted at start-up
(tag_pack_conditions.c): once the Forge menu has been opened here, its
palette or a Forge save says so */
int forge_pack_wanted(void)
{
	FILE *f = fopen(FORGE_PACK_PALETTE_FILE, "r");

	if (f)
		fclose(f);
	return f != NULL || (g_forge_state && g_forge.initialized);
}

/* the profile the player chose for this Forge session (its folder: the TAG
PROFILE screen after the map; "" for none), which the level plays; and once a
level has loaded, what it does play, on every machine: a client's the host's
share, and a saved map's bake */
extern long tag_injector_level_profile(void);
extern long tag_injector_level_baked_profile(void);
/* (the game's own props, which every Forge level plays: tools/prop_assets.py) */
extern long tag_injector_level_props_profile(void);
static char s_forge_profile_folder[128];

void forge_tag_profile_set(char const *folder)
{
	snprintf(s_forge_profile_folder, sizeof(s_forge_profile_folder), "%s", folder ? folder : "");
}

char const *forge_tag_profile_folder(void)
{
	return s_forge_profile_folder;
}

static long forge_chosen_profile(void)
{
	long playing = tag_injector_level_profile();

	if (playing != NONE)
		return playing;
	return s_forge_profile_folder[0] ? port_tag_profiles_find(s_forge_profile_folder) : NONE;
}

static long forge_baked_profile(void)
{
	return tag_injector_level_baked_profile();
}

/* the chosen profile's tags, in the palette as [C] tags */
static void forge_profile_catalog_add(long profile)
{
	long t;
	long count = profile == NONE ? 0 : port_tag_profiles_tag_count(profile);

	for (t = 0; t < count; t++)
	{
		uint32_t grp = (uint32_t)port_tag_profiles_tag_group(profile, t);
		char const *tag_path = port_tag_profiles_tag_name(profile, t);
		char const *custom_name;
		char prefix_buf[140];
		uint8_t cat_id;

		if (!port_tag_profiles_tag_enabled(profile, t) || !tag_path || !tag_path[0])
			continue;
		/* (the game's props' Firefight copies: a drop area, a resupply rack) */
		if (_strnicmp(tag_path, "firefight\\", 10) == 0)
			cat_id = _forge_cat_firefight;
		else if (grp == 'weap')
			cat_id = _forge_cat_weapons;
		else if (grp == 'eqip')
			cat_id = _forge_cat_equipment;
		else if (grp == 'vehi')
			cat_id = _forge_cat_vehicles;
		else if (grp == 'bipd')
			cat_id = _forge_cat_bipeds;
		else if (grp == 'actv')
			cat_id = _forge_cat_ai_squads;
		else if (grp == 'lifi')
			cat_id = _forge_cat_lights;
		else if (grp == 'mach' || grp == 'ctrl')
			cat_id = is_forge_light_beacon_emitter_tag(grp, tag_path) ? _forge_cat_lights : _forge_cat_barriers_devices;
		else if (grp == 'scen')
			cat_id = is_forge_particle_emitter_tag(grp, tag_path) ? _forge_cat_effects :
				is_forge_light_beacon_emitter_tag(grp, tag_path) ? _forge_cat_lights : _forge_cat_scenery;
		/* (a map's effects and decals the Tag Injector took out: placed as the
		pack's are, an effect spot and a decal with no object) */
		else if (grp == 'effe')
			cat_id = _forge_cat_effects;
		else if (grp == 'deca')
			cat_id = _forge_cat_decals;
		else
			continue;
		custom_name = port_tag_profiles_tag_option(profile, t, "name");
		if (cat_id == _forge_cat_firefight && custom_name && custom_name[0])
			snprintf(prefix_buf, sizeof(prefix_buf), "[FF] %s", custom_name);
		else if (custom_name && custom_name[0])
			snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s",
				_strnicmp(custom_name, "[c] ", 4) == 0 ? custom_name + 4 :
				_strnicmp(custom_name, "[c]", 3) == 0 ? custom_name + 3 : custom_name);
		else
			snprintf(prefix_buf, sizeof(prefix_buf), "[C]");
		add_catalog_entry(grp, cat_id, 2, tag_path, prefix_buf);
	}
}

/* the folder rows of the split tabs (FORGE_GROUP_FOLDER), made again from the
tags there are: an origin's that has any tag of the tab, and a keyword's that
two or more of an origin's tags share. Their path names their tab, origin and
keyword; they go after the tags, so the rows' order is the tags' */
static void forge_catalog_folders_rebuild(void)
{
	int r, w = 0;
	int category;

	for (r = 0; r < g_forge.catalog_count; r++)
	{
		if (g_forge.catalog[r].group_tag != FORGE_GROUP_FOLDER)
		{
			if (w != r)
				g_forge.catalog[w] = g_forge.catalog[r];
			w++;
		}
	}
	g_forge.catalog_count = w;

	for (category = 0; category < NUMBER_OF_FORGE_PALETTE_CATEGORIES; category++)
	{
		int origin;

		if (!forge_catalog_category_split(category))
			continue;
		for (origin = _forge_origin_custom; origin < NUMBER_OF_FORGE_CATALOG_ORIGINS; origin++)
		{
			int keyword_counts[256];
			int any = 0;
			int k;

			memset(keyword_counts, 0, sizeof(keyword_counts));
			for (r = 0; r < w; r++)
			{
				struct forge_catalog_entry const *e = &g_forge.catalog[r];

				if (e->category != category || e->origin != origin || !forge_catalog_entry_allowed(e))
					continue;
				any = 1;
				keyword_counts[e->keyword]++;
			}
			if (!any || g_forge.catalog_count >= FORGE_MAX_CATALOG_ENTRIES)
				continue;
			for (k = 0; k < 256 && g_forge.catalog_count < FORGE_MAX_CATALOG_ENTRIES; k++)
			{
				struct forge_catalog_entry *e;

				/* (the origin's own row first, then its keywords' of two or more) */
				if (k != 0 && keyword_counts[k] < 2)
					continue;
				e = &g_forge.catalog[g_forge.catalog_count++];
				memset(e, 0, sizeof(*e));
				e->group_tag = FORGE_GROUP_FOLDER;
				e->category = (uint8_t)category;
				e->source_cache_id = 1;
				e->origin = (uint8_t)origin;
				e->keyword = (uint8_t)k;
				snprintf(e->tag_path, sizeof(e->tag_path), FORGE_FOLDER_PATH "%d\\%d\\%d", category, origin, k);
				snprintf(e->display_name, sizeof(e->display_name), "%s",
					k ? forge_catalog_keyword_name(k) : forge_catalog_origin_name(origin));
			}
		}
	}
}

/* a FIREFIGHT SPAWNS row: a Firefight marker placed with `usage` */
static void add_ff_preset(char const *name, char const *display_name, int usage)
{
	char path[FORGE_TAG_PATH_MAX];
	int before = g_forge.catalog_count;

	snprintf(path, sizeof(path), "%s%s", FORGE_FF_PRESET_PATH, name);
	add_catalog_entry('ffsp', _forge_cat_firefight, 1, path, display_name);
	if (g_forge.catalog_count > before)
		g_forge.catalog[before].preset = (uint16_t)usage;
}

int forge_runtime_is_particle_emitter(uint32_t group_tag, char const *tag_path)
{
	return is_forge_particle_emitter_tag(group_tag, tag_path);
}

void forge_runtime_reload_catalogs(void)
{
	g_forge.catalog_count = 0;

	/* Built-in Spawns & Teleporters (source_cache_id = 1; 0 is reserved for transient [MAP] local tags) */
	add_catalog_entry('spwn', _forge_cat_spawns, 1, "spawn\\player_red", "[SPAWN] Red Team Spawn");
	add_catalog_entry('spwn', _forge_cat_spawns, 1, "spawn\\player_blue", "[SPAWN] Blue Team Spawn");
	add_catalog_entry('spwn', _forge_cat_spawns, 1, "spawn\\player_ffa", "[SPAWN] FFA / Neutral Spawn");
	/* (FIREFIGHT SPAWNS opens its kinds of marker, each placing its usage) */
	add_catalog_entry(FORGE_GROUP_FF_MENU, _forge_cat_firefight, 1, FORGE_FF_MENU_PATH, "[FF] Firefight Spawns");
	add_ff_preset("player_spawn", "[FF] Player Spawn", FORGE_FF_PACK_USAGE(_forge_ff_spawn_player_spawn, 0, 0));
	add_ff_preset("player_vehicle", "[FF] Player Vehicle", FORGE_FF_PACK_USAGE(_forge_ff_spawn_player_vehicle, 0, 0));
	add_ff_preset("infantry_ai", "[FF] Infantry AI", FORGE_FF_PACK_USAGE(_forge_ff_spawn_ai_infantry_only, 0, 0));
	add_ff_preset("vehicle_ai", "[FF] Vehicle AI", FORGE_FF_PACK_USAGE(_forge_ff_spawn_enemy_ghost_only, 0, 0));
	add_ff_preset("enemy_drop", "[FF] Enemy Drop", FORGE_FF_PACK_USAGE(FF_DROP_SPAWN_TYPE_ANY, 0, 0));
	/* (the AI's routes, in the Actors tab: forge_ai_routes.c) */
	add_catalog_entry(FORGE_GROUP_AI_ROUTE, _forge_cat_ai_squads, 1, FORGE_AI_ROUTE_PATH, "[AI] AI Route");
	/* (damage and kill volumes: forge_volumes.c) */
	add_catalog_entry(FORGE_GROUP_VOLUME, _forge_cat_volumes, 1, FORGE_VOLUME_BOX_PATH, "[VOLUME] Box Volume");
	add_catalog_entry(FORGE_GROUP_VOLUME, _forge_cat_volumes, 1, FORGE_VOLUME_CYLINDER_PATH, "[VOLUME] Cylinder Volume");
	add_catalog_entry(FORGE_GROUP_VOLUME, _forge_cat_volumes, 1, FORGE_VOLUME_SPHERE_PATH, "[VOLUME] Sphere Volume");
	/* (the teleporter scenery joins these: add_catalog_entry) */
	add_catalog_entry('tele', _forge_cat_teleporters, 1, "teleporter\\origin", "[NODE] Sender");
	add_catalog_entry('tele', _forge_cat_teleporters, 1, "teleporter\\destination", "[NODE] Receiver");

	/* Built-in Netgame Objectives */
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\ctf_flag_red", "[CTF] Red Flag Base");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\ctf_flag_blue", "[CTF] Blue Flag Base");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\oddball_spawn", "[ODDBALL] Ball Spawn Marker");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\koth_hill", "[KOTH] Hill Marker");
	add_catalog_entry('flag', _forge_cat_firefight, 1, "objective\\firefight_generator", "[FF] Generator");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\race_track", "[RACE] Race Marker / Checkpoint");

	/* 1. The tags of the map's bake, the game's props and the chosen Tag Injector profile (at the top of each
	category), then the pack's */
	s_catalog_origin = _forge_origin_custom;
	forge_profile_catalog_add(tag_injector_level_props_profile());
	s_catalog_origin = _forge_origin_profile;
	forge_profile_catalog_add(forge_baked_profile());
	forge_profile_catalog_add(forge_chosen_profile());
	s_catalog_origin = _forge_origin_campaign;
	if (forge_pack_palette_ready())
		scan_cached_tags_manifest(FORGE_PACK_PALETTE_FILE);
	s_catalog_origin = _forge_origin_builtin;

	/* 2. Stable-sort custom entries (source_cache_id == 2) to the front of g_forge.catalog */
	if (g_forge.catalog_count > 10)
	{
		struct forge_catalog_entry *sorted =
			(struct forge_catalog_entry *)malloc((size_t)g_forge.catalog_count * sizeof(struct forge_catalog_entry));
		if (sorted)
		{
			int i, out_n = 0;
			for (i = 0; i < g_forge.catalog_count; i++)
			{
				if (g_forge.catalog[i].source_cache_id == 2)
					sorted[out_n++] = g_forge.catalog[i];
			}
			for (i = 0; i < g_forge.catalog_count; i++)
			{
				if (g_forge.catalog[i].source_cache_id == 1)
					sorted[out_n++] = g_forge.catalog[i];
			}
			for (i = 0; i < g_forge.catalog_count; i++)
			{
				if (g_forge.catalog[i].source_cache_id != 2 && g_forge.catalog[i].source_cache_id != 1)
					sorted[out_n++] = g_forge.catalog[i];
			}
			memcpy(g_forge.catalog, sorted, (size_t)out_n * sizeof(struct forge_catalog_entry));
			free(sorted);
		}
	}
	forge_catalog_folders_rebuild();
}

#ifdef _WIN32
int __stdcall CreateDirectoryA(char const *lpPathName, void *lpSecurityAttributes);
#endif

static int is_xbox_version5_map_file(char const *map_path)
{
	FILE *fp = fopen(map_path, "rb");
	uint32_t hdr[2];
	if (!fp)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr))
	{
		fclose(fp);
		return 0;
	}
	fclose(fp);
	return (hdr[0] == 0x68656164u /* 'head' */ && hdr[1] == 5u) ? 1 : 0;
}

static int ensure_xbox_map_decompressed_cache(
	char const *map_path,
	char *out_cache_path,
	size_t out_cache_sz);

int forge_get_readable_donor_map_path(char const *map_path, char *out_path, size_t out_sz)
{
	if (!map_path || !map_path[0] || !out_path || out_sz == 0)
		return 0;
	if (is_xbox_version5_map_file(map_path))
		return ensure_xbox_map_decompressed_cache(map_path, out_path, out_sz);
	snprintf(out_path, out_sz, "%s", map_path);
	return 1;
}

/*
 * Checks if a pre-existing decompressed Xbox cache file already exists for `map_path`
 * in OpenCE's Xbox hard-drive cache (z:\cache000.map..z:\cache005.map or z:\cache_donor.map)
 * or if `map_path` itself is already uncompressed on disk.
 * If not, decompresses the Xbox v5 map payload using zlib into OpenCE's z:\cache_donor.map.
 */
static int ensure_xbox_map_decompressed_cache(
	char const *map_path,
	char *out_cache_path,
	size_t out_cache_sz)
{
	FILE *in_fp;
	uint8_t hdr[2048];
	uint32_t head_magic, map_version, decomp_len, tag_off, tag_sz;
	char map_internal_name[36];
	int slot;

	if (!map_path || !map_path[0] || !out_cache_path || out_cache_sz == 0)
		return 0;
	out_cache_path[0] = 0;

	in_fp = fopen(map_path, "rb");
	if (!in_fp)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), in_fp) != sizeof(hdr))
	{
		fclose(in_fp);
		return 0;
	}

	head_magic = *(uint32_t const *)(hdr + 0x00);
	map_version = *(uint32_t const *)(hdr + 0x04);
	decomp_len = *(uint32_t const *)(hdr + 0x08);
	tag_off = *(uint32_t const *)(hdr + 0x10);
	tag_sz = *(uint32_t const *)(hdr + 0x14);
	memset(map_internal_name, 0, sizeof(map_internal_name));
	memcpy(map_internal_name, hdr + 0x20, 31);

	if (head_magic != 0x68656164u || map_version != 5u ||
		tag_off < 2048u || tag_sz < 64u || tag_sz > 64u * 1024u * 1024u)
	{
		fclose(in_fp);
		return 0;
	}

	/* 1. Check if OpenCE's Xbox hard-drive cache (z:\cache000.map .. z:\cache005.map or z:\cache_donor.map)
	 * already holds this decompressed Xbox map; if so, use it directly without duplicating! */
	for (slot = 0; slot <= 6; slot++)
	{
		char z_xbox[64];
		char z_path[512];
		FILE *zf;
		if (slot < 6)
			snprintf(z_xbox, sizeof(z_xbox), "z:\\cache00%d.map", slot);
		else
			snprintf(z_xbox, sizeof(z_xbox), "z:\\cache_donor.map");
		platform_translate_path(z_xbox, z_path, sizeof(z_path));
		zf = fopen(z_path, "rb");
		if (zf)
		{
			uint8_t zhdr[2048];
			long zsz = 0;
			fseek(zf, 0, SEEK_END);
			zsz = ftell(zf);
			fseek(zf, 0, SEEK_SET);
			if (zsz >= (long)(tag_off + tag_sz) && fread(zhdr, 1, sizeof(zhdr), zf) == sizeof(zhdr))
			{
				if (*(uint32_t const *)(zhdr + 0x00) == 0x68656164u &&
					*(uint32_t const *)(zhdr + 0x04) == 5u &&
					*(uint32_t const *)(zhdr + 0x10) == tag_off &&
					*(uint32_t const *)(zhdr + 0x14) == tag_sz &&
					_strnicmp((char const *)(zhdr + 0x20), map_internal_name, 31) == 0)
				{
					fclose(zf);
					fclose(in_fp);
					snprintf(out_cache_path, out_cache_sz, "%s", z_path);
					return 1;
				}
			}
			fclose(zf);
		}
	}

	/* 2. Check if map_path itself is already uncompressed on disk; if so, read it directly in-place! */
	{
		long in_sz = 0;
		fseek(in_fp, 0, SEEK_END);
		in_sz = ftell(in_fp);
		if (decomp_len >= tag_off + tag_sz && in_sz >= (long)decomp_len)
		{
			uint32_t tag_sig = 0;
			if (fseek(in_fp, (long)(tag_off + 0x20u), SEEK_SET) == 0 &&
				fread(&tag_sig, 4, 1, in_fp) == 1 &&
				tag_sig == 0x626C616Du /* 'blam' */)
			{
				fclose(in_fp);
				snprintf(out_cache_path, out_cache_sz, "%s", map_path);
				return 1;
			}
		}
		fseek(in_fp, 2048L, SEEK_SET);
	}

	/* 3. Stream-decompress the Xbox v5 map into the scratch file z:\cache_donor.map, which goes with the level */
	platform_translate_path("z:\\cache_donor.map", out_cache_path, out_cache_sz);
	{
		FILE *tmp_out = fopen(out_cache_path, "wb");
		z_stream strm;
		uint8_t *in_chunk;
		uint8_t *out_chunk;
		size_t const chunk_sz = 262144u;
		int zret = Z_OK;
		int decomp_ok = 0;

		if (!tmp_out)
		{
			fclose(in_fp);
			return 0;
		}
		fwrite(hdr, 1, sizeof(hdr), tmp_out);

		in_chunk = (uint8_t *)malloc(chunk_sz);
		out_chunk = (uint8_t *)malloc(chunk_sz);
		memset(&strm, 0, sizeof(strm));
		if (in_chunk && out_chunk && inflateInit(&strm) == Z_OK)
		{
			decomp_ok = 1;
			do
			{
				strm.avail_in = (uInt)fread(in_chunk, 1, chunk_sz, in_fp);
				if (ferror(in_fp))
				{
					decomp_ok = 0;
					break;
				}
				if (strm.avail_in == 0)
					break;
				strm.next_in = in_chunk;
				do
				{
					size_t have;
					strm.avail_out = (uInt)chunk_sz;
					strm.next_out = out_chunk;
					zret = inflate(&strm, Z_NO_FLUSH);
					if (zret == Z_NEED_DICT || zret == Z_DATA_ERROR || zret == Z_MEM_ERROR)
					{
						decomp_ok = 0;
						break;
					}
					have = chunk_sz - (size_t)strm.avail_out;
					if (have > 0 && fwrite(out_chunk, 1, have, tmp_out) != have)
					{
						decomp_ok = 0;
						break;
					}
					if (s_forge_loading_active && s_forge_loading_map_span > 0.0f)
					{
						unsigned long now_ms = system_milliseconds();
						if (now_ms - s_forge_loading_last_ms >= 15u)
						{
							float map_frac = (decomp_len > 2048u)
								? ((float)strm.total_out / (float)(decomp_len - 2048u))
								: 0.5f;
							if (map_frac < 0.0f) map_frac = 0.0f;
							if (map_frac > 0.99f) map_frac = 0.99f;
							forge_xbox_loading_screen_render_frame(
								s_forge_loading_map_base + map_frac * s_forge_loading_map_span);
						}
					}
				} while (strm.avail_out == 0);
			} while (decomp_ok && zret != Z_STREAM_END);
			inflateEnd(&strm);
		}
		free(in_chunk);
		free(out_chunk);
		fclose(tmp_out);
		fclose(in_fp);

		if (!decomp_ok)
		{
			remove(out_cache_path);
			out_cache_path[0] = 0;
			return 0;
		}
	}
	return 1;
}

void delta_forge_probe_set(int (*probe)(void)); /* AE hook */
static int forge_net_session_probe(void) { return g_forge_session_enabled != 0; } /* AE hook */

void forge_runtime_init(void)
{
	delta_forge_probe_set(forge_net_session_probe); /* AE hook */
	if (g_forge.initialized)
		return;
	memset(&g_forge, 0, sizeof(g_forge));
	g_forge.route_follow_live_index = -1;
	g_forge.initialized = 1;
	g_forge.mode = _forge_mode_play;
	g_forge.active_modal = _forge_modal_none;
	g_forge.current_category = _forge_cat_vehicles;
	g_forge.snap_mode = _forge_snap_15_deg;
	g_forge.ai_team_mode = _forge_ai_team_enemy_covenant;
	g_forge.cam_speed = 12.0f;
	g_forge.grabbed_object_index = NONE;
	g_forge.grabbed_live_index = -1;
	g_forge.hovered_object_index = NONE;
	g_forge.hovered_live_index = -1;
	g_forge.grab_distance = 5.0f;
	snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "forge_custom_arena");
	g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);

#if FORGE_HOST_FILES
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/custom_tags", NULL);
#endif
	tag_pack_request(FORGE_PACK);
	forge_runtime_reload_catalogs();
	forge_snap_points_load_all();
}

/* ============================================================================
 * NATIVE XBOX MAP TAG LOADER & .OCETAGS CUSTOM TAG LOADER
 * Default tags are loaded natively from Xbox maps (using OpenCE's exact tag
 * schema & hardware vertex/triangle/texture registration).
 * Extra custom tags are loaded natively from .ocetags in custom_tags/.
 * ============================================================================ */

#define FORGE_MAX_HW_PARTS       48
#define FORGE_MAX_DYNAMIC_TAGS   256
#define FORGE_MAX_PHYS_ALLOCS    8192
#define FORGE_MAX_DYN_SOUND_PERMS 4096

struct forge_hw_part
{
	struct vertex_buffer vertex_buffer;
	struct triangle_buffer triangle_buffer;
	char shader_hint[128];
	float u_scale;
	float v_scale;
	/* (the marker meshes': what the buffers above were copied from) */
	void *source_vertices;
	void *source_triangles;
};

struct forge_dynamic_tag_entry
{
	uint32_t group_tag;
	char tag_path[256];
	long definition_index;
	float bounding_radius;
	int part_count;
	struct forge_hw_part parts[FORGE_MAX_HW_PARTS];
};

struct forge_dyn_tags_state
{
	void *saved_base_instances;
	long saved_base_count;
	void *allocated_instances;
	int count;
	struct forge_dynamic_tag_entry entries[FORGE_MAX_DYNAMIC_TAGS];
};
FORGE_HEAP_STATIC(struct forge_dyn_tags_state, s_dyn_tags)
#define s_dyn_tags FORGE_HEAP(s_dyn_tags)

static struct
{
	int count;
	void *ptrs[FORGE_MAX_PHYS_ALLOCS];
} s_forge_phys_allocs;

static struct
{
	int count;
	struct sound_permutation const *perms[FORGE_MAX_DYN_SOUND_PERMS];
} s_dyn_sound_perms;

static void *forge_phys_alloc(size_t bytes)
{
	void *p;
	size_t alloc_sz = (bytes > 64u) ? bytes : 64u;
	p = XPhysicalAlloc((unsigned long)alloc_sz, (unsigned long)-1, 0, PAGE_READWRITE);
	if (p)
	{
		memset(p, 0, alloc_sz);
		if (s_forge_phys_allocs.count < FORGE_MAX_PHYS_ALLOCS)
			s_forge_phys_allocs.ptrs[s_forge_phys_allocs.count++] = p;
		return p;
	}
	return calloc(1, alloc_sz);
}

static void forge_register_dyn_sound_perm(struct sound_permutation const *perm)
{
	int i;
	if (!perm || !perm->samples.address || perm->samples.size <= 0)
		return;
	for (i = 0; i < s_dyn_sound_perms.count; i++)
	{
		if (s_dyn_sound_perms.perms[i] == perm)
			return;
	}
	if (s_dyn_sound_perms.count < FORGE_MAX_DYN_SOUND_PERMS)
		s_dyn_sound_perms.perms[s_dyn_sound_perms.count++] = perm;
}

boolean mod_sounds_read(
	struct sound_permutation const *permutation,
	void *buffer)
{
	int i;
	if (permutation && buffer)
	{
		for (i = 0; i < s_dyn_sound_perms.count; i++)
		{
			if (s_dyn_sound_perms.perms[i] == permutation &&
				permutation->samples.address &&
				permutation->samples.size > 0)
			{
				memcpy(buffer, XBOX_POINTER(void const, permutation->samples.address), (size_t)permutation->samples.size); /* AE hook */
				return TRUE;
			}
		}
	}
	return orig_mod_sounds_read(permutation, buffer);
}

static void marker_meshes_dispose(void);

static int s_marker_meshes_loaded_flag = 0;
static long s_monitor_bipd_handle = NONE;
static int s_monitor_resolved = 0;
static int s_local_monitor_drawn_this_frame = 0;
static int s_mcc_hud_active = 0;

void forge_runtime_on_map_unload(void)
{
	int i;
	s_mcc_hud_active = 0;
	/* Dispose converted D3D8 vertex/index buffers for injected models BEFORE freeing tag buffers */
	custom_edition_injected_models_dispose();
	for (i = 0; i < s_forge_phys_allocs.count; i++)
	{
		if (s_forge_phys_allocs.ptrs[i])
			XPhysicalFree(s_forge_phys_allocs.ptrs[i]);
	}
	s_forge_phys_allocs.count = 0;
	s_dyn_sound_perms.count = 0;
	/* (each level builds them again: kept, every Forge load took their memory
	for good) */
	marker_meshes_dispose();
	s_marker_meshes_loaded_flag = 0;
	s_monitor_bipd_handle = NONE;
	s_monitor_resolved = 0;
	s_local_monitor_drawn_this_frame = 0;

	if (s_dyn_tags.saved_base_instances)
	{
		cache_files_set_tag_instances(s_dyn_tags.saved_base_instances, s_dyn_tags.saved_base_count);
		s_dyn_tags.saved_base_instances = NULL;
		s_dyn_tags.saved_base_count = 0;
	}
	if (s_dyn_tags.allocated_instances)
	{
		free(s_dyn_tags.allocated_instances);
		s_dyn_tags.allocated_instances = NULL;
	}
	s_dyn_tags.count = 0;
	forge_pack_donor_close();
	{
		char donor_cache[512];
		platform_translate_path("z:\\cache_donor.map", donor_cache, sizeof(donor_cache));
		remove(donor_cache);
	}
}

static void build_hw_part_from_arrays(
	struct forge_hw_part *dst_part,
	uint8_t *uncomp_verts,
	int32_t vtx_count,
	uint8_t const *idx_bytes,
	int32_t tri_count,
	char const *shader_hint,
	float scale_factor,
	float z_offset)
{
	uint32_t uncomp_sz;
	uint32_t comp_sz;
	uint32_t idx_sz;
	void *hw_verts;
	void *hw_tris;
	int v, c;

	/* Strip trailing 0xFFFF padding indices (from .gbxmodel 3-index model_triangle blocks) */
	if (idx_bytes && vtx_count >= 3)
	{
		uint16_t const *raw_idx = (uint16_t const *)idx_bytes;
		while (tri_count > 1 && raw_idx[tri_count + 1] >= (uint16_t)vtx_count)
			tri_count--;
	}
	if (vtx_count < 3 || tri_count < 1)
		return;

	uncomp_sz = (uint32_t)vtx_count * 68u;
	comp_sz = (uint32_t)vtx_count * 32u;
	idx_sz = ((uint32_t)tri_count + 2u) * 2u;

	for (v = 0; v < vtx_count; v++)
	{
		uint8_t *vp = uncomp_verts + (uint32_t)v * 68u;
		if (scale_factor != 1.0f || z_offset != 0.0f)
		{
			*(float *)(vp + 0) *= scale_factor;
			*(float *)(vp + 4) *= scale_factor;
			*(float *)(vp + 8) = (*(float *)(vp + 8) * scale_factor) + z_offset;
		}
		for (c = 0; c < 9; c++)
		{
			float val = *(float *)(vp + 12 + c * 4);
			if (!(val >= -1.0f && val <= 1.0f))
				*(float *)(vp + 12 + c * 4) = (c % 3 == 0) ? 1.0f : 0.0f;
		}
		*(int16_t *)(vp + 56) = 0;
		*(int16_t *)(vp + 58) = 0;
		*(float *)(vp + 60) = 1.0f;
		*(float *)(vp + 64) = 0.0f;
	}

	/* (in the heap: the buffers below copy them into contiguous memory) */
	hw_verts = malloc(comp_sz);
	hw_tris = malloc(idx_sz);
	if (!hw_verts || !hw_tris)
	{
		free(hw_verts);
		free(hw_tris);
		return;
	}

	memset(dst_part, 0, sizeof(*dst_part));
	dst_part->source_vertices = hw_verts;
	dst_part->source_triangles = hw_tris;
	dst_part->u_scale = 1.0f;
	dst_part->v_scale = 1.0f;
	if (shader_hint && shader_hint[0])
	{
		char const *bar1 = strchr(shader_hint, '|');
		char const *bar2 = bar1 ? strchr(bar1 + 1, '|') : NULL;
		if (bar2)
		{
			float us = 1.0f, vs = 1.0f;
			size_t copy_len = (size_t)(bar2 - shader_hint);
			if (copy_len >= sizeof(dst_part->shader_hint))
				copy_len = sizeof(dst_part->shader_hint) - 1;
			memcpy(dst_part->shader_hint, shader_hint, copy_len);
			dst_part->shader_hint[copy_len] = '\0';
			if (sscanf(bar2 + 1, "%f,%f", &us, &vs) == 2)
			{
				if (us > 0.0001f && us < 10000.0f)
					dst_part->u_scale = us;
				if (vs > 0.0001f && vs < 10000.0f)
					dst_part->v_scale = vs;
			}
		}
		else
		{
			snprintf(dst_part->shader_hint, sizeof(dst_part->shader_hint), "%s", shader_hint);
		}
	}

	rasterizer_geometry_compress_vertices(
		_rasterizer_vertex_type_model_uncompressed,
		vtx_count,
		hw_verts,
		(long)comp_sz,
		uncomp_verts,
		(long)uncomp_sz);
	memcpy(hw_tris, idx_bytes, idx_sz);
	{
		uint16_t *tris_u16 = (uint16_t *)hw_tris;
		int32_t total_idx = tri_count + 2;
		int32_t k;
		for (k = 0; k < total_idx; k++)
		{
			if (tris_u16[k] >= (uint16_t)vtx_count)
				tris_u16[k] = (k > 0 && tris_u16[k - 1] < (uint16_t)vtx_count) ? tris_u16[k - 1] : 0u;
		}
	}

	rasterizer_vertex_buffer_new(
		&dst_part->vertex_buffer,
		_rasterizer_vertex_type_model_compressed,
		vtx_count,
		hw_verts,
		(long)comp_sz);

	rasterizer_triangle_buffer_new(
		&dst_part->triangle_buffer,
		_triangle_buffer_type_precompiled_strip,
		tri_count,
		hw_tris);
}

static int load_hw_parts_from_ftag_file(
	char const *ftag_path,
	struct forge_hw_part *out_parts,
	int max_parts,
	int *out_part_count,
	float *out_radius,
	float scale_factor,
	float z_offset)
{
	FILE *f = fopen(ftag_path, "rb");
	char magic[4];
	uint32_t version = 0, grp = 0, part_count = 0, p;
	float radius = 1.5f;
	char tag_name[256];

	if (!f)
		return 0;
	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "FTAG", 4) != 0)
	{
		fclose(f);
		return 0;
	}
	if (fread(&version, 4, 1, f) != 1 || version != 1 ||
		fread(&grp, 4, 1, f) != 1 ||
		fread(&radius, 4, 1, f) != 1 ||
		fread(tag_name, 1, 256, f) != 256 ||
		fread(&part_count, 4, 1, f) != 1)
	{
		fclose(f);
		return 0;
	}

	*out_part_count = 0;
	if (out_radius)
		*out_radius = (radius > 0.1f && radius < 100.0f) ? radius : 1.5f;

	for (p = 0; p < part_count && *out_part_count < max_parts; p++)
	{
		int32_t vtx_count = 0, tri_count = 0;
		char shader_hint[128];
		uint32_t uncomp_sz, idx_sz;
		uint8_t *uncomp_verts;
		uint8_t *idx_bytes;

		if (fread(&vtx_count, 4, 1, f) != 1 ||
			fread(&tri_count, 4, 1, f) != 1 ||
			fread(shader_hint, 1, 128, f) != 128)
		{
			break;
		}
		if (vtx_count <= 0 || tri_count <= 0 || vtx_count > 32768 || tri_count > 32768)
			break;

		uncomp_sz = (uint32_t)vtx_count * 68u;
		idx_sz = ((uint32_t)tri_count + 2u) * 2u;
		uncomp_verts = (uint8_t *)malloc(uncomp_sz);
		idx_bytes = (uint8_t *)malloc(idx_sz);
		if (!uncomp_verts || !idx_bytes)
		{
			free(uncomp_verts);
			free(idx_bytes);
			break;
		}
		if (fread(uncomp_verts, 1, uncomp_sz, f) != uncomp_sz ||
			fread(idx_bytes, 1, idx_sz, f) != idx_sz)
		{
			free(uncomp_verts);
			free(idx_bytes);
			break;
		}

		build_hw_part_from_arrays(
			&out_parts[(*out_part_count)++],
			uncomp_verts,
			vtx_count,
			idx_bytes,
			tri_count,
			shader_hint,
			scale_factor,
			z_offset);

		free(uncomp_verts);
		free(idx_bytes);
	}

	fclose(f);
	return (*out_part_count > 0) ? 1 : 0;
}

static long append_runtime_tag_instance(
	long group_tag,
	long parent0,
	long parent1,
	char const *tag_path,
	void *base_address)
{
	long existing_count = 0;
	struct forge_cache_file_tag_instance *instances;
	struct forge_cache_file_tag_instance *grown;
	long max_salt = 0xE174;
	long i, new_abs_idx, new_tag_index;
	char *name_copy;

	instances = (struct forge_cache_file_tag_instance *)cache_files_tag_instances(&existing_count);
	if (!instances || existing_count <= 0 || existing_count >= 65000)
		return NONE;

	if (!s_dyn_tags.saved_base_instances)
	{
		s_dyn_tags.saved_base_instances = instances;
		s_dyn_tags.saved_base_count = existing_count;
	}

	grown = (struct forge_cache_file_tag_instance *)malloc((size_t)(existing_count + 1) * sizeof(*grown));
	name_copy = (char *)malloc(strlen(tag_path) + 1);
	if (!grown || !name_copy)
	{
		free(grown);
		free(name_copy);
		return NONE;
	}
	memcpy(grown, instances, (size_t)existing_count * sizeof(*grown));
	strcpy(name_copy, tag_path);

	for (i = 0; i < existing_count; i++)
	{
		if (instances[i].tag_index != NONE)
		{
			long s = (long)(((unsigned long)instances[i].tag_index >> 16) & 0x7FFFu);
			if (s > max_salt)
				max_salt = s;
		}
	}

	new_abs_idx = existing_count;
	new_tag_index = ((max_salt + 1) << 16) | (new_abs_idx & 0xFFFF);

	memset(&grown[new_abs_idx], 0, sizeof(grown[new_abs_idx]));
	grown[new_abs_idx].group_tag = group_tag;
	grown[new_abs_idx].parent_group_tags[0] = parent0;
	grown[new_abs_idx].parent_group_tags[1] = parent1;
	grown[new_abs_idx].tag_index = new_tag_index;
	grown[new_abs_idx].name = XBOX_ADDRESS(name_copy); /* AE hook */
	grown[new_abs_idx].base_address = XBOX_ADDRESS(base_address); /* AE hook */

	cache_files_set_tag_instances(grown, existing_count + 1);
	if (s_dyn_tags.allocated_instances)
		free(s_dyn_tags.allocated_instances);
	s_dyn_tags.allocated_instances = grown;

	return new_tag_index;
}

uint32_t forge_get_base_map_tag_count(void)
{
	long existing_count = 0;
	if (s_dyn_tags.saved_base_count > 0)
		return (uint32_t)s_dyn_tags.saved_base_count;
	if (cache_files_tag_instances(&existing_count) && existing_count > 0)
		return (uint32_t)existing_count;
	return 0;
}

static int is_valid_cyborg_driver_seat_label(char const *label)
{
	if (!label || !label[0])
		return 0;
	if (_stricmp(label, "W-driver") == 0 ||
		_stricmp(label, "G-driver") == 0 ||
		_stricmp(label, "B-driver") == 0 ||
		_stricmp(label, "S-driver") == 0 ||
		_stricmp(label, "R-driver") == 0 ||
		_stricmp(label, "scorpion-driver") == 0 ||
		_stricmp(label, "CT-driver") == 0)
		return 1;
	return 0;
}

static void patch_vehicle_tag_for_player_driving(long vehi_tag_idx, char const *tag_path)
{
	struct unit_definition *ud;

	if (vehi_tag_idx == NONE)
		return;

	ud = (struct unit_definition *)tag_get('vehi', vehi_tag_idx);
	if (ud)
	{
		ud->unit.flags |= FLAG(_unit_has_entrance_points_inside_bounding_sphere_bit);
		if (ud->unit.seats.count > 0 && ud->unit.seats.address)
		{
			struct unit_seat *seat0 = XBOX_POINTER(struct unit_seat, ud->unit.seats.address); /* AE hook */
			seat0->flags &= ~FLAG(_unit_seat_invisible_bit);
			seat0->flags |= FLAG(_unit_seat_driver_bit) |
							FLAG(_unit_seat_third_person_camera_bit) |
							FLAG(_unit_seat_third_person_on_enter_bit);
			if (tag_path && strstr(tag_path, "wraith"))
				seat0->flags |= FLAG(_unit_seat_gunner_bit);
			if (!is_valid_cyborg_driver_seat_label(seat0->label))
				snprintf(seat0->label, sizeof(seat0->label), "W-driver");
		}
	}
}

static void patch_light_definition_buffer(uint8_t *lbuf)
{
	if (!lbuf)
		return;
	/* Bit 0 (_light_definition_dynamic_bit = 1) must be set and radius_modifier_upper_bound > 0
	 * for Halo's object_lights.c to illuminate surrounding world geometry and objects! */
	*(uint32_t *)(lbuf + 0x00) |= 1u;
	if (*(float const *)(lbuf + 0x04) < 3.5f)
		*(float *)(lbuf + 0x04) = 4.5f;
	if (*(float const *)(lbuf + 0x08) == 0.0f && *(float const *)(lbuf + 0x0C) == 0.0f)
	{
		*(float *)(lbuf + 0x08) = 1.0f;
		*(float *)(lbuf + 0x0C) = 1.0f;
	}
	if (*(float const *)(lbuf + 0x4C) == 0.0f &&
		*(float const *)(lbuf + 0x50) == 0.0f &&
		*(float const *)(lbuf + 0x54) == 0.0f)
	{
		*(float *)(lbuf + 0x38) = 1.0f;
		*(float *)(lbuf + 0x3C) = 1.0f;
		*(float *)(lbuf + 0x40) = 1.0f;
		*(float *)(lbuf + 0x44) = 1.0f;
		*(float *)(lbuf + 0x48) = 1.0f;
		*(float *)(lbuf + 0x4C) = 1.0f;
		*(float *)(lbuf + 0x50) = 1.0f;
		*(float *)(lbuf + 0x54) = 1.0f;
	}
}

static void patch_forge_device_and_light_tag_runtime(long tag_idx, uint32_t grp, char const *tag_path)
{
	uint8_t *obuf;
	int32_t att_cnt, a, model_idx;
	uint8_t *att_base;

	(void)tag_path;
	if (tag_idx == NONE)
		return;
	if (grp != 'scen' && grp != 'mach' && grp != 'lifi' && grp != 'ctrl')
		return;

	obuf = (uint8_t *)tag_get((long)grp, tag_idx);
	if (!obuf)
		return;

	/* Ensure scenery, devices, and light fixtures have generous bounding and render radii so Forge raycasting
	 * and long-distance frustum/LOD visibility never cull custom scenery early */
	if (*(float const *)(obuf + 0x04u) < 1.5f)
		*(float *)(obuf + 0x04u) = 1.5f;
	if (*(float const *)(obuf + 0x104u) < 8.0f)
		*(float *)(obuf + 0x104u) = 8.0f;
	if (*(float const *)(obuf + 0x104u) < *(float const *)(obuf + 0x04u) * 2.0f)
		*(float *)(obuf + 0x104u) = *(float const *)(obuf + 0x04u) * 2.0f;

	/* Patch the object's model ('mode'/'mod2') LOD cutoffs and permutation geometry_indices[0..4]
	 * so custom scenery items with missing low-LOD geometries or positive super_low pixel cutoffs
	 * render across the entire map just like base-game scenery! */
	model_idx = *(int32_t const *)(obuf + 0x34u);
	if (model_idx != NONE)
	{
		uint8_t *mbuf = (uint8_t *)tag_get('mode', model_idx);
		if (!mbuf)
			mbuf = (uint8_t *)tag_get('mod2', model_idx);
		if (mbuf)
		{
			float *cutoffs = (float *)(mbuf + 0x08u);
			int32_t reg_cnt = *(int32_t const *)(mbuf + 0xC4u);
			uint8_t *reg_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(mbuf + 0xC8u)); /* AE hook */
			int32_t geom_cnt = *(int32_t const *)(mbuf + 0xD0u);
			uint8_t *geom_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(mbuf + 0xD4u)); /* AE hook */
			int32_t r, p, lv;

			if (cutoffs[0] > cutoffs[4] && cutoffs[4] >= 0.0f)
			{
				float t0 = cutoffs[0], t1 = cutoffs[1];
				cutoffs[0] = cutoffs[4];
				cutoffs[1] = cutoffs[3];
				cutoffs[3] = t1;
				cutoffs[4] = t0;
			}
			cutoffs[0] = 0.0f; /* Never cull at super_low distance (models.c:1000) */

			if (reg_cnt > 0 && reg_cnt <= 32 && reg_base && geom_cnt > 0 && geom_base)
			{
				for (r = 0; r < reg_cnt; r++)
				{
					uint8_t *reg = reg_base + (size_t)r * 76u;
					int32_t perm_cnt = *(int32_t const *)(reg + 0x40u);
					uint8_t *perm_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(reg + 0x44u)); /* AE hook */
					if (perm_cnt <= 0 || perm_cnt > 32 || !perm_base)
						continue;
					for (p = 0; p < perm_cnt; p++)
					{
						uint8_t *perm = perm_base + (size_t)p * 88u;
						int16_t *g_indices = (int16_t *)(perm + 0x40u);
						int16_t best_g = -1;
						for (lv = 4; lv >= 0; lv--)
						{
							int16_t gi = g_indices[lv];
							if (gi >= 0 && gi < (int16_t)geom_cnt)
							{
								int32_t part_cnt = *(int32_t const *)(geom_base + (size_t)gi * 48u + 36u);
								if (part_cnt > 0)
								{
									best_g = gi;
									break;
								}
							}
						}
						if (best_g >= 0)
						{
							for (lv = 0; lv < 5; lv++)
							{
								int16_t gi = g_indices[lv];
								if (gi < 0 || gi >= (int16_t)geom_cnt ||
									*(int32_t const *)(geom_base + (size_t)gi * 48u + 36u) <= 0)
								{
									g_indices[lv] = best_g;
								}
							}
						}
					}
				}
			}
		}
	}

	if (grp == 'mach' && tag_path &&
		(_stricmp(tag_path, "levels\\a30\\devices\\beam emitter\\beam emitter") == 0 ||
		 _stricmp(tag_path, "levels\\a30\\devices\\beam emitter\\beam emitter red") == 0))
	{
		*(float *)(obuf + 0x04u) = 65.0f;  /* bounding_radius */
		*(float *)(obuf + 0x10u) = 0.0f;   /* bounding_offset.z */
		*(float *)(obuf + 0x104u) = 65.0f; /* render_bounding_radius */
	}

	/* Patch attached 'ligh' tags so placed lights, spotlights, and point lights cast dynamic illumination */
	att_cnt = *(int32_t const *)(obuf + 0x138u);
	att_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(obuf + 0x13Cu)); /* AE hook */
	for (a = 0; a < att_cnt && att_base; a++)
	{
		uint8_t *att = att_base + (size_t)a * 72u;
		uint32_t agrp = *(uint32_t const *)(att + 0x00u);
		int32_t aidx = *(int32_t const *)(att + 0x0Cu);
		if (agrp == 'ligh' && aidx != NONE)
		{
			uint8_t *lbuf = (uint8_t *)tag_get('ligh', aidx);
			if (lbuf)
				patch_light_definition_buffer(lbuf);
		}
	}

	/* Configure 'mach' doors to open/close automatically when players or vehicles approach.
	   Only in a Forge session: this runs on every map, and a campaign's "door" machines are
	   not all doors (a10's airlock_1_door_3 starts open and changes only once, so as an
	   automatic door it shut for good after 3 seconds, and the level could not be finished) */
	if (grp == 'mach' && g_forge_session_enabled)
	{
		int16_t mtype = *(int16_t const *)(obuf + 0x290u);
		if (mtype == 0 /* _machine_type_door */ ||
			(tag_path && (strstr(tag_path, "door") != NULL || strstr(tag_path, "Door") != NULL || strstr(tag_path, "DOOR") != NULL)))
		{
			*(int16_t *)(obuf + 0x290u) = 0;    /* _machine_type_door */
			*(float *)(obuf + 0x21Cu) = 2.5f;   /* device.automatic_activation_radius */
			*(uint16_t *)(obuf + 0x292u) |= 0x0003u; /* _machine_is_pathfinding_obstacle_bit | _machine_is_not_pathfinding_obstacle_when_open_bit */
			*(float *)(obuf + 0x294u) = 3.0f;   /* machine.door_open_time */
			*(int32_t *)(obuf + 0x320u) = 90;   /* machine.runtime_door_open_ticks (3.0f * 30) */
		}
	}
}

static long find_native_or_mp_variant_tag(uint32_t group_tag, char const *tag_path)
{
	long idx;
	long i;

	idx = tag_loaded((long)group_tag, tag_path);
	if (idx != NONE)
	{
		if (group_tag == 'vehi')
			patch_vehicle_tag_for_player_driving(idx, tag_path);
		else if (group_tag == 'scen' || group_tag == 'mach' || group_tag == 'lifi' || group_tag == 'ctrl')
			patch_forge_device_and_light_tag_runtime(idx, group_tag, tag_path);
		return idx;
	}

	/* Map between SP and MP vehicle/weapon names natively present in MP maps */
	if (group_tag == 'vehi')
	{
		static struct { char const *a; char const *b; } const vehi_pairs[] = {
			{ "vehicles\\warthog\\warthog", "vehicles\\warthog\\mp_warthog" },
			{ "vehicles\\ghost\\ghost", "vehicles\\ghost\\ghost_mp" },
			{ "vehicles\\scorpion\\scorpion", "vehicles\\scorpion\\scorpion_mp" },
			{ "vehicles\\banshee\\banshee", "vehicles\\banshee\\banshee_mp" },
			{ "vehicles\\c_gun_turret\\c_gun_turret", "vehicles\\c_gun_turret\\c_gun_turret_mp" },
			{ "vehicles\\c gun turret\\c gun turret", "vehicles\\c gun turret\\c gun turret_mp" },
			{ "vehicles\\c_gun_turret\\c_gun_turret", "vehicles\\c gun turret\\c gun turret_mp" },
			{ "vehicles\\c gun turret\\c gun turret", "vehicles\\c_gun_turret\\c_gun_turret_mp" }
		};
		for (i = 0; i < (long)(sizeof(vehi_pairs) / sizeof(vehi_pairs[0])); i++)
		{
			if (_stricmp(tag_path, vehi_pairs[i].a) == 0)
			{
				idx = tag_loaded('vehi', vehi_pairs[i].b);
				if (idx != NONE) return idx;
			}
			else if (_stricmp(tag_path, vehi_pairs[i].b) == 0)
			{
				idx = tag_loaded('vehi', vehi_pairs[i].a);
				if (idx != NONE) return idx;
			}
		}
	}
	return NONE;
}

static struct tag_schema_group const *forge_runtime_schema_group_get(uint32_t group_tag)
{
	struct tag_schema_group const *const *list;
	struct tag_schema_group const *group;

	if (group_tag == 'mod2')
	{
		for (group = tag_schema_custom_edition_groups; group->group_tag != 0; group++)
		{
			if (group->group_tag == 'mod2')
				return group;
		}
	}

	for (list = tag_schema_group_lists; *list != NULL; list++)
	{
		for (group = *list; group->group_tag != 0; group++)
		{
			if (group->group_tag == (unsigned long)group_tag)
				return group;
		}
	}
	return NULL;
}

static long load_native_ocetags_package_runtime(uint32_t req_group, char const *req_path, uint32_t *out_actual_group);

struct xbox_runtime_closure
{
	uint8_t const *tag_buf;
	uint32_t tag_sz;
	uint32_t tag_base;
	uint32_t inst_off;
	int32_t tag_count;
	uint8_t *visited;
	int32_t *order;
	int32_t order_count;
	long *runtime_handles;
	uint8_t **cloned_roots;
	int is_red_beam_variant;
};

static void xbox_collect_schema_dependencies(
	struct xbox_runtime_closure *cl,
	struct tag_schema_definition const *def,
	uint32_t base_off,
	int32_t elem_count,
	int depth);

static void xbox_visit_tag_index(struct xbox_runtime_closure *cl, int32_t abs_idx)
{
	uint32_t io, grp, name_addr, data_addr;
	char const *tname;
	struct tag_schema_group const *schema;

	if (abs_idx < 0 || abs_idx >= cl->tag_count)
		return;
	if (cl->visited[abs_idx])
		return;
	cl->visited[abs_idx] = 1;

	io = cl->inst_off + (uint32_t)abs_idx * 32u;
	if (io + 32u > cl->tag_sz)
		return;
	grp = *(uint32_t const *)(cl->tag_buf + io + 0x00);
	name_addr = *(uint32_t const *)(cl->tag_buf + io + 0x10);
	data_addr = *(uint32_t const *)(cl->tag_buf + io + 0x14);
	if (name_addr < cl->tag_base || (name_addr - cl->tag_base) >= cl->tag_sz)
		return;
	tname = (char const *)(cl->tag_buf + (name_addr - cl->tag_base));

	if (grp == 'udlg' || grp == 'sbsp' || grp == 'scnr')
	{
		cl->runtime_handles[abs_idx] = NONE;
		return;
	}

	/* Strictly keep Xbox tags using Xbox bitmaps, shaders, models, and sounds from their source Xbox .map!
	 * Only reuse non-visual/non-audio shared dependencies if we are on a stock Xbox map (never on a CE map). */
	if (cl->order_count > 0 && !custom_edition_cache_tags_loaded() && !cl->is_red_beam_variant &&
		grp != 'bitm' && grp != 'mode' && grp != 'snd!' && grp != 'lsnd' &&
		grp != 'shdr' && grp != 'senv' && grp != 'soso' && grp != 'sotr' &&
		grp != 'schi' && grp != 'scex' && grp != 'swat' && grp != 'sgla' &&
		grp != 'smet' && grp != 'spla')
	{
		long ex = tag_loaded((long)grp, tname);
		if (ex != NONE)
		{
			cl->runtime_handles[abs_idx] = ex;
			return;
		}
	}

	if (data_addr < cl->tag_base || (data_addr - cl->tag_base) >= cl->tag_sz)
		return;

	schema = forge_runtime_schema_group_get(grp);
	if (!schema || !schema->definition || schema->definition->size <= 0)
		return;
	if ((data_addr - cl->tag_base) + (uint32_t)schema->definition->size > cl->tag_sz)
		return;

	cl->order[cl->order_count++] = abs_idx;
	xbox_collect_schema_dependencies(cl, schema->definition, data_addr - cl->tag_base, 1, 1);
}

static void xbox_collect_schema_dependencies(
	struct xbox_runtime_closure *cl,
	struct tag_schema_definition const *def,
	uint32_t base_off,
	int32_t elem_count,
	int depth)
{
	int32_t elem_idx;
	struct tag_schema_field const *field;
	uint32_t elem_size;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 10)
		return;
	elem_size = (uint32_t)def->size;
	if ((uint64_t)base_off + (uint64_t)elem_count * elem_size > cl->tag_sz)
		return;

	for (elem_idx = 0; elem_idx < elem_count; elem_idx++)
	{
		uint32_t elem_off = base_off + (uint32_t)elem_idx * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = field->count > 0 ? field->count : 1;
			uint32_t fstride = field->size > 0 ? (uint32_t)field->size : 0;

			for (k = 0; k < fcount; k++)
			{
				uint32_t f_off = elem_off + (uint32_t)field->offset + (uint32_t)k * fstride;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_off + (uint32_t)sub_def->size <= elem_off + elem_size)
						xbox_collect_schema_dependencies(cl, sub_def, f_off, 1, depth + 1);
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_off + 12u <= elem_off + elem_size)
					{
						int32_t cnt = *(int32_t const *)(cl->tag_buf + f_off + 0u);
						uint32_t addr = *(uint32_t const *)(cl->tag_buf + f_off + 4u);
						int32_t max_cnt = (field->maximum > 0) ? (int32_t)field->maximum : 32768;
						if (cnt > 0 && cnt <= max_cnt && addr >= cl->tag_base &&
							(uint64_t)(addr - cl->tag_base) + (uint64_t)cnt * (uint32_t)child_def->size <= cl->tag_sz)
						{
							xbox_collect_schema_dependencies(cl, child_def, addr - cl->tag_base, cnt, depth + 1);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_off + 16u <= elem_off + elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(cl->tag_buf + f_off + 12u);
						if (ref_idx != -1 && (ref_idx & 0xFFFF) < cl->tag_count)
							xbox_visit_tag_index(cl, ref_idx & 0xFFFF);
					}
				}
			}
		}
	}
}

static void xbox_relocate_schema_elements(
	struct xbox_runtime_closure *cl,
	struct tag_schema_definition const *def,
	uint8_t *dst_base,
	uint32_t src_base_off,
	int32_t elem_count,
	int depth)
{
	int32_t elem_idx;
	struct tag_schema_field const *field;
	uint32_t elem_size;

	if (!def || !def->fields || !dst_base || elem_count <= 0 || def->size <= 0 || depth > 10)
		return;
	elem_size = (uint32_t)def->size;

	for (elem_idx = 0; elem_idx < elem_count; elem_idx++)
	{
		uint8_t *elem_ptr = dst_base + (size_t)elem_idx * elem_size;
		uint32_t src_elem_off = src_base_off + (uint32_t)elem_idx * elem_size;

		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = field->count > 0 ? field->count : 1;
			uint32_t fstride = field->size > 0 ? (uint32_t)field->size : 0;

			for (k = 0; k < fcount; k++)
			{
				uint32_t f_rel = (uint32_t)field->offset + (uint32_t)k * fstride;
				uint32_t src_f_off = src_elem_off + f_rel;

				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_rel + (uint32_t)sub_def->size <= elem_size)
					{
						xbox_relocate_schema_elements(
							cl, sub_def, elem_ptr + f_rel, src_f_off, 1, depth + 1);
					}
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_rel + 12u <= elem_size)
					{
						struct tag_block *blk = (struct tag_block *)(elem_ptr + f_rel);
						int32_t cnt = *(int32_t const *)(cl->tag_buf + src_f_off + 0u);
						uint32_t addr = *(uint32_t const *)(cl->tag_buf + src_f_off + 4u);
						uint32_t sub_elem_sz = (uint32_t)child_def->size;
						int32_t max_cnt = (field->maximum > 0) ? (int32_t)field->maximum : 32768;

						blk->definition = XBOX_ADDRESS(NULL); /* AE hook */
						if (cnt > 0 && cnt <= max_cnt && addr >= cl->tag_base &&
							(uint64_t)(addr - cl->tag_base) + (uint64_t)cnt * sub_elem_sz <= cl->tag_sz)
						{
							size_t copy_bytes = (size_t)cnt * sub_elem_sz;
							uint8_t *new_sub = (uint8_t *)calloc(1, copy_bytes);
							if (new_sub)
							{
								memcpy(new_sub, cl->tag_buf + (addr - cl->tag_base), copy_bytes);
								blk->count = cnt;
								blk->address = XBOX_ADDRESS(new_sub); /* AE hook */
								xbox_relocate_schema_elements(
									cl, child_def, new_sub, addr - cl->tag_base, cnt, depth + 1);
							}
							else
							{
								blk->count = 0;
								blk->address = XBOX_ADDRESS(NULL); /* AE hook */
							}
						}
						else
						{
							blk->count = 0;
							blk->address = XBOX_ADDRESS(NULL); /* AE hook */
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_rel + 20u <= elem_size)
					{
						struct tag_data *td = (struct tag_data *)(elem_ptr + f_rel);
						int32_t sz = *(int32_t const *)(cl->tag_buf + src_f_off + 0u);
						uint32_t addr = *(uint32_t const *)(cl->tag_buf + src_f_off + 12u);
						int32_t max_sz = (field->maximum > 0) ? (int32_t)field->maximum : (16 * 1024 * 1024);

						td->definition = XBOX_ADDRESS(NULL); /* AE hook */
						if (sz > 0 && sz <= max_sz && addr >= cl->tag_base &&
							(uint64_t)(addr - cl->tag_base) + (uint32_t)sz <= cl->tag_sz)
						{
							void *new_td = malloc((size_t)sz);
							if (new_td)
							{
								memcpy(new_td, cl->tag_buf + (addr - cl->tag_base), (size_t)sz);
								td->size = sz;
								td->address = XBOX_ADDRESS(new_td); /* AE hook */
							}
							else
							{
								td->size = 0;
								td->address = XBOX_ADDRESS(NULL); /* AE hook */
							}
						}
						else
						{
							td->size = 0;
							td->address = XBOX_ADDRESS(NULL); /* AE hook */
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_rel + 16u <= elem_size)
					{
						struct tag_reference *ref = (struct tag_reference *)(elem_ptr + f_rel);
						uint32_t name_addr = *(uint32_t const *)(cl->tag_buf + src_f_off + 4u);
						int32_t ref_idx = *(int32_t const *)(cl->tag_buf + src_f_off + 12u);

						ref->name = XBOX_ADDRESS(NULL); /* AE hook */
						ref->name_length = 0;
						if (name_addr >= cl->tag_base && (name_addr - cl->tag_base) < cl->tag_sz)
						{
							char const *rname = (char const *)(cl->tag_buf + (name_addr - cl->tag_base));
							size_t rlen = strlen(rname);
							char *ncopy = (char *)malloc(rlen + 1u);
							if (ncopy)
							{
								memcpy(ncopy, rname, rlen + 1u);
								ref->name = XBOX_ADDRESS(ncopy); /* AE hook */
							}
						}
						if (ref_idx != -1 && (ref_idx & 0xFFFF) < cl->tag_count &&
							cl->runtime_handles[ref_idx & 0xFFFF] != NONE)
						{
							long hdl = cl->runtime_handles[ref_idx & 0xFFFF];
							ref->index = hdl;
							ref->group_tag = tag_get_group_tag(hdl);
						}
						else
						{
							ref->index = NONE;
						}
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_rel + 4u <= elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(cl->tag_buf + src_f_off);
						if (ref_idx != -1 && (ref_idx & 0xFFFF) < cl->tag_count &&
							cl->runtime_handles[ref_idx & 0xFFFF] != NONE)
						{
							*(int32_t *)(elem_ptr + f_rel) = cl->runtime_handles[ref_idx & 0xFFFF];
						}
						else
						{
							*(int32_t *)(elem_ptr + f_rel) = NONE;
						}
					}
				}
			}
		}
	}
}

static void runtime_tint_dxt1_blue_to_red(uint8_t *pix, uint32_t byte_count)
{
	uint32_t off;
	if (!pix)
		return;
	for (off = 0; off + 8u <= byte_count; off += 8u)
	{
		uint16_t c0 = *(uint16_t const *)(pix + off + 0u);
		uint16_t c1 = *(uint16_t const *)(pix + off + 2u);
		uint32_t lut = *(uint32_t const *)(pix + off + 4u);
		uint16_t r0 = (uint16_t)((c0 >> 11) & 0x1Fu);
		uint16_t g0 = (uint16_t)((c0 >> 5) & 0x3Fu);
		uint16_t b0 = (uint16_t)(c0 & 0x1Fu);
		uint16_t r1 = (uint16_t)((c1 >> 11) & 0x1Fu);
		uint16_t g1 = (uint16_t)((c1 >> 5) & 0x3Fu);
		uint16_t b1 = (uint16_t)(c1 & 0x1Fu);
		uint16_t nr0 = (b0 > r0) ? b0 : r0;
		uint16_t nb0 = (r0 < b0) ? (uint16_t)(r0 >> 1) : (uint16_t)(b0 >> 1);
		uint16_t ng0 = (uint16_t)(g0 >> 2);
		uint16_t nr1 = (b1 > r1) ? b1 : r1;
		uint16_t nb1 = (r1 < b1) ? (uint16_t)(r1 >> 1) : (uint16_t)(b1 >> 1);
		uint16_t ng1 = (uint16_t)(g1 >> 2);
		uint16_t nc0 = (uint16_t)((nr0 << 11) | (ng0 << 5) | nb0);
		uint16_t nc1 = (uint16_t)((nr1 << 11) | (ng1 << 5) | nb1);

		if (c0 > c1)
		{
			if (nc0 < nc1)
			{
				uint16_t tmp = nc0;
				nc0 = nc1;
				nc1 = tmp;
				lut ^= 0x55555555u;
			}
			else if (nc0 == nc1)
			{
				if (nc0 < 0xFFFFu) nc0++;
				else nc1--;
			}
		}
		else
		{
			if (nc0 > nc1)
			{
				uint16_t tmp = nc0;
				uint32_t new_lut = 0;
				int p;
				nc0 = nc1;
				nc1 = tmp;
				for (p = 0; p < 16; p++)
				{
					uint32_t idx = (lut >> (p * 2)) & 3u;
					if (idx == 0u) idx = 1u;
					else if (idx == 1u) idx = 0u;
					new_lut |= (idx << (p * 2));
				}
				lut = new_lut;
			}
		}
		*(uint16_t *)(pix + off + 0u) = nc0;
		*(uint16_t *)(pix + off + 2u) = nc1;
		*(uint32_t *)(pix + off + 4u) = lut;
	}
}

static long load_native_xbox_tag_from_pack(
	struct tag_import_donor *donor,
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
	unsigned long pack_tag_sz = 0;
	uint32_t tag_sz, inst_addr;
	int32_t tag_count, i, root_abs_idx = -1;
	uint8_t *tag_buf;
	struct xbox_runtime_closure cl;
	long root_handle = NONE;
	int is_red_beam_variant = (req_group == 'mach' &&
		_stricmp(req_path, "levels\\a30\\devices\\beam emitter\\beam emitter red") == 0) ? 1 : 0;
	char const *lookup_path = is_red_beam_variant
		? "levels\\a30\\devices\\beam emitter\\beam emitter"
		: req_path;

	tag_buf = tag_import_pack_tags(donor, &pack_tag_sz);
	tag_sz = (uint32_t)pack_tag_sz;
	if (!tag_buf || tag_sz < 64u)
		return NONE;

	inst_addr = *(uint32_t const *)(tag_buf + 0x00);
	tag_count = *(int32_t const *)(tag_buf + 0x0C);
	memset(&cl, 0, sizeof(cl));
	cl.tag_buf = tag_buf;
	cl.tag_sz = tag_sz;
	cl.tag_base = inst_addr - 0x24u;
	cl.inst_off = 0x24u;
	cl.tag_count = tag_count;
	cl.is_red_beam_variant = is_red_beam_variant;

	if (tag_count <= 0 || tag_count > 16384 || cl.inst_off + (uint32_t)tag_count * 32u > tag_sz)
	{
		return NONE;
	}

	/* Locate the requested tag in this Xbox map's tag table */
	for (i = 0; i < tag_count; i++)
	{
		uint32_t io = cl.inst_off + (uint32_t)i * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
		char const *tname;
		if (name_addr < cl.tag_base || (name_addr - cl.tag_base) >= tag_sz)
			continue;
		tname = (char const *)(tag_buf + (name_addr - cl.tag_base));
		if ((grp == req_group || (req_group == 'mod2' && grp == 'mode')) &&
			_stricmp(tname, lookup_path) == 0)
		{
			root_abs_idx = i;
			break;
		}
	}

	if (root_abs_idx < 0)
	{
		return NONE;
	}

	cl.visited = (uint8_t *)calloc((size_t)tag_count, sizeof(uint8_t));
	cl.order = (int32_t *)calloc((size_t)tag_count, sizeof(int32_t));
	cl.runtime_handles = (long *)malloc((size_t)tag_count * sizeof(long));
	cl.cloned_roots = (uint8_t **)calloc((size_t)tag_count, sizeof(uint8_t *));
	if (!cl.visited || !cl.order || !cl.runtime_handles || !cl.cloned_roots)
	{
		free(cl.visited);
		free(cl.order);
		free(cl.runtime_handles);
		free(cl.cloned_roots);
		return NONE;
	}
	for (i = 0; i < tag_count; i++)
		cl.runtime_handles[i] = NONE;

	/* 1. Collect transitive tag dependency closure via OpenCE's tag_schema_group_lists */
	xbox_visit_tag_index(&cl, root_abs_idx);

	/* 2. Allocate root tag definitions and register them in global_tag_instances */
	for (i = 0; i < cl.order_count; i++)
	{
		int32_t abs_idx = cl.order[i];
		uint32_t io = cl.inst_off + (uint32_t)abs_idx * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t p0 = *(uint32_t const *)(tag_buf + io + 0x04);
		uint32_t p1 = *(uint32_t const *)(tag_buf + io + 0x08);
		uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		char const *tname = (char const *)(tag_buf + (name_addr - cl.tag_base));
		char reg_name[256];
		struct tag_schema_group const *schema = forge_runtime_schema_group_get(grp);
		size_t root_sz;
		uint8_t *root_buf;

		if (!schema || !schema->definition || schema->definition->size <= 0)
			continue;
		root_sz = (size_t)schema->definition->size;
		root_buf = (uint8_t *)calloc(1, root_sz < 64u ? 64u : root_sz);
		if (!root_buf)
			continue;
		memcpy(root_buf, tag_buf + (data_addr - cl.tag_base), root_sz);
		cl.cloned_roots[abs_idx] = root_buf;
		if (is_red_beam_variant)
		{
			if (abs_idx == root_abs_idx)
				snprintf(reg_name, sizeof(reg_name), "%s", req_path);
			else
				snprintf(reg_name, sizeof(reg_name), "%s__red", tname);
		}
		else if (abs_idx != root_abs_idx && tag_loaded((long)grp, tname) != NONE)
		{
			snprintf(reg_name, sizeof(reg_name), "%s__f%d", tname, s_dyn_tags.count);
		}
		else
		{
			snprintf(reg_name, sizeof(reg_name), "%s", tname);
		}
		cl.runtime_handles[abs_idx] = append_runtime_tag_instance(
			(long)grp,
			schema->parent_group_tags[0] ? (long)schema->parent_group_tags[0] : (long)p0,
			schema->parent_group_tags[1] ? (long)schema->parent_group_tags[1] : (long)p1,
			reg_name,
			root_buf);
	}

	/* 3. Relocate all nested tag_blocks, tag_data, and tag_references */
	for (i = 0; i < cl.order_count; i++)
	{
		int32_t abs_idx = cl.order[i];
		uint32_t io = cl.inst_off + (uint32_t)abs_idx * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		struct tag_schema_group const *schema = forge_runtime_schema_group_get(grp);
		if (cl.cloned_roots[abs_idx] && schema && schema->definition)
		{
			xbox_relocate_schema_elements(
				&cl,
				schema->definition,
				cl.cloned_roots[abs_idx],
				data_addr - cl.tag_base,
				1,
				1);
		}
	}

	/* 4. Initialize Xbox 'bitm' hardware textures and 'mode' hardware vertex/triangle buffers using OpenCE's native API */
	for (i = 0; i < cl.order_count; i++)
	{
		int32_t abs_idx = cl.order[i];
		uint32_t io = cl.inst_off + (uint32_t)abs_idx * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		uint8_t *root_buf = cl.cloned_roots[abs_idx];

		if (!root_buf || cl.runtime_handles[abs_idx] == NONE)
			continue;

		if (grp == 'bitm')
		{
			struct bitmap_group *bgrp = (struct bitmap_group *)root_buf;
			int b_idx;
			for (b_idx = 0; b_idx < bgrp->bitmaps.count && bgrp->bitmaps.address; b_idx++)
			{
				struct bitmap_data *bmp = (struct bitmap_data *)(XBOX_POINTER(uint8_t, bgrp->bitmaps.address) + (size_t)b_idx * 48u); /* AE hook */
				long req_bytes;
				uint32_t read_bytes, alloc_bytes;
				uint8_t *pix_mem;
				D3DBaseTexture *tex;

				/* Clear _bitmap_cached_bit (bit 7 = 0x80) so _texture_cache_bitmap_get_hardware_format returns bmp->hardware_format directly! */
				bmp->flags &= (uint16_t)~(1u << 7);
				bmp->tag_index = cl.runtime_handles[abs_idx];
				bmp->cache_block_index = NONE;
				bmp->base_address = XBOX_ADDRESS(NULL); /* AE hook */
				bmp->hardware_format = XBOX_ADDRESS(NULL); /* AE hook */

				if (bmp->width <= 0 || bmp->height <= 0 || bmp->depth <= 0 || bmp->pixels_offset < 2048)
					continue;
				req_bytes = rasterizer_xbox_bitmap_get_pixel_data_size(bmp);
				if (req_bytes <= 0)
					continue;
				read_bytes = (bmp->pixels_size > 0 && bmp->pixels_size < req_bytes) ? (uint32_t)bmp->pixels_size : (uint32_t)req_bytes;
				alloc_bytes = (uint32_t)req_bytes;
				if ((uint32_t)bmp->pixels_size > alloc_bytes && bmp->pixels_size <= 16 * 1024 * 1024)
					alloc_bytes = (uint32_t)bmp->pixels_size;

				pix_mem = (uint8_t *)forge_phys_alloc(alloc_bytes);
				tex = (D3DBaseTexture *)calloc(1, sizeof(D3DBaseTexture));
				if (!pix_mem || !tex)
				{
					free(tex);
					continue;
				}
				if (tag_import_pack_read(donor, (long)bmp->pixels_offset, (long)read_bytes, pix_mem) != (long)read_bytes)
				{
					free(tex);
					continue;
				}
				if (is_red_beam_variant && bmp->format == 14)
				{
					uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
					char const *btname = (name_addr >= cl.tag_base && (name_addr - cl.tag_base) < tag_sz)
						? (char const *)(tag_buf + (name_addr - cl.tag_base))
						: "";
					if (strstr(btname, "jackal shield") != NULL || strstr(btname, "static blue") != NULL)
						runtime_tint_dxt1_blue_to_red(pix_mem, read_bytes);
				}
				bmp->base_address = XBOX_ADDRESS(pix_mem); /* AE hook */
				halo_custom_edition_texels_channels(XBOX_POINTER(void, bmp->base_address), 0 /* _custom_edition_channels_xbox */); /* AE hook */

				/* Exact OpenCE texture_cache_initialize_hardware_format (source/cache/xbox_texture_cache.c:613-647) */
				tex->Data = 0;
				tex->Lock = 0;
				tex->Common = D3DCOMMON_TYPE_TEXTURE | 1;
				if (TEST_FLAG(bmp->flags, 4 /* _bitmap_linear_bit */))
				{
					tex->Format =
						(bitmap_format_to_d3d_linear_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						(1 << D3DFORMAT_MIPMAP_SHIFT) |
						(2 << D3DFORMAT_DIMENSION_SHIFT) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size =
						(((bitmap_mipmap_get_row_pitch(bmp, 0) + D3DTEXTURE_PITCH_ALIGNMENT - 1) / D3DTEXTURE_PITCH_ALIGNMENT - 1)
							<< D3DSIZE_PITCH_SHIFT) |
						((bmp->height - 1) << D3DSIZE_HEIGHT_SHIFT) |
						(bmp->width - 1);
				}
				else
				{
					tex->Format =
						(floor_log2(bmp->depth) << D3DFORMAT_PSIZE_SHIFT) |
						(floor_log2(bmp->height) << D3DFORMAT_VSIZE_SHIFT) |
						(floor_log2(bmp->width) << D3DFORMAT_USIZE_SHIFT) |
						(bitmap_format_to_d3d_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						((bmp->type == 1 /* _bitmap_type_3d */ ? 3 : 2) << D3DFORMAT_DIMENSION_SHIFT) |
						((rasterizer_xbox_bitmap_get_max_mipmap_count(bmp) + 1) << D3DFORMAT_MIPMAP_SHIFT) |
						(bmp->type == 2 /* _bitmap_type_cube_map */ ? D3DFORMAT_CUBEMAP : 0) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size = 0;
				}
				IDirect3DBaseTexture8_Register(tex, XBOX_POINTER(void, bmp->base_address)); /* AE hook */
				bmp->hardware_format = XBOX_ADDRESS(tex); /* AE hook */
			}
		}
		else if (grp == 'senv' || grp == 'soso' || grp == 'sotr' || grp == 'schi' ||
				 grp == 'scex' || grp == 'swat' || grp == 'sgla' || grp == 'smet' || grp == 'spla')
		{
			int16_t stype = 4;
			if (grp == 'senv') stype = 3;
			else if (grp == 'soso') stype = 4;
			else if (grp == 'sotr') stype = 5;
			else if (grp == 'schi' || grp == 'scex') stype = 6;
			else if (grp == 'swat') stype = 7;
			else if (grp == 'sgla') stype = 8;
			else if (grp == 'smet') stype = 9;
			else if (grp == 'spla') stype = 10;
			*(int16_t *)(root_buf + 0x24u) = stype;
		}
		else if (grp == 'snd!')
		{
			struct sound_definition *sdef = (struct sound_definition *)root_buf;
			int pr_i, pm_i;
			sdef->promotion_sound.index = NONE;
			sdef->promotion_count = 0;
			for (pr_i = 0; pr_i < sdef->pitch_ranges.count && sdef->pitch_ranges.address; pr_i++)
			{
				struct sound_pitch_range *pr = (struct sound_pitch_range *)(XBOX_POINTER(uint8_t, sdef->pitch_ranges.address) + (size_t)pr_i * sizeof(struct sound_pitch_range)); /* AE hook */
				pr->played_permutation_mask = 0xFFFFFFFFu;
				pr->previous_permutation_index = NONE;
				pr->forced_permutation_index = NONE;
				for (pm_i = 0; pm_i < pr->permutations.count && pr->permutations.address; pm_i++)
				{
					struct sound_permutation *perm = (struct sound_permutation *)(XBOX_POINTER(uint8_t, pr->permutations.address) + (size_t)pm_i * sizeof(struct sound_permutation)); /* AE hook */
					perm->unknown0 = NONE;
					perm->unknown1 = 0;
					perm->unknown2 = (unsigned long)cl.runtime_handles[abs_idx];
					perm->unknown3 = (unsigned long)cl.runtime_handles[abs_idx];
					perm->samples.address = XBOX_ADDRESS(NULL); /* AE hook */
					if (perm->samples.size > 0 && perm->samples.size <= MAXIMUM_SOUND_DATA_SIZE &&
						perm->samples.file_offset >= 2048)
					{
						void *sbuf = malloc((size_t)perm->samples.size);
						if (sbuf && tag_import_pack_read(donor, (long)perm->samples.file_offset,
								(long)perm->samples.size, sbuf) == (long)perm->samples.size)
						{
							perm->samples.address = XBOX_ADDRESS(sbuf); /* AE hook */
							perm->samples.file_offset = 0;
							forge_register_dyn_sound_perm(perm);
						}
						else
						{
							free(sbuf);
							perm->samples.size = 0;
						}
					}
					else
					{
						perm->samples.size = 0;
					}
				}
			}
		}
		else if (grp == 'mode')
		{
			uint32_t mod_off = data_addr - cl.tag_base;
			int32_t geom_cnt = *(int32_t const *)(root_buf + 0xD0u);
			uint8_t *geom_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(root_buf + 0xD4u)); /* AE hook */
			int32_t shdr_cnt = *(int32_t const *)(root_buf + 0xDCu);
			uint8_t *shdr_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(root_buf + 0xE0u)); /* AE hook */
			uint32_t src_geom_addr = *(uint32_t const *)(tag_buf + mod_off + 0xD4u);
			int g_idx, s_idx;

			if (!(*(float const *)(root_buf + 0x30u) > 0.0001f && *(float const *)(root_buf + 0x30u) < 10000.0f))
				*(float *)(root_buf + 0x30u) = 1.0f;
			if (!(*(float const *)(root_buf + 0x34u) > 0.0001f && *(float const *)(root_buf + 0x34u) < 10000.0f))
				*(float *)(root_buf + 0x34u) = 1.0f;

			/* Shift beam emitter root node down by 6.5 world units (~10m in Forge HUD) so its flare collar sits at the placement origin */
			if (is_red_beam_variant || _stricmp(req_path, "levels\\a30\\devices\\beam emitter\\beam emitter") == 0)
			{
				int32_t node_cnt = *(int32_t const *)(root_buf + 0xB8u);
				uint8_t *node_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(root_buf + 0xBCu)); /* AE hook */
				if (node_cnt > 0 && node_base)
					*(float *)(node_base + 48u) = -6.5f;
			}

			for (s_idx = 0; s_idx < shdr_cnt && shdr_base; s_idx++)
			{
				int16_t *perm_idx_ptr = (int16_t *)(shdr_base + (size_t)s_idx * 32u + 16u);
				if (*perm_idx_ptr < 0)
					*perm_idx_ptr = 0;
			}

			if (geom_cnt > 0 && geom_base && src_geom_addr >= cl.tag_base &&
				(uint64_t)(src_geom_addr - cl.tag_base) + (uint64_t)geom_cnt * 48u <= tag_sz)
			{
				for (g_idx = 0; g_idx < geom_cnt; g_idx++)
				{
					uint8_t *dst_geom = geom_base + (size_t)g_idx * 48u;
					uint32_t src_g_off = (src_geom_addr - cl.tag_base) + (uint32_t)g_idx * 48u;
					int32_t part_cnt = *(int32_t const *)(dst_geom + 36u);
					uint8_t *dst_parts = XBOX_POINTER(uint8_t, *(uint32_t const *)(dst_geom + 40u)); /* AE hook */
					uint32_t src_part_addr = *(uint32_t const *)(tag_buf + src_g_off + 40u);
					int p_idx;

					if (part_cnt <= 0 || !dst_parts || src_part_addr < cl.tag_base ||
						(uint64_t)(src_part_addr - cl.tag_base) + (uint64_t)part_cnt * 104u > tag_sz)
						continue;

					for (p_idx = 0; p_idx < part_cnt; p_idx++)
					{
						uint8_t *dst_part = dst_parts + (size_t)p_idx * 104u;
						uint32_t p_off = (src_part_addr - cl.tag_base) + (uint32_t)p_idx * 104u;
						int16_t tri_type = *(int16_t const *)(tag_buf + p_off + 68u);
						int32_t tri_count = *(int32_t const *)(tag_buf + p_off + 72u);
						uint32_t tb_hw_addr = *(uint32_t const *)(tag_buf + p_off + 80u);
						int16_t vtx_type = *(int16_t const *)(tag_buf + p_off + 84u);
						int32_t vtx_count = *(int32_t const *)(tag_buf + p_off + 88u);
						uint32_t vb_hw_addr = *(uint32_t const *)(tag_buf + p_off + 100u);

						memset(dst_part + 68u, 0, 36u);
						((struct vertex_buffer *)(dst_part + 84u))->type = _rasterizer_vertex_type_model_compressed;
						if (tri_count > 0 && vtx_count > 0 &&
							tb_hw_addr >= cl.tag_base && (tb_hw_addr - cl.tag_base) + 12u <= tag_sz &&
							vb_hw_addr >= cl.tag_base && (vb_hw_addr - cl.tag_base) + 12u <= tag_sz)
						{
							uint32_t ib_data_addr = *(uint32_t const *)(tag_buf + (tb_hw_addr - cl.tag_base) + 4u);
							uint32_t vb_data_addr = *(uint32_t const *)(tag_buf + (vb_hw_addr - cl.tag_base) + 4u);
							uint32_t comp_sz = (uint32_t)vtx_count * 32u;
							uint32_t idx_sz = (tri_type == _triangle_buffer_type_precompiled_strip)
								? ((uint32_t)tri_count + 2u) * 2u
								: (uint32_t)tri_count * 6u;

							if (ib_data_addr >= cl.tag_base && (ib_data_addr - cl.tag_base) + idx_sz <= tag_sz &&
								vb_data_addr >= cl.tag_base && (vb_data_addr - cl.tag_base) + comp_sz <= tag_sz)
							{
								void *hw_verts = forge_phys_alloc(comp_sz);
								void *hw_tris = forge_phys_alloc(idx_sz);
								if (hw_verts && hw_tris)
								{
									memcpy(hw_verts, tag_buf + (vb_data_addr - cl.tag_base), comp_sz);
									memcpy(hw_tris, tag_buf + (ib_data_addr - cl.tag_base), idx_sz);
									rasterizer_vertex_buffer_new(
										(struct vertex_buffer *)(dst_part + 84u),
										vtx_type ? vtx_type : _rasterizer_vertex_type_model_compressed,
										vtx_count,
										hw_verts,
										(long)comp_sz);
									rasterizer_triangle_buffer_new(
										(struct triangle_buffer *)(dst_part + 68u),
										tri_type,
										tri_count,
										hw_tris);
								}
							}
						}
					}
				}
			}
			models_fix_transparent_part_links();
		}
		else if (grp == 'vehi')
		{
			struct unit_definition *ud = (struct unit_definition *)root_buf;
			ud->unit.flags |= FLAG(_unit_has_entrance_points_inside_bounding_sphere_bit);
		}
		else if (grp == 'antr')
		{
			/* Clear _animation_world_relative_bit (0x0002) on scenery/character animations (e.g. flood_captain in d20)
			 * so placed objects render at their placed world position rather than teleporting to campaign cutscene coordinates. */
			int32_t anim_cnt = *(int32_t const *)(root_buf + 0x74u);
			uint8_t *anim_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(root_buf + 0x78u)); /* AE hook */
			float n0_tx = 0.0f, n0_ty = 0.0f, n0_tz = 0.0f;
			int m_k, a_idx;

			for (m_k = 0; m_k < cl.order_count; m_k++)
			{
				int m_abs = cl.order[m_k];
				uint32_t m_io = cl.inst_off + (uint32_t)m_abs * 32u;
				if (*(uint32_t const *)(tag_buf + m_io + 0x00) == 'mode' && cl.cloned_roots[m_abs])
				{
					uint8_t const *mod_buf = cl.cloned_roots[m_abs];
					int32_t n_cnt = *(int32_t const *)(mod_buf + 0xB8u);
					uint8_t const *n_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(mod_buf + 0xBCu)); /* AE hook */
					if (n_cnt > 0 && n_base)
					{
						float tx = *(float const *)(n_base + 40u);
						float ty = *(float const *)(n_base + 44u);
						float tz = *(float const *)(n_base + 48u);
						if (tx > -10.0f && tx < 10.0f && ty > -10.0f && ty < 10.0f && tz > -10.0f && tz < 10.0f)
						{
							n0_tx = tx;
							n0_ty = ty;
							n0_tz = tz;
						}
					}
					break;
				}
			}

			for (a_idx = 0; a_idx < anim_cnt && anim_base; a_idx++)
			{
				uint8_t *anim = anim_base + (size_t)a_idx * 180u;
				uint16_t aflags = *(uint16_t const *)(anim + 0x3Au);
				if (aflags & 0x0002u)
				{
					*(uint16_t *)(anim + 0x3Au) = (uint16_t)(aflags & ~0x0002u);
					if (!(aflags & 0x0001u))
					{
						uint32_t trans_flags0 = *(uint32_t const *)(anim + 0x5Cu);
						uint32_t rot_flags0 = *(uint32_t const *)(anim + 0x64u);
						uint32_t n0_trans_off = (rot_flags0 & 1u) ? 0u : 8u;
						if (!(trans_flags0 & 1u))
						{
							int32_t def_sz = *(int32_t const *)(anim + 0x80u);
							uint8_t *def_ptr = XBOX_POINTER(uint8_t, *(uint32_t const *)(anim + 0x88u)); /* AE hook */
							if (def_ptr && def_sz >= (int32_t)(n0_trans_off + 12u))
							{
								*(float *)(def_ptr + n0_trans_off + 0u) = n0_tx;
								*(float *)(def_ptr + n0_trans_off + 4u) = n0_ty;
								*(float *)(def_ptr + n0_trans_off + 8u) = n0_tz;
							}
						}
						else
						{
							int16_t frame_cnt = *(int16_t const *)(anim + 0x24u);
							int16_t frame_sz = *(int16_t const *)(anim + 0x26u);
							int32_t fdata_sz = *(int32_t const *)(anim + 0x94u);
							uint8_t *fdata_ptr = XBOX_POINTER(uint8_t, *(uint32_t const *)(anim + 0x9Cu)); /* AE hook */
							int f_i;
							for (f_i = 0; f_i < frame_cnt && frame_sz >= (int16_t)(n0_trans_off + 12u) && fdata_ptr; f_i++)
							{
								uint32_t f_off = (uint32_t)f_i * (uint32_t)frame_sz + n0_trans_off;
								if (f_off + 12u <= (uint32_t)fdata_sz)
								{
									*(float *)(fdata_ptr + f_off + 0u) = n0_tx;
									*(float *)(fdata_ptr + f_off + 4u) = n0_ty;
									*(float *)(fdata_ptr + f_off + 8u) = n0_tz;
								}
							}
						}
					}
				}
			}
		}
		else if (grp == 'ligh')
		{
			patch_light_definition_buffer(root_buf);
			if (is_red_beam_variant)
			{
				*(float *)(root_buf + 0x38u) = 1.0f;
				*(float *)(root_buf + 0x3Cu) = 1.0f;
				*(float *)(root_buf + 0x40u) = 0.12f;
				*(float *)(root_buf + 0x44u) = 0.08f;
				*(float *)(root_buf + 0x48u) = 1.0f;
				*(float *)(root_buf + 0x4Cu) = 1.0f;
				*(float *)(root_buf + 0x50u) = 0.18f;
				*(float *)(root_buf + 0x54u) = 0.10f;
			}
		}
		else if (grp == 'lens' && is_red_beam_variant)
		{
			int32_t refl_cnt = *(int32_t const *)(root_buf + 0xC4u);
			uint8_t *refl_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(root_buf + 0xC8u)); /* AE hook */
			int r_idx;
			for (r_idx = 0; r_idx < refl_cnt && refl_base; r_idx++)
			{
				uint8_t *rf = refl_base + (size_t)r_idx * 128u;
				*(float *)(rf + 0x40u) = 1.0f;
				*(float *)(rf + 0x44u) = 1.0f;
				*(float *)(rf + 0x48u) = 0.12f;
				*(float *)(rf + 0x4Cu) = 0.08f;
				*(float *)(rf + 0x50u) = 1.0f;
				*(float *)(rf + 0x54u) = 1.0f;
				*(float *)(rf + 0x58u) = 0.18f;
				*(float *)(rf + 0x5Cu) = 0.10f;
				*(float *)(rf + 0x60u) = 1.0f;
				*(float *)(rf + 0x64u) = 1.0f;
				*(float *)(rf + 0x68u) = 0.12f;
				*(float *)(rf + 0x6Cu) = 0.08f;
			}
		}
	}

	root_handle = cl.runtime_handles[root_abs_idx];
	if (root_handle != NONE)
	{
		if (req_group == 'vehi')
			patch_vehicle_tag_for_player_driving(root_handle, req_path);
		else if (req_group == 'scen' || req_group == 'mach' || req_group == 'lifi' || req_group == 'ctrl')
			patch_forge_device_and_light_tag_runtime(root_handle, req_group, req_path);
		if (s_dyn_tags.count < FORGE_MAX_DYNAMIC_TAGS)
		{
			struct forge_dynamic_tag_entry *entry = &s_dyn_tags.entries[s_dyn_tags.count++];
			memset(entry, 0, sizeof(*entry));
			entry->group_tag = req_group;
			snprintf(entry->tag_path, sizeof(entry->tag_path), "%s", req_path);
			entry->definition_index = root_handle;
			entry->part_count = 0; /* 0 = rendered natively by Halo's render_model! */
			entry->bounding_radius = cl.cloned_roots[root_abs_idx]
				? *(float const *)(cl.cloned_roots[root_abs_idx] + 0x04u)
				: 1.5f;
			if (!(entry->bounding_radius > 0.1f && entry->bounding_radius < 100.0f))
				entry->bounding_radius = 1.5f;
		}
		if (out_actual_group)
			*out_actual_group = (uint32_t)tag_get_group_tag(root_handle);
	}

	free(cl.visited);
	free(cl.order);
	free(cl.runtime_handles);
	free(cl.cloned_roots);
	return root_handle;
}

/* the pack's map open now: it is kept, a level's objects mostly coming from
the same one, and let go with the level */
static struct tag_import_donor *forge_pack_donor_of(int map_index)
{
	long pack = port_tag_packs_find(FORGE_PACK);

	if (pack < 0 || map_index < 0 || map_index >= (int)port_tag_packs_map_count(pack))
		return NULL;
	if (s_forge_pack_donor && s_forge_pack_donor_map == map_index)
		return s_forge_pack_donor;
	forge_pack_donor_close();
	s_forge_pack_donor = tag_import_pack_open(FORGE_PACK, port_tag_packs_map(pack, map_index));
	s_forge_pack_donor_map = s_forge_pack_donor ? map_index : -1;
	return s_forge_pack_donor;
}

/* a default tag: from the map of the pack the palette found it in, or any
map of the pack that has it (an old saved map's, off the palette) */
static long load_native_xbox_tag_runtime(
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
	long pack = port_tag_packs_find(FORGE_PACK);
	int count = pack < 0 ? 0 : (int)port_tag_packs_map_count(pack);
	int known = -1;
	int i;

	if (!req_path || !req_path[0] || !tag_pack_ready(FORGE_PACK))
		return NONE;
	for (i = 0; i < g_forge.catalog_count; i++)
	{
		if ((g_forge.catalog[i].group_tag == req_group ||
			 (req_group == 'actv' && g_forge.catalog[i].group_tag == FORGE_GROUP_AI_SPAWN)) &&
			g_forge.catalog[i].pad && _stricmp(g_forge.catalog[i].tag_path, req_path) == 0)
		{
			known = (int)g_forge.catalog[i].pad - 1;
			break;
		}
	}
	if (known >= 0)
	{
		struct tag_import_donor *donor = forge_pack_donor_of(known);
		long handle = donor ? load_native_xbox_tag_from_pack(donor, req_group, req_path, out_actual_group) : NONE;

		if (handle != NONE)
			return handle;
	}
	for (i = 0; i < count; i++)
	{
		struct tag_import_donor *donor;
		long handle;

		if (i == known)
			continue;
		donor = forge_pack_donor_of(i);
		handle = donor ? load_native_xbox_tag_from_pack(donor, req_group, req_path, out_actual_group) : NONE;
		if (handle != NONE)
			return handle;
	}
	return NONE;
}

static long register_dynamic_cached_tag(uint32_t group_tag, char const *tag_path, uint32_t *out_actual_group)
{
	int i;
	long native_idx;

	/* Check if already registered (or negative-cached) for the current map */
	for (i = 0; i < s_dyn_tags.count; i++)
	{
		if (s_dyn_tags.entries[i].group_tag == group_tag &&
			_stricmp(s_dyn_tags.entries[i].tag_path, tag_path) == 0)
		{
			if (s_dyn_tags.entries[i].definition_index == NONE)
				return NONE;
			if (out_actual_group)
				*out_actual_group = (uint32_t)tag_get_group_tag(s_dyn_tags.entries[i].definition_index);
			return s_dyn_tags.entries[i].definition_index;
		}
	}

	if (s_dyn_tags.count >= FORGE_MAX_DYNAMIC_TAGS)
		return NONE;

	/* 1. Check if an exact .ocetags package exists in custom_tags/ and load its complete tag tree natively! */
	native_idx = load_native_ocetags_package_runtime(group_tag, tag_path, out_actual_group);
	if (native_idx != NONE)
		return native_idx;

	/* 2. Load default Xbox tag natively from Xbox map cache using OpenCE's exact tag schema & hardware format! */
	native_idx = load_native_xbox_tag_runtime(group_tag, tag_path, out_actual_group);
	if (native_idx != NONE)
		return native_idx;

	/* 3. Negative-cache missing tag so subsequent frames do not re-scan disk */
	if (s_dyn_tags.count < FORGE_MAX_DYNAMIC_TAGS)
	{
		struct forge_dynamic_tag_entry *neg = &s_dyn_tags.entries[s_dyn_tags.count++];
		memset(neg, 0, sizeof(*neg));
		neg->group_tag = group_tag;
		snprintf(neg->tag_path, sizeof(neg->tag_path), "%s", tag_path);
		neg->definition_index = NONE;
	}

	return NONE;
}

long forge_resolve_engine_tag(uint32_t group_tag, char const *tag_path, uint32_t *out_actual_group)
{
	long idx;
	int i;

	if (out_actual_group)
		*out_actual_group = group_tag;
	if (!tag_path || !tag_path[0])
		return NONE;

	/* 0. If already dynamically registered (or negative-cached) for this map, return it directly */
	for (i = 0; i < s_dyn_tags.count; i++)
	{
		if (s_dyn_tags.entries[i].group_tag == group_tag &&
			_stricmp(s_dyn_tags.entries[i].tag_path, tag_path) == 0)
		{
			if (s_dyn_tags.entries[i].definition_index == NONE)
				return NONE;
			if (out_actual_group)
				*out_actual_group = (uint32_t)tag_get_group_tag(s_dyn_tags.entries[i].definition_index);
			return s_dyn_tags.entries[i].definition_index;
		}
	}

	/* 1. Check if the exact tag (or its native _mp / SP counterpart) is already in the map */
	idx = find_native_or_mp_variant_tag(group_tag, tag_path);
	if (idx != NONE)
	{
		if (group_tag == 'vehi')
			patch_vehicle_tag_for_player_driving(idx, tag_path);
		return idx;
	}

	/* 2. Dynamically load from custom_tags\*.ocetags or Xbox map cache into global_tag_instances! */
	idx = register_dynamic_cached_tag(group_tag, tag_path, out_actual_group);
	if (idx != NONE)
		return idx;

	return NONE;
}

static int ocetags_json_find_key_range(char const *buf, size_t start, size_t end, char const *key, size_t *out_val_pos)
{
	char pat[128];
	size_t pat_len;
	size_t p;

	snprintf(pat, sizeof(pat), "\"%s\"", key);
	pat_len = strlen(pat);
	if (end <= start || end - start < pat_len)
		return 0;
	for (p = start; p + pat_len <= end; p++)
	{
		if (memcmp(buf + p, pat, pat_len) == 0)
		{
			size_t q = p + pat_len;
			while (q < end && (buf[q] == ' ' || buf[q] == '\t' || buf[q] == '\r' || buf[q] == '\n'))
				q++;
			if (q < end && buf[q] == ':')
			{
				q++;
				while (q < end && (buf[q] == ' ' || buf[q] == '\t' || buf[q] == '\r' || buf[q] == '\n'))
					q++;
				*out_val_pos = q;
				return 1;
			}
		}
	}
	return 0;
}

static int ocetags_json_get_str(char const *buf, size_t start, size_t end, char const *key, char *out, size_t out_sz)
{
	size_t vpos = 0;
	size_t i = 0;

	if (!out || out_sz == 0)
		return 0;
	out[0] = 0;
	if (!ocetags_json_find_key_range(buf, start, end, key, &vpos))
		return 0;
	if (vpos >= end || buf[vpos] != '"')
		return 0;
	vpos++;
	while (vpos < end && buf[vpos] != '"' && i + 1 < out_sz)
	{
		if (buf[vpos] == '\\' && vpos + 1 < end)
		{
			vpos++;
			if (buf[vpos] == '\\' || buf[vpos] == '"' || buf[vpos] == '/')
				out[i++] = buf[vpos];
			else if (buf[vpos] == 'n')
				out[i++] = '\n';
			else
				out[i++] = buf[vpos];
		}
		else
		{
			out[i++] = buf[vpos];
		}
		vpos++;
	}
	out[i] = 0;
	return 1;
}

static int ocetags_json_get_u32(char const *buf, size_t start, size_t end, char const *key, uint32_t *out_val)
{
	size_t vpos = 0;
	if (!ocetags_json_find_key_range(buf, start, end, key, &vpos))
		return 0;
	if (vpos >= end)
		return 0;
	*out_val = (uint32_t)strtoul(buf + vpos, NULL, 10);
	return 1;
}

static int ocetags_find_matching_brace(char const *buf, size_t open_pos, size_t limit, char open_ch, char close_ch, size_t *out_close)
{
	int depth = 0;
	int in_str = 0;
	size_t p;

	for (p = open_pos; p < limit; p++)
	{
		char c = buf[p];
		if (in_str)
		{
			if (c == '\\' && p + 1 < limit)
				p++;
			else if (c == '"')
				in_str = 0;
		}
		else
		{
			if (c == '"')
				in_str = 1;
			else if (c == open_ch)
				depth++;
			else if (c == close_ch)
			{
				depth--;
				if (depth == 0)
				{
					*out_close = p;
					return 1;
				}
			}
		}
	}
	return 0;
}

static int ocetags_json_skip_value(char const *buf, size_t pos, size_t end, size_t *out_next)
{
	size_t close_pos = 0;
	while (pos < end && (buf[pos] == ' ' || buf[pos] == '\t' || buf[pos] == '\r' || buf[pos] == '\n'))
		pos++;
	if (pos >= end)
		return 0;
	if (buf[pos] == '{')
	{
		if (!ocetags_find_matching_brace(buf, pos, end, '{', '}', &close_pos))
			return 0;
		if (out_next)
			*out_next = close_pos + 1;
		return 1;
	}
	if (buf[pos] == '[')
	{
		if (!ocetags_find_matching_brace(buf, pos, end, '[', ']', &close_pos))
			return 0;
		if (out_next)
			*out_next = close_pos + 1;
		return 1;
	}
	if (buf[pos] == '"')
	{
		pos++;
		while (pos < end)
		{
			if (buf[pos] == '\\' && pos + 1 < end)
				pos += 2;
			else if (buf[pos] == '"')
			{
				pos++;
				break;
			}
			else
				pos++;
		}
		if (out_next)
			*out_next = pos;
		return 1;
	}
	while (pos < end && buf[pos] != ',' && buf[pos] != '}' && buf[pos] != ']' &&
		   buf[pos] != ' ' && buf[pos] != '\t' && buf[pos] != '\r' && buf[pos] != '\n')
		pos++;
	if (out_next)
		*out_next = pos;
	return 1;
}

struct ocetags_chunk_entry
{
	char id[64];
	char group[8];
	char path[256];
	char kind[48];
	uint32_t offset;
	uint32_t size;
	uint32_t vertex_count;
	uint32_t index_count;
	uint32_t width;
	uint32_t height;
	uint32_t format_code;
};

#define OCETAGS_MAX_CHUNKS 256

static uint32_t ocetags_group_str_to_fourcc(char const *grp)
{
	static struct { char const *name; uint32_t fourcc; } const s_map[] = {
		{ "actor", 'actr' },
		{ "actor_variant", 'actv' },
		{ "antenna", 'ant!' },
		{ "model_animations", 'antr' },
		{ "biped", 'bipd' },
		{ "bitmap", 'bitm' },
		{ "continuous_damage_effect", 'cdmg' },
		{ "model_collision_geometry", 'coll' },
		{ "color_table", 'colo' },
		{ "contrail", 'cont' },
		{ "device_control", 'ctrl' },
		{ "decal", 'deca' },
		{ "ui_widget_definition", 'DeLa' },
		{ "input_device_defaults", 'devc' },
		{ "device", 'devi' },
		{ "detail_object_collection", 'dobc' },
		{ "effect", 'effe' },
		{ "equipment", 'eqip' },
		{ "flag", 'flag' },
		{ "fog", 'fog ' },
		{ "font", 'font' },
		{ "material_effects", 'foot' },
		{ "garbage", 'garb' },
		{ "glow", 'glw!' },
		{ "grenade_hud_interface", 'grhi' },
		{ "hud_message_text", 'hmt ' },
		{ "hud_number", 'hud#' },
		{ "hud_globals", 'hudg' },
		{ "item", 'item' },
		{ "item_collection", 'itmc' },
		{ "damage_effect", 'jpt!' },
		{ "lens_flare", 'lens' },
		{ "lightning", 'elec' },
		{ "device_light_fixture", 'lifi' },
		{ "light", 'ligh' },
		{ "sound_looping", 'lsnd' },
		{ "device_machine", 'mach' },
		{ "globals", 'matg' },
		{ "meter", 'metr' },
		{ "light_volume", 'mgs2' },
		{ "gbxmodel", 'mod2' },
		{ "model", 'mode' },
		{ "multiplayer_scenario_description", 'mply' },
		{ "preferences_network_game", 'ngpr' },
		{ "object", 'obje' },
		{ "particle", 'part' },
		{ "particle_system", 'pctl' },
		{ "physics", 'phys' },
		{ "placeholder", 'plac' },
		{ "point_physics", 'pphy' },
		{ "projectile", 'proj' },
		{ "weather_particle_system", 'rain' },
		{ "scenario_structure_bsp", 'sbsp' },
		{ "scenery", 'scen' },
		{ "shader_transparent_chicago_extended", 'scex' },
		{ "shader_transparent_chicago", 'schi' },
		{ "scenario", 'scnr' },
		{ "shader_environment", 'senv' },
		{ "shader_transparent_glass", 'sgla' },
		{ "shader", 'shdr' },
		{ "sky", 'sky ' },
		{ "shader_transparent_meter", 'smet' },
		{ "sound", 'snd!' },
		{ "sound_environment", 'snde' },
		{ "shader_model", 'soso' },
		{ "shader_transparent_generic", 'sotr' },
		{ "ui_widget_collection", 'Soul' },
		{ "shader_transparent_plasma", 'spla' },
		{ "sound_scenery", 'ssce' },
		{ "string_list", 'str#' },
		{ "shader_transparent_water", 'swat' },
		{ "tag_collection", 'tagc' },
		{ "camera_track", 'trak' },
		{ "dialogue", 'udlg' },
		{ "unit_hud_interface", 'unhi' },
		{ "unit", 'unit' },
		{ "unicode_string_list", 'ustr' },
		{ "virtual_keyboard", 'vcky' },
		{ "vehicle", 'vehi' },
		{ "weapon", 'weap' },
		{ "wind", 'wind' },
		{ "weapon_hud_interface", 'wphi' }
	};
	size_t i;

	if (!grp || !grp[0])
		return 0;
	for (i = 0; i < sizeof(s_map) / sizeof(s_map[0]); i++)
	{
		if (_stricmp(grp, s_map[i].name) == 0)
			return s_map[i].fourcc;
	}
	if (strlen(grp) == 4)
	{
		return ((uint32_t)(uint8_t)grp[0] << 24) |
		       ((uint32_t)(uint8_t)grp[1] << 16) |
		       ((uint32_t)(uint8_t)grp[2] << 8) |
		       (uint32_t)(uint8_t)grp[3];
	}
	return 0;
}

static int ocetags_is_shader_fourcc(uint32_t fourcc)
{
	return (fourcc == 'soso' || fourcc == 'senv' || fourcc == 'schi' || fourcc == 'scex' ||
	        fourcc == 'sgla' || fourcc == 'smet' || fourcc == 'spla' || fourcc == 'swat' ||
	        fourcc == 'shdr');
}

static uint8_t *ocetags_json_decode_base64_key(
	char const *buf,
	size_t start,
	size_t end,
	char const *key,
	size_t *out_len)
{
	size_t vpos = 0;
	size_t b64_start, b64_len, cap;
	uint8_t *out;
	size_t o = 0;
	uint32_t val = 0;
	int valb = -8;
	size_t p;

	if (out_len)
		*out_len = 0;
	if (!ocetags_json_find_key_range(buf, start, end, key, &vpos))
		return NULL;
	if (vpos >= end || buf[vpos] != '"')
		return NULL;
	vpos++;
	b64_start = vpos;
	while (vpos < end && buf[vpos] != '"')
	{
		if (buf[vpos] == '\\' && vpos + 1 < end)
			vpos += 2;
		else
			vpos++;
	}
	b64_len = vpos - b64_start;
	cap = (b64_len * 3u) / 4u + 16u;
	out = (uint8_t *)malloc(cap);
	if (!out)
		return NULL;

	for (p = b64_start; p < vpos; p++)
	{
		unsigned char c = (unsigned char)buf[p];
		int d = -1;
		if (c >= 'A' && c <= 'Z') d = c - 'A';
		else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
		else if (c >= '0' && c <= '9') d = c - '0' + 52;
		else if (c == '+') d = 62;
		else if (c == '/') d = 63;
		else if (c == '=') break;
		else continue;

		val = (val << 6) | (uint32_t)d;
		valb += 6;
		if (valb >= 0)
		{
			if (o < cap)
				out[o++] = (uint8_t)((val >> valb) & 0xFFu);
			valb -= 8;
		}
	}
	if (out_len)
		*out_len = o;
	return out;
}

struct ocetags_v5_tag_span
{
	uint32_t fourcc;
	char group[32];
	char path[256];
	size_t obj_start;
	size_t obj_end;
	int is_2d_bitmap;
};

#define OCETAGS_V5_MAX_TAGS 512

struct ocetags_runtime_pkg_tag
{
	uint32_t fourcc;
	uint32_t actual_group;
	uint32_t orig_index;
	uint16_t orig_low;
	char path[256];
	size_t obj_start;
	size_t obj_end;
	uint8_t *tag_buf;
	size_t raw_len;
	long runtime_handle;
	int is_reused_existing;
};

static void ocetags_runtime_rebase_and_link_schema(
	struct tag_schema_definition const *def,
	uint8_t *elem_base,
	int32_t elem_count,
	uint8_t *tag_buf,
	size_t raw_len,
	uint32_t root_size,
	struct ocetags_runtime_pkg_tag const *pkg_tags,
	int pkg_tag_count,
	int depth)
{
	int32_t e;
	struct tag_schema_field const *field;
	uint32_t elem_size;

	if (!def || !def->fields || !elem_base || elem_count <= 0 || def->size <= 0 || depth > 10)
		return;

	elem_size = (uint32_t)def->size;
	for (e = 0; e < elem_count; e++)
	{
		uint8_t *elem_ptr = elem_base + (size_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;

			for (k = 0; k < fcount; k++)
			{
				uint32_t f_off = (uint32_t)field->offset + (uint32_t)k * fstride;
				uint8_t *f_ptr = elem_ptr + f_off;

				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_off + (uint32_t)sub_def->size <= elem_size)
					{
						ocetags_runtime_rebase_and_link_schema(
							sub_def, f_ptr, 1, tag_buf, raw_len, root_size,
							pkg_tags, pkg_tag_count, depth + 1);
					}
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_off + 12u <= elem_size)
					{
						int32_t cnt = *(int32_t const *)(f_ptr + 0);
						uint32_t rel = *(uint32_t const *)(f_ptr + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (cnt > 0 && cnt <= 65536 &&
							rel >= root_size &&
							(uint64_t)rel + (uint64_t)cnt * child_sz <= (uint64_t)raw_len)
						{
							uint8_t *child_base = tag_buf + rel;
							*(int32_t *)(f_ptr + 0) = cnt;
							*(uint32_t *)(f_ptr + 4) = XBOX_ADDRESS(child_base); /* AE hook */
							*(uint32_t *)(f_ptr + 8) = XBOX_ADDRESS(NULL); /* AE hook */
							ocetags_runtime_rebase_and_link_schema(
								child_def, child_base, cnt, tag_buf, raw_len, root_size,
								pkg_tags, pkg_tag_count, depth + 1);
						}
						else
						{
							memset(f_ptr, 0, 12);
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_off + 20u <= elem_size)
					{
						int32_t sz = *(int32_t const *)(f_ptr + 0);
						uint32_t rel = *(uint32_t const *)(f_ptr + 12);
						if (sz > 0 && sz <= 16 * 1024 * 1024 &&
							rel >= root_size &&
							(uint64_t)rel + (uint64_t)sz <= (uint64_t)raw_len)
						{
							*(int32_t *)(f_ptr + 0) = sz;
							*(uint32_t *)(f_ptr + 4) = 0;
							*(uint32_t *)(f_ptr + 8) = 0;
							*(uint32_t *)(f_ptr + 12) = XBOX_ADDRESS(tag_buf + rel); /* AE hook */
							*(uint32_t *)(f_ptr + 16) = XBOX_ADDRESS(NULL); /* AE hook */
						}
						else
						{
							memset(f_ptr, 0, 20);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_off + 16u <= elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(f_ptr + 12);
						long resolved_handle = NONE;
						uint32_t resolved_grp = *(uint32_t const *)(f_ptr + 0);
						if (ref_idx != -1 && ref_idx != 0)
						{
							uint16_t orig_low = (uint16_t)((uint32_t)ref_idx & 0xFFFFu);
							int m;
							for (m = 0; m < pkg_tag_count; m++)
							{
								if ((uint32_t)ref_idx == pkg_tags[m].orig_index || orig_low == pkg_tags[m].orig_low)
								{
									resolved_handle = pkg_tags[m].runtime_handle;
									resolved_grp = pkg_tags[m].actual_group;
									break;
								}
							}
						}
						if (resolved_grp == 'mod2') resolved_grp = 'mode';
						else if (resolved_grp == 'scex') resolved_grp = 'schi';
						*(uint32_t *)(f_ptr + 0) = (resolved_handle != NONE) ? resolved_grp : 0u;
						*(uint32_t *)(f_ptr + 4) = XBOX_ADDRESS(NULL); /* AE hook */
						*(int32_t *)(f_ptr + 8) = 0;
						*(int32_t *)(f_ptr + 12) = (int32_t)resolved_handle;
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_off + 4u <= elem_size)
					{
						int32_t ref_idx = *(int32_t const *)f_ptr;
						long resolved_handle = NONE;
						if (ref_idx != -1 && ref_idx != 0)
						{
							uint16_t orig_low = (uint16_t)((uint32_t)ref_idx & 0xFFFFu);
							int m;
							for (m = 0; m < pkg_tag_count; m++)
							{
								if ((uint32_t)ref_idx == pkg_tags[m].orig_index || orig_low == pkg_tags[m].orig_low)
								{
									resolved_handle = pkg_tags[m].runtime_handle;
									break;
								}
							}
						}
						*(int32_t *)f_ptr = (int32_t)resolved_handle;
					}
				}
			}
		}
	}
}

static long load_native_ocetags_package_from_json(
	char const *json,
	size_t json_len,
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
	size_t fmt_pos = 0, tags_pos = 0, tags_end = 0;
	struct ocetags_runtime_pkg_tag *pkg_tags = NULL;
	int pkg_tag_count = 0;
	int i;
	uint8_t **raw_assets = NULL;
	size_t *raw_asset_lens = NULL;
	int raw_asset_count = 0;
	long root_handle = NONE;

	if (!json || json_len < 32 || !req_path || !req_path[0])
		return NONE;
	if (!ocetags_json_find_key_range(json, 0, json_len, "formatVersion", &fmt_pos))
		return NONE;
	if (!ocetags_json_find_key_range(json, 0, json_len, "tags", &tags_pos) ||
		!ocetags_json_skip_value(json, tags_pos, json_len, &tags_end))
		return NONE;

	pkg_tags = (struct ocetags_runtime_pkg_tag *)calloc(OCETAGS_V5_MAX_TAGS, sizeof(*pkg_tags));
	if (!pkg_tags)
		return NONE;

	/* 1. Scan tags[] array and check if tags[0] matches (req_group, req_path) */
	{
		size_t p = tags_pos + 1;
		while (p < tags_end && pkg_tag_count < OCETAGS_V5_MAX_TAGS)
		{
			size_t obj_start, obj_end;
			while (p < tags_end && json[p] != '{' && json[p] != ']')
				p++;
			if (p >= tags_end || json[p] != '{')
				break;
			obj_start = p;
			if (!ocetags_json_skip_value(json, obj_start, tags_end, &obj_end))
				break;
			{
				struct ocetags_runtime_pkg_tag *pt = &pkg_tags[pkg_tag_count];
				char gstr[64];
				gstr[0] = 0;
				ocetags_json_get_str(json, obj_start, obj_end, "group", gstr, sizeof(gstr));
				ocetags_json_get_str(json, obj_start, obj_end, "path", pt->path, sizeof(pt->path));
				ocetags_json_get_u32(json, obj_start, obj_end, "originalIndex", &pt->orig_index);
				pt->orig_low = (uint16_t)(pt->orig_index & 0xFFFFu);
				pt->fourcc = ocetags_group_str_to_fourcc(gstr);
				pt->actual_group = (pt->fourcc == 'mod2') ? 'mode' : ((pt->fourcc == 'scex') ? 'schi' : pt->fourcc);
				pt->obj_start = obj_start;
				pt->obj_end = obj_end;
				pt->runtime_handle = NONE;
				if (pkg_tag_count == 0)
				{
					/* Match root tag! */
					if (pt->fourcc != req_group || _stricmp(pt->path, req_path) != 0)
					{
						free(pkg_tags);
						return NONE;
					}
				}
				pkg_tag_count++;
			}
			p = obj_end;
		}
	}

	if (pkg_tag_count <= 0)
	{
		free(pkg_tags);
		return NONE;
	}

	/* 2. Decode each tag's raw binary data and register in global_tag_instances.
	 * Strictly keep Custom Edition .ocetags packages using their own Custom Edition bitmaps,
	 * shaders, models, and sounds from the .ocetags package! */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		struct tag_schema_group const *schema_grp = forge_runtime_schema_group_get(pt->fourcc);
		long p0 = schema_grp ? schema_grp->parent_group_tags[0] : NONE;
		long p1 = schema_grp ? schema_grp->parent_group_tags[1] : NONE;
		uint8_t *decoded = NULL;
		size_t decoded_len = 0;
		size_t alloc_sz;

		if (pt->fourcc == 'scex')
		{
			p0 = 'shdr';
			p1 = NONE;
		}

		decoded = ocetags_json_decode_base64_key(json, pt->obj_start, pt->obj_end, "data", &decoded_len);
		if (!decoded || decoded_len == 0)
		{
			free(decoded);
			/* Only reuse an existing map tag if the .ocetags entry has no embedded raw data AND we are on a CE map */
			if (i > 0 && custom_edition_cache_tags_loaded())
			{
				long ex = tag_loaded((long)pt->actual_group, pt->path);
				if (ex != NONE)
				{
					pt->runtime_handle = ex;
					pt->is_reused_existing = 1;
				}
			}
			continue;
		}

		alloc_sz = (decoded_len > 4096u) ? (decoded_len + 256u) : 4096u;
		pt->tag_buf = (uint8_t *)calloc(1, alloc_sz);
		if (!pt->tag_buf)
		{
			free(decoded);
			continue;
		}
		memcpy(pt->tag_buf, decoded, decoded_len);
		pt->raw_len = decoded_len;
		free(decoded);

		{
			char reg_name[256];
			if (i > 0 && tag_loaded((long)pt->actual_group, pt->path) != NONE)
				snprintf(reg_name, sizeof(reg_name), "%s__f%d", pt->path, s_dyn_tags.count);
			else
				snprintf(reg_name, sizeof(reg_name), "%s", pt->path);
			pt->runtime_handle = append_runtime_tag_instance(
				(long)pt->actual_group, p0, p1, reg_name, pt->tag_buf);
		}
	}

	/* 4. Rebase internal tag_blocks/tag_data and link all tag_references across the package */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		struct tag_schema_group const *schema_grp;
		uint32_t root_sz;

		if (!pt->tag_buf || pt->is_reused_existing || pt->runtime_handle == NONE)
			continue;
		schema_grp = forge_runtime_schema_group_get(pt->fourcc);
		if (!schema_grp || !schema_grp->definition || schema_grp->definition->size <= 0)
			continue;
		root_sz = (uint32_t)schema_grp->definition->size;
		if (pt->raw_len < root_sz)
			continue;

		ocetags_runtime_rebase_and_link_schema(
			schema_grp->definition,
			pt->tag_buf,
			1,
			pt->tag_buf,
			pt->raw_len,
			root_sz,
			pkg_tags,
			pkg_tag_count,
			0);
	}

	/* 5. Decode per-tag embeddedRawDataAssets & embeddedRawDataPatches for Custom Edition 'bitm' and 'snd!' tags */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		if (!pt->tag_buf || pt->is_reused_existing || pt->runtime_handle == NONE)
			continue;

		if (pt->fourcc == 'bitm' && pt->raw_len >= 0x6Cu)
		{
			struct bitmap_group *bgrp = (struct bitmap_group *)pt->tag_buf;
			uint8_t **tag_assets = NULL;
			size_t *tag_asset_lens = NULL;
			int tag_asset_count = 0;
			size_t assets_pos = 0, assets_end = 0;
			size_t patches_pos = 0, patches_end = 0;
			unsigned char chan_order = 0;
			int k, bi, ai;

			*(uint32_t *)(pt->tag_buf + 0x38u) = 0; /* clear processed_pixel_data.size */
			*(uint32_t *)(pt->tag_buf + 0x44u) = XBOX_ADDRESS(NULL); /* AE hook */

			/* Check if any shader_model ('soso') uses this bitmap as multipurpose_map (+0xBC + 12 = +0xC8)
			 * without also using it as base_map (+0xB0), detail_map (+0xE8), or reflection_cube_map (+0x170) */
			{
				int used_mp = 0, used_rgba = 0;
				for (k = 0; k < pkg_tag_count; k++)
				{
					struct ocetags_runtime_pkg_tag *st = &pkg_tags[k];
					if (!st->tag_buf || st->fourcc != 'soso' || st->raw_len < 0x178u)
						continue;
					if (*(int32_t const *)(st->tag_buf + 0xC8u) == (int32_t)pt->runtime_handle)
						used_mp = 1;
					if (*(int32_t const *)(st->tag_buf + 0xB0u) == (int32_t)pt->runtime_handle ||
						*(int32_t const *)(st->tag_buf + 0xE8u) == (int32_t)pt->runtime_handle ||
						*(int32_t const *)(st->tag_buf + 0x170u) == (int32_t)pt->runtime_handle)
						used_rgba = 1;
				}
				if (used_mp && !used_rgba)
					chan_order = 1; /* _custom_edition_channels_multipurpose */
			}

			/* Decode per-tag embeddedRawDataAssets[] inside pt->obj_start .. pt->obj_end */
			if (ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataAssets", &assets_pos) &&
				ocetags_json_skip_value(json, assets_pos, pt->obj_end, &assets_end))
			{
				size_t p = assets_pos + 1;
				tag_assets = (uint8_t **)calloc(64, sizeof(uint8_t *));
				tag_asset_lens = (size_t *)calloc(64, sizeof(size_t));
				while (tag_assets && tag_asset_lens && p < assets_end && tag_asset_count < 64)
				{
					size_t a_start, a_end;
					while (p < assets_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= assets_end || json[p] != '{')
						break;
					a_start = p;
					if (!ocetags_json_skip_value(json, a_start, assets_end, &a_end))
						break;
					tag_assets[tag_asset_count] = ocetags_json_decode_base64_key(
						json, a_start, a_end, "data", &tag_asset_lens[tag_asset_count]);
					tag_asset_count++;
					p = a_end;
				}
			}

			for (bi = 0; bi < bgrp->bitmaps.count && bgrp->bitmaps.address; bi++)
			{
				struct bitmap_data *bmp = (struct bitmap_data *)(XBOX_POINTER(uint8_t, bgrp->bitmaps.address) + (size_t)bi * sizeof(struct bitmap_data)); /* AE hook */
				bmp->base_address = XBOX_ADDRESS(NULL); /* AE hook */
				bmp->hardware_format = XBOX_ADDRESS(NULL); /* AE hook */
				bmp->cache_block_index = NONE;
				bmp->tag_index = pt->runtime_handle;
			}

			/* Apply per-tag embeddedRawDataPatches[] (supports both patchOffset and offsetFieldOffset) */
			if (tag_asset_count > 0 &&
				ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataPatches", &patches_pos) &&
				ocetags_json_skip_value(json, patches_pos, pt->obj_end, &patches_end))
			{
				size_t p = patches_pos + 1;
				while (p < patches_end)
				{
					size_t p_start, p_end;
					uint32_t a_idx = 0xFFFFFFFFu, patch_off = 0xFFFFFFFFu;
					while (p < patches_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= patches_end || json[p] != '{')
						break;
					p_start = p;
					if (!ocetags_json_skip_value(json, p_start, patches_end, &p_end))
						break;
					if (ocetags_json_get_u32(json, p_start, p_end, "assetIndex", &a_idx) &&
						(ocetags_json_get_u32(json, p_start, p_end, "patchOffset", &patch_off) ||
						 ocetags_json_get_u32(json, p_start, p_end, "offsetFieldOffset", &patch_off)) &&
						a_idx < (uint32_t)tag_asset_count &&
						tag_assets[a_idx] && tag_asset_lens[a_idx] > 0 &&
						patch_off >= 24u && patch_off + 24u <= pt->raw_len)
					{
						struct bitmap_data *bmp = (struct bitmap_data *)(pt->tag_buf + (patch_off - 24u));
						bmp->base_address = XBOX_ADDRESS(tag_assets[a_idx]); /* AE hook */
						bmp->pixels_size = (long)tag_asset_lens[a_idx];
					}
					p = p_end;
				}
			}

			/* Finalize each bitmap_data entry: allocate in Xbox contiguous memory via forge_phys_alloc,
			 * convert Custom Edition texel layout via rasterizer_xbox_bitmap_rebuild_hardware_format,
			 * and initialize D3DBaseTexture hardware_format! */
			for (bi = 0; bi < bgrp->bitmaps.count && bgrp->bitmaps.address; bi++)
			{
				struct bitmap_data *bmp = (struct bitmap_data *)(XBOX_POINTER(uint8_t, bgrp->bitmaps.address) + (size_t)bi * sizeof(struct bitmap_data)); /* AE hook */
				uint8_t const *src_pixels = XBOX_POINTER(uint8_t const, bmp->base_address); /* AE hook */
				size_t src_len = (bmp->pixels_size > 0) ? (size_t)bmp->pixels_size : 0u;
				short max_mips;
				long req_bytes;
				size_t alloc_bytes;
				void *pix_copy;
				D3DBaseTexture *tex;

				if ((!src_pixels || src_len == 0) && bi < tag_asset_count && tag_assets[bi] && tag_asset_lens[bi] > 0)
				{
					src_pixels = tag_assets[bi];
					src_len = tag_asset_lens[bi];
				}
				bmp->base_address = XBOX_ADDRESS(NULL); /* AE hook */
				bmp->hardware_format = XBOX_ADDRESS(NULL); /* AE hook */

				bmp->signature = 'bitm';
				bmp->flags &= (unsigned short)~(0x80u | 0x08u | 0x10u | 0x100u);
				if (bmp->width > 0 && (bmp->width & (bmp->width - 1)) == 0 &&
					bmp->height > 0 && (bmp->height & (bmp->height - 1)) == 0)
					bmp->flags |= 0x01u; /* _bitmap_power_of_two_dimensions_bit */
				if (bmp->format >= 14 && bmp->format <= 16)
					bmp->flags |= 0x02u; /* _bitmap_compressed_bit */
				else
					bmp->flags &= (unsigned short)~0x02u;
				if (bmp->format == 17)
					bmp->flags |= 0x04u; /* _bitmap_palettized_bit */

				if (!src_pixels || src_len == 0 || bmp->width <= 0 || bmp->height <= 0 || bmp->depth <= 0)
					continue;

				max_mips = rasterizer_xbox_bitmap_get_max_mipmap_count(bmp);
				if (bmp->mipmap_count < 0)
					bmp->mipmap_count = 0;
				if (bmp->mipmap_count > max_mips)
					bmp->mipmap_count = max_mips;
				while (bmp->mipmap_count > 0 &&
					   (size_t)bitmap_get_pixel_data_size(bmp) > src_len)
				{
					bmp->mipmap_count--;
				}
				req_bytes = rasterizer_xbox_bitmap_get_pixel_data_size(bmp);
				if (req_bytes <= 0)
					continue;
				alloc_bytes = ((size_t)req_bytes > src_len) ? (size_t)req_bytes : src_len;
				pix_copy = forge_phys_alloc(alloc_bytes);
				tex = (D3DBaseTexture *)calloc(1, sizeof(D3DBaseTexture));
				if (!pix_copy || !tex)
				{
					free(tex);
					continue;
				}
				memcpy(pix_copy, src_pixels, src_len);
				bmp->base_address = XBOX_ADDRESS(pix_copy); /* AE hook */
				bmp->pixels_offset = 0;
				bmp->pixels_size = (long)alloc_bytes;
				bmp->tag_index = pt->runtime_handle;
				bmp->cache_block_index = NONE;

				halo_custom_edition_texels_channels(XBOX_POINTER(void, bmp->base_address), chan_order); /* AE hook */
				rasterizer_xbox_bitmap_rebuild_hardware_format(bmp);

				tex->Data = 0;
				tex->Lock = 0;
				tex->Common = D3DCOMMON_TYPE_TEXTURE | 1;
				if (TEST_FLAG(bmp->flags, 4 /* _bitmap_linear_bit */))
				{
					tex->Format =
						(bitmap_format_to_d3d_linear_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						(1 << D3DFORMAT_MIPMAP_SHIFT) |
						(2 << D3DFORMAT_DIMENSION_SHIFT) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size =
						(((bitmap_mipmap_get_row_pitch(bmp, 0) + D3DTEXTURE_PITCH_ALIGNMENT - 1) / D3DTEXTURE_PITCH_ALIGNMENT - 1)
							<< D3DSIZE_PITCH_SHIFT) |
						((bmp->height - 1) << D3DSIZE_HEIGHT_SHIFT) |
						(bmp->width - 1);
				}
				else
				{
					tex->Format =
						(floor_log2(bmp->depth) << D3DFORMAT_PSIZE_SHIFT) |
						(floor_log2(bmp->height) << D3DFORMAT_VSIZE_SHIFT) |
						(floor_log2(bmp->width) << D3DFORMAT_USIZE_SHIFT) |
						(bitmap_format_to_d3d_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						((bmp->type == 1 /* _bitmap_type_3d */ ? 3 : 2) << D3DFORMAT_DIMENSION_SHIFT) |
						((rasterizer_xbox_bitmap_get_max_mipmap_count(bmp) + 1) << D3DFORMAT_MIPMAP_SHIFT) |
						(bmp->type == 2 /* _bitmap_type_cube_map */ ? D3DFORMAT_CUBEMAP : 0) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size = 0;
				}
				IDirect3DBaseTexture8_Register(tex, XBOX_POINTER(void, bmp->base_address)); /* AE hook */
				bmp->hardware_format = XBOX_ADDRESS(tex); /* AE hook */
			}

			if (tag_assets)
			{
				for (ai = 0; ai < tag_asset_count; ai++)
					free(tag_assets[ai]);
				free(tag_assets);
			}
			free(tag_asset_lens);
		}
		else if (pt->fourcc == 'snd!' && pt->raw_len >= 0xA4u)
		{
			struct sound_definition *sdef = (struct sound_definition *)pt->tag_buf;
			uint8_t **tag_assets = NULL;
			size_t *tag_asset_lens = NULL;
			int tag_asset_count = 0;
			size_t assets_pos = 0, assets_end = 0;
			size_t patches_pos = 0, patches_end = 0;
			long channels = (sdef->encoding == 1) ? 2 : 1;
			long rate = (sdef->sample_rate == 1) ? 44100 : 22050;
			long encoded_rate = (channels == 1) ? 22050 : rate;
			int pr_i, pm_i, ai, flat_perm_idx = 0, valid_perms = 0;

			/* Decode per-tag embeddedRawDataAssets[] inside pt->obj_start .. pt->obj_end */
			if (ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataAssets", &assets_pos) &&
				ocetags_json_skip_value(json, assets_pos, pt->obj_end, &assets_end))
			{
				size_t p = assets_pos + 1;
				tag_assets = (uint8_t **)calloc(128, sizeof(uint8_t *));
				tag_asset_lens = (size_t *)calloc(128, sizeof(size_t));
				while (tag_assets && tag_asset_lens && p < assets_end && tag_asset_count < 128)
				{
					size_t a_start, a_end;
					while (p < assets_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= assets_end || json[p] != '{')
						break;
					a_start = p;
					if (!ocetags_json_skip_value(json, a_start, assets_end, &a_end))
						break;
					tag_assets[tag_asset_count] = ocetags_json_decode_base64_key(
						json, a_start, a_end, "data", &tag_asset_lens[tag_asset_count]);
					tag_asset_count++;
					p = a_end;
				}
			}

			sdef->promotion_sound.index = NONE;
			sdef->promotion_count = 0;
			if (channels == 1 && sdef->sample_rate == 1)
				sdef->sample_rate = 0; /* Xbox plays mono sounds at 22 kHz */
			if (sdef->minimum_distance <= 0.0f) sdef->minimum_distance = 1.0f;
			if (sdef->maximum_distance <= 0.0f) sdef->maximum_distance = 30.0f;
			if (sdef->random_pitch_bounds.lower <= 0.0f) sdef->random_pitch_bounds.lower = 1.0f;
			if (sdef->random_pitch_bounds.upper <= 0.0f) sdef->random_pitch_bounds.upper = 1.0f;
			if (sdef->zero_skip_fraction_modifier == 0.0f && sdef->one_skip_fraction_modifier == 0.0f)
			{
				sdef->zero_skip_fraction_modifier = 1.0f;
				sdef->one_skip_fraction_modifier = 1.0f;
			}
			if (sdef->zero_gain_modifier == 0.0f && sdef->one_gain_modifier == 0.0f)
			{
				sdef->zero_gain_modifier = 1.0f;
				sdef->one_gain_modifier = 1.0f;
			}
			if (sdef->zero_pitch_modifier == 0.0f && sdef->one_pitch_modifier == 0.0f)
			{
				sdef->zero_pitch_modifier = 1.0f;
				sdef->one_pitch_modifier = 1.0f;
			}

			/* Match embeddedRawDataPatches to permutations */
			if (tag_asset_count > 0 &&
				ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataPatches", &patches_pos) &&
				ocetags_json_skip_value(json, patches_pos, pt->obj_end, &patches_end))
			{
				size_t p = patches_pos + 1;
				while (p < patches_end)
				{
					size_t p_start, p_end;
					uint32_t a_idx = 0xFFFFFFFFu, patch_off = 0xFFFFFFFFu;
					while (p < patches_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= patches_end || json[p] != '{')
						break;
					p_start = p;
					if (!ocetags_json_skip_value(json, p_start, patches_end, &p_end))
						break;
					if (ocetags_json_get_u32(json, p_start, p_end, "assetIndex", &a_idx) &&
						(ocetags_json_get_u32(json, p_start, p_end, "patchOffset", &patch_off) ||
						 ocetags_json_get_u32(json, p_start, p_end, "offsetFieldOffset", &patch_off)) &&
						a_idx < (uint32_t)tag_asset_count &&
						tag_assets[a_idx] && tag_asset_lens[a_idx] > 0)
					{
						for (pr_i = 0; pr_i < sdef->pitch_ranges.count && sdef->pitch_ranges.address; pr_i++)
						{
							struct sound_pitch_range *pr = (struct sound_pitch_range *)(XBOX_POINTER(uint8_t, sdef->pitch_ranges.address) + (size_t)pr_i * sizeof(struct sound_pitch_range)); /* AE hook */
							for (pm_i = 0; pm_i < pr->permutations.count && pr->permutations.address; pm_i++)
							{
								struct sound_permutation *perm = (struct sound_permutation *)(XBOX_POINTER(uint8_t, pr->permutations.address) + (size_t)pm_i * sizeof(struct sound_permutation)); /* AE hook */
								uint32_t perm_off = (uint32_t)((uint8_t *)perm - pt->tag_buf);
								if (patch_off >= perm_off + 0x40u && patch_off < perm_off + 0x54u)
								{
									perm->samples.address = XBOX_ADDRESS(tag_assets[a_idx]); /* AE hook */
									perm->samples.size = (long)tag_asset_lens[a_idx];
								}
							}
						}
					}
					p = p_end;
				}
			}

			for (pr_i = 0; pr_i < sdef->pitch_ranges.count && sdef->pitch_ranges.address; pr_i++)
			{
				struct sound_pitch_range *pr = (struct sound_pitch_range *)(XBOX_POINTER(uint8_t, sdef->pitch_ranges.address) + (size_t)pr_i * sizeof(struct sound_pitch_range)); /* AE hook */
				if (pr->natural_pitch <= 0.0f) pr->natural_pitch = 1.0f;
				pr->playback_rate = 1.0f / pr->natural_pitch;
				pr->played_permutation_mask = 0xFFFFFFFFu;
				pr->previous_permutation_index = NONE;
				pr->forced_permutation_index = NONE;
				if (pr->actual_permutation_count <= 0 || pr->actual_permutation_count > pr->permutations.count)
					pr->actual_permutation_count = (short)pr->permutations.count;

				for (pm_i = 0; pm_i < pr->permutations.count && pr->permutations.address; pm_i++, flat_perm_idx++)
				{
					struct sound_permutation *perm = (struct sound_permutation *)(XBOX_POINTER(uint8_t, pr->permutations.address) + (size_t)pm_i * sizeof(struct sound_permutation)); /* AE hook */
					uint8_t const *src_snd = XBOX_POINTER(uint8_t const, perm->samples.address); /* AE hook */
					size_t src_snd_len = (perm->samples.size > 0) ? (size_t)perm->samples.size : 0u;
					unsigned long encoded_bytes = 0;
					void *encoded_buf = NULL;

					if ((!src_snd || src_snd_len == 0) && flat_perm_idx < tag_asset_count &&
						tag_assets[flat_perm_idx] && tag_asset_lens[flat_perm_idx] > 0)
					{
						src_snd = tag_assets[flat_perm_idx];
						src_snd_len = tag_asset_lens[flat_perm_idx];
					}
					perm->samples.address = XBOX_ADDRESS(NULL); /* AE hook */
					perm->samples.size = 0;
					perm->samples.file_offset = 0;
					perm->unknown0 = NONE;
					perm->unknown1 = 0;
					perm->unknown2 = (unsigned long)pt->runtime_handle;
					perm->unknown3 = (unsigned long)pt->runtime_handle;
					if (perm->gain <= 0.0f)
						perm->gain = 1.0f;

					if (src_snd && src_snd_len > 0)
					{
						if (perm->compression == 1 /* Xbox ADPCM */ && rate == encoded_rate)
						{
							encoded_buf = malloc(src_snd_len);
							if (encoded_buf)
							{
								memcpy(encoded_buf, src_snd, src_snd_len);
								encoded_bytes = (unsigned long)src_snd_len;
							}
						}
						else
						{
							encoded_buf = custom_edition_sounds_encode(
								src_snd,
								(long)src_snd_len,
								perm->compression,
								FALSE /* .ocetags raw data is little-endian */,
								channels,
								rate,
								encoded_rate,
								&encoded_bytes);
						}
						if (encoded_buf && encoded_bytes > 0 && encoded_bytes <= MAXIMUM_SOUND_DATA_SIZE)
						{
							perm->samples.address = XBOX_ADDRESS(encoded_buf); /* AE hook */
							perm->samples.size = (long)encoded_bytes;
							perm->compression = 1; /* SOUND_COMPRESSION_XBOX_ADPCM */
							perm->sample_buffer_size = 0;
							forge_register_dyn_sound_perm(perm);
							valid_perms++;
						}
						else
						{
							free(encoded_buf);
						}
					}
				}
			}
			sdef->compression = 1;
			if (valid_perms == 0)
			{
				sdef->pitch_ranges.count = 0;
				sdef->pitch_ranges.address = XBOX_ADDRESS(NULL); /* AE hook */
			}

			if (tag_assets)
			{
				for (ai = 0; ai < tag_asset_count; ai++)
					free(tag_assets[ai]);
				free(tag_assets);
			}
			free(tag_asset_lens);
		}
	}

	/* 6. Perform OpenCE runtime conversions on models ('mod2'->'mode'), shaders, animations, weapons, HUD, collision, and vehicles */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		if (!pt->tag_buf || pt->is_reused_existing || pt->runtime_handle == NONE)
			continue;

		/* Shader runtime type and scex->schi conversion */
		if (ocetags_is_shader_fourcc(pt->fourcc) && pt->raw_len >= 0x28u)
		{
			int16_t stype = 4;
			if (pt->fourcc == 'senv') stype = 3;
			else if (pt->fourcc == 'soso') stype = 4;
			else if (pt->fourcc == 'sotr') stype = 5;
			else if (pt->fourcc == 'schi' || pt->fourcc == 'scex') stype = 6;
			else if (pt->fourcc == 'swat') stype = 7;
			else if (pt->fourcc == 'sgla') stype = 8;
			else if (pt->fourcc == 'smet') stype = 9;
			else if (pt->fourcc == 'spla') stype = 10;
			*(int16_t *)(pt->tag_buf + 0x24u) = stype;

			if (pt->fourcc == 'scex' && pt->raw_len >= 0x78u)
			{
				if (*(int32_t const *)(pt->tag_buf + 0x54u) == 0 && *(int32_t const *)(pt->tag_buf + 0x60u) > 0)
				{
					memcpy(pt->tag_buf + 0x54u, pt->tag_buf + 0x60u, 12);
					*(uint32_t *)(pt->tag_buf + 0x60u) = *(uint32_t const *)(pt->tag_buf + 0x6Cu);
					memset(pt->tag_buf + 0x64u, 0, 8);
				}
			}
		}
		else if (pt->fourcc == 'mod2' && pt->raw_len >= 0xE8u)
		{
			uint8_t *mbuf = pt->tag_buf;
			uint32_t mflags = *(uint32_t const *)(mbuf + 0x00u);
			int has_local_nodes = ((mflags >> 1) & 1u) != 0;
			int32_t node_cnt = *(int32_t const *)(mbuf + 0xB8u);
			uint8_t *node_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(mbuf + 0xBCu)); /* AE hook */
			int32_t geom_cnt = *(int32_t const *)(mbuf + 0xD0u);
			uint8_t *geom_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(mbuf + 0xD4u)); /* AE hook */
			int32_t shdr_cnt = *(int32_t const *)(mbuf + 0xDCu);
			uint8_t *shdr_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(mbuf + 0xE0u)); /* AE hook */
			int n, s_idx;

			if (!(*(float const *)(mbuf + 0x30u) > 0.0001f && *(float const *)(mbuf + 0x30u) < 10000.0f))
				*(float *)(mbuf + 0x30u) = 1.0f;
			if (!(*(float const *)(mbuf + 0x34u) > 0.0001f && *(float const *)(mbuf + 0x34u) < 10000.0f))
				*(float *)(mbuf + 0x34u) = 1.0f;

			for (s_idx = 0; s_idx < shdr_cnt && shdr_base; s_idx++)
			{
				int16_t *perm_idx_ptr = (int16_t *)(shdr_base + (size_t)s_idx * 32u + 16u);
				if (*perm_idx_ptr < 0)
					*perm_idx_ptr = 0;
			}

			/* Ensure model nodes have valid quaternions and runtime_default_inverse_matrix */
			if (node_cnt > 0 && node_base)
			{
				struct real_matrix4x3 default_node_mats[64];
				int need_inv_calc = 0;

				for (n = 0; n < node_cnt; n++)
				{
					uint8_t *mn = node_base + (size_t)n * 156u;
					float qi = *(float const *)(mn + 0x34u);
					float qj = *(float const *)(mn + 0x38u);
					float qk = *(float const *)(mn + 0x3Cu);
					float qw = *(float const *)(mn + 0x40u);
					float mscale = *(float const *)(mn + 0x68u);
					if (qi == 0.0f && qj == 0.0f && qk == 0.0f && qw == 0.0f)
						*(float *)(mn + 0x40u) = 1.0f;
					if (!(mscale > 0.0001f && mscale < 1000.0f))
						need_inv_calc = 1;
				}
				if (need_inv_calc)
				{
					memset(default_node_mats, 0, sizeof(default_node_mats));
					model_get_node_matrices((struct model const *)mbuf, default_node_mats, NULL, NULL, NULL);
					for (n = 0; n < node_cnt && n < 64; n++)
					{
						uint8_t *mn = node_base + (size_t)n * 156u;
						float mscale = *(float const *)(mn + 0x68u);
						if (!(mscale > 0.0001f && mscale < 1000.0f))
						{
							if (default_node_mats[n].scale > 0.0001f)
							{
								matrix4x3_inverse(
									&default_node_mats[n],
									(struct real_matrix4x3 *)(mn + 0x68u));
							}
							else
							{
								memset(mn + 0x68u, 0, 52);
								*(float *)(mn + 0x68u) = 1.0f;
								*(float *)(mn + 0x6Cu) = 1.0f;
								*(float *)(mn + 0x7Cu) = 1.0f;
								*(float *)(mn + 0x8Cu) = 1.0f;
							}
						}
					}
				}
				for (n = 0; n < 5; n++)
				{
					int16_t *cutoff = (int16_t *)(mbuf + 0x1Cu + (size_t)n * 2u);
					if (*cutoff <= 0 || *cutoff > node_cnt)
						*cutoff = (int16_t)(node_cnt - 1);
				}
			}

			/* Convert 132-byte custom_edition_model_part elements in-place to 104-byte model_geometry_part
			 * and build hardware vertex/triangle buffers from gbxmodelGeometryParts[] */
			{
				size_t gparts_arr = 0, gparts_end = 0;
				int has_gparts = (ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "gbxmodelGeometryParts", &gparts_arr) &&
								  ocetags_json_skip_value(json, gparts_arr, pt->obj_end, &gparts_end));
				int g_idx;

				for (g_idx = 0; g_idx < geom_cnt && geom_base; g_idx++)
				{
					uint8_t *geom = geom_base + (size_t)g_idx * 48u;
					int32_t part_cnt = *(int32_t const *)(geom + 36u);
					uint8_t *parts_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(geom + 40u)); /* AE hook */
					int p_idx;

					if (!parts_base || part_cnt <= 0)
						continue;

					for (p_idx = 0; p_idx < part_cnt; p_idx++)
					{
						uint8_t ce_part[132];
						uint8_t xbox_part[104];
						uint8_t *verts_raw = NULL;
						size_t verts_bytes = 0;
						uint8_t *tris_raw = NULL;
						size_t tris_bytes = 0;

						memcpy(ce_part, parts_base + (size_t)p_idx * 132u, 132u);
						memset(xbox_part, 0, sizeof(xbox_part));
						memcpy(xbox_part, ce_part, 32u);
						*(uint32_t *)(xbox_part + 0u) &= ~0x02u; /* clear _model_geometry_part_local_nodes_bit */
						xbox_part[6] = 0xFFu; /* previous_part_index = -1 */
						xbox_part[7] = 0xFFu; /* next_part_index = -1 */
						if (has_local_nodes)
						{
							int16_t lnc = (int16_t)ce_part[107u];
							uint8_t const *ln = ce_part + 108u;
							int16_t c0 = *(int16_t const *)(xbox_part + 8u);
							int16_t c1 = *(int16_t const *)(xbox_part + 10u);
							if (lnc > 0 && lnc <= 22)
							{
								if (c0 >= 0 && c0 < lnc) *(int16_t *)(xbox_part + 8u) = (int16_t)ln[c0];
								if (c1 >= 0 && c1 < lnc) *(int16_t *)(xbox_part + 10u) = (int16_t)ln[c1];
							}
						}
						((struct vertex_buffer *)(xbox_part + 84u))->type = _rasterizer_vertex_type_model_compressed;

						/* Find matching entry in gbxmodelGeometryParts[] */
						if (has_gparts)
						{
							size_t gp = gparts_arr + 1;
							while (gp < gparts_end)
							{
								size_t gp_start, gp_end;
								uint32_t j_geom = 0xFFFFFFFFu, j_part = 0xFFFFFFFFu;
								while (gp < gparts_end && json[gp] != '{' && json[gp] != ']')
									gp++;
								if (gp >= gparts_end || json[gp] != '{')
									break;
								gp_start = gp;
								if (!ocetags_json_skip_value(json, gp_start, gparts_end, &gp_end))
									break;
								if (ocetags_json_get_u32(json, gp_start, gp_end, "geometryIndex", &j_geom) &&
									ocetags_json_get_u32(json, gp_start, gp_end, "partIndex", &j_part) &&
									j_geom == (uint32_t)g_idx && j_part == (uint32_t)p_idx)
								{
									verts_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "vertexData", &verts_bytes);
									if (!verts_raw)
										verts_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "vertices", &verts_bytes);
									tris_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "triangleData", &tris_bytes);
									if (!tris_raw)
										tris_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "triangles", &tris_bytes);
									break;
								}
								gp = gp_end;
							}
						}

						if (verts_raw && tris_raw && verts_bytes >= 3u * 68u && tris_bytes >= 6u)
						{
							int32_t vtx_count = (int32_t)(verts_bytes / 68u);
							int32_t idx_count = (int32_t)(tris_bytes / 2u);
							uint16_t const *raw_ibuf = (uint16_t const *)tris_raw;
							int16_t local_node_count = (int16_t)ce_part[107u];
							uint8_t const *local_nodes = ce_part + 108u;
							int v, c;

							while (idx_count > 3 && raw_ibuf[idx_count - 1] >= (uint16_t)vtx_count)
								idx_count--;

							for (v = 0; v < vtx_count; v++)
							{
								uint8_t *vp = verts_raw + (size_t)v * 68u;
								int16_t n0 = *(int16_t const *)(vp + 56u);
								int16_t n1 = *(int16_t const *)(vp + 58u);
								float w0 = *(float const *)(vp + 60u);
								float w1 = *(float const *)(vp + 64u);

								for (c = 0; c < 9; c++)
								{
									float val = *(float const *)(vp + 12u + (size_t)c * 4u);
									if (!(val >= -1.0f && val <= 1.0f))
										*(float *)(vp + 12u + (size_t)c * 4u) = (c % 3 == 0) ? 1.0f : 0.0f;
								}

								if (has_local_nodes && local_node_count > 0 && local_node_count <= 22)
								{
									n0 = (n0 >= 0 && n0 < local_node_count) ? (int16_t)local_nodes[n0] : 0;
									n1 = (n1 >= 0 && n1 < local_node_count) ? (int16_t)local_nodes[n1] : 0;
								}
								if (node_cnt > 0)
								{
									if (n0 < 0 || n0 >= node_cnt) n0 = 0;
									if (n1 < 0 || n1 >= node_cnt) n1 = 0;
								}
								else
								{
									n0 = 0;
									n1 = 0;
								}
								if (!(w0 >= 0.0f && w0 <= 1.0f) || !(w1 >= 0.0f && w1 <= 1.0f))
								{
									w0 = 1.0f;
									w1 = 0.0f;
								}
								*(int16_t *)(vp + 56u) = n0;
								*(int16_t *)(vp + 58u) = n1;
								*(float *)(vp + 60u) = w0;
								*(float *)(vp + 64u) = w1;
							}

							if (vtx_count >= 3 && idx_count >= 3)
							{
								uint32_t uncomp_sz = (uint32_t)vtx_count * 68u;
								uint32_t comp_sz = (uint32_t)vtx_count * 32u;
								uint32_t idx_sz = (uint32_t)idx_count * 2u;
								void *hw_verts = forge_phys_alloc(comp_sz);
								void *hw_tris = forge_phys_alloc(idx_sz);
								if (hw_verts && hw_tris)
								{
									uint16_t *tris_u16 = (uint16_t *)hw_tris;
									int32_t k;
									rasterizer_geometry_compress_vertices(
										_rasterizer_vertex_type_model_uncompressed,
										vtx_count,
										hw_verts,
										(long)comp_sz,
										verts_raw,
										(long)uncomp_sz);
									memcpy(hw_tris, tris_raw, idx_sz);
									for (k = 0; k < idx_count; k++)
									{
										if (tris_u16[k] >= (uint16_t)vtx_count)
											tris_u16[k] = (k > 0 && tris_u16[k - 1] < (uint16_t)vtx_count) ? tris_u16[k - 1] : 0u;
									}
									rasterizer_vertex_buffer_new(
										(struct vertex_buffer *)(xbox_part + 84u),
										_rasterizer_vertex_type_model_compressed,
										vtx_count,
										hw_verts,
										(long)comp_sz);
									rasterizer_triangle_buffer_new(
										(struct triangle_buffer *)(xbox_part + 68u),
										_triangle_buffer_type_precompiled_strip,
										idx_count - 2,
										hw_tris);
								}
							}
						}

						free(verts_raw);
						free(tris_raw);
						memcpy(parts_base + (size_t)p_idx * 104u, xbox_part, 104u);
					}
				}
			}

			/* Clear _gbxmodel_parts_have_local_nodes_bit (bit 1) now that local nodes are resolved */
			*(uint32_t *)(mbuf + 0x00u) = mflags & ~(1u << 1);
			models_fix_transparent_part_links();
		}
		else if (pt->fourcc == 'antr' && pt->raw_len >= 0x80u)
		{
			/* Sanitize animation_graph object_overlays (+0x00, stride 20) and clamp animations node_count */
			int32_t ov_cnt = *(int32_t const *)(pt->tag_buf + 0x00u);
			uint8_t *ov_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(pt->tag_buf + 0x04u)); /* AE hook */
			int32_t anim_cnt = *(int32_t const *)(pt->tag_buf + 0x74u);
			uint8_t *anim_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(pt->tag_buf + 0x78u)); /* AE hook */
			int32_t node_cnt = *(int32_t const *)(pt->tag_buf + 0x5Cu);
			int a;

			if (ov_cnt > 0 && ov_base)
			{
				for (a = 0; a < ov_cnt; a++)
				{
					uint8_t *ov = ov_base + (size_t)a * 20u;
					int16_t fn_ctrl = *(int16_t const *)(ov + 0x04u);
					int16_t fn_idx = *(int16_t const *)(ov + 0x06u);
					if (fn_ctrl < 0 || fn_ctrl > 3 || fn_idx < 0 || fn_idx >= 4)
					{
						*(int16_t *)(ov + 0x02u) = -1;
						*(int16_t *)(ov + 0x04u) = 0;
						*(int16_t *)(ov + 0x06u) = 0;
					}
				}
			}
			if (anim_cnt > 0 && anim_base)
			{
				for (a = 0; a < anim_cnt; a++)
				{
					uint8_t *an = anim_base + (size_t)a * 180u;
					int16_t nc = *(int16_t const *)(an + 0x3Au);
					if (nc < 0 || nc > 64 || (node_cnt > 0 && nc > node_cnt))
						*(int16_t *)(an + 0x3Au) = (int16_t)((node_cnt > 0 && node_cnt <= 64) ? node_cnt : 1);
				}
			}
		}
		else if (pt->fourcc == 'weap' && pt->raw_len >= 0x508u)
		{
			/* Convert Custom Edition weapon exported function modes at +0x330 (17->15, 18->16) */
			int f;
			for (f = 0; f < 4; f++)
			{
				int16_t *fn_mode = (int16_t *)(pt->tag_buf + 0x330u + (size_t)f * 2u);
				if (*fn_mode == 17) *fn_mode = 15;
				else if (*fn_mode == 18) *fn_mode = 16;
			}
		}
		else if (pt->fourcc == 'coll' && pt->raw_len >= 0x298u)
		{
			/* Sanitize collision pathfinding box and Mantle 0x0BDEA5CE vertex offsets */
			int pf_i, n;
			int32_t reg_cnt = *(int32_t const *)(pt->tag_buf + 0x240u);
			int32_t cn_cnt = *(int32_t const *)(pt->tag_buf + 0x28Cu);
			uint8_t *cn_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(pt->tag_buf + 0x290u)); /* AE hook */

			for (pf_i = 0; pf_i < 6; pf_i++)
			{
				float pfv = *(float const *)(pt->tag_buf + 0x268u + (size_t)pf_i * 4u);
				if (!(pfv >= -500.0f && pfv <= 500.0f))
					*(float *)(pt->tag_buf + 0x268u + (size_t)pf_i * 4u) = (pf_i & 1) ? 2.5f : -2.5f;
			}
			if (cn_cnt > 0 && cn_base)
			{
				for (n = 0; n < cn_cnt; n++)
				{
					uint8_t *cn = cn_base + (size_t)n * 64u;
					int16_t r_idx = *(int16_t const *)(cn + 0x20u);
					int32_t bsp_cnt = *(int32_t const *)(cn + 0x34u);
					uint8_t *bsp_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(cn + 0x38u)); /* AE hook */
					int b;

					if (r_idx != -1 && (r_idx < 0 || r_idx >= reg_cnt))
						*(int16_t *)(cn + 0x20u) = (reg_cnt > 0) ? 0 : -1;
					for (b = 0; b < bsp_cnt && bsp_base; b++)
					{
						uint8_t *bsp = bsp_base + (size_t)b * 96u;
						int32_t v_cnt = *(int32_t const *)(bsp + 7u * 12u + 0u);
						uint8_t *v_base = XBOX_POINTER(uint8_t, *(uint32_t const *)(bsp + 7u * 12u + 4u)); /* AE hook */
						int v_i, c_i;
						for (v_i = 0; v_i < v_cnt && v_base; v_i++)
						{
							uint8_t *cv = v_base + (size_t)v_i * 16u;
							for (c_i = 0; c_i < 3; c_i++)
							{
								float val = *(float const *)(cv + (size_t)c_i * 4u);
								if (!(val >= -500.0f && val <= 500.0f))
								{
									uint32_t raw_bits = *(uint32_t const *)(cv + (size_t)c_i * 4u);
									uint32_t fixed_bits = raw_bits - 0x0BDEA5CEu;
									float fixed_val = 0.0f;
									if (fixed_bits >= 0x40440000u && fixed_bits <= 0x41B40000u)
										memcpy(&fixed_val, &fixed_bits, 4);
									*(float *)(cv + (size_t)c_i * 4u) = fixed_val;
								}
							}
						}
					}
				}
			}
		}
		else if (pt->fourcc == 'vehi' && pt->raw_len >= 0x3F0u)
		{
			patch_vehicle_tag_for_player_driving(pt->runtime_handle, pt->path);
		}
	}

	root_handle = pkg_tags[0].runtime_handle;
	if (root_handle != NONE && s_dyn_tags.count < FORGE_MAX_DYNAMIC_TAGS)
	{
		struct forge_dynamic_tag_entry *entry = &s_dyn_tags.entries[s_dyn_tags.count++];
		memset(entry, 0, sizeof(*entry));
		entry->group_tag = req_group;
		snprintf(entry->tag_path, sizeof(entry->tag_path), "%s", req_path);
		entry->definition_index = root_handle;
		entry->part_count = 0; /* 0 = let Halo's native render_model render the exact gbxmodel with all nodes/regions/shaders! */
		entry->bounding_radius = (pkg_tags[0].raw_len >= 0x08u) ? *(float const *)(pkg_tags[0].tag_buf + 0x04u) : 1.5f;
		if (!(entry->bounding_radius > 0.1f && entry->bounding_radius < 100.0f))
			entry->bounding_radius = 1.5f;
	}

	if (out_actual_group && root_handle != NONE)
		*out_actual_group = pkg_tags[0].actual_group;

	if (raw_assets)
	{
		for (i = 0; i < raw_asset_count; i++)
			free(raw_assets[i]);
		free(raw_assets);
	}
	free(raw_asset_lens);
	free(pkg_tags);
	return root_handle;
}

static void forge_make_safe_ocetags_stem(uint32_t grp, char const *tag_path, char *out_stem, size_t out_sz);
static uint8_t *forge_read_and_decompress_ocetags_file(char const *path, size_t *out_len, int *out_was_compressed);

static long load_native_ocetags_package_runtime(
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
#if FORGE_HOST_FILES
	static char const *const s_ocetags_dirs[] = {
		"OpenCE Forge/custom_tags",
		"OpenCE Forge/cached_tags",
		"Faber_data/custom_tags",
		"custom_tags",
		"Faber_data/ocetags",
		"Faber_data/cached_tags",
		"Faber_data",
		"ocetags",
		"custom_maps",
		"maps",
		"mods"
	};
	char safe_stem[128];
	int d;

	if (!req_path || !req_path[0])
		return NONE;

	/* Fast O(1) direct lookup by <safe_stem>.ocetags first */
	forge_make_safe_ocetags_stem(req_group, req_path, safe_stem, sizeof(safe_stem));
	for (d = 0; d < (int)(sizeof(s_ocetags_dirs) / sizeof(s_ocetags_dirs[0])); d++)
	{
		char direct_path[512];
		size_t ulen = 0;
		int was_c = 0;
		uint8_t *ubuf;
		snprintf(direct_path, sizeof(direct_path), "%s/%s.ocetags", s_ocetags_dirs[d], safe_stem);
		ubuf = forge_read_and_decompress_ocetags_file(direct_path, &ulen, &was_c);
		if (ubuf && ulen > 32u)
		{
			long hdl = load_native_ocetags_package_from_json(
				(char const *)ubuf, ulen, req_group, req_path, out_actual_group);
			free(ubuf);
			if (hdl != NONE)
				return hdl;
		}
		else
		{
			free(ubuf);
		}
	}

	for (d = 0; d < (int)(sizeof(s_ocetags_dirs) / sizeof(s_ocetags_dirs[0])); d++)
	{
		char pattern[512];
		WIN32_FIND_DATAA fd;
		HANDLE h;
		snprintf(pattern, sizeof(pattern), "%s/*.ocetags", s_ocetags_dirs[d]);
		h = FindFirstFileA(pattern, &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					char full_path[512];
					FILE *fp;
					snprintf(full_path, sizeof(full_path), "%s/%s", s_ocetags_dirs[d], fd.cFileName);
					fp = fopen(full_path, "rb");
					if (fp)
					{
						long fsz;
						fseek(fp, 0, SEEK_END);
						fsz = ftell(fp);
						fseek(fp, 0, SEEK_SET);
						if (fsz > 16 && fsz <= 128 * 1024 * 1024)
						{
							uint8_t magic_hdr[8];
							size_t mn = fread(magic_hdr, 1, 8, fp);
							fseek(fp, 0, SEEK_SET);
							if (mn == 8 && *(uint32_t const *)magic_hdr == 0x5A45434Fu)
							{
								uint32_t uncomp_sz = *(uint32_t const *)(magic_hdr + 4);
								if (uncomp_sz > 32u && uncomp_sz <= 256u * 1024u * 1024u)
								{
									uint8_t *comp_buf = (uint8_t *)malloc((size_t)fsz);
									char *json_buf = (char *)malloc((size_t)uncomp_sz + 1u);
									if (comp_buf && json_buf && fread(comp_buf, 1, (size_t)fsz, fp) == (size_t)fsz)
									{
										uLongf dst_len = (uLongf)uncomp_sz;
										if (uncompress((Bytef *)json_buf, &dst_len, (Bytef const *)(comp_buf + 8), (uLong)((size_t)fsz - 8u)) == Z_OK)
										{
											json_buf[dst_len] = 0;
											if (strstr(json_buf, "\"formatVersion\"") != NULL)
											{
												long hdl = load_native_ocetags_package_from_json(
													json_buf, (size_t)dst_len, req_group, req_path, out_actual_group);
												if (hdl != NONE)
												{
													free(comp_buf);
													free(json_buf);
													fclose(fp);
													CloseHandle(h);
													return hdl;
												}
											}
										}
									}
									free(comp_buf);
									free(json_buf);
								}
							}
							else if (fsz > 32)
							{
								/* Read a 2048-byte prefix first to quickly check if tags[0].path matches req_path */
								char hdr_peek[2049];
								size_t peek_n = fread(hdr_peek, 1, 2048, fp);
								hdr_peek[peek_n] = 0;
								if (strstr(hdr_peek, "\"formatVersion\"") != NULL)
								{
									char *json_buf;
									fseek(fp, 0, SEEK_SET);
									json_buf = (char *)malloc((size_t)fsz + 1u);
									if (json_buf)
									{
										if (fread(json_buf, 1, (size_t)fsz, fp) == (size_t)fsz)
										{
											long hdl;
											json_buf[fsz] = 0;
											hdl = load_native_ocetags_package_from_json(
												json_buf, (size_t)fsz, req_group, req_path, out_actual_group);
											if (hdl != NONE)
											{
												free(json_buf);
												fclose(fp);
												CloseHandle(h);
												return hdl;
											}
										}
										free(json_buf);
									}
								}
							}
						}
						fclose(fp);
					}
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}
#else
	(void)req_group;
	(void)req_path;
	(void)out_actual_group;
#endif
	return NONE;
}

#define OCETAGS_COMPRESSED_MAGIC 0x5A45434Fu /* 'OCEZ' */

#undef compress2
extern int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);

static uint8_t *forge_read_and_decompress_ocetags_file(char const *path, size_t *out_len, int *out_was_compressed)
{
	FILE *fp;
	size_t fsz;
	uint8_t *fbuf;

	if (out_len) *out_len = 0;
	if (out_was_compressed) *out_was_compressed = 0;
	if (!path || !path[0])
		return NULL;
	fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	fseek(fp, 0, SEEK_END);
	fsz = (size_t)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (fsz < 12u || fsz > 128u * 1024u * 1024u)
	{
		fclose(fp);
		return NULL;
	}
	fbuf = (uint8_t *)malloc(fsz + 1u);
	if (!fbuf)
	{
		fclose(fp);
		return NULL;
	}
	if (fread(fbuf, 1, fsz, fp) != fsz)
	{
		free(fbuf);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	fbuf[fsz] = 0;

	if (*(uint32_t const *)fbuf == OCETAGS_COMPRESSED_MAGIC)
	{
		uint32_t uncomp_sz = *(uint32_t const *)(fbuf + 4);
		uint8_t *ubuf;
		uLongf dst_len;
		if (out_was_compressed) *out_was_compressed = 1;
		if (uncomp_sz < 16u || uncomp_sz > 256u * 1024u * 1024u)
		{
			free(fbuf);
			return NULL;
		}
		ubuf = (uint8_t *)malloc((size_t)uncomp_sz + 1u);
		if (!ubuf)
		{
			free(fbuf);
			return NULL;
		}
		dst_len = (uLongf)uncomp_sz;
		if (uncompress((Bytef *)ubuf, &dst_len, (Bytef const *)(fbuf + 8), (uLong)(fsz - 8u)) != Z_OK)
		{
			free(ubuf);
			free(fbuf);
			return NULL;
		}
		free(fbuf);
		ubuf[dst_len] = 0;
		if (out_len) *out_len = (size_t)dst_len;
		return ubuf;
	}
	if (out_len) *out_len = fsz;
	return fbuf;
}

static int is_ce_v609_map_file(char const *path)
{
	FILE *f = fopen(path, "rb");
	uint32_t hdr[2];
	if (!f)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
	{
		fclose(f);
		return 0;
	}
	fclose(f);
	return (hdr[0] == 0x68656164u && hdr[1] == 609u) ? 1 : 0;
}

static int extract_embedded_forge_base_map_stem(char const *map_path, char *out_stem, size_t out_sz)
{
	FILE *f;
	uint8_t hdr[0x200];
	uint32_t state_off = 0, state_sz = 0, sig = 0;

	if (!map_path || !out_stem || out_sz == 0)
		return 0;
	out_stem[0] = 0;
	f = fopen(map_path, "rb");
	if (!f)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
	{
		fclose(f);
		return 0;
	}
	memcpy(&state_off, hdr + 0x1E4u, 4);
	memcpy(&state_sz, hdr + 0x1E8u, 4);
	memcpy(&sig, hdr + 0x1FCu, 4);
	if (sig == 0x464C4731u && state_off >= 0x800u && state_sz > 0 && state_sz < 4u * 1024u * 1024u)
	{
		uint32_t read_sz = (state_sz < 1024u) ? state_sz : 1024u;
		char buf[1025];
		if (fseek(f, (long)state_off, SEEK_SET) == 0 && fread(buf, 1, read_sz, f) == read_sz)
		{
			char const *p;
			buf[read_sz] = 0;
			p = strstr(buf, "Base Map: ");
			if (p)
			{
				size_t i = 0;
				p += 10;
				while (p[i] && p[i] != '\r' && p[i] != '\n' && i + 1 < out_sz)
				{
					out_stem[i] = p[i];
					i++;
				}
				out_stem[i] = 0;
			}
		}
	}
	fclose(f);
	return out_stem[0] ? 1 : 0;
}

static int map_file_has_equal_tag_count(char const *path_a, char const *path_b)
{
	FILE *fa;
	FILE *fb;
	unsigned char hdr_a[0x40];
	unsigned char hdr_b[0x40];
	unsigned char idx_a[0x28];
	unsigned char idx_b[0x28];
	unsigned long off_a, off_b;
	long cnt_a = -1, cnt_b = -2;

	fa = fopen(path_a, "rb");
	if (!fa)
		return 0;
	fb = fopen(path_b, "rb");
	if (!fb)
	{
		fclose(fa);
		return 0;
	}
	if (fread(hdr_a, 1, sizeof(hdr_a), fa) == sizeof(hdr_a) &&
		fread(hdr_b, 1, sizeof(hdr_b), fb) == sizeof(hdr_b))
	{
		off_a = *(unsigned long const *)(hdr_a + 0x10);
		off_b = *(unsigned long const *)(hdr_b + 0x10);
		if (fseek(fa, (long)off_a, SEEK_SET) == 0 &&
			fseek(fb, (long)off_b, SEEK_SET) == 0 &&
			fread(idx_a, 1, sizeof(idx_a), fa) == sizeof(idx_a) &&
			fread(idx_b, 1, sizeof(idx_b), fb) == sizeof(idx_b))
		{
			cnt_a = *(long const *)(idx_a + 0x0C);
			cnt_b = *(long const *)(idx_b + 0x0C);
		}
	}
	fclose(fa);
	fclose(fb);
	return (cnt_a > 0 && cnt_a == cnt_b) ? 1 : 0;
}

static int try_resolve_clean_ce_map_by_stem(char const *stem, char *out_path, size_t out_sz)
{
	char candidate[512];
	char h_xbox[128];
	char dummy_stem[64];
	if (!stem || !stem[0])
		return 0;

	snprintf(h_xbox, sizeof(h_xbox), "h:\\maps\\%s.map", stem);
	platform_translate_path(h_xbox, candidate, sizeof(candidate));
	if (is_ce_v609_map_file(candidate) && !extract_embedded_forge_base_map_stem(candidate, dummy_stem, sizeof(dummy_stem)))
	{
		snprintf(out_path, out_sz, "%s", candidate);
		return 1;
	}

	snprintf(candidate, sizeof(candidate), "C:/Program Files (x86)/Microsoft Games/Halo Custom Edition/maps/%s.map", stem);
	if (is_ce_v609_map_file(candidate) && !extract_embedded_forge_base_map_stem(candidate, dummy_stem, sizeof(dummy_stem)))
	{
		snprintf(out_path, out_sz, "%s", candidate);
		return 1;
	}

	snprintf(candidate, sizeof(candidate), "C:/Program Files/Microsoft Games/Halo Custom Edition/maps/%s.map", stem);
	if (is_ce_v609_map_file(candidate) && !extract_embedded_forge_base_map_stem(candidate, dummy_stem, sizeof(dummy_stem)))
	{
		snprintf(out_path, out_sz, "%s", candidate);
		return 1;
	}

	snprintf(candidate, sizeof(candidate), "custom_maps/%s.map", stem);
	if (is_ce_v609_map_file(candidate) && !extract_embedded_forge_base_map_stem(candidate, dummy_stem, sizeof(dummy_stem)))
	{
		snprintf(out_path, out_sz, "%s", candidate);
		return 1;
	}

	snprintf(candidate, sizeof(candidate), "maps/%s.map", stem);
	if (is_ce_v609_map_file(candidate) && !extract_embedded_forge_base_map_stem(candidate, dummy_stem, sizeof(dummy_stem)))
	{
		snprintf(out_path, out_sz, "%s", candidate);
		return 1;
	}

	return 0;
}

static void resolve_current_base_map_path(char *out_path, size_t out_sz)
{
	char const *ce_loaded = custom_edition_cache_loaded_path();
	char const *map_leaf;
	char base_stem[64];
	char clean_candidate[512];
	FILE *f;

	if (ce_loaded && ce_loaded[0])
	{
		char const *rel_ce = ce_loaded;
		if ((rel_ce[0] == 'd' || rel_ce[0] == 'D') && rel_ce[1] == ':' && (rel_ce[2] == '\\' || rel_ce[2] == '/'))
			rel_ce += 3;
		if (is_ce_v609_map_file(rel_ce))
		{
			if (extract_embedded_forge_base_map_stem(rel_ce, base_stem, sizeof(base_stem)) &&
				_stricmp(base_stem, rel_ce) != 0 &&
				try_resolve_clean_ce_map_by_stem(base_stem, clean_candidate, sizeof(clean_candidate)) &&
				map_file_has_equal_tag_count(rel_ce, clean_candidate))
			{
				snprintf(out_path, out_sz, "%s", clean_candidate);
				return;
			}
			snprintf(out_path, out_sz, "%s", rel_ce);
			return;
		}
	}

	map_leaf = forge_get_active_map_name();
	if (map_leaf && map_leaf[0])
	{
		char const *slash = strrchr(map_leaf, '\\');
		char const *slash2 = strrchr(map_leaf, '/');
		char candidate[512];
		if (slash2 && (!slash || slash2 > slash))
			slash = slash2;
		if (slash)
			map_leaf = slash + 1;

		snprintf(candidate, sizeof(candidate), "custom_maps/%s.map", map_leaf);
		if (is_ce_v609_map_file(candidate))
		{
			if (extract_embedded_forge_base_map_stem(candidate, base_stem, sizeof(base_stem)) &&
				_stricmp(base_stem, map_leaf) != 0 &&
				try_resolve_clean_ce_map_by_stem(base_stem, clean_candidate, sizeof(clean_candidate)) &&
				map_file_has_equal_tag_count(candidate, clean_candidate))
			{
				snprintf(out_path, out_sz, "%s", clean_candidate);
				return;
			}
			snprintf(out_path, out_sz, "%s", candidate);
			return;
		}
		if (try_resolve_clean_ce_map_by_stem(map_leaf, out_path, out_sz))
			return;

		if ((f = fopen(candidate, "rb")) != NULL)
		{
			fclose(f);
			snprintf(out_path, out_sz, "%s", candidate);
			return;
		}
		snprintf(candidate, sizeof(candidate), "maps/%s.map", map_leaf);
		if ((f = fopen(candidate, "rb")) != NULL)
		{
			fclose(f);
			snprintf(out_path, out_sz, "%s", candidate);
			return;
		}
	}

	if (try_resolve_clean_ce_map_by_stem("bloodgulch", out_path, out_sz))
		return;
	if ((f = fopen("maps/bloodgulch.map", "rb")) != NULL)
	{
		fclose(f);
		snprintf(out_path, out_sz, "maps/bloodgulch.map");
		return;
	}
	snprintf(out_path, out_sz, "maps/bloodgulch.map");
}

/* a map made on an MCC map's level: saved as an MCC map of Forge's own
package, mcc_maps\forge\<name> (forge_export_mcc_scenario_map), which the MCC
MULTIPLAYER list shows in its [Forge maps] folder. None of the base map's
resources are in it: like a Custom Edition map's bitmaps.map, sounds.map and
loc.map, they are asked for where the base map has them (MCC's _shared) */
static int forge_ui_save_mcc(char const *name, int was_already_saved)
{
	char const *base_level = mcc_cache_level_name();
	char clean_name[41];
	char level[64];
	char base_path[128];
	char out_path[128];
	FILE *existing;
	int is_new_map;

	/* (the catalog keeps 54 characters of forge\<name>: mcc_maps.c) */
	snprintf(clean_name, sizeof(clean_name), "%s", name);
	snprintf(level, sizeof(level), "%s%s\\%s", MCC_MAPS_LEVEL_PREFIX, MCC_PACKAGE_FORGE, clean_name);
	if (!base_level || !mcc_package_map_path(base_level, base_path) || !mcc_package_map_path(level, out_path))
	{
		forge_set_status("ERROR: '%s' can't name an MCC map", clean_name);
		return 0;
	}
	existing = fopen(out_path, "rb");
	is_new_map = existing == NULL;
	if (existing)
		fclose(existing);

	forge_ai_saved_tags_collect();
	if (!forge_export_mcc_scenario_map(base_path, base_level, &g_forge.edit_state, clean_name, out_path))
	{
		forge_set_status("ERROR: Failed to export mcc_maps/%s/%s.map", MCC_PACKAGE_FORGE, clean_name);
		return 0;
	}
	if (is_new_map)
		social_creator_add(_social_creator_forge_maps, 1);
	platform_log("Forge: saved %s on the MCC map %s with %d campaign tags for its AI, %d AI spawns", level, base_level,
		(int)g_forge.edit_state.ai_actor_count, (int)g_forge.edit_state.ai_spawn_count);
	mcc_maps_rescan();
	snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", clean_name);
	g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
	g_forge.has_saved_map_once = 1;
	if (was_already_saved)
	{
		forge_set_status("Updated saved map 'mcc_maps/%s/%s.map' (%d placed, %d default edits)!", MCC_PACKAGE_FORGE,
			clean_name, (int)g_forge.edit_state.placed_object_count, (int)g_forge.edit_state.default_edit_count);
	}
	else
		forge_set_status("Initial save complete: 'mcc_maps/%s/%s.map'", MCC_PACKAGE_FORGE, clean_name);
	return 1;
}

int forge_ui_save_copy_as(char const *new_map_name)
{
	char base_map_path[512];
	char clean_name[64];
	int was_already_saved = g_forge.has_saved_map_once;
	int is_new_map = 0;
	int ok;

	if (!new_map_name || !new_map_name[0])
	{
		char const *cur = forge_get_active_map_name();
		char const *slash = cur ? strrchr(cur, '\\') : NULL;
		char const *slash2 = cur ? strrchr(cur, '/') : NULL;
		if (slash2 && (!slash || slash2 > slash))
			slash = slash2;
		snprintf(clean_name, sizeof(clean_name), "%s_forge", slash ? (slash + 1) : ((cur && cur[0]) ? cur : "bloodgulch"));
	}
	else
	{
		snprintf(clean_name, sizeof(clean_name), "%s", new_map_name);
	}

	if (mcc_cache_level_name())
		return forge_ui_save_mcc(clean_name, was_already_saved);

	resolve_current_base_map_path(base_map_path, sizeof(base_map_path));

#if FORGE_HOST_FILES
	CreateDirectoryA("d:\\custom_maps", NULL);
	CreateDirectoryA("custom_maps", NULL);
#endif

	/* (a name no map had: a map made, for the player card's CREATOR PROFILE) */
	{
		char existing_path[512];
		FILE *existing;

		snprintf(existing_path, sizeof(existing_path), "custom_maps/%s.map", clean_name);
		existing = (FILE *)forge_host_fopen(existing_path, "rb");
		is_new_map = existing == NULL;
		if (existing)
			fclose(existing);
	}

	forge_ai_saved_tags_collect();
	ok = forge_export_scenario_map(
		base_map_path,
		&g_forge.edit_state,
		clean_name);

	if (ok)
	{
		extern void custom_edition_maps_look_again(void);
		if (is_new_map)
			social_creator_add(_social_creator_forge_maps, 1);
		platform_log("Forge: saved %s with %d campaign tags for its AI, %d AI spawns", clean_name,
			(int)g_forge.edit_state.ai_actor_count, (int)g_forge.edit_state.ai_spawn_count);
		custom_edition_maps_look_again();
		snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", clean_name);
		g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
		g_forge.has_saved_map_once = 1;

		if (was_already_saved)
		{
			forge_set_status("Updated saved map 'custom_maps/%s.map' (%d placed, %d default edits)!",
				clean_name,
				(int)g_forge.edit_state.placed_object_count,
				(int)g_forge.edit_state.default_edit_count);
		}
		else
		{
			forge_set_status("Initial save complete: 'custom_maps/%s.map'",
				clean_name);
		}
		return 1;
	}
	forge_set_status("ERROR: Failed to export custom_maps/%s.map", clean_name);
	return 0;
}

void forge_trigger_save_map_action(void)
{
	/* If an initial save has already been done in this session (or loaded from Edit Maps), quick-update immediately! */
	if (g_forge.has_saved_map_once && g_forge.save_map_name[0])
	{
		g_forge.active_modal = _forge_modal_none;
		forge_ui_save_copy_as(g_forge.save_map_name);
		return;
	}

	/* Otherwise, open the initial save prompt so the user can name their custom map */
	if (!g_forge.save_map_name[0])
	{
		char const *cur = forge_get_active_map_name();
		char const *slash = cur ? strrchr(cur, '\\') : NULL;
		if (!slash && cur)
			slash = strrchr(cur, '/');
		snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s_forge", slash ? (slash + 1) : ((cur && cur[0]) ? cur : "custom"));
		g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
	}
	g_forge.active_modal = _forge_modal_save_copy_as;
}

extern void forge_3d_saved_object_place(long object_index, float x, float y, float z, float yaw, float pitch, float roll);
extern void forge_3d_saved_markers_apply(
	struct scenario *scen,
	struct forge_player_spawn_entry const *spawns,
	int spawn_count,
	int replace_spawns,
	struct forge_netgame_flag_entry const *flags,
	int flag_count,
	int replace_flags);

/* the scenario of the level loading (its tags are there; the game's global
scenario is not set yet) */
static struct scenario *forge_loading_scenario(void)
{
	long count = 0;
	struct forge_cache_file_tag_instance *instances =
		(struct forge_cache_file_tag_instance *)cache_files_tag_instances(&count);
	long i;

	for (i = 0; instances && i < count; i++)
	{
		if (instances[i].group_tag == 'scnr' && instances[i].base_address)
			return XBOX_POINTER(struct scenario, instances[i].base_address); /* AE hook */
	}
	return NULL;
}

int ae_forge_dev_gametype_plays(unsigned flags); /* AE hook */

/* a reloaded saved object's gametypes (its OBJECT_GAMETYPES line) kept on the editor's object and its saved entry, so that a
save of the map after an edit writes the line again */
static void saved_object_gametypes_keep(int32_t live_idx, unsigned gametypes)
{
	struct forge_live_instance *inst;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (!inst->active)
		return;
	inst->gametype_flags = (uint16_t)gametypes;
	if (!inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
		inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
		g_forge.edit_state.placed_objects[inst->edit_entry_index].gametype_flags = (uint16_t)gametypes;
}

static void parse_forge_state_text_buffer(char const *text_buf, char const *leaf)
{
	char const *cur = text_buf;
	char line[512];
	int in_forge_state = 0;
	/* (the next OBJECT line's vehicle is set to ALWAYS SPAWN; its scenery's
	SCALE, 0 for none) */
	int always_spawn = 0;
	int no_damage = 0;
	int no_collision = 0;
	float scale = 0.0f;
	/* (the gametypes the next OBJECT line is for, 0: all: OBJECT_GAMETYPES) */
	unsigned gametypes = 0; /* AE hook */

	if (!cur)
		return;

	while (*cur)
	{
		size_t len = 0;
		while (cur[len] && cur[len] != '\r' && cur[len] != '\n')
			len++;
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, cur, len);
		line[len] = 0;
		cur += len;
		while (*cur == '\r' || *cur == '\n')
			cur++;

		if (strcmp(line, "[OpenCE Forge Map]") == 0)
		{
			/* This map is already a saved Forge map: enable quick-update on F5! */
			snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", leaf);
			g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
			g_forge.has_saved_map_once = 1;
		}
		else if (strcmp(line, "[ForgeState]") == 0)
		{
			in_forge_state = 1;
		}
		else if (in_forge_state && strncmp(line, "OBJECT_GAMETYPES|", 17) == 0) /* AE hook */
		{ /* AE hook */
			gametypes = (unsigned)atoi(line + 17); /* AE hook */
		} /* AE hook */
		else if (in_forge_state && strcmp(line, "OBJECT_ALWAYS_SPAWN") == 0)
		{
			always_spawn = 1;
		}
		/* (an emitter's DEAL DAMAGE off, before its OBJECT line) */
		else if (in_forge_state && strcmp(line, "OBJECT_NO_DAMAGE") == 0)
		{
			no_damage = 1;
		}
		/* (a scenery's COLLISION off, the same way) */
		else if (in_forge_state && strcmp(line, "OBJECT_NO_COLLISION") == 0)
		{
			no_collision = 1;
		}
		/* (a damage or kill volume: forge_volumes.c) */
		else if (in_forge_state && strncmp(line, "VOLUME|", 7) == 0)
		{
			forge_volumes_load_line(line);
		}
		/* (an AI route's point: forge_ai_routes.c) */
		else if (in_forge_state && strncmp(line, "AI_ROUTE|", 9) == 0)
		{
			forge_ai_routes_load_line(line);
		}
		else if (in_forge_state && strncmp(line, "OBJECT_SCALE|", 13) == 0)
		{
			scale = (float)atof(line + 13);
		}
		else if (in_forge_state && strncmp(line, "OBJECT|", 7) == 0)
		{
			unsigned int grp = 0;
			int src_cache = 0, baked = 0, team = 0;
			float x = 0, y = 0, z = 0, yaw = 0, pitch = 0, roll = 0;
			char tpath[256];
			int object_always_spawn = always_spawn;
			int object_no_damage = no_damage;
			int object_no_collision = no_collision;
			float object_scale = scale;
			int32_t made_idx;
			unsigned object_gametypes = gametypes; /* AE hook */
			int in_this_game = ae_forge_dev_gametype_plays(object_gametypes); /* AE hook */

			gametypes = 0; /* AE hook */
			always_spawn = 0;
			no_damage = 0;
			no_collision = 0;
			scale = 0.0f;
			tpath[0] = 0;
			if (sscanf(line + 7, "%u|%d|%d|%f|%f|%f|%f|%f|%f|%d|%255[^\r\n]",
					&grp, &src_cache, &baked, &x, &y, &z, &yaw, &pitch, &roll, &team, tpath) == 11 && tpath[0])
			{
				if (!in_this_game && !baked) /* AE hook */
				{ /* AE hook */
					platform_log("Forge: %s is for gametypes %u: not in this game", tpath, object_gametypes); /* AE hook */
				} /* AE hook */
				else if (!baked)
				{
					/* Cached tag object not baked into base_map: spawn live now! */
					g_forge_next_spawn_always = object_always_spawn;
					g_forge_next_spawn_scale = object_scale;
					made_idx = forge_3d_spawn_object_at((uint32_t)grp, tpath, (uint8_t)src_cache, x, y, z, yaw, pitch, roll,
						(int16_t)team, 0);
					forge_3d_no_damage_apply(made_idx, object_no_damage);
					saved_object_gametypes_keep(made_idx, object_gametypes); /* AE hook */
					forge_3d_no_collision_apply(made_idx, object_no_collision);
				}
				else
				{
					struct object_iterator iter;
					struct object_datum *obj;
					long found_oidx = NONE;
					float best_d2 = 36.0f;

					object_iterator_new(&iter, (unsigned long)-1, 0);
					while ((obj = object_iterator_next(&iter)) != NULL)
					{
						int li, already_claimed = 0;
						float dx, dy, dz, d2;
						char const *oname = NULL;
						if (obj->object.parent_object_index != NONE)
							continue;
						if (obj->object.type == _object_type_biped &&
							((struct unit_datum *)obj)->unit.player_index != NONE)
							continue;
						for (li = 0; li < g_forge.live_instance_count; li++)
						{
							if (g_forge.live_instances[li].active &&
								g_forge.live_instances[li].object_index == iter.index)
							{
								already_claimed = 1;
								break;
							}
						}
						if (already_claimed)
							continue;
						if (obj->definition_index != NONE)
							oname = tag_get_name(obj->definition_index);
						if (oname && oname[0])
						{
							size_t tlen = strlen(tpath);
							if (_stricmp(oname, tpath) != 0 &&
								_strnicmp(oname, tpath, tlen) != 0 &&
								!((uint32_t)grp == 'vehi' && _strnicmp(oname, "forge\\vehicles\\placed_unit_", 27) == 0))
								continue;
						}
						dx = obj->object.position.x - x;
						dy = obj->object.position.y - y;
						dz = obj->object.position.z - z;
						/* Physics objects (vehicles/weapons) spawn with +bounding_sphere_radius*0.5f on Z */
						if (dx * dx + dy * dy > 9.0f || (float)fabs((double)dz) > 6.0f)
							continue;
						d2 = dx * dx + dy * dy + dz * dz * 0.25f;
						if (d2 < best_d2)
						{
							best_d2 = d2;
							found_oidx = iter.index;
						}
					}

					if (!in_this_game) /* AE hook */
					{ /* AE hook */
						/* (a map's own placement of an object for other gametypes: taken out of this game) */ /* AE hook */
						if (found_oidx != NONE) /* AE hook */
							object_delete(found_oidx); /* AE hook */
						platform_log("Forge: %s is for gametypes %u: %s", tpath, object_gametypes, found_oidx != NONE ? "taken out of this game" : "not in this game"); /* AE hook */
					} /* AE hook */
					else if (found_oidx == NONE)
					{
						/* Not yet spawned by scenario: spawn live and record in placed_objects */
						g_forge_next_spawn_always = object_always_spawn;
						g_forge_next_spawn_scale = object_scale;
						made_idx = forge_3d_spawn_object_at((uint32_t)grp, tpath, (uint8_t)src_cache, x, y, z, yaw, pitch, roll,
							(int16_t)team, 0);
						forge_3d_no_damage_apply(made_idx, object_no_damage);
					saved_object_gametypes_keep(made_idx, object_gametypes); /* AE hook */
						forge_3d_no_collision_apply(made_idx, object_no_collision);
					}
					else if (g_forge.edit_state.placed_object_count < FORGE_MAX_PLACED_OBJECTS)
					{
						/* Record baked object in edit_state and link to its live instance */
						int32_t po_idx = g_forge.edit_state.placed_object_count++;
						struct forge_placed_object_entry *po = &g_forge.edit_state.placed_objects[po_idx];
						struct object_datum *fobj = (struct object_datum *)object_try_and_get_and_verify_type(found_oidx, (unsigned long)-1);

						memset(po, 0, sizeof(*po));
						po->active = 1;
						po->group_tag = (uint32_t)grp;
						po->source_cache_id = (uint8_t)src_cache;
						po->category = (grp == 'vehi') ? _forge_object_vehicle :
							(grp == 'weap') ? _forge_object_weapon :
							(grp == 'bipd') ? _forge_object_biped :
							(grp == 'eqip') ? _forge_object_equipment : _forge_object_scenery;
						snprintf(po->tag_path, sizeof(po->tag_path), "%s", tpath);
						po->x = x; po->y = y; po->z = z;
						po->yaw = yaw; po->pitch = pitch; po->roll = roll;
						po->team_index = (int16_t)team;
						po->always_spawn = (uint8_t)(object_always_spawn && (uint32_t)grp == 'vehi');
						po->gametype_flags = (uint16_t)object_gametypes; /* AE hook */

						if (fobj && fobj->definition_index != NONE)
						{
							if ((uint32_t)grp == 'vehi')
								patch_vehicle_tag_for_player_driving(fobj->definition_index, tpath);
							else
								patch_forge_device_and_light_tag_runtime(fobj->definition_index, (uint32_t)grp, tpath);
						}

						if (g_forge.live_instance_count < FORGE_MAX_LIVE_INSTANCES)
						{
							int32_t li_idx = g_forge.live_instance_count++;
							struct forge_live_instance *inst = &g_forge.live_instances[li_idx];
							memset(inst, 0, sizeof(*inst));
							inst->active = 1;
							inst->object_index = found_oidx;
							inst->actor_index = NONE;
							inst->edit_entry_index = po_idx;
							inst->group_tag = (uint32_t)grp;
							inst->source_cache_id = (uint8_t)src_cache;
							inst->is_default_scenario_object = 0;
							inst->team_designator = (int16_t)team;
							inst->always_spawn = po->always_spawn;
							inst->gametype_flags = po->gametype_flags; /* AE hook */
							inst->x = x; inst->y = y; inst->z = z;
							inst->yaw = yaw; inst->pitch = pitch; inst->roll = roll;
							snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", tpath);
							forge_3d_move_instance(li_idx, x, y, z, yaw, pitch, roll, 0);
							{
								extern void forge_scenery_hold_still_if_strayed(long object_index);

								forge_scenery_hold_still_if_strayed(found_oidx);
							}
							/* (the baked scenery the scenario made, at its size) */
							if (object_scale > 0.0f)
								forge_3d_scale_apply(li_idx, object_scale);
							forge_3d_no_damage_apply(li_idx, object_no_damage);
							forge_3d_no_collision_apply(li_idx, object_no_collision);
						}
					}
				}
			}
		}
		else if (in_forge_state && strncmp(line, "FF_SPAWN|", 9) == 0)
		{
			float x = 0, y = 0, z = 0, yaw = 0;
			int usage_id = 0;
			if (sscanf(line + 9, "%f|%f|%f|%f|%d", &x, &y, &z, &yaw, &usage_id) == 5)
			{
				int li, already_exists = 0;
				for (li = 0; li < g_forge.live_instance_count; li++)
				{
					if (g_forge.live_instances[li].active &&
						g_forge.live_instances[li].group_tag == 'ffsp')
					{
						float dx = g_forge.live_instances[li].x - x;
						float dy = g_forge.live_instances[li].y - y;
						float dz = g_forge.live_instances[li].z - z;
						if (dx * dx + dy * dy + dz * dz < 0.25f)
						{
							already_exists = 1;
							break;
						}
					}
				}
				if (!already_exists)
				{
					forge_3d_spawn_object_at('ffsp', "spawn\\firefight_spawn", 1, x, y, z, yaw, 0.0f, 0.0f, (int16_t)usage_id, 0);
				}
			}
		}
		else if (in_forge_state && strncmp(line, "DEFAULT_EDIT|", 13) == 0)
		{
			unsigned int grp = 0;
			int deleted = 0;
			float ox = 0, oy = 0, oz = 0, nx = 0, ny = 0, nz = 0, nyaw = 0, npitch = 0, nroll = 0;
			char tpath[256];
			tpath[0] = 0;
			if (sscanf(line + 13, "%u|%d|%f|%f|%f|%f|%f|%f|%f|%f|%f|%255[^\r\n]",
					&grp, &deleted, &ox, &oy, &oz, &nx, &ny, &nz, &nyaw, &npitch, &nroll, tpath) == 12 &&
				g_forge.edit_state.default_edit_count < FORGE_MAX_DEFAULT_OBJECT_EDITS)
			{
				struct forge_default_object_edit *de = &g_forge.edit_state.default_edits[g_forge.edit_state.default_edit_count++];
				struct object_iterator iter;
				struct object_datum *obj;
				long found_oidx = NONE;
				float best_d2 = 9.0f;

				memset(de, 0, sizeof(*de));
				de->active = 1;
				de->group_tag = (uint32_t)grp;
				de->deleted = (uint8_t)deleted;
				de->orig_x = ox; de->orig_y = oy; de->orig_z = oz;
				de->new_x = nx; de->new_y = ny; de->new_z = nz;
				de->new_yaw = nyaw; de->new_pitch = npitch; de->new_roll = nroll;
				snprintf(de->tag_path, sizeof(de->tag_path), "%s", tpath);

				/* Apply default edit live if the scenario object is still at its unedited origin (e.g. Xbox v5 FLG1 container) */
				object_iterator_new(&iter, (unsigned long)-1, 0);
				while ((obj = object_iterator_next(&iter)) != NULL)
				{
					float dx = obj->object.position.x - ox;
					float dy = obj->object.position.y - oy;
					float dz = obj->object.position.z - oz;
					float d2 = dx * dx + dy * dy + dz * dz * 0.25f;
					if (obj->object.parent_object_index != NONE)
						continue;
					if (obj->object.type == _object_type_biped &&
						((struct unit_datum *)obj)->unit.player_index != NONE)
						continue;
					if (d2 < best_d2)
					{
						best_d2 = d2;
						found_oidx = iter.index;
					}
				}
				if (found_oidx != NONE)
				{
					if (deleted)
					{
						object_delete(found_oidx);
					}
					else
					{
						forge_3d_saved_object_place(found_oidx, nx, ny, nz, nyaw, npitch, nroll);
					}
				}
			}
		}
	}
}

/* a saved Forge map: its file, as the game finds it on this machine (a
network game's its host's version) */
int ae_forge_maps_path(char const *name, char *path, int size); /* AE hook */
static FILE *forge_saved_map_open(char const *scenario_name)
{
	char const *leaf = scenario_name ? scenario_name : "";
	char const *slash;
	char path[256];
	char host[1024];

	/* (an MCC map's level, mcc_maps\<package>\<name>: its file, Forge's own
	in mcc_maps\forge among them) */
	if (scenario_name && mcc_level_name(scenario_name))
	{
		char mcc_path[128];

		if (!mcc_package_map_path(scenario_name, mcc_path))
			return NULL;
		platform_translate_path(mcc_path, host, sizeof(host));
		return fopen(host, "rb");
	}
	while ((slash = strpbrk(leaf, "\\/")) != NULL)
		leaf = slash + 1;
	if (!leaf[0] || !(ae_forge_maps_path(leaf, path, (int)sizeof(path)) /* AE hook */ || custom_edition_map_file_path(leaf, path)))
		return NULL;
	platform_translate_path(path, host, sizeof(host));
	return fopen(host, "rb");
}

/* a saved map's state, its trailer's text: NULL when it is no saved map */
static char *forge_saved_map_state(FILE *f)
{
	uint8_t hdr[0x200];
	uint32_t state_off = 0, state_sz = 0, sig = 0;
	char *text;

	if (fseek(f, 0, SEEK_SET) != 0 || fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
		return NULL;
	memcpy(&state_off, hdr + 0x1E4u, 4);
	memcpy(&state_sz, hdr + 0x1E8u, 4);
	memcpy(&sig, hdr + 0x1FCu, 4);
	if (sig != 0x464C4731u || state_off < 0x800u || !state_sz || state_sz >= 4u * 1024u * 1024u)
		return NULL;
	text = (char *)malloc(state_sz + 1u);
	if (!text)
		return NULL;
	if (fseek(f, (long)state_off, SEEK_SET) != 0 || fread(text, 1, state_sz, f) != state_sz)
	{
		free(text);
		return NULL;
	}
	text[state_sz] = 0;
	return text;
}

int forge_saved_map(char const *scenario_name)
{
	FILE *f = forge_saved_map_open(scenario_name);
	uint8_t hdr[0x200];
	uint32_t sig = 0;

	if (!f)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr))
		memcpy(&sig, hdr + 0x1FCu, 4);
	fclose(f);
	return sig == 0x464C4731u;
}

/* the part of a map's file at `offset`, `size` bytes of it, copied to `path` */
static int forge_file_part_copy(FILE *f, uint32_t offset, uint32_t size, char const *path)
{
	FILE *out = fopen(path, "wb");
	uint8_t *chunk = (uint8_t *)malloc(0x40000u);
	int copied = out && chunk && fseek(f, (long)offset, SEEK_SET) == 0;

	while (copied && size)
	{
		size_t part = size < 0x40000u ? (size_t)size : 0x40000u;

		copied = fread(chunk, 1, part, f) == part && fwrite(chunk, 1, part, out) == part;
		size -= (uint32_t)part;
	}
	free(chunk);
	if (out)
		copied = fclose(out) == 0 && copied;
	return copied;
}

/* a saved map's tags, unpacked once and opened for the level (and its spawns
and flags, which the level then plays): the profile, NONE for none */
static void forge_saved_markers_apply(char const *text);

/* ---------- a saved map's tags its bake could not hold

MCC's own content never leaves the machine it was taken on (its EULA), so a
map's bake leaves out any placed object or AI spawn's actor or weapon taken
out of an MCC map (port_tag_profiles_baked_build). The save names each
instead (LOCAL_TAG|group|profile folder|path), and a level of the map reads
them from this machine's own profiles: the map's bake and these, copied
together once into a bake of this machine's alone (.baked/l<id>), never
sent anywhere. A machine without them plays the map without them */
#define FORGE_MAX_LOCAL_TAGS 256

struct forge_local_tag
{
	uint32_t group_tag;
	char folder[64];
	char path[256];
};

/* (the level's, as its save named them: a profile folder for a tag a later
save finds in the level's own bake) */
static struct forge_local_tag s_level_local_tags[FORGE_MAX_LOCAL_TAGS];
static int s_level_local_tag_count;

static int forge_local_tags_parse(char const *text, struct forge_local_tag *tags, int capacity)
{
	char const *cur = text;
	int count = 0;

	while ((cur = strstr(cur, "LOCAL_TAG|")) != NULL && count < capacity)
	{
		unsigned int group = 0;
		struct forge_local_tag *tag = &tags[count];
		char const *folder;
		char const *path;
		size_t length;

		cur += 10;
		memset(tag, 0, sizeof(*tag));
		folder = strchr(cur, '|');
		path = folder ? strchr(folder + 1, '|') : NULL;
		if (!path || sscanf(cur, "%x", &group) != 1)
			continue;
		/* (an empty folder: any of this machine's profiles) */
		length = (size_t)(path - folder - 1);
		snprintf(tag->folder, sizeof(tag->folder), "%.*s", (int)(length < sizeof(tag->folder) ? length : 0), folder + 1);
		if (sscanf(path + 1, "%255[^\r\n]", tag->path) == 1)
		{
			tag->group_tag = (uint32_t)group;
			count++;
		}
	}
	return count;
}

/* this machine's profile and tag of a local tag, by its folder first: 0 when
none has it */
static int forge_local_tag_find(struct forge_local_tag const *local, long *profile, long *tag)
{
	long count = port_tag_profiles_count();
	long index;

	*profile = local->folder[0] ? port_tag_profiles_find(local->folder) : NONE;
	*tag = *profile >= 0 ? port_tag_profiles_tag_find(*profile, local->group_tag, local->path) : NONE;
	for (index = 0; *tag < 0 && index < count; index++)
	{
		*profile = index;
		*tag = port_tag_profiles_tag_find(index, local->group_tag, local->path);
	}
	return *tag >= 0;
}

/* the level's bake: `baked` (the map's, NONE for none) with the map's local
tags this machine has added, made once. The profile, `baked` when there are
none */
static long forge_level_local_tags_add(char const *text, long baked, char const *baked_id)
{
	struct forge_local_tag *locals = s_level_local_tags;
	long *profiles = NULL;
	long *tags = NULL;
	long baked_count = baked != NONE ? port_tag_profiles_tag_count(baked) : 0;
	int local_count;
	int found = 0;
	int i;
	uint32_t low = 0x811C9DC5u, high = 0x9E3779B9u;
	char id[40];
	long local = NONE;

	s_level_local_tag_count = local_count = forge_local_tags_parse(text, locals, FORGE_MAX_LOCAL_TAGS);
	if (!local_count)
		return baked;
	profiles = (long *)malloc((size_t)(baked_count + local_count) * sizeof(*profiles));
	tags = (long *)malloc((size_t)(baked_count + local_count) * sizeof(*tags));
	if (!profiles || !tags)
	{
		free(profiles);
		free(tags);
		return baked;
	}
	for (i = 0; i < baked_count; i++)
	{
		profiles[i] = baked;
		tags[i] = i;
	}
	/* (the profiles this machine has now: the level's bake keeps its slot) */
	port_tag_profiles_scan();
	for (i = 0; baked_id && baked_id[i]; i++)
		low = (low ^ (uint8_t)baked_id[i]) * 16777619u;
	for (i = 0; i < local_count; i++)
	{
		long profile, tag;
		char const *c;
		unsigned long size;
		unsigned long signature;

		if (baked != NONE && port_tag_profiles_tag_find(baked, locals[i].group_tag, locals[i].path) >= 0)
			continue;
		if (!forge_local_tag_find(&locals[i], &profile, &tag))
		{
			error(_error_silent, "forge: the map's %s is not in any of this machine's Tag Injector profiles: it is "
				"played without it", locals[i].path);
			continue;
		}
		profiles[baked_count + found] = profile;
		tags[baked_count + found] = tag;
		found++;
		size = port_tag_profiles_tag_size(profile, tag);
		signature = port_tag_profiles_tag_signature(profile, tag);
		for (c = locals[i].path; *c; c++)
			low = (low ^ (uint8_t)*c) * 16777619u;
		low = (low ^ locals[i].group_tag) * 16777619u;
		high = (high + (uint32_t)size) * 2654435761u;
		high = (high + (uint32_t)signature) * 2654435761u;
	}
	if (found)
	{
		snprintf(id, sizeof(id), "l%08x%08x", (unsigned int)high, (unsigned int)low);
		if (!port_tag_profiles_baked_present(id))
		{
			char scratch[1024];

			port_tag_profiles_baked_scratch_path(scratch, sizeof(scratch));
			if (!port_tag_profiles_baked_build_local(profiles, tags, baked_count + found, scratch) ||
				!port_tag_profiles_baked_unpack(scratch, id))
			{
				error(_error_silent, "forge: the map's tags from this machine's profiles cannot be copied");
			}
			remove(scratch);
		}
		/* (the map's own bake closed, as this one opens) */
		if (port_tag_profiles_baked_present(id))
			local = port_tag_profiles_baked_open(id);
		if (local == NONE || local < 0)
		{
			local = NONE;
			if (baked_id && baked_id[0])
				baked = port_tag_profiles_baked_open(baked_id);
		}
		else
			error(_error_silent, "forge: %d of the map's %d tags from this machine's profiles added (%s)", found,
				local_count, id);
	}
	free(profiles);
	free(tags);
	return local != NONE ? local : baked;
}

/* a level that is no saved map: none named */
void forge_level_local_tags_clear(void)
{
	s_level_local_tag_count = 0;
}

/* the profile folder the level's save named for a local tag, "" for none */
static char const *forge_local_tag_folder(uint32_t group_tag, char const *path)
{
	int i;

	for (i = 0; i < s_level_local_tag_count; i++)
	{
		if (s_level_local_tags[i].group_tag == group_tag && !_stricmp(s_level_local_tags[i].path, path))
			return s_level_local_tags[i].folder;
	}
	return "";
}

long forge_level_baked_open(char const *scenario_name)
{
	FILE *f = forge_saved_map_open(scenario_name);
	char *text = f ? forge_saved_map_state(f) : NULL;
	char const *line = text ? strstr(text, "BAKED_TAGS|") : NULL;
	unsigned int offset = 0, size = 0;
	char id[40];
	long profile = NONE;

	id[0] = 0;
	if (text)
		forge_saved_markers_apply(text);
	/* (its name, sky, bipeds, hands and atmosphere: forge_map_settings.c) */
	forge_map_settings_level_loading(text);
	if (line && sscanf(line + 11, "%x|%u|%39[^\r\n]", &offset, &size, id) == 3 && id[0] && offset >= 0x800u)
	{
		if (!port_tag_profiles_baked_present(id))
		{
			char scratch[1024];

			port_tag_profiles_baked_scratch_path(scratch, sizeof(scratch));
			if (!forge_file_part_copy(f, offset, size, scratch) || !port_tag_profiles_baked_unpack(scratch, id))
				platform_log("Forge: the tags saved in %s cannot be read", scenario_name);
			remove(scratch);
		}
		profile = port_tag_profiles_baked_open(id);
		if (profile == NONE || profile < 0)
		{
			platform_log("Forge: the tags saved in %s are not there", scenario_name);
			profile = NONE;
		}
	}
	s_level_local_tag_count = 0;
	if (text)
		profile = forge_level_local_tags_add(text, profile, profile != NONE ? id : NULL);
	free(text);
	if (f)
		fclose(f);
	return profile;
}

/* the LOCAL_TAG lines of a save (forge_level_local_tags_add): what is placed
and the AI spawns use that the bake leaves out as MCC's own content, each
once, into `text`; and those a level of the map named but this machine has
not got, kept for the machines that have. Its length */
int forge_baked_local_lines(struct forge_scenario_edit_state const *edits, char *text, unsigned long text_size)
{
	long baked = tag_injector_level_baked_profile();
	long playing = tag_injector_level_profile();
	/* (and the map settings' sky, bipeds and hands: forge_map_settings_tags) */
	uint32_t settings_groups[8];
	char const *settings_paths[8];
	int settings_count = forge_map_settings_tags(settings_groups, settings_paths, 8);
	int object_capacity = edits ? edits->placed_object_count + edits->ai_spawn_count * 2 : 0;
	int capacity = edits ? object_capacity + settings_count : 0;
	unsigned long length = 0;
	int i;

	if (!text || !text_size)
		return 0;
	text[0] = 0;
	for (i = 0; i < capacity; i++)
	{
		int is_setting = i >= object_capacity;
		int is_ai = !is_setting && i >= edits->placed_object_count;
		int ai_index = (i - edits->placed_object_count) / 2;
		int is_ai_weapon = is_ai && ((i - edits->placed_object_count) & 1);
		uint32_t group = is_setting ? settings_groups[i - object_capacity] :
			is_ai_weapon ? 'weap' : is_ai ? 'actv' : edits->placed_objects[i].group_tag;
		char const *name = is_setting ? settings_paths[i - object_capacity] :
			is_ai_weapon ? edits->ai_spawns[ai_index].weapon_path :
			is_ai ? edits->ai_spawns[ai_index].actor_variant_path :
			edits->placed_objects[i].tag_path;
		char const *folder = NULL;
		long profile = baked;
		long tag = NONE;
		char line[400];
		int line_length;

		if (!name[0] || (!is_ai && !is_setting && edits->placed_objects[i].x < -90000.0f) ||
			(is_ai && !edits->ai_spawns[ai_index].active))
			continue;
		if (baked != NONE)
			tag = is_setting ? forge_map_settings_profile_tag(baked, group, name) :
				port_tag_profiles_tag_find(baked, group, name);
		if (tag == NONE && playing != NONE)
		{
			profile = playing;
			tag = is_setting ? forge_map_settings_profile_tag(playing, group, name) :
				port_tag_profiles_tag_find(playing, group, name);
		}
		if (tag != NONE && port_tag_profiles_tag_mcc_own(profile, tag))
		{
			folder = forge_local_tag_folder(group, name);
			/* (a regular profile's folder: a level's bake or a share is no
			profile of anyone's) */
			if (!folder[0] && profile == playing && port_tag_profiles_folder(profile)[0] != '.')
				folder = port_tag_profiles_folder(profile);
		}
		else if (tag == NONE)
		{
			int known;

			for (known = 0; known < s_level_local_tag_count &&
				!(s_level_local_tags[known].group_tag == group && !_stricmp(s_level_local_tags[known].path, name));
				known++)
			{
			}
			if (known < s_level_local_tag_count)
				folder = s_level_local_tags[known].folder;
		}
		if (!folder)
			continue;
		line_length = snprintf(line, sizeof(line), "LOCAL_TAG|%08x|%s|%s\r\n", (unsigned int)group, folder, name);
		if (line_length <= 0 || (unsigned long)line_length >= sizeof(line) || strstr(text, line) ||
			length + (unsigned long)line_length >= text_size)
		{
			continue;
		}
		memcpy(text + length, line, (size_t)line_length + 1);
		length += (unsigned long)line_length;
	}
	return (int)length;
}

/* the tags of what is placed that are not the game's, into an archive to
save with the map: from the level's bake, else its profile, a tag no object
uses left out. 0 when there are none; else their count, the archive's path,
size and an id made of its bytes */
int forge_baked_archive_make(
	struct forge_scenario_edit_state const *edits,
	char *path,
	unsigned long path_size,
	char *id,
	unsigned long id_size,
	uint32_t *size)
{
	long baked = tag_injector_level_baked_profile();
	long playing = tag_injector_level_profile();
	/* (and after them, the tags the map's general settings name: its sky,
	the players' bipeds and hands, forge_map_settings_tags) */
	uint32_t settings_groups[8];
	char const *settings_paths[8];
	int settings_count = forge_map_settings_tags(settings_groups, settings_paths, 8);
	int object_capacity = edits ? edits->placed_object_count + edits->ai_spawn_count * 2 : 0;
	int capacity = object_capacity + settings_count;
	long *profiles;
	long *tags;
	int count = 0;
	int i;
	long made = 0;

	*size = 0;
	if (!edits || capacity <= 0 || (baked == NONE && playing == NONE))
		return 0;
	profiles = (long *)malloc((size_t)capacity * sizeof(*profiles));
	tags = (long *)malloc((size_t)capacity * sizeof(*tags));
	if (!profiles || !tags)
	{
		free(profiles);
		free(tags);
		return 0;
	}
	for (i = 0; i < capacity; i++)
	{
		int is_setting = i >= object_capacity;
		int is_ai = !is_setting && i >= edits->placed_object_count;
		/* (an AI spawn's actor, then its weapon) */
		int ai_index = (i - edits->placed_object_count) / 2;
		int is_ai_weapon = is_ai && ((i - edits->placed_object_count) & 1);
		uint32_t group = is_setting ? settings_groups[i - object_capacity] :
			is_ai_weapon ? 'weap' : is_ai ? 'actv' : edits->placed_objects[i].group_tag;
		char const *name = is_setting ? settings_paths[i - object_capacity] :
			is_ai_weapon ? edits->ai_spawns[ai_index].weapon_path :
			is_ai ? edits->ai_spawns[ai_index].actor_variant_path :
			edits->placed_objects[i].tag_path;
		long profile = baked;
		long tag = NONE;
		int other;

		if (!name[0] || (!is_ai && !is_setting && edits->placed_objects[i].x < -90000.0f))
			continue;
		if (baked != NONE)
			tag = is_setting ? forge_map_settings_profile_tag(baked, group, name) :
				port_tag_profiles_tag_find(baked, group, name);
		if (tag == NONE && playing != NONE)
		{
			profile = playing;
			tag = is_setting ? forge_map_settings_profile_tag(playing, group, name) :
				port_tag_profiles_tag_find(playing, group, name);
		}
		if (tag == NONE)
			continue;
		for (other = 0; other < count && !(profiles[other] == profile && tags[other] == tag); other++)
		{
		}
		if (other == count)
		{
			profiles[count] = profile;
			tags[count] = tag;
			count++;
		}
	}
	if (count)
	{
		port_tag_profiles_baked_scratch_path(path, path_size);
		made = port_tag_profiles_baked_build(profiles, tags, count, path);
	}
	free(profiles);
	free(tags);
	if (made)
	{
		FILE *f = fopen(path, "rb");
		uint32_t low = 0x811C9DC5u, high = 0x9E3779B9u;
		uint8_t *chunk = (uint8_t *)malloc(0x40000u);
		size_t part;

		if (!f || !chunk)
			made = 0;
		while (f && chunk && (part = fread(chunk, 1, 0x40000u, f)) > 0)
		{
			size_t b;

			for (b = 0; b < part; b++)
			{
				low = (low ^ chunk[b]) * 16777619u;
				high = (high + chunk[b]) * 2654435761u;
			}
			*size += (uint32_t)part;
		}
		free(chunk);
		if (f)
			fclose(f);
		snprintf(id, id_size, "%08x%08x", (unsigned int)high, (unsigned int)low);
	}
	return (int)made;
}

/* a saved map's spawns and flags, replacing the map's own in the level
loading's scenario (forge_3d_saved_markers_apply). A map saved before them
keeps the originals */
static void forge_saved_markers_apply(char const *text)
{
	struct forge_player_spawn_entry *spawns;
	struct forge_netgame_flag_entry *flags;
	int spawn_count = 0, flag_count = 0;
	int replace_spawns = strstr(text, "PLAYER_SPAWNS_SAVED\r\n") != NULL;
	int replace_flags = strstr(text, "NETGAME_FLAGS_SAVED\r\n") != NULL;
	char const *cur = text;

	forge_ai_saved_parse(text);
	if (!replace_spawns && !replace_flags)
		return;
	spawns = (struct forge_player_spawn_entry *)calloc(FORGE_MAX_PLAYER_SPAWNS, sizeof(*spawns));
	flags = (struct forge_netgame_flag_entry *)calloc(FORGE_MAX_NETGAME_FLAGS, sizeof(*flags));
	while (spawns && flags && cur && *cur)
	{
		if (replace_spawns && !strncmp(cur, "PLAYER_SPAWN|", 13) && spawn_count < FORGE_MAX_PLAYER_SPAWNS)
		{
			struct forge_player_spawn_entry *sp = &spawns[spawn_count];
			int team = 0, bsp = 0, t0 = 0, t1 = 0, t2 = 0, t3 = 0;

			if (sscanf(cur + 13, "%f|%f|%f|%f|%d|%d|%d|%d|%d|%d", &sp->x, &sp->y, &sp->z, &sp->yaw, &team, &bsp,
					&t0, &t1, &t2, &t3) == 10)
			{
				sp->team_index = (int16_t)team;
				sp->bsp_index = (int16_t)bsp;
				sp->type0 = (int16_t)t0;
				sp->type1 = (int16_t)t1;
				sp->type2 = (int16_t)t2;
				sp->type3 = (int16_t)t3;
				spawn_count++;
			}
		}
		else if (replace_flags && !strncmp(cur, "NETGAME_FLAG|", 13) && flag_count < FORGE_MAX_NETGAME_FLAGS)
		{
			struct forge_netgame_flag_entry *nf = &flags[flag_count];
			int type = 0, usage = 0;

			if (sscanf(cur + 13, "%f|%f|%f|%f|%d|%d|%255[^\r\n]", &nf->x, &nf->y, &nf->z, &nf->yaw, &type, &usage,
					nf->weapon_group_path) >= 6)
			{
				nf->type = (int16_t)type;
				nf->usage_id = (int16_t)usage;
				flag_count++;
			}
		}
		/* (Firefight's flags are saved on lines of their own) */
		else if (replace_flags && !strncmp(cur, "FF_SPAWN|", 9) && flag_count < FORGE_MAX_NETGAME_FLAGS)
		{
			struct forge_netgame_flag_entry *nf = &flags[flag_count];
			int usage = 0;

			if (sscanf(cur + 9, "%f|%f|%f|%f|%d", &nf->x, &nf->y, &nf->z, &nf->yaw, &usage) == 5)
			{
				nf->type = FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN;
				nf->usage_id = (int16_t)usage;
				flag_count++;
			}
		}
		cur = strchr(cur, '\n');
		if (cur)
			cur++;
	}
	if (spawns && flags)
		forge_3d_saved_markers_apply(forge_loading_scenario(), spawns, spawn_count, replace_spawns, flags, flag_count,
			replace_flags);
	free(spawns);
	free(flags);
}

/* the map of Forge's pack a tag is read from, as its palette has it */
int forge_campaign_tag_map(uint32_t group_tag, char const *path, char *map, size_t map_size)
{
	long pack = port_tag_packs_find(FORGE_PACK);
	uint32_t palette_group = group_tag == 'actv' ? FORGE_GROUP_AI_SPAWN : group_tag;
	int i;

	for (i = 0; pack >= 0 && i < g_forge.catalog_count; i++)
	{
		struct forge_catalog_entry const *entry = &g_forge.catalog[i];

		if (entry->group_tag == palette_group && entry->pad && !_stricmp(entry->tag_path, path))
		{
			snprintf(map, map_size, "%s", port_tag_packs_map(pack, (long)entry->pad - 1));
			return 1;
		}
	}
	return 0;
}

/* the level loading is a multiplayer map's (the game's global scenario is not
set yet) */
int forge_level_loading_multiplayer(void)
{
	struct scenario *scen = forge_loading_scenario();

	return scen && scen->type == _scenario_type_multiplayer;
}

static void forge_SyncScenarioTagsWithLoadedMap(void)
{
	int r, w = 0;
	long total = 0, i;
	int ai_map = forge_ai_map_editable();
	struct forge_cache_file_tag_instance *instances;

	/* 1. Strip any transient map-local entries (source_cache_id == 0) from previous map loads */
	for (r = 0; r < g_forge.catalog_count; r++)
	{
		if (g_forge.catalog[r].source_cache_id != 0)
		{
			if (w != r)
				g_forge.catalog[w] = g_forge.catalog[r];
			w++;
		}
	}
	g_forge.catalog_count = w;

	/* 2. Scan the loaded map's tag instances and append uncached object tags with source_cache_id = 0 ("[MAP] ...") */
	instances = (struct forge_cache_file_tag_instance *)cache_files_tag_instances(&total);
	if (!instances || total <= 0)
		return;

	for (i = 0; i < total; i++)
	{
		uint32_t grp = instances[i].group_tag;
		char const *tpath = XBOX_POINTER(char const, instances[i].name); /* AE hook */
		uint8_t cat_id;
		int k, exists = 0;

		if (!tpath || !tpath[0] || !instances[i].base_address)
			continue;
		if (_strnicmp(tpath, "forge\\markers\\", 14) == 0 ||
		    _strnicmp(tpath, "forge/markers/", 14) == 0)
		{
			continue;
		}

		if (grp == 'effe')
		{
			if (!is_forge_effect_spot_tag(grp, tpath))
				continue;
			cat_id = _forge_cat_effects;
		}
		else if (grp == 'deca')
		{
			cat_id = _forge_cat_decals;
		}
		else if (grp == 'weap')
		{
			if (is_excluded_weapon_or_equipment_tag(tpath))
				continue;
			cat_id = _forge_cat_weapons;
		}
		else if (grp == 'eqip' || grp == 'garb')
		{
			if (is_excluded_weapon_or_equipment_tag(tpath))
				continue;
			cat_id = _forge_cat_equipment;
		}
		else if (grp == 'vehi')
		{
			cat_id = _forge_cat_vehicles;
		}
		else if (grp == 'lifi')
		{
			cat_id = _forge_cat_lights;
		}
		else if (grp == 'actv')
		{
			/* (placed as a spawn: a vehicle's gunner is not one to place) */
			if (!ai_map || !strncmp(tpath, "vehicles\\", 9))
				continue;
			grp = FORGE_GROUP_AI_SPAWN;
			cat_id = _forge_cat_ai_squads;
		}
		else if (grp == 'mach' || grp == 'ctrl')
		{
			cat_id = is_forge_light_beacon_emitter_tag(grp, tpath)
				? _forge_cat_lights
				: _forge_cat_barriers_devices;
		}
		else if (grp == 'scen')
		{
			if (is_forge_particle_emitter_tag(grp, tpath))
				cat_id = _forge_cat_effects;
			else if (is_excluded_scenery_or_effect_tag(tpath))
				continue;
			else
				cat_id = is_forge_light_beacon_emitter_tag(grp, tpath)
					? _forge_cat_lights
					: _forge_cat_scenery;
		}
		else
		{
			continue;
		}

		/* (a split tab's MAP folder has all of the map's own, the pack's
		too, but not those the props or a profile copied in; any other tab
		a tag once) */
		for (k = 0; k < g_forge.catalog_count; k++)
		{
			if (g_forge.catalog[k].group_tag == grp &&
			    _stricmp(g_forge.catalog[k].tag_path, tpath) == 0 &&
			    (!forge_catalog_category_split(cat_id) ||
			     (g_forge.catalog[k].origin != _forge_origin_campaign && g_forge.catalog[k].origin != _forge_origin_map)))
			{
				exists = 1;
				break;
			}
		}
		if (!exists && g_forge.catalog_count < FORGE_MAX_CATALOG_ENTRIES)
		{
			s_catalog_origin = _forge_origin_map;
			add_catalog_entry(grp, cat_id, 0, tpath,
				grp == FORGE_GROUP_AI_SPAWN || forge_catalog_category_split(cat_id) ? "" : "[MAP]");
			s_catalog_origin = _forge_origin_builtin;
		}
	}
	forge_catalog_folders_rebuild();
}

static void normalize_hud_placement_at(uint8_t *placement, int halve_offset, int halve_scale)
{
	if (!placement)
		return;
	if (halve_offset)
	{
		int16_t ox = *(int16_t *)(placement + 0x00);
		int16_t oy = *(int16_t *)(placement + 0x02);
		*(int16_t *)(placement + 0x00) = (int16_t)(ox / 2);
		*(int16_t *)(placement + 0x02) = (int16_t)(oy / 2);
	}
	if (halve_scale)
	{
		float sx = *(float *)(placement + 0x04);
		float sy = *(float *)(placement + 0x08);
		if (sx == 0.0f)
			sx = 0.5f;
		else if (sx > 0.6f)
			sx *= 0.5f;
		if (sy == 0.0f)
			sy = 0.5f;
		else if (sy > 0.6f)
			sy *= 0.5f;
		*(float *)(placement + 0x04) = sx;
		*(float *)(placement + 0x08) = sy;
		/* Clear _hud_use_high_resolution_scale_bit (bit 2) so placement is not halved twice */
		*(uint16_t *)(placement + 0x0C) &= (uint16_t)~(1u << 2);
	}
}

static int static_hud_element_follows_zoom(uint8_t const *static_elem)
{
	int32_t ov_count;
	uint8_t const *ov_elems;
	int32_t oi;

	if (!static_elem)
		return 0;
	/* multitexture_overlays tag_block is at +0x58 inside static_hud_element_definition (stride 0x1E0) */
	ov_count = *(int32_t const *)(static_elem + 0x58);
	ov_elems = XBOX_POINTER(uint8_t const, *(uint32_t const *)(static_elem + 0x5C)); /* AE hook */
	if (ov_count <= 0 || ov_count > 32 || !ov_elems)
		return 0;
	for (oi = 0; oi < ov_count; oi++)
	{
		uint8_t const *ov = ov_elems + (uint32_t)oi * 0x1E0u;
		int32_t fn_count = *(int32_t const *)(ov + 0x154u);
		uint8_t const *fn_elems = XBOX_POINTER(uint8_t const, *(uint32_t const *)(ov + 0x158u)); /* AE hook */
		int32_t fi;
		if (fn_count <= 0 || fn_count > 32 || !fn_elems)
			continue;
		for (fi = 0; fi < fn_count; fi++)
		{
			uint8_t const *eff = fn_elems + (uint32_t)fi * 0xDCu;
			int16_t source = *(int16_t const *)(eff + 0x44u);
			if (source == 7 /* _hud_multitexture_overlay_effector_source_zoom_level */)
				return 1;
		}
	}
	return 0;
}

static int hud_tag_ref_is_fullscreen_scope_mask(
	struct forge_cache_file_tag_instance const *instances,
	long total,
	uint8_t const *tag_ref_ptr)
{
	int32_t ref_idx;
	uint32_t abs_idx;
	uint8_t const *bitm_base;
	int32_t b_cnt;
	uint8_t const *b_elems;

	if (!instances || !tag_ref_ptr)
		return 0;
	ref_idx = *(int32_t const *)(tag_ref_ptr + 0x0C);
	if (ref_idx == NONE)
		return 0;
	abs_idx = (uint32_t)ref_idx & 0xFFFFu;
	if ((long)abs_idx >= total || instances[abs_idx].group_tag != 'bitm' || !instances[abs_idx].base_address)
		return 0;
	bitm_base = XBOX_POINTER(uint8_t const, instances[abs_idx].base_address); /* AE hook */
	b_cnt = *(int32_t const *)(bitm_base + 0x60u);
	b_elems = XBOX_POINTER(uint8_t const, *(uint32_t const *)(bitm_base + 0x64u)); /* AE hook */
	if (b_cnt > 0 && b_cnt <= 64 && b_elems)
	{
		uint16_t w = *(uint16_t const *)(b_elems + 0x04u);
		uint16_t h = *(uint16_t const *)(b_elems + 0x06u);
		if (w >= 512u && h >= 512u)
			return 1;
	}
	return 0;
}

int forge_is_mcc_hud_active(void)
{
	return s_mcc_hud_active;
}

void forge_normalize_mcc_hud_tags(void)
{
	long total = 0, i;
	struct forge_cache_file_tag_instance *instances;
	int mcc_detected = 0;

	s_mcc_hud_active = 0;
	instances = (struct forge_cache_file_tag_instance *)cache_files_tag_instances(&total);
	if (!instances || total <= 0)
		return;

	/* 1. Detect MCC 2x HUD on hud# tags EXCLUDING ui\hud\counter (which is 24/18 on all stock maps) */
	for (i = 0; i < total; i++)
	{
		if (instances[i].group_tag == 'hud#' && instances[i].base_address && instances[i].name)
		{
			uint8_t const *p;
			int8_t char_w, scr_w;
			if (_stricmp(XBOX_POINTER(char const, instances[i].name), "ui\\hud\\counter") == 0 || /* AE hook */
			    _stricmp(XBOX_POINTER(char const, instances[i].name), "ui/hud/counter") == 0) /* AE hook */
			{
				continue;
			}
			p = XBOX_POINTER(uint8_t const, instances[i].base_address); /* AE hook */
			char_w = *(int8_t const *)(p + 0x10);
			scr_w  = *(int8_t const *)(p + 0x11);
			if (char_w >= 20 || scr_w >= 16)
			{
				mcc_detected = 1;
				break;
			}
		}
	}

	if (!mcc_detected)
		return;

	s_mcc_hud_active = 1;

	/* 2. Normalize non-counter hud#, hudg, wphi, grhi, and unhi tags */
	for (i = 0; i < total; i++)
	{
		uint32_t grp = instances[i].group_tag;
		uint8_t *base = XBOX_POINTER(uint8_t, instances[i].base_address); /* AE hook */
		if (!base)
			continue;

		if (grp == 'hud#')
		{
			int8_t char_w, scr_w, x_off, y_off, dec_w, col_w;
			if (instances[i].name &&
			    (_stricmp(XBOX_POINTER(char const, instances[i].name), "ui\\hud\\counter") == 0 || /* AE hook */
			     _stricmp(XBOX_POINTER(char const, instances[i].name), "ui/hud/counter") == 0)) /* AE hook */
			{
				continue;
			}
			char_w = *(int8_t *)(base + 0x10);
			scr_w  = *(int8_t *)(base + 0x11);
			if (char_w >= 20 || scr_w >= 16)
			{
				x_off = *(int8_t *)(base + 0x12);
				y_off = *(int8_t *)(base + 0x13);
				dec_w = *(int8_t *)(base + 0x14);
				col_w = *(int8_t *)(base + 0x15);
				*(int8_t *)(base + 0x10) = (int8_t)(((int)char_w + 1) / 2);
				*(int8_t *)(base + 0x11) = (int8_t)(((int)scr_w + 1) / 2);
				*(int8_t *)(base + 0x12) = (int8_t)((int)x_off / 2);
				*(int8_t *)(base + 0x13) = (int8_t)((int)y_off / 2);
				*(int8_t *)(base + 0x14) = (int8_t)(((int)dec_w + 1) / 2);
				*(int8_t *)(base + 0x15) = (int8_t)(((int)col_w + 1) / 2);
			}
		}
		else if (grp == 'hudg')
		{
			int32_t btn_cnt, bi;
			uint8_t *btn_elems;
			float ms_scale;

			/* messaging.placement (+0x024) */
			normalize_hud_placement_at(base + 0x024, 1, 1);
			/* messaging.button_icons (+0x0A8, stride 0x10) */
			btn_cnt = *(int32_t *)(base + 0x0A8);
			btn_elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x0AC)); /* AE hook */
			if (btn_cnt > 0 && btn_cnt <= 64 && btn_elems)
			{
				for (bi = 0; bi < btn_cnt; bi++)
				{
					uint8_t *ic = btn_elems + (uint32_t)bi * 0x10u;
					*(int16_t *)(ic + 0x02) = (int16_t)(*(int16_t *)(ic + 0x02) / 2);
					*(int16_t *)(ic + 0x04) = (int16_t)(*(int16_t *)(ic + 0x04) / 2);
					*(int16_t *)(ic + 0x06) = (int16_t)(*(int16_t *)(ic + 0x06) / 2);
				}
			}
			/* waypoint offsets (+0x120..+0x12C) */
			*(float *)(base + 0x120) *= 0.5f;
			*(float *)(base + 0x124) *= 0.5f;
			*(float *)(base + 0x128) *= 0.5f;
			*(float *)(base + 0x12C) *= 0.5f;
			/* defaults.motion_sensor_scale (+0x2D8) */
			ms_scale = *(float *)(base + 0x2D8);
			if (ms_scale > 0.6f)
				*(float *)(base + 0x2D8) = ms_scale * 0.5f;
			/* damage_indicators offsets (+0x310..+0x316) */
			*(int16_t *)(base + 0x310) = (int16_t)(*(int16_t *)(base + 0x310) / 2);
			*(int16_t *)(base + 0x312) = (int16_t)(*(int16_t *)(base + 0x312) / 2);
			*(int16_t *)(base + 0x314) = (int16_t)(*(int16_t *)(base + 0x314) / 2);
			*(int16_t *)(base + 0x316) = (int16_t)(*(int16_t *)(base + 0x316) / 2);
		}
		else if (grp == 'wphi')
		{
			int32_t cnt, k;
			uint8_t *elems;

			/* statics (+0x60, stride 0xB4, static_element at +0x24) */
			cnt = *(int32_t *)(base + 0x60);
			elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x64)); /* AE hook */
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *st = elems + (uint32_t)k * 0xB4u + 0x24u;
					if (!static_hud_element_follows_zoom(st))
						normalize_hud_placement_at(st, 1, 1);
				}
			}
			/* meters (+0x6C, stride 0xB4, meter_element at +0x24) */
			cnt = *(int32_t *)(base + 0x6C);
			elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x70)); /* AE hook */
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
					normalize_hud_placement_at(elems + (uint32_t)k * 0xB4u + 0x24u, 1, 1);
			}
			/* numbers (+0x78, stride 0xA0, number_element at +0x24) */
			cnt = *(int32_t *)(base + 0x78);
			elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x7C)); /* AE hook */
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *num_el = elems + (uint32_t)k * 0xA0u + 0x24u;
					uint8_t num_flags = *(uint8_t *)(num_el + 0x45u);
					if (!(num_flags & (1u << 1))) /* skip _hud_number_show_only_when_zoomed_bit */
						normalize_hud_placement_at(num_el, 1, 0);
				}
			}
			/* crosshairs (+0x84, stride 0x68; bitmap at +0x24, items at +0x34 stride 0x6C) */
			cnt = *(int32_t *)(base + 0x84);
			elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x88)); /* AE hook */
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *ch = elems + (uint32_t)k * 0x68u;
					int32_t icnt = *(int32_t *)(ch + 0x34u);
					uint8_t *ielems = XBOX_POINTER(uint8_t, *(uint32_t *)(ch + 0x38u)); /* AE hook */
					int32_t ii;
					if (hud_tag_ref_is_fullscreen_scope_mask(instances, total, ch + 0x24u))
						continue;
					if (icnt > 0 && icnt <= 64 && ielems)
					{
						for (ii = 0; ii < icnt; ii++)
							normalize_hud_placement_at(ielems + (uint32_t)ii * 0x6Cu, 1, 1);
					}
				}
			}
			/* overlays (+0x90, stride 0x68; items at +0x34 stride 0x88) */
			cnt = *(int32_t *)(base + 0x90);
			elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x94)); /* AE hook */
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *ov = elems + (uint32_t)k * 0x68u;
					int32_t icnt = *(int32_t *)(ov + 0x34u);
					uint8_t *ielems = XBOX_POINTER(uint8_t, *(uint32_t *)(ov + 0x38u)); /* AE hook */
					int32_t ii;
					if (icnt > 0 && icnt <= 64 && ielems)
					{
						for (ii = 0; ii < icnt; ii++)
							normalize_hud_placement_at(ielems + (uint32_t)ii * 0x88u, 1, 1);
					}
				}
			}
			/* messaging_icon (+0x13C) */
			*(int16_t *)(base + 0x13C + 0x02) = (int16_t)(*(int16_t *)(base + 0x13C + 0x02) / 2);
			*(int16_t *)(base + 0x13C + 0x04) = (int16_t)(*(int16_t *)(base + 0x13C + 0x04) / 2);
			*(int16_t *)(base + 0x13C + 0x06) = (int16_t)(*(int16_t *)(base + 0x13C + 0x06) / 2);
		}
		else if (grp == 'grhi')
		{
			int32_t icnt, ii;
			uint8_t *ielems;

			/* background.placement (+0x024) */
			normalize_hud_placement_at(base + 0x024, 1, 1);
			/* grenade_count_panel.background.placement (+0x08C) */
			normalize_hud_placement_at(base + 0x08C, 1, 1);
			/* grenade_count_panel.numbers.placement (+0x0F4) */
			normalize_hud_placement_at(base + 0x0F4, 1, 0);
			/* overlays.items (+0x15C, stride 0x88) */
			icnt = *(int32_t *)(base + 0x15C);
			ielems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x160)); /* AE hook */
			if (icnt > 0 && icnt <= 64 && ielems)
			{
				for (ii = 0; ii < icnt; ii++)
					normalize_hud_placement_at(ielems + (uint32_t)ii * 0x88u, 1, 1);
			}
		}
		else if (grp == 'unhi')
		{
			int32_t cnt, k;
			uint8_t *elems;

			/* Exact struct unit_hud_interface_definition offsets (from unit_hud_interface_definition.h): */
			normalize_hud_placement_at(base + 0x024, 1, 1); /* background.placement */
			normalize_hud_placement_at(base + 0x08C, 1, 1); /* shield_meter.background.placement */
			normalize_hud_placement_at(base + 0x0F4, 1, 1); /* shield_meter.meter.placement */
			normalize_hud_placement_at(base + 0x17C, 1, 1); /* health_meter.background.placement */
			normalize_hud_placement_at(base + 0x1E4, 1, 1); /* health_meter.meter.placement */
			normalize_hud_placement_at(base + 0x26C, 1, 1); /* motion_sensor.background.placement */
			normalize_hud_placement_at(base + 0x2D4, 1, 1); /* motion_sensor.foreground.placement */
			normalize_hud_placement_at(base + 0x35C, 1, 1); /* blip_placement */

			/* auxilary_panel.auxilary_overlays (tag_block at +0x3A4, stride 0x84, placement at +0x00) */
			cnt = *(int32_t *)(base + 0x3A4);
			elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x3A8)); /* AE hook */
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
					normalize_hud_placement_at(elems + (uint32_t)k * 0x84u, 1, 1);
			}

			/* auxilary_meters (tag_block at +0x3CC, stride 0x144, panel.background at +0x14, panel.meter at +0x7C) */
			cnt = *(int32_t *)(base + 0x3CC);
			elems = XBOX_POINTER(uint8_t, *(uint32_t *)(base + 0x3D0)); /* AE hook */
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *am = elems + (uint32_t)k * 0x144u;
					normalize_hud_placement_at(am + 0x14u, 1, 1);
					normalize_hud_placement_at(am + 0x7Cu, 1, 1);
				}
			}
		}
	}
}

/* the file a level's saved Forge state is read from (the map's own file as the game finds it, then the folders Forge saves
into); NULL for none */
static FILE *saved_state_file_open(char const *map_name, char const *leaf)
{
	char map_path[512];
	FILE *f;

	f = forge_saved_map_open(map_name);
	if (!f && !mcc_level_name(map_name))
	{
		snprintf(map_path, sizeof(map_path), "custom_maps/%s.map", leaf);
		f = fopen(map_path, "rb");
	}
	if (!f && !mcc_level_name(map_name))
	{
		static char const *const state_map_cands[] = {
			"OpenCE Forge/forge_maps/%s.map",
			"OpenCE Forge/forge_maps/%s/%s.map",
			"Faber_data/forge_maps/%s/%s.map",
			"forge_maps/%s.map",
			"forge_maps/%s/%s.map"
		};
		int ci;
		for (ci = 0; ci < 5 && !f; ci++)
		{
			if (strstr(state_map_cands[ci], "%s/%s"))
				snprintf(map_path, sizeof(map_path), state_map_cands[ci], leaf, leaf);
			else
				snprintf(map_path, sizeof(map_path), state_map_cands[ci], leaf);
			f = fopen(map_path, "rb");
		}
	}
	return f;
}

/* TRUE for a level that Forge has a part in: a Forge session, or a map saved by Forge (its file says so). Any other level
is left as it is: none of its tags changed, no Forge data made for it */
int forge_level_is_forges(char const *map_name)
{
	char const *slash;
	char leaf[64];
	uint8_t hdr[0x200];
	uint32_t sig = 0;
	FILE *f;

	if (g_forge_session_enabled)
		return 1;
	if (!map_name || !map_name[0])
		return 0;
	slash = strrchr(map_name, '\\');
	if (!slash)
		slash = strrchr(map_name, '/');
	snprintf(leaf, sizeof(leaf), "%s", slash ? (slash + 1) : map_name);
	f = saved_state_file_open(map_name, leaf);
	if (f)
	{
		if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr))
			memcpy(&sig, hdr + 0x1FCu, 4);
		fclose(f);
	}
	if (sig == 0x464C4731u)
		return 1;
	/* (a map whose Forge state is a text file beside Forge's maps) */
	{
		static char const *const texts[] = { "OpenCE Forge/forge_maps/%s/%s.txt", "Faber_data/forge_maps/%s/%s.txt", "forge_maps/%s/%s.txt",
			"custom_maps/%s.txt" };
		char path[512];
		int each;

		for (each = 0; each < 4; each++)
		{
			snprintf(path, sizeof(path), texts[each], leaf, leaf);
			f = fopen(path, "rb");
			if (f)
			{
				fclose(f);
				return 1;
			}
		}
	}
	return 0;
}

void forge_load_saved_map_state_if_present(char const *map_name)
{
	char const *slash;
	char leaf[64];
	char map_path[512];
	char txt_path[512];
	FILE *f;

	{
		char const *active_map = forge_get_active_map_name();
		if (active_map && active_map[0])
			map_name = active_map;
	}
	if (!map_name || !map_name[0])
		return;
	/* (a level Forge has no part in: nothing of it is changed) */
	if (!forge_level_is_forges(map_name))
		return;

	/* Auto-populate uncached local map tags ([MAP]) into the Forge palette and normalize MCC 2x HUD tags */
	forge_runtime_init();
	forge_SyncScenarioTagsWithLoadedMap();
	forge_normalize_mcc_hud_tags();

	/* Patch any baked vehicles and custom scenery/devices/lights in the loaded map so vehicle seats and long-distance LOD rendering work.
	 * Never a campaign level's: Forge plays only multiplayer maps, and this runs for every level that loads. On a
	 * campaign level it renamed seat 0 of every vehicle to "W-driver" - b30_evolved's insertion Pelican's
	 * "P-riderLF", so (unit_enter_vehicle (player0) insertion_pelican_1 "p-riderlf") found no seat and the
	 * player was left on the beach - and changed its lights and scenery too */
	if (!global_scenario_try_and_get() || global_scenario_try_and_get()->type != 0 /* _scenario_type_solo */)
	{
		long tag_cnt = 0, ti;
		struct forge_cache_file_tag_instance *instances =
			(struct forge_cache_file_tag_instance *)cache_files_tag_instances(&tag_cnt);
		if (instances && tag_cnt > 0)
		{
			for (ti = 0; ti < tag_cnt; ti++)
			{
				uint32_t g = (uint32_t)instances[ti].group_tag;
				if (!instances[ti].base_address)
					continue;
				if (g == 'vehi')
					patch_vehicle_tag_for_player_driving(instances[ti].tag_index, XBOX_POINTER(char const, instances[ti].name)); /* AE hook */
				else if (g == 'scen' || g == 'mach' || g == 'lifi' || g == 'ctrl')
					patch_forge_device_and_light_tag_runtime(instances[ti].tag_index, g, XBOX_POINTER(char const, instances[ti].name)); /* AE hook */
			}
		}
		if (object_header_data)
		{
			unsigned long scen_mask =
				_object_mask_scenery |
				_object_mask_machine |
				_object_mask_control |
				_object_mask_light_fixture;
			long o_idx;
			for (o_idx = data_next_index(object_header_data, NONE);
				 o_idx != NONE;
				 o_idx = data_next_index(object_header_data, o_idx))
			{
				struct object_datum *obj = (struct object_datum *)object_try_and_get_and_verify_type(o_idx, scen_mask);
				if (obj && obj->definition_index != NONE)
				{
					uint8_t const *obuf = (uint8_t const *)object_definition_get(obj->definition_index);
					if (obuf)
					{
						float def_br = *(float const *)(obuf + 0x04u);
						if (obj->object.bounding_sphere_radius < def_br)
							obj->object.bounding_sphere_radius = def_br;
					}
				}
			}
		}
	}

	/* Clients receive the live Forge object state from the Host via !FH: / !FM: / !FD: packets */
	if (game_connection() == _game_connection_network_client)
		return;

	slash = strrchr(map_name, '\\');
	if (!slash)
		slash = strrchr(map_name, '/');
	snprintf(leaf, sizeof(leaf), "%s", slash ? (slash + 1) : map_name);

	/* 1. Read embedded [OpenCE Forge Map] / [ForgeState] directly from .map header (0x1E4 / 0x1E8)
	(the map's file as the game finds it, as forge_saved_map and the bake do:
	in a Map Manager folder of custom_maps too, whose maps were played with
	none of their objects. An MCC map's level, Forge's own in mcc_maps\forge:
	its file, and nothing beside) */
	f = saved_state_file_open(map_name, leaf);
	if (f)
	{
		uint8_t hdr[0x200];
		if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr))
		{
			uint32_t state_off = 0, state_sz = 0, sig = 0;
			memcpy(&state_off, hdr + 0x1E4u, 4);
			memcpy(&state_sz, hdr + 0x1E8u, 4);
			memcpy(&sig, hdr + 0x1FCu, 4);
			if (sig == 0x464C4731u)
			{
				forge_runtime_init();
				snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", leaf);
				g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
				g_forge.has_saved_map_once = 1;
				if (state_off >= 0x800u && state_sz > 0 && state_sz < 4u * 1024u * 1024u)
				{
					char *buf = (char *)malloc(state_sz + 1u);
					if (buf)
					{
						if (fseek(f, (long)state_off, SEEK_SET) == 0 && fread(buf, 1, state_sz, f) == state_sz)
						{
							buf[state_sz] = 0;
							parse_forge_state_text_buffer(buf, leaf);
							free(buf);
							fclose(f);
							return;
						}
						free(buf);
					}
				}
			}
		}
		fclose(f);
	}

	/* 2. Legacy fallback for maps saved before embedded state (never an MCC map's: Forge saved none so) */
	if (mcc_level_name(map_name))
		return;
	snprintf(txt_path, sizeof(txt_path), "OpenCE Forge/forge_maps/%s/%s.txt", leaf, leaf);
	f = fopen(txt_path, "rb");
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "Faber_data/forge_maps/%s/%s.txt", leaf, leaf);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "forge_maps/%s/%s.txt", leaf, leaf);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "custom_maps/%s.txt", leaf);
		f = fopen(txt_path, "rb");
	}
	if (f)
	{
		long sz;
		fseek(f, 0, SEEK_END);
		sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (sz > 0 && sz < 4 * 1024 * 1024)
		{
			char *buf = (char *)malloc((size_t)sz + 1u);
			if (buf)
			{
				size_t rd = fread(buf, 1, (size_t)sz, f);
				buf[rd] = 0;
				parse_forge_state_text_buffer(buf, leaf);
				free(buf);
			}
		}
		fclose(f);
	}
}

/* ============================================================================
 * MONITOR ("characters\monitor\monitor") & NATIVE MODEL RENDERER
 * - Renders 343 Guilty Spark ("characters\monitor\monitor") and 3D Spawn Menu
 *   previews directly through OpenCE's native orig_render_model!
 * ============================================================================ */

/* the next draw_native_object_model_at's colors and armor, as the tag's (not a
real object's, which has its own): set by forge_model_look_use, gone once drawn */
static struct
{
	int active;
	float colors[4][3];
	int colored[4];
	short forced_shader;
} s_model_look;

static void forge_model_look_use(long object_definition, char const *actor_path)
{
	forge_model_look_get(object_definition, actor_path, s_model_look.colors, s_model_look.colored,
		&s_model_look.forced_shader);
	s_model_look.active = 1;
}

static int draw_native_object_model_at(
	long obj_def_idx,
	long unique_id,
	void const *lighting_ptr,
	union real_point3d const *pos,
	union real_vector3d const *forward,
	union real_vector3d const *up,
	int is_monitor,
	int is_preview,
	unsigned long flags)
{
	uint8_t const *obj_def;
	float radius;
	long model_idx;
	struct model const *mdef;
	struct real_matrix4x3 node_matrices[64];
	static struct render_lighting studio_lighting;
	struct render_lighting const *use_lighting;
	static union real_rgb_color change_colors_mon[4];
	static union real_rgb_color change_colors_std[4];
	static real function_values[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	union real_rgb_color *change_colors = is_monitor ? change_colors_mon : change_colors_std;
	union real_point3d draw_centroid = *pos;
	int node_count, n;

	if (obj_def_idx == NONE)
		return 0;
	obj_def = (uint8_t const *)tag_get(tag_get_group_tag(obj_def_idx), obj_def_idx);
	if (!obj_def)
		return 0;

	radius = *(float const *)(obj_def + 0x04u);
	if (!(radius > 0.1f && radius < 100.0f))
		radius = 1.5f;

	model_idx = *(int32_t const *)(obj_def + 0x34u);
	if (model_idx == NONE)
		return 0;
	mdef = (struct model const *)tag_get('mode', model_idx);
	if (!mdef)
		return 0;

	node_count = (int)mdef->nodes.count;
	if (node_count <= 0)
		return 0;
	if (node_count > 64)
		node_count = 64;

	memset(node_matrices, 0, sizeof(node_matrices));
	if (is_preview)
	{
		float preview_scale = 0.52f / ((radius > 0.15f) ? radius : 0.52f);
		struct real_matrix4x3 scale_pos_mat;
		if (preview_scale < 0.035f) preview_scale = 0.035f;
		if (preview_scale > 1.35f) preview_scale = 1.35f;

		model_get_node_matrices(mdef, node_matrices, global_origin3d, forward, up);
		matrix4x3_from_point_and_vectors(&scale_pos_mat, pos, global_forward3d, global_up3d);
		scale_pos_mat.scale = preview_scale;
		for (n = 0; n < node_count; n++)
		{
			struct real_matrix4x3 tmp = node_matrices[n];
			matrix4x3_multiply(&scale_pos_mat, &tmp, &node_matrices[n]);
		}
		radius *= preview_scale;
	}
	else if (is_monitor)
	{
		union real_point3d mpos = *pos;
		struct real_matrix4x3 scale_pos_mat;
		mpos.z += 0.55f;
		draw_centroid = mpos;
		model_get_node_matrices(mdef, node_matrices, global_origin3d, forward, up);
		matrix4x3_from_point_and_vectors(&scale_pos_mat, &mpos, global_forward3d, global_up3d);
		scale_pos_mat.scale = 1.35f;
		for (n = 0; n < node_count; n++)
		{
			struct real_matrix4x3 tmp = node_matrices[n];
			matrix4x3_multiply(&scale_pos_mat, &tmp, &node_matrices[n]);
		}
		radius *= 1.35f;
	}
	else
	{
		model_get_node_matrices(mdef, node_matrices, pos, forward, up);
	}

	if (is_monitor)
	{
		change_colors[0].red = 0.28f;
		change_colors[0].green = 0.72f;
		change_colors[0].blue = 1.0f;
		change_colors[1].red = 0.65f;
		change_colors[1].green = 0.90f;
		change_colors[1].blue = 1.0f;
	}
	else
	{
		change_colors[0].red = 0.85f;
		change_colors[0].green = 0.85f;
		change_colors[0].blue = 0.85f;
		change_colors[1].red = 0.75f;
		change_colors[1].green = 0.75f;
		change_colors[1].blue = 0.75f;
	}
	change_colors[2] = change_colors[0];
	change_colors[3] = change_colors[1];
	if (s_model_look.active)
	{
		for (n = 0; n < 4; n++)
		{
			if (s_model_look.colored[n])
			{
				change_colors[n].red = s_model_look.colors[n][0];
				change_colors[n].green = s_model_look.colors[n][1];
				change_colors[n].blue = s_model_look.colors[n][2];
			}
		}
	}

	function_values[0] = 1.0f;
	function_values[1] = 1.0f;
	function_values[2] = 1.0f;
	function_values[3] = 1.0f;

	if (lighting_ptr)
	{
		use_lighting = (struct render_lighting const *)lighting_ptr;
	}
	else
	{
		memset(&studio_lighting, 0, sizeof(studio_lighting));
		studio_lighting.ambient_color.red = 0.72f;
		studio_lighting.ambient_color.green = 0.74f;
		studio_lighting.ambient_color.blue = 0.78f;
		studio_lighting.distant_light_count = 1;
		studio_lighting.distant_lights[0].color.red = 0.95f;
		studio_lighting.distant_lights[0].color.green = 0.95f;
		studio_lighting.distant_lights[0].color.blue = 0.92f;
		studio_lighting.distant_lights[0].direction.i = -0.577f;
		studio_lighting.distant_lights[0].direction.j = -0.577f;
		studio_lighting.distant_lights[0].direction.k = -0.577f;
		use_lighting = &studio_lighting;
	}

	orig_render_model(
		model_idx,
		10000.0f,
		node_matrices,
		NULL,
		change_colors,
		function_values,
		use_lighting,
		&draw_centroid,
		radius,
		NULL,
		(is_monitor || is_preview) ? NONE : unique_id,
		s_model_look.active ? s_model_look.forced_shader : 0,
		is_preview ? (flags | (1u << 3)) /* _render_model_first_person_bit */ : flags);
	s_model_look.active = 0;
	return 1;
}

enum forge_marker_color_preset
{
	_marker_color_ffa_yellow = 0,
	_marker_color_red_team,
	_marker_color_blue_team,
	_marker_color_tele_green,
	_marker_color_race_orange,
	_marker_color_oddball_purple,
	_marker_color_koth_gold,
	NUMBER_OF_MARKER_COLOR_PRESETS
};

enum forge_marker_mesh_kind
{
	_marker_mesh_player_spawn = 0,
	_marker_mesh_tele_origin,
	_marker_mesh_tele_dest,
	_marker_mesh_race,
	_marker_mesh_oddball,
	_marker_mesh_ctf_flag,
	_marker_mesh_koth,
	_marker_mesh_koth_beam,
	_marker_mesh_firefight_spawn,
	NUMBER_OF_MARKER_MESH_KINDS
};

static struct
{
	int loaded;
	struct
	{
		int valid;
		int part_count;
		float radius;
		struct forge_hw_part parts[FORGE_MAX_HW_PARTS];
	} meshes[NUMBER_OF_MARKER_MESH_KINDS];
} s_marker_meshes = { 0 };

static long get_or_create_translucent_marker_shader(int color_preset, int is_highlighted, int is_z_prime)
{
	static uint32_t const s_preset_rgb[NUMBER_OF_MARKER_COLOR_PRESETS] = {
		0x00FFE618u, /* FFA Player Spawn     - Translucent Yellow */
		0x00FF2424u, /* Red Team Spawn/Flag  - Translucent Red    */
		0x002488FFu, /* Blue Team Spawn/Flag - Translucent Blue   */
		0x0024FF54u, /* Teleporter In/Out    - Translucent Green  */
		0x00FF8C14u, /* Race Marker          - Translucent Orange */
		0x00BC38FFu, /* Oddball Marker       - Translucent Purple */
		0x00FFC824u  /* KOTH Hill Marker     - Translucent Gold   */
	};
	char shdr_name[96];
	char bitm_name[96];
	long shdr_idx, tmpl_soso_idx, bitm_idx;
	struct tag_iterator iter;
	void *tmpl_ptr;
	struct bitmap_data *bmp;
	struct bitmap_group *bgrp;
	uint8_t *soso_def;
	uint32_t rgb, alpha_byte, argb;
	uint32_t *pix;
	float r_f, g_f, b_f;
	int i;

	if (color_preset < 0 || color_preset >= NUMBER_OF_MARKER_COLOR_PRESETS)
		color_preset = _marker_color_ffa_yellow;

	/* (is_highlighted 2 and 3: a faint tint, a volume's faces, and the one
	pointed at a little less faint) */
	if (is_highlighted < 0 || is_highlighted > 3)
		is_highlighted = 1;
	snprintf(shdr_name, sizeof(shdr_name), "forge\\markers\\shdr_%d_%d_%d",
		color_preset, is_highlighted, is_z_prime ? 1 : 0);
	shdr_idx = tag_loaded('soso', shdr_name);
	if (shdr_idx != NONE)
		return shdr_idx;

	rgb = s_preset_rgb[color_preset];
	alpha_byte = is_z_prime ? 0xFFu : is_highlighted == 3 ? 0x40u : is_highlighted == 2 ? 0x24u :
		(is_highlighted ? 0xB8u : 0x8Cu);
	argb = (alpha_byte << 24) | rgb;
	r_f = (float)((rgb >> 16) & 0xFFu) / 255.0f;
	g_f = (float)((rgb >> 8) & 0xFFu) / 255.0f;
	b_f = (float)(rgb & 0xFFu) / 255.0f;

	snprintf(bitm_name, sizeof(bitm_name), "forge\\markers\\bitm_%d_%d_%d",
		color_preset, is_highlighted, is_z_prime ? 1 : 0);
	bitm_idx = tag_loaded('bitm', bitm_name);
	if (bitm_idx == NONE)
	{
		/* Format 11 = _bitmap_format_a8r8g8b8 (32-bit ARGB) */
		bmp = bitmap_2d_new(16, 16, 0, 11);
		if (!bmp || !bmp->base_address)
			return NONE;
		pix = XBOX_POINTER(uint32_t, bmp->base_address); /* AE hook */
		for (i = 0; i < 16 * 16; i++)
			pix[i] = argb;
		bitmap_rebuild(bmp);

		bgrp = (struct bitmap_group *)calloc(1, sizeof(struct bitmap_group));
		if (!bgrp)
			return NONE;
		bgrp->bitmaps.count = 1;
		bgrp->bitmaps.address = XBOX_ADDRESS(bmp); /* AE hook */
		bitm_idx = append_runtime_tag_instance('bitm', NONE, NONE, bitm_name, bgrp);
		if (bitm_idx == NONE)
			return NONE;
	}

	tag_iterator_new(&iter, 'soso');
	tmpl_soso_idx = tag_iterator_next(&iter);
	if (tmpl_soso_idx == NONE)
		return NONE;
	tmpl_ptr = shader_definition_get(tmpl_soso_idx);
	if (!tmpl_ptr)
		return NONE;

	soso_def = (uint8_t *)calloc(1, 512);
	if (!soso_def)
		return NONE;
	memcpy(soso_def, tmpl_ptr, 440);

	*(int16_t *)(soso_def + 0x24) = 4; /* _shader_type_model */
	/* Bit 1 = _shader_model_two_sided_bit, Bit 2 = _shader_model_not_alpha_tested_bit, Bit 3 = _shader_model_alpha_blended_decal_bit */
	*(uint16_t *)(soso_def + 0x28) = is_z_prime ? ((1u << 1) | (1u << 2)) : ((1u << 1) | (1u << 2) | (1u << 3));
	*(float *)(soso_def + 0x38) = is_highlighted == 3 ? 0.26f : is_highlighted == 2 ? 0.16f :
		is_highlighted ? 0.72f : 0.55f;
	*(int16_t *)(soso_def + 0x4C) = 0;
	*(uint16_t *)(soso_def + 0x6C) = 1u; /* _shader_model_self_illumination_no_random_phase_bit */
	*(int16_t *)(soso_def + 0x70) = 0;
	*(int16_t *)(soso_def + 0x72) = 0;
	*(float *)(soso_def + 0x74) = 1.0f;
	{
		float glow = is_highlighted >= 2 ? 0.2f : 0.34f;

		*(float *)(soso_def + 0x78) = r_f * glow;
		*(float *)(soso_def + 0x7C) = g_f * glow;
		*(float *)(soso_def + 0x80) = b_f * glow;
		*(float *)(soso_def + 0x84) = r_f * glow;
		*(float *)(soso_def + 0x88) = g_f * glow;
		*(float *)(soso_def + 0x8C) = b_f * glow;
	}
	*(float *)(soso_def + 0x9C) = 1.0f;
	*(float *)(soso_def + 0xA0) = 1.0f;
	*(int32_t *)(soso_def + 0xA4) = 'bitm';
	*(int32_t *)(soso_def + 0xA8) = 0;
	*(int32_t *)(soso_def + 0xAC) = 0;
	*(int32_t *)(soso_def + 0xB0) = bitm_idx;
	*(int32_t *)(soso_def + 0xC8) = NONE;
	*(int16_t *)(soso_def + 0xD4) = 0;
	*(int16_t *)(soso_def + 0xD6) = 0;
	*(float *)(soso_def + 0xD8) = 1.0f;
	*(int32_t *)(soso_def + 0xE8) = NONE;
	*(float *)(soso_def + 0xEC) = 1.0f;
	*(float *)(soso_def + 0x144) = 0.0f;
	*(float *)(soso_def + 0x154) = 0.0f;
	*(int32_t *)(soso_def + 0x170) = NONE;

	return append_runtime_tag_instance('soso', 'shdr', NONE, shdr_name, soso_def);
}

/* the objective markers' meshes (.ftag), zlib-compressed: forge_markers.bin
(port/assets/forge/forge_markers.bin, beside config.toml, where every build
puts it: tools/linux_build.py's GAME_DATA_FILES), read when they are first
wanted and written out to Faber_data/cached_tags/markers for the loader. Its
layout, little-endian: "FGMK", 32-bit version (1) and count; each marker its
name and its alias (a byte's length, then the name), its size, its packed
size and its packed bytes */
#define FORGE_MARKERS_FILE "forge_markers.bin"
void config_folder(char *path, size_t size);
char *config_file_read(const char *path, size_t *size);
void platform_log(char const *format, ...);

static uint32_t forge_markers_u32(unsigned char const *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/* writes out each marker of forge_markers.bin that markers_dir has not */
static void forge_markers_write_missing(char const *markers_dir)
{
	char path[1024];
	size_t size = 0, offset;
	unsigned char *file;
	uint32_t count, index;

	config_folder(path, sizeof(path));
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "%s", FORGE_MARKERS_FILE);
	file = (unsigned char *)config_file_read(path, &size);
	if (!file || size < 12 || memcmp(file, "FGMK", 4) || forge_markers_u32(file + 4) != 1)
	{
		platform_log("Forge: the markers' file %s %s, so objectives have no meshes", path,
			!file ? "cannot be read" : "is not one this game reads");
		(free)(file); /* AE hook */
		return;
	}
	count = forge_markers_u32(file + 8);
	offset = 12;
	for (index = 0; index < count; index++)
	{
		char name[256], mpath[512];
		size_t length;
		uint32_t unpacked_size, packed_size;
		FILE *chk;

		if (offset + 1 > size || offset + 1 + file[offset] + 1 > size)
			break;
		length = file[offset];
		memcpy(name, file + offset + 1, length);
		name[length] = 0;
		offset += 1 + length;
		offset += 1 + file[offset];
		if (offset + 8 > size)
			break;
		unpacked_size = forge_markers_u32(file + offset);
		packed_size = forge_markers_u32(file + offset + 4);
		offset += 8;
		if (offset + packed_size > size)
			break;
		snprintf(mpath, sizeof(mpath), "%s/%s", markers_dir, name);
		chk = fopen(mpath, "rb");
		if (chk)
			fclose(chk);
		else
		{
			uLongf out_len = (uLongf)unpacked_size;
			uint8_t *raw = (uint8_t *)malloc(out_len);

			if (raw)
			{
				if (uncompress(raw, &out_len, file + offset, (uLong)packed_size) == Z_OK &&
					out_len == (uLongf)unpacked_size)
				{
					FILE *wf = fopen(mpath, "wb");
					if (wf)
					{
						fwrite(raw, 1, (size_t)out_len, wf);
						fclose(wf);
					}
				}
				free(raw);
			}
		}
		offset += packed_size;
	}
	(free)(file); /* AE hook */
}

static void ensure_objective_marker_meshes_loaded(void)
{
	static char const *const s_files[NUMBER_OF_MARKER_MESH_KINDS] = {
		"Faber_data/cached_tags/markers/player_spawn.ftag",
		"Faber_data/cached_tags/markers/teleporter_origin.ftag",
		"Faber_data/cached_tags/markers/teleporter_destination.ftag",
		"Faber_data/cached_tags/markers/race_marker.ftag",
		"Faber_data/cached_tags/markers/oddball_marker.ftag",
		"Faber_data/cached_tags/markers/ctf_flag_marker.ftag",
		"Faber_data/cached_tags/markers/koth_marker.ftag",
		"Faber_data/cached_tags/markers/koth_beam.ftag",
		"Faber_data/cached_tags/markers/firefight_spawn.ftag"
	};
	int k;

	if (s_marker_meshes.loaded && s_marker_meshes_loaded_flag)
		return;
	s_marker_meshes.loaded = 1;
	s_marker_meshes_loaded_flag = 1;

#if FORGE_HOST_FILES
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);
	CreateDirectoryA("Faber_data/cached_tags/markers", NULL);
#endif
	forge_markers_write_missing("Faber_data/cached_tags/markers");

	for (k = 0; k < NUMBER_OF_MARKER_MESH_KINDS; k++)
	{
		s_marker_meshes.meshes[k].valid = 0;
		s_marker_meshes.meshes[k].part_count = 0;
		s_marker_meshes.meshes[k].radius = 0.45f;
		if (load_hw_parts_from_ftag_file(
				s_files[k],
				s_marker_meshes.meshes[k].parts,
				FORGE_MAX_HW_PARTS,
				&s_marker_meshes.meshes[k].part_count,
				&s_marker_meshes.meshes[k].radius,
				1.0f,
				0.0f))
		{
			s_marker_meshes.meshes[k].valid = (s_marker_meshes.meshes[k].part_count > 0) ? 1 : 0;
		}
		else if (k == _marker_mesh_firefight_spawn &&
			load_hw_parts_from_ftag_file(
				"Faber_data/cached_tags/markers/player_spawn.ftag",
				s_marker_meshes.meshes[k].parts,
				FORGE_MAX_HW_PARTS,
				&s_marker_meshes.meshes[k].part_count,
				&s_marker_meshes.meshes[k].radius,
				1.0f,
				0.0f))
		{
			s_marker_meshes.meshes[k].valid = (s_marker_meshes.meshes[k].part_count > 0) ? 1 : 0;
		}
	}
}

/* as a Forge level loads, while the room kept from its tags is still there
(level_contiguous_reserve_release): at the first draw it could be gone */
void forge_runtime_markers_preload(void)
{
	ensure_objective_marker_meshes_loaded();
}

static void marker_meshes_dispose(void)
{
	int k, p;

	for (k = 0; k < NUMBER_OF_MARKER_MESH_KINDS; k++)
	{
		for (p = 0; p < s_marker_meshes.meshes[k].part_count; p++)
		{
			struct forge_hw_part *part = &s_marker_meshes.meshes[k].parts[p];

			rasterizer_triangle_buffer_delete(&part->triangle_buffer);
			rasterizer_vertex_buffer_delete(&part->vertex_buffer);
			free(part->source_vertices);
			free(part->source_triangles);
			memset(part, 0, sizeof(*part));
		}
		s_marker_meshes.meshes[k].part_count = 0;
		s_marker_meshes.meshes[k].valid = 0;
	}
	s_marker_meshes.loaded = 0;
}

static void draw_translucent_marker_hw_parts(
	union real_point3d const *pos,
	union real_vector3d const *forward,
	union real_vector3d const *up,
	float radius,
	float scale,
	struct forge_hw_part *parts,
	int part_count,
	int color_preset,
	int is_highlighted,
	int is_preview)
{
	struct rasterizer_model_begin_parameters model_parameters;
	static union real_rgb_color change_colors[4];
	static real function_values[4];
	struct real_matrix4x3 node_matrix;
	long col_shdr_idx;
	struct shader *col_shdr_def;
	int p;

	if (!parts || part_count <= 0)
		return;

	(void)get_or_create_translucent_marker_shader(color_preset, is_highlighted, 1);
	col_shdr_idx = get_or_create_translucent_marker_shader(color_preset, is_highlighted, 0);
	if (col_shdr_idx == NONE)
		return;
	col_shdr_def = shader_definition_get(col_shdr_idx);
	if (!col_shdr_def)
		return;

	matrix4x3_from_point_and_vectors(&node_matrix, pos, forward, up);
	if (scale > 0.001f && scale != 1.0f)
		node_matrix.scale = scale;

	change_colors[0].red = 1.0f;
	change_colors[0].green = 1.0f;
	change_colors[0].blue = 1.0f;
	change_colors[1] = change_colors[0];
	change_colors[2] = change_colors[0];
	change_colors[3] = change_colors[0];

	function_values[0] = 1.0f;
	function_values[1] = 1.0f;
	function_values[2] = 1.0f;
	function_values[3] = 1.0f;

	memset(&model_parameters, 0, sizeof(model_parameters));
	model_parameters.unique_identifier = NONE;
	model_parameters.lighting.ambient_color.red = is_highlighted ? 0.88f : 0.72f;
	model_parameters.lighting.ambient_color.green = is_highlighted ? 0.88f : 0.72f;
	model_parameters.lighting.ambient_color.blue = is_highlighted ? 0.88f : 0.72f;
	model_parameters.lighting.distant_light_count = 1;
	model_parameters.lighting.distant_lights[0].color.red = 0.95f;
	model_parameters.lighting.distant_lights[0].color.green = 0.95f;
	model_parameters.lighting.distant_lights[0].color.blue = 0.95f;
	model_parameters.lighting.distant_lights[0].direction.i = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.j = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.k = -0.77f;
	model_parameters.centroid = *pos;
	model_parameters.radius = (radius > 0.1f) ? (radius * ((scale > 0.001f) ? scale : 1.0f)) : 0.6f;
	model_parameters.animation.colors = change_colors;
	model_parameters.animation.values = function_values;
	model_parameters.skinning.node_matrices = &node_matrix;
	model_parameters.skinning.node_matrix_count = 1;
	model_parameters.geometry_flags = (1u << 2) | (is_preview ? (1u << 4) : 0u); /* _rasterizer_geometry_no_fog_bit */
	model_parameters.base_map_scale.i = 1.0f;
	model_parameters.base_map_scale.j = 1.0f;

	rasterizer_model_begin(&model_parameters, FALSE);

	for (p = 0; p < part_count; p++)
	{
		rasterizer_model_draw(
			col_shdr_def,
			0,
			&parts[p].triangle_buffer,
			NONE,
			parts[p].triangle_buffer.count,
			&parts[p].vertex_buffer,
			NONE);
	}
	rasterizer_model_end();
	if (is_preview)
	{
		rasterizer_set_stencil_mode(0);
		rasterizer_set_frustum_z(0.0f, 0.0f);
	}
}

static int resolve_marker_kind_and_color(
	uint32_t group_tag,
	char const *tag_path,
	int16_t netgame_flag_type,
	int16_t team_designator,
	int *out_mesh_kind,
	int *out_color_preset)
{
	/* (where a vehicle, weapon or powerup goes back to: forge_origins_reset) */
	if (group_tag == 'vehi' || group_tag == 'weap' || group_tag == 'eqip')
	{
		*out_mesh_kind = _marker_mesh_firefight_spawn;
		*out_color_preset = group_tag == 'vehi' ? _marker_color_blue_team :
			group_tag == 'weap' ? _marker_color_oddball_purple : _marker_color_tele_green;
		return 1;
	}
	/* (an effect spot, a decal, an emitter: where it is, to grab it; Forge's
	mode alone, as the Firefight mesh is) */
	if (group_tag == 'effe' || group_tag == 'deca' || is_forge_particle_emitter_tag(group_tag, tag_path))
	{
		*out_mesh_kind = _marker_mesh_firefight_spawn;
		*out_color_preset = group_tag == 'effe' ? _marker_color_red_team :
			group_tag == 'deca' ? _marker_color_oddball_purple : _marker_color_race_orange;
		return 1;
	}
	/* (a volume's middle, to grab it by: its edges are drawn too) */
	if (group_tag == FORGE_GROUP_VOLUME)
	{
		*out_mesh_kind = _marker_mesh_firefight_spawn;
		*out_color_preset = _marker_color_oddball_purple;
		return 1;
	}
	/* (an AI route's point: its start green, the rest blue) */
	if (group_tag == FORGE_GROUP_AI_ROUTE)
	{
		*out_mesh_kind = _marker_mesh_firefight_spawn;
		*out_color_preset = FORGE_AI_ROUTE_ORDER(team_designator) == 0 ? _marker_color_tele_green :
			_marker_color_blue_team;
		return 1;
	}
	if (group_tag == FORGE_GROUP_AI_SPAWN)
	{
		*out_mesh_kind = _marker_mesh_firefight_spawn;
		/* (the players' teams: ALL PLAYERS green as PLAYER is, RED and BLUE
		their own, FORGE_AI_TEAM_* of forge_engine.h) */
		*out_color_preset = team_designator == 1 || team_designator == 2 ||
			team_designator == FORGE_AI_TEAM_ALL_PLAYERS ? _marker_color_tele_green :
			team_designator == FORGE_AI_TEAM_BLUE ? _marker_color_blue_team :
			team_designator == 3 || team_designator == FORGE_AI_TEAM_RED ? _marker_color_red_team :
			team_designator == 4 ? _marker_color_race_orange : _marker_color_koth_gold;
		return 1;
	}
	if (group_tag == 'ffsp' ||
		netgame_flag_type == FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN ||
		(tag_path && _stricmp(tag_path, "spawn\\firefight_spawn") == 0))
	{
		int ff_type = (team_designator >= 0) ? FORGE_FF_UNPACK_TYPE(team_designator) : 0;
		*out_mesh_kind = _marker_mesh_firefight_spawn;
		if (ff_type == _forge_ff_spawn_player_vehicle)
			*out_color_preset = _marker_color_tele_green;
		else if (ff_type == _forge_ff_spawn_player_spawn)
			*out_color_preset = _marker_color_blue_team;
		else if (ff_drop_point_kinds(ff_type) != 0 || ff_type == FF_ROUTE_SPAWN_TYPE)
			*out_color_preset = _marker_color_koth_gold;
		else if (ff_type >= _forge_ff_spawn_ai_and_vehicles && ff_type <= _forge_ff_spawn_enemy_heavy_veh)
			*out_color_preset = _marker_color_race_orange;
		else
			*out_color_preset = _marker_color_red_team;
		return 1;
	}
	if (group_tag == 'spwn' || (tag_path && strncmp(tag_path, "spawn\\", 6) == 0))
	{
		*out_mesh_kind = _marker_mesh_player_spawn;
		if (team_designator == 1 || (tag_path && strstr(tag_path, "blue")))
			*out_color_preset = _marker_color_blue_team;
		else if (team_designator == 2 || (tag_path && strstr(tag_path, "ffa")))
			*out_color_preset = _marker_color_ffa_yellow;
		else
			*out_color_preset = _marker_color_red_team;
		return 1;
	}
	if (group_tag == 'tele' || (tag_path && strncmp(tag_path, "teleporter\\", 11) == 0))
	{
		int is_sender = (netgame_flag_type == 6 /* _netgame_flag_teleporter_source */ ||
			(tag_path && (strstr(tag_path, "origin") || strstr(tag_path, "sender"))));
		*out_mesh_kind = is_sender ? _marker_mesh_tele_origin : _marker_mesh_tele_dest;
		*out_color_preset = _marker_color_tele_green;
		return 1;
	}
	if (group_tag == 'flag' || (tag_path && strncmp(tag_path, "objective\\", 10) == 0))
	{
		if (netgame_flag_type == FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR ||
			(tag_path && strstr(tag_path, "firefight_generator")))
		{
			*out_mesh_kind = _marker_mesh_firefight_spawn;
			*out_color_preset = _marker_color_blue_team;
			return 1;
		}
		if (netgame_flag_type == 2 /* _netgame_flag_oddball_ball_spawn */ ||
			(tag_path && strstr(tag_path, "oddball")))
		{
			*out_mesh_kind = _marker_mesh_oddball;
			*out_color_preset = _marker_color_oddball_purple;
			return 1;
		}
		if (netgame_flag_type == 3 /* _netgame_flag_race_track */ ||
			netgame_flag_type == 4 /* _netgame_flag_race_vehicle */ ||
			(tag_path && strstr(tag_path, "race")))
		{
			*out_mesh_kind = _marker_mesh_race;
			*out_color_preset = _marker_color_race_orange;
			return 1;
		}
		if (netgame_flag_type == 0 /* _netgame_flag_ctf_flag */ ||
			netgame_flag_type == 1 /* _netgame_flag_ctf_vehicle */ ||
			(tag_path && strstr(tag_path, "ctf")))
		{
			*out_mesh_kind = _marker_mesh_ctf_flag;
			*out_color_preset = (team_designator == 1 || (tag_path && strstr(tag_path, "blue")))
				? _marker_color_blue_team
				: _marker_color_red_team;
			return 1;
		}
		*out_mesh_kind = _marker_mesh_koth;
		*out_color_preset = _marker_color_koth_gold;
		return 1;
	}
	return 0;
}

static void draw_translucent_beam_3d(
	union real_point3d const *p0,
	union real_point3d const *p1,
	int color_preset,
	float thickness)
{
	struct rasterizer_model_begin_parameters model_parameters;
	union real_rgb_color change_colors[4];
	real function_values[4];
	struct real_matrix4x3 node_matrix;
	long col_shdr_idx;
	struct shader *col_shdr_def;
	float dx, dy, dz, len_xy, len_3d;
	int p;

	if (!s_marker_meshes.meshes[_marker_mesh_koth_beam].valid ||
		s_marker_meshes.meshes[_marker_mesh_koth_beam].part_count <= 0)
		return;

	dx = p1->x - p0->x;
	dy = p1->y - p0->y;
	dz = p1->z - p0->z;
	len_xy = square_root(dx * dx + dy * dy);
	len_3d = square_root(dx * dx + dy * dy + dz * dz);
	if (len_3d < 0.01f)
		return;

	col_shdr_idx = get_or_create_translucent_marker_shader(color_preset, 1, 0);
	if (col_shdr_idx == NONE)
		return;
	col_shdr_def = shader_definition_get(col_shdr_idx);
	if (!col_shdr_def)
		return;

	/*
	 * Stretch the unit 3D beam (x in [0..1], y in [-0.015..+0.015], z in [0.025..0.055])
	 * along forward = (p1 - p0) while keeping left and up unit-length so cross-section
	 * thickness stays constant regardless of segment length!
	 */
	memset(&node_matrix, 0, sizeof(node_matrix));
	node_matrix.scale = 1.0f;
	node_matrix.position = *p0;
	node_matrix.forward.i = dx;
	node_matrix.forward.j = dy;
	node_matrix.forward.k = dz;
	if (len_xy > 0.001f)
	{
		node_matrix.left.i = -dy / len_xy;
		node_matrix.left.j = dx / len_xy;
		node_matrix.left.k = 0.0f;
	}
	else
	{
		node_matrix.left.i = 0.0f;
		node_matrix.left.j = 1.0f;
		node_matrix.left.k = 0.0f;
	}
	node_matrix.up.i = 0.0f;
	node_matrix.up.j = 0.0f;
	node_matrix.up.k = thickness;
	/* (a vertical beam: its up across it, else it is drawn edge-on) */
	if (len_xy <= 0.001f)
	{
		node_matrix.up.i = thickness;
		node_matrix.up.k = 0.0f;
	}
	node_matrix.left.i *= thickness;
	node_matrix.left.j *= thickness;

	change_colors[0].red = 1.0f;
	change_colors[0].green = 1.0f;
	change_colors[0].blue = 1.0f;
	change_colors[1] = change_colors[0];
	change_colors[2] = change_colors[0];
	change_colors[3] = change_colors[0];

	function_values[0] = 1.0f;
	function_values[1] = 1.0f;
	function_values[2] = 1.0f;
	function_values[3] = 1.0f;

	memset(&model_parameters, 0, sizeof(model_parameters));
	model_parameters.unique_identifier = NONE;
	model_parameters.lighting.ambient_color.red = 0.90f;
	model_parameters.lighting.ambient_color.green = 0.90f;
	model_parameters.lighting.ambient_color.blue = 0.90f;
	model_parameters.lighting.distant_light_count = 1;
	model_parameters.lighting.distant_lights[0].color.red = 0.95f;
	model_parameters.lighting.distant_lights[0].color.green = 0.95f;
	model_parameters.lighting.distant_lights[0].color.blue = 0.95f;
	model_parameters.lighting.distant_lights[0].direction.i = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.j = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.k = -0.77f;
	model_parameters.centroid.x = 0.5f * (p0->x + p1->x);
	model_parameters.centroid.y = 0.5f * (p0->y + p1->y);
	model_parameters.centroid.z = 0.5f * (p0->z + p1->z);
	model_parameters.radius = 0.5f * len_3d + 0.5f;
	model_parameters.animation.colors = change_colors;
	model_parameters.animation.values = function_values;
	model_parameters.skinning.node_matrices = &node_matrix;
	model_parameters.skinning.node_matrix_count = 1;
	model_parameters.geometry_flags = (1u << 2); /* _rasterizer_geometry_no_fog_bit */
	model_parameters.base_map_scale.i = 1.0f;
	model_parameters.base_map_scale.j = 1.0f;

	rasterizer_model_begin(&model_parameters, FALSE);
	for (p = 0; p < s_marker_meshes.meshes[_marker_mesh_koth_beam].part_count; p++)
	{
		rasterizer_model_draw(
			col_shdr_def,
			0,
			&s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].triangle_buffer,
			NONE,
			s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].triangle_buffer.count,
			&s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].vertex_buffer,
			NONE);
	}
	rasterizer_model_end();
}

/* a parallelogram (a corner and its two sides) faintly tinted: the beam's
box flattened onto it. Its x (0 to 1) along side_a, its y (-0.015 to 0.015)
across side_b, its z (0.025 to 0.055) a sliver through it */
static void draw_translucent_face_3d(float const *corner, float const *side_a, float const *side_b, int color_preset,
	int highlighted)
{
	struct rasterizer_model_begin_parameters model_parameters;
	union real_rgb_color change_colors[4];
	real function_values[4];
	struct real_matrix4x3 node_matrix;
	long shader_index;
	struct shader *shader;
	float normal[3], length, sliver;
	float la, lb;
	int p;

	if (!s_marker_meshes.meshes[_marker_mesh_koth_beam].valid ||
		s_marker_meshes.meshes[_marker_mesh_koth_beam].part_count <= 0)
		return;
	normal[0] = side_a[1] * side_b[2] - side_a[2] * side_b[1];
	normal[1] = side_a[2] * side_b[0] - side_a[0] * side_b[2];
	normal[2] = side_a[0] * side_b[1] - side_a[1] * side_b[0];
	length = (float)sqrt((double)(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]));
	if (length < 0.000001f)
		return;
	shader_index = get_or_create_translucent_marker_shader(color_preset, highlighted ? 3 : 2, 0);
	shader = shader_index != NONE ? shader_definition_get(shader_index) : NULL;
	if (!shader)
		return;

	/* (a sliver 0.004 thick: up spans 0.03 of the mesh's z) */
	sliver = 0.004f / 0.03f / length;
	memset(&node_matrix, 0, sizeof(node_matrix));
	node_matrix.scale = 1.0f;
	node_matrix.forward.i = side_a[0];
	node_matrix.forward.j = side_a[1];
	node_matrix.forward.k = side_a[2];
	node_matrix.left.i = side_b[0] / 0.03f;
	node_matrix.left.j = side_b[1] / 0.03f;
	node_matrix.left.k = side_b[2] / 0.03f;
	node_matrix.up.i = normal[0] * sliver;
	node_matrix.up.j = normal[1] * sliver;
	node_matrix.up.k = normal[2] * sliver;
	node_matrix.position.x = corner[0] + side_b[0] * 0.5f - node_matrix.up.i * 0.04f;
	node_matrix.position.y = corner[1] + side_b[1] * 0.5f - node_matrix.up.j * 0.04f;
	node_matrix.position.z = corner[2] + side_b[2] * 0.5f - node_matrix.up.k * 0.04f;

	change_colors[0].red = change_colors[0].green = change_colors[0].blue = 1.0f;
	change_colors[1] = change_colors[2] = change_colors[3] = change_colors[0];
	function_values[0] = function_values[1] = function_values[2] = function_values[3] = 1.0f;

	la = (float)sqrt((double)(side_a[0] * side_a[0] + side_a[1] * side_a[1] + side_a[2] * side_a[2]));
	lb = (float)sqrt((double)(side_b[0] * side_b[0] + side_b[1] * side_b[1] + side_b[2] * side_b[2]));
	memset(&model_parameters, 0, sizeof(model_parameters));
	model_parameters.unique_identifier = NONE;
	model_parameters.lighting.ambient_color.red = 0.90f;
	model_parameters.lighting.ambient_color.green = 0.90f;
	model_parameters.lighting.ambient_color.blue = 0.90f;
	model_parameters.centroid.x = corner[0] + (side_a[0] + side_b[0]) * 0.5f;
	model_parameters.centroid.y = corner[1] + (side_a[1] + side_b[1]) * 0.5f;
	model_parameters.centroid.z = corner[2] + (side_a[2] + side_b[2]) * 0.5f;
	model_parameters.radius = 0.5f * (la + lb) + 0.5f;
	if (!render_frustum_sphere_visible(&render.frustum, (union real_point3d const *)&model_parameters.centroid,
			model_parameters.radius))
	{
		return;
	}
	model_parameters.animation.colors = change_colors;
	model_parameters.animation.values = function_values;
	model_parameters.skinning.node_matrices = &node_matrix;
	model_parameters.skinning.node_matrix_count = 1;
	model_parameters.geometry_flags = (1u << 2); /* _rasterizer_geometry_no_fog_bit */
	model_parameters.base_map_scale.i = 1.0f;
	model_parameters.base_map_scale.j = 1.0f;

	rasterizer_model_begin(&model_parameters, FALSE);
	for (p = 0; p < s_marker_meshes.meshes[_marker_mesh_koth_beam].part_count; p++)
	{
		rasterizer_model_draw(
			shader,
			0,
			&s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].triangle_buffer,
			NONE,
			s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].triangle_buffer.count,
			&s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].vertex_buffer,
			NONE);
	}
	rasterizer_model_end();
}

static void draw_dropship_route_piece(float const *from, float const *to, int kind)
{
	static int const colors[] = {
		_marker_color_koth_gold,  /* approach */
		_marker_color_blue_team,  /* departure */
		_marker_color_red_team,   /* blocked */
		_marker_color_tele_green  /* tail */
	};
	union real_point3d p0, p1;

	p0.x = from[0];
	p0.y = from[1];
	p0.z = from[2];
	p1.x = to[0];
	p1.y = to[1];
	p1.z = to[2];
	/* (a piece out of view: the sphere around it, and the beam's width) */
	{
		union real_point3d middle;
		float dx = p1.x - p0.x, dy = p1.y - p0.y, dz = p1.z - p0.z;

		middle.x = (p0.x + p1.x) * 0.5f;
		middle.y = (p0.y + p1.y) * 0.5f;
		middle.z = (p0.z + p1.z) * 0.5f;
		if (!render_frustum_sphere_visible(&render.frustum, &middle,
				(float)sqrt((double)(dx * dx + dy * dy + dz * dz)) * 0.5f + 3.0f))
		{
			return;
		}
	}
	draw_translucent_beam_3d(&p0, &p1, colors[kind], 3.0f);
}

/* a volume's edge (forge_volumes_render): red where it kills, gold where it
hurts (and ONE WAY), grey-blue where it does nothing, purple where it pulls
(GRAVITY, and its arrow), green round an AI PATROL, and thin blue round its
PURSUIT RANGE; thicker for the one held or pointed at */
static int const s_volume_line_colors[] = { _marker_color_koth_gold, _marker_color_red_team, _marker_color_blue_team,
	_marker_color_oddball_purple, _marker_color_koth_gold, _marker_color_tele_green, _marker_color_blue_team };
#define VOLUME_LINE_COLOR_PURSUIT 6

static void draw_volume_line(float const *from, float const *to, int color, int highlighted)
{
	int const count = (int)(sizeof(s_volume_line_colors) / sizeof(s_volume_line_colors[0]));
	union real_point3d p0, p1, middle;
	float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];

	p0.x = from[0];
	p0.y = from[1];
	p0.z = from[2];
	p1.x = to[0];
	p1.y = to[1];
	p1.z = to[2];
	middle.x = (p0.x + p1.x) * 0.5f;
	middle.y = (p0.y + p1.y) * 0.5f;
	middle.z = (p0.z + p1.z) * 0.5f;
	if (!render_frustum_sphere_visible(&render.frustum, &middle,
			(float)sqrt((double)(dx * dx + dy * dy + dz * dz)) * 0.5f + 1.0f))
	{
		return;
	}
	draw_translucent_beam_3d(&p0, &p1, s_volume_line_colors[color >= 0 && color < count ? color : 0],
		color == VOLUME_LINE_COLOR_PURSUIT ? 0.75f : highlighted ? 2.5f : 1.25f);
}

/* a volume's face, faintly tinted in its edges' color */
static void draw_volume_face(float const *corner, float const *side_a, float const *side_b, int color, int highlighted)
{
	int const count = (int)(sizeof(s_volume_line_colors) / sizeof(s_volume_line_colors[0]));

	draw_translucent_face_3d(corner, side_a, side_b, s_volume_line_colors[color >= 0 && color < count ? color : 0],
		highlighted);
}

/* an AI route's piece (forge_ai_routes_render): green on its way, blue on a
LOOP's way back; thicker for the route held or pointed at */
static void draw_ai_route_line(float const *from, float const *to, int color, int highlighted)
{
	union real_point3d p0, p1, middle;
	float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];

	p0.x = from[0];
	p0.y = from[1];
	p0.z = from[2];
	p1.x = to[0];
	p1.y = to[1];
	p1.z = to[2];
	middle.x = (p0.x + p1.x) * 0.5f;
	middle.y = (p0.y + p1.y) * 0.5f;
	middle.z = (p0.z + p1.z) * 0.5f;
	if (!render_frustum_sphere_visible(&render.frustum, &middle,
			(float)sqrt((double)(dx * dx + dy * dy + dz * dz)) * 0.5f + 1.0f))
	{
		return;
	}
	/* AE: a piece of the line in view is noted in debug.txt, at most every two seconds, so that a staged scene can tell that the line draws
	(the original has no such line) */
	{
		static unsigned long s_noted_at;
		unsigned long now = system_milliseconds();

		if (!s_noted_at || now - s_noted_at >= 2000u)
		{
			s_noted_at = now ? now : 1u;
			platform_log("forge: an AI route line is drawn (a piece %.1f long)", (float)sqrt((double)(dx * dx + dy * dy + dz * dz)));
		}
	}
	draw_translucent_beam_3d(&p0, &p1, color == 1 ? _marker_color_blue_team : _marker_color_tele_green,
		highlighted ? 2.5f : 1.5f);
}

/* an AI spawn's line to its goal (forge_ai_goal_links_render): gold */
static void draw_ai_goal_link(float const *from, float const *to)
{
	union real_point3d p0, p1;

	p0.x = from[0];
	p0.y = from[1];
	p0.z = from[2];
	p1.x = to[0];
	p1.y = to[1];
	p1.z = to[2];
	draw_translucent_beam_3d(&p0, &p1, _marker_color_koth_gold, 2.0f);
}

/* the model of a Firefight generator, as a match shows it
(firefight_generators.c's find_generator_scenery_tag): the map's, or read from
Forge's pack, as it is not a multiplayer map's. NONE: the marker's mesh shows */
static long forge_generator_model_tag(void)
{
	return forge_resolve_engine_tag('scen', "scenery\\c_uplink\\c_uplink", NULL);
}

static void render_world_objective_and_spawn_markers(void)
{
	int i, hill_id;

	ensure_objective_marker_meshes_loaded();

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		int mesh_kind = 0, color_preset = 0;
		int is_hi;
		float dx, dy, dz, d2;
		union real_point3d pos;
		union real_vector3d fwd, up;

		if (!inst->active)
			continue;
		if (inst->group_tag != 'spwn' && inst->group_tag != 'tele' && inst->group_tag != 'flag' && inst->group_tag != 'ffsp' &&
			inst->group_tag != FORGE_GROUP_AI_SPAWN && inst->group_tag != FORGE_GROUP_VOLUME &&
			inst->group_tag != FORGE_GROUP_AI_ROUTE &&
			inst->group_tag != 'vehi' && inst->group_tag != 'weap' &&
			inst->group_tag != 'eqip' && inst->group_tag != 'effe' && inst->group_tag != 'deca' &&
			!is_forge_particle_emitter_tag(inst->group_tag, inst->tag_path))
			continue;
		if (!resolve_marker_kind_and_color(
				inst->group_tag,
				inst->tag_path,
				inst->netgame_flag_type,
				inst->team_designator,
				&mesh_kind,
				&color_preset))
			continue;
		if (mesh_kind < 0 || mesh_kind >= NUMBER_OF_MARKER_MESH_KINDS ||
			!s_marker_meshes.meshes[mesh_kind].valid)
			continue;

		/* Player spawns, Firefight spawns, KOTH, Oddball, and Race markers are only visible while Forge Mode is active */
		if (g_forge.mode != _forge_mode_3d_monitor &&
			(mesh_kind == _marker_mesh_player_spawn ||
			 mesh_kind == _marker_mesh_firefight_spawn ||
			 mesh_kind == _marker_mesh_koth ||
			 mesh_kind == _marker_mesh_oddball ||
			 mesh_kind == _marker_mesh_race))
		{
			continue;
		}

		if (mesh_kind == _marker_mesh_koth)
		{
			/* KOTH markers only highlight when actively clicked/grabbed (or during 10-node placement), never on hover/proximity */
			is_hi = (i == g_forge.grabbed_live_index) ||
					(g_forge.koth_group_placing && inst->team_designator == g_forge.koth_group_hill_id);
		}
		else
		{
			is_hi = (i == g_forge.grabbed_live_index || i == g_forge.hovered_live_index);
		}

		dx = inst->x - render.camera.position.x;
		dy = inst->y - render.camera.position.y;
		dz = inst->z - render.camera.position.z;
		d2 = dx * dx + dy * dy + dz * dz;
		if (d2 > 300.0f * 300.0f && !is_hi)
			continue;

		pos.x = inst->x;
		pos.y = inst->y;
		pos.z = inst->z;
		/* (only what this view shows: the engine culls its objects, but these
		are drawn here, up to 70000 triangles each. The radius is the mesh's
		about its middle, and pos may be its foot, so a margin) */
		if (!is_hi)
		{
			float cull_radius = s_marker_meshes.meshes[mesh_kind].radius * 1.5f +
				(inst->netgame_flag_type == FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR ? 2.5f : 0.5f);

			if (!render_frustum_sphere_visible(&render.frustum, &pos, cull_radius) ||
				render_frustum_sphere_diameter_in_pixels(&render.frustum, &pos, cull_radius) < 2.0f)
			{
				continue;
			}
		}
		fwd.i = (float)cosine(inst->yaw);
		fwd.j = (float)sine(inst->yaw);
		fwd.k = 0.0f;
		up.i = 0.0f;
		up.j = 0.0f;
		up.k = 1.0f;

		/* (a generator as it will stand, its marker over it alone while it is
		pointed at or held) */
		if (inst->netgame_flag_type == FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR)
		{
			long generator_model = forge_generator_model_tag();

			if (generator_model != NONE)
			{
				forge_model_look_use(generator_model, NULL);
				draw_native_object_model_at(generator_model, NONE, NULL, &pos, &fwd, &up, 0, 0, 0);
				if (!is_hi)
					continue;
			}
		}

		draw_translucent_marker_hw_parts(
			&pos,
			&fwd,
			&up,
			s_marker_meshes.meshes[mesh_kind].radius,
			1.0f,
			s_marker_meshes.meshes[mesh_kind].parts,
			s_marker_meshes.meshes[mesh_kind].part_count,
			color_preset,
			is_hi,
			0);
	}

	/* Render 3D translucent KOTH boundary beams ONLY in Forge mode when a node of that hill is actually clicked/grabbed
	 * (or actively being placed as a 10-point hill), connecting the exact 3D marker positions. */
	if (g_forge.mode != _forge_mode_3d_monitor)
		return;

	forge_3d_dropship_routes_render(draw_dropship_route_piece);
	forge_volumes_render(draw_volume_line, draw_volume_face);
	forge_ai_routes_render(draw_ai_route_line);
	forge_ai_goal_links_render(draw_ai_goal_link);

	/* An AI spawn's other units, where they will start: models, not objects, so
	nothing can pick them up and they cost none of the level's objects */
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		int units, unit;
		long unit_definition;
		union real_vector3d fwd, up;

		union real_point3d hover;
		int hovers;

		if (!inst->active || inst->group_tag != FORGE_GROUP_AI_SPAWN)
			continue;
		units = forge_ai_spawn_grid_count(i);
		/* (a flyer with a HEIGHT: every unit where it will fly, the first
		too, whose model stands on the marker) */
		hovers = forge_ai_spawn_hover_point(i, 0, &hover);
		if (units < 2 && !hovers)
			continue;
		unit_definition = forge_ai_actor_unit_definition(inst->tag_path);
		if (unit_definition == NONE)
			continue;
		fwd.i = (float)cosine(inst->yaw);
		fwd.j = (float)sine(inst->yaw);
		fwd.k = 0.0f;
		up.i = 0.0f;
		up.j = 0.0f;
		up.k = 1.0f;
		for (unit = hovers ? 0 : 1; unit < (units > 1 ? units : 1); unit++)
		{
			union real_point3d point, start;

			if (unit == 0)
			{
				start.x = inst->x;
				start.y = inst->y;
				start.z = inst->z;
			}
			else if (!forge_ai_spawn_grid_point(i, unit, &start))
				continue;
			point = start;
			if (hovers && forge_ai_spawn_hover_point(i, unit, &point))
			{
				float from[3], to[3];

				/* (the line from where it stands up to its height, gold) */
				from[0] = start.x;
				from[1] = start.y;
				from[2] = start.z + 0.1f;
				to[0] = point.x;
				to[1] = point.y;
				to[2] = point.z;
				draw_volume_line(from, to, 0, i == g_forge.grabbed_live_index);
			}
			if (render_frustum_sphere_visible(&render.frustum, &point, 2.0f))
			{
				forge_model_look_use(unit_definition, inst->tag_path);
				draw_native_object_model_at(unit_definition, NONE, NULL, &point, &fwd, &up, 0, 0, 0);
			}
		}
	}

	for (hill_id = 0; hill_id < 16; hill_id++)
	{
		int node_indices[32];
		float node_angles[32];
		int ncount = 0;
		int hill_grabbed = (g_forge.koth_group_placing && g_forge.koth_group_hill_id == hill_id) ? 1 : 0;
		float cx_h = 0.0f, cy_h = 0.0f;
		int a, b;

		for (i = 0; i < g_forge.live_instance_count && ncount < 32; i++)
		{
			struct forge_live_instance const *inst = &g_forge.live_instances[i];
			if (!inst->active || inst->group_tag != 'flag')
				continue;
			if (inst->netgame_flag_type != 8 /* _netgame_flag_hill */ && strstr(inst->tag_path, "koth") == NULL)
				continue;
			if (inst->team_designator != hill_id)
				continue;
			node_indices[ncount++] = i;
			cx_h += inst->x;
			cy_h += inst->y;
			if (i == g_forge.grabbed_live_index)
				hill_grabbed = 1;
		}

		if (ncount >= 2 && hill_grabbed)
		{
			cx_h /= (float)ncount;
			cy_h /= (float)ncount;

			for (a = 0; a < ncount; a++)
			{
				struct forge_live_instance const *na = &g_forge.live_instances[node_indices[a]];
				node_angles[a] = (float)atan2((double)(na->y - cy_h), (double)(na->x - cx_h));
			}
			for (a = 0; a < ncount - 1; a++)
			{
				for (b = a + 1; b < ncount; b++)
				{
					if (node_angles[b] < node_angles[a])
					{
						float ta = node_angles[a];
						int ti = node_indices[a];
						node_angles[a] = node_angles[b];
						node_indices[a] = node_indices[b];
						node_angles[b] = ta;
						node_indices[b] = ti;
					}
				}
			}

			for (a = 0; a < ncount; a++)
			{
				int nb = (a + 1) % ncount;
				struct forge_live_instance const *inst0 = &g_forge.live_instances[node_indices[a]];
				struct forge_live_instance const *inst1 = &g_forge.live_instances[node_indices[nb]];
				union real_point3d p0, p1;
				p0.x = inst0->x;
				p0.y = inst0->y;
				p0.z = inst0->z;
				p1.x = inst1->x;
				p1.y = inst1->y;
				p1.z = inst1->z;
				draw_translucent_beam_3d(&p0, &p1, _marker_color_koth_gold, 1.0f);
			}
		}
	}
}

/* how long RANDOM's preview shows each rank */
#define FORGE_RANDOM_PREVIEW_MILLISECONDS 1200u

static void render_spawn_palette_3d_preview(long object_index, void const *lighting_ptr)
{
	int i, match_count = 0, chosen_idx = -1;
	struct forge_catalog_entry const *entry;
	union real_point3d preview_pos;
	union real_vector3d preview_fwd, preview_up, cam_right;
	float dist, angle;
	int marker_kind = -1, marker_color = 0;
	uint32_t actual_grp = 0;
	uint32_t load_group;
	long obj_def_idx;
	int generator;

	if (g_forge.active_modal != _forge_modal_spawn_palette || g_forge.h3_menu_stage != 1)
		return;

	{
		extern int forge_palette_filtered_indices(int *out_indices, int max_out);
		int filtered[512];
		int count = forge_palette_filtered_indices(filtered, 512);

		if (g_forge.palette_cursor >= 0 && g_forge.palette_cursor < count)
			chosen_idx = filtered[g_forge.palette_cursor];
		(void)match_count;
		/* (a group's row is no tag to read) */
		if (chosen_idx >= 0 && (g_forge.catalog[chosen_idx].group_tag == FORGE_GROUP_AI_MENU ||
			g_forge.catalog[chosen_idx].group_tag == FORGE_GROUP_FOLDER ||
			g_forge.catalog[chosen_idx].group_tag == FORGE_GROUP_FF_MENU ||
			g_forge.catalog[chosen_idx].group_tag == FORGE_GROUP_VOLUME ||
			g_forge.catalog[chosen_idx].group_tag == FORGE_GROUP_AI_ROUTE))
		{
			chosen_idx = -1;
		}
		/* (RANDOM shows each of its group's ranks in turn) */
		if (chosen_idx >= 0 && forge_ai_row_is_random(&g_forge.catalog[chosen_idx]))
		{
			int group = forge_ai_row_group(&g_forge.catalog[chosen_idx]);
			int ranks[32];
			int rank_count = 0;

			for (i = 0; i < g_forge.catalog_count && rank_count < (int)NUMBEROF(ranks); i++)
			{
				struct forge_catalog_entry const *row = &g_forge.catalog[i];

				if (row->group_tag == FORGE_GROUP_AI_SPAWN && !forge_ai_row_is_random(row) &&
					forge_ai_row_group(row) == group && forge_catalog_entry_allowed(row))
				{
					ranks[rank_count++] = i;
				}
			}
			chosen_idx = rank_count > 0 ?
				ranks[(system_milliseconds() / FORGE_RANDOM_PREVIEW_MILLISECONDS) % (unsigned long)rank_count] : -1;
		}
	}
	if (chosen_idx < 0)
		return;

	entry = &g_forge.catalog[chosen_idx];
	/* (an effect, an emitter, a decal: forge_3d_mode.c's live preview shows
	the real one in the world) */
	if (entry->category == _forge_cat_effects || entry->category == _forge_cat_decals)
		return;

	/* An actor shows as the unit it is, read as any other palette tag is */
	load_group = entry->group_tag == FORGE_GROUP_AI_SPAWN ? 'actv' : entry->group_tag;
	/* (and a generator as its model: forge_generator_model_tag) */
	generator = strstr(entry->tag_path, "firefight_generator") != NULL;

	/* Check if this catalog entry is one of our 3D Translucent Spawn/Teleporter/Objective Markers!
	(A vehicle's, a weapon's or a powerup's marker is where it goes back to in
	the world: here it is shown as itself) */
	if (load_group != 'actv' && load_group != 'vehi' && load_group != 'weap' && load_group != 'eqip' && !generator &&
		resolve_marker_kind_and_color(entry->group_tag, entry->tag_path, -1, -1, &marker_kind, &marker_color))
	{
		ensure_objective_marker_meshes_loaded();
		if (marker_kind >= 0 && marker_kind < NUMBER_OF_MARKER_MESH_KINDS &&
			s_marker_meshes.meshes[marker_kind].valid)
		{
			dist = 1.45f;
			cross_product3d(&render.camera.forward, &render.camera.up, &cam_right);
			preview_pos.x = render.camera.position.x + render.camera.forward.i * dist + cam_right.i * 0.42f - render.camera.up.i * 0.18f;
			preview_pos.y = render.camera.position.y + render.camera.forward.j * dist + cam_right.j * 0.42f - render.camera.up.j * 0.18f;
			preview_pos.z = render.camera.position.z + render.camera.forward.k * dist + cam_right.k * 0.42f - render.camera.up.k * 0.18f;

			angle = (float)(system_milliseconds() % 6000u) * (6.2831853f / 6000.0f);
			preview_fwd.i = (float)cosine(angle);
			preview_fwd.j = (float)sine(angle);
			preview_fwd.k = 0.0f;
			preview_up.i = 0.0f;
			preview_up.j = 0.0f;
			preview_up.k = 1.0f;

			draw_translucent_marker_hw_parts(
				&preview_pos,
				&preview_fwd,
				&preview_up,
				s_marker_meshes.meshes[marker_kind].radius,
				1.15f,
				s_marker_meshes.meshes[marker_kind].parts,
				s_marker_meshes.meshes[marker_kind].part_count,
				marker_color,
				1,
				1);
			return;
		}
	}

	/* Debounce disk-backed runtime tag loading by 200 ms so rapid scrolling in the Spawn Palette never freezes */
	{
		static uint32_t s_preview_debounce_group = 0;
		static char s_preview_debounce_path[256] = "";
		static unsigned long s_preview_debounce_ms = 0;
		int already_in_ram = 0;
		int di;

		for (di = 0; di < s_dyn_tags.count; di++)
		{
			if (s_dyn_tags.entries[di].group_tag == load_group &&
				_stricmp(s_dyn_tags.entries[di].tag_path, entry->tag_path) == 0)
			{
				already_in_ram = 1;
				break;
			}
		}
		if (!already_in_ram && find_native_or_mp_variant_tag(load_group, entry->tag_path) != NONE)
			already_in_ram = 1;

		if (!already_in_ram)
		{
			unsigned long now_ms = system_milliseconds();
			if (s_preview_debounce_group != load_group ||
				_stricmp(s_preview_debounce_path, entry->tag_path) != 0)
			{
				s_preview_debounce_group = load_group;
				snprintf(s_preview_debounce_path, sizeof(s_preview_debounce_path), "%s", entry->tag_path);
				s_preview_debounce_ms = now_ms;
				return;
			}
			if (now_ms - s_preview_debounce_ms < 200u)
				return;
		}
	}

	obj_def_idx = generator ? forge_generator_model_tag() :
		load_group == 'actv' ? forge_ai_actor_unit_definition(entry->tag_path) :
		forge_resolve_engine_tag(entry->group_tag, entry->tag_path, &actual_grp);
	if (obj_def_idx == NONE)
		return;

	dist = 1.55f;
	cross_product3d(&render.camera.forward, &render.camera.up, &cam_right);
	preview_pos.x = render.camera.position.x + render.camera.forward.i * dist + cam_right.i * 0.42f - render.camera.up.i * 0.14f;
	preview_pos.y = render.camera.position.y + render.camera.forward.j * dist + cam_right.j * 0.42f - render.camera.up.j * 0.14f;
	preview_pos.z = render.camera.position.z + render.camera.forward.k * dist + cam_right.k * 0.42f - render.camera.up.k * 0.14f;

	angle = (float)(system_milliseconds() % 6000u) * (6.2831853f / 6000.0f);
	preview_fwd.i = (float)cosine(angle);
	preview_fwd.j = (float)sine(angle);
	preview_fwd.k = 0.0f;
	preview_up.i = 0.0f;
	preview_up.j = 0.0f;
	preview_up.k = 1.0f;
	/* (a drop area stands, as it is placed and as its pods land: its model's
	x down, turning about its length; and lower, as it stands up from its
	base) */
	if (entry->group_tag == 'scen' && _stricmp(entry->tag_path, "firefight\\props\\drop_area_pod") == 0)
	{
		preview_up = preview_fwd;
		preview_fwd.i = 0.0f;
		preview_fwd.j = 0.0f;
		preview_fwd.k = -1.0f;
		preview_pos.z -= 0.25f;
	}

	forge_model_look_use(obj_def_idx, load_group == 'actv' ? entry->tag_path : NULL);
	draw_native_object_model_at(
		obj_def_idx,
		object_index,
		lighting_ptr,
		&preview_pos,
		&preview_fwd,
		&preview_up,
		0,
		1,
		0);
}

void forge_render_world_3d_overlays(void const *lighting_ptr)
{
	if (!g_forge_session_enabled || !g_forge.initialized)
		return;
	if (g_forge.mode == _forge_mode_3d_monitor && g_forge.third_person_view && !s_local_monitor_drawn_this_frame)
	{
		long local_unit = player_control_get_unit_index(0);
		if (local_unit != NONE)
			forge_monitor_render_player_unit(local_unit, lighting_ptr, 0);
	}
	s_local_monitor_drawn_this_frame = 0;
	render_world_objective_and_spawn_markers();
	/* (the palette's model on top of everything: drawn in the front sliver of
	the depth range, so nothing of the level is nearer, its own parts still in
	order. The first-person flag does not do it: the weapon is drawn before the
	level, which a stencil keeps off it, and this after) */
	if (g_forge.active_modal == _forge_modal_spawn_palette)
	{
		D3DVIEWPORT8 viewport, front;

		D3DDevice_GetViewport(&viewport);
		front = viewport;
		front.MinZ = 0.0f;
		front.MaxZ = 0.01f;
		D3DDevice_SetViewport(&front);
		render_spawn_palette_3d_preview(NONE, lighting_ptr);
		D3DDevice_SetViewport(&viewport);
	}
}

int forge_monitor_render_player_unit(long object_index, void const *lighting_ptr, unsigned long flags)
{
	struct unit_datum *unit;

	if (!g_forge_session_enabled || object_index == NONE)
		return 0;

	/* Check if this object is a player biped in Forge Monitor mode (per-player in MP!) */
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_biped);
	if (!unit || unit->object.type != _object_type_biped || unit->unit.player_index == NONE)
		return 0;

	if (!forge_is_unit_in_monitor_mode(object_index))
		return 0;

	if (flags & (1u << 1)) /* Skip shadow pass for Monitor */
		return 1;

	if (!s_monitor_resolved || s_monitor_bipd_handle == NONE)
	{
		uint32_t actual_grp = 'bipd';
		s_monitor_resolved = 1;
		s_monitor_bipd_handle = forge_resolve_engine_tag('bipd', "characters\\monitor\\monitor", &actual_grp);
	}
	if (s_monitor_bipd_handle == NONE)
		return 0;

	if (object_index == player_control_get_unit_index(0))
	{
		union real_point3d pos;
		union real_vector3d fwd, up;
		float cy, sy, cp, sp;

		if (!g_forge.third_person_view)
			return 1;
		s_local_monitor_drawn_this_frame = 1;

		/* Place local Monitor at exact live camera anchor (draw_native_object_model_at adds +0.55f to Z) */
		pos.x = g_forge.cam_x;
		pos.y = g_forge.cam_y;
		pos.z = g_forge.cam_z - 0.55f;

		cy = (float)cosine(g_forge.cam_yaw);
		sy = (float)sine(g_forge.cam_yaw);
		cp = (float)cosine(g_forge.cam_pitch);
		sp = (float)sine(g_forge.cam_pitch);
		fwd.i = cy * cp;
		fwd.j = sy * cp;
		fwd.k = sp;
		up.i = -cy * sp;
		up.j = -sy * sp;
		up.k = cp;

		return draw_native_object_model_at(
			s_monitor_bipd_handle,
			NONE,
			lighting_ptr,
			&pos,
			&fwd,
			&up,
			1,
			0,
			flags);
	}

	{
		union real_point3d pos = unit->object.position;
		union real_vector3d fwd = unit->object.forward;
		union real_vector3d up = unit->object.up;
		int p_abs = (int)((uint32_t)unit->unit.player_index & 0xFFFFu);

		if (p_abs >= 0 && p_abs < 128 && g_forge.player_has_monitor_pos[p_abs])
		{
			float rx = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pos[p_abs][0]
				: g_forge.player_monitor_pos[p_abs][0];
			float ry = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pos[p_abs][1]
				: g_forge.player_monitor_pos[p_abs][1];
			float rz = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pos[p_abs][2]
				: g_forge.player_monitor_pos[p_abs][2];
			float ryaw = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_yaw[p_abs]
				: g_forge.player_monitor_yaw[p_abs];
			float rpitch = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pitch[p_abs]
				: g_forge.player_monitor_pitch[p_abs];
			float cy = (float)cosine(ryaw);
			float sy = (float)sine(ryaw);
			float cp = (float)cosine(rpitch);
			float sp = (float)sine(rpitch);

			pos.x = rx;
			pos.y = ry;
			pos.z = rz - 0.55f;
			fwd.i = cy * cp;
			fwd.j = sy * cp;
			fwd.k = sp;
			up.i = -cy * sp;
			up.j = -sy * sp;
			up.k = cp;
		}

		return draw_native_object_model_at(
			s_monitor_bipd_handle,
			NONE,
			lighting_ptr,
			&pos,
			&fwd,
			&up,
			1,
			0,
			flags);
	}
}

/* =========================================================================
 * MANIFEST.BIN AND THE DONOR MAP .OCETAGS EXTRACTOR
 * ========================================================================= */

#define FORGE_MANIFEST_MAGIC 0x464E4D4Fu /* 'OMNF' */

struct forge_manifest_bin_record
{
	uint32_t group_tag;
	uint32_t path_hash;
	uint32_t compressed_size;
	uint8_t is_custom;
	uint8_t category;
	uint16_t pad;
	char tag_path[256];
	char custom_name[64];
	char file_name[128];
};

static uint32_t forge_fnv1a_hash_ci(uint32_t grp, char const *s)
{
	uint32_t h = 2166136261u ^ grp;
	if (s)
	{
		while (*s)
		{
			unsigned char c = (unsigned char)*s++;
			if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
			if (c == '/') c = '\\';
			h ^= (uint32_t)c;
			h *= 16777619u;
		}
	}
	return h;
}

static void forge_make_safe_ocetags_stem(uint32_t grp, char const *tag_path, char *out_stem, size_t out_sz)
{
	char const *leaf = tag_path;
	char const *p;
	char clean[64];
	size_t ci = 0;
	uint32_t h;
	char gstr[5];

	if (!tag_path) tag_path = "tag";
	for (p = tag_path; *p; p++)
	{
		if (*p == '\\' || *p == '/')
			leaf = p + 1;
	}
	for (p = leaf; *p && ci + 1 < sizeof(clean); p++)
	{
		char c = *p;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
			clean[ci++] = c;
		else if (c == ' ')
			clean[ci++] = '_';
	}
	if (ci == 0)
	{
		clean[0] = 't'; clean[1] = 'a'; clean[2] = 'g'; ci = 3;
	}
	clean[ci] = '\0';
	gstr[0] = (char)((grp >> 24) & 0xFF);
	gstr[1] = (char)((grp >> 16) & 0xFF);
	gstr[2] = (char)((grp >> 8) & 0xFF);
	gstr[3] = (char)(grp & 0xFF);
	gstr[4] = '\0';
	for (ci = 0; ci < 4; ci++)
	{
		if (!((gstr[ci] >= 'a' && gstr[ci] <= 'z') || (gstr[ci] >= 'A' && gstr[ci] <= 'Z') || (gstr[ci] >= '0' && gstr[ci] <= '9')))
			gstr[ci] = '_';
	}
	h = forge_fnv1a_hash_ci(grp, tag_path);
	snprintf(out_stem, out_sz, "%s_%s_%08X", gstr, clean, (unsigned int)h);
}

static char const s_b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct forge_str_builder
{
	char *buf;
	size_t len;
	size_t cap;
};

struct forge_donor_map_ctx
{
	FILE *map_fp;
	FILE *bitmaps_fp;
	FILE *sounds_fp;
	int is_xbox_v5;
	uint32_t tag_off;
	uint32_t tag_sz;
	uint32_t tag_base;
	uint32_t inst_off;
	int32_t tag_count;
	uint32_t vtx_file_off;
	uint32_t idx_file_off;
	uint8_t *tag_buf;
};

static void forge_donor_map_close(struct forge_donor_map_ctx *ctx)
{
	if (!ctx) return;
	if (ctx->map_fp) fclose(ctx->map_fp);
	if (ctx->bitmaps_fp) fclose(ctx->bitmaps_fp);
	if (ctx->sounds_fp) fclose(ctx->sounds_fp);
	free(ctx->tag_buf);
	memset(ctx, 0, sizeof(*ctx));
}

static FILE *forge_open_companion_map(char const *donor_map_path, char const *companion_name)
{
	char path[512];
	char const *slash = strrchr(donor_map_path, '\\');
	FILE *fp;
	if (!slash) slash = strrchr(donor_map_path, '/');
	if (slash)
	{
		size_t dlen = (size_t)(slash - donor_map_path);
		if (dlen < sizeof(path) - 32u)
		{
			memcpy(path, donor_map_path, dlen);
			snprintf(path + dlen, sizeof(path) - dlen, "/%s", companion_name);
			fp = fopen(path, "rb");
			if (fp) return fp;
		}
	}
	snprintf(path, sizeof(path), "maps/%s", companion_name);
	fp = fopen(path, "rb");
	if (fp) return fp;
	snprintf(path, sizeof(path), "custom_maps/%s", companion_name);
	fp = fopen(path, "rb");
	if (fp) return fp;
	snprintf(path, sizeof(path), "C:/Program Files (x86)/Microsoft Games/Halo Custom Edition/maps/%s", companion_name);
	fp = fopen(path, "rb");
	if (fp) return fp;
	snprintf(path, sizeof(path), "C:/Program Files/Microsoft Games/Halo Custom Edition/maps/%s", companion_name);
	return fopen(path, "rb");
}

static int forge_donor_map_open(char const *donor_map_path, struct forge_donor_map_ctx *ctx)
{
	char open_path[512];
	uint8_t hdr[2048];
	uint32_t head_magic, map_ver, inst_addr;

	memset(ctx, 0, sizeof(*ctx));
	if (!donor_map_path || !donor_map_path[0])
		return 0;

	if (is_xbox_version5_map_file(donor_map_path))
	{
		if (!ensure_xbox_map_decompressed_cache(donor_map_path, open_path, sizeof(open_path)))
			return 0;
		ctx->is_xbox_v5 = 1;
	}
	else
	{
		snprintf(open_path, sizeof(open_path), "%s", donor_map_path);
		ctx->is_xbox_v5 = 0;
	}

	ctx->map_fp = fopen(open_path, "rb");
	if (!ctx->map_fp)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), ctx->map_fp) != sizeof(hdr))
	{
		forge_donor_map_close(ctx);
		return 0;
	}
	head_magic = *(uint32_t const *)(hdr + 0x00);
	map_ver = *(uint32_t const *)(hdr + 0x04);
	if (head_magic != 0x68656164u)
	{
		forge_donor_map_close(ctx);
		return 0;
	}
	if (map_ver == 5u)
		ctx->is_xbox_v5 = 1;

	ctx->tag_off = *(uint32_t const *)(hdr + 0x10);
	ctx->tag_sz = *(uint32_t const *)(hdr + 0x14);
	if (ctx->tag_off < 2048u || ctx->tag_sz < 64u || ctx->tag_sz > 128u * 1024u * 1024u)
	{
		forge_donor_map_close(ctx);
		return 0;
	}
	ctx->tag_buf = (uint8_t *)malloc(ctx->tag_sz);
	if (!ctx->tag_buf ||
		fseek(ctx->map_fp, (long)ctx->tag_off, SEEK_SET) != 0 ||
		fread(ctx->tag_buf, 1, ctx->tag_sz, ctx->map_fp) != ctx->tag_sz)
	{
		forge_donor_map_close(ctx);
		return 0;
	}

	inst_addr = *(uint32_t const *)(ctx->tag_buf + 0x00);
	ctx->tag_count = *(int32_t const *)(ctx->tag_buf + 0x0C);
	ctx->inst_off = ctx->is_xbox_v5 ? 0x24u : 0x28u;
	ctx->tag_base = inst_addr - ctx->inst_off;
	if (ctx->tag_count <= 0 || ctx->tag_count > 32768 ||
		ctx->inst_off + (uint32_t)ctx->tag_count * 32u > ctx->tag_sz)
	{
		forge_donor_map_close(ctx);
		return 0;
	}

	if (!ctx->is_xbox_v5)
	{
		uint32_t vtx_off = *(uint32_t const *)(ctx->tag_buf + 0x14);
		uint32_t idx_delta = *(uint32_t const *)(ctx->tag_buf + 0x1C);
		ctx->vtx_file_off = vtx_off;
		ctx->idx_file_off = vtx_off + idx_delta;
		ctx->bitmaps_fp = forge_open_companion_map(donor_map_path, "bitmaps.map");
		ctx->sounds_fp = forge_open_companion_map(donor_map_path, "sounds.map");
	}
	return 1;
}

static int forge_donor_find_tag_index(struct forge_donor_map_ctx const *ctx, uint32_t grp, char const *tag_path)
{
	char const *lookup_path = tag_path;
	int32_t i;
	int pass;

	if (!ctx || !tag_path || !tag_path[0])
		return -1;
	if (_stricmp(tag_path, "levels\\a30\\devices\\beam emitter\\beam emitter red") == 0)
		lookup_path = "levels\\a30\\devices\\beam emitter\\beam emitter";

	/* Pass 0: exact group match (with mode/mod2 equivalence).
	 * Pass 1: fallback match across placeable object groups (scen/mach/lifi/ctrl/vehi/weap/eqip/bipd)
	 * in case a stock catalog entry's group_tag differs from the map's primary group_tag. */
	for (pass = 0; pass < 2; pass++)
	{
		for (i = 0; i < ctx->tag_count; i++)
		{
			uint32_t io = ctx->inst_off + (uint32_t)i * 32u;
			uint32_t t_grp = *(uint32_t const *)(ctx->tag_buf + io + 0x00);
			uint32_t name_addr = *(uint32_t const *)(ctx->tag_buf + io + 0x10);
			char const *tname;
			if (pass == 0)
			{
				if (t_grp != grp)
				{
					if (!((grp == 'mode' && t_grp == 'mod2') || (grp == 'mod2' && t_grp == 'mode')))
						continue;
				}
			}
			else
			{
				if (t_grp != 'scen' && t_grp != 'mach' && t_grp != 'lifi' && t_grp != 'ctrl' &&
					t_grp != 'vehi' && t_grp != 'weap' && t_grp != 'eqip' && t_grp != 'bipd' &&
					t_grp != 'actv' && t_grp != 'mod2' && t_grp != 'mode')
					continue;
			}
			if (name_addr < ctx->tag_base || (name_addr - ctx->tag_base) >= ctx->tag_sz)
				continue;
			tname = (char const *)(ctx->tag_buf + (name_addr - ctx->tag_base));
			if (_stricmp(tname, lookup_path) == 0 || (lookup_path != tag_path && _stricmp(tname, tag_path) == 0))
				return (int)i;
		}
	}
	return -1;
}

struct forge_ext_closure_list
{
	int32_t tag_indices[512];
	int count;
};

static void forge_donor_collect_schema_refs(
	struct forge_donor_map_ctx const *ctx,
	struct tag_schema_definition const *def,
	uint32_t elem_off,
	int32_t elem_count,
	struct forge_ext_closure_list *list,
	int depth);

static void forge_donor_add_tag_closure(
	struct forge_donor_map_ctx const *ctx,
	int32_t tag_idx,
	struct forge_ext_closure_list *list,
	int depth)
{
	uint32_t io, grp, data_addr, ext_flag;
	struct tag_schema_group const *schema_grp;
	int i;

	if (tag_idx < 0 || tag_idx >= ctx->tag_count || list->count >= 512 || depth > 24)
		return;
	for (i = 0; i < list->count; i++)
	{
		if (list->tag_indices[i] == tag_idx)
			return;
	}
	io = ctx->inst_off + (uint32_t)tag_idx * 32u;
	grp = *(uint32_t const *)(ctx->tag_buf + io + 0x00);
	data_addr = *(uint32_t const *)(ctx->tag_buf + io + 0x14);
	ext_flag = *(uint32_t const *)(ctx->tag_buf + io + 0x18);
	if (grp == 'scnr' || grp == 'sbsp' || grp == 'matg' || grp == 'ui  ')
		return;
	if (!ctx->is_xbox_v5 && ext_flag != 0)
	{
		/* Include external 'bitm' tags indexed in bitmaps.map so .ocetags bundles have diffuse textures */
		if (grp == 'bitm' && ctx->bitmaps_fp != NULL)
			list->tag_indices[list->count++] = tag_idx;
		return;
	}
	if (data_addr < ctx->tag_base || (data_addr - ctx->tag_base) >= ctx->tag_sz)
		return;

	schema_grp = forge_runtime_schema_group_get(grp);
	if (!schema_grp || !schema_grp->definition || schema_grp->definition->size <= 0)
		return;
	if ((data_addr - ctx->tag_base) + (uint32_t)schema_grp->definition->size > ctx->tag_sz)
		return;

	list->tag_indices[list->count++] = tag_idx;
	forge_donor_collect_schema_refs(
		ctx,
		schema_grp->definition,
		data_addr - ctx->tag_base,
		1,
		list,
		depth + 1);
}

static void forge_donor_collect_schema_refs(
	struct forge_donor_map_ctx const *ctx,
	struct tag_schema_definition const *def,
	uint32_t elem_off,
	int32_t elem_count,
	struct forge_ext_closure_list *list,
	int depth)
{
	int32_t e;
	uint32_t elem_size;
	struct tag_schema_field const *field;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 24)
		return;
	elem_size = (uint32_t)def->size;
	if ((uint64_t)elem_off + (uint64_t)elem_count * elem_size > (uint64_t)ctx->tag_sz)
		return;

	for (e = 0; e < elem_count; e++)
	{
		uint32_t base_off = elem_off + (uint32_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;
			for (k = 0; k < fcount; k++)
			{
				uint32_t f_off = base_off + (uint32_t)field->offset + (uint32_t)k * fstride;
				uint8_t const *f_ptr = ctx->tag_buf + f_off;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_off + (uint32_t)sub_def->size <= base_off + elem_size)
						forge_donor_collect_schema_refs(ctx, sub_def, f_off, 1, list, depth + 1);
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_off + 12u <= base_off + elem_size)
					{
						int32_t cnt = *(int32_t const *)(f_ptr + 0);
						uint32_t addr = *(uint32_t const *)(f_ptr + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (ctx->is_xbox_v5 && child_def->size == 132)
							child_sz = 104u;
						if (cnt > 0 && cnt <= 65536 && addr >= ctx->tag_base &&
							(uint64_t)(addr - ctx->tag_base) + (uint64_t)cnt * child_sz <= (uint64_t)ctx->tag_sz)
						{
							if (!(ctx->is_xbox_v5 && child_def->size == 132))
								forge_donor_collect_schema_refs(ctx, child_def, addr - ctx->tag_base, cnt, list, depth + 1);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_off + 16u <= base_off + elem_size)
					{
						int32_t ref_handle = *(int32_t const *)(f_ptr + 12);
						if (ref_handle != -1 && ref_handle != 0)
						{
							int32_t ref_low = (int32_t)((uint32_t)ref_handle & 0xFFFFu);
							if (ref_low >= 0 && ref_low < ctx->tag_count)
								forge_donor_add_tag_closure(ctx, ref_low, list, depth + 1);
						}
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_off + 4u <= base_off + elem_size)
					{
						int32_t ref_handle = *(int32_t const *)f_ptr;
						if (ref_handle != -1 && ref_handle != 0)
						{
							int32_t ref_low = (int32_t)((uint32_t)ref_handle & 0xFFFFu);
							if (ref_low >= 0 && ref_low < ctx->tag_count)
								forge_donor_add_tag_closure(ctx, ref_low, list, depth + 1);
						}
					}
				}
			}
		}
	}
}

struct forge_flat_buf
{
	uint8_t *data;
	uint32_t size;
	uint32_t cap;
};

static uint32_t ffb_alloc(struct forge_flat_buf *fb, uint32_t bytes)
{
	uint32_t aligned_sz = (fb->size + 3u) & ~3u;
	uint32_t need = aligned_sz + bytes;
	if (need > fb->cap)
	{
		uint32_t ncap = (fb->cap > 0) ? fb->cap : 16384u;
		uint8_t *ndata;
		while (ncap < need)
			ncap *= 2u;
		ndata = (uint8_t *)realloc(fb->data, ncap);
		if (!ndata)
			return 0xFFFFFFFFu;
		memset(ndata + fb->cap, 0, ncap - fb->cap);
		fb->data = ndata;
		fb->cap = ncap;
	}
	if (aligned_sz > fb->size)
		memset(fb->data + fb->size, 0, aligned_sz - fb->size);
	fb->size = need;
	return aligned_sz;
}

static void forge_donor_flatten_schema(
	struct forge_donor_map_ctx const *ctx,
	struct tag_schema_definition const *def,
	uint32_t src_elem_off,
	uint32_t dst_elem_off,
	int32_t elem_count,
	struct forge_flat_buf *fb,
	int depth)
{
	int32_t e;
	uint32_t elem_size;
	struct tag_schema_field const *field;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 24)
		return;
	elem_size = (uint32_t)def->size;

	for (e = 0; e < elem_count; e++)
	{
		uint32_t s_base = src_elem_off + (uint32_t)e * elem_size;
		uint32_t d_base = dst_elem_off + (uint32_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;
			for (k = 0; k < fcount; k++)
			{
				uint32_t f_rel = (uint32_t)field->offset + (uint32_t)k * fstride;
				uint32_t s_foff = s_base + f_rel;
				uint32_t d_foff = d_base + f_rel;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_rel + (uint32_t)sub_def->size <= elem_size)
						forge_donor_flatten_schema(ctx, sub_def, s_foff, d_foff, 1, fb, depth + 1);
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_rel + 12u <= elem_size)
					{
						int32_t cnt = *(int32_t const *)(ctx->tag_buf + s_foff + 0);
						uint32_t addr = *(uint32_t const *)(ctx->tag_buf + s_foff + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (ctx->is_xbox_v5 && child_sz == 132u)
						{
							/* Convert Xbox 104-byte model_geometry_part array to 132-byte custom_edition_model_part array */
							if (cnt > 0 && cnt <= 1024 && addr >= ctx->tag_base &&
								(uint64_t)(addr - ctx->tag_base) + (uint64_t)cnt * 104u <= (uint64_t)ctx->tag_sz)
							{
								uint32_t c_dst = ffb_alloc(fb, (uint32_t)cnt * 132u);
								if (c_dst != 0xFFFFFFFFu)
								{
									int32_t pi;
									for (pi = 0; pi < cnt; pi++)
									{
										uint8_t const *xp = ctx->tag_buf + (addr - ctx->tag_base) + (uint32_t)pi * 104u;
										uint8_t *cp = fb->data + c_dst + (uint32_t)pi * 132u;
										memset(cp, 0, 132u);
										memcpy(cp, xp, 32u);
									}
									*(int32_t *)(fb->data + d_foff + 0) = cnt;
									*(uint32_t *)(fb->data + d_foff + 4) = c_dst;
									*(uint32_t *)(fb->data + d_foff + 8) = 0;
								}
								else
								{
									memset(fb->data + d_foff, 0, 12);
								}
							}
							else
							{
								memset(fb->data + d_foff, 0, 12);
							}
						}
						else if (cnt > 0 && cnt <= 65536 && addr >= ctx->tag_base &&
							(uint64_t)(addr - ctx->tag_base) + (uint64_t)cnt * child_sz <= (uint64_t)ctx->tag_sz)
						{
							uint32_t c_src = addr - ctx->tag_base;
							uint32_t c_dst = ffb_alloc(fb, (uint32_t)cnt * child_sz);
							if (c_dst != 0xFFFFFFFFu)
							{
								memcpy(fb->data + c_dst, ctx->tag_buf + c_src, (size_t)cnt * child_sz);
								*(int32_t *)(fb->data + d_foff + 0) = cnt;
								*(uint32_t *)(fb->data + d_foff + 4) = c_dst;
								*(uint32_t *)(fb->data + d_foff + 8) = 0;
								forge_donor_flatten_schema(ctx, child_def, c_src, c_dst, cnt, fb, depth + 1);
							}
							else
							{
								memset(fb->data + d_foff, 0, 12);
							}
						}
						else
						{
							memset(fb->data + d_foff, 0, 12);
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_rel + 20u <= elem_size)
					{
						int32_t sz = *(int32_t const *)(ctx->tag_buf + s_foff + 0);
						uint32_t ext_flags = *(uint32_t const *)(ctx->tag_buf + s_foff + 4);
						uint32_t addr = *(uint32_t const *)(ctx->tag_buf + s_foff + 12);
						if ((ext_flags & 1u) == 0 && sz > 0 && sz <= 16 * 1024 * 1024 &&
							addr >= ctx->tag_base &&
							(uint64_t)(addr - ctx->tag_base) + (uint64_t)sz <= (uint64_t)ctx->tag_sz)
						{
							uint32_t d_dst = ffb_alloc(fb, (uint32_t)sz);
							if (d_dst != 0xFFFFFFFFu)
							{
								memcpy(fb->data + d_dst, ctx->tag_buf + (addr - ctx->tag_base), (size_t)sz);
								*(int32_t *)(fb->data + d_foff + 0) = sz;
								*(uint32_t *)(fb->data + d_foff + 4) = 0;
								*(uint32_t *)(fb->data + d_foff + 8) = 0;
								*(uint32_t *)(fb->data + d_foff + 12) = d_dst;
								*(uint32_t *)(fb->data + d_foff + 16) = 0;
							}
							else
							{
								memset(fb->data + d_foff, 0, 20);
							}
						}
						else
						{
							memset(fb->data + d_foff, 0, 20);
						}
					}
				}
			}
		}
	}
}

static void forge_decompress_xbox_vertex_to_68b(uint8_t const *cv, uint8_t *uv)
{
	uint32_t c_norm, c_binorm, c_tan;
	int16_t u_s, v_s, n0_b, n1_b;
	uint16_t w0_u;
	float nx, ny, nz;

	memset(uv, 0, 68u);
	memcpy(uv + 0u, cv + 0u, 12u); /* x, y, z */

	/* Decode 11-11-10 normal */
	c_norm = *(uint32_t const *)(cv + 12u);
	nx = (float)((int32_t)(c_norm << 21) >> 21) / 1023.0f;
	ny = (float)((int32_t)(c_norm << 10) >> 21) / 1023.0f;
	nz = (float)((int32_t)c_norm >> 22) / 511.0f;
	*(float *)(uv + 12u) = nx;
	*(float *)(uv + 16u) = ny;
	*(float *)(uv + 20u) = nz;

	c_binorm = *(uint32_t const *)(cv + 16u);
	*(float *)(uv + 24u) = (float)((int32_t)(c_binorm << 21) >> 21) / 1023.0f;
	*(float *)(uv + 28u) = (float)((int32_t)(c_binorm << 10) >> 21) / 1023.0f;
	*(float *)(uv + 32u) = (float)((int32_t)c_binorm >> 22) / 511.0f;

	c_tan = *(uint32_t const *)(cv + 20u);
	*(float *)(uv + 36u) = (float)((int32_t)(c_tan << 21) >> 21) / 1023.0f;
	*(float *)(uv + 40u) = (float)((int32_t)(c_tan << 10) >> 21) / 1023.0f;
	*(float *)(uv + 44u) = (float)((int32_t)c_tan >> 22) / 511.0f;

	u_s = *(int16_t const *)(cv + 24u);
	v_s = *(int16_t const *)(cv + 26u);
	*(float *)(uv + 48u) = (float)u_s / 32767.0f;
	*(float *)(uv + 52u) = (float)v_s / 32767.0f;

	n0_b = (int16_t)((uint8_t)cv[28] / 3u);
	n1_b = (int16_t)((uint8_t)cv[29] / 3u);
	w0_u = *(uint16_t const *)(cv + 30u);
	*(int16_t *)(uv + 56u) = n0_b;
	*(int16_t *)(uv + 58u) = n1_b;
	{
		int16_t w0_s = (int16_t)w0_u;
		float w0_f = (float)w0_s / 32767.0f;
		if (w0_f < 0.0f) w0_f = 0.0f;
		if (w0_f > 1.0f) w0_f = 1.0f;
		*(float *)(uv + 60u) = w0_f;
		*(float *)(uv + 64u) = 1.0f - w0_f;
	}
}

static void forge_fabb_rebase_schema(
	struct tag_schema_definition const *def,
	uint8_t *slice,
	size_t slice_len,
	uint32_t root_off,
	uint32_t elem_rel,
	int32_t elem_count,
	int32_t bundle_tag_count,
	int depth)
{
	int32_t e;
	uint32_t elem_size;
	struct tag_schema_field const *field;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 24)
		return;
	elem_size = (uint32_t)def->size;
	if ((uint64_t)elem_rel + (uint64_t)elem_count * elem_size > (uint64_t)slice_len)
		return;

	for (e = 0; e < elem_count; e++)
	{
		uint32_t base_rel = elem_rel + (uint32_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;
			for (k = 0; k < fcount; k++)
			{
				uint32_t f_rel = base_rel + (uint32_t)field->offset + (uint32_t)k * fstride;
				uint8_t *f_ptr = slice + f_rel;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_rel + (uint32_t)sub_def->size <= base_rel + elem_size)
					{
						forge_fabb_rebase_schema(
							sub_def, slice, slice_len, root_off, f_rel, 1, bundle_tag_count, depth + 1);
					}
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_rel + 12u <= base_rel + elem_size)
					{
						int32_t cnt = *(int32_t const *)(f_ptr + 0);
						uint32_t addr = *(uint32_t const *)(f_ptr + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (cnt > 0 && cnt <= 65536 && addr >= root_off &&
							(uint64_t)(addr - root_off) + (uint64_t)cnt * child_sz <= (uint64_t)slice_len)
						{
							uint32_t c_rel = addr - root_off;
							*(uint32_t *)(f_ptr + 4) = c_rel;
							*(uint32_t *)(f_ptr + 8) = 0;
							forge_fabb_rebase_schema(
								child_def, slice, slice_len, root_off, c_rel, cnt, bundle_tag_count, depth + 1);
						}
						else
						{
							memset(f_ptr, 0, 12);
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_rel + 20u <= base_rel + elem_size)
					{
						int32_t sz = *(int32_t const *)(f_ptr + 0);
						uint32_t addr = *(uint32_t const *)(f_ptr + 12);
						if (sz > 0 && sz <= 16 * 1024 * 1024 && addr >= root_off &&
							(uint64_t)(addr - root_off) + (uint64_t)sz <= (uint64_t)slice_len)
						{
							*(uint32_t *)(f_ptr + 4) = 0;
							*(uint32_t *)(f_ptr + 8) = 0;
							*(uint32_t *)(f_ptr + 12) = addr - root_off;
							*(uint32_t *)(f_ptr + 16) = 0;
						}
						else
						{
							memset(f_ptr, 0, 20);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_rel + 16u <= base_rel + elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(f_ptr + 12);
						if (ref_idx >= 0 && ref_idx < bundle_tag_count)
							*(int32_t *)(f_ptr + 12) = ref_idx + 1;
						else
							*(int32_t *)(f_ptr + 12) = -1;
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_rel + 4u <= base_rel + elem_size)
					{
						int32_t ref_idx = *(int32_t const *)f_ptr;
						if (ref_idx >= 0 && ref_idx < bundle_tag_count)
							*(int32_t *)f_ptr = ref_idx + 1;
						else
							*(int32_t *)f_ptr = -1;
					}
				}
			}
		}
	}
}

/* =========================================================================
 * SNAP POINTS: PER TAG, AND THE BOX OF THE TAG'S MODEL THEIR PRESETS MEASURE
 * ========================================================================= */

/* the box of the model of the tag being given snap points: the Tag Injector's
preview's (forge_snap_points_bounds_set) or a stock tag's, read from its map
(forge_snap_points_read_stock_bounds) */
static float s_snap_bounds_min[3] = { -0.5f, -0.5f, -0.5f };
static float s_snap_bounds_max[3] = { 0.5f, 0.5f, 0.5f };

struct forge_snap_db_state
{
	int loaded;
	int count;
	struct forge_tag_snap_config configs[FORGE_MAX_SNAP_TAG_CONFIGS];
};
FORGE_HEAP_STATIC(struct forge_snap_db_state, s_snap_db)
#define s_snap_db FORGE_HEAP(s_snap_db)

void forge_snap_points_fill_normal_from_dir(struct forge_snap_point *pt, int normal_dir)
{
	if (!pt)
		return;
	if (normal_dir < 0 || normal_dir >= NUMBER_OF_FORGE_SNAP_DIRS)
		normal_dir = _forge_snap_dir_front;
	pt->normal_dir = (uint8_t)normal_dir;
	pt->nx = 0.0f;
	pt->ny = 0.0f;
	pt->nz = 0.0f;
	switch (normal_dir)
	{
	case _forge_snap_dir_front: pt->nx =  1.0f; break;
	case _forge_snap_dir_right: pt->ny = -1.0f; break;
	case _forge_snap_dir_back:  pt->nx = -1.0f; break;
	case _forge_snap_dir_left:  pt->ny =  1.0f; break;
	case _forge_snap_dir_up:    pt->nz =  1.0f; break;
	case _forge_snap_dir_down:  pt->nz = -1.0f; break;
	default:                    pt->nx =  1.0f; break;
	}
}

void forge_snap_points_load_all(void)
{
	static char const *const s_paths[] = {
		"Faber_data/cached_tags/snap_points.txt",
		"OpenCE Forge/cached_tags/snap_points.txt",
		"Faber_data/custom_tags/snap_points.txt"
	};
	FILE *fp = NULL;
	char line[512];
	int p;

	memset(&s_snap_db, 0, sizeof(s_snap_db));
	s_snap_db.loaded = 1;

	for (p = 0; p < 3; p++)
	{
		fp = fopen(s_paths[p], "r");
		if (fp)
			break;
	}
	if (!fp)
		return;

	while (fgets(line, sizeof(line), fp))
	{
		char *nl;
		unsigned int grp_u = 0;
		char tpath[256];
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float nx = 1.0f, ny = 0.0f, nz = 0.0f;
		int ndir = 0, preset = 0;
		struct forge_tag_snap_config *cfg;
		struct forge_snap_point *pt;

		nl = strchr(line, '\r'); if (nl) *nl = '\0';
		nl = strchr(line, '\n'); if (nl) *nl = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;

		tpath[0] = '\0';
		if (sscanf(line, "%x|%255[^|]|%f|%f|%f|%f|%f|%f|%d|%d",
				&grp_u, tpath, &x, &y, &z, &nx, &ny, &nz, &ndir, &preset) < 8)
		{
			continue;
		}
		if (!tpath[0])
			continue;

		cfg = forge_snap_points_get_for_tag((uint32_t)grp_u, tpath, 1);
		if (!cfg || cfg->point_count >= FORGE_MAX_SNAP_POINTS_PER_TAG)
			continue;

		pt = &cfg->points[cfg->point_count++];
		memset(pt, 0, sizeof(*pt));
		pt->x = x;
		pt->y = y;
		pt->z = z;
		pt->nx = nx;
		pt->ny = ny;
		pt->nz = nz;
		pt->normal_dir = (uint8_t)((ndir >= 0 && ndir < NUMBER_OF_FORGE_SNAP_DIRS) ? ndir : 0);
		pt->preset_id = (uint8_t)((preset >= 0 && preset < NUMBER_OF_FORGE_SNAP_PRESETS) ? preset : 0);
	}
	fclose(fp);
}

int forge_snap_points_save_all(void)
{
	FILE *fp;
	int i, p;

#if FORGE_HOST_FILES
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);
#endif
	fp = fopen("Faber_data/cached_tags/snap_points.txt", "wb");
	if (!fp)
		return 0;

	fprintf(fp, "# OpenCE : Faber - 3D Tag Snap Points Configuration\n");
	fprintf(fp, "# Format: group_hex|tag_path|x|y|z|nx|ny|nz|normal_dir|preset_id\n");
	for (i = 0; i < s_snap_db.count; i++)
	{
		struct forge_tag_snap_config const *cfg = &s_snap_db.configs[i];
		if (!cfg->tag_path[0] || cfg->point_count <= 0)
			continue;
		for (p = 0; p < cfg->point_count && p < FORGE_MAX_SNAP_POINTS_PER_TAG; p++)
		{
			struct forge_snap_point const *pt = &cfg->points[p];
			fprintf(fp, "%08X|%s|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%d|%d\n",
				(unsigned int)cfg->group_tag,
				cfg->tag_path,
				(double)pt->x, (double)pt->y, (double)pt->z,
				(double)pt->nx, (double)pt->ny, (double)pt->nz,
				(int)pt->normal_dir,
				(int)pt->preset_id);
		}
	}
	fclose(fp);
	return 1;
}

struct forge_tag_snap_config *forge_snap_points_get_for_tag(uint32_t group_tag, char const *tag_path, int create_if_missing)
{
	int i;
	if (!tag_path || !tag_path[0])
		return NULL;
	if (!s_snap_db.loaded)
		forge_snap_points_load_all();

	for (i = 0; i < s_snap_db.count; i++)
	{
		if (s_snap_db.configs[i].group_tag == group_tag &&
			_stricmp(s_snap_db.configs[i].tag_path, tag_path) == 0)
		{
			return &s_snap_db.configs[i];
		}
	}
	/* Fallback match by tag_path if group_tag differs between scen/mach/lifi */
	for (i = 0; i < s_snap_db.count; i++)
	{
		if (_stricmp(s_snap_db.configs[i].tag_path, tag_path) == 0)
			return &s_snap_db.configs[i];
	}
	if (!create_if_missing || s_snap_db.count >= FORGE_MAX_SNAP_TAG_CONFIGS)
		return NULL;

	i = s_snap_db.count++;
	memset(&s_snap_db.configs[i], 0, sizeof(s_snap_db.configs[i]));
	s_snap_db.configs[i].group_tag = group_tag;
	snprintf(s_snap_db.configs[i].tag_path, sizeof(s_snap_db.configs[i].tag_path), "%s", tag_path);
	return &s_snap_db.configs[i];
}

int forge_snap_points_count_for_tag(uint32_t group_tag, char const *tag_path)
{
	struct forge_tag_snap_config const *cfg = forge_snap_points_get_for_tag(group_tag, tag_path, 0);
	return cfg ? cfg->point_count : 0;
}

void forge_snap_points_apply_preset_to_point(struct forge_snap_point *pt, int preset_id, int normal_dir)
{
	float min_x = s_snap_bounds_min[0], max_x = s_snap_bounds_max[0];
	float min_y = s_snap_bounds_min[1], max_y = s_snap_bounds_max[1];
	float min_z = s_snap_bounds_min[2], max_z = s_snap_bounds_max[2];
	float cx = 0.5f * (min_x + max_x);
	float cy = 0.5f * (min_y + max_y);
	float cz = 0.5f * (min_z + max_z);
	int auto_dir = _forge_snap_dir_front;

	if (!pt)
		return;
	if (preset_id < 0 || preset_id >= NUMBER_OF_FORGE_SNAP_PRESETS)
		preset_id = _forge_snap_preset_front_center;

	pt->preset_id = (uint8_t)preset_id;
	switch (preset_id)
	{
	case _forge_snap_preset_front_center:
		pt->x = max_x; pt->y = cy; pt->z = cz;
		auto_dir = _forge_snap_dir_front;
		break;
	case _forge_snap_preset_back_center:
		pt->x = min_x; pt->y = cy; pt->z = cz;
		auto_dir = _forge_snap_dir_back;
		break;
	case _forge_snap_preset_left_center:
		pt->x = cx; pt->y = max_y; pt->z = cz;
		auto_dir = _forge_snap_dir_left;
		break;
	case _forge_snap_preset_right_center:
		pt->x = cx; pt->y = min_y; pt->z = cz;
		auto_dir = _forge_snap_dir_right;
		break;
	case _forge_snap_preset_top_center:
		pt->x = cx; pt->y = cy; pt->z = max_z;
		auto_dir = _forge_snap_dir_up;
		break;
	case _forge_snap_preset_bot_center:
		pt->x = cx; pt->y = cy; pt->z = min_z;
		auto_dir = _forge_snap_dir_down;
		break;
	case _forge_snap_preset_front_base:
		pt->x = max_x; pt->y = cy; pt->z = min_z;
		auto_dir = _forge_snap_dir_front;
		break;
	case _forge_snap_preset_back_base:
		pt->x = min_x; pt->y = cy; pt->z = min_z;
		auto_dir = _forge_snap_dir_back;
		break;
	case _forge_snap_preset_left_base:
		pt->x = cx; pt->y = max_y; pt->z = min_z;
		auto_dir = _forge_snap_dir_left;
		break;
	case _forge_snap_preset_right_base:
		pt->x = cx; pt->y = min_y; pt->z = min_z;
		auto_dir = _forge_snap_dir_right;
		break;
	default:
		auto_dir = (int)pt->normal_dir;
		break;
	}
	forge_snap_points_fill_normal_from_dir(pt, (normal_dir >= 0) ? normal_dir : auto_dir);
}

void forge_snap_points_auto_generate(uint32_t group_tag, char const *tag_path, int mode)
{
	struct forge_tag_snap_config *cfg = forge_snap_points_get_for_tag(group_tag, tag_path, 1);
	if (!cfg)
		return;

	cfg->point_count = 0;
	if (mode == 0) /* 4-SIDE BOX (Front, Back, Left, Right face centers) */
	{
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_center, -1);
	}
	else if (mode == 1) /* 2-END BRIDGE / WALL (Opposite ends along longest horizontal axis) */
	{
		float dx = s_snap_bounds_max[0] - s_snap_bounds_min[0];
		float dy = s_snap_bounds_max[1] - s_snap_bounds_min[1];
		if (dy > 1.25f * dx)
		{
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_center, -1);
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_center, -1);
		}
		else
		{
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_center, -1);
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_center, -1);
		}
	}
	else if (mode == 2) /* 6-FACE CUBE (All 6 bounding box faces) */
	{
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_top_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_bot_center, -1);
	}
	else if (mode == 3) /* 4 BASE EDGES (Floor-aligned Front, Back, Left, Right base midpoints) */
	{
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_base, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_base, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_base, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_base, -1);
	}
	forge_snap_points_save_all();
}

void forge_snap_points_bounds_set(float const bounds_min[3], float const bounds_max[3])
{
	memcpy(s_snap_bounds_min, bounds_min, sizeof(s_snap_bounds_min));
	memcpy(s_snap_bounds_max, bounds_max, sizeof(s_snap_bounds_max));
}

static int s_stock_bounds_found;
static float s_stock_bounds_min[3];
static float s_stock_bounds_max[3];

/* a part's vertices (68 bytes each), posed by the model's nodes, into the box */
static void forge_stock_bounds_add_part_68b(
	uint8_t const *verts_68,
	int32_t vtx_cnt,
	struct real_matrix4x3 const *node_mats,
	int32_t node_cnt,
	uint8_t const *local_nodes,
	int32_t local_node_cnt)
{
	int32_t vi;

	for (vi = 0; vi < vtx_cnt; vi++)
	{
		uint8_t const *vp = verts_68 + (size_t)vi * 68u;
		float position[3];
		float px = *(float const *)(vp + 0u);
		float py = *(float const *)(vp + 4u);
		float pz = *(float const *)(vp + 8u);
		int16_t n0 = *(int16_t const *)(vp + 56u);
		int16_t n1 = *(int16_t const *)(vp + 58u);
		float w0 = *(float const *)(vp + 60u);
		float w1 = *(float const *)(vp + 64u);
		int axis;

		if (!(px > -10000.0f && px < 10000.0f)) px = 0.0f;
		if (!(py > -10000.0f && py < 10000.0f)) py = 0.0f;
		if (!(pz > -10000.0f && pz < 10000.0f)) pz = 0.0f;
		if (!(w0 >= 0.0f && w0 <= 1.0f)) w0 = 1.0f;
		if (!(w1 >= 0.0f && w1 <= 1.0f)) w1 = 0.0f;
		if (w0 + w1 <= 0.001f) { w0 = 1.0f; w1 = 0.0f; }

		if (local_nodes && local_node_cnt > 0)
		{
			if (n0 >= 0 && n0 < local_node_cnt && n0 < 22)
				n0 = (int16_t)local_nodes[n0];
			if (n1 >= 0 && n1 < local_node_cnt && n1 < 22)
				n1 = (int16_t)local_nodes[n1];
		}

		if (node_mats && node_cnt > 0)
		{
			if (n0 >= 0 && n0 < node_cnt && n1 >= 0 && n1 < node_cnt && n1 != n0 && w1 > 0.001f)
			{
				struct real_matrix4x3 const *m0 = &node_mats[n0];
				struct real_matrix4x3 const *m1 = &node_mats[n1];
				float tx0 = m0->scale * (m0->forward.i * px + m0->left.i * py + m0->up.i * pz) + m0->position.x;
				float ty0 = m0->scale * (m0->forward.j * px + m0->left.j * py + m0->up.j * pz) + m0->position.y;
				float tz0 = m0->scale * (m0->forward.k * px + m0->left.k * py + m0->up.k * pz) + m0->position.z;
				float tx1 = m1->scale * (m1->forward.i * px + m1->left.i * py + m1->up.i * pz) + m1->position.x;
				float ty1 = m1->scale * (m1->forward.j * px + m1->left.j * py + m1->up.j * pz) + m1->position.y;
				float tz1 = m1->scale * (m1->forward.k * px + m1->left.k * py + m1->up.k * pz) + m1->position.z;
				px = w0 * tx0 + w1 * tx1;
				py = w0 * ty0 + w1 * ty1;
				pz = w0 * tz0 + w1 * tz1;
			}
			else if (n0 >= 0 && n0 < node_cnt)
			{
				struct real_matrix4x3 const *m = &node_mats[n0];
				float tx = m->scale * (m->forward.i * px + m->left.i * py + m->up.i * pz) + m->position.x;
				float ty = m->scale * (m->forward.j * px + m->left.j * py + m->up.j * pz) + m->position.y;
				float tz = m->scale * (m->forward.k * px + m->left.k * py + m->up.k * pz) + m->position.z;
				px = tx; py = ty; pz = tz;
			}
		}
		position[0] = px;
		position[1] = py;
		position[2] = pz;
		for (axis = 0; axis < 3; axis++)
		{
			if (!s_stock_bounds_found || position[axis] < s_stock_bounds_min[axis])
				s_stock_bounds_min[axis] = position[axis];
			if (!s_stock_bounds_found || position[axis] > s_stock_bounds_max[axis])
				s_stock_bounds_max[axis] = position[axis];
		}
		s_stock_bounds_found = 1;
	}
}

static void forge_preview_compute_node_mats(
	uint8_t const *node_base,
	int32_t node_cnt,
	struct real_matrix4x3 *out_mats)
{
	int32_t n;
	int pass;
	int computed[64];

	memset(computed, 0, sizeof(computed));
	if (node_cnt > 64) node_cnt = 64;
	for (n = 0; n < node_cnt; n++)
	{
		out_mats[n].scale = 1.0f;
		out_mats[n].forward.i = 1.0f; out_mats[n].forward.j = 0.0f; out_mats[n].forward.k = 0.0f;
		out_mats[n].left.i = 0.0f;    out_mats[n].left.j = 1.0f;    out_mats[n].left.k = 0.0f;
		out_mats[n].up.i = 0.0f;      out_mats[n].up.j = 0.0f;      out_mats[n].up.k = 1.0f;
		out_mats[n].position.x = 0.0f; out_mats[n].position.y = 0.0f; out_mats[n].position.z = 0.0f;
	}
	for (pass = 0; pass < node_cnt; pass++)
	{
		for (n = 0; n < node_cnt; n++)
		{
			uint8_t const *mn = node_base + (size_t)n * 156u;
			int16_t parent_idx = -1;
			int32_t m;
			struct real_matrix4x3 loc;
			struct real_quaternion rot;
			union real_point3d trans;

			if (computed[n])
				continue;
			/* Find parent by scanning first_child_node_index (+36) and next_sibling_node_index (+34) */
			for (m = 0; m < node_cnt; m++)
			{
				uint8_t const *pmn = node_base + (size_t)m * 156u;
				int16_t fc = *(int16_t const *)(pmn + 36u);
				int guard = 0;
				while (fc >= 0 && fc < node_cnt && guard++ < 64)
				{
					if (fc == n) { parent_idx = (int16_t)m; break; }
					fc = *(int16_t const *)(node_base + (size_t)fc * 156u + 34u);
				}
				if (parent_idx >= 0) break;
			}
			if (parent_idx >= 0 && !computed[parent_idx])
				continue;

			trans.x = *(float const *)(mn + 0x28u);
			trans.y = *(float const *)(mn + 0x2Cu);
			trans.z = *(float const *)(mn + 0x30u);
			rot.v.i = *(float const *)(mn + 0x34u);
			rot.v.j = *(float const *)(mn + 0x38u);
			rot.v.k = *(float const *)(mn + 0x3Cu);
			rot.w   = *(float const *)(mn + 0x40u);
			if (rot.v.i == 0.0f && rot.v.j == 0.0f && rot.v.k == 0.0f && rot.w == 0.0f)
				rot.w = 1.0f;
			matrix4x3_from_point_and_quaternion(&loc, &trans, &rot);
			if (parent_idx >= 0)
				matrix4x3_multiply(&out_mats[parent_idx], &loc, &out_mats[n]);
			else
				out_mats[n] = loc;
			computed[n] = 1;
		}
	}
}

static int forge_stock_bounds_from_donor_map(char const *map_path, uint32_t group_tag, char const *tag_path)
{
	struct forge_donor_map_ctx ctx;
	struct forge_ext_closure_list *closure;
	int root_idx;
	uint32_t obj_io, obj_data_addr, obj_off;
	int32_t mod_handle, mod_idx;
	uint32_t mod_io, mod_data_addr, mod_off;
	int32_t node_cnt, reg_cnt, geom_cnt, r_i;
	uint32_t node_addr, reg_addr, geom_addr;
	struct real_matrix4x3 node_mats[64];

	if (!forge_donor_map_open(map_path, &ctx))
		return 0;

	root_idx = forge_donor_find_tag_index(&ctx, group_tag, tag_path);
	if (root_idx < 0)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}

	/* The closure, for a child mod2 to fall back on */
	closure = (struct forge_ext_closure_list *)calloc(1, sizeof(*closure));
	if (closure)
		forge_donor_add_tag_closure(&ctx, root_idx, closure, 0);

	mod_idx = -1;
	obj_io = ctx.inst_off + (uint32_t)root_idx * 32u;
	obj_data_addr = *(uint32_t const *)(ctx.tag_buf + obj_io + 0x14);
	if (group_tag == 'actv' && obj_data_addr >= ctx.tag_base && (obj_data_addr - ctx.tag_base) + 0x24u <= ctx.tag_sz)
	{
		uint32_t actv_off = obj_data_addr - ctx.tag_base;
		uint32_t unit_grp = *(uint32_t const *)(ctx.tag_buf + actv_off + 0x14u);
		int32_t unit_handle = *(int32_t const *)(ctx.tag_buf + actv_off + 0x20u);
		if ((unit_grp == 'bipd' || unit_grp == 'unit' || unit_grp == 'vehi') && unit_handle != -1 && unit_handle != 0)
		{
			int32_t unit_idx = (int32_t)((uint32_t)unit_handle & 0xFFFFu);
			if (unit_idx >= 0 && unit_idx < ctx.tag_count)
			{
				uint32_t u_io = ctx.inst_off + (uint32_t)unit_idx * 32u;
				uint32_t u_addr = *(uint32_t const *)(ctx.tag_buf + u_io + 0x14u);
				if (u_addr >= ctx.tag_base && (u_addr - ctx.tag_base) + 0x38u <= ctx.tag_sz)
				{
					uint32_t u_off = u_addr - ctx.tag_base;
					uint32_t mod_ref_grp = *(uint32_t const *)(ctx.tag_buf + u_off + 0x28u);
					int32_t u_mod_handle = *(int32_t const *)(ctx.tag_buf + u_off + 0x34u);
					if ((mod_ref_grp == 'mod2' || mod_ref_grp == 'mode') && u_mod_handle != -1 && u_mod_handle != 0)
					{
						int32_t cand_idx = (int32_t)((uint32_t)u_mod_handle & 0xFFFFu);
						if (cand_idx >= 0 && cand_idx < ctx.tag_count)
							mod_idx = cand_idx;
					}
				}
			}
		}
	}
	else if (group_tag != 'actv' && obj_data_addr >= ctx.tag_base && (obj_data_addr - ctx.tag_base) + 0x38u <= ctx.tag_sz)
	{
		uint32_t mod_ref_grp;
		obj_off = obj_data_addr - ctx.tag_base;
		mod_ref_grp = *(uint32_t const *)(ctx.tag_buf + obj_off + 0x28u);
		mod_handle = *(int32_t const *)(ctx.tag_buf + obj_off + 0x34u);
		if ((mod_ref_grp == 'mod2' || mod_ref_grp == 'mode') && mod_handle != -1 && mod_handle != 0)
		{
			int32_t cand_idx = (int32_t)((uint32_t)mod_handle & 0xFFFFu);
			if (cand_idx >= 0 && cand_idx < ctx.tag_count)
				mod_idx = cand_idx;
		}
	}
	/* Fallback for 'actv' or composite objects whose model is in a child tag of the closure */
	if (mod_idx < 0 && closure)
	{
		int ci;
		for (ci = 0; ci < closure->count; ci++)
		{
			int32_t ti = closure->tag_indices[ci];
			uint32_t tio = ctx.inst_off + (uint32_t)ti * 32u;
			uint32_t tgrp = *(uint32_t const *)(ctx.tag_buf + tio + 0x00);
			if (tgrp == 'mod2' || tgrp == 'mode')
			{
				mod_idx = ti;
				break;
			}
		}
	}
	free(closure);
	if (mod_idx < 0 || mod_idx >= ctx.tag_count)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}

	mod_io = ctx.inst_off + (uint32_t)mod_idx * 32u;
	mod_data_addr = *(uint32_t const *)(ctx.tag_buf + mod_io + 0x14);
	if (mod_data_addr < ctx.tag_base || (mod_data_addr - ctx.tag_base) + 0xE8u > ctx.tag_sz)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}
	mod_off = mod_data_addr - ctx.tag_base;

	node_cnt = *(int32_t const *)(ctx.tag_buf + mod_off + 0xB8u);
	node_addr = *(uint32_t const *)(ctx.tag_buf + mod_off + 0xBCu);
	if (node_cnt > 0 && node_cnt <= 64 && node_addr >= ctx.tag_base &&
		(node_addr - ctx.tag_base) + (uint32_t)node_cnt * 156u <= ctx.tag_sz)
	{
		forge_preview_compute_node_mats(ctx.tag_buf + (node_addr - ctx.tag_base), node_cnt, node_mats);
	}
	else
	{
		node_cnt = 0;
	}

	reg_cnt = *(int32_t const *)(ctx.tag_buf + mod_off + 0xC4u);
	reg_addr = *(uint32_t const *)(ctx.tag_buf + mod_off + 0xC8u);
	geom_cnt = *(int32_t const *)(ctx.tag_buf + mod_off + 0xD0u);
	geom_addr = *(uint32_t const *)(ctx.tag_buf + mod_off + 0xD4u);

	if (geom_cnt <= 0 || geom_cnt > 256 || geom_addr < ctx.tag_base ||
		(uint64_t)(geom_addr - ctx.tag_base) + (uint64_t)geom_cnt * 48u > (uint64_t)ctx.tag_sz)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}

	/* Load highest LOD geometry of each region (or geometry 0 if no regions) */
	{
	for (r_i = 0; r_i < ((reg_cnt > 0 && reg_cnt <= 32) ? reg_cnt : 1); r_i++)
	{
		int32_t g_idx = 0;
		uint32_t g_off;
		int32_t part_cnt, p_idx;
		uint32_t parts_addr;
		uint32_t part_stride = ctx.is_xbox_v5 ? 104u : 132u;

		if (reg_cnt > 0 && reg_addr >= ctx.tag_base &&
			(uint64_t)(reg_addr - ctx.tag_base) + (uint64_t)reg_cnt * 76u <= (uint64_t)ctx.tag_sz)
		{
			uint32_t r_off = (reg_addr - ctx.tag_base) + (uint32_t)r_i * 76u;
			int32_t perm_cnt = *(int32_t const *)(ctx.tag_buf + r_off + 64u);
			uint32_t perm_addr = *(uint32_t const *)(ctx.tag_buf + r_off + 68u);
			if (perm_cnt > 0 && perm_addr >= ctx.tag_base &&
				(perm_addr - ctx.tag_base) + 88u <= ctx.tag_sz)
			{
				int lod_i;
				for (lod_i = 4; lod_i >= 0; lod_i--)
				{
					int16_t cand_g = *(int16_t const *)(ctx.tag_buf + (perm_addr - ctx.tag_base) + 64u + (uint32_t)lod_i * 2u);
					if (cand_g >= 0 && cand_g < geom_cnt)
					{
						g_idx = cand_g;
						break;
					}
				}
			}
		}

		g_off = (geom_addr - ctx.tag_base) + (uint32_t)g_idx * 48u;
		part_cnt = *(int32_t const *)(ctx.tag_buf + g_off + 36u);
		parts_addr = *(uint32_t const *)(ctx.tag_buf + g_off + 40u);
		if (part_cnt <= 0 || part_cnt > 256 || parts_addr < ctx.tag_base ||
			(uint64_t)(parts_addr - ctx.tag_base) + (uint64_t)part_cnt * part_stride > (uint64_t)ctx.tag_sz)
			continue;

		for (p_idx = 0; p_idx < part_cnt; p_idx++)
		{
			uint8_t const *sp = ctx.tag_buf + (parts_addr - ctx.tag_base) + (uint32_t)p_idx * part_stride;

			if (!ctx.is_xbox_v5)
			{
				int32_t tri_cnt = *(int32_t const *)(sp + 0x48u);
				uint32_t idx_off = *(uint32_t const *)(sp + 0x4Cu);
				int32_t vtx_cnt = *(int32_t const *)(sp + 0x58u);
				uint32_t vtx_off = *(uint32_t const *)(sp + 0x64u);
				if (vtx_cnt > 0 && vtx_cnt <= 32768 && tri_cnt > 0 && tri_cnt <= 32768)
				{
					size_t v_bytes = (size_t)vtx_cnt * 68u;
					size_t i_bytes = (size_t)(tri_cnt + 2) * 2u;
					uint8_t *v_buf = (uint8_t *)malloc(v_bytes);
					uint16_t *i_buf = (uint16_t *)malloc(i_bytes);
					if (v_buf && i_buf &&
						fseek(ctx.map_fp, (long)(ctx.vtx_file_off + vtx_off), SEEK_SET) == 0 &&
						fread(v_buf, 1, v_bytes, ctx.map_fp) == v_bytes &&
						fseek(ctx.map_fp, (long)(ctx.idx_file_off + idx_off), SEEK_SET) == 0 &&
						fread(i_buf, 1, i_bytes, ctx.map_fp) == i_bytes)
					{
						{
							int has_local_nodes = ((*(uint32_t const *)(ctx.tag_buf + mod_off + 0x00) & 2u) != 0);
							forge_stock_bounds_add_part_68b(
								v_buf, vtx_cnt,
								(node_cnt > 0) ? node_mats : NULL, node_cnt,
								has_local_nodes ? (sp + 108u) : NULL,
								has_local_nodes ? (int32_t)sp[107u] : 0);
						}
					}
					free(v_buf);
					free(i_buf);
				}
			}
			else
			{
				/* Xbox 104-byte model_geometry_part:
				 * triangle_buffer at +68 (count at +72, base_address at +76, hardware_format D3DIndexBuffer* at +80)
				 * vertex_buffer at +84 (type at +84, count at +88, offset at +92, base_address at +96, hardware_format D3DVertexBuffer* at +100)
				 * In Xbox v5 cache files, base_address (+76 / +96) is 0 on disk; hardware_format (+80 / +100) points
				 * to a 12-byte D3D header in the tag cache whose +4u field holds the actual data address. */
				int32_t tri_cnt = *(int32_t const *)(sp + 68u + 4u);
				uint32_t tri_addr = *(uint32_t const *)(sp + 68u + 8u);
				uint32_t tb_hw_addr = *(uint32_t const *)(sp + 68u + 12u);
				int16_t vtx_type = *(int16_t const *)(sp + 84u + 0u);
				int32_t vtx_cnt = *(int32_t const *)(sp + 84u + 4u);
				uint32_t vtx_addr = *(uint32_t const *)(sp + 84u + 12u);
				uint32_t vb_hw_addr = *(uint32_t const *)(sp + 84u + 16u);
				uint32_t v_stride = (vtx_type == _rasterizer_vertex_type_model_uncompressed) ? 68u : 32u;

				if ((tri_addr < ctx.tag_base || (tri_addr - ctx.tag_base) >= ctx.tag_sz) &&
					tb_hw_addr >= ctx.tag_base && (tb_hw_addr - ctx.tag_base) + 12u <= ctx.tag_sz)
				{
					tri_addr = *(uint32_t const *)(ctx.tag_buf + (tb_hw_addr - ctx.tag_base) + 4u);
				}
				if ((vtx_addr < ctx.tag_base || (vtx_addr - ctx.tag_base) >= ctx.tag_sz) &&
					vb_hw_addr >= ctx.tag_base && (vb_hw_addr - ctx.tag_base) + 12u <= ctx.tag_sz)
				{
					vtx_addr = *(uint32_t const *)(ctx.tag_buf + (vb_hw_addr - ctx.tag_base) + 4u);
				}

				if (vtx_cnt > 0 && vtx_cnt <= 32768 && tri_cnt > 0 && tri_cnt <= 32768 &&
					tri_addr >= ctx.tag_base && (tri_addr - ctx.tag_base) + (uint32_t)(tri_cnt + 2) * 2u <= ctx.tag_sz &&
					vtx_addr >= ctx.tag_base && (vtx_addr - ctx.tag_base) + (uint32_t)vtx_cnt * v_stride <= ctx.tag_sz)
				{
					uint8_t *v_buf = (uint8_t *)malloc((size_t)vtx_cnt * 68u);
					if (v_buf)
					{
						int32_t vi;
						if (v_stride == 68u)
						{
							memcpy(v_buf, ctx.tag_buf + (vtx_addr - ctx.tag_base), (size_t)vtx_cnt * 68u);
						}
						else
						{
							for (vi = 0; vi < vtx_cnt; vi++)
							{
								forge_decompress_xbox_vertex_to_68b(
									ctx.tag_buf + (vtx_addr - ctx.tag_base) + (uint32_t)vi * 32u,
									v_buf + (size_t)vi * 68u);
							}
						}
						forge_stock_bounds_add_part_68b(
							v_buf, vtx_cnt,
							(node_cnt > 0) ? node_mats : NULL, node_cnt,
							NULL, 0);
						free(v_buf);
					}
				}
			}
		}
	}
	}

	forge_donor_map_close(&ctx);
	return s_stock_bounds_found;
}

/* whether a tag is one of the default palette's, the path of the map of the
pack it was found in into `map_path` (which may be NULL) */
int forge_snap_points_stock_tag(uint32_t group_tag, char const *tag_path, char *map_path, size_t map_path_size)
{
	long pack = port_tag_packs_find(FORGE_PACK);
	int ci;

	if (pack < 0 || !tag_path || !tag_path[0])
		return 0;
	for (ci = 0; ci < g_forge.catalog_count; ci++)
	{
		if (g_forge.catalog[ci].pad && g_forge.catalog[ci].group_tag == group_tag &&
			_stricmp(g_forge.catalog[ci].tag_path, tag_path) == 0)
		{
			if (map_path)
				snprintf(map_path, map_path_size, "maps/%s.map", port_tag_packs_map(pack, g_forge.catalog[ci].pad - 1));
			return 1;
		}
	}
	return 0;
}

int forge_snap_points_read_stock_bounds(uint32_t group_tag, char const *tag_path)
{
	char map_path[512];

	if (!forge_snap_points_stock_tag(group_tag, tag_path, map_path, sizeof(map_path)))
		return 0;
	s_stock_bounds_found = 0;
	if (!forge_stock_bounds_from_donor_map(map_path, group_tag, tag_path))
		return 0;
	forge_snap_points_bounds_set(s_stock_bounds_min, s_stock_bounds_max);
	return 1;
}

