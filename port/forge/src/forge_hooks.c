/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Engine Symbol Bridge Hooks (linked via llvm-objcopy --redefine-sym).
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "camera/director.h"
#include "interface/hud.h"
#include "interface/hud_draw.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "main/console.h"
#include "models/models.h"
#include "models/model_definitions.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "saved games/player_profile.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"
#include "network_chat.h"
#include "custom_edition_maps.h"
#include <xtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forge_engine.h"

#undef fopen
#undef fclose
#undef fread
#undef fseek
#ifndef HALO_64BIT /* AE hook */
#undef malloc
#undef free
#endif /* AE hook */
#undef realloc
#define fopen(p, m) ((FILE *)forge_host_fopen((p), (m)))

#include "game/game_engine.h"
#include "objects/objects.h"
#include "units/units.h"

extern unsigned long system_milliseconds(void);
extern long hud_get_font_index(void);
extern boolean firefight_is_generator_hidden_biped(long object_index);
extern void *cache_files_tag_instances(long *count);
extern void cache_files_set_tag_instances(void *instances, long count);
extern char const *pc_menu_function_name(long function_index);
extern char const *pc_menu_game_data_input_name(long function_index);
extern void main_menu_switch_to_single_player(void);
extern void game_connection_set(short connection);
extern void dispose_global_network_game_client(void);
extern void dispose_global_network_game_server(void);
extern void network_game_accept_remote_connections(boolean accept);
extern boolean network_game_is_active(void);
extern long player_ui_get_active_player_profile_index(short local_player_index);
extern boolean player_ui_get_active_player_profile(short local_player_index, struct player_profile *profile);
extern void player_ui_set_active_player_profile(short local_player_index, long profile_index, struct player_profile const *profile);
extern void player_ui_set_single_player_local_player_controller(short local_player_index, short controller_index);
extern void player_ui_remember_player1_profile(boolean remember);
extern void ui_play_audio_feedback_sound(short index);
extern void display_error_text_deferred(wchar_t const *error_text, short text_style);
extern short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
extern boolean ui_widget_port_gametype_choose(long profile_index);
extern boolean ui_widget_port_open(void *widget, char const *name, boolean *widget_deleted);
extern void main_disallow_persistent_storage(void);
extern void ui_widget_port_go_back(void *widget);
extern boolean ui_widget_port_game_variant_set(struct game_variant *variant, struct game_variant_options const *options);
extern struct game_variant *build_game_variant_slayer(struct game_variant *variant);
extern void game_variant_options_default(struct game_variant const *variant, struct game_variant_options *options);

static int s_is_forge_map_cache_valid = 0;
/* whether each of custom_edition_maps_count(FALSE)'s maps is Forge's */
static unsigned char s_ce_map_forge_made[CUSTOM_EDITION_MAPS_MAXIMUM];
static long s_save_forge_btn_tag = NONE;

/* Original engine symbols redefined via llvm-objcopy in FORGE CODE/build/wrapped_obj/ */
extern void orig_network_chat_update(void);
extern void orig_network_chat_draw(short local_player_index);
extern boolean orig_network_chat_typing(void);
extern void orig_network_chat_handle_up(long machine_index, void const *entries, boolean from_stream);
extern void orig_network_chat_handle_down(void const *entries, boolean from_stream);
extern void orig_network_objects_handle_changes(void const *entries, short count);
extern void orig_director_update(real time_delta_sec);
extern short orig_director_get_perspective(short local_player_index);
extern struct observer_result const *orig_render_interpolation_camera(
	short local_player_index,
	struct observer_result const *fallback);
extern void orig_first_person_weapon_draw(void);
extern void orig_hud_render_weapon_interface(struct player_datum *player);
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
extern boolean orig_hs_compile_and_evaluate(char const *source);
extern void orig_hs_runtime_update(void);
extern void orig_cinematic_start(void);
extern boolean orig_scenario_trigger_volume_test_object(short trigger_volume_index, long object_index);
extern long orig_game_engine_remap_object_definition(long definition_index);
extern void orig_game_engine_post_rasterize_objects(void);
extern void rasterizer_models_begin(boolean sky);
extern void rasterizer_models_end(void);
extern void orig_menu_tags_loaded(char const *map_name);
extern void orig_menu_tags_unloaded(void);
extern boolean orig_pc_menu_event_function_invoke(
	void *widget,
	void *event,
	long function_index,
	boolean *widget_deleted);
extern void orig_pc_menu_game_data_function_invoke(
	void *widget,
	long function);

static unsigned long g_last_forge_frame_ms = 0;
static char g_last_forge_map_name[128] = "";

/* ---------- Widget Instance Layout (matches port/linux/game/menu_functions.c) */

struct forge_widget_instance
{
	long definition_tag_index;
	char const *name;
	short local_player_index;
	short horizontal_offset;
	short vertical_offset;
	short type;
	boolean visible;
	boolean render_regardless_of_controller_index;
	boolean disabled;
	boolean pause_game_time;
	boolean delete_recursion_lock;
	boolean widget_is_error_dialog;
	boolean close_if_local_player_controller_present;
	byte pad17;
	long creation_time;
	unsigned long milliseconds_to_auto_close;
	unsigned long auto_close_fade_time;
	real alpha_modifier;
	struct forge_widget_instance *previous;
	struct forge_widget_instance *next;
	struct forge_widget_instance *parent;
	struct forge_widget_instance *child;
	struct forge_widget_instance *focused_child;
	union
	{
		struct
		{
			wchar_t *text;
			short string_list_index;
		} text_box;
		struct
		{
			short selected_index;
			short last_list_tab_direction;
			void *list_items;
			word number_of_items;
			struct forge_widget_instance *extended_description;
			wchar_t *item_text;
		} list;
	} parameters;
	struct
	{
		short current_frame_index;
		short first_frame_index;
		short last_frame_index;
		short number_of_sprite_frames;
	} animation;
};

static struct forge_widget_instance *fw_descendant(struct forge_widget_instance *w, char const *name, long *nth)
{
	struct forge_widget_instance *c;
	for (c = w ? w->child : NULL; c; c = c->next)
	{
		struct forge_widget_instance *found;
		if (c->name && !strcmp(c->name, name) && (*nth)-- == 0)
			return c;
		found = fw_descendant(c, name, nth);
		if (found)
			return found;
	}
	return NULL;
}

static struct forge_widget_instance *fw_named(struct forge_widget_instance *w, char const *name, long nth)
{
	return w ? fw_descendant(w, name, &nth) : NULL;
}

static void fw_text_set_ascii(struct forge_widget_instance *tb, char const *ascii)
{
	int i;
	if (!tb || !ascii)
		return;
	tb->parameters.text_box.string_list_index = NONE;
	if (tb->definition_tag_index != NONE)
	{
		uint8_t *def = (uint8_t *)tag_get('DeLa', tb->definition_tag_index);
		if (def)
			*(int32_t *)(def + 0xF8) = NONE;
	}
	if (!tb->parameters.text_box.text)
	{
		tb->parameters.text_box.text = (wchar_t *)ui_widget_realloc(NULL, 256 * sizeof(wchar_t), __FILE__, __LINE__);
	}
	if (tb->parameters.text_box.text)
	{
		for (i = 0; ascii[i] && i < 254; i++)
			tb->parameters.text_box.text[i] = (wchar_t)(unsigned char)ascii[i];
		tb->parameters.text_box.text[i] = 0;
	}
}

/* ---------- Forge Edit Maps (User Created / Downloaded) & Inject Custom Tags Native Menus */

#define FORGE_MAX_EDIT_MAPS 64

static struct
{
	int tab; /* 0 = USER CREATED, 1 = DOWNLOADED */
	int count;
	int first;
	int chosen;
	char map_leaves[FORGE_MAX_EDIT_MAPS][64];
	char map_titles[FORGE_MAX_EDIT_MAPS][64];
	char map_authors[FORGE_MAX_EDIT_MAPS][64];
} s_forge_menu;

static int is_forge_created_map_file(char const *map_path, char const *leaf_no_ext, int *out_is_user_created, char *out_author, size_t author_sz)
{
	FILE *f;
	uint8_t hdr[2048];
	uint32_t marker = 0;
	char txt_path[512];
	int has_flg1 = 0;
	int user_created = 0;

	if (out_author && author_sz > 0)
		snprintf(out_author, author_sz, "Community Creator");

	f = fopen(map_path, "rb");
	if (f)
	{
		if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr))
		{
			uint32_t state_off = *(uint32_t const *)(hdr + 0x1E4);
			uint32_t state_sz = *(uint32_t const *)(hdr + 0x1E8);
			char const * internal_name = (char const *)(hdr + 0x20);
			marker = *(uint32_t const *)(hdr + 0x1FC);
			if (marker == 0x464C4731u) /* 'FLG1' */
			{
				has_flg1 = 1;
				user_created = 1;
				if (state_off >= 0x800u && state_sz > 0 && state_sz < 4u * 1024u * 1024u)
				{
					char buf[1024];
					uint32_t rsz = (state_sz < sizeof(buf) - 1u) ? state_sz : (uint32_t)(sizeof(buf) - 1u);
					if (fseek(f, (long)state_off, SEEK_SET) == 0 && fread(buf, 1, rsz, f) == rsz)
					{
						buf[rsz] = 0;
						if (strstr(buf, "[OpenCE Forge Map]") != NULL)
							has_flg1 = 1;
						if (strstr(buf, "ForgeSource=UserCreated") != NULL)
							user_created = 1;
					}
				}
			}
			else if (_strnicmp(internal_name, "levels\\forge\\", 13) == 0 ||
			         _strnicmp(internal_name, "levels/forge/", 13) == 0)
			{
				has_flg1 = 1;
				user_created = 1;
			}
		}
		fclose(f);
	}

	if (leaf_no_ext && strlen(leaf_no_ext) > 6 &&
	    _stricmp(leaf_no_ext + strlen(leaf_no_ext) - 6, "_forge") == 0)
	{
		has_flg1 = 1;
		user_created = 1;
	}

	snprintf(txt_path, sizeof(txt_path), "OpenCE Forge/forge_maps/%s/%s.txt", leaf_no_ext, leaf_no_ext);
	f = fopen(txt_path, "rb");
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "Faber_data/forge_maps/%s/%s.txt", leaf_no_ext, leaf_no_ext);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "forge_maps/%s/%s.txt", leaf_no_ext, leaf_no_ext);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "custom_maps/%s.txt", leaf_no_ext);
		f = fopen(txt_path, "rb");
	}
	if (f)
	{
		char buf[1024];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		buf[n] = 0;
		fclose(f);
		if (strstr(buf, "[OpenCE Forge Map]") != NULL)
			has_flg1 = 1;
		if (strstr(buf, "ForgeSource=UserCreated") != NULL)
			user_created = 1;
	}

	if (out_is_user_created)
		*out_is_user_created = user_created;
	return has_flg1;
}

static int is_custom_map_leaf_forge_made(char const *leaf_no_ext)
{
	char check_path[512];
	FILE *f;
	static char const *const s_forge_check_fmts[] = {
		"OpenCE Forge/forge_maps/%s.map",
		"OpenCE Forge/forge_maps/%s/%s.map",
		"OpenCE Forge/forge_maps/%s/%s.txt",
		"Faber_data/forge_maps/%s/%s.txt",
		"Faber_data/forge_maps/%s/%s.map",
		"forge_maps/%s.map",
		"forge_maps/%s/%s.txt",
		"forge_maps/%s/%s.map"
	};
	int i;

	if (!leaf_no_ext || !leaf_no_ext[0])
		return 0;

	if (strlen(leaf_no_ext) > 6 &&
	    _stricmp(leaf_no_ext + strlen(leaf_no_ext) - 6, "_forge") == 0)
	{
		return 1;
	}

	for (i = 0; i < (int)(sizeof(s_forge_check_fmts) / sizeof(s_forge_check_fmts[0])); i++)
	{
		if (strstr(s_forge_check_fmts[i], "%s/%s") != NULL)
			snprintf(check_path, sizeof(check_path), s_forge_check_fmts[i], leaf_no_ext, leaf_no_ext);
		else
			snprintf(check_path, sizeof(check_path), s_forge_check_fmts[i], leaf_no_ext);
		f = fopen(check_path, "rb");
		if (f)
		{
			fclose(f);
			return 1;
		}
	}

	/* Also check if custom_maps/<name>.map has the 'FLG1' Forge header marker or levels\forge\ scenario
	(the file the game finds for it: in a Map Manager folder too) */
	{
		extern boolean custom_edition_map_file_path(char const *map_name, char *path);
		char found[256];

		if (custom_edition_map_file_path(leaf_no_ext, found))
			return is_forge_created_map_file(found, leaf_no_ext, NULL, NULL, 0);
	}
	snprintf(check_path, sizeof(check_path), "custom_maps/%s.map", leaf_no_ext);
	return is_forge_created_map_file(check_path, leaf_no_ext, NULL, NULL, 0);
}

static void ensure_forge_ce_map_partition_built(void)
{
	short total_ce, i;

	if (s_is_forge_map_cache_valid)
		return;
	s_is_forge_map_cache_valid = 1;

	total_ce = custom_edition_maps_count(FALSE);
	for (i = 0; i < total_ce; i++)
	{
		short disp = custom_edition_maps_display_index_of(FALSE, i);
		char const *lvl = custom_edition_maps_level_name(disp);
		char const *leaf = lvl;
		if (lvl)
		{
			char const *bs = strrchr(lvl, '\\');
			char const *fs = strrchr(lvl, '/');
			if (bs && (!fs || bs > fs))
				leaf = bs + 1;
			else if (fs)
				leaf = fs + 1;
		}
		s_ce_map_forge_made[i] = leaf && is_custom_map_leaf_forge_made(leaf);
	}
}

void forge_menu_custom_edition_maps_look_again(void)
{
	s_is_forge_map_cache_valid = 0;
	custom_edition_maps_look_again();
}

/* the map lists' FORGE MAPS: whether a map of custom_edition_maps_count(FALSE)'s
list is one Forge saved (menu_functions.c lists these) */
int forge_custom_map_is_forge_made(short index)
{
	ensure_forge_ce_map_partition_built();
	return index >= 0 && index < custom_edition_maps_count(FALSE) && s_ce_map_forge_made[index];
}

static void scan_forge_edit_maps(int wanted_tab)
{
	WIN32_FIND_DATAA fd;
	HANDLE h;

	s_forge_menu.tab = wanted_tab;
	s_forge_menu.count = 0;
	s_forge_menu.first = 0;
	s_forge_menu.chosen = 0;

	/* Scan OpenCE Forge/forge_maps, Faber_data/forge_maps, and forge_maps subfolders & .map files */
	{
		static char const *const forge_scan_dirs[] = {
			"OpenCE Forge\\forge_maps\\*",
			"Faber_data\\forge_maps\\*",
			"forge_maps\\*"
		};
		int di;
		for (di = 0; di < 3; di++)
		{
			h = FindFirstFileA(forge_scan_dirs[di], &fd);
			if (h == INVALID_HANDLE_VALUE)
				continue;
			do
			{
				char full_path[512];
				char leaf[64];
				char author[64];
				int is_user = 0;
				int k, already_listed = 0;
				FILE *tf;

				if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
					continue;

				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					snprintf(leaf, sizeof(leaf), "%s", fd.cFileName);
				}
				else
				{
					char *dot;
					snprintf(leaf, sizeof(leaf), "%s", fd.cFileName);
					dot = strrchr(leaf, '.');
					if (!dot || _stricmp(dot, ".map") != 0)
						continue;
					*dot = 0;
				}

				for (k = 0; k < s_forge_menu.count; k++)
				{
					if (_stricmp(s_forge_menu.map_leaves[k], leaf) == 0)
					{
						already_listed = 1;
						break;
					}
				}
				if (already_listed)
					continue;

				snprintf(full_path, sizeof(full_path), "custom_maps/%s.map", leaf);
				tf = fopen(full_path, "rb");
				if (tf)
				{
					fclose(tf);
				}
				else
				{
					static char const *const map_cands[] = {
						"OpenCE Forge/forge_maps/%s.map",
						"OpenCE Forge/forge_maps/%s/%s.map",
						"Faber_data/forge_maps/%s/%s.map",
						"forge_maps/%s.map",
						"forge_maps/%s/%s.map"
					};
					int ci;
					for (ci = 0; ci < 5; ci++)
					{
						if (strstr(map_cands[ci], "%s/%s"))
							snprintf(full_path, sizeof(full_path), map_cands[ci], leaf, leaf);
						else
							snprintf(full_path, sizeof(full_path), map_cands[ci], leaf);
						tf = fopen(full_path, "rb");
						if (tf)
						{
							fclose(tf);
							break;
						}
					}
				}
				if (is_forge_created_map_file(full_path, leaf, &is_user, author, sizeof(author)))
				{
					if ((wanted_tab == 0 && is_user) || (wanted_tab == 1 && !is_user))
					{
						if (s_forge_menu.count < FORGE_MAX_EDIT_MAPS)
						{
							int idx = s_forge_menu.count++;
							snprintf(s_forge_menu.map_leaves[idx], sizeof(s_forge_menu.map_leaves[0]), "%s", leaf);
							snprintf(s_forge_menu.map_titles[idx], sizeof(s_forge_menu.map_titles[0]), "%s", leaf);
							snprintf(s_forge_menu.map_authors[idx], sizeof(s_forge_menu.map_authors[0]), "%s",
								is_user ? "User Created Forge Map" : "Downloaded Forge Map");
						}
					}
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}

	/* Also scan legacy custom_maps\*.map Forge maps */
	h = FindFirstFileA("custom_maps\\*.map", &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		char full_path[512];
		char leaf[64];
		char author[64];
		char *dot;
		int is_user = 0;
		int k, already_listed = 0;

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		if (_stricmp(fd.cFileName, "bitmaps.map") == 0 ||
			_stricmp(fd.cFileName, "sounds.map") == 0 ||
			_stricmp(fd.cFileName, "loc.map") == 0)
		{
			continue;
		}

		snprintf(leaf, sizeof(leaf), "%s", fd.cFileName);
		dot = strrchr(leaf, '.');
		if (dot)
			*dot = 0;

		for (k = 0; k < s_forge_menu.count; k++)
		{
			if (_stricmp(s_forge_menu.map_leaves[k], leaf) == 0)
			{
				already_listed = 1;
				break;
			}
		}
		if (already_listed)
			continue;

		snprintf(full_path, sizeof(full_path), "custom_maps/%s", fd.cFileName);
		if (is_forge_created_map_file(full_path, leaf, &is_user, author, sizeof(author)))
		{
			if ((wanted_tab == 0 && is_user) || (wanted_tab == 1 && !is_user))
			{
				if (s_forge_menu.count < FORGE_MAX_EDIT_MAPS)
				{
					int idx = s_forge_menu.count++;
					snprintf(s_forge_menu.map_leaves[idx], sizeof(s_forge_menu.map_leaves[0]), "%s", leaf);
					snprintf(s_forge_menu.map_titles[idx], sizeof(s_forge_menu.map_titles[0]), "%s", leaf);
					snprintf(s_forge_menu.map_authors[idx], sizeof(s_forge_menu.map_authors[0]), "%s",
						is_user ? "User Created Forge Map" : "Downloaded Forge Map");
				}
			}
		}
	} while (FindNextFileA(h, &fd));
	CloseHandle(h);
}

static void update_forge_edit_maps_widget(struct forge_widget_instance *list)
{
	struct forge_widget_instance *spinner = fw_named(list, "edit_maps_kind_spinner", 0);
	struct forge_widget_instance *desc = list->parameters.list.extended_description;
	struct forge_widget_instance *child;
	int kind = spinner ? (int)spinner->parameters.list.selected_index : 0;
	int row_idx = 0;
	int focused_entry = -1;

	if (kind < 0) kind = 0;
	if (kind > 1) kind = 1;
	if (kind != s_forge_menu.tab)
	{
		scan_forge_edit_maps(kind);
	}

	for (child = list->child; child; child = child->next, row_idx++)
	{
		int item_slot;
		if (row_idx < 1)
			continue;
		if (!child->name || strncmp(child->name, "list_item_", 10) != 0)
			break;
		item_slot = s_forge_menu.first + (row_idx - 1);
		if (s_forge_menu.count == 0)
		{
			child->visible = (row_idx == 1) ? TRUE : FALSE;
			if (row_idx == 1)
			{
				fw_text_set_ascii(
					fw_named(child, "list_item_text", 0),
					kind == 0 ? "(No User Created Maps Yet)" : "(No Downloaded Forge Maps)");
			}
		}
		else
		{
			child->visible = (item_slot < s_forge_menu.count) ? TRUE : FALSE;
			if (item_slot < s_forge_menu.count)
			{
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), s_forge_menu.map_titles[item_slot]);
				if (child == list->focused_child)
					focused_entry = item_slot;
			}
		}
		if (fw_named(child, "list_item_arrows", 0))
			fw_named(child, "list_item_arrows", 0)->visible = (child == list->focused_child);
		if (fw_named(child, "scroll_up_button", 0))
			fw_named(child, "scroll_up_button", 0)->visible = FALSE;
		if (fw_named(child, "scroll_down_button", 0))
			fw_named(child, "scroll_down_button", 0)->visible = FALSE;
	}

	if (focused_entry >= 0 && focused_entry < s_forge_menu.count)
		s_forge_menu.chosen = focused_entry;

	if (desc)
	{
		struct forge_widget_instance *sp_name = fw_named(desc, "replay_level_right_name", 0);
		struct forge_widget_instance *sp_pic = fw_named(desc, "replay_level_right_pic", 0);
		struct forge_widget_instance *sp_data = fw_named(desc, "replay_level_right_data", 0);
		struct forge_widget_instance *mp_name = fw_named(desc, "mp_map_right_name", 0);
		struct forge_widget_instance *mp_pic = fw_named(desc, "mp_map_right_pic", 0);
		struct forge_widget_instance *mp_data = fw_named(desc, "mp_map_right_data", 0);
		long d_idx;

		/* Always hide the single-player right-hand widgets ('Truth and Reconciliation' default) and difficulty icons */
		if (sp_name) sp_name->visible = FALSE;
		if (sp_pic) sp_pic->visible = FALSE;
		if (sp_data) sp_data->visible = FALSE;
		for (d_idx = 0; d_idx < 3; d_idx++)
		{
			struct forge_widget_instance *di = fw_named(desc, "difficulty_indicator", d_idx);
			if (di) di->visible = FALSE;
		}

		if (s_forge_menu.count > 0 && s_forge_menu.chosen >= 0 && s_forge_menu.chosen < s_forge_menu.count)
		{
			char level_name[128];
			char info[192];
			short disp_idx;

			snprintf(level_name, sizeof(level_name), "custom_maps\\%s", s_forge_menu.map_leaves[s_forge_menu.chosen]);
			disp_idx = custom_edition_maps_display_index(level_name);
			snprintf(info, sizeof(info), "%s\r\n%s.map\r\nPress OK to Edit in Forge",
				s_forge_menu.map_authors[s_forge_menu.chosen],
				s_forge_menu.map_leaves[s_forge_menu.chosen]);

			if (disp_idx != NONE)
			{
				wchar_t *desc_w = custom_edition_maps_description(disp_idx);
				if (desc_w)
				{
					int k;
					for (k = 0; info[k] && k < 254; k++)
						desc_w[k] = (wchar_t)(unsigned char)info[k];
					desc_w[k] = 0;
				}
				if (mp_name)
				{
					mp_name->visible = TRUE;
					mp_name->parameters.text_box.string_list_index = disp_idx;
				}
				if (mp_pic)
				{
					mp_pic->visible = TRUE;
					mp_pic->animation.current_frame_index = disp_idx;
				}
				if (mp_data)
				{
					mp_data->visible = TRUE;
					mp_data->parameters.text_box.string_list_index = disp_idx;
				}
			}
			else
			{
				if (mp_name) mp_name->visible = FALSE;
				if (mp_pic) mp_pic->visible = FALSE;
				if (mp_data) mp_data->visible = FALSE;
			}
		}
		else
		{
			if (mp_name) mp_name->visible = FALSE;
			if (mp_pic) mp_pic->visible = FALSE;
			if (mp_data) mp_data->visible = FALSE;
		}
	}
}

/* NONE, then each profile with a tag on (MAXIMUM_TAG_PROFILES at most) */
#define FORGE_PROFILE_SCREEN_ROWS 65

struct forge_profile_row
{
	char display_row[64];
};

/* The TAG PROFILE screen, after the map */
struct forge_profile_screen_state
{
	int profile_count;
	int profile_first;
	int profile_chosen;
	int profile_prev_row_focus;
	long profile_ids[FORGE_PROFILE_SCREEN_ROWS];
	struct forge_profile_row profile_rows[FORGE_PROFILE_SCREEN_ROWS];
};

/* (on the heap, made when first wanted: the game's image on Android has no
room for its size) */
FORGE_HEAP_STATIC(struct forge_profile_screen_state, s_forge_profile_screen)
#define s_forge_profile_screen FORGE_HEAP(s_forge_profile_screen)

static int forge_FocusRowIndexOfList(struct forge_widget_instance *list, struct forge_widget_instance **out_rows)
{
	struct forge_widget_instance *c;
	int r = 0, focused_r = -1;
	for (c = list ? list->child : NULL; c; c = c->next)
	{
		if (c->name && strncmp(c->name, "list_item_", 10) == 0 && r < 10)
		{
			if (out_rows)
				out_rows[r] = c;
			if (c == list->focused_child)
				focused_r = r;
			r++;
		}
	}
	return focused_r;
}

static void forge_handle_10row_edge_scroll(
	struct forge_widget_instance *list,
	struct forge_widget_instance **rows,
	int focused_r,
	int *io_prev_focus,
	int *io_first,
	int *io_chosen,
	int total_count)
{
	if (total_count <= 0)
	{
		*io_first = 0;
		*io_chosen = 0;
		*io_prev_focus = focused_r;
		return;
	}
	if (focused_r >= 0)
	{
		/* Check if user pressed UP from row 0 or DOWN from row 9 */
		if (*io_prev_focus == 0 && focused_r != 0 && focused_r != 1 && *io_first > 0)
		{
			/* Focus wrapped from row 0 up to spinner -> scroll list up and keep focus on row 0 */
			(*io_first)--;
			list->focused_child = rows[0];
			focused_r = 0;
		}
		else if (*io_prev_focus == 9 && focused_r != 9 && focused_r != 8 && (*io_first + 10) < total_count)
		{
			(*io_first)++;
			list->focused_child = rows[9];
			focused_r = 9;
		}
		else if (focused_r == 0 && *io_prev_focus == 1 && *io_first > 0 && (*io_first + focused_r) > 0)
		{
			/* Reached top visible row while more items exist above: scroll up */
			(*io_first)--;
			list->focused_child = rows[1];
			focused_r = 1;
		}
		else if (focused_r == 9 && *io_prev_focus == 8 && (*io_first + 10) < total_count)
		{
			/* Reached bottom visible row while more items exist below: scroll down */
			(*io_first)++;
			list->focused_child = rows[8];
			focused_r = 8;
		}

		if (*io_first + focused_r >= total_count)
		{
			int valid_r = total_count - 1 - *io_first;
			if (valid_r < 0) valid_r = 0;
			if (valid_r > 9) valid_r = 9;
			if (rows[valid_r])
				list->focused_child = rows[valid_r];
			focused_r = valid_r;
		}
		*io_chosen = *io_first + focused_r;
	}
	else if (*io_prev_focus == 9 && list->focused_child && list->focused_child->name &&
			 strcmp(list->focused_child->name, "forge_browser_button_bar") == 0 &&
			 (*io_first + 10) < total_count)
	{
		(*io_first)++;
		list->focused_child = rows[9];
		focused_r = 9;
		*io_chosen = *io_first + 9;
	}

	if (*io_chosen < 0) *io_chosen = 0;
	if (*io_chosen >= total_count) *io_chosen = total_count - 1;
	*io_prev_focus = focused_r;
}

/* The Tag Injector's profiles, for the TAG PROFILE screen: how many rows it
would have past NONE (0: it is skipped) */
extern long port_tag_profiles_scan(void);
extern long port_tag_profiles_count(void);
extern long port_tag_profiles_tag_count(long profile);
extern int port_tag_profiles_tag_enabled(long profile, long tag);
extern char const *port_tag_profiles_name(long profile);
extern char const *port_tag_profiles_folder(long profile);
extern void forge_tag_profile_set(char const *folder);
extern void tag_multiplayer_game_forge_profile_set(long profile);
extern int tag_pack_ready(char const *name);
extern void tag_pack_request(char const *name);
extern void platform_show_message(char const *title, char const *message);

static int forge_tag_profiles_scan(void)
{
	long count, profile;

	s_forge_profile_screen.profile_count = 1;
	s_forge_profile_screen.profile_ids[0] = NONE;
	snprintf(s_forge_profile_screen.profile_rows[0].display_row, sizeof(s_forge_profile_screen.profile_rows[0].display_row), "NONE");
	if (!halo_custom_edition_tag_cache())
		return 0;
	port_tag_profiles_scan();
	count = port_tag_profiles_count();
	for (profile = 0; profile < count && s_forge_profile_screen.profile_count < FORGE_PROFILE_SCREEN_ROWS; profile++)
	{
		long tag, tags = port_tag_profiles_tag_count(profile);
		struct forge_profile_row *row;

		for (tag = 0; tag < tags && !port_tag_profiles_tag_enabled(profile, tag); tag++)
		{
		}
		if (tag == tags)
			continue;
		row = &s_forge_profile_screen.profile_rows[s_forge_profile_screen.profile_count];
		s_forge_profile_screen.profile_ids[s_forge_profile_screen.profile_count++] = profile;
		snprintf(row->display_row, sizeof(row->display_row), "%s", port_tag_profiles_name(profile));
	}
	return s_forge_profile_screen.profile_count - 1;
}

/* whether Forge's tags are there to start with: the pack's written (the
menus wait while it is); a pack that could not be is asked for again, and
said */
static int forge_tags_ready(void)
{
	if (tag_pack_ready("watanoge_forge"))
		return 1;
	tag_pack_request("watanoge_forge");
	platform_show_message("Halo: Forge", "Forge's tags are not ready: they are built from the game's maps at the main menu.");
	ui_play_audio_feedback_sound(4);
	return 0;
}

/* (network_client_manager.c's) a Forge game can be joined with the tags
to place in it: asked for, and said, otherwise */
int forge_join_tags_missing(int tell)
{
	if (!g_forge_session_enabled || tag_pack_ready("watanoge_forge"))
		return 0;
	if (tell)
		forge_tags_ready();
	return 1;
}

/* the level a Forge session starts once its profile is chosen (the map, a
Single Player or Edit Maps start), or Server Setup (a multiplayer one's) */
enum
{
	_forge_start_none,
	_forge_start_solo,
	_forge_start_multiplayer
};

static int s_forge_start_kind;
static char s_forge_start_map[128];

static boolean forge_start(void **widget_ptr, boolean *widget_deleted)
{
	if (s_forge_start_kind == _forge_start_multiplayer)
	{
		s_forge_start_kind = _forge_start_none;
		return ui_widget_port_open(
			widget_ptr,
			"pc\\main_menu\\multiplayer_type_select\\server_settings\\server_settings_screen",
			widget_deleted);
	}
	s_forge_start_kind = _forge_start_none;
	dispose_global_network_game_client();
	dispose_global_network_game_server();
	network_game_accept_remote_connections(FALSE);
	custom_edition_maps_look_again();
	player_ui_set_single_player_local_player_controller(0, 0);
	main_set_map_name(s_forge_start_map);
	main_disallow_persistent_storage();
	main_set_difficulty(1);
	game_connection_set(0);
	main_menu_switch_to_single_player();
	player_ui_remember_player1_profile(TRUE);
	ui_play_audio_feedback_sound(2);
	return TRUE;
}

/* a Forge session's start: the map chosen, then the TAG PROFILE screen when
there are profiles to choose among, and the start itself once one is */
static boolean forge_start_ask(void **widget_ptr, boolean *widget_deleted, int kind, char const *map)
{
	if (!forge_tags_ready())
		return FALSE;
	s_forge_start_kind = kind;
	snprintf(s_forge_start_map, sizeof(s_forge_start_map), "%s", map ? map : "");
	forge_tag_profile_set("");
	if (kind == _forge_start_multiplayer)
		tag_multiplayer_game_forge_profile_set(NONE);
	if (forge_tag_profiles_scan())
	{
		ui_play_audio_feedback_sound(2);
		return ui_widget_port_open(widget_ptr, "pc\\main_menu\\forge_select\\forge_tag_profile_screen", widget_deleted);
	}
	return forge_start(widget_ptr, widget_deleted);
}

static void forge_populate_10row_tag_list(
	struct forge_widget_instance *const rows[10],
	struct forge_profile_row const *entries,
	int count,
	int first,
	int chosen,
	char const *empty_label)
{
	int r;
	for (r = 0; r < 10; r++)
	{
		struct forge_widget_instance *child = rows[r];
		int slot = first + r;
		if (!child) continue;
		if (count == 0)
		{
			child->visible = (r == 0) ? TRUE : FALSE;
			if (r == 0)
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), empty_label);
			if (fw_named(child, "list_item_arrows", 0))
				fw_named(child, "list_item_arrows", 0)->visible = FALSE;
		}
		else
		{
			child->visible = (slot < count) ? TRUE : FALSE;
			if (slot < count)
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), entries[slot].display_row);
			if (fw_named(child, "list_item_arrows", 0))
				fw_named(child, "list_item_arrows", 0)->visible = (slot == chosen);
		}
		if (fw_named(child, "scroll_up_button", 0))
			fw_named(child, "scroll_up_button", 0)->visible = (r == 0 && first > 0);
		if (fw_named(child, "scroll_down_button", 0))
			fw_named(child, "scroll_down_button", 0)->visible = (r == 9 && (first + 10) < count);
	}
}

static char const *const s_snap_autogen_names[4] = {
	"4-SIDE BOX",
	"2-END BRIDGE",
	"6-FACE CUBE",
	"4 BASE EDGES"
};

static char const *const s_snap_preset_names[NUMBER_OF_FORGE_SNAP_PRESETS] = {
	"CUSTOM OFFSET",
	"FRONT FACE",
	"BACK FACE",
	"LEFT FACE",
	"RIGHT FACE",
	"TOP FACE",
	"BOTTOM FACE",
	"FRONT BASE",
	"BACK BASE",
	"LEFT BASE",
	"RIGHT BASE"
};

static char const *const s_snap_dir_names[NUMBER_OF_FORGE_SNAP_DIRS] = {
	"FRONT (+X)",
	"RIGHT (-Y)",
	"BACK (-X)",
	"LEFT (+Y)",
	"UP (+Z)",
	"DOWN (-Z)"
};

/* A tag's snap points, each tag's own: the Tag Injector's SNAP POINTS list
(menu_functions.c) edits the tag shown, all of what is here saved at once.
Its rows are these, in this order. */
enum
{
	_forge_snap_row_layout,
	_forge_snap_row_add,
	_forge_snap_row_point,
	_forge_snap_row_preset,
	_forge_snap_row_x,
	_forge_snap_row_y,
	_forge_snap_row_z,
	_forge_snap_row_normal,
	_forge_snap_row_delete,
	_forge_snap_row_delete_all,
	NUMBER_OF_FORGE_SNAP_ROWS
};

static struct
{
	uint32_t group_tag;
	char tag_path[FORGE_TAG_PATH_MAX];
	int point;
	int layout;
	/* the box of its model is known (forge_snap_points_bounds_set) */
	int measured;
} s_snap_editor;

/* the tag whose box Object Properties' SNAP POINTS row read last, and the
layout it has been given */
static char s_snap_layout_tag[FORGE_TAG_PATH_MAX];
static int s_snap_layout = -1;

short forge_snap_editor_row_count(void)
{
	return NUMBER_OF_FORGE_SNAP_ROWS;
}

void forge_snap_editor_open(unsigned long group_tag, char const *tag_path)
{
	s_snap_editor.group_tag = (uint32_t)group_tag;
	snprintf(s_snap_editor.tag_path, sizeof(s_snap_editor.tag_path), "%s", tag_path ? tag_path : "");
	s_snap_editor.point = 0;
	s_snap_editor.layout = 0;
	s_snap_editor.measured = 0;
	s_snap_layout_tag[0] = 0;
}

/* the box of the tag's model, once its preview has read it */
void forge_snap_editor_measured(float const bounds_min[3], float const bounds_max[3])
{
	forge_snap_points_bounds_set(bounds_min, bounds_max);
	s_snap_editor.measured = 1;
}

static struct forge_tag_snap_config *forge_snap_editor_config(int create)
{
	struct forge_tag_snap_config *cfg =
		forge_snap_points_get_for_tag(s_snap_editor.group_tag, s_snap_editor.tag_path, create);

	if (cfg && s_snap_editor.point >= cfg->point_count)
		s_snap_editor.point = cfg->point_count > 0 ? cfg->point_count - 1 : 0;
	return cfg;
}

static struct forge_snap_point *forge_snap_editor_point(void)
{
	struct forge_tag_snap_config *cfg = forge_snap_editor_config(0);

	return cfg && cfg->point_count > 0 ? &cfg->points[s_snap_editor.point] : NULL;
}

int forge_snap_editor_tag_count(unsigned long group_tag, char const *tag_path)
{
	return forge_snap_points_count_for_tag((uint32_t)group_tag, tag_path);
}

int forge_snap_editor_count(void)
{
	struct forge_tag_snap_config const *cfg = forge_snap_editor_config(0);

	return cfg ? cfg->point_count : 0;
}

/* a row's name and its value */
void forge_snap_editor_row(short row, char *name, size_t name_size, char *state, size_t state_size)
{
	struct forge_snap_point const *pt = forge_snap_editor_point();
	int count = forge_snap_editor_count();

	state[0] = 0;
	switch (row)
	{
	case _forge_snap_row_layout:
		snprintf(name, name_size, "LAYOUT");
		snprintf(state, state_size, "%s", s_snap_autogen_names[s_snap_editor.layout & 3]);
		break;
	case _forge_snap_row_add:
		snprintf(name, name_size, "ADD POINT");
		snprintf(state, state_size, "%d OF %d", count, FORGE_MAX_SNAP_POINTS_PER_TAG);
		break;
	case _forge_snap_row_point:
		snprintf(name, name_size, "POINT");
		if (pt)
			snprintf(state, state_size, "%d OF %d", s_snap_editor.point + 1, count);
		else
			snprintf(state, state_size, "NONE");
		break;
	case _forge_snap_row_preset:
		snprintf(name, name_size, "PRESET");
		if (pt)
			snprintf(state, state_size, "%s",
				s_snap_preset_names[pt->preset_id < NUMBER_OF_FORGE_SNAP_PRESETS ? pt->preset_id : 0]);
		break;
	case _forge_snap_row_x:
		snprintf(name, name_size, "OFFSET FORWARD");
		if (pt)
			snprintf(state, state_size, "%+.2f", (double)pt->x);
		break;
	case _forge_snap_row_y:
		snprintf(name, name_size, "OFFSET SIDE");
		if (pt)
			snprintf(state, state_size, "%+.2f", (double)pt->y);
		break;
	case _forge_snap_row_z:
		snprintf(name, name_size, "OFFSET UP");
		if (pt)
			snprintf(state, state_size, "%+.2f", (double)pt->z);
		break;
	case _forge_snap_row_normal:
		snprintf(name, name_size, "FACING");
		if (pt)
			snprintf(state, state_size, "%s",
				s_snap_dir_names[pt->normal_dir < NUMBER_OF_FORGE_SNAP_DIRS ? pt->normal_dir : 0]);
		break;
	case _forge_snap_row_delete:
		snprintf(name, name_size, "DELETE POINT");
		break;
	default:
		snprintf(name, name_size, "DELETE ALL POINTS");
		break;
	}
}

/* left or right on a row (0: it has nothing to change, or needs the model's
box, which is still being read) */
int forge_snap_editor_step(short row, short dir)
{
	struct forge_tag_snap_config *cfg = forge_snap_editor_config(1);
	struct forge_snap_point *pt = forge_snap_editor_point();
	float step = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 ? 0.02f : 0.08f;

	if (!cfg)
		return 0;
	switch (row)
	{
	case _forge_snap_row_layout:
		if (!s_snap_editor.measured)
			return 0;
		s_snap_editor.layout = (s_snap_editor.layout + dir + 4) % 4;
		forge_snap_points_auto_generate(s_snap_editor.group_tag, s_snap_editor.tag_path, s_snap_editor.layout);
		s_snap_editor.point = 0;
		return 1;
	case _forge_snap_row_point:
		if (cfg->point_count <= 0)
			return 0;
		s_snap_editor.point = (s_snap_editor.point + dir + cfg->point_count) % cfg->point_count;
		return 1;
	case _forge_snap_row_preset:
	{
		int preset;

		if (!pt || !s_snap_editor.measured)
			return 0;
		preset = (int)pt->preset_id + dir;
		if (preset < 1) preset = NUMBER_OF_FORGE_SNAP_PRESETS - 1;
		if (preset >= NUMBER_OF_FORGE_SNAP_PRESETS) preset = 1;
		forge_snap_points_apply_preset_to_point(pt, preset, -1);
		break;
	}
	case _forge_snap_row_x:
	case _forge_snap_row_y:
	case _forge_snap_row_z:
		if (!pt)
			return 0;
		if (row == _forge_snap_row_x)
			pt->x += (float)dir * step;
		else if (row == _forge_snap_row_y)
			pt->y += (float)dir * step;
		else
			pt->z += (float)dir * step;
		pt->preset_id = _forge_snap_preset_custom;
		break;
	case _forge_snap_row_normal:
		if (!pt)
			return 0;
		forge_snap_points_fill_normal_from_dir(pt, ((int)pt->normal_dir + dir + NUMBER_OF_FORGE_SNAP_DIRS) % NUMBER_OF_FORGE_SNAP_DIRS);
		break;
	default:
		return 0;
	}
	forge_snap_points_save_all();
	return 1;
}

/* A on a row: the layout made again, a point added or deleted, else as right */
int forge_snap_editor_activate(short row)
{
	struct forge_tag_snap_config *cfg = forge_snap_editor_config(1);

	if (!cfg)
		return 0;
	switch (row)
	{
	case _forge_snap_row_layout:
		if (!s_snap_editor.measured)
			return 0;
		forge_snap_points_auto_generate(s_snap_editor.group_tag, s_snap_editor.tag_path, s_snap_editor.layout);
		s_snap_editor.point = 0;
		return 1;
	case _forge_snap_row_add:
	{
		int added = cfg->point_count;

		if (!s_snap_editor.measured || added >= FORGE_MAX_SNAP_POINTS_PER_TAG)
			return 0;
		cfg->point_count++;
		forge_snap_points_apply_preset_to_point(&cfg->points[added], (added % 6) + 1, -1);
		s_snap_editor.point = added;
		break;
	}
	case _forge_snap_row_delete:
	{
		int index;

		if (cfg->point_count <= 0)
			return 0;
		for (index = s_snap_editor.point; index + 1 < cfg->point_count; index++)
			cfg->points[index] = cfg->points[index + 1];
		cfg->point_count--;
		forge_snap_editor_config(0);
		break;
	}
	case _forge_snap_row_delete_all:
		if (cfg->point_count <= 0)
			return 0;
		cfg->point_count = 0;
		s_snap_editor.point = 0;
		break;
	default:
		return forge_snap_editor_step(row, 1);
	}
	forge_snap_points_save_all();
	return 1;
}

/* the points, for the preview to mark, and which is the one chosen */
short forge_snap_editor_points(float (*points)[3], short maximum, short *chosen)
{
	struct forge_tag_snap_config const *cfg = forge_snap_editor_config(0);
	short count = 0;

	*chosen = (short)s_snap_editor.point;
	for (; cfg && count < cfg->point_count && count < maximum; count++)
	{
		points[count][0] = cfg->points[count].x;
		points[count][1] = cfg->points[count].y;
		points[count][2] = cfg->points[count].z;
	}
	return count;
}

/* Object Properties' SNAP POINTS row, for the stock tags, which have no page
in the Tag Injector: left and right choose among none and the layouts, for the
tag of the object held. The box is read from its map once (0: it cannot be). */
int forge_snap_tag_layout_step(uint32_t group_tag, char const *tag_path, int dir)
{
	if (_stricmp(s_snap_layout_tag, tag_path) != 0)
	{
		if (!forge_snap_points_read_stock_bounds(group_tag, tag_path))
			return 0;
		snprintf(s_snap_layout_tag, sizeof(s_snap_layout_tag), "%s", tag_path);
		s_snap_layout = -1;
	}
	s_snap_layout = ((s_snap_layout + 1 + dir + 5) % 5) - 1;
	if (s_snap_layout >= 0)
	{
		forge_snap_points_auto_generate(group_tag, tag_path, s_snap_layout);
	}
	else
	{
		struct forge_tag_snap_config *cfg = forge_snap_points_get_for_tag(group_tag, tag_path, 0);

		if (cfg)
		{
			cfg->point_count = 0;
			forge_snap_points_save_all();
		}
	}
	return 1;
}

void forge_snap_tag_layout_text(uint32_t group_tag, char const *tag_path, char *text, size_t text_size)
{
	int count = forge_snap_points_count_for_tag(group_tag, tag_path);

	if (count <= 0)
		snprintf(text, text_size, "NONE");
	else if (s_snap_layout >= 0 && _stricmp(s_snap_layout_tag, tag_path) == 0)
		snprintf(text, text_size, "%s", s_snap_autogen_names[s_snap_layout]);
	else
		snprintf(text, text_size, "%d POINTS", count);
}

static void update_forge_tag_profile_widget(struct forge_widget_instance *list)
{
	struct forge_widget_instance *rows[10] = { NULL };
	int focused_r = forge_FocusRowIndexOfList(list, rows);

	forge_handle_10row_edge_scroll(
		list, rows, focused_r,
		&s_forge_profile_screen.profile_prev_row_focus,
		&s_forge_profile_screen.profile_first,
		&s_forge_profile_screen.profile_chosen,
		s_forge_profile_screen.profile_count);
	forge_populate_10row_tag_list(
		rows,
		s_forge_profile_screen.profile_rows,
		s_forge_profile_screen.profile_count,
		s_forge_profile_screen.profile_first,
		s_forge_profile_screen.profile_chosen,
		"NONE");
}

static unsigned long s_forge_name_modal_active_ms = 0;

boolean forge_player_profile_get(long player_profile_index, struct player_profile *profile)
{
	boolean ok = player_profile_get(player_profile_index, profile);
	if (ok && profile && g_forge_session_enabled)
	{
		int lv;
		for (lv = 0; lv < NUMBER_OF_SINGLE_PLAYER_LEVELS; lv++)
			profile->single_player_map_flags[lv] |= 1;
	}
	return ok;
}

/* the game variant is a Forge game's; never in the firing range, played
alone with whatever variant the last game left */
static int forge_variant_named_forge(struct game_variant const *v)
{
	return v &&
		(_wcsicmp(v->human_readable_game_description, L"Forge") == 0 ||
		 v->universal_variant.score_to_win == 0x3FFFFFFF);
}

static int forge_variant_is_forge(void)
{
	extern boolean firing_range_requested(void);

	/* (a joined machine is in a Forge session only as its own menus made it: a host's variant named Forge never puts it in one) */
	return !firing_range_requested() && game_connection() != _game_connection_network_client && forge_variant_named_forge(game_engine_get_variant()); /* AE hook */
}

/* the gametype the menus had before a Forge game put its own in their place
(mp level select), given back as the player leaves Forge's menus: else the
next game hosted without choosing a gametype (a LOCAL game, a lobby's) was
the Forge one, and menu_tags_loaded turned Forge on in it */
extern boolean player_ui_game_variant_specified(struct game_variant *variant);
extern void player_ui_set_game_variant(struct game_variant *variant);
extern void player_ui_set_game_variant_options(struct game_variant_options const *options);
extern struct game_variant_options const *player_ui_get_game_variant_options(void);
extern void game_engine_override_game_variant(struct game_variant const *variant);

static struct
{
	int kept;
	struct game_variant variant;
	struct game_variant_options options;
} s_pre_forge_variant;

static void forge_variant_keep_previous(void)
{
	struct game_variant current;

	if (player_ui_game_variant_specified(&current) && !forge_variant_named_forge(&current))
	{
		struct game_variant_options const *options = player_ui_get_game_variant_options();

		s_pre_forge_variant.variant = current;
		if (options)
			s_pre_forge_variant.options = *options;
		else
			game_variant_options_default(&current, &s_pre_forge_variant.options);
		s_pre_forge_variant.kept = 1;
	}
}

/* (the menus' and the next round's only: a server still up keeps its own
until the game it hosts is set up again) */
static void forge_variant_give_back(void)
{
	struct game_variant current;

	if (!player_ui_game_variant_specified(&current) || !forge_variant_named_forge(&current))
		return;
	if (!s_pre_forge_variant.kept)
	{
		build_game_variant_slayer(&s_pre_forge_variant.variant);
		game_variant_options_default(&s_pre_forge_variant.variant, &s_pre_forge_variant.options);
	}
	player_ui_set_game_variant(&s_pre_forge_variant.variant);
	player_ui_set_game_variant_options(&s_pre_forge_variant.options);
	game_engine_override_game_variant(&s_pre_forge_variant.variant);
	s_pre_forge_variant.kept = 0;
}

boolean pc_menu_event_function_invoke(
	void *widget_ptr,
	void *event_ptr,
	long function_index,
	boolean *widget_deleted)
{
	struct forge_widget_instance *widget = (struct forge_widget_instance *)widget_ptr;
	char const *fn_name = pc_menu_function_name(function_index);
	char const *w_name = (widget && widget->name) ? widget->name : "";
	char const *p_name = (widget && widget->parent && widget->parent->name) ? widget->parent->name : "";

	/* Scope Forge session strictly to Multiplayer -> Forge flows */
	{
		extern boolean firefight_active(void);
		extern boolean multiplayer_firefight_mode(void);
		/* (Forge chosen comes first: Firefight's mode and variant are still
		set here, gamemode_select_forge clearing them after this) */
		if ((fn_name && strcmp(fn_name, "gamemode select forge") == 0) ||
			(fn_name && strcmp(fn_name, "gamemode select choose") == 0 && strcmp(w_name, "gm_forge_item") == 0))
		{
			forge_session_start(0, 1); /* AE hook */
			forge_runtime_init();
		}
		else if (firefight_active() || multiplayer_firefight_mode())
		{
			g_forge_session_enabled = 0;
		}
		else if (strncmp(w_name, "forge_", 6) == 0 || strncmp(p_name, "forge_", 6) == 0)
		{
			forge_session_start(0, 1); /* AE hook */
			forge_runtime_init();
		}
		else if (fn_name &&
			((strcmp(fn_name, "gamemode select choose") == 0 && strcmp(w_name, "gm_forge_item") != 0) ||
			 strcmp(fn_name, "gamemode select standard") == 0 ||
			 strcmp(fn_name, "gamemode select ranked") == 0 ||
			 strcmp(fn_name, "gamemode select firefight") == 0 ||
			 strcmp(fn_name, "new campaign decision") == 0 ||
			 strcmp(fn_name, "campaign menu init") == 0 ||
			 strcmp(fn_name, "port coop begin") == 0 ||
			 strcmp(fn_name, "port play init") == 0 ||
			 strcmp(fn_name, "firing range begin") == 0 ||
			 strcmp(fn_name, "mp type select") == 0 ||
			 strcmp(fn_name, "main menu intialize") == 0 ||
			 strcmp(fn_name, "main menu initialize") == 0))
		{
			g_forge_session_enabled = 0;
			forge_variant_give_back();
		}
	}

	/* Forge -> Edit Maps screen initialization & activation */
	if (fn_name && strcmp(fn_name, "load game menu init") == 0 &&
		strcmp(w_name, "forge_edit_maps_list") == 0)
	{
		forge_session_start(0, 1); /* AE hook */
		dispose_global_network_game_client();
		dispose_global_network_game_server();
		network_game_accept_remote_connections(FALSE);
		custom_edition_maps_look_again();
		scan_forge_edit_maps(0);
		if (fw_named(widget, "list_item_1", 0))
			widget->focused_child = fw_named(widget, "list_item_1", 0);
		return TRUE;
	}
	if (fn_name && strcmp(fn_name, "load game menu init") == 0 &&
		strcmp(w_name, "forge_tag_profile_list") == 0)
	{
		s_forge_profile_screen.profile_first = 0;
		s_forge_profile_screen.profile_chosen = 0;
		s_forge_profile_screen.profile_prev_row_focus = -1;
		if (fw_named(widget, "list_item_1", 0))
			widget->focused_child = fw_named(widget, "list_item_1", 0);
		return TRUE;
	}
	if (fn_name && strcmp(fn_name, "load game menu activated") == 0)
	{
		struct forge_widget_instance *anc;
		for (anc = widget; anc; anc = anc->parent)
		{
			if (!anc->name)
				continue;
			if (strcmp(anc->name, "forge_edit_maps_list") == 0)
			{
				if (s_forge_menu.count > 0 && s_forge_menu.chosen >= 0 && s_forge_menu.chosen < s_forge_menu.count)
				{
					char level_path[128];
					forge_session_start(0, 1); /* AE hook */
					snprintf(level_path, sizeof(level_path), "custom_maps\\%s", s_forge_menu.map_leaves[s_forge_menu.chosen]);
					return forge_start_ask(widget_ptr, widget_deleted, _forge_start_solo, level_path);
				}
				ui_play_audio_feedback_sound(4);
				return FALSE;
			}
			if (strcmp(anc->name, "forge_tag_profile_list") == 0)
			{
				int place = s_forge_profile_screen.profile_chosen;
				long profile;

				if (place < 0 || place >= s_forge_profile_screen.profile_count)
				{
					ui_play_audio_feedback_sound(4);
					return FALSE;
				}
				profile = s_forge_profile_screen.profile_ids[place];
				forge_tag_profile_set(profile == NONE ? "" : port_tag_profiles_folder(profile));
				if (s_forge_start_kind == _forge_start_multiplayer)
					tag_multiplayer_game_forge_profile_set(profile);
				return forge_start(widget_ptr, widget_deleted);
			}
		}
	}

	/* Refresh Custom Edition & saved Forge maps on Single Player level list init */
	if (fn_name && strcmp(fn_name, "initialize sp level list solo") == 0)
	{
		if (g_forge_session_enabled)
		{
			dispose_global_network_game_client();
			dispose_global_network_game_server();
			network_game_accept_remote_connections(FALSE);
		}
		return orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
	}

	if (fn_name && strcmp(fn_name, "solo level set map") == 0)
	{
		boolean res = orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);

		if (g_forge_session_enabled && res)
		{
			char const *cur = main_get_map_name();

			forge_start_ask(widget_ptr, widget_deleted, _forge_start_solo,
				(cur && cur[0]) ? cur : "levels\\test\\bloodgulch\\bloodgulch");
			return FALSE; /* Returning FALSE prevents opening difficulty_select_list_screen */
		}
		return res;
	}

	/* Point 7: Multiplayer Map Select (FORGE MAPS tab works in Standard/Ranked MP & Forge; Forge skips Gametype Select) */
	if (fn_name && strcmp(fn_name, "mp level select") == 0)
	{
		boolean res = orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);

		if (g_forge_session_enabled && res)
		{
			long gtypes[64];
			short last_gt = 0;
			struct game_variant forge_var;
			struct game_variant_options forge_opts;

			forge_variant_keep_previous();
			if (ui_widget_port_gametypes(gtypes, 64, &last_gt) > 0)
				ui_widget_port_gametype_choose(gtypes[0]);

			memset(&forge_var, 0, sizeof(forge_var));
			build_game_variant_slayer(&forge_var);
			ustrncpy(
				forge_var.human_readable_game_description,
				L"Forge",
				NUMBEROF(forge_var.human_readable_game_description) - 1);
			forge_var.universal_variant.teams = FALSE;
			forge_var.universal_variant.score_to_win = 0x3FFFFFFF; /* Endless Forge session: never ends on kills */
			forge_var.universal_variant.respawn_time = 90;
			forge_var.universal_variant.suicide_penalty = 0;

			game_variant_options_default(&forge_var, &forge_opts);
			forge_opts.time_limit = 0; /* No time limit */
			forge_opts.vehicle_set[0] = VARIANT_VEHICLE_SET_PC; /* All map vehicles enabled */
			forge_opts.vehicle_set[1] = VARIANT_VEHICLE_SET_PC;
			ui_widget_port_game_variant_set(&forge_var, &forge_opts);

			forge_start_ask(widget_ptr, widget_deleted, _forge_start_multiplayer, NULL);
			return FALSE; /* Returning FALSE prevents XML handler from opening gametype_select_screen_wrapper */
		}
		return res;
	}

	/* In-Game Escape Pause Menu -> "SAVE MAP.." button handler
	 * Note: ui_widget_event_handler_function_invoke subtracts PC_MENU_FUNCTION_BASE (0x100 = 256) before calling pc_menu_event_function_invoke */
	if (function_index == 0x7777 ||
		function_index == (0x7777 - 256) ||
		function_index == (0x7777 - 102) ||
		(widget && widget->definition_tag_index == s_save_forge_btn_tag && s_save_forge_btn_tag != NONE) ||
		(w_name && strcmp(w_name, "save_forge_button") == 0))
	{
		ui_widgets_close_all();
		if (widget_deleted)
			*widget_deleted = TRUE;
		ui_play_audio_feedback_sound(2);
		forge_trigger_save_map_action();
		return TRUE;
	}

	/* Settings -> Controls Setup ("FORGE" and "FORGE PAD" tabs) */
	if (fn_name &&
		(strcmp(fn_name, "controls screen init") == 0 ||
		 strcmp(fn_name, "controls screen defaults") == 0 ||
		 strcmp(fn_name, "controls screen change set") == 0 ||
		 strcmp(fn_name, "controls binding slot") == 0 ||
		 strcmp(fn_name, "controls begin binding") == 0))
	{
		extern boolean forge_controls_menu_event(struct forge_widget_instance *w, void *event_ptr, char const *fn_name, long function_index, boolean *widget_deleted);
		if (forge_controls_menu_event(widget, event_ptr, fn_name, function_index, widget_deleted))
			return TRUE;
	}

	return orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
}

/* ---------- Settings -> Controls Setup ("FORGE" & "FORGE PAD" Tabs) Implementation */

static struct
{
	int initialized;
	int32_t pending_vk[FORGE_BIND_COUNT];
	int32_t pending_gp[FORGE_BIND_COUNT];
	float pending_look_sens;
	float pending_fly_speed;
	int slot; /* 0 = Keyboard/Mouse, 1 = Controller */
	int capturing_action; /* -1 when idle, 0..FORGE_BIND_COUNT-1 when waiting for input */
	int capturing_slot;
	unsigned long capture_start_ms;
	int prev_vk_down[256];
	int prev_gp_down[FORGE_GP_COUNT];
} s_forge_controls_ui = { 0, { 0 }, { 0 }, 1.0f, 1.0f, 0, -1, 0, 0, { 0 }, { 0 } };

static char const *const s_forge_bind_labels[FORGE_BIND_COUNT] = {
	"FORGE / SPARTAN",
	"OBJECT PALETTE",
	"3RD-PERSON VIEW",
	"QUICK SAVE MAP",
	"ROTATE ITEM",
	"SNAP TO FLOOR",
	"SNAP TO SURFACE",
	"ROTATION SNAP",
	"OBJECT OPTIONS",
	"DELETE OBJECT",
	"COPY OBJECT",
	"OBJECT SETTINGS"
};

static struct forge_widget_instance *fw_screen_of(struct forge_widget_instance *w)
{
	while (w && w->parent)
		w = w->parent;
	return w;
}

static struct forge_widget_instance *fw_control_row(struct forge_widget_instance *w)
{
	for (; w; w = w->parent)
	{
		if (w->name && strncmp(w->name, "op_command_", 11) == 0)
			return w;
	}
	return NULL;
}

static int fw_controls_group(struct forge_widget_instance *w)
{
	struct forge_widget_instance *sp = fw_named(fw_screen_of(w), "group_spinner", 0);
	return sp ? (int)sp->parameters.list.selected_index : 0;
}

static void forge_controls_ui_ensure_loaded(void)
{
	int i;
	if (g_forge.bind_vk[0] == 0 && g_forge.bind_gp[0] == 0)
	{
		forge_controls_init_defaults();
		forge_controls_load();
	}
	if (!s_forge_controls_ui.initialized)
	{
		for (i = 0; i < FORGE_BIND_COUNT; i++)
		{
			s_forge_controls_ui.pending_vk[i] = g_forge.bind_vk[i];
			s_forge_controls_ui.pending_gp[i] = g_forge.bind_gp[i];
		}
		s_forge_controls_ui.pending_look_sens = (g_forge.controller_look_sens >= 0.25f) ? g_forge.controller_look_sens : 1.0f;
		s_forge_controls_ui.pending_fly_speed = (g_forge.controller_fly_speed >= 0.25f) ? g_forge.controller_fly_speed : 1.0f;
		s_forge_controls_ui.capturing_action = -1;
		s_forge_controls_ui.initialized = 1;
	}
}

static void forge_controls_snapshot_input(void)
{
	int vk, gp;
	float lx, ly, rx, ry, lt, rt;
	forge_poll_controller_state(&lx, &ly, &rx, &ry, &lt, &rt);
	for (vk = 1; vk < 255; vk++)
		s_forge_controls_ui.prev_vk_down[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
	for (gp = 1; gp < FORGE_GP_COUNT; gp++)
		s_forge_controls_ui.prev_gp_down[gp] = forge_gp_button_down(gp);
}

boolean forge_controls_menu_event(
	struct forge_widget_instance *widget,
	void *event_ptr,
	char const *fn_name,
	long function_index,
	boolean *widget_deleted)
{
	int group = fw_controls_group(widget);
	int i;

	if (strcmp(fn_name, "controls screen init") == 0)
	{
		s_forge_controls_ui.initialized = 0;
		forge_controls_ui_ensure_loaded();
		orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
		return TRUE;
	}
	if (strcmp(fn_name, "controls screen defaults") == 0)
	{
		forge_controls_ui_ensure_loaded();
		if (group == 4 || group == 5)
		{
			int32_t saved_vk[FORGE_BIND_COUNT];
			int32_t saved_gp[FORGE_BIND_COUNT];
			float saved_ls = g_forge.controller_look_sens;
			float saved_fs = g_forge.controller_fly_speed;
			for (i = 0; i < FORGE_BIND_COUNT; i++)
			{
				saved_vk[i] = g_forge.bind_vk[i];
				saved_gp[i] = g_forge.bind_gp[i];
			}
			forge_controls_init_defaults();
			for (i = 0; i < FORGE_BIND_COUNT; i++)
			{
				s_forge_controls_ui.pending_vk[i] = g_forge.bind_vk[i];
				s_forge_controls_ui.pending_gp[i] = g_forge.bind_gp[i];
				g_forge.bind_vk[i] = saved_vk[i];
				g_forge.bind_gp[i] = saved_gp[i];
			}
			s_forge_controls_ui.pending_look_sens = 1.0f;
			s_forge_controls_ui.pending_fly_speed = 1.0f;
			g_forge.controller_look_sens = saved_ls;
			g_forge.controller_fly_speed = saved_fs;
			s_forge_controls_ui.capturing_action = -1;
			return TRUE;
		}
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	if (strcmp(fn_name, "controls screen change set") == 0)
	{
		forge_controls_ui_ensure_loaded();
		for (i = 0; i < FORGE_BIND_COUNT; i++)
		{
			g_forge.bind_vk[i] = s_forge_controls_ui.pending_vk[i];
			g_forge.bind_gp[i] = s_forge_controls_ui.pending_gp[i];
		}
		g_forge.controller_look_sens = s_forge_controls_ui.pending_look_sens;
		g_forge.controller_fly_speed = s_forge_controls_ui.pending_fly_speed;
		forge_controls_save();
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	if (strcmp(fn_name, "controls binding slot") == 0)
	{
		forge_controls_ui_ensure_loaded();
		if (group == 4)
		{
			s_forge_controls_ui.slot = !s_forge_controls_ui.slot;
			orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
			return TRUE;
		}
		if (group == 5)
		{
			struct forge_widget_instance *row = fw_control_row(widget);
			int row_idx = row ? (atoi(row->name + 11) - 1) : -1;
			int dir = 1;
			if ((GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000) || forge_gp_button_down(FORGE_GP_DPAD_LEFT))
			{
				dir = -1;
			}
			else if (event_ptr)
			{
				short const *ev_s = (short const *)event_ptr;
				if ((ev_s[0] == 2 && ev_s[2] == 14) || (ev_s[0] == 0 && ev_s[2] < 0))
					dir = -1;
			}
			if (row_idx == 0)
			{
				s_forge_controls_ui.pending_look_sens += (float)dir * 0.25f;
				if (s_forge_controls_ui.pending_look_sens < 0.25f) s_forge_controls_ui.pending_look_sens = 0.25f;
				if (s_forge_controls_ui.pending_look_sens > 2.50f) s_forge_controls_ui.pending_look_sens = 2.50f;
				return TRUE;
			}
			if (row_idx == 1)
			{
				s_forge_controls_ui.pending_fly_speed += (float)dir * 0.25f;
				if (s_forge_controls_ui.pending_fly_speed < 0.25f) s_forge_controls_ui.pending_fly_speed = 0.25f;
				if (s_forge_controls_ui.pending_fly_speed > 2.50f) s_forge_controls_ui.pending_fly_speed = 2.50f;
				return TRUE;
			}
			s_forge_controls_ui.slot = 1;
			return TRUE;
		}
		s_forge_controls_ui.slot = !s_forge_controls_ui.slot;
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	if (strcmp(fn_name, "controls begin binding") == 0)
	{
		forge_controls_ui_ensure_loaded();
		if (group == 4)
		{
			struct forge_widget_instance *row = fw_control_row(widget);
			int row_idx = row ? (atoi(row->name + 11) - 1) : -1;
			if (row_idx >= 0 && row_idx < FORGE_BIND_COUNT)
			{
				s_forge_controls_ui.capturing_action = row_idx;
				s_forge_controls_ui.capturing_slot = s_forge_controls_ui.slot;
				s_forge_controls_ui.capture_start_ms = system_milliseconds();
				forge_controls_snapshot_input();
				return TRUE;
			}
		}
		else if (group == 5)
		{
			struct forge_widget_instance *row = fw_control_row(widget);
			int row_idx = row ? (atoi(row->name + 11) - 1) : -1;
			if (row_idx == 0)
			{
				s_forge_controls_ui.pending_look_sens += 0.25f;
				if (s_forge_controls_ui.pending_look_sens > 2.50f)
					s_forge_controls_ui.pending_look_sens = 0.25f;
				return TRUE;
			}
			if (row_idx == 1)
			{
				s_forge_controls_ui.pending_fly_speed += 0.25f;
				if (s_forge_controls_ui.pending_fly_speed > 2.50f)
					s_forge_controls_ui.pending_fly_speed = 0.25f;
				return TRUE;
			}
		}
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	return FALSE;
}

static void forge_controls_menu_update(struct forge_widget_instance *list)
{
	int group = fw_controls_group(list);
	struct forge_widget_instance *row;
	struct forge_widget_instance *focused_row = fw_control_row(list ? list->focused_child : NULL);
	float lx, ly, rx, ry, lt, rt;

	forge_controls_ui_ensure_loaded();

	if (group < 4)
	{
		s_forge_controls_ui.capturing_action = -1;
		for (row = list ? list->child : NULL; row; row = row->next)
		{
			if (row->name && strncmp(row->name, "op_command_", 11) == 0)
			{
				int idx = atoi(row->name + 11) - 1;
				if (idx >= 0 && idx < 7)
				{
					row->vertical_offset = (short)(103 + idx * 30);
				}
				else if (idx >= 7)
				{
					row->visible = FALSE;
					if (list->focused_child == row)
					{
						list->focused_child = list->child;
						list->parameters.list.selected_index = 0;
					}
				}
			}
		}
		return;
	}

	forge_poll_controller_state(&lx, &ly, &rx, &ry, &lt, &rt);

	/* Poll for new Keyboard/Mouse or Controller button press when capturing */
	if (s_forge_controls_ui.capturing_action >= 0 && s_forge_controls_ui.capturing_action < FORGE_BIND_COUNT)
	{
		unsigned long now_ms = system_milliseconds();
		int act = s_forge_controls_ui.capturing_action;
		int vk, gp;

		if (now_ms - s_forge_controls_ui.capture_start_ms >= 120)
		{
			int esc_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
			int del_down = ((GetAsyncKeyState(VK_DELETE) & 0x8000) || (GetAsyncKeyState(VK_BACK) & 0x8000)) != 0;

			if (esc_down && !s_forge_controls_ui.prev_vk_down[VK_ESCAPE])
			{
				s_forge_controls_ui.capturing_action = -1;
			}
			else if (del_down && (!s_forge_controls_ui.prev_vk_down[VK_DELETE] && !s_forge_controls_ui.prev_vk_down[VK_BACK]))
			{
				if (s_forge_controls_ui.capturing_slot == 0)
					s_forge_controls_ui.pending_vk[act] = 0;
				else
					s_forge_controls_ui.pending_gp[act] = FORGE_GP_NONE;
				s_forge_controls_ui.capturing_action = -1;
			}
			else
			{
				/* Check physical gamepad buttons first -> binds Controller column (slot 1) */
				for (gp = 1; gp < FORGE_GP_COUNT; gp++)
				{
					int down = forge_gp_button_down(gp);
					if (down && !s_forge_controls_ui.prev_gp_down[gp])
					{
						int other;
						for (other = 0; other < FORGE_BIND_COUNT; other++)
						{
							if (s_forge_controls_ui.pending_gp[other] == gp)
								s_forge_controls_ui.pending_gp[other] = FORGE_GP_NONE;
						}
						s_forge_controls_ui.pending_gp[act] = gp;
						s_forge_controls_ui.capturing_action = -1;
						break;
					}
				}

				/* Check keyboard & mouse buttons -> binds Key/Mouse column (slot 0) */
				if (s_forge_controls_ui.capturing_action >= 0)
				{
					for (vk = 1; vk < 254; vk++)
					{
						int down;
						if (vk == VK_ESCAPE || vk == VK_DELETE || vk == VK_BACK)
							continue;
						if (vk == VK_SHIFT || vk == VK_CONTROL || vk == 0x12 /* VK_MENU */)
							continue;
						if ((vk == VK_LBUTTON || vk == VK_RETURN) && (now_ms - s_forge_controls_ui.capture_start_ms < 260))
							continue;
						down = (GetAsyncKeyState(vk) & 0x8000) != 0;
						if (down && !s_forge_controls_ui.prev_vk_down[vk])
						{
							char const *vname = forge_vk_display_name(vk);
							if (vname && strcmp(vname, "-") != 0)
							{
								int other;
								for (other = 0; other < FORGE_BIND_COUNT; other++)
								{
									if (s_forge_controls_ui.pending_vk[other] == vk)
										s_forge_controls_ui.pending_vk[other] = 0;
								}
								s_forge_controls_ui.pending_vk[act] = vk;
								s_forge_controls_ui.capturing_action = -1;
								break;
							}
						}
					}
				}
			}
		}

		for (vk = 1; vk < 255; vk++)
			s_forge_controls_ui.prev_vk_down[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
		for (gp = 1; gp < FORGE_GP_COUNT; gp++)
			s_forge_controls_ui.prev_gp_down[gp] = forge_gp_button_down(gp);
	}

	/* Populate rows for group == 4 (FORGE) or group == 5 (FORGE PAD) */
	for (row = list ? list->child : NULL; row; row = row->next)
	{
		int idx;
		struct forge_widget_instance *lbl_w;
		struct forge_widget_instance *bind0_w;
		struct forge_widget_instance *bind1_w;
		char col0[64], col1[64];

		if (!row->name || strncmp(row->name, "op_command_", 11) != 0)
			continue;
		idx = atoi(row->name + 11) - 1;
		lbl_w = fw_named(row, "command_label", 0);
		bind0_w = fw_named(row, "command_binding", 0);
		bind1_w = fw_named(row, "command_binding", 1);

		if (group == 4)
		{
			if (idx < 0 || idx >= FORGE_BIND_COUNT)
			{
				row->visible = FALSE;
				continue;
			}
			row->visible = TRUE;
			row->vertical_offset = (short)(99 + idx * 24);
			fw_text_set_ascii(lbl_w, s_forge_bind_labels[idx]);

			if (s_forge_controls_ui.capturing_action == idx && s_forge_controls_ui.capturing_slot == 0)
				snprintf(col0, sizeof(col0), "PRESS KEY/PAD");
			else if (row == focused_row && s_forge_controls_ui.slot == 0)
				snprintf(col0, sizeof(col0), "> %s <", forge_vk_display_name(s_forge_controls_ui.pending_vk[idx]));
			else
				snprintf(col0, sizeof(col0), "%s", forge_vk_display_name(s_forge_controls_ui.pending_vk[idx]));

			if (s_forge_controls_ui.capturing_action == idx && s_forge_controls_ui.capturing_slot == 1)
				snprintf(col1, sizeof(col1), "PRESS KEY/PAD");
			else if (row == focused_row && s_forge_controls_ui.slot == 1)
				snprintf(col1, sizeof(col1), "> %s <", forge_gp_display_name(s_forge_controls_ui.pending_gp[idx]));
			else
				snprintf(col1, sizeof(col1), "%s", forge_gp_display_name(s_forge_controls_ui.pending_gp[idx]));

			fw_text_set_ascii(bind0_w, col0);
			fw_text_set_ascii(bind1_w, col1);
		}
		else if (group == 5)
		{
			if (idx == 0 || idx == 1)
			{
				int pct = (int)((idx == 0 ? s_forge_controls_ui.pending_look_sens : s_forge_controls_ui.pending_fly_speed) * 100.0f + 0.5f);
				row->visible = TRUE;
				row->vertical_offset = (short)(103 + idx * 30);
				fw_text_set_ascii(lbl_w, (idx == 0) ? "PAD LOOK SENS" : "PAD FLY SPEED");
				if (row == focused_row)
					snprintf(col0, sizeof(col0), "< %d%% >", pct);
				else
					snprintf(col0, sizeof(col0), "%d%%", pct);
				snprintf(col1, sizeof(col1), "LEFT / RIGHT");
				fw_text_set_ascii(bind0_w, col0);
				fw_text_set_ascii(bind1_w, col1);
			}
			else
			{
				row->visible = FALSE;
				if (list->focused_child == row)
				{
					list->focused_child = list->child;
					list->parameters.list.selected_index = 0;
				}
			}
		}
	}

	if (list && list->parameters.list.extended_description)
	{
		list->parameters.list.extended_description->parameters.text_box.string_list_index =
			(s_forge_controls_ui.capturing_action >= 0) ? 2 : (focused_row ? 1 : 0);
	}
}

void pc_menu_game_data_function_invoke(
	void *widget_ptr,
	long function)
{
	struct forge_widget_instance *widget = (struct forge_widget_instance *)widget_ptr;
	char const *fn_name = pc_menu_game_data_input_name(function);
	char const *w_name = (widget && widget->name) ? widget->name : "";
	char const *p_name = (widget && widget->parent && widget->parent->name) ? widget->parent->name : "";

	{
		extern boolean firefight_active(void);
		extern boolean multiplayer_firefight_mode(void);
		if (firefight_active() || multiplayer_firefight_mode())
		{
			g_forge_session_enabled = 0;
		}
		else if (strncmp(w_name, "forge_", 6) == 0 || strncmp(p_name, "forge_", 6) == 0)
		{
			forge_session_start(0, 1); /* AE hook */
		}
	}

	if (fn_name && strcmp(fn_name, "load game list update") == 0 &&
		strcmp(w_name, "forge_edit_maps_list") == 0)
	{
		update_forge_edit_maps_widget(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "load game list update") == 0 &&
		strcmp(w_name, "forge_tag_profile_list") == 0)
	{
		update_forge_tag_profile_widget(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "controls update menu") == 0)
	{
		int group = fw_controls_group(widget);
		if (group >= 4)
		{
			forge_controls_menu_update(widget);
			return;
		}
		orig_pc_menu_game_data_function_invoke(widget_ptr, function);
		forge_controls_menu_update(widget);
		return;
	}

	orig_pc_menu_game_data_function_invoke(widget_ptr, function);

	/* Point 7: Hide gametype rows on Server Setup screen when hosting a Multiplayer Forge session */
	if (g_forge_session_enabled && fn_name && strcmp(fn_name, "server settings update") == 0)
	{
		static char const *const gt_rows[] = {
			"op_game_type", "op_player_options", "op_item_options",
			"op_vehicle_options", "op_indicator_options", "op_team_options"
		};
		int r;
		for (r = 0; r < 6; r++)
		{
			struct forge_widget_instance *row_w = fw_named(widget, gt_rows[r], 0);
			if (row_w)
				row_w->visible = FALSE;
		}
	}
}

/* ---------- Frame Update, HUD, Camera, 343 Guilty Spark Model & Console Hooks */

static void forge_reset_transient_session_state(int clear_saved_map_state)
{
	g_forge.monitor_cam_initialized = 0;
	g_forge.grabbed_live_index = -1;
	g_forge.grabbed_object_index = NONE;
	g_forge.grabbed_is_new_spawn = 0;
	g_forge.hovered_object_index = NONE;
	g_forge.hovered_live_index = -1;
	g_forge.hovered_object_label[0] = 0;
	forge_3d_reset_session_tracking();
	if (clear_saved_map_state)
	{
		g_forge.live_instance_count = 0;
		g_forge.has_saved_map_once = 0;
		g_forge.save_map_name[0] = 0;
		g_forge.save_map_name_len = 0;
		memset(&g_forge.edit_state, 0, sizeof(g_forge.edit_state));
		forge_network_reset_session();
	}
}

int forge_is_main_menu_active(void)
{
	struct scenario *scen = global_scenario_try_and_get();
	char const *cur_map = forge_get_active_map_name();
	if (!scen || scen->type == 2 /* _scenario_type_main_menu */)
		return 1;
	if (cur_map && (_stricmp(cur_map, "levels\\ui\\ui") == 0 || _stricmp(cur_map, "ui") == 0))
		return 1;
	return 0;
}

void network_chat_update(void)
{
	unsigned long now = system_milliseconds();
	float dt = 0.016f;
	char const *cur_map = forge_get_active_map_name();

	orig_network_chat_update();

	/* When on the UI main menu (levels\ui\ui), main_load_ui_scenario leaves main_get_map_name()
	 * set to the previous playable map while global_scenario_try_and_get()->type == _scenario_type_main_menu (2).
	 * Reset Forge monitor/modal state and never run Forge camera/update hooks on the UI background scene! */
	if (forge_is_main_menu_active())
	{
		extern unsigned char firefight_stats_poll_pending_link_popup(
			char *out_code, size_t code_size, char *out_device_id, size_t id_size);
		char link_code[16];
		char dev_id[32];

		if (firefight_stats_poll_pending_link_popup(link_code, sizeof(link_code), dev_id, sizeof(dev_id)))
		{
			char text[256];
			static wchar_t msg[256];
			size_t c;

			snprintf(
				text,
				sizeof(text),
				"\rEnter Code on Halostats\r%s",
				link_code);
			for (c = 0; text[c] != '\0' && c + 1 < sizeof(msg) / sizeof(msg[0]); c++)
				msg[c] = (wchar_t)(unsigned char)text[c];
			msg[c] = 0;
			ui_play_audio_feedback_sound(2);
			display_error_text_deferred(msg, 0);
		}

		g_last_forge_map_name[0] = 0;
		g_forge.active_modal = _forge_modal_none;
		g_forge.mode = _forge_mode_play;
		forge_reset_transient_session_state(0);
		return;
	}

	/* Reset per-map monitor state only when switching between different active maps */
	if (cur_map && cur_map[0] && _stricmp(cur_map, g_last_forge_map_name) != 0)
	{
		int had_previous_map = (g_last_forge_map_name[0] != 0);
		snprintf(g_last_forge_map_name, sizeof(g_last_forge_map_name), "%s", cur_map);
		g_forge.active_modal = _forge_modal_none;
		if (had_previous_map)
			forge_reset_transient_session_state(1);
		forge_normalize_mcc_hud_tags();
	}

	if (g_last_forge_frame_ms != 0)
	{
		unsigned long diff = now - g_last_forge_frame_ms;
		if (diff > 0 && diff < 250)
			dt = (float)diff / 1000.0f;
	}
	g_last_forge_frame_ms = now;

	forge_3d_update(dt);
}

void network_chat_draw(short local_player_index)
{
	long font_index;
	struct font_header *font;
	short line_height = 13;
	rectangle2d window;

	orig_network_chat_draw(local_player_index);

	/* Never draw Forge HUD overlay on the main menu (levels\ui\ui) or outside a Forge session */
	if (!g_forge_session_enabled || forge_is_main_menu_active())
		return;

	font_index = hud_get_font_index();
	if (font_index != NONE)
	{
		font = font_definition_get(font_index);
		if (font)
			line_height = (short)(font->ascending_height + font->descending_height + font->leading_height);
	}
	if (line_height <= 0)
		line_height = 13;

	window = render.camera.window_bounds;
	offset_rectangle2d(&window, -render.camera.viewport_bounds.x0, -render.camera.viewport_bounds.y0);

	forge_3d_draw_hud(
		local_player_index,
		window.x0,
		window.y0,
		window.x1,
		window.y1,
		font_index,
		line_height);
}

static unsigned long s_forge_esc_cancel_ms = 0;

boolean network_chat_typing(void)
{
	if (orig_network_chat_typing())
		return TRUE;
	/* Block native keyboard/Escape input when a Forge modal is open or while holding a grabbed item in Monitor Mode */
	if (g_forge_session_enabled && g_forge.initialized &&
		!forge_is_main_menu_active())
	{
		if (g_forge.active_modal != _forge_modal_none ||
			forge_has_held_item_to_cancel() ||
			(s_forge_esc_cancel_ms != 0 && (system_milliseconds() - s_forge_esc_cancel_ms) < 250UL))
		{
			return TRUE;
		}
	}
	return FALSE;
}

void network_chat_handle_up(long machine_index, void const *entries, boolean from_stream)
{
	if (entries && from_stream)
	{
		char const *text = (char const *)entries + 4;
		if (forge_network_intercept_chat_packet(machine_index, text, 1))
			return;
	}
	orig_network_chat_handle_up(machine_index, entries, from_stream);
}

void network_chat_handle_down(void const *entries, boolean from_stream)
{
	if (entries && from_stream)
	{
		char const *text = (char const *)entries + 4;
		if (forge_network_intercept_chat_packet(NONE, text, 0))
			return;
	}
	orig_network_chat_handle_down(entries, from_stream);
}

void network_objects_handle_changes(void const *entries, short count)
{
	forge_network_pre_handle_object_changes(entries, count);
	orig_network_objects_handle_changes(entries, count);
	forge_network_post_handle_object_changes(entries, count);
}

void director_update(real time_delta_sec)
{
	orig_director_update(time_delta_sec);
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		forge_3d_apply_monitor_camera(0);
	}
}

short director_get_perspective(short local_player_index)
{
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		if (g_forge.mode == _forge_mode_3d_monitor && g_forge.third_person_view)
		{
			return _director_perspective_third_person; /* 3rd-person perspective so Monitor is drawn */
		}
	}
	return orig_director_get_perspective(local_player_index);
}

struct observer_result const *render_interpolation_camera(
	short local_player_index,
	struct observer_result const *fallback)
{
	struct observer_result const *base = orig_render_interpolation_camera(local_player_index, fallback);
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		forge_render_begin_monitor_visibility();
	}
	if (base && g_forge_session_enabled && g_forge.initialized &&
		!forge_is_main_menu_active() && local_player_index == 0 &&
		g_forge.mode == _forge_mode_3d_monitor)
	{
		static struct observer_result s_live_obs;
		s_live_obs = *base;
		if (forge_3d_fill_live_observer_result(&s_live_obs))
			return &s_live_obs;
	}
	return base;
}

void first_person_weapon_draw(void)
{
	if (g_forge_session_enabled && g_forge.initialized &&
		g_forge.mode != _forge_mode_play &&
		!forge_is_main_menu_active())
	{
		/* Hide player's first-person hands & weapon model when in Forge Mode */
		return;
	}
	orig_first_person_weapon_draw();
}

extern void rasterizer_set_stencil_mode(long stencil_mode);
extern void rasterizer_set_frustum_z(real z_near, real z_far);

void game_engine_post_rasterize_objects(void)
{
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		forge_render_end_monitor_visibility();
	}
	orig_game_engine_post_rasterize_objects();

	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		/* Render 3D Translucent Objective/Spawn/Teleporter Markers and 3D Spawn Menu Preview
		 * AFTER sky, opaque objects, BSP lightmaps, BSP diffuse textures, and fog have been drawn!
		 * Reset stencil mode to NONE and restore camera frustum constants so world markers never get rejected! */
		rasterizer_set_stencil_mode(0);
		rasterizer_set_frustum_z(0.0f, 0.0f);
		rasterizer_models_begin(FALSE);
		forge_render_world_3d_overlays(NULL);
		rasterizer_models_end();
	}
}

void hud_render_weapon_interface(struct player_datum *player)
{
	if (g_forge_session_enabled && g_forge.initialized &&
		g_forge.mode != _forge_mode_play &&
		!forge_is_main_menu_active())
	{
		/* Hide weapon HUD, weapon reticle, and 'No Weapon' prompt in Forge mode (unarmed reticle drawn in forge_3d_draw_hud) */
		return;
	}
	orig_hud_render_weapon_interface(player);
}

extern void orig_hud_draw_bitmap_direct(
	struct bitmap_data const *bitmap,
	short placement,
	point2d const *point,
	real_rectangle2d const *clip,
	real scale,
	real theta,
	pixel32 color,
	boolean is_interface_bitmap);

extern void orig_hud_draw_numbers(
	short local_player_index,
	struct hud_absolute_placement_definition const *absolute_placement,
	struct number_hud_element_definition const *numbers,
	short value,
	short decimal_value,
	short draw_flags,
	long flash_reference_time,
	real override_scale);

static int s_forge_in_hud_draw_numbers = 0;

void hud_draw_numbers(
	short local_player_index,
	struct hud_absolute_placement_definition const *absolute_placement,
	struct number_hud_element_definition const *numbers,
	short value,
	short decimal_value,
	short draw_flags,
	long flash_reference_time,
	real override_scale)
{
	s_forge_in_hud_draw_numbers = 1;
	orig_hud_draw_numbers(
		local_player_index,
		absolute_placement,
		numbers,
		value,
		decimal_value,
		draw_flags,
		flash_reference_time,
		override_scale);
	s_forge_in_hud_draw_numbers = 0;
}

void hud_draw_bitmap_direct(
	struct bitmap_data const *bitmap,
	short placement,
	point2d const *point,
	real_rectangle2d const *clip,
	real scale,
	real theta,
	pixel32 color,
	boolean is_interface_bitmap)
{
	if (forge_is_mcc_hud_active() && bitmap)
	{
		uint16_t w = *(uint16_t const *)((uint8_t const *)bitmap + 0x04);
		uint16_t h = *(uint16_t const *)((uint8_t const *)bitmap + 0x06);
		if (!(w >= 512u && h >= 512u) && (!s_forge_in_hud_draw_numbers || scale > 0.6f))
			scale *= 0.5f;
	}
	orig_hud_draw_bitmap_direct(
		bitmap,
		placement,
		point,
		clip,
		scale,
		theta,
		color,
		is_interface_bitmap);
}

void render_model(
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
	unsigned long flags)
{
	if (unique_identifier != NONE && firefight_is_generator_hidden_biped(unique_identifier))
		return;

	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		if (unique_identifier != NONE)
		{
			/* Point 10: Hide floating gun or child items attached to any player biped that is in Monitor mode */
			{
				struct object_datum *obj = (struct object_datum *)object_try_and_get_and_verify_type(unique_identifier, (unsigned long)-1);
				if (obj && obj->object.parent_object_index != NONE &&
					forge_is_unit_in_monitor_mode(obj->object.parent_object_index))
				{
					return;
				}
			}
			if (g_forge.mode == _forge_mode_3d_monitor &&
				!g_forge.third_person_view &&
				player_control_get_unit_index(0) == unique_identifier)
			{
				return;
			}

			/* Render dynamic Cached Tag objects and Monitor player units */
			if (forge_monitor_render_player_unit(unique_identifier, lighting, flags))
				return;

			/* For the currently held Forge object, bypass 30 Hz render interpolation and use live 60+ Hz node matrices */
			if (unique_identifier == g_forge.grabbed_object_index && g_forge.grabbed_live_index >= 0)
			{
				struct object_datum *grab_obj = (struct object_datum *)object_try_and_get_and_verify_type(unique_identifier, (unsigned long)-1);
				if (grab_obj && grab_obj->object.node_matrices.size > 0)
				{
					struct real_matrix4x3 const *live_mats =
						(struct real_matrix4x3 const *)object_header_block_get(unique_identifier, &grab_obj->object.node_matrices);
					if (live_mats)
						node_matrices = live_mats;
				}
			}
		}
	}

	/* Ensure custom scenery/device models inside .map files never disappear at distance due to
	 * non-zero super_low detail_cutoff_pixels[0] or missing low-LOD permutation geometry_indices */
	if (unique_identifier != NONE && model_index != NONE)
	{
		struct object_datum *scen_obj = (struct object_datum *)object_try_and_get_and_verify_type(
			unique_identifier,
			_object_mask_scenery | _object_mask_machine | _object_mask_light_fixture | _object_mask_control);
		if (scen_obj)
		{
			struct model *m = model_definition_get(model_index);
			if (m)
			{
				if (m->detail_cutoff_pixels[0] > 0.0f)
					m->detail_cutoff_pixels[0] = 0.0f;
				if (m->geometries.count <= 1 && level_of_detail_pixels < m->detail_cutoff_pixels[4] + 1.0f)
					level_of_detail_pixels = m->detail_cutoff_pixels[4] + 1.0f;
				if (level_of_detail_pixels < 32.0f)
					level_of_detail_pixels = 32.0f;
			}
		}
	}

	orig_render_model(
		model_index,
		level_of_detail_pixels,
		node_matrices,
		region_permutation_indices,
		change_colors,
		function_values,
		lighting,
		centroid,
		radius,
		model_effect,
		unique_identifier,
		forced_shader_permutation_index,
		flags);
}

boolean hs_compile_and_evaluate(char const *source)
{
	if (source)
	{
		char const *s = source;
		while (*s == ' ' || *s == '\t')
			s++;
		if (_stricmp(s, "third_person") == 0 ||
			_stricmp(s, "forge_third_person") == 0 ||
			_stricmp(s, "3rd_person") == 0)
		{
			g_forge.third_person_view = !g_forge.third_person_view;
			console_printf(
				FALSE,
				"Forge 3rd-Person View: %s",
				g_forge.third_person_view ? "ON" : "OFF");
			forge_set_status(
				"3rd-Person Forge View: %s",
				g_forge.third_person_view ? "ON" : "OFF");
			return TRUE;
		}
		if (_stricmp(s, "forge") == 0 || _stricmp(s, "forge_mode") == 0)
		{
			forge_session_start(0, 1); /* AE hook */
			g_forge.mode = (g_forge.mode == _forge_mode_play) ? _forge_mode_3d_monitor : _forge_mode_play;
			console_printf(
				FALSE,
				"Mode: %s",
				g_forge.mode == _forge_mode_3d_monitor ? "Forge Mode" : "Spartan Play");
			return TRUE;
		}
	}
	return orig_hs_compile_and_evaluate(source);
}

/* ---------- Point 1: Singleplayer Maps Load Straight In (No Cutscenes, No Script Trigger Points) */

void hs_runtime_update(void)
{
	if (g_forge_session_enabled && !forge_is_main_menu_active())
	{
		/* Suppress scenario startup/continuous scripts in Forge mode so SP maps load straight in with no cutscenes */
		return;
	}
	orig_hs_runtime_update();
}

void cinematic_start(void)
{
	if (g_forge_session_enabled && !forge_is_main_menu_active())
	{
		return;
	}
	orig_cinematic_start();
}

boolean scenario_trigger_volume_test_object(short trigger_volume_index, long object_index)
{
	if (g_forge_session_enabled && !forge_is_main_menu_active())
	{
		/* Suppress all script trigger volumes and BSP switch trigger volumes in Forge mode */
		return FALSE;
	}
	return orig_scenario_trigger_volume_test_object(trigger_volume_index, object_index);
}

/* ---------- Point 5: Bypass Multiplayer Gametype Vehicle/Weapon Remapping in Forge Mode */

extern boolean custom_edition_cache_tags_loaded(void);

long game_engine_remap_object_definition(long definition_index)
{
	if (g_forge_session_enabled || g_forge.has_saved_map_once || custom_edition_cache_tags_loaded())
	{
		/* Never remap or reject Scorpion, Banshee, Rocket Warthog, Shade Turret, or custom Forge vehicles! */
		return definition_index;
	}
	return orig_game_engine_remap_object_definition(definition_index);
}

/* ---------- Point 2 & 6: In-Game Escape Pause Menu "SAVE FORGE MAP" Patch & Map Unload Cleanup */

struct forge_pause_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name; /* AE hook */
	XPTR(void) base_address; /* AE hook */
	unsigned long unused[2];
};

static void *s_pause_saved_instances = NULL;
static long s_pause_saved_count = 0;
static void *s_pause_alloc_instances = NULL;

void menu_tags_unloaded(void)
{
	forge_runtime_on_map_unload();
	s_save_forge_btn_tag = NONE;
	g_last_forge_map_name[0] = 0;
	g_forge.monitor_cam_initialized = 0;
	g_forge.live_instance_count = 0;
	forge_3d_history_clear();
	g_forge.grabbed_live_index = -1;
	g_forge.grabbed_object_index = NONE;
	g_forge.hovered_live_index = -1;
	g_forge.hovered_object_index = NONE;
	if (s_pause_saved_instances)
	{
		cache_files_set_tag_instances(s_pause_saved_instances, s_pause_saved_count);
		s_pause_saved_instances = NULL;
		s_pause_saved_count = 0;
	}
	if (s_pause_alloc_instances)
	{
		free(s_pause_alloc_instances);
		s_pause_alloc_instances = NULL;
	}
	orig_menu_tags_unloaded();
}

/* the level loading is a Forge game's: by the test menu_tags_loaded makes just
after its tags, which a client that joined one has not made yet */
int forge_level_is_session(void)
{
	return g_forge_session_enabled || forge_variant_is_forge();
}

void menu_tags_loaded(char const *map_name)
{
	orig_menu_tags_loaded(map_name);

	s_save_forge_btn_tag = NONE;
	g_last_forge_map_name[0] = 0;
	g_forge.live_instance_count = 0;
	forge_3d_history_clear();

	if (map_name && (_stricmp(map_name, "ui") == 0 || _stricmp(map_name, "levels\\ui\\ui") == 0))
	{
		forge_reset_transient_session_state(0);
		g_forge_session_enabled = 0;
		/* (back at the menus from a Forge game: the gametype from before it,
		else the next game started without one chosen through the menus that
		give it back - a LOCAL game, the PLAY menu's - loaded as Forge again.
		A Forge lobby still hosted between its rounds keeps its own) */
		if (!network_game_is_active())
			forge_variant_give_back();
		g_forge.mode = _forge_mode_play;
		g_forge.active_modal = _forge_modal_none;
		forge_menu_custom_edition_maps_look_again();
	}
	else
	{
		if (!g_forge_session_enabled && forge_variant_is_forge())
		{
			forge_session_start(0, 1); /* AE hook */
		}
		forge_reset_transient_session_state(1);
		if (game_connection() == 0 /* _game_connection_local */ || !network_game_is_active())
		{
			dispose_global_network_game_client();
			dispose_global_network_game_server();
			network_game_accept_remote_connections(FALSE);
		}
	}

	/* (an MCC level's pause menus are MCC's own, port/linux/game/mcc_pause.c,
	   which have their SAVE MAP.. in a Forge session) */
	{
		extern void mcc_pause_forge(boolean session);
		mcc_pause_forge(g_forge_session_enabled && map_name && strcmp(map_name, "ui") != 0);
	}

#ifndef HALO_64BIT /* AE hook */
	/* Patch "SAVE FORGE MAP" button into the in-game Escape Pause Menu when in a Forge session */
	if (g_forge_session_enabled && map_name && strcmp(map_name, "ui") != 0)
	{
		long settings_btn_tag = tag_loaded('DeLa', "pc\\pause\\settings_button");

		forge_runtime_markers_preload();
		long existing_count = 0;
		struct forge_pause_tag_instance *instances =
			(struct forge_pause_tag_instance *)cache_files_tag_instances(&existing_count);

		if (settings_btn_tag != NONE && instances && existing_count > 0 && existing_count < 65000)
		{
			uint8_t const *settings_def = (uint8_t const *)tag_get('DeLa', settings_btn_tag);
			struct forge_pause_tag_instance *grown =
				(struct forge_pause_tag_instance *)malloc((size_t)(existing_count + 2) * sizeof(*grown));
			uint8_t *btn_def = (uint8_t *)malloc(0x3EC);
			struct tag_block *ustr_def = (struct tag_block *)calloc(1, sizeof(struct tag_block));
			struct tag_data *ustr_entry = (struct tag_data *)calloc(1, sizeof(struct tag_data));
			wchar_t *wtext = (wchar_t *)calloc(32, sizeof(wchar_t));

			if (grown && btn_def && ustr_def && ustr_entry && wtext && settings_def)
			{
				static char const *const label = "SAVE MAP..";
				long max_salt = 0xE174;
				long i, ustr_tag, btn_tag;
				int32_t h_count;
				uint8_t const *h_src;

				for (i = 0; label[i]; i++)
					wtext[i] = (wchar_t)(unsigned char)label[i];
				wtext[i] = 0;

				ustr_entry->size = (long)((i + 1) * sizeof(wchar_t));
				ustr_entry->address = wtext;
				ustr_def->count = 1;
				ustr_def->address = ustr_entry;

				memcpy(grown, instances, (size_t)existing_count * sizeof(*grown));
				for (i = 0; i < existing_count; i++)
				{
					if (instances[i].tag_index != NONE)
					{
						long s = (long)(((unsigned long)instances[i].tag_index >> 16) & 0x7FFFu);
						if (s > max_salt)
							max_salt = s;
					}
				}

				ustr_tag = ((max_salt + 1) << 16) | (existing_count & 0xFFFF);
				btn_tag = ((max_salt + 2) << 16) | ((existing_count + 1) & 0xFFFF);
				s_save_forge_btn_tag = btn_tag;

				memset(&grown[existing_count], 0, sizeof(grown[0]));
				grown[existing_count].group_tag = 'ustr';
				grown[existing_count].parent_group_tags[0] = NONE;
				grown[existing_count].parent_group_tags[1] = NONE;
				grown[existing_count].tag_index = ustr_tag;
				grown[existing_count].name = "pc\\pause\\save_forge_button text";
				grown[existing_count].base_address = ustr_def;

				memcpy(btn_def, settings_def, 0x3EC);
				snprintf((char *)(btn_def + 0x04), 32, "save_forge_button");
				/* Set text_label_string_list at offset 0xEC to our ustr_tag */
				*(int32_t *)(btn_def + 0xEC + 0x00) = 'ustr';
				*(char const **)(btn_def + 0xEC + 0x04) = "pc\\pause\\save_forge_button text";
				*(int32_t *)(btn_def + 0xEC + 0x08) = 0;
				*(int32_t *)(btn_def + 0xEC + 0x0C) = ustr_tag;

				/* Clone event_handlers at offset 0x54 -> run function 0x7777 & close all pause widgets */
				h_count = *(int32_t const *)(settings_def + 0x54);
				h_src = *(uint8_t const *const *)(settings_def + 0x58);
				if (h_count > 0 && h_count <= 16 && h_src)
				{
					uint8_t *h_dst = (uint8_t *)malloc((size_t)h_count * 0x48u);
					if (h_dst)
					{
						int h;
						memcpy(h_dst, h_src, (size_t)h_count * 0x48u);
						for (h = 0; h < h_count; h++)
						{
							uint8_t *hp = h_dst + (size_t)h * 0x48u;
							/* _event_handler_run_function_bit (bit 7) | _event_handler_close_all_widgets_bit (bit 2) */
							*(int32_t *)(hp + 0x00) = (1 << 7) | (1 << 2);
							*(int16_t *)(hp + 0x06) = 0x7777;
							*(int32_t *)(hp + 0x08) = 'DeLa';
							*(void **)(hp + 0x0C) = NULL;
							*(int32_t *)(hp + 0x10) = 0;
							*(int32_t *)(hp + 0x14) = NONE;
						}
						*(uint8_t **)(btn_def + 0x58) = h_dst;
					}
				}

				memset(&grown[existing_count + 1], 0, sizeof(grown[0]));
				grown[existing_count + 1].group_tag = 'DeLa';
				grown[existing_count + 1].parent_group_tags[0] = NONE;
				grown[existing_count + 1].parent_group_tags[1] = NONE;
				grown[existing_count + 1].tag_index = btn_tag;
				grown[existing_count + 1].name = "pc\\pause\\save_forge_button";
				grown[existing_count + 1].base_address = btn_def;

				s_pause_saved_instances = instances;
				s_pause_saved_count = existing_count;
				s_pause_alloc_instances = grown;
				cache_files_set_tag_instances(grown, existing_count + 2);

				/* Insert save_forge_button into the pause menu's column list right before settings_button */
				for (i = 0; i < existing_count; i++)
				{
					uint8_t *w_def;
					int32_t c_count, c, found_at = -1;
					uint8_t *c_arr;

					if (grown[i].group_tag != 'DeLa' || !grown[i].base_address)
						continue;
					w_def = (uint8_t *)grown[i].base_address;
					if (*(int16_t const *)(w_def + 0x00) != 3) /* _widget_type_column_list */
						continue;
					c_count = *(int32_t const *)(w_def + 0x3E0);
					c_arr = *(uint8_t *const *)(w_def + 0x3E4);
					if (c_count <= 0 || c_count > 16 || !c_arr)
						continue;
					for (c = 0; c < c_count; c++)
					{
						int32_t child_tag = *(int32_t const *)(c_arr + (size_t)c * 0x50u + 0x0C);
						long abs_idx = child_tag & 0xFFFF;
						if (child_tag == settings_btn_tag ||
							(child_tag != NONE && abs_idx >= 0 && abs_idx < existing_count &&
							 grown[abs_idx].name && strcmp(grown[abs_idx].name, "pc\\pause\\settings_button") == 0))
						{
							found_at = c;
							break;
						}
					}
					if (found_at >= 0)
					{
						uint8_t *new_children = (uint8_t *)malloc((size_t)(c_count + 1) * 0x50u);
						if (new_children)
						{
							int16_t old_y0 = *(int16_t const *)(c_arr + 0 * 0x50u + 0x36);
							int16_t spacing = (c_count + 1 <= 4) ? 29 : 25;
							int16_t base_y = (old_y0 >= 60) ? (int16_t)(old_y0 - 20) : 0;
							long list_tag_idx = grown[i].tag_index;
							long s_idx;

							if (found_at > 0)
								memcpy(new_children, c_arr, (size_t)found_at * 0x50u);
							memcpy(new_children + (size_t)found_at * 0x50u, c_arr + (size_t)found_at * 0x50u, 0x50u);
							*(int32_t *)(new_children + (size_t)found_at * 0x50u + 0x00) = 'DeLa';
							*(char const **)(new_children + (size_t)found_at * 0x50u + 0x04) = "pc\\pause\\save_forge_button";
							*(int32_t *)(new_children + (size_t)found_at * 0x50u + 0x08) = 0;
							*(int32_t *)(new_children + (size_t)found_at * 0x50u + 0x0C) = btn_tag;

							for (c = found_at; c < c_count; c++)
							{
								uint8_t *dst_c = new_children + (size_t)(c + 1) * 0x50u;
								memcpy(dst_c, c_arr + (size_t)c * 0x50u, 0x50u);
							}
							for (c = 0; c < c_count + 1; c++)
							{
								uint8_t *dst_c = new_children + (size_t)c * 0x50u;
								*(int16_t *)(dst_c + 0x36) = (int16_t)(base_y + c * spacing);
							}
							*(int32_t *)(w_def + 0x3E0) = c_count + 1;
							*(uint8_t **)(w_def + 0x3E4) = new_children;
							*(int16_t *)(w_def + 0x28) = (int16_t)(*(int16_t const *)(w_def + 0x28) + spacing); /* bounds.y1 */

							/* Shift the pause list widget itself upward inside its parent screen widget so all 4 options sit centered inside the box */
							if (old_y0 < 60)
							{
								int16_t lift = (old_y0 > 10) ? 14 : 22;
								for (s_idx = 0; s_idx < existing_count; s_idx++)
								{
									uint8_t *sw_def;
									int32_t sc_count, sc;
									uint8_t *sc_arr;
									if (grown[s_idx].group_tag != 'DeLa' || !grown[s_idx].base_address)
										continue;
									sw_def = (uint8_t *)grown[s_idx].base_address;
									sc_count = *(int32_t const *)(sw_def + 0x3E0);
									sc_arr = *(uint8_t *const *)(sw_def + 0x3E4);
									if (sc_count <= 0 || sc_count > 16 || !sc_arr)
										continue;
									for (sc = 0; sc < sc_count; sc++)
									{
										if (*(int32_t const *)(sc_arr + (size_t)sc * 0x50u + 0x0C) == list_tag_idx)
										{
											*(int16_t *)(sc_arr + (size_t)sc * 0x50u + 0x36) =
												(int16_t)(*(int16_t const *)(sc_arr + (size_t)sc * 0x50u + 0x36) - lift);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

#else /* AE hook */
	if (g_forge_session_enabled && map_name && strcmp(map_name, "ui") != 0) /* AE hook */
		forge_runtime_markers_preload(); /* AE hook */
#endif /* AE hook */
	/* Ensure local tag cache and .ocetags packages are extracted and loaded AFTER pause menu tag expansion */
	if (g_forge_session_enabled)
		forge_runtime_init();
}

/* ---------- Point 11: Redirect custom_maps\<name>.(map|bmp|txt) to forge_maps\<name>\<name>.(map|bmp|txt) */

extern void *file_reference_create_from_path(void *reference, char const *full_path, boolean is_directory);
/* (the C runtime's fopen, the game's paths translated: the port's) */
#ifdef _WIN32
extern FILE *halo_windows_fopen(char const *filename, char const *mode);
#define forge_game_fopen halo_windows_fopen
#else
extern FILE *halo_linux_fopen(char const *filename, char const *mode);
#define forge_game_fopen halo_linux_fopen
#endif

static char const *resolve_forge_maps_redirect_path(char const *path, char *out_buf, size_t out_sz)
{
	char const *leaf = NULL;
	char const *dot;
	char sub_dir[64];
	char disk_check[512];
	size_t base_len;
	int has_d = 0;
	FILE *tf;

	if (!path || !path[0])
		return path;

	if (_strnicmp(path, "d:\\custom_maps\\", 15) == 0 || _strnicmp(path, "d:/custom_maps/", 15) == 0)
	{
		has_d = 1;
		leaf = path + 15;
	}
	else if (_strnicmp(path, "custom_maps\\", 12) == 0 || _strnicmp(path, "custom_maps/", 12) == 0)
	{
		has_d = 0;
		leaf = path + 12;
	}
	else
	{
		return path;
	}

	if (strchr(leaf, '\\') != NULL || strchr(leaf, '/') != NULL)
		return path;

	if (_stricmp(leaf, "bitmaps.map") == 0 ||
		_stricmp(leaf, "sounds.map") == 0 ||
		_stricmp(leaf, "loc.map") == 0)
	{
		snprintf(disk_check, sizeof(disk_check), "custom_maps\\%s", leaf);
		tf = fopen(disk_check, "rb");
		if (tf)
		{
			fclose(tf);
			return path;
		}
		snprintf(disk_check, sizeof(disk_check), "Faber_data\\forge_maps\\%s", leaf);
		tf = fopen(disk_check, "rb");
		if (tf)
		{
			fclose(tf);
			snprintf(out_buf, out_sz, "%sFaber_data\\forge_maps\\%s", has_d ? "d:\\" : "", leaf);
			return out_buf;
		}
		return path;
	}

	dot = strrchr(leaf, '.');
	base_len = dot ? (size_t)(dot - leaf) : strlen(leaf);
	if (base_len == 0 || base_len >= sizeof(sub_dir))
		return path;

	memcpy(sub_dir, leaf, base_len);
	sub_dir[base_len] = '\0';

	/* If custom.bmp exists inside OpenCE Forge\forge_maps\<name>\custom.bmp, Faber_data\forge_maps\<name>\custom.bmp, or forge_maps\<name>\custom.bmp, prefer it as the custom map preview! */
	if (dot && _stricmp(dot, ".bmp") == 0)
	{
		static char const *const s_bmp_dirs[] = {
			"OpenCE Forge\\forge_maps",
			"Faber_data\\forge_maps",
			"forge_maps"
		};
		int bd;
		for (bd = 0; bd < 3; bd++)
		{
			snprintf(disk_check, sizeof(disk_check), "%s\\%s\\custom.bmp", s_bmp_dirs[bd], sub_dir);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s\\custom.bmp", has_d ? "d:\\" : "", s_bmp_dirs[bd], sub_dir);
				return out_buf;
			}
			snprintf(disk_check, sizeof(disk_check), "%s\\%s\\%s", s_bmp_dirs[bd], sub_dir, leaf);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s\\%s", has_d ? "d:\\" : "", s_bmp_dirs[bd], sub_dir, leaf);
				return out_buf;
			}
		}
	}

	/* If the file exists directly in custom_maps\<leaf> (e.g. custom_maps\<name>.map or custom_maps\<name>.bmp), use it */
	snprintf(disk_check, sizeof(disk_check), "custom_maps\\%s", leaf);
	tf = fopen(disk_check, "rb");
	if (tf)
	{
		fclose(tf);
		return path;
	}

	{
		static char const *const s_map_dirs[] = {
			"OpenCE Forge\\forge_maps",
			"Faber_data\\forge_maps",
			"forge_maps"
		};
		int md;
		for (md = 0; md < 3; md++)
		{
			snprintf(disk_check, sizeof(disk_check), "%s\\%s\\%s", s_map_dirs[md], sub_dir, leaf);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s\\%s", has_d ? "d:\\" : "", s_map_dirs[md], sub_dir, leaf);
				return out_buf;
			}
			snprintf(disk_check, sizeof(disk_check), "%s\\%s", s_map_dirs[md], leaf);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s", has_d ? "d:\\" : "", s_map_dirs[md], leaf);
				return out_buf;
			}
		}
	}
	return path;
}

void *forge_ce_file_reference_create_from_path(void *reference, char const *full_path, boolean is_directory)
{
	char redirected[512];
	char const *actual = resolve_forge_maps_redirect_path(full_path, redirected, sizeof(redirected));
	return file_reference_create_from_path(reference, actual, is_directory);
}

#define FORGE_EMBED_FILE_SENTINEL ((FILE *)(uintptr_t)0x7F046E99u)

static struct
{
	int active;
	uint8_t *data;
	size_t size;
	size_t pos;
} s_embed_vfile;

static FILE *try_open_embedded_map_sidecar(char const *filename)
{
	char const *leaf;
	char const *dot;
	size_t stem_len;
	char stem[128];
	char map_path[512];
	int is_bmp = 0, is_txt = 0;
	FILE *mf;
	uint8_t hdr[0x200];
	uint32_t sig = 0, emb_off = 0, emb_sz = 0;

	if (!filename || !filename[0])
		return NULL;
	leaf = strrchr(filename, '\\');
	if (!leaf)
		leaf = strrchr(filename, '/');
	leaf = leaf ? (leaf + 1) : filename;
	dot = strrchr(leaf, '.');
	if (!dot)
		return NULL;
	if (_stricmp(dot, ".bmp") == 0)
		is_bmp = 1;
	else if (_stricmp(dot, ".txt") == 0)
		is_txt = 1;
	else
		return NULL;

	stem_len = (size_t)(dot - leaf);
	if (stem_len == 0 || stem_len >= sizeof(stem))
		return NULL;
	memcpy(stem, leaf, stem_len);
	stem[stem_len] = 0;

	snprintf(map_path, sizeof(map_path), "custom_maps/%s.map", stem);
	mf = fopen(map_path, "rb");
	if (!mf)
	{
		static char const *const sidecar_cands[] = {
			"OpenCE Forge/forge_maps/%s.map",
			"OpenCE Forge/forge_maps/%s/%s.map",
			"Faber_data/forge_maps/%s/%s.map",
			"forge_maps/%s.map",
			"forge_maps/%s/%s.map"
		};
		int ci;
		for (ci = 0; ci < 5 && !mf; ci++)
		{
			if (strstr(sidecar_cands[ci], "%s/%s"))
				snprintf(map_path, sizeof(map_path), sidecar_cands[ci], stem, stem);
			else
				snprintf(map_path, sizeof(map_path), sidecar_cands[ci], stem);
			mf = fopen(map_path, "rb");
		}
	}
	if (!mf)
		return NULL;
	if (fread(hdr, 1, sizeof(hdr), mf) != sizeof(hdr))
	{
		fclose(mf);
		return NULL;
	}
	memcpy(&sig, hdr + 0x1FCu, 4);
	if (sig == 0x464C4731u)
	{
		if (is_bmp)
		{
			memcpy(&emb_off, hdr + 0x1ECu, 4);
			memcpy(&emb_sz, hdr + 0x1F0u, 4);
		}
		else if (is_txt)
		{
			memcpy(&emb_off, hdr + 0x1F4u, 4);
			memcpy(&emb_sz, hdr + 0x1F8u, 4);
		}
	}
	if (emb_off >= 0x800u && emb_sz > 0 && emb_sz <= 4u * 1024u * 1024u)
	{
		uint8_t *buf = (uint8_t *)malloc(emb_sz);
		if (buf)
		{
			if (fseek(mf, (long)emb_off, SEEK_SET) == 0 && fread(buf, 1, emb_sz, mf) == emb_sz)
			{
				fclose(mf);
				if (s_embed_vfile.data)
					free(s_embed_vfile.data);
				s_embed_vfile.active = 1;
				s_embed_vfile.data = buf;
				s_embed_vfile.size = emb_sz;
				s_embed_vfile.pos = 0;
				return FORGE_EMBED_FILE_SENTINEL;
			}
			free(buf);
		}
	}
	fclose(mf);
	return NULL;
}

FILE *forge_ce_fopen(char const *filename, char const *mode)
{
	char redirected[512];
	char const *actual = resolve_forge_maps_redirect_path(filename, redirected, sizeof(redirected));
	FILE *fp = forge_game_fopen(actual, mode);
	if (!fp)
		fp = try_open_embedded_map_sidecar(filename);
	return fp;
}

size_t forge_ce_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		size_t total = size * nmemb;
		size_t avail;
		if (!s_embed_vfile.active || !s_embed_vfile.data || size == 0 || nmemb == 0)
			return 0;
		avail = (s_embed_vfile.pos < s_embed_vfile.size) ? (s_embed_vfile.size - s_embed_vfile.pos) : 0;
		if (total > avail)
			total = (avail / size) * size;
		if (total > 0)
		{
			memcpy(ptr, s_embed_vfile.data + s_embed_vfile.pos, total);
			s_embed_vfile.pos += total;
		}
		return total / size;
	}
	return fread(ptr, size, nmemb, stream);
}

int forge_ce_fseek(FILE *stream, long offset, int whence)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		long target = 0;
		if (!s_embed_vfile.active)
			return -1;
		if (whence == SEEK_SET)
			target = offset;
		else if (whence == SEEK_CUR)
			target = (long)s_embed_vfile.pos + offset;
		else if (whence == SEEK_END)
			target = (long)s_embed_vfile.size + offset;
		else
			return -1;
		if (target < 0 || (size_t)target > s_embed_vfile.size)
			return -1;
		s_embed_vfile.pos = (size_t)target;
		return 0;
	}
	return fseek(stream, offset, whence);
}

long forge_ce_ftell(FILE *stream)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		if (!s_embed_vfile.active)
			return -1L;
		return (long)s_embed_vfile.pos;
	}
	return ftell(stream);
}

int forge_ce_fclose(FILE *stream)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		if (s_embed_vfile.data)
		{
			free(s_embed_vfile.data);
			s_embed_vfile.data = NULL;
		}
		s_embed_vfile.active = 0;
		s_embed_vfile.size = 0;
		s_embed_vfile.pos = 0;
		return 0;
	}
	return fclose(stream);
}

#define FORGE_CE_FIND_SENTINEL_HANDLE ((HANDLE)(uintptr_t)0x7F046E01u)

static struct
{
	boolean in_use;
	HANDLE primary_handle;
	HANDLE forge_dirs_handle;
	int forge_dir_idx;
} s_forge_ce_find = { FALSE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, 0 };

static BOOL forge_ce_advance_forge_dirs(LPWIN32_FIND_DATAA data)
{
	static char const *const find_patterns[3] = {
		"OpenCE Forge\\forge_maps\\*.*",
		"Faber_data\\forge_maps\\*.*",
		"d:\\forge_maps\\*.*"
	};
	static char const *const find_roots[3] = {
		"OpenCE Forge\\forge_maps",
		"Faber_data\\forge_maps",
		"forge_maps"
	};
	WIN32_FIND_DATAA dir_data;
	BOOL got_entry = FALSE;

	while (s_forge_ce_find.forge_dir_idx < 3)
	{
		char const *root = find_roots[s_forge_ce_find.forge_dir_idx];
		if (s_forge_ce_find.forge_dirs_handle == INVALID_HANDLE_VALUE)
		{
			s_forge_ce_find.forge_dirs_handle = FindFirstFileA(find_patterns[s_forge_ce_find.forge_dir_idx], &dir_data);
			if (s_forge_ce_find.forge_dirs_handle == INVALID_HANDLE_VALUE)
			{
				s_forge_ce_find.forge_dir_idx++;
				continue;
			}
			got_entry = TRUE;
		}
		else
		{
			got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
		}

		while (got_entry)
		{
			if (dir_data.cFileName[0] != '.' &&
				strlen(dir_data.cFileName) > 0 &&
				strlen(dir_data.cFileName) < 64)
			{
				char stem[64];
				char map_check[512];
				FILE *tf;

				if (dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					snprintf(stem, sizeof(stem), "%s", dir_data.cFileName);
				}
				else
				{
					char *dot;
					snprintf(stem, sizeof(stem), "%s", dir_data.cFileName);
					dot = strrchr(stem, '.');
					if (!dot || _stricmp(dot, ".map") != 0)
					{
						got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
						continue;
					}
					*dot = 0;
				}

				/* Skip if custom_maps\<name>.map already exists (already enumerated by primary_handle) */
				snprintf(map_check, sizeof(map_check), "custom_maps\\%s.map", stem);
				tf = fopen(map_check, "rb");
				if (tf)
				{
					fclose(tf);
					got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
					continue;
				}

				if (dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					snprintf(map_check, sizeof(map_check), "%s\\%s\\%s.map", root, stem, stem);
				else
					snprintf(map_check, sizeof(map_check), "%s\\%s.map", root, stem);
				tf = fopen(map_check, "rb");
				if (tf)
				{
					fclose(tf);
					memset(data, 0, sizeof(*data));
					data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
					snprintf(data->cFileName, sizeof(data->cFileName), "%s.map", stem);
					return TRUE;
				}
			}
			got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
		}

		CloseHandle(s_forge_ce_find.forge_dirs_handle);
		s_forge_ce_find.forge_dirs_handle = INVALID_HANDLE_VALUE;
		s_forge_ce_find.forge_dir_idx++;
	}

	return FALSE;
}

HANDLE WINAPI forge_ce_FindFirstFileA(LPCSTR file_name, LPWIN32_FIND_DATAA data)
{
	if (!s_forge_ce_find.in_use && file_name &&
		(_stricmp(file_name, "d:\\custom_maps\\*.*") == 0 ||
		 _stricmp(file_name, "custom_maps\\*.*") == 0))
	{
		s_forge_ce_find.in_use = TRUE;
		s_forge_ce_find.primary_handle = FindFirstFileA(file_name, data);
		s_forge_ce_find.forge_dirs_handle = INVALID_HANDLE_VALUE;
		s_forge_ce_find.forge_dir_idx = 0;
		if (s_forge_ce_find.primary_handle != INVALID_HANDLE_VALUE)
			return FORGE_CE_FIND_SENTINEL_HANDLE;
		if (forge_ce_advance_forge_dirs(data))
			return FORGE_CE_FIND_SENTINEL_HANDLE;
		s_forge_ce_find.in_use = FALSE;
		return INVALID_HANDLE_VALUE;
	}
	return FindFirstFileA(file_name, data);
}

BOOL WINAPI forge_ce_FindNextFileA(HANDLE find_file, LPWIN32_FIND_DATAA data)
{
	if (find_file == FORGE_CE_FIND_SENTINEL_HANDLE)
	{
		if (s_forge_ce_find.primary_handle != INVALID_HANDLE_VALUE)
		{
			if (FindNextFileA(s_forge_ce_find.primary_handle, data))
				return TRUE;
			CloseHandle(s_forge_ce_find.primary_handle);
			s_forge_ce_find.primary_handle = INVALID_HANDLE_VALUE;
		}
		return forge_ce_advance_forge_dirs(data);
	}
	return FindNextFileA(find_file, data);
}

BOOL WINAPI forge_ce_CloseHandle(HANDLE hObject)
{
	if (hObject == FORGE_CE_FIND_SENTINEL_HANDLE)
	{
		if (s_forge_ce_find.primary_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(s_forge_ce_find.primary_handle);
			s_forge_ce_find.primary_handle = INVALID_HANDLE_VALUE;
		}
		if (s_forge_ce_find.forge_dirs_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(s_forge_ce_find.forge_dirs_handle);
			s_forge_ce_find.forge_dirs_handle = INVALID_HANDLE_VALUE;
		}
		s_forge_ce_find.in_use = FALSE;
		return TRUE;
	}
	return CloseHandle(hObject);
}

/* ---------- Disable Menu, Mouse, Keyboard & Controller Interaction When OpenCE - Faber.exe Is Unfocused */

struct forge_sdl_ui_pointer
{
	float x, y;
	float click_x, click_y;
	BOOL moved;
	int left_clicks, right_clicks;
	int wheel_steps;
	int downs;
	float down_x, down_y;
	BOOL touch;
};

struct forge_sdl_input_state
{
	unsigned char keys[512];
	unsigned char mouse_buttons[8];
	float mouse_dx, mouse_dy;
	float mouse_wheel;
	BOOL focused;
	BOOL mouse_released;
	BOOL ui_pointer;
	BOOL menus;
};

struct forge_sdl_keystroke
{
	BYTE virtual_key;
	CHAR ascii;
	BYTE flags;
	BOOL overlay;
};

extern BOOL orig_platform_ui_pointer_read(struct forge_sdl_ui_pointer *pointer);
extern void orig_platform_input_read(struct forge_sdl_input_state *state, BOOL consume_motion);
extern BOOL orig_platform_next_keystroke(struct forge_sdl_keystroke *keystroke);
extern DWORD WINAPI orig_XInputGetState(HANDLE hDevice, PXINPUT_STATE pState);

#ifdef _WIN32
__declspec(dllimport) void *WINAPI GetForegroundWindow(void);
__declspec(dllimport) DWORD WINAPI GetWindowThreadProcessId(void *hWnd, LPDWORD lpdwProcessId);
__declspec(dllimport) DWORD WINAPI GetCurrentProcessId(void);
#endif

int forge_window_is_focused(void)
{
#ifdef _WIN32
	static DWORD s_self_pid = 0;
	void *fg;
	DWORD fg_pid = 0;

	if (s_self_pid == 0)
		s_self_pid = GetCurrentProcessId();
	fg = GetForegroundWindow();
	if (!fg)
		return 0;
	GetWindowThreadProcessId(fg, &fg_pid);
	return (fg_pid == s_self_pid) ? 1 : 0;
#else
	return 1;
#endif
}

#ifdef _WIN32
#undef GetAsyncKeyState
__declspec(dllimport) SHORT WINAPI GetAsyncKeyState(int vKey);
#else
/* the keys and mouse buttons down, as the platform layer last read them
(platform_input_read, below): GetAsyncKeyState's answer on the builds that
have no Windows to ask (forge_key_state) */
static unsigned char s_forge_keys_down[512];
static unsigned char s_forge_mouse_down[8];

/* the SDL scancode of a Windows virtual key Forge asks for, else 0 (and a
second, the right-hand modifier's, in *other) */
static int forge_vk_scancode(int vk, int *other)
{
	*other = 0;
	if (vk >= 'A' && vk <= 'Z')
		return 4 + (vk - 'A');
	if (vk >= '1' && vk <= '9')
		return 30 + (vk - '1');
	if (vk == '0')
		return 39;
	if (vk >= 0x70 && vk <= 0x7B) /* VK_F1..VK_F12 */
		return 58 + (vk - 0x70);
	if (vk >= 0x61 && vk <= 0x69) /* VK_NUMPAD1..9 */
		return 89 + (vk - 0x61);
	switch (vk)
	{
	case 0x60: return 98; /* VK_NUMPAD0 */
	case 0x08: return 42; /* VK_BACK */
	case 0x09: return 43; /* VK_TAB */
	case 0x0D: return 40; /* VK_RETURN */
	case 0x1B: return 41; /* VK_ESCAPE */
	case 0x20: return 44; /* VK_SPACE */
	case 0x10: *other = 229; return 225; /* VK_SHIFT */
	case 0x11: *other = 228; return 224; /* VK_CONTROL */
	case 0x12: *other = 230; return 226; /* VK_MENU */
	case 0xA0: return 225; /* VK_LSHIFT */
	case 0xA1: return 229; /* VK_RSHIFT */
	case 0xA2: return 224; /* VK_LCONTROL */
	case 0xA3: return 228; /* VK_RCONTROL */
	case 0xA4: return 226; /* VK_LMENU */
	case 0xA5: return 230; /* VK_RMENU */
	case 0x21: return 75; /* VK_PRIOR */
	case 0x22: return 78; /* VK_NEXT */
	case 0x23: return 77; /* VK_END */
	case 0x24: return 74; /* VK_HOME */
	case 0x25: return 80; /* VK_LEFT */
	case 0x26: return 82; /* VK_UP */
	case 0x27: return 79; /* VK_RIGHT */
	case 0x28: return 81; /* VK_DOWN */
	case 0x2D: return 73; /* VK_INSERT */
	case 0x2E: return 76; /* VK_DELETE */
	case 0x6A: return 85; /* VK_MULTIPLY */
	case 0x6B: return 87; /* VK_ADD */
	case 0x6D: return 86; /* VK_SUBTRACT */
	case 0x6E: return 99; /* VK_DECIMAL */
	case 0x6F: return 84; /* VK_DIVIDE */
	case 0xBA: return 51; /* VK_OEM_1 ; */
	case 0xBB: return 46; /* VK_OEM_PLUS = */
	case 0xBC: return 54; /* VK_OEM_COMMA */
	case 0xBD: return 45; /* VK_OEM_MINUS */
	case 0xBE: return 55; /* VK_OEM_PERIOD */
	case 0xBF: return 56; /* VK_OEM_2 / */
	case 0xC0: return 53; /* VK_OEM_3 ` */
	case 0xDB: return 47; /* VK_OEM_4 [ */
	case 0xDC: return 49; /* VK_OEM_5 \ */
	case 0xDD: return 48; /* VK_OEM_6 ] */
	case 0xDE: return 52; /* VK_OEM_7 ' */
	}
	return 0;
}
#endif

/* whether a chat box is being typed in (the match's, network_chat.c, or
the fork's, social_chat.c): Forge then leaves the keys and the controller to
it */
extern boolean social_chat_typing(void);

int forge_chat_typing(void)
{
	return orig_network_chat_typing() || social_chat_typing();
}

int forge_menu_open(void)
{
	return g_forge_session_enabled && g_forge.initialized && g_forge.active_modal != _forge_modal_none &&
		!forge_is_main_menu_active();
}

short forge_key_state(int vk)
{
	if (!forge_window_is_focused() || forge_chat_typing())
		return 0;
#ifdef _WIN32
	return GetAsyncKeyState(vk);
#else
	{
		int other, scancode;

		/* (VK_LBUTTON, VK_RBUTTON, VK_MBUTTON: SDL's buttons 1, 3, 2) */
		if (vk == 0x01 || vk == 0x02 || vk == 0x04)
			return s_forge_mouse_down[vk == 0x01 ? 1 : vk == 0x02 ? 3 : 2] ? (short)0x8000 : 0;
		scancode = forge_vk_scancode(vk, &other);
		if (!scancode)
			return 0;
		return (s_forge_keys_down[scancode] || (other && s_forge_keys_down[other])) ? (short)0x8000 : 0;
	}
#endif
}

BOOL platform_ui_pointer_read(struct forge_sdl_ui_pointer *pointer)
{
	BOOL active = orig_platform_ui_pointer_read(pointer);
	if (!forge_window_is_focused())
	{
		if (pointer)
		{
			pointer->moved = FALSE;
			pointer->left_clicks = 0;
			pointer->right_clicks = 0;
			pointer->wheel_steps = 0;
			pointer->downs = 0;
		}
		return FALSE;
	}
	return active;
}

void platform_input_read(struct forge_sdl_input_state *state, BOOL consume_motion)
{
	orig_platform_input_read(state, consume_motion);
#ifndef _WIN32
	if (state)
	{
		memcpy(s_forge_keys_down, state->keys, sizeof(s_forge_keys_down));
		memcpy(s_forge_mouse_down, state->mouse_buttons, sizeof(s_forge_mouse_down));
	}
#endif
	if (state && (!forge_window_is_focused() || !state->focused))
	{
		memset(state->keys, 0, sizeof(state->keys));
		memset(state->mouse_buttons, 0, sizeof(state->mouse_buttons));
		state->mouse_dx = 0.0f;
		state->mouse_dy = 0.0f;
		state->mouse_wheel = 0.0f;
		state->focused = FALSE;
	}
	else if (state)
	{
		if (forge_has_held_item_to_cancel() &&
			state->keys[41 /* SDL_SCANCODE_ESCAPE */])
		{
			s_forge_esc_cancel_ms = system_milliseconds();
		}
		if (s_forge_esc_cancel_ms != 0 &&
			(system_milliseconds() - s_forge_esc_cancel_ms) < 250UL)
		{
			state->keys[41 /* SDL_SCANCODE_ESCAPE */] = 0;
		}
	}
}

BOOL platform_next_keystroke(struct forge_sdl_keystroke *keystroke)
{
	if (!forge_window_is_focused())
	{
		struct forge_sdl_keystroke dummy;
		while (orig_platform_next_keystroke(&dummy))
		{
		}
		return FALSE;
	}
	while (orig_platform_next_keystroke(keystroke))
	{
		if (keystroke &&
			(keystroke->virtual_key == VK_BACK || keystroke->ascii == '\b') &&
			s_forge_name_modal_active_ms != 0 &&
			(system_milliseconds() - s_forge_name_modal_active_ms) < 250UL)
		{
			continue;
		}
		if (keystroke &&
			(keystroke->virtual_key == VK_ESCAPE || keystroke->ascii == 27))
		{
			if (forge_has_held_item_to_cancel())
			{
				s_forge_esc_cancel_ms = system_milliseconds();
				continue;
			}
			if (s_forge_esc_cancel_ms != 0 &&
				(system_milliseconds() - s_forge_esc_cancel_ms) < 250UL)
			{
				continue;
			}
		}
		return TRUE;
	}
	return FALSE;
}

/* the first controller's state as the game last got it (XInputGetState,
below): the port's, its buttons remapped and the touch controls' in it, for
Forge's controls (forge_pad_read) */
extern int xinput_device_port(HANDLE device);
/* (a custom controller layout's buttons put back where they were pressed:
Forge's binds name them by where they sit) */
extern void xinput_forge_unmapped(XINPUT_GAMEPAD *pad);
static XINPUT_GAMEPAD s_forge_pad;
static BOOL s_forge_pad_valid;

DWORD WINAPI XInputGetState(HANDLE hDevice, PXINPUT_STATE pState)
{
	DWORD rc = orig_XInputGetState(hDevice, pState);
	if (!forge_window_is_focused() && pState)
	{
		memset(&pState->Gamepad, 0, sizeof(pState->Gamepad));
	}
	if (pState && xinput_device_port(hDevice) == 0)
	{
		s_forge_pad_valid = rc == ERROR_SUCCESS;
		s_forge_pad = pState->Gamepad;
		if (s_forge_pad_valid && forge_window_is_focused())
			xinput_forge_unmapped(&s_forge_pad);
	}
	return rc;
}

int forge_pad_read(struct forge_pad *pad)
{
	int index;

	memset(pad, 0, sizeof(*pad));
	if (!s_forge_pad_valid || forge_chat_typing())
		return 0;
	pad->buttons = s_forge_pad.wButtons;
	for (index = 0; index < 8; index++)
		pad->analog[index] = s_forge_pad.bAnalogButtons[index];
	pad->lx = s_forge_pad.sThumbLX;
	pad->ly = s_forge_pad.sThumbLY;
	pad->rx = s_forge_pad.sThumbRX;
	pad->ry = s_forge_pad.sThumbRY;
	return 1;
}




