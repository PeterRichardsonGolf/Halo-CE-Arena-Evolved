/*
FIREFIGHT_STATS.C

Persistent per-profile Firefight statistics, personal best record tracker,
and web-ready JSON exporter:
- Per-Profile Identity (`player_id` 16-hex public ID + `stats_key` 32-hex secret key)
- Per-Map Firefight Personal Best High Scores (best_score, best_set, best_round,
  best_wave, best_seconds, best_difficulty, best_skulls, best_medals)
- Lifetime Firefight Career Stats (total_matches, total_sets_completed,
  total_waves_cleared, total_kills, total_boss_kills, total_score,
  total_seconds, total_deaths, total_assists, best_kill_spree, medal_counts[38])
- Saved in each player profile's directory as `firefight_records.txt`
- Exported as `firefight_stats_export.json` for web dashboard sync & local viewing
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries/cseries.h"
#include <xtl.h>
#include "game/cheats.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_engine_place.h"
#include "game/players.h"
#include "game_modes.h"
#include "interface/player_ui.h"
#include "memory/data.h"
#include "saved games/player_profile.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "network_replay.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"

#include "opence_features.h"
#include "social.h"
#include "halo_firefight.h"
#include "halo_emblems.h" /* AE hook */
#include "ae_pve_glue.h" /* AE hook */
#include "service_rank.h"
#include "halo_skulls.h"
#include "campaign_score.h"
#include "campaign_medals.h"
#include "interface/hud_messaging.h"

/* ---------- constants */

#define FIREFIGHT_RECORDS_FILE "firefight_records.txt"
#define FIREFIGHT_EXPORT_JSON_FILE "firefight_stats_export.json"
#define DEFAULT_HALO_STATS_API_URL ""
#define MAX_TRACKED_MAPS 64

/* ---------- externs */

extern struct game_variant global_variant;
boolean player_profile_get_enclosing_directory_path(long profile, char *full_path);
int config_boolean(const char *name);
const char *config_string(const char *name);
void platform_open_url(const char *url);
void platform_clipboard_set(const char *text);
unsigned long system_milliseconds(void);
void p2p_hardware_id(char *hex, int size);
int posix_socket(int family, int type, int protocol);
int posix_socket_close(int socket);
int posix_socket_bind(int socket, const void *address, int address_length);
int posix_socket_connect(int socket, const void *address, int address_length);
int posix_socket_listen(int socket, int backlog);
int posix_socket_accept(int socket, void *address, int *address_length);
int posix_socket_send(int socket, const void *buffer, int length, int flags);
int posix_socket_recv(int socket, void *buffer, int length, int flags);
int posix_socket_setsockopt(int socket, int level, int name, const void *value, int length);
int update_request(
	const char *method,
	const char *url,
	const char *headers,
	const char *body,
	char *response,
	int response_size,
	char *error,
	int error_size);

/* ---------- structures */

struct halo_achievement_def
{
	char const *key;
	wchar_t const *title;
	wchar_t const *desc;
	short points;
};

static struct halo_achievement_def const s_achievement_defs[HALO_ACHIEVEMENT_COUNT] =
{
	/* 0..9: Finishing each Campaign level (10 x 10 = 100 pts) */
	{ "camp_a10", L"PILLAR OF AUTUMN", L"FINISH LEVEL A10", 10 },
	{ "camp_a30", L"HALO", L"FINISH LEVEL A30", 10 },
	{ "camp_a50", L"TRUTH & RECON", L"FINISH LEVEL A50", 10 },
	{ "camp_b30", L"SILENT CARTOG.", L"FINISH LEVEL B30", 10 },
	{ "camp_b40", L"CONTROL ROOM", L"FINISH LEVEL B40", 10 },
	{ "camp_c10", L"343 GUILTY SPARK", L"FINISH LEVEL C10", 10 },
	{ "camp_c20", L"THE LIBRARY", L"FINISH LEVEL C20", 10 },
	{ "camp_c40", L"TWO BETRAYALS", L"FINISH LEVEL C40", 10 },
	{ "camp_d20", L"KEYES", L"FINISH LEVEL D20", 10 },
	{ "camp_d40", L"THE MAW", L"FINISH LEVEL D40", 10 },
	/* 10..12: Finishing a Campaign level on Normal, Heroic, Legendary (10 + 15 + 25 = 50 pts) */
	{ "lvl_normal", L"LVL: NORMAL", L"LEVEL ON NORMAL", 10 },
	{ "lvl_heroic", L"LVL: HEROIC", L"LEVEL ON HEROIC", 15 },
	{ "lvl_legendary", L"LVL: LEGENDARY", L"LEVEL ON LEGENDARY", 25 },
	/* 13..16: Finishing full Campaign & on Normal, Heroic, Legendary (40 + 50 + 75 + 125 = 290 pts) */
	{ "camp_complete", L"FINISH CAMPAIGN", L"ALL 10 MISSIONS", 40 },
	{ "camp_normal", L"CAMP: NORMAL", L"CAMPAIGN NORMAL", 50 },
	{ "camp_heroic", L"CAMP: HEROIC", L"CAMPAIGN HEROIC", 75 },
	{ "camp_legendary", L"CAMP: LEGENDARY", L"CAMPAIGN LEGENDARY", 125 },
	/* 17..27: Complete 1 Set Firefight + 1 Set on each map (30 + 10 x 25 = 280 pts) */
	{ "ff_first_set", L"FF: 1ST SET", L"COMPLETE 1 SET", 30 },
	{ "ff_set_a10", L"FF SET: A10", L"1 SET ON AUTUMN", 25 },
	{ "ff_set_a30", L"FF SET: A30", L"1 SET ON HALO", 25 },
	{ "ff_set_a50", L"FF SET: A50", L"1 SET ON T&R", 25 },
	{ "ff_set_b30", L"FF SET: B30", L"1 SET ON CARTOG.", 25 },
	{ "ff_set_b40", L"FF SET: B40", L"1 SET ON AOTCR", 25 },
	{ "ff_set_c10", L"FF SET: C10", L"1 SET ON 343 GS", 25 },
	{ "ff_set_c20", L"FF SET: C20", L"1 SET ON LIBRARY", 25 },
	{ "ff_set_c40", L"FF SET: C40", L"1 SET ON TB", 25 },
	{ "ff_set_d20", L"FF SET: D20", L"1 SET ON KEYES", 25 },
	{ "ff_set_d40", L"FF SET: D40", L"1 SET ON THE MAW", 25 },
	/* 28..38: Achievements for each Skull that appears on a map + Elite Skull-Carrier (10 x 25 + 30 = 280 pts) */
	{ "skull_iron", L"SKULL: IRON", L"FOUND ON A10", 25 },
	{ "skull_mythic", L"SKULL: MYTHIC", L"FOUND ON A30", 25 },
	{ "skull_boom", L"SKULL: BOOM", L"FOUND ON A50", 25 },
	{ "skull_bandanna", L"SKULL: BANDANNA", L"FOUND ON B30", 25 },
	{ "skull_fog", L"SKULL: FOG", L"FOUND ON B40", 25 },
	{ "skull_famine", L"SKULL: FAMINE", L"FOUND ON C10", 25 },
	{ "skull_black_eye", L"SKULL: BLACK EYE", L"FOUND ON C20", 25 },
	{ "skull_eye_patch", L"SKULL: EYE PATCH", L"FOUND ON C40", 25 },
	{ "skull_recession", L"SKULL: RECESSION", L"FOUND ON D20", 25 },
	{ "skull_malfunction", L"SKULL: MALFUNC.", L"FOUND ON D40", 25 },
	{ "skull_elite_hunter", L"SKULL: ELITE DROP", L"SLAIN ELITE CARRIER", 30 }
};

struct firefight_stored_map_entry
{
	char map_key[32];
	boolean valid;
	struct firefight_map_record record;
};

struct firefight_telemetry_session
{
	char category[24];          /* "campaign" | "firefight" | "multiplayer" | "ranked" */
	char player_name[64];
	char map_name[32];
	short difficulty;
	long score;
	long kills;
	long deaths;
	long sets_completed;
	long rounds_completed;
	long waves_completed;
	long generators_defended;
	long duration_seconds;
	long skulls_unlocked_count;
};

struct firefight_telemetry_post_job
{
	char url[512];
	char *json_payload;
};

/* ---------- globals */

static struct firefight_career_stats g_career_stats;
static struct firefight_emblem_info g_profile_emblem = FIREFIGHT_EMBLEM_DEFAULT;

/* what the emblems drawn by code became, by their old indexes */
static short const s_old_emblem_foregrounds[] =
{
	FIREFIGHT_EMBLEM_FG_SPARTAN_HELMET,
	FIREFIGHT_EMBLEM_FG_SKULL_KING,
	FIREFIGHT_EMBLEM_FG_VALKYRIE,
	FIREFIGHT_EMBLEM_FG_HELMET,
	FIREFIGHT_EMBLEM_FG_MARK_OF_SHAME,
	FIREFIGHT_EMBLEM_FG_MARATHON,
	FIREFIGHT_EMBLEM_FG_BULLSEYE,
	FIREFIGHT_EMBLEM_FG_CROSSED_SWORDS,
	FIREFIGHT_EMBLEM_FG_SPEARHEAD,
	FIREFIGHT_EMBLEM_FG_PHOENIX,
	FIREFIGHT_EMBLEM_FG_RADIOACTIVE,
	FIREFIGHT_EMBLEM_FG_CROSSHAIRS,
	FIREFIGHT_EMBLEM_FG_RADIOACTIVE,
	FIREFIGHT_EMBLEM_FG_JOLLY_ROGER,
	FIREFIGHT_EMBLEM_FG_TRIAD,
	FIREFIGHT_EMBLEM_FG_CHAMPION
};

static short const s_old_emblem_backgrounds[] =
{
	FIREFIGHT_EMBLEM_BG_NONE,
	FIREFIGHT_EMBLEM_BG_SHIELD,
	FIREFIGHT_EMBLEM_BG_CIRCLE,
	FIREFIGHT_EMBLEM_BG_DIAMOND,
	FIREFIGHT_EMBLEM_BG_OCTAGON,
	FIREFIGHT_EMBLEM_BG_HEXAGON,
	FIREFIGHT_EMBLEM_BG_PLUS,
	FIREFIGHT_EMBLEM_BG_8_POINTED_STAR,
	FIREFIGHT_EMBLEM_BG_BANNER,
	FIREFIGHT_EMBLEM_BG_SHIELD,
	FIREFIGHT_EMBLEM_BG_DELTA_WING,
	FIREFIGHT_EMBLEM_BG_COG
};

/* in the order of the armour colours (which the old emblems' colours shared) */
static short const s_armor_emblem_colors[] =
{
	FIREFIGHT_EMBLEM_COLOR_WHITE,
	FIREFIGHT_EMBLEM_COLOR_STEEL, /* black: the palette's darkest */
	FIREFIGHT_EMBLEM_COLOR_RED,
	FIREFIGHT_EMBLEM_COLOR_BLUE,
	FIREFIGHT_EMBLEM_COLOR_SILVER, /* gray */
	FIREFIGHT_EMBLEM_COLOR_YELLOW,
	FIREFIGHT_EMBLEM_COLOR_GREEN,
	FIREFIGHT_EMBLEM_COLOR_PINK,
	FIREFIGHT_EMBLEM_COLOR_VIOLET, /* purple */
	FIREFIGHT_EMBLEM_COLOR_CYAN,
	FIREFIGHT_EMBLEM_COLOR_COBALT,
	FIREFIGHT_EMBLEM_COLOR_ORANGE,
	FIREFIGHT_EMBLEM_COLOR_TEAL,
	FIREFIGHT_EMBLEM_COLOR_SAGE,
	FIREFIGHT_EMBLEM_COLOR_BROWN,
	FIREFIGHT_EMBLEM_COLOR_TAN,
	FIREFIGHT_EMBLEM_COLOR_CRIMSON, /* maroon */
	FIREFIGHT_EMBLEM_COLOR_SALMON
};

short firefight_emblem_color_from_armor(short armor_color)
{
	return s_armor_emblem_colors[PIN(armor_color, 0, (short)NUMBEROF(s_armor_emblem_colors) - 1)];
}

static void store_profile_emblem(struct firefight_emblem_info const *emblem)
{
	g_profile_emblem.foreground = (short)PIN(emblem->foreground, 0, FIREFIGHT_EMBLEM_FOREGROUND_COUNT - 1);
	g_profile_emblem.background = (short)PIN(emblem->background, 0, FIREFIGHT_EMBLEM_BACKGROUND_COUNT - 1);
	g_profile_emblem.secondary_layer = emblem->secondary_layer ? TRUE : FALSE;
	g_profile_emblem.primary_color = (short)PIN(emblem->primary_color, 0, FIREFIGHT_EMBLEM_COLOR_COUNT - 1);
	g_profile_emblem.secondary_color = (short)PIN(emblem->secondary_color, 0, FIREFIGHT_EMBLEM_COLOR_COUNT - 1);
	g_profile_emblem.background_color = (short)PIN(emblem->background_color, 0, FIREFIGHT_EMBLEM_COLOR_COUNT - 1);
	g_profile_emblem.nameplate = (short)PIN(emblem->nameplate, 0, FIREFIGHT_NAMEPLATE_COUNT - 1);
}
static long s_session_player_medals[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static short s_session_player_medal_counts[HALO_PORT_MAXIMUM_NETWORK_PLAYERS][38];
static long s_session_start_xp = -1;
static long s_session_combat_xp = 0;
static long s_session_medal_xp = 0;
static long s_session_bonus_xp = 0;
/* what this match's XP came from, for the postgame's XP screen */
static struct firefight_postgame_xp_line s_session_xp_lines[FIREFIGHT_POSTGAME_XP_LINES];
static short s_session_xp_line_count = 0;
static boolean g_career_loaded = FALSE;
static boolean g_profile_records_dirty = FALSE;
static struct firefight_stored_map_entry g_map_records[MAX_TRACKED_MAPS];
static short g_map_record_count = 0;
static long g_loaded_profile_index = NONE;
static char g_player_id[17] = "";
static char g_stats_key[33] = "";
static char g_cached_dev_id[17] = "";
static char g_cached_dev_key[33] = "";
static boolean g_cached_dev_identity_checked = FALSE;
static boolean g_cached_dev_identity_valid = FALSE;
static char g_active_link_code[8] = "";
static unsigned long g_active_link_expiry_ms = 0;
static volatile boolean g_pending_link_popup = FALSE;
static boolean g_loopback_server_started = FALSE;

/* ---------- helpers */

static unsigned long fnv1a_hash_step(unsigned long hash, const void *data, size_t len)
{
	const unsigned char *bytes = (const unsigned char *)data;
	size_t i;

	for (i = 0; i < len; i++)
	{
		hash ^= (unsigned long)bytes[i];
		hash *= 16777619UL;
	}
	return hash;
}

static boolean derive_device_linked_identity(char *out_player_id, size_t id_size, char *out_stats_key, size_t key_size)
{
	char hw_hex[64];
	unsigned long h1 = 2166136261UL ^ 0xA5A5A5A5UL;
	unsigned long h2 = 2166136261UL ^ 0x9E3779B9UL;
	unsigned long h3 = 2166136261UL ^ 0x85EBCA6BUL;
	unsigned long h4 = 2166136261UL ^ 0xC2B2AE35UL;
	size_t len;
	FILE *fp;

	if (g_cached_dev_identity_checked)
	{
		if (g_cached_dev_identity_valid)
		{
			snprintf(out_player_id, id_size, "%s", g_cached_dev_id);
			snprintf(out_stats_key, key_size, "%s", g_cached_dev_key);
			return TRUE;
		}
		return FALSE;
	}
	g_cached_dev_identity_checked = TRUE;

	hw_hex[0] = '\0';
	p2p_hardware_id(hw_hex, sizeof(hw_hex));
	len = strlen(hw_hex);
	if (len >= 16)
	{
		h1 = fnv1a_hash_step(h1, hw_hex, len);
		h2 = fnv1a_hash_step(h2, hw_hex, len / 2);
		h3 = fnv1a_hash_step(h3, hw_hex + (len / 2), len - (len / 2));
		h4 = fnv1a_hash_step(h4, &h1, sizeof(h1));
		h4 = fnv1a_hash_step(h4, &h2, sizeof(h2));

		snprintf(out_stats_key, key_size, "%08lX%08lX%08lX%08lX", h1, h2, h3, h4);
		snprintf(out_player_id, id_size, "%08lX%08lX", h1 ^ h3, h2 ^ h4);
		snprintf(g_cached_dev_id, sizeof(g_cached_dev_id), "%s", out_player_id);
		snprintf(g_cached_dev_key, sizeof(g_cached_dev_key), "%s", out_stats_key);
		g_cached_dev_identity_valid = TRUE;

		fp = fopen("Faber_data/device_identity.txt", "w");
		if (fp)
		{
			fprintf(fp, "%s:%s\n", out_player_id, out_stats_key);
			fclose(fp);
		}
		return TRUE;
	}

	fp = fopen("Faber_data/device_identity.txt", "r");
	if (fp)
	{
		char saved_id[24] = "";
		char saved_key[40] = "";
		if (fscanf(fp, "%16[^:\r\n]:%32[^\r\n]", saved_id, saved_key) == 2 &&
			strlen(saved_id) >= 8 && strlen(saved_key) >= 16)
		{
			fclose(fp);
			snprintf(out_player_id, id_size, "%s", saved_id);
			snprintf(out_stats_key, key_size, "%s", saved_key);
			snprintf(g_cached_dev_id, sizeof(g_cached_dev_id), "%s", saved_id);
			snprintf(g_cached_dev_key, sizeof(g_cached_dev_key), "%s", saved_key);
			g_cached_dev_identity_valid = TRUE;
			return TRUE;
		}
		fclose(fp);
	}
	return FALSE;
}

static void generate_profile_identity(long profile_index)
{
	char dir_path[260];
	unsigned long h1 = 2166136261UL;
	unsigned long h2 = 2166136261UL ^ 0x9E3779B9UL;
	unsigned long h3 = 2166136261UL ^ 0x85EBCA6BUL;
	unsigned long h4 = 2166136261UL ^ 0xC2B2AE35UL;
	FILE *fp;

	if (derive_device_linked_identity(g_player_id, sizeof(g_player_id), g_stats_key, sizeof(g_stats_key)))
		return;

	dir_path[0] = '\0';
	if (profile_index != NONE)
		player_profile_get_enclosing_directory_path(profile_index, dir_path);

	h1 = fnv1a_hash_step(h1, dir_path, strlen(dir_path));
	h1 = fnv1a_hash_step(h1, &profile_index, sizeof(profile_index));
	h2 = fnv1a_hash_step(h2, dir_path, strlen(dir_path));
	h3 = fnv1a_hash_step(h3, &h1, sizeof(h1));
	h3 = fnv1a_hash_step(h3, &h2, sizeof(h2));
	h4 = fnv1a_hash_step(h4, &h3, sizeof(h3));
	h4 = fnv1a_hash_step(h4, dir_path, strlen(dir_path));

	snprintf(g_stats_key, sizeof(g_stats_key), "%08lX%08lX%08lX%08lX", h1, h2, h3, h4);
	snprintf(g_player_id, sizeof(g_player_id), "%08lX%08lX", h1 ^ h3, h2 ^ h4);

	fp = fopen("Faber_data/device_identity.txt", "w");
	if (fp)
	{
		fprintf(fp, "%s:%s\n", g_player_id, g_stats_key);
		fclose(fp);
	}
}

static void normalize_map_key(char const *map_name, char *out_key, size_t key_size)
{
	char const *base = map_name ? map_name : "unknown";
	size_t i = 0;

	if (!out_key || key_size == 0)
		return;

	for (i = 0; map_name && map_name[i] != '\0'; i++)
	{
		if (map_name[i] == '\\' || map_name[i] == '/')
			base = map_name + i + 1;
	}

	for (i = 0; base[i] != '\0' && base[i] != ';' && base[i] != ':' && i + 1 < key_size; i++)
	{
		char c = base[i];
		out_key[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}
	out_key[i] = '\0';
}

static long get_active_profile_index(long requested_profile_index)
{
	struct player_profile profile;
	short local_player;
	long prof;
	word count = 1;

	/* Fast path: avoid reading and SHA1-hashing blam.sav from disk on every stats query/kill
	   when the active profile is already loaded in memory */
	if (g_career_loaded && g_loaded_profile_index != NONE)
	{
		if (requested_profile_index == g_loaded_profile_index)
			return g_loaded_profile_index;
		if (requested_profile_index == NONE)
		{
			long ui_prof = player_ui_get_active_player_profile_index(0);
			if (ui_prof == g_loaded_profile_index || ui_prof == NONE)
				return g_loaded_profile_index;
		}
	}

	if (requested_profile_index != NONE)
	{
		return requested_profile_index;
	}

	for (local_player = 0; local_player < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local_player++)
	{
		prof = player_ui_get_active_player_profile_index(local_player);
		if (prof != NONE && player_profile_get(prof, &profile))
			return prof;
	}

	prof = player_ui_get_player1_last_used_profile_index();
	if (prof != NONE && player_profile_get(prof, &profile))
		return prof;

	prof = NONE;
	player_profiles_enumerate_available_to_local_player_index(NONE, &count, &prof, FALSE);
	if ((short)count > 0 && prof != NONE && player_profile_get(prof, &profile))
		return prof;

	return NONE;
}

static boolean get_profile_file_path(long profile_index, char const *filename, char *out_path, size_t path_size)
{
	char dir_path[260];

	if (!out_path || path_size == 0 || !filename)
		return FALSE;

	dir_path[0] = '\0';
	if (profile_index != NONE && player_profile_get_enclosing_directory_path(profile_index, dir_path) && dir_path[0])
	{
		snprintf(out_path, path_size, "%s%s", dir_path, filename);
		return TRUE;
	}

	snprintf(out_path, path_size, "u:\\saved_games\\%s", filename);
	return TRUE;
}

static boolean read_xbox_text_file(char const *path, char **out_buf)
{
	HANDLE file;
	DWORD size, read_bytes = 0;
	char *buf;

	if (!out_buf)
		return FALSE;
	*out_buf = NULL;
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	size = GetFileSize(file, NULL);
	if (size == INVALID_FILE_SIZE || size == 0 || size > 512 * 1024)
	{
		CloseHandle(file);
		return FALSE;
	}
	buf = (char *)malloc((size_t)size + 1);
	if (!buf)
	{
		CloseHandle(file);
		return FALSE;
	}
	if (!ReadFile(file, buf, size, &read_bytes, NULL) || read_bytes == 0)
	{
		free(buf);
		CloseHandle(file);
		return FALSE;
	}
	buf[read_bytes] = '\0';
	CloseHandle(file);
	*out_buf = buf;
	return TRUE;
}

static boolean write_xbox_text_file(char const *path, char const *content, size_t length)
{
	HANDLE file;
	DWORD written = 0;
	boolean ok;

	if (!path || !content)
		return FALSE;
	file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	ok = WriteFile(file, content, (DWORD)length, &written, NULL) && written == (DWORD)length;
	CloseHandle(file);
	return ok;
}

/* ---------- Achievement Disabling (Terminal Commands & Infinite Skulls) ---------- */

static boolean g_terminal_command_used_in_session = FALSE;

static void print_hud_notice_to_all_local_players(char const *text)
{
	wchar_t msg[96];
	short c, lp;

	if (!text || !game_in_progress())
		return;
	for (c = 0; text[c] && c < (short)NUMBEROF(msg) - 1; c++)
		msg[c] = (wchar_t)(unsigned char)text[c];
	msg[c] = 0;
	for (lp = 0; lp < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; lp++)
	{
		if (local_player_get_player_index(lp) != NONE)
			hud_print_message(lp, msg);
	}
}

void firefight_stats_notify_terminal_command_used(void)
{
	if (!g_terminal_command_used_in_session)
	{
		g_terminal_command_used_in_session = TRUE;
#if OPENCE_ENABLE_ACHIEVEMENTS
		print_hud_notice_to_all_local_players("Terminal command used - achievements disabled for this session.");
#endif
	}
}

void firefight_stats_reset_session_terminal_flag(void)
{
	g_terminal_command_used_in_session = FALSE;
}

boolean firefight_stats_achievements_disabled(void)
{
	struct skull_set active_cfg = skulls_set_from_text(config_string("game.skulls_active"));

	if (g_terminal_command_used_in_session)
		return TRUE;
	if (cheat.deathless_player || cheat.infinite_ammo || cheat.bottomless_clip ||
		cheat.medusa || cheat.omnipotent || cheat.jetpack)
	{
		return TRUE;
	}
	if (skull_active(_skull_bandanna) || skull_active(_skull_scarab) || skull_active(_skull_envy))
		return TRUE;
	/* (the campaign's skulls setting is not Firefight's: skull_active has
	those the game has on) */
	(void)active_cfg;
	return FALSE;
}

/* ---------- the service ranks (service_rank.c) ---------- */

static void compute_mcc_rank_from_xp(long total_xp, struct firefight_mcc_rank_info *out_rank)
{
	service_rank_compute_from_xp(total_xp, out_rank);
}

/* ---------- Halo 3 Ranked Mode Progression (42 Ranks, 0..100 Points: +2 Win, -2 Loss, 0 Draw) ---------- */

static char const *const s_h3_ranked_names[42] =
{
	"Recruit",
	"Apprentice", "Apprentice (Grade 2)",
	"Private", "Private (Grade 2)",
	"Corporal", "Corporal (Grade 2)",
	"Sergeant", "Sergeant (Grade 2)", "Sergeant (Grade 3)",
	"Gunnery Sergeant", "Gunnery Sergeant (Grade 2)", "Gunnery Sergeant (Grade 3)", "Master Sergeant",
	"Lieutenant", "Lieutenant (Grade 2)", "Lieutenant (Grade 3)", "First Lieutenant",
	"Captain", "Captain (Grade 2)", "Captain (Grade 3)", "Staff Captain",
	"Major", "Major (Grade 2)", "Major (Grade 3)", "Field Major",
	"Commander", "Commander (Grade 2)", "Commander (Grade 3)", "Strike Commander",
	"Colonel", "Colonel (Grade 2)", "Colonel (Grade 3)", "Force Colonel",
	"Brigadier", "Brigadier (Grade 2)", "Brigadier (Grade 3)", "Brigadier General",
	"General", "General (Grade 2)", "General (Grade 3)", "General (Grade 4)"
};

static short const s_h3_ranked_min_points[42] =
{
	 0,
	 2,  4,
	 6,  8,
	10, 12,
	14, 16, 18,
	20, 22, 24, 26,
	28, 30, 32, 34,
	36, 38, 40, 42,
	44, 46, 48, 50,
	52, 54, 56, 60,
	62, 64, 68, 72,
	76, 80, 84, 88,
	90, 94, 98, 100
};

static void compute_h3_ranked_from_points(
	long points,
	long wins,
	long losses,
	long draws,
	struct firefight_h3_ranked_info *out_ranked)
{
	long pts = PIN(points, 0L, 100L);
	short idx = 0;
	short i;

	for (i = 41; i >= 0; i--)
	{
		if (pts >= (long)s_h3_ranked_min_points[i])
		{
			idx = i;
			break;
		}
	}

	out_ranked->rank_index = idx;
	out_ranked->rank_name = s_h3_ranked_names[idx];
	out_ranked->points = pts;
	out_ranked->next_rank_points = (idx < 41) ? (long)s_h3_ranked_min_points[idx + 1] : 100L;
	out_ranked->wins = MAX(0L, wins);
	out_ranked->losses = MAX(0L, losses);
	out_ranked->draws = MAX(0L, draws);
}

/* ---------- MCC Daily & Weekly Challenges (Campaign, Firefight, Multiplayer) ---------- */

enum challenge_metric_type
{
	_chal_metric_kills = 0,
	_chal_metric_headshots,
	_chal_metric_melee,
	_chal_metric_grenades,
	_chal_metric_heavy_kills,
	_chal_metric_completions,
	_chal_metric_heroic_clear,
	_chal_metric_legendary_clear,
	_chal_metric_score,
	_chal_metric_waves,
	_chal_metric_medals,
	_chal_metric_close_combat /* melee + grenades */
};

struct challenge_template_def
{
	char const *title;
	char const *description;
	short category;
	short period;
	short metric;
	long target;
	long xp_reward;
};

static struct challenge_template_def const s_challenge_pool[] =
{
	/* Campaign Daily (0..7) */
	{ "Pillar to Post", "Kill 50 enemies in Campaign missions", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_kills, 50, 2500 },
	{ "Precision Instrument", "Kill 20 enemies with headshots in Campaign", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_headshots, 20, 2500 },
	{ "Up Close & Personal", "Kill 15 enemies with melee attacks in Campaign", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_melee, 15, 2500 },
	{ "Pineapple Express", "Kill 10 enemies with grenades in Campaign", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_grenades, 10, 2500 },
	{ "Mission Accomplished", "Complete 1 Campaign mission", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_completions, 1, 4000 },
	{ "Heroic Valor", "Complete a Campaign mission on Heroic or higher", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_heroic_clear, 1, 5000 },
	{ "Combat Scorer", "Earn 15,000 points in Campaign scoring", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_score, 15000, 3500 },
	{ "Big Game Hunter", "Kill 10 Elites or Hunters in Campaign", _ff_challenge_cat_campaign, _ff_challenge_period_daily, _chal_metric_heavy_kills, 10, 3000 },

	/* Campaign Weekly (8..13) */
	{ "Covenant Bane", "Kill 300 enemies across Campaign missions", _ff_challenge_cat_campaign, _ff_challenge_period_weekly, _chal_metric_kills, 300, 12000 },
	{ "Campaign Veteran", "Complete 3 Campaign missions", _ff_challenge_cat_campaign, _ff_challenge_period_weekly, _chal_metric_completions, 3, 15000 },
	{ "Legendary Conqueror", "Complete a Campaign mission on Legendary", _ff_challenge_cat_campaign, _ff_challenge_period_weekly, _chal_metric_legendary_clear, 1, 20000 },
	{ "Deadeye Sharpshooter", "Score 100 headshot kills in Campaign", _ff_challenge_cat_campaign, _ff_challenge_period_weekly, _chal_metric_headshots, 100, 12500 },
	{ "High Roller", "Earn 75,000 total Campaign score points", _ff_challenge_cat_campaign, _ff_challenge_period_weekly, _chal_metric_score, 75000, 15000 },
	{ "Close Quarters Master", "Get 50 melee or grenade kills in Campaign", _ff_challenge_cat_campaign, _ff_challenge_period_weekly, _chal_metric_close_combat, 50, 10000 },

	/* Firefight Daily (14..21) */
	{ "Hold the Line", "Kill 75 enemies in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_kills, 75, 3000 },
	{ "Wave Breaker", "Clear 5 waves in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_waves, 5, 3500 },
	{ "Pop Goes the Grunt", "Score 25 headshot kills in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_headshots, 25, 3000 },
	{ "Heavy Ordnance", "Kill 10 Boss or Heavy enemies in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_heavy_kills, 10, 4000 },
	{ "Brawler", "Kill 15 enemies with melee in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_melee, 15, 2500 },
	{ "Set Survivor", "Complete 1 full Set in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_completions, 1, 6000 },
	{ "Score Attack", "Earn 20,000 points in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_score, 20000, 3500 },
	{ "Frag Out", "Kill 15 enemies with grenades in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_daily, _chal_metric_grenades, 15, 3000 },

	/* Firefight Weekly (22..27) */
	{ "ODST Endurance", "Complete 3 full Sets in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_weekly, _chal_metric_completions, 3, 18000 },
	{ "Exterminator", "Kill 500 enemies in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_weekly, _chal_metric_kills, 500, 15000 },
	{ "Wave Crusher", "Clear 30 waves in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_weekly, _chal_metric_waves, 30, 15000 },
	{ "Boss Slayer", "Kill 50 Boss or Heavy enemies in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_weekly, _chal_metric_heavy_kills, 50, 16000 },
	{ "Firefight Millionaire", "Score 150,000 points in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_weekly, _chal_metric_score, 150000, 18000 },
	{ "Skull Collector", "Score 125 headshot kills in Firefight", _ff_challenge_cat_firefight, _ff_challenge_period_weekly, _chal_metric_headshots, 125, 12500 },

	/* Multiplayer Daily (28..33) */
	{ "Spartan Skirmish", "Complete 1 Multiplayer match", _ff_challenge_cat_multiplayer, _ff_challenge_period_daily, _chal_metric_completions, 1, 3000 },
	{ "Slayer Specialist", "Get 15 kills in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_daily, _chal_metric_kills, 15, 3500 },
	{ "Headhunter", "Get 5 headshot kills in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_daily, _chal_metric_headshots, 5, 3000 },
	{ "Beatdown Artist", "Get 5 melee kills in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_daily, _chal_metric_melee, 5, 2500 },
	{ "Medal Collector", "Earn 10 medals across any mode", _ff_challenge_cat_multiplayer, _ff_challenge_period_daily, _chal_metric_medals, 10, 3000 },
	{ "Combat Assist", "Earn 1,500 points in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_daily, _chal_metric_score, 1500, 2500 },

	/* Multiplayer Weekly (34..39) */
	{ "Arena Gladiator", "Complete 5 Multiplayer matches", _ff_challenge_cat_multiplayer, _ff_challenge_period_weekly, _chal_metric_completions, 5, 14000 },
	{ "Centurion Slayer", "Get 75 kills in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_weekly, _chal_metric_kills, 75, 16000 },
	{ "Precision Spartan", "Get 25 headshot kills in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_weekly, _chal_metric_headshots, 25, 12500 },
	{ "Close Combatant", "Get 20 melee or grenade kills in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_weekly, _chal_metric_close_combat, 20, 12000 },
	{ "Decorated Veteran", "Earn 50 medals across any mode", _ff_challenge_cat_multiplayer, _ff_challenge_period_weekly, _chal_metric_medals, 50, 15000 },
	{ "Score Leader", "Earn 7,500 points in Multiplayer", _ff_challenge_cat_multiplayer, _ff_challenge_period_weekly, _chal_metric_score, 7500, 14000 }
};

#define CHALLENGE_POOL_COUNT ((short)NUMBEROF(s_challenge_pool))
#define CHALLENGES_PER_BUCKET 4

static long g_challenge_day_id = 0;
static long g_challenge_week_id = 0;
static long g_daily_progress[CHALLENGE_POOL_COUNT];
static unsigned long g_daily_completed_hi = 0;
static unsigned long g_daily_completed_lo = 0;
static long g_weekly_progress[CHALLENGE_POOL_COUNT];
static unsigned long g_weekly_completed_hi = 0;
static unsigned long g_weekly_completed_lo = 0;

static long current_utc_day_id(void)
{
	time_t now = time(NULL);
	return (long)(now > 0 ? (now / 86400) : 20000);
}

static long current_utc_week_id(void)
{
	time_t now = time(NULL);
	return (long)(now > 0 ? ((now - 345600) / 604800) : 2850);
}

static void check_and_rotate_challenge_periods(void)
{
	long d = current_utc_day_id();
	long w = current_utc_week_id();

	if (g_challenge_day_id != d)
	{
		g_challenge_day_id = d;
		csmemset(g_daily_progress, 0, sizeof(g_daily_progress));
		g_daily_completed_hi = 0;
		g_daily_completed_lo = 0;
	}
	if (g_challenge_week_id != w)
	{
		g_challenge_week_id = w;
		csmemset(g_weekly_progress, 0, sizeof(g_weekly_progress));
		g_weekly_completed_hi = 0;
		g_weekly_completed_lo = 0;
	}
}

static short select_active_challenge_indices(short category, short period, short *out_indices, short max_out)
{
	short pool_indices[16];
	short pool_count = 0;
	short i, pick;
	unsigned long seed;

	check_and_rotate_challenge_periods();
	for (i = 0; i < CHALLENGE_POOL_COUNT && pool_count < 16; i++)
	{
		if (s_challenge_pool[i].category == category && s_challenge_pool[i].period == period)
			pool_indices[pool_count++] = i;
	}
	seed = (unsigned long)((period == _ff_challenge_period_daily ? g_challenge_day_id : g_challenge_week_id) * 2654435761UL) ^
		((unsigned long)(category + 1) * 0x9E3779B9UL);
	for (i = pool_count - 1; i > 0; i--)
	{
		short j;
		short tmp;
		seed = seed * 1664525UL + 1013904223UL;
		j = (short)((seed >> 16) % (unsigned long)(i + 1));
		tmp = pool_indices[i];
		pool_indices[i] = pool_indices[j];
		pool_indices[j] = tmp;
	}
	pick = MIN(pool_count, MIN(max_out, (short)CHALLENGES_PER_BUCKET));
	for (i = 0; i < pick; i++)
		out_indices[i] = pool_indices[i];
	return pick;
}

static boolean is_challenge_done_bit(short period, short pool_idx)
{
	unsigned long lo = (period == _ff_challenge_period_daily) ? g_daily_completed_lo : g_weekly_completed_lo;
	unsigned long hi = (period == _ff_challenge_period_daily) ? g_daily_completed_hi : g_weekly_completed_hi;
	if (pool_idx < 32)
		return (lo & (1UL << pool_idx)) != 0;
	return (hi & (1UL << (pool_idx - 32))) != 0;
}

static void set_challenge_done_bit(short period, short pool_idx)
{
	if (period == _ff_challenge_period_daily)
	{
		if (pool_idx < 32)
			g_daily_completed_lo |= (1UL << pool_idx);
		else
			g_daily_completed_hi |= (1UL << (pool_idx - 32));
	}
	else
	{
		if (pool_idx < 32)
			g_weekly_completed_lo |= (1UL << pool_idx);
		else
			g_weekly_completed_hi |= (1UL << (pool_idx - 32));
	}
}

static void advance_challenge_metric(short category, short metric, long delta)
{
	short period;

#if !OPENCE_ENABLE_EXT_CHALLENGES
	return;
#endif
	if (delta <= 0 || firefight_stats_session_withheld())
		return;
	check_and_rotate_challenge_periods();

	for (period = 0; period < FIREFIGHT_CHALLENGE_PERIOD_COUNT; period++)
	{
		short active_idx[CHALLENGES_PER_BUCKET];
		short count = select_active_challenge_indices(category, period, active_idx, CHALLENGES_PER_BUCKET);
		short k;

		for (k = 0; k < count; k++)
		{
			short idx = active_idx[k];
			struct challenge_template_def const *def = &s_challenge_pool[idx];
			long *prog = (period == _ff_challenge_period_daily) ? &g_daily_progress[idx] : &g_weekly_progress[idx];

			if (def->metric != metric)
				continue;
			*prog += delta;
			if (*prog >= def->target && !is_challenge_done_bit(period, idx))
			{
				char msg[96];
				*prog = def->target;
				set_challenge_done_bit(period, idx);
				snprintf(msg, sizeof(msg), "CHALLENGE COMPLETE: %s (+%ld XP)", def->title, def->xp_reward);
				print_hud_notice_to_all_local_players(msg);
				firefight_stats_add_xp(def->xp_reward, def->title);
			}
		}
	}
}

static void save_profile_records(long profile_index);
static void fill_sync_telemetry_session(long profile_index, char const *category, struct firefight_telemetry_session *out_session);
static void dispatch_telemetry_post_async(long profile_index, struct firefight_telemetry_session const *session);
static boolean s_initial_web_sync_done = FALSE;

static boolean is_ach_bit_set(short ach_id)
{
	if (ach_id < 0 || ach_id >= HALO_ACHIEVEMENT_COUNT)
		return FALSE;
	if (ach_id < 32)
		return (g_career_stats.achievements_lo & (1UL << ach_id)) != 0;
	return (g_career_stats.achievements_hi & (1UL << (ach_id - 32))) != 0;
}

static short compute_unlocked_achievement_score(short *out_unlocked_count)
{
	short score = 0;
	short count = 0;
	short i;

	for (i = 0; i < HALO_ACHIEVEMENT_COUNT; i++)
	{
		if (is_ach_bit_set(i))
		{
			score += s_achievement_defs[i].points;
			count++;
		}
	}
	if (out_unlocked_count)
		*out_unlocked_count = count;
	return score;
}

static boolean set_ach_bit_internal(short ach_id, boolean announce_hud)
{
	short lp;

#if !OPENCE_ENABLE_ACHIEVEMENTS
	return FALSE;
#endif
	if (ach_id < 0 || ach_id >= HALO_ACHIEVEMENT_COUNT || is_ach_bit_set(ach_id))
		return FALSE;
	/* Block earning achievements during a session if terminal commands or infinite ammo/grenade/clip skulls were used */
	if (announce_hud && firefight_stats_achievements_disabled())
		return FALSE;
	if (ach_id < 32)
		g_career_stats.achievements_lo |= (1UL << ach_id);
	else
		g_career_stats.achievements_hi |= (1UL << (ach_id - 32));

	if (announce_hud && game_in_progress())
	{
		wchar_t msg[80];
		wchar_t const *t_str = s_achievement_defs[ach_id].title;
		char text[80];
		char narrow_title[48];
		short c;

		for (c = 0; t_str && t_str[c] && c < (short)sizeof(narrow_title) - 1; c++)
			narrow_title[c] = (char)t_str[c];
		narrow_title[c] = '\0';

		snprintf(
			text,
			sizeof(text),
			"ACHIEVEMENT UNLOCKED: %s (+%dG)",
			narrow_title,
			(int)s_achievement_defs[ach_id].points);
		for (c = 0; text[c] && c < (short)NUMBEROF(msg) - 1; c++)
			msg[c] = (wchar_t)(unsigned char)text[c];
		msg[c] = 0;
		for (lp = 0; lp < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; lp++)
		{
			if (local_player_get_player_index(lp) != NONE)
				hud_print_message(lp, msg);
		}
	}
	return TRUE;
}

static short camp_map_index_from_key(char const *map_key)
{
	static char const *const s_camp_keys[10] =
	{
		"a10", "a30", "a50", "b30", "b40",
		"c10", "c20", "c40", "d20", "d40"
	};
	short i;

	if (!map_key || !map_key[0])
		return NONE;
	for (i = 0; i < 10; i++)
	{
		if (strcmp(map_key, s_camp_keys[i]) == 0)
			return i;
	}
	return NONE;
}

static boolean sync_retroactive_achievements(boolean announce_hud)
{
	static char const *const s_camp_levels[10] =
	{
		"levels\\a10\\a10", "levels\\a30\\a30", "levels\\a50\\a50",
		"levels\\b30\\b30", "levels\\b40\\b40", "levels\\c10\\c10",
		"levels\\c20\\c20", "levels\\c40\\c40", "levels\\d20\\d20",
		"levels\\d40\\d40"
	};
	static char const *const s_camp_short[10] =
	{
		"a10", "a30", "a50", "b30", "b40",
		"c10", "c20", "c40", "d20", "d40"
	};
	boolean changed = FALSE;
	short cleared_count = 0;
	short min_cleared_diff = 99;
	short i;

	if (g_career_stats.total_sets_completed >= 1)
	{
		if (set_ach_bit_internal(_ach_ff_first_set, announce_hud))
			changed = TRUE;
	}

	for (i = 0; i < 10; i++)
	{
		struct campaign_score_record crec;
		boolean has_c = campaign_score_level_record(s_camp_levels[i], NONE, &crec);
		boolean lvl_cleared = has_c;
		short lvl_diff = (has_c && crec.difficulty >= 0 && crec.difficulty <= 3) ? crec.difficulty : -1;
		short r;

		for (r = 0; r < g_map_record_count; r++)
		{
			if (!g_map_records[r].valid || strcmp(g_map_records[r].map_key, s_camp_short[i]) != 0)
				continue;
			if (g_map_records[r].record.best_set == 0 && g_map_records[r].record.best_score > 0)
			{
				lvl_cleared = TRUE;
				if (g_map_records[r].record.best_difficulty > lvl_diff &&
					g_map_records[r].record.best_difficulty <= 3)
				{
					lvl_diff = g_map_records[r].record.best_difficulty;
				}
			}
			if (g_map_records[r].record.best_set > 1 ||
				(g_map_records[r].record.best_set == 1 && g_map_records[r].record.best_round >= FIREFIGHT_ROUNDS_PER_SET))
			{
				if (set_ach_bit_internal((short)(_ach_ff_a10 + i), announce_hud))
					changed = TRUE;
				if (set_ach_bit_internal(_ach_ff_first_set, announce_hud))
					changed = TRUE;
			}
		}

		if (lvl_cleared)
		{
			if (set_ach_bit_internal((short)(_ach_camp_a10 + i), announce_hud))
				changed = TRUE;
			if (lvl_diff >= 1 && set_ach_bit_internal(_ach_lvl_normal, announce_hud))
				changed = TRUE;
			if (lvl_diff >= 2 && set_ach_bit_internal(_ach_lvl_heroic, announce_hud))
				changed = TRUE;
			if (lvl_diff >= 3 && set_ach_bit_internal(_ach_lvl_legendary, announce_hud))
				changed = TRUE;
			cleared_count++;
			if (lvl_diff < min_cleared_diff)
				min_cleared_diff = MAX((short)0, lvl_diff);
		}
	}

	if (cleared_count >= 10)
	{
		if (set_ach_bit_internal(_ach_camp_complete, announce_hud))
			changed = TRUE;
		if (min_cleared_diff >= 1 && set_ach_bit_internal(_ach_camp_normal, announce_hud))
			changed = TRUE;
		if (min_cleared_diff >= 2 && set_ach_bit_internal(_ach_camp_heroic, announce_hud))
			changed = TRUE;
		if (min_cleared_diff >= 3 && set_ach_bit_internal(_ach_camp_legendary, announce_hud))
			changed = TRUE;
	}

	return changed;
}

static void load_profile_records(long profile_index)
{
	char path[300];
	char line[512];
	char dev_id[17];
	char dev_key[33];
	char *file_buf = NULL;
	boolean needs_identity_save = FALSE;

	profile_index = get_active_profile_index(profile_index);
	if (g_loaded_profile_index == profile_index && g_career_loaded)
	{
		check_and_rotate_challenge_periods();
		return;
	}

	csmemset(&g_career_stats, 0, sizeof(g_career_stats));
	{
		struct firefight_emblem_info const default_emblem = FIREFIGHT_EMBLEM_DEFAULT;
		g_profile_emblem = default_emblem;
	}
	g_career_loaded = TRUE;
	g_map_record_count = 0;
	g_loaded_profile_index = profile_index;
	g_player_id[0] = '\0';
	g_stats_key[0] = '\0';
	g_challenge_day_id = current_utc_day_id();
	g_challenge_week_id = current_utc_week_id();
	csmemset(g_daily_progress, 0, sizeof(g_daily_progress));
	csmemset(g_weekly_progress, 0, sizeof(g_weekly_progress));
	g_daily_completed_hi = 0;
	g_daily_completed_lo = 0;
	g_weekly_completed_hi = 0;
	g_weekly_completed_lo = 0;

	if (!get_profile_file_path(profile_index, FIREFIGHT_RECORDS_FILE, path, sizeof(path)))
		return;

	if (!read_xbox_text_file(path, &file_buf) || !file_buf)
	{
		/* Migrate any existing fallback records from u:\saved_games\firefight_records.txt or d:\firefight_records.txt */
		if (read_xbox_text_file("u:\\saved_games\\" FIREFIGHT_RECORDS_FILE, &file_buf) && file_buf)
			needs_identity_save = TRUE;
		else if (read_xbox_text_file(FIREFIGHT_RECORDS_FILE, &file_buf) && file_buf)
			needs_identity_save = TRUE;
	}

	if (file_buf)
	{
		char const *cursor = file_buf;
		while (*cursor)
		{
			size_t line_len = 0;
			while (cursor[line_len] != '\0' && cursor[line_len] != '\n')
				line_len++;
			if (line_len >= sizeof(line))
				line_len = sizeof(line) - 1;
			memcpy(line, cursor, line_len);
			line[line_len] = '\0';
			cursor += line_len;
			if (*cursor == '\n')
				cursor++;

			if (strncmp(line, "IDENTITY:", 9) == 0)
			{
				sscanf(line + 9, "%16[^:\r\n]:%32[^\r\n]", g_player_id, g_stats_key);
			}
			else if (strncmp(line, "CAREER:", 7) == 0)
			{
				sscanf(
					line + 7,
					"%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld",
					&g_career_stats.total_matches,
					&g_career_stats.total_sets_completed,
					&g_career_stats.total_waves_cleared,
					&g_career_stats.total_kills,
					&g_career_stats.total_boss_kills,
					&g_career_stats.total_score,
					&g_career_stats.total_seconds,
					&g_career_stats.total_deaths,
					&g_career_stats.total_assists,
					&g_career_stats.best_kill_spree);
			}
			else if (strncmp(line, "FF_STATS:", 9) == 0)
			{
				sscanf(
					line + 9,
					"%ld:%ld:%ld:%ld:%ld:%ld:%ld",
					&g_career_stats.ff_matches,
					&g_career_stats.ff_kills,
					&g_career_stats.ff_deaths,
					&g_career_stats.ff_assists,
					&g_career_stats.ff_score,
					&g_career_stats.ff_seconds,
					&g_career_stats.ff_best_spree);
			}
			else if (strncmp(line, "MP_STATS:", 9) == 0)
			{
				sscanf(
					line + 9,
					"%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld",
					&g_career_stats.mp_matches,
					&g_career_stats.mp_kills,
					&g_career_stats.mp_deaths,
					&g_career_stats.mp_assists,
					&g_career_stats.mp_score,
					&g_career_stats.ranked_matches,
					&g_career_stats.ranked_kills,
					&g_career_stats.ranked_deaths,
					&g_career_stats.ranked_assists,
					&g_career_stats.ranked_csr);
			}
			else if (strncmp(line, "CAREER_XP:", 10) == 0)
			{
				sscanf(line + 10, "%ld", &g_career_stats.career_xp);
			}
			else if (strncmp(line, "H3_RANKED:", 10) == 0)
			{
				long r_pts = 0, r_w = 0, r_l = 0, r_d = 0;
				if (sscanf(line + 10, "%ld:%ld:%ld:%ld", &r_pts, &r_w, &r_l, &r_d) >= 1)
				{
					g_career_stats.ranked_points = PIN(r_pts, 0L, 100L);
					g_career_stats.ranked_wins = MAX(0L, r_w);
					g_career_stats.ranked_losses = MAX(0L, r_l);
					g_career_stats.ranked_draws = MAX(0L, r_d);
					g_career_stats.ranked_csr = g_career_stats.ranked_points;
				}
			}
			else if (strncmp(line, "EMBLEM2:", 8) == 0)
			{
				int fg, bg, sec, c1, c2, c3, np = 0;
				if (sscanf(line + 8, "%d:%d:%d:%d:%d:%d:%d", &fg, &bg, &sec, &c1, &c2, &c3, &np) >= 6)
				{
					struct firefight_emblem_info loaded;
					loaded.foreground = (short)fg;
					loaded.background = (short)bg;
					loaded.secondary_layer = (sec != 0) ? TRUE : FALSE;
					loaded.primary_color = (short)c1;
					loaded.secondary_color = (short)c2;
					loaded.background_color = (short)c3;
					loaded.nameplate = (short)np;
					store_profile_emblem(&loaded);
				}
			}
			else if (strncmp(line, "EMBLEM:", 7) == 0)
			{
				/* a profile saved when emblems were drawn by code: the sprites nearest what it showed */
				int fg = 0, bg = 1, sec = 1, c1 = 11, c2 = 0, c3 = 3, np = 0;
				if (sscanf(line + 7, "%d:%d:%d:%d:%d:%d:%d", &fg, &bg, &sec, &c1, &c2, &c3, &np) >= 6)
				{
					struct firefight_emblem_info loaded;
					loaded.foreground = s_old_emblem_foregrounds[PIN(fg, 0, (int)NUMBEROF(s_old_emblem_foregrounds) - 1)];
					loaded.background = s_old_emblem_backgrounds[PIN(bg, 0, (int)NUMBEROF(s_old_emblem_backgrounds) - 1)];
					loaded.secondary_layer = (sec != 0) ? TRUE : FALSE;
					loaded.primary_color = firefight_emblem_color_from_armor((short)c1);
					loaded.secondary_color = firefight_emblem_color_from_armor((short)c2);
					loaded.background_color = firefight_emblem_color_from_armor((short)c3);
					loaded.nameplate = (short)np;
					store_profile_emblem(&loaded);
				}
			}
			else if (strncmp(line, "DAILY_CHAL:", 11) == 0)
			{
				long saved_day = 0;
				unsigned long hi = 0, lo = 0;
				int consumed = 0;
				if (sscanf(line + 11, "%ld:%lx:%lx:%n", &saved_day, &hi, &lo, &consumed) >= 3 &&
					saved_day == current_utc_day_id())
				{
					char const *p = line + 11 + consumed;
					short c_idx;
					g_challenge_day_id = saved_day;
					g_daily_completed_hi = hi;
					g_daily_completed_lo = lo;
					for (c_idx = 0; c_idx < CHALLENGE_POOL_COUNT && *p; c_idx++)
					{
						g_daily_progress[c_idx] = strtol(p, (char **)&p, 10);
						if (*p == ':')
							p++;
					}
				}
			}
			else if (strncmp(line, "WEEKLY_CHAL:", 12) == 0)
			{
				long saved_week = 0;
				unsigned long hi = 0, lo = 0;
				int consumed = 0;
				if (sscanf(line + 12, "%ld:%lx:%lx:%n", &saved_week, &hi, &lo, &consumed) >= 3 &&
					saved_week == current_utc_week_id())
				{
					char const *p = line + 12 + consumed;
					short c_idx;
					g_challenge_week_id = saved_week;
					g_weekly_completed_hi = hi;
					g_weekly_completed_lo = lo;
					for (c_idx = 0; c_idx < CHALLENGE_POOL_COUNT && *p; c_idx++)
					{
						g_weekly_progress[c_idx] = strtol(p, (char **)&p, 10);
						if (*p == ':')
							p++;
					}
				}
			}
			else if (strncmp(line, "ACHIEVEMENTS:", 13) == 0)
			{
				sscanf(
					line + 13,
					"%lx:%lx",
					&g_career_stats.achievements_hi,
					&g_career_stats.achievements_lo);
			}
			else if (strncmp(line, "MEDALS:", 7) == 0)
			{
				char const *p = line + 7;
				short m;
				for (m = 0; m < 38 && *p; m++)
				{
					g_career_stats.medal_counts[m] = strtol(p, (char **)&p, 10);
					if (*p == ':')
						p++;
				}
			}
			else if (strncmp(line, "MP_MEDALS:", 10) == 0)
			{
				char const *p = line + 10;
				short m;
				for (m = 0; m < 38 && *p; m++)
				{
					g_career_stats.mp_medal_counts[m] = strtol(p, (char **)&p, 10);
					if (*p == ':')
						p++;
				}
			}
			else if (strncmp(line, "MAP:", 4) == 0 && g_map_record_count < MAX_TRACKED_MAPS)
			{
				struct firefight_stored_map_entry *entry = &g_map_records[g_map_record_count];
				int set_v = 0, rnd_v = 0, wave_v = 0, diff_v = 0, skulls_v = 0;

				csmemset(entry, 0, sizeof(*entry));
				if (sscanf(
					line + 4,
					"%31[^:]:%ld:%d:%d:%d:%ld:%d:%d:%ld",
					entry->map_key,
					&entry->record.best_score,
					&set_v,
					&rnd_v,
					&wave_v,
					&entry->record.best_seconds,
					&diff_v,
					&skulls_v,
					&entry->record.best_medals) >= 6)
				{
					entry->valid = TRUE;
					entry->record.best_set = (short)set_v;
					entry->record.best_round = (short)rnd_v;
					entry->record.best_wave = (short)wave_v;
					entry->record.best_difficulty = (short)diff_v;
					entry->record.best_skulls = (short)skulls_v;
					g_map_record_count++;
				}
			}
		}
		free(file_buf);
	}

	/* Enforce device-linked Player ID whenever hardware ID is available */
	if (derive_device_linked_identity(dev_id, sizeof(dev_id), dev_key, sizeof(dev_key)))
	{
		if (strcmp(g_player_id, dev_id) != 0 || strcmp(g_stats_key, dev_key) != 0)
		{
			snprintf(g_player_id, sizeof(g_player_id), "%s", dev_id);
			snprintf(g_stats_key, sizeof(g_stats_key), "%s", dev_key);
			needs_identity_save = TRUE;
		}
	}
	else if (!g_player_id[0] || !g_stats_key[0])
	{
		generate_profile_identity(profile_index);
		needs_identity_save = TRUE;
	}

	if (sync_retroactive_achievements(FALSE))
		needs_identity_save = TRUE;

	if (needs_identity_save)
		save_profile_records(profile_index);

	if (!s_initial_web_sync_done && g_player_id[0] != '\0')
	{
		struct firefight_telemetry_session sync_session;
		s_initial_web_sync_done = TRUE;
		fill_sync_telemetry_session(profile_index, "auto_sync", &sync_session);
		dispatch_telemetry_post_async(profile_index, &sync_session);
	}
}

static void save_profile_records(long profile_index)
{
	char path[300];
	size_t cap = 32768;
	char *buf;
	size_t pos = 0;
	short i;

	profile_index = get_active_profile_index(profile_index);
	if (!g_player_id[0] || !g_stats_key[0])
		generate_profile_identity(profile_index);

	if (!get_profile_file_path(profile_index, FIREFIGHT_RECORDS_FILE, path, sizeof(path)))
		return;

	buf = (char *)malloc(cap);
	if (!buf)
		return;

	pos += (size_t)snprintf(buf + pos, cap - pos, "IDENTITY:%s:%s\n", g_player_id, g_stats_key);
	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"CAREER:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld\n",
		g_career_stats.total_matches,
		g_career_stats.total_sets_completed,
		g_career_stats.total_waves_cleared,
		g_career_stats.total_kills,
		g_career_stats.total_boss_kills,
		g_career_stats.total_score,
		g_career_stats.total_seconds,
		g_career_stats.total_deaths,
		g_career_stats.total_assists,
		g_career_stats.best_kill_spree);
	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"FF_STATS:%ld:%ld:%ld:%ld:%ld:%ld:%ld\n",
		g_career_stats.ff_matches,
		g_career_stats.ff_kills,
		g_career_stats.ff_deaths,
		g_career_stats.ff_assists,
		g_career_stats.ff_score,
		g_career_stats.ff_seconds,
		g_career_stats.ff_best_spree);
	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"MP_STATS:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld\n",
		g_career_stats.mp_matches,
		g_career_stats.mp_kills,
		g_career_stats.mp_deaths,
		g_career_stats.mp_assists,
		g_career_stats.mp_score,
		g_career_stats.ranked_matches,
		g_career_stats.ranked_kills,
		g_career_stats.ranked_deaths,
		g_career_stats.ranked_assists,
		g_career_stats.ranked_csr);
	pos += (size_t)snprintf(buf + pos, cap - pos, "CAREER_XP:%ld\n", g_career_stats.career_xp);
	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"H3_RANKED:%ld:%ld:%ld:%ld\n",
		PIN(g_career_stats.ranked_points, 0L, 100L),
		MAX(0L, g_career_stats.ranked_wins),
		MAX(0L, g_career_stats.ranked_losses),
		MAX(0L, g_career_stats.ranked_draws));
	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"EMBLEM2:%d:%d:%d:%d:%d:%d:%d\n",
		(int)g_profile_emblem.foreground,
		(int)g_profile_emblem.background,
		g_profile_emblem.secondary_layer ? 1 : 0,
		(int)g_profile_emblem.primary_color,
		(int)g_profile_emblem.secondary_color,
		(int)g_profile_emblem.background_color,
		(int)g_profile_emblem.nameplate);

	check_and_rotate_challenge_periods();
	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"DAILY_CHAL:%ld:%08lX:%08lX:",
		g_challenge_day_id,
		g_daily_completed_hi,
		g_daily_completed_lo);
	for (i = 0; i < CHALLENGE_POOL_COUNT && pos + 32 < cap; i++)
	{
		pos += (size_t)snprintf(
			buf + pos,
			cap - pos,
			"%ld%s",
			g_daily_progress[i],
			(i + 1 < CHALLENGE_POOL_COUNT) ? ":" : "\n");
	}

	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"WEEKLY_CHAL:%ld:%08lX:%08lX:",
		g_challenge_week_id,
		g_weekly_completed_hi,
		g_weekly_completed_lo);
	for (i = 0; i < CHALLENGE_POOL_COUNT && pos + 32 < cap; i++)
	{
		pos += (size_t)snprintf(
			buf + pos,
			cap - pos,
			"%ld%s",
			g_weekly_progress[i],
			(i + 1 < CHALLENGE_POOL_COUNT) ? ":" : "\n");
	}

	pos += (size_t)snprintf(
		buf + pos,
		cap - pos,
		"ACHIEVEMENTS:%08lX:%08lX\n",
		g_career_stats.achievements_hi,
		g_career_stats.achievements_lo);

	pos += (size_t)snprintf(buf + pos, cap - pos, "MEDALS:");
	for (i = 0; i < 38 && pos + 32 < cap; i++)
	{
		pos += (size_t)snprintf(buf + pos, cap - pos, "%ld%s", g_career_stats.medal_counts[i], (i < 37) ? ":" : "\n");
	}

	pos += (size_t)snprintf(buf + pos, cap - pos, "MP_MEDALS:");
	for (i = 0; i < 38 && pos + 32 < cap; i++)
	{
		pos += (size_t)snprintf(buf + pos, cap - pos, "%ld%s", g_career_stats.mp_medal_counts[i], (i < 37) ? ":" : "\n");
	}

	for (i = 0; i < g_map_record_count && pos + 128 < cap; i++)
	{
		struct firefight_stored_map_entry const *e = &g_map_records[i];
		if (!e->valid || !e->map_key[0])
			continue;

		pos += (size_t)snprintf(
			buf + pos,
			cap - pos,
			"MAP:%s:%ld:%d:%d:%d:%ld:%d:%d:%ld\n",
			e->map_key,
			e->record.best_score,
			(int)e->record.best_set,
			(int)e->record.best_round,
			(int)e->record.best_wave,
			e->record.best_seconds,
			(int)e->record.best_difficulty,
			(int)e->record.best_skulls,
			e->record.best_medals);
	}

	write_xbox_text_file(path, buf, pos);
	if (strcmp(path, "u:\\saved_games\\" FIREFIGHT_RECORDS_FILE) != 0)
		write_xbox_text_file("u:\\saved_games\\" FIREFIGHT_RECORDS_FILE, buf, pos);
	free(buf);
	g_profile_records_dirty = FALSE;
}

/* ---------- public code */

void firefight_stats_get_player_id(long profile_index, char *out_id, size_t id_size)
{
	if (!out_id || id_size == 0)
		return;

	load_profile_records(profile_index);
	snprintf(out_id, id_size, "%s", g_player_id[0] ? g_player_id : "0000000000000000");
}

short firefight_stats_get_map_record_count(long profile_index)
{
	load_profile_records(profile_index);
	return g_map_record_count;
}

boolean firefight_stats_get_indexed_map_record(
	short index,
	long profile_index,
	char *out_map_key,
	size_t key_size,
	struct firefight_map_record *out_record)
{
	load_profile_records(profile_index);
	if (index < 0 || index >= g_map_record_count || !g_map_records[index].valid)
		return FALSE;

	if (out_map_key && key_size > 0)
		snprintf(out_map_key, key_size, "%s", g_map_records[index].map_key);
	if (out_record)
		*out_record = g_map_records[index].record;
	return TRUE;
}

static char *serialize_telemetry_json_payload(
	long profile_index,
	struct firefight_telemetry_session const *session);

boolean firefight_stats_export_json(
	long profile_index,
	char *out_path,
	size_t path_size)
{
	(void)profile_index;
	if (out_path && path_size > 0)
		out_path[0] = '\0';
	return FALSE;
}

static const char *get_stats_telemetry_url(void)
{
	const char *env_url = getenv("HALO_STATS_URL");
	const char *cfg_url;

	if (env_url && env_url[0])
		return env_url;

	cfg_url = config_string("stats.api_url");
	if (cfg_url && cfg_url[0])
		return cfg_url;

	return DEFAULT_HALO_STATS_API_URL;
}

static void get_active_player_name_utf8(long profile_index, char *out_name, size_t name_size)
{
	struct player_profile profile;
	size_t i;

	if (!out_name || name_size == 0)
		return;

	out_name[0] = '\0';
	profile_index = get_active_profile_index(profile_index);
	if (profile_index != NONE && player_profile_get(profile_index, &profile))
	{
		for (i = 0; i < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH && profile.player_name[i] != 0 && i + 1 < name_size; i++)
		{
			wchar_t ch = profile.player_name[i];
			if (ch == L'"' || ch == L'\\')
				out_name[i] = '_';
			else if (ch >= 32 && ch < 127)
				out_name[i] = (char)ch;
			else
				out_name[i] = '_';
		}
		out_name[i] = '\0';
	}

	if (!out_name[0])
		snprintf(out_name, name_size, "%s", g_player_id[0] ? g_player_id : "Spartan");
}

static long count_unlocked_skulls(void)
{
	struct skull_set u_set = skulls_unlocked_set();
	long unlocked = 0;
	int i;

	for (i = 0; i < NUMBER_OF_SKULLS; i++)
	{
		if (skull_set_has(&u_set, i))
			unlocked++;
	}
	return unlocked;
}

static char *serialize_telemetry_json_payload(
	long profile_index,
	struct firefight_telemetry_session const *session)
{
	size_t capacity = 36864;
	char *buf = (char *)malloc(capacity);
	size_t pos = 0;
	long total_medals = 0;
	long mp_total_medals = 0;
	short ach_unlocked = 0;
	short ach_score = 0;
	boolean has_active_code = FALSE;
	short i;

	if (!buf || !session)
	{
		free(buf);
		return NULL;
	}

	profile_index = get_active_profile_index(profile_index);
	load_profile_records(profile_index);
	if (g_profile_records_dirty)
		save_profile_records(profile_index);
	ach_score = compute_unlocked_achievement_score(&ach_unlocked);

	if (g_active_link_code[0] != '\0' &&
		(g_active_link_expiry_ms == 0 || system_milliseconds() < g_active_link_expiry_ms))
	{
		has_active_code = TRUE;
	}

	for (i = 0; i < 38; i++)
	{
		total_medals += g_career_stats.medal_counts[i];
		mp_total_medals += g_career_stats.mp_medal_counts[i];
	}

	{
		struct firefight_mcc_rank_info rk;
		struct firefight_h3_ranked_info h3_rk;
		struct player_profile prof_data;
		int armor_col = 6;
		if (profile_index != NONE && player_profile_get(profile_index, &prof_data))
			armor_col = (int)PIN(prof_data.primary_color_index, 0, 17);
		compute_mcc_rank_from_xp(g_career_stats.career_xp, &rk);
		compute_h3_ranked_from_points(
			g_career_stats.ranked_points,
			g_career_stats.ranked_wins,
			g_career_stats.ranked_losses,
			g_career_stats.ranked_draws,
			&h3_rk);

		pos += (size_t)snprintf(
			buf + pos,
			capacity - pos,
			"{\n"
			"  \"game\": \"OpenCE : Faber\",\n"
			"  \"schema_version\": 2,\n"
			"  \"category\": \"%s\",\n"
			"  \"player_id\": \"%s\",\n"
			"  \"device_id\": \"%s\",\n"
			"  \"link_code\": \"%s\",\n"
			"  \"player_name\": \"%s\",\n"
			"  \"map_name\": \"%s\",\n"
			"  \"difficulty\": %d,\n"
			"  \"score\": %ld,\n"
			"  \"kills\": %ld,\n"
			"  \"deaths\": %ld,\n"
			"  \"sets_completed\": %ld,\n"
			"  \"rounds_completed\": %ld,\n"
			"  \"waves_completed\": %ld,\n"
			"  \"generators_defended\": %ld,\n"
			"  \"duration_seconds\": %ld,\n"
			"  \"skulls_unlocked_count\": %ld,\n"
			"  \"achievement_score\": %d,\n"
			"  \"max_achievement_score\": 1000,\n"
			"  \"achievements_unlocked\": %d,\n"
			"  \"achievements_total\": %d,\n"
			"  \"achievements_lo\": %lu,\n"
			"  \"achievements_hi\": %lu,\n"
			"  \"career_xp\": %ld,\n"
			"  \"mcc_tour\": %d,\n"
			"  \"mcc_tour_name\": \"%s\",\n"
			"  \"mcc_rank\": %d,\n"
			"  \"mcc_rank_name\": \"%s\",\n"
			"  \"ranked_points\": %ld,\n"
			"  \"ranked_rank_index\": %d,\n"
			"  \"ranked_rank_name\": \"%s\",\n"
			"  \"ranked_wins\": %ld,\n"
			"  \"ranked_losses\": %ld,\n"
			"  \"ranked_draws\": %ld,\n"
			"  \"armor_color\": %d,\n"
			"  \"emblem\": {\n"
			"    \"fg\": %d,\n"
			"    \"bg\": %d,\n"
			"    \"toggle\": %d,\n"
			"    \"c1\": %d,\n"
			"    \"c2\": %d,\n"
			"    \"c3\": %d,\n"
			"    \"nameplate\": %d\n"
			"  },\n"
			"  \"career\": {\n"
			"    \"total_matches\": %ld,\n"
			"    \"total_sets_completed\": %ld,\n"
			"    \"total_waves_cleared\": %ld,\n"
			"    \"total_kills\": %ld,\n"
			"    \"total_boss_kills\": %ld,\n"
			"    \"total_deaths\": %ld,\n"
			"    \"total_assists\": %ld,\n"
			"    \"best_kill_spree\": %ld,\n"
			"    \"total_score\": %ld,\n"
			"    \"total_seconds\": %ld,\n"
			"    \"total_medals\": %ld,\n"
			"    \"mp_total_medals\": %ld,\n"
			"    \"mp_matches\": %ld,\n"
			"    \"mp_kills\": %ld,\n"
			"    \"mp_deaths\": %ld,\n"
			"    \"mp_assists\": %ld,\n"
			"    \"mp_score\": %ld,\n"
			"    \"ranked_matches\": %ld,\n"
			"    \"ranked_kills\": %ld,\n"
			"    \"ranked_deaths\": %ld,\n"
			"    \"ranked_assists\": %ld,\n"
			"    \"ranked_csr\": %ld,\n"
			"    \"ranked_points\": %ld,\n"
			"    \"ranked_wins\": %ld,\n"
			"    \"ranked_losses\": %ld,\n"
			"    \"ranked_draws\": %ld,\n"
			"    \"career_xp\": %ld\n"
			"  },\n"
			"  \"achievements\": {\n",
			session->category[0] ? session->category : "firefight",
			g_player_id[0] ? g_player_id : "0000000000000000",
			g_player_id[0] ? g_player_id : "0000000000000000",
			has_active_code ? g_active_link_code : "",
			session->player_name,
			session->map_name[0] ? session->map_name : "unknown",
			(int)session->difficulty,
			session->score,
			session->kills,
			session->deaths,
			session->sets_completed,
			session->rounds_completed,
			session->waves_completed,
			session->generators_defended,
			session->duration_seconds,
			session->skulls_unlocked_count,
			(int)ach_score,
			(int)ach_unlocked,
			(int)HALO_ACHIEVEMENT_COUNT,
			g_career_stats.achievements_lo,
			g_career_stats.achievements_hi,
			g_career_stats.career_xp,
			(int)rk.tour_number,
			rk.tour_name,
			(int)rk.rank_in_tour,
			rk.rank_name,
			h3_rk.points,
			(int)h3_rk.rank_index,
			h3_rk.rank_name,
			h3_rk.wins,
			h3_rk.losses,
			h3_rk.draws,
			armor_col,
			(int)g_profile_emblem.foreground,
			(int)g_profile_emblem.background,
			g_profile_emblem.secondary_layer ? 1 : 0,
			(int)g_profile_emblem.primary_color,
			(int)g_profile_emblem.secondary_color,
			(int)g_profile_emblem.background_color,
			(int)g_profile_emblem.nameplate,
			g_career_stats.total_matches,
			g_career_stats.total_sets_completed,
			g_career_stats.total_waves_cleared,
			g_career_stats.total_kills,
			g_career_stats.total_boss_kills,
			g_career_stats.total_deaths,
			g_career_stats.total_assists,
			g_career_stats.best_kill_spree,
			g_career_stats.total_score,
			g_career_stats.total_seconds,
			total_medals,
			mp_total_medals,
			g_career_stats.mp_matches,
			g_career_stats.mp_kills,
			g_career_stats.mp_deaths,
			g_career_stats.mp_assists,
			g_career_stats.mp_score,
			g_career_stats.ranked_matches,
			g_career_stats.ranked_kills,
			g_career_stats.ranked_deaths,
			g_career_stats.ranked_assists,
			h3_rk.points,
			h3_rk.points,
			h3_rk.wins,
			h3_rk.losses,
			h3_rk.draws,
			g_career_stats.career_xp);
	}

	for (i = 0; i < HALO_ACHIEVEMENT_COUNT && pos + 128 < capacity; i++)
	{
		pos += (size_t)snprintf(
			buf + pos,
			capacity - pos,
			"    \"%s\": %d%s\n",
			s_achievement_defs[i].key,
			is_ach_bit_set(i) ? 1 : 0,
			(i + 1 < HALO_ACHIEVEMENT_COUNT) ? "," : "");
	}

	pos += (size_t)snprintf(buf + pos, capacity - pos, "  },\n  \"medals\": {\n");

	for (i = 0; i < NUMBER_OF_CAMPAIGN_MEDALS && pos + 128 < capacity; i++)
	{
		pos += (size_t)snprintf(
			buf + pos,
			capacity - pos,
			"    \"%s\": %ld%s\n",
			campaign_medal_descriptions[i].key,
			g_career_stats.medal_counts[i],
			(i + 1 < NUMBER_OF_CAMPAIGN_MEDALS) ? "," : "");
	}

	pos += (size_t)snprintf(buf + pos, capacity - pos, "  },\n  \"mp_medals\": {\n");

	for (i = 0; i < NUMBER_OF_CAMPAIGN_MEDALS && pos + 128 < capacity; i++)
	{
		pos += (size_t)snprintf(
			buf + pos,
			capacity - pos,
			"    \"%s\": %ld%s\n",
			campaign_medal_descriptions[i].key,
			g_career_stats.mp_medal_counts[i],
			(i + 1 < NUMBER_OF_CAMPAIGN_MEDALS) ? "," : "");
	}

	pos += (size_t)snprintf(buf + pos, capacity - pos, "  },\n  \"maps\": [\n");
	for (i = 0; i < g_map_record_count && pos + 512 < capacity; i++)
	{
		struct firefight_stored_map_entry const *e = &g_map_records[i];
		if (!e->valid || !e->map_key[0])
			continue;

		pos += (size_t)snprintf(
			buf + pos,
			capacity - pos,
			"    {\n"
			"      \"map\": \"%s\",\n"
			"      \"best_score\": %ld,\n"
			"      \"best_set\": %d,\n"
			"      \"best_round\": %d,\n"
			"      \"best_wave\": %d,\n"
			"      \"best_seconds\": %ld,\n"
			"      \"best_difficulty\": %d,\n"
			"      \"best_skulls\": %d,\n"
			"      \"best_medals\": %ld\n"
			"    }%s\n",
			e->map_key,
			e->record.best_score,
			(int)e->record.best_set,
			(int)e->record.best_round,
			(int)e->record.best_wave,
			e->record.best_seconds,
			(int)e->record.best_difficulty,
			(int)e->record.best_skulls,
			e->record.best_medals,
			(i + 1 < g_map_record_count) ? "," : "");
	}
	snprintf(buf + pos, capacity - pos, "  ]\n}\n");
	return buf;
}

static DWORD WINAPI stats_telemetry_thread_proc(LPVOID parameter)
{
	struct firefight_telemetry_post_job *job = (struct firefight_telemetry_post_job *)parameter;
	char response[512];
	char error[160];

	if (!job)
		return 0;

	if (job->url[0] && job->json_payload && job->json_payload[0])
	{
		response[0] = '\0';
		error[0] = '\0';
		update_request(
			"POST",
			job->url,
			"Content-Type: application/json; charset=utf-8\r\n",
			job->json_payload,
			response,
			sizeof(response),
			error,
			sizeof(error));
	}

	free(job->json_payload);
	free(job);
	return 0;
}

static void dispatch_telemetry_post_async(
	long profile_index,
	struct firefight_telemetry_session const *session)
{
	const char *api_url = get_stats_telemetry_url();
	struct firefight_telemetry_post_job *job;
	char *payload;
	HANDLE thread_handle;

	if (!session)
		return;

	payload = serialize_telemetry_json_payload(profile_index, session);
	if (!payload)
		return;

	/* Groundwork for web tracking: always keep local JSON export synchronized */
	write_xbox_text_file("u:\\saved_games\\" FIREFIGHT_EXPORT_JSON_FILE, payload, strlen(payload));

	if (!api_url || !api_url[0])
	{
		free(payload);
		return;
	}

	if (!ae_stats_upload_allowed()) { free(payload); return; } /* AE hook */

	job = (struct firefight_telemetry_post_job *)malloc(sizeof(*job));
	if (!job)
	{
		free(payload);
		return;
	}

	snprintf(job->url, sizeof(job->url), "%s", api_url);
	job->json_payload = payload;

	thread_handle = CreateThread(NULL, 0, stats_telemetry_thread_proc, job, 0, NULL);
	if (thread_handle)
	{
		CloseHandle(thread_handle);
	}
	else
	{
		free(job->json_payload);
		free(job);
	}
}

boolean firefight_stats_open_web_profile(
	long profile_index,
	char *out_status,
	size_t status_size)
{
	char url[512];
	char profile_name_raw[64];
	char profile_name_encoded[192];
	const char *web_base = config_string("stats.web_url");
	struct firefight_telemetry_session sync_session;
	size_t i, out_pos = 0;

	if (!web_base)
		web_base = "";

	profile_index = get_active_profile_index(profile_index);
	load_profile_records(profile_index);
	get_active_player_name_utf8(profile_index, profile_name_raw, sizeof(profile_name_raw));

	csmemset(&sync_session, 0, sizeof(sync_session));
	snprintf(sync_session.category, sizeof(sync_session.category), "firefight");
	snprintf(sync_session.player_name, sizeof(sync_session.player_name), "%s", profile_name_raw);
	snprintf(sync_session.map_name, sizeof(sync_session.map_name), "%s",
		(g_map_record_count > 0 && g_map_records[0].valid) ? g_map_records[0].map_key : "bloodgulch");
	sync_session.difficulty = (g_map_record_count > 0 && g_map_records[0].valid) ?
		g_map_records[0].record.best_difficulty : 2;
	sync_session.score = g_career_stats.total_score;
	sync_session.kills = g_career_stats.total_kills;
	sync_session.deaths = g_career_stats.total_deaths;
	sync_session.sets_completed = g_career_stats.total_sets_completed;
	sync_session.rounds_completed = g_career_stats.total_sets_completed * FIREFIGHT_ROUNDS_PER_SET;
	sync_session.waves_completed = g_career_stats.total_waves_cleared;
	sync_session.generators_defended = 0;
	sync_session.duration_seconds = g_career_stats.total_seconds;
	sync_session.skulls_unlocked_count = count_unlocked_skulls();
	dispatch_telemetry_post_async(profile_index, &sync_session);

	for (i = 0; profile_name_raw[i] != '\0' && out_pos + 4 < sizeof(profile_name_encoded); i++)
	{
		unsigned char c = (unsigned char)profile_name_raw[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~')
		{
			profile_name_encoded[out_pos++] = (char)c;
		}
		else
		{
			snprintf(&profile_name_encoded[out_pos], 4, "%%%02X", (unsigned int)c);
			out_pos += 3;
		}
	}
	profile_name_encoded[out_pos] = '\0';

	if (web_base && web_base[0])
	{
		const char *sep = strchr(web_base, '?') ? "&" : "?";
		snprintf(
			url,
			sizeof(url),
			"%s%stab=firefight&player=%s",
			web_base,
			sep,
			profile_name_encoded[0] ? profile_name_encoded : g_player_id);
		platform_open_url(url);
		if (out_status && status_size > 0)
			snprintf(out_status, status_size, "OPENED WEB STATS (%s)", profile_name_raw);
		return TRUE;
	}

	if (out_status && status_size > 0)
		snprintf(out_status, status_size, "SAVED STATS JSON LOCALLY");
	return TRUE;
}

boolean firefight_stats_copy_player_id(
	long profile_index,
	char *out_status,
	size_t status_size)
{
	profile_index = get_active_profile_index(profile_index);
	load_profile_records(profile_index);

	platform_clipboard_set(g_player_id);
	if (out_status && status_size > 0)
		snprintf(out_status, status_size, "COPIED ID: %s", g_player_id);
	return TRUE;
}

static boolean g_session_stats_recorded = FALSE;
/* (noted as the map starts: a replay's playback may end before its map does) */
static boolean s_session_gives_no_xp = FALSE;

/* a replay watched or a Forge session counts for nothing: no XP (so neither
the postgame's XP screen nor the card after leaving has anything to show), no
career stats, medals or map records, nothing sent to the servers */
boolean firefight_stats_session_withheld(void)
{
	extern int g_forge_session_enabled; /* port/forge/src/forge_runtime_tags.c */

	return s_session_gives_no_xp || g_forge_session_enabled || network_replay_playing();
}

void firefight_stats_on_map_initialize(void)
{
	struct scenario *scen = global_scenario_index != NONE ? global_scenario_try_and_get() : NULL;

	/* (not the menus' map: what they give stays theirs) */
	s_session_gives_no_xp = FALSE;
	if (scen && scen->type != _scenario_type_main_menu)
		s_session_gives_no_xp = firefight_stats_session_withheld();
	g_session_stats_recorded = FALSE;
	g_terminal_command_used_in_session = FALSE;
	csmemset(s_session_player_medals, 0, sizeof(s_session_player_medals));
	csmemset(s_session_player_medal_counts, 0, sizeof(s_session_player_medal_counts));
	load_profile_records(NONE);
	s_session_start_xp = g_career_stats.career_xp;
	s_session_combat_xp = 0;
	s_session_medal_xp = 0;
	s_session_bonus_xp = 0;
	s_session_xp_line_count = 0;
}

/* a source of this match's XP, added to its line (the last line takes the
rest when they are all used) */
static void session_xp_note(char const *label, long xp, long count)
{
	short index;

	if (xp <= 0 || !label)
		return;
	for (index = 0; index < s_session_xp_line_count; index++)
	{
		if (!strcmp(s_session_xp_lines[index].label, label))
			break;
	}
	if (index == s_session_xp_line_count)
	{
		if (index == FIREFIGHT_POSTGAME_XP_LINES)
		{
			index--;
			snprintf(s_session_xp_lines[index].label, sizeof(s_session_xp_lines[index].label), "Other");
		}
		else
		{
			csmemset(&s_session_xp_lines[index], 0, sizeof(s_session_xp_lines[index]));
			snprintf(s_session_xp_lines[index].label, sizeof(s_session_xp_lines[index].label), "%s", label);
			s_session_xp_line_count++;
		}
	}
	s_session_xp_lines[index].xp += xp;
	s_session_xp_lines[index].count += count;
}

void firefight_stats_record_medal(long player_index, short medal_index)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	boolean is_multiplayer = (game_in_progress() && game_engine_running() && !firefight_active());
	struct player_datum const *player = (player_index != NONE) ? player_try_and_get(player_index) : NULL;
	short abs_idx = (player_index != NONE) ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : 0;

	if (medal_index < 0 || medal_index >= 38)
		return;

	if (abs_idx >= 0 && abs_idx < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
	{
		s_session_player_medals[abs_idx]++;
		if (s_session_player_medal_counts[abs_idx][medal_index] < 30000)
			s_session_player_medal_counts[abs_idx][medal_index]++;
	}

	if (player_index != NONE && (!player || player->local_player_index == NONE))
		return;
	if (firefight_stats_session_withheld())
		return;

	load_profile_records(NONE);
	if (is_multiplayer)
		g_career_stats.mp_medal_counts[medal_index]++;
	else
		g_career_stats.medal_counts[medal_index]++;

	s_session_medal_xp += 100L;
	session_xp_note("Medals", 100L, 1L);
	firefight_stats_add_xp(100, NULL);
	advance_challenge_metric(_ff_challenge_cat_multiplayer, _chal_metric_medals, 1);
	g_profile_records_dirty = TRUE;
}

long firefight_stats_get_session_medals(long player_index)
{
	short abs_idx = (player_index != NONE) ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : 0;
	if (abs_idx >= 0 && abs_idx < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		return s_session_player_medals[abs_idx];
	return 0;
}

short firefight_stats_get_player_top_medals(
	long player_index,
	short *out_medal_ids,
	short *out_counts,
	short max_medals)
{
	short abs_idx = (player_index != NONE) ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : 0;
	boolean used[38];
	short found = 0;
	short slot, m;

	if (!out_medal_ids || !out_counts || max_medals <= 0)
		return 0;
	if (abs_idx < 0 || abs_idx >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		return 0;

	csmemset(used, 0, sizeof(used));
	for (slot = 0; slot < max_medals && slot < 38; slot++)
	{
		short best_m = NONE;
		short best_count = 0;
		for (m = 0; m < 38; m++)
		{
			if (!used[m] && s_session_player_medal_counts[abs_idx][m] > best_count)
			{
				best_count = s_session_player_medal_counts[abs_idx][m];
				best_m = m;
			}
		}
		if (best_m == NONE || best_count <= 0)
			break;
		used[best_m] = TRUE;
		out_medal_ids[found] = best_m;
		out_counts[found] = best_count;
		found++;
	}
	return found;
}

void firefight_stats_get_emblem(long profile_index, struct firefight_emblem_info *out_emblem)
{
	if (!out_emblem)
		return;
	load_profile_records(profile_index);
	*out_emblem = g_profile_emblem;
}

void firefight_stats_set_emblem(long profile_index, struct firefight_emblem_info const *emblem)
{
	struct firefight_telemetry_session sync_session;

	if (!emblem)
		return;
	load_profile_records(profile_index);
	store_profile_emblem(emblem);
	save_profile_records(profile_index);
	fill_sync_telemetry_session(profile_index, "auto_sync", &sync_session);
	dispatch_telemetry_post_async(profile_index, &sync_session);
	social_emblem_changed();
}

void firefight_stats_record_campaign_mission_end(
	char const *map_name,
	long score,
	long kills,
	long deaths,
	long seconds,
	short difficulty,
	short skulls,
	long medals)
{
	char const *raw_map = (map_name && map_name[0]) ? map_name :
		((global_scenario_index != NONE) ? tag_get_name(global_scenario_index) : "campaign");
	char key[32];
	struct firefight_stored_map_entry *target_entry = NULL;
	struct firefight_telemetry_session session;
	short i;

	if (firefight_active() || g_session_stats_recorded || firefight_stats_session_withheld())
		return;
	g_session_stats_recorded = TRUE;

	load_profile_records(NONE);
	normalize_map_key(raw_map, key, sizeof(key));

	g_career_stats.total_matches++;
	g_career_stats.total_kills += MAX(0L, kills);
	g_career_stats.total_deaths += MAX(0L, deaths);
	g_career_stats.total_score += MAX(0L, score);
	g_career_stats.total_seconds += MAX(0L, seconds);

	/* Award MCC Career XP for completing a Campaign mission + score bonus */
	firefight_stats_add_xp(
		2500L + (long)MAX(0, difficulty) * 1500L + MAX(0L, score / 10L),
		"Campaign Mission Complete");
	advance_challenge_metric(_ff_challenge_cat_campaign, _chal_metric_completions, 1);
	if (difficulty >= 2)
		advance_challenge_metric(_ff_challenge_cat_campaign, _chal_metric_heroic_clear, 1);
	if (difficulty >= 3)
		advance_challenge_metric(_ff_challenge_cat_campaign, _chal_metric_legendary_clear, 1);
	if (score > 0)
		advance_challenge_metric(_ff_challenge_cat_campaign, _chal_metric_score, score);

	for (i = 0; i < g_map_record_count; i++)
	{
		if (strcmp(g_map_records[i].map_key, key) == 0)
		{
			target_entry = &g_map_records[i];
			break;
		}
	}

	if (!target_entry && g_map_record_count < MAX_TRACKED_MAPS)
	{
		target_entry = &g_map_records[g_map_record_count++];
		csmemset(target_entry, 0, sizeof(*target_entry));
		strncpy(target_entry->map_key, key, sizeof(target_entry->map_key) - 1);
	}

	if (target_entry)
	{
		struct firefight_map_record *rec = &target_entry->record;
		if (!target_entry->valid || score > rec->best_score)
		{
			rec->best_score = score;
			rec->best_difficulty = difficulty;
			rec->best_skulls = skulls;
		}
		else if (difficulty > rec->best_difficulty && difficulty <= 3)
		{
			rec->best_difficulty = difficulty;
		}
		if (!target_entry->valid || (seconds > 0 && (rec->best_seconds <= 0 || seconds < rec->best_seconds)))
		{
			rec->best_seconds = seconds;
		}
		if (!target_entry->valid || medals > rec->best_round)
		{
			rec->best_round = (short)MIN(32767L, MAX(0L, medals));
		}
		target_entry->valid = TRUE;
	}

	{
		short camp_idx = camp_map_index_from_key(key);
		if (camp_idx != NONE)
		{
			set_ach_bit_internal((short)(_ach_camp_a10 + camp_idx), TRUE);
			if (difficulty >= 1)
				set_ach_bit_internal(_ach_lvl_normal, TRUE);
			if (difficulty >= 2)
				set_ach_bit_internal(_ach_lvl_heroic, TRUE);
			if (difficulty >= 3)
				set_ach_bit_internal(_ach_lvl_legendary, TRUE);
		}
		sync_retroactive_achievements(TRUE);
	}

	save_profile_records(NONE);

	csmemset(&session, 0, sizeof(session));
	snprintf(session.category, sizeof(session.category), "campaign");
	get_active_player_name_utf8(NONE, session.player_name, sizeof(session.player_name));
	snprintf(session.map_name, sizeof(session.map_name), "%s", key);
	session.difficulty = difficulty;
	session.score = MAX(0L, score);
	session.kills = MAX(0L, kills);
	session.deaths = MAX(0L, deaths);
	session.sets_completed = 0;
	session.rounds_completed = 1;
	session.waves_completed = 0;
	session.generators_defended = 0;
	session.duration_seconds = MAX(0L, seconds);
	session.skulls_unlocked_count = count_unlocked_skulls();
	dispatch_telemetry_post_async(NONE, &session);
}

void firefight_stats_record_match_end(char const *map_name)
{
	char const *raw_map = (map_name && map_name[0]) ? map_name :
		((global_scenario_index != NONE) ? tag_get_name(global_scenario_index) : "map");
	char key[32];
	long session_score = MAX(ff_state.team_score, campaign_score_team_points());
	long session_seconds = MAX(0L, ff_state.session_elapsed_ticks / TICKS_PER_SECOND);
	short active_skulls = 0;
	long total_kills = 0;
	long total_boss_kills = 0;
	long total_deaths = 0;
	long total_assists = 0;
	long best_spree = 0;
	long waves_cleared = 0;
	long sets_this_match = 0;
	long rounds_this_match = 0;
	struct firefight_stored_map_entry *target_entry = NULL;
	struct firefight_telemetry_session session;
	short i;

	if (g_session_stats_recorded || firefight_stats_session_withheld())
		return;
	g_session_stats_recorded = TRUE;

	load_profile_records(NONE);

	for (i = 0; i < NUMBER_OF_SKULLS; i++)
	{
		if (skull_active(i))
			active_skulls++;
	}

	/* (the career is this machine's players': not a bot's or another machine's) */
	for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++)
	{
		long p_idx = local_player_get_player_index(i);
		short abs_idx = p_idx != NONE ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(p_idx) : NONE;

		if (abs_idx < 0 || abs_idx >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			continue;
		total_kills += ff_state.player_stats[abs_idx].kills;
		total_boss_kills += ff_state.player_stats[abs_idx].boss_kills;
		total_deaths += ff_state.player_stats[abs_idx].deaths;
		total_assists += ff_state.player_stats[abs_idx].assists;
		if (ff_state.player_stats[abs_idx].best_spree > best_spree)
			best_spree = ff_state.player_stats[abs_idx].best_spree;
	}

	if (total_kills == 0)
	{
		short lp;
		boolean deaths_counted = total_deaths != 0;

		for (lp = 0; lp < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; lp++)
		{
			long p_idx = local_player_get_player_index(lp);
			if (p_idx != NONE)
			{
				total_kills += MAX(0L, campaign_score_player_kills(p_idx));
				if (!deaths_counted)
					total_deaths += MAX(0L, campaign_score_player_deaths(p_idx));
			}
		}
	}

	waves_cleared = MAX(0, (ff_state.current_set - 1) * (FIREFIGHT_ROUNDS_PER_SET * FIREFIGHT_WAVES_PER_ROUND) +
		(ff_state.current_round - 1) * FIREFIGHT_WAVES_PER_ROUND +
		MAX(0, ff_state.current_wave - 1));
	sets_this_match = MAX(0, ff_state.current_set - 1) + (ff_state.match_won ? 1 : 0);
	rounds_this_match = MAX(0, (ff_state.current_set - 1) * FIREFIGHT_ROUNDS_PER_SET +
		MAX(0, ff_state.current_round - 1)) + (ff_state.match_won ? 1 : 0);

	{
		static long const s_kill_milestones[] = { 100, 250, 500, 1000, 2500 };
		static long const s_set_milestones[] = { 1, 3, 5, 10, 15, 20 };
		long prev_kills = g_career_stats.total_kills;
		long prev_sets = g_career_stats.total_sets_completed;
		short m;

		g_career_stats.total_matches++;
		g_career_stats.total_kills += total_kills;
		g_career_stats.total_boss_kills += total_boss_kills;
		g_career_stats.total_deaths += total_deaths;
		g_career_stats.total_assists += total_assists;
		if (best_spree > g_career_stats.best_kill_spree)
			g_career_stats.best_kill_spree = best_spree;
		g_career_stats.total_sets_completed += sets_this_match;
		g_career_stats.total_waves_cleared += waves_cleared;
		g_career_stats.total_score += session_score;
		g_career_stats.total_seconds += session_seconds;
		g_career_stats.ff_matches++;
		g_career_stats.ff_kills += total_kills;
		g_career_stats.ff_deaths += total_deaths;
		g_career_stats.ff_assists += total_assists;
		g_career_stats.ff_score += session_score;
		g_career_stats.ff_seconds += session_seconds;
		if (best_spree > g_career_stats.ff_best_spree)
			g_career_stats.ff_best_spree = best_spree;

		/* (not for a match left before it was won or lost) */
		if (ff_state.match_won || ff_state.match_lost)
			firefight_stats_add_xp_counted(500L, "Match complete", 0L);
		firefight_stats_add_xp_counted(sets_this_match * 4000L, "Sets cleared", sets_this_match);
		firefight_stats_add_xp_counted(waves_cleared * 250L, "Waves cleared", waves_cleared);
		firefight_stats_add_xp_counted(total_kills * 25L, "Kill bonus", 0L);
		firefight_stats_add_xp_counted(MAX(0L, session_score / 15L), "Score bonus", 0L);
		if (sets_this_match > 0)
			advance_challenge_metric(_ff_challenge_cat_firefight, _chal_metric_completions, sets_this_match);
		if (waves_cleared > 0)
			advance_challenge_metric(_ff_challenge_cat_firefight, _chal_metric_waves, waves_cleared);
		if (session_score > 0)
			advance_challenge_metric(_ff_challenge_cat_firefight, _chal_metric_score, session_score);

		for (m = 0; m < (short)NUMBEROF(s_set_milestones); m++)
		{
			if (prev_sets < s_set_milestones[m] && g_career_stats.total_sets_completed >= s_set_milestones[m])
			{
				char reason[48];
				snprintf(reason, sizeof(reason), "Firefight Set %ld milestone", s_set_milestones[m]);
				skulls_unlock_next_by_milestone(reason);
			}
		}
		for (m = 0; m < (short)NUMBEROF(s_kill_milestones); m++)
		{
			if (prev_kills < s_kill_milestones[m] && g_career_stats.total_kills >= s_kill_milestones[m])
			{
				char reason[48];
				snprintf(reason, sizeof(reason), "%ld Firefight kills", s_kill_milestones[m]);
				skulls_unlock_next_by_milestone(reason);
			}
		}
	}

	normalize_map_key(raw_map, key, sizeof(key));
	for (i = 0; i < g_map_record_count; i++)
	{
		if (strcmp(g_map_records[i].map_key, key) == 0)
		{
			target_entry = &g_map_records[i];
			break;
		}
	}

	if (!target_entry && g_map_record_count < MAX_TRACKED_MAPS)
	{
		target_entry = &g_map_records[g_map_record_count++];
		csmemset(target_entry, 0, sizeof(*target_entry));
		strncpy(target_entry->map_key, key, sizeof(target_entry->map_key) - 1);
	}

	if (target_entry)
	{
		struct firefight_map_record *rec = &target_entry->record;
		if (!target_entry->valid || session_score > rec->best_score)
		{
			rec->best_score = session_score;
			rec->best_difficulty = global_variant.game_engine_variant.firefight.difficulty;
			rec->best_skulls = active_skulls;
		}
		if (!target_entry->valid ||
			ff_state.current_set > rec->best_set ||
			(ff_state.current_set == rec->best_set && ff_state.current_round > rec->best_round) ||
			(ff_state.current_set == rec->best_set && ff_state.current_round == rec->best_round && ff_state.current_wave > rec->best_wave))
		{
			rec->best_set = ff_state.current_set;
			rec->best_round = ff_state.current_round;
			rec->best_wave = ff_state.current_wave;
		}
		if (!target_entry->valid || session_seconds > rec->best_seconds)
		{
			rec->best_seconds = session_seconds;
		}
		target_entry->valid = TRUE;
	}

	if (sets_this_match >= 1)
	{
		short camp_idx = camp_map_index_from_key(key);
		set_ach_bit_internal(_ach_ff_first_set, TRUE);
		if (camp_idx != NONE)
			set_ach_bit_internal((short)(_ach_ff_a10 + camp_idx), TRUE);
	}

	save_profile_records(NONE);
	social_firefight_changed();

	csmemset(&session, 0, sizeof(session));
	snprintf(session.category, sizeof(session.category), "firefight");
	get_active_player_name_utf8(NONE, session.player_name, sizeof(session.player_name));
	snprintf(session.map_name, sizeof(session.map_name), "%s", key);
	session.difficulty = (short)global_variant.game_engine_variant.firefight.difficulty;
	session.score = MAX(0L, session_score);
	session.kills = MAX(0L, total_kills);
	session.deaths = MAX(0L, total_deaths);
	session.sets_completed = MAX(0L, sets_this_match);
	session.rounds_completed = MAX(0L, rounds_this_match);
	session.waves_completed = MAX(0L, waves_cleared);
	session.generators_defended = MAX(0L, (long)ff_state.generators_alive);
	session.duration_seconds = MAX(0L, session_seconds);
	session.skulls_unlocked_count = count_unlocked_skulls();
	dispatch_telemetry_post_async(NONE, &session);
}

void firefight_stats_record_multiplayer_match_end(char const *map_name)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	char const *raw_map = (map_name && map_name[0]) ? map_name :
		((global_scenario_index != NONE) ? tag_get_name(global_scenario_index) : "multiplayer");
	char key[32];
	long session_seconds = MAX(0L, game_time_get() / TICKS_PER_SECOND);
	long local_kills = 0;
	long local_deaths = 0;
	long local_assists = 0;
	long local_score = 0;
	long best_spree = 0;
	short local_player_idx;
	struct firefight_telemetry_session session;

	if (firefight_active() || g_session_stats_recorded || firefight_stats_session_withheld())
		return;
	g_session_stats_recorded = TRUE;

	load_profile_records(NONE);
	normalize_map_key(raw_map, key, sizeof(key));

	for (local_player_idx = 0; local_player_idx < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local_player_idx++)
	{
		long player_index = local_player_get_player_index(local_player_idx);
		struct player_datum *player = (player_index != NONE) ? player_try_and_get(player_index) : NULL;
		if (!player)
			continue;

		local_kills += MAX(0, (int)player->statistics.kills[0]);
		local_deaths += MAX(0, (int)player->statistics.deaths);
		local_assists += MAX(0, (int)player->statistics.assists[0]);
		if (player->statistics.kills_in_a_row > best_spree)
			best_spree = player->statistics.kills_in_a_row;
		local_score += MAX(0L, local_kills * 100L);
	}

	g_career_stats.total_matches++;
	g_career_stats.total_kills += local_kills;
	g_career_stats.total_deaths += local_deaths;
	g_career_stats.total_assists += local_assists;
	if (best_spree > g_career_stats.best_kill_spree)
		g_career_stats.best_kill_spree = best_spree;
	g_career_stats.total_score += local_score;
	g_career_stats.total_seconds += session_seconds;

	/* (a match left before its end was not completed: only its kills count) */
	if (!game_engine_in_play())
		firefight_stats_add_xp_counted(1500L, "Match complete", 0L);
	firefight_stats_add_xp_counted(local_kills * 100L, "Kill bonus", 0L);
	firefight_stats_add_xp_counted(local_assists * 50L, "Assists", local_assists);
	advance_challenge_metric(_ff_challenge_cat_multiplayer, _chal_metric_completions, 1);
	if (local_kills > 0)
		advance_challenge_metric(_ff_challenge_cat_multiplayer, _chal_metric_kills, local_kills);
	if (local_score > 0)
		advance_challenge_metric(_ff_challenge_cat_multiplayer, _chal_metric_score, local_score);

	g_career_stats.mp_matches++;
	g_career_stats.mp_kills += local_kills;
	g_career_stats.mp_deaths += local_deaths;
	g_career_stats.mp_assists += local_assists;
	g_career_stats.mp_score += local_score;

	save_profile_records(NONE);

	csmemset(&session, 0, sizeof(session));
	snprintf(session.category, sizeof(session.category), "multiplayer");
	get_active_player_name_utf8(NONE, session.player_name, sizeof(session.player_name));
	snprintf(session.map_name, sizeof(session.map_name), "%s", key);
	session.difficulty = 1;
	session.score = local_score;
	session.kills = local_kills;
	session.deaths = local_deaths;
	session.sets_completed = 0;
	session.rounds_completed = 1;
	session.waves_completed = 0;
	session.generators_defended = 0;
	session.duration_seconds = session_seconds;
	session.skulls_unlocked_count = count_unlocked_skulls();
	dispatch_telemetry_post_async(NONE, &session);
}

static void firefight_stats_record_left_map(void)
{
	struct scenario *scen;
	char const *map_name;

	if (g_session_stats_recorded || global_scenario_index == NONE || firefight_stats_session_withheld())
	{
		if (g_profile_records_dirty)
			save_profile_records(NONE);
		return;
	}

	scen = global_scenario_try_and_get();
	if (!scen || scen->type == _scenario_type_main_menu)
	{
		if (g_profile_records_dirty)
			save_profile_records(NONE);
		return;
	}

	map_name = tag_get_name(global_scenario_index);
	if (firefight_active())
	{
		firefight_stats_record_match_end(map_name);
	}
	else if (game_engine_running() || scen->type == _scenario_type_multiplayer)
	{
		firefight_stats_record_multiplayer_match_end(map_name);
	}
	else if (scen->type == _scenario_type_solo)
	{
		/* Player ended/left a Campaign session before mission completion cutscene:
		   record session kills, deaths, score, time, and XP locally and sync to website */
		char key[32];
		long session_seconds = MAX(0L, game_time_get() / TICKS_PER_SECOND);
		long session_score = MAX(0L, campaign_score_team_points());
		long session_kills = 0;
		long session_deaths = 0;
		short lp;
		struct firefight_telemetry_session session;

		g_session_stats_recorded = TRUE;
		load_profile_records(NONE);
		normalize_map_key(map_name, key, sizeof(key));

		for (lp = 0; lp < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; lp++)
		{
			long p_idx = local_player_get_player_index(lp);
			if (p_idx != NONE)
			{
				session_kills += MAX(0L, campaign_score_player_kills(p_idx));
				session_deaths += MAX(0L, campaign_score_player_deaths(p_idx));
			}
		}

		g_career_stats.total_matches++;
		g_career_stats.total_kills += session_kills;
		g_career_stats.total_deaths += session_deaths;
		g_career_stats.total_score += session_score;
		g_career_stats.total_seconds += session_seconds;

		firefight_stats_add_xp(
			250L + session_kills * 25L + MAX(0L, session_score / 10L),
			"Campaign Session Ended");
		if (session_score > 0)
			advance_challenge_metric(_ff_challenge_cat_campaign, _chal_metric_score, session_score);

		save_profile_records(NONE);

		csmemset(&session, 0, sizeof(session));
		snprintf(session.category, sizeof(session.category), "campaign");
		get_active_player_name_utf8(NONE, session.player_name, sizeof(session.player_name));
		snprintf(session.map_name, sizeof(session.map_name), "%s", key);
		session.difficulty = game_difficulty_level_get();
		session.score = session_score;
		session.kills = session_kills;
		session.deaths = session_deaths;
		session.sets_completed = 0;
		session.rounds_completed = 0;
		session.waves_completed = 0;
		session.generators_defended = 0;
		session.duration_seconds = session_seconds;
		session.skulls_unlocked_count = count_unlocked_skulls();
		dispatch_telemetry_post_async(NONE, &session);
	}
}

void firefight_stats_on_map_dispose(void)
{
	struct scenario *scen = global_scenario_index != NONE ? global_scenario_try_and_get() : NULL;
	boolean played = scen && scen->type != _scenario_type_main_menu;

	firefight_stats_record_left_map();
	/* (what the game gave, for the card over the menus, and the server) */
	if (played)
	{
		firefight_hud_xp_exit_note();
		social_firefight_changed();
	}
}

boolean firefight_stats_get_map_record(
	char const *map_name,
	long profile_index,
	struct firefight_map_record *out_record)
{
	char key[32];
	short i;

	if (!out_record)
		return FALSE;

	csmemset(out_record, 0, sizeof(*out_record));
	load_profile_records(profile_index);
	normalize_map_key(map_name, key, sizeof(key));

	for (i = 0; i < g_map_record_count; i++)
	{
		if (strcmp(g_map_records[i].map_key, key) == 0 && g_map_records[i].valid)
		{
			*out_record = g_map_records[i].record;
			return TRUE;
		}
	}

	return FALSE;
}

static void fill_sync_telemetry_session(long profile_index, char const *category, struct firefight_telemetry_session *out_session);

boolean firefight_stats_get_career(
	long profile_index,
	struct firefight_career_stats *out_career)
{
	static unsigned long last_auto_sync_ms = 0;
	unsigned long now_ms = system_milliseconds();

	if (!out_career)
		return FALSE;

	load_profile_records(profile_index);
	if (g_profile_records_dirty && !game_in_progress())
		save_profile_records(profile_index);
	*out_career = g_career_stats;

	if (last_auto_sync_ms == 0 || (now_ms - last_auto_sync_ms) > 30000UL)
	{
		struct firefight_telemetry_session sync_session;
		last_auto_sync_ms = now_ms ? now_ms : 1UL;
		fill_sync_telemetry_session(profile_index, "auto_sync", &sync_session);
		dispatch_telemetry_post_async(profile_index, &sync_session);
	}
	return g_career_loaded;
}

void firefight_stats_unlock_achievement(short ach_id)
{
	load_profile_records(NONE);
	if (set_ach_bit_internal(ach_id, TRUE))
	{
		struct firefight_telemetry_session sync_session;
		save_profile_records(NONE);
		if (config_boolean("stats.auto_export_json"))
			firefight_stats_export_json(NONE, NULL, 0);
		fill_sync_telemetry_session(NONE, "auto_sync", &sync_session);
		dispatch_telemetry_post_async(NONE, &sync_session);
	}
}

boolean firefight_stats_is_achievement_unlocked(long profile_index, short ach_id)
{
	load_profile_records(profile_index);
	return is_ach_bit_set(ach_id);
}

void firefight_stats_get_achievement_count(long profile_index, short *out_unlocked, short *out_total)
{
	short unlocked = 0;

	load_profile_records(profile_index);
	compute_unlocked_achievement_score(&unlocked);
	if (out_unlocked)
		*out_unlocked = unlocked;
	if (out_total)
		*out_total = HALO_ACHIEVEMENT_COUNT;
}

short firefight_stats_get_achievement_points(short ach_id)
{
	if (ach_id < 0 || ach_id >= HALO_ACHIEVEMENT_COUNT)
		return 0;
	return s_achievement_defs[ach_id].points;
}

void firefight_stats_get_achievement_score(long profile_index, short *out_score, short *out_max_score)
{
	short score;

	load_profile_records(profile_index);
	score = compute_unlocked_achievement_score(NULL);
	if (out_score)
		*out_score = score;
	if (out_max_score)
		*out_max_score = 1000;
}

boolean firefight_stats_get_achievement_info(
	short ach_id,
	char const **out_key,
	wchar_t const **out_title,
	wchar_t const **out_desc)
{
	if (ach_id < 0 || ach_id >= HALO_ACHIEVEMENT_COUNT)
		return FALSE;
	if (out_key)
		*out_key = s_achievement_defs[ach_id].key;
	if (out_title)
		*out_title = s_achievement_defs[ach_id].title;
	if (out_desc)
		*out_desc = s_achievement_defs[ach_id].desc;
	return TRUE;
}

void firefight_stats_notify_skull_claimed(int skull_enum_id, boolean from_elite_carrier)
{
	static int const s_map_skulls[10] =
	{
		_skull_iron, _skull_mythic, _skull_boom, _skull_bandanna, _skull_fog,
		_skull_famine, _skull_black_eye, _skull_eye_patch, _skull_recession, _skull_malfunction
	};
	boolean changed = FALSE;
	short i;

	if (firefight_active() || !global_scenario || global_scenario->type != _scenario_type_solo)
		return;

	load_profile_records(NONE);
	if (from_elite_carrier)
	{
		if (set_ach_bit_internal(_ach_skull_elite_hunter, TRUE))
			changed = TRUE;
	}
	if (skull_enum_id >= 0 && game_difficulty_level_get() == _game_difficulty_level_impossible)
	{
		for (i = 0; i < 10; i++)
		{
			if (s_map_skulls[i] == skull_enum_id)
			{
				if (set_ach_bit_internal((short)(_ach_skull_iron + i), TRUE))
					changed = TRUE;
			}
		}
	}
	if (changed)
	{
		struct firefight_telemetry_session sync_session;
		save_profile_records(NONE);
		if (config_boolean("stats.auto_export_json"))
			firefight_stats_export_json(NONE, NULL, 0);
		fill_sync_telemetry_session(NONE, "auto_sync", &sync_session);
		dispatch_telemetry_post_async(NONE, &sync_session);
	}
}

/* ---------- 6-Digit Device Link Code & Loopback Bridge (127.0.0.1:29917) */

#define GENESIS_LINK_LOOPBACK_PORT 29917

struct genesis_ipv4_sockaddr
{
	short sin_family;
	unsigned short sin_port;
	unsigned long sin_addr;
	char sin_zero[8];
};

static void fill_sync_telemetry_session(long profile_index, char const *category, struct firefight_telemetry_session *out_session)
{
	char profile_name_raw[64];

	csmemset(out_session, 0, sizeof(*out_session));
	profile_index = get_active_profile_index(profile_index);
	load_profile_records(profile_index);
	get_active_player_name_utf8(profile_index, profile_name_raw, sizeof(profile_name_raw));

	snprintf(out_session->category, sizeof(out_session->category), "%s", category ? category : "firefight");
	snprintf(out_session->player_name, sizeof(out_session->player_name), "%s", profile_name_raw);
	snprintf(out_session->map_name, sizeof(out_session->map_name), "%s",
		(g_map_record_count > 0 && g_map_records[0].valid) ? g_map_records[0].map_key : "bloodgulch");
	out_session->difficulty = (g_map_record_count > 0 && g_map_records[0].valid) ?
		g_map_records[0].record.best_difficulty : 2;
	out_session->score = g_career_stats.total_score;
	out_session->kills = g_career_stats.total_kills;
	out_session->deaths = g_career_stats.total_deaths;
	out_session->sets_completed = g_career_stats.total_sets_completed;
	out_session->rounds_completed = g_career_stats.total_sets_completed * FIREFIGHT_ROUNDS_PER_SET;
	out_session->waves_completed = g_career_stats.total_waves_cleared;
	out_session->generators_defended = 0;
	out_session->duration_seconds = g_career_stats.total_seconds;
	out_session->skulls_unlocked_count = count_unlocked_skulls();
}

boolean firefight_stats_generate_link_code(
	long profile_index,
	char *out_code,
	size_t code_size,
	char *out_status,
	size_t status_size)
{
	unsigned long seed;
	unsigned long now_ms;
	unsigned long unix_sec;
	unsigned long code_val;
	struct firefight_telemetry_session sync_session;

	profile_index = get_active_profile_index(profile_index);
	load_profile_records(profile_index);

	now_ms = system_milliseconds();
	unix_sec = (unsigned long)time(NULL);
	seed = fnv1a_hash_step(2166136261UL, g_player_id, strlen(g_player_id));
	seed = fnv1a_hash_step(seed, g_stats_key, strlen(g_stats_key));
	seed ^= now_ms ^ (unix_sec * 2654435761UL);
	seed = fnv1a_hash_step(seed, &now_ms, sizeof(now_ms));

	code_val = 100000UL + (seed % 900000UL);
	snprintf(g_active_link_code, sizeof(g_active_link_code), "%06lu", code_val);
	g_active_link_expiry_ms = now_ms + (15UL * 60UL * 1000UL);

	fill_sync_telemetry_session(profile_index, "link_code", &sync_session);
	dispatch_telemetry_post_async(profile_index, &sync_session);

	platform_clipboard_set(g_active_link_code);

	if (out_code && code_size > 0)
		snprintf(out_code, code_size, "%s", g_active_link_code);
	if (out_status && status_size > 0)
		snprintf(out_status, status_size, "LINK CODE: %s (COPIED)", g_active_link_code);
	return TRUE;
}

boolean firefight_stats_poll_pending_link_popup(
	char *out_code,
	size_t code_size,
	char *out_device_id,
	size_t id_size)
{
	if (!g_pending_link_popup)
		return FALSE;

	g_pending_link_popup = FALSE;
	load_profile_records(NONE);
	if (!g_active_link_code[0] ||
		(g_active_link_expiry_ms != 0 && system_milliseconds() >= g_active_link_expiry_ms))
	{
		firefight_stats_generate_link_code(NONE, NULL, 0, NULL, 0);
	}
	if (out_code && code_size > 0)
		snprintf(out_code, code_size, "%s", g_active_link_code);
	if (out_device_id && id_size > 0)
		snprintf(out_device_id, id_size, "%s", g_player_id[0] ? g_player_id : "0000000000000000");
	return TRUE;
}

static void send_http_json_response(int client_sock, int status_code, char const *status_text, char const *json_body)
{
	char header[512];
	int body_len = json_body ? (int)strlen(json_body) : 0;
	int hdr_len = snprintf(
		header,
		sizeof(header),
		"HTTP/1.1 %d %s\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
		"Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
		"Access-Control-Allow-Private-Network: true\r\n"
		"Content-Type: application/json; charset=utf-8\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n\r\n",
		status_code,
		status_text,
		body_len);
	if (hdr_len > 0)
		posix_socket_send(client_sock, header, hdr_len, 0);
	if (body_len > 0)
		posix_socket_send(client_sock, json_body, body_len, 0);
}

static DWORD WINAPI device_link_loopback_thread_proc(LPVOID parameter)
{
	int srv_sock;
	int reuse = 1;
	struct genesis_ipv4_sockaddr addr;

	(void)parameter;
	srv_sock = posix_socket(2 /* AF_INET */, 1 /* SOCK_STREAM */, 6 /* IPPROTO_TCP */);
	if (srv_sock < 0)
		return 0;

	posix_socket_setsockopt(srv_sock, 0xFFFF /* SOL_SOCKET */, 0x0004 /* SO_REUSEADDR */, &reuse, sizeof(reuse));
	csmemset(&addr, 0, sizeof(addr));
	addr.sin_family = 2; /* AF_INET */
	addr.sin_port = (unsigned short)((GENESIS_LINK_LOOPBACK_PORT >> 8) | ((GENESIS_LINK_LOOPBACK_PORT & 0xFF) << 8));
	addr.sin_addr = 0x0100007FUL; /* 127.0.0.1 in network byte order on little-endian */

	if (posix_socket_bind(srv_sock, &addr, sizeof(addr)) != 0 ||
		posix_socket_listen(srv_sock, 4) != 0)
	{
		posix_socket_close(srv_sock);
		return 0;
	}

	for (;;)
	{
		struct genesis_ipv4_sockaddr cli_addr;
		int cli_len = sizeof(cli_addr);
		int cli_sock = posix_socket_accept(srv_sock, &cli_addr, &cli_len);
		char req[1024];
		int n;

		if (cli_sock < 0)
		{
			Sleep(100);
			continue;
		}

		n = posix_socket_recv(cli_sock, req, sizeof(req) - 1, 0);
		if (n > 0)
		{
			req[n] = '\0';
			if (strncmp(req, "OPTIONS ", 8) == 0)
			{
				send_http_json_response(cli_sock, 204, "No Content", "");
			}
			else if (strstr(req, "/trigger-code") != NULL)
			{
				char body[256];
				firefight_stats_generate_link_code(NONE, NULL, 0, NULL, 0);
				g_pending_link_popup = TRUE;
				snprintf(
					body,
					sizeof(body),
					"{\"ok\":true,\"device_id\":\"%s\"}",
					g_player_id[0] ? g_player_id : "0000000000000000");
				send_http_json_response(cli_sock, 200, "OK", body);
			}
			else if (strstr(req, "/verify-code") != NULL)
			{
				char const *code_param = strstr(req, "code=");
				char submitted_code[16] = "";
				size_t k = 0;

				if (code_param)
				{
					code_param += 5;
					while (code_param[k] >= '0' && code_param[k] <= '9' && k < 6)
					{
						submitted_code[k] = code_param[k];
						k++;
					}
					submitted_code[k] = '\0';
				}

				if (submitted_code[0] != '\0' &&
					g_active_link_code[0] != '\0' &&
					strcmp(submitted_code, g_active_link_code) == 0 &&
					(g_active_link_expiry_ms == 0 || system_milliseconds() < g_active_link_expiry_ms))
				{
					struct firefight_telemetry_session sync_session;
					char *payload;
					fill_sync_telemetry_session(NONE, "verified_link", &sync_session);
					payload = serialize_telemetry_json_payload(NONE, &sync_session);
					if (payload)
					{
						send_http_json_response(cli_sock, 200, "OK", payload);
						free(payload);
					}
					else
					{
						send_http_json_response(cli_sock, 500, "Error", "{\"ok\":false,\"error\":\"Serialization failed\"}");
					}
				}
				else
				{
					send_http_json_response(
						cli_sock,
						400,
						"Bad Request",
						"{\"ok\":false,\"error\":\"Invalid or expired 6-digit link code. Click Open Game & Get Code to generate a fresh code.\"}");
				}
			}
			else
			{
				struct firefight_telemetry_session sync_session;
				char *payload;
				load_profile_records(NONE);
				fill_sync_telemetry_session(NONE, "firefight", &sync_session);
				payload = serialize_telemetry_json_payload(NONE, &sync_session);
				if (payload)
				{
					send_http_json_response(cli_sock, 200, "OK", payload);
					free(payload);
				}
				else
				{
					char body[256];
					snprintf(
						body,
						sizeof(body),
						"{\"ok\":true,\"game\":\"OpenCE : Faber\",\"device_id\":\"%s\"}",
						g_player_id[0] ? g_player_id : "0000000000000000");
					send_http_json_response(cli_sock, 200, "OK", body);
				}
			}
		}
		posix_socket_close(cli_sock);
	}
	return 0;
}

static boolean notify_existing_instance_trigger_code(void)
{
	int sock;
	struct genesis_ipv4_sockaddr addr;
	char const *req = "GET /trigger-code HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
	char resp[256];
	int n;

	sock = posix_socket(2 /* AF_INET */, 1 /* SOCK_STREAM */, 6 /* IPPROTO_TCP */);
	if (sock < 0)
		return FALSE;

	csmemset(&addr, 0, sizeof(addr));
	addr.sin_family = 2;
	addr.sin_port = (unsigned short)((GENESIS_LINK_LOOPBACK_PORT >> 8) | ((GENESIS_LINK_LOOPBACK_PORT & 0xFF) << 8));
	addr.sin_addr = 0x0100007FUL;

	if (posix_socket_connect(sock, &addr, sizeof(addr)) != 0)
	{
		posix_socket_close(sock);
		return FALSE;
	}

	posix_socket_send(sock, req, (int)strlen(req), 0);
	n = posix_socket_recv(sock, resp, sizeof(resp) - 1, 0);
	posix_socket_close(sock);
	return n > 0;
}

boolean firefight_stats_startup_check(void)
{
	if (!ae_stats_socket_allowed()) return FALSE; /* AE hook */
	if (!g_loopback_server_started)
	{
		HANDLE th = CreateThread(NULL, 0, device_link_loopback_thread_proc, NULL, 0, NULL);
		if (th)
		{
			CloseHandle(th);
			g_loopback_server_started = TRUE;
		}
	}
	return FALSE;
}

void firefight_stats_add_xp(long xp_amount, char const *reason)
{
	firefight_stats_add_xp_counted(xp_amount, reason, 0L);
}

/* (count: how many of the reason this is, for the XP screen's line: 0 for no
number) */
void firefight_stats_add_xp_counted(long xp_amount, char const *reason, long count)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
#if OPENCE_ENABLE_SERVICE_RANK
	if (xp_amount <= 0 || firefight_stats_session_withheld())
		return;
	load_profile_records(NONE);
	if (s_session_start_xp < 0)
		s_session_start_xp = g_career_stats.career_xp;
	if (reason && reason[0])
	{
		s_session_bonus_xp += xp_amount;
		session_xp_note(reason, xp_amount, count);
	}
	g_career_stats.career_xp = service_rank_apply_xp_gain(g_career_stats.career_xp, xp_amount, reason, TRUE);
	g_profile_records_dirty = TRUE;
	if (!game_in_progress())
		save_profile_records(NONE);
#else
	(void)xp_amount;
	(void)reason;
	(void)count;
#endif
}

void firefight_stats_set_career_xp(long xp)
{
	load_profile_records(NONE);
	/* (this game's XP still counted from where it started) */
	if (s_session_start_xp >= 0)
		s_session_start_xp = MAX(0L, s_session_start_xp + xp - g_career_stats.career_xp);
	g_career_stats.career_xp = xp;
	g_profile_records_dirty = TRUE;
	if (!game_in_progress())
		save_profile_records(NONE);
}

/* a line of the catch-up's card (none for no XP) */
static void catch_up_line(
	struct firefight_postgame_xp_summary *summary,
	char const *label,
	long count,
	long each)
{
	struct firefight_postgame_xp_line *line;

	if (count <= 0 || summary->line_count >= FIREFIGHT_POSTGAME_XP_LINES)
		return;
	line = &summary->lines[summary->line_count++];
	snprintf(line->label, sizeof(line->label), "%s", label);
	line->count = count;
	line->xp = count * each;
	summary->total_gained_xp += line->xp;
}

/* (as the matches give it: firefight_stats_record_*_end; a level's difficulty
is not in the records, so each counts as Easy's) */
boolean firefight_stats_rank_catch_up(struct firefight_rank_history const *history)
{
#if OPENCE_ENABLE_SERVICE_RANK
	struct firefight_postgame_xp_summary summary;
	long medals = 0;
	long campaign_matches;
	long campaign_kills;
	short index;

	load_profile_records(NONE);
	campaign_matches = MAX(0L, g_career_stats.total_matches - g_career_stats.mp_matches - g_career_stats.ff_matches);
	campaign_kills = MAX(0L, g_career_stats.total_kills - g_career_stats.mp_kills - g_career_stats.ff_kills);
	for (index = 0; index < (short)NUMBEROF(g_career_stats.medal_counts); index++)
		medals += g_career_stats.medal_counts[index] + g_career_stats.mp_medal_counts[index];

	/* (the server's records or the career's, whichever saw more) */
	csmemset(&summary, 0, sizeof(summary));
	catch_up_line(&summary, "Multiplayer matches", MAX(history->multiplayer_matches, g_career_stats.mp_matches), 1500L);
	catch_up_line(&summary, "Multiplayer kills", MAX(history->multiplayer_kills, g_career_stats.mp_kills), 100L);
	catch_up_line(&summary, "Multiplayer assists", MAX(history->multiplayer_assists, g_career_stats.mp_assists), 50L);
	catch_up_line(&summary, "Campaign levels", MAX(history->campaign_levels, campaign_matches), 2500L);
	catch_up_line(&summary, "Campaign kills", MAX(history->campaign_kills, campaign_kills), 50L);
	catch_up_line(&summary, "Firefight matches", MAX(history->firefight_matches, g_career_stats.ff_matches), 500L);
	catch_up_line(&summary, "Firefight sets", MAX(history->firefight_sets, g_career_stats.total_sets_completed), 4000L);
	catch_up_line(&summary, "Firefight waves", MAX(history->firefight_waves, g_career_stats.total_waves_cleared), 250L);
	catch_up_line(&summary, "Firefight kills", MAX(history->firefight_kills, g_career_stats.ff_kills), 25L);
	catch_up_line(&summary, "Medals", MAX(history->multiplayer_medals + history->campaign_medals, medals), 100L);
	if (summary.total_gained_xp <= g_career_stats.career_xp)
		return FALSE;
	summary.current_xp = summary.total_gained_xp;
	summary.match_bonus_xp = summary.total_gained_xp;
	compute_mcc_rank_from_xp(0L, &summary.before_rank);
	compute_mcc_rank_from_xp(summary.total_gained_xp, &summary.after_rank);
	summary.promoted = summary.after_rank.tour_number > summary.before_rank.tour_number ||
		summary.after_rank.rank_in_tour > summary.before_rank.rank_in_tour;
	firefight_stats_set_career_xp(summary.total_gained_xp);
	firefight_hud_xp_card_show(&summary);
	return TRUE;
#else
	(void)history;
	return FALSE;
#endif
}

void firefight_stats_get_mcc_rank(long profile_index, struct firefight_mcc_rank_info *out_rank)
{
	if (!out_rank)
		return;
	load_profile_records(profile_index);
	compute_mcc_rank_from_xp(g_career_stats.career_xp, out_rank);
}

void firefight_stats_get_h3_ranked(long profile_index, struct firefight_h3_ranked_info *out_ranked)
{
	if (!out_ranked)
		return;
	load_profile_records(profile_index);
	compute_h3_ranked_from_points(
		g_career_stats.ranked_points,
		g_career_stats.ranked_wins,
		g_career_stats.ranked_losses,
		g_career_stats.ranked_draws,
		out_ranked);
}

void firefight_stats_get_postgame_xp_summary(
	struct firefight_postgame_xp_summary *out_summary)
{
	long start_xp;
	long end_xp;

	if (!out_summary)
		return;
	csmemset(out_summary, 0, sizeof(*out_summary));

	/* Ensure match-completion XP and stats are recorded as soon as PGCR renders */
	if (!g_session_stats_recorded && game_in_progress())
	{
		if (firefight_active())
			firefight_stats_record_match_end(NULL);
		else if (game_engine_running())
			firefight_stats_record_multiplayer_match_end(NULL);
	}

	load_profile_records(NONE);
	end_xp = MAX(0L, g_career_stats.career_xp);
	start_xp = (s_session_start_xp >= 0 && s_session_start_xp <= end_xp) ? s_session_start_xp : end_xp;

	out_summary->combat_xp = MAX(0L, s_session_combat_xp);
	out_summary->medal_xp = MAX(0L, s_session_medal_xp);
	out_summary->match_bonus_xp = MAX(0L, s_session_bonus_xp);
	out_summary->total_gained_xp = MAX(0L, end_xp - start_xp);
	if (out_summary->total_gained_xp > (out_summary->combat_xp + out_summary->medal_xp + out_summary->match_bonus_xp))
	{
		out_summary->match_bonus_xp = out_summary->total_gained_xp - (out_summary->combat_xp + out_summary->medal_xp);
	}
	out_summary->start_xp = start_xp;
	out_summary->current_xp = end_xp;
	compute_mcc_rank_from_xp(start_xp, &out_summary->before_rank);
	compute_mcc_rank_from_xp(end_xp, &out_summary->after_rank);
	out_summary->promoted = (boolean)(
		out_summary->after_rank.tour_number > out_summary->before_rank.tour_number ||
		out_summary->after_rank.rank_in_tour > out_summary->before_rank.rank_in_tour);
	/* (the lines, and any XP none of them says where it came from) */
	{
		long listed = 0;
		short index;

		out_summary->line_count = s_session_xp_line_count;
		for (index = 0; index < s_session_xp_line_count; index++)
		{
			out_summary->lines[index] = s_session_xp_lines[index];
			listed += s_session_xp_lines[index].xp;
		}
		if (out_summary->total_gained_xp > listed)
		{
			index = (short)MIN(out_summary->line_count, FIREFIGHT_POSTGAME_XP_LINES - 1);
			if (index == out_summary->line_count)
			{
				csmemset(&out_summary->lines[index], 0, sizeof(out_summary->lines[index]));
				out_summary->line_count++;
			}
			snprintf(out_summary->lines[index].label, sizeof(out_summary->lines[index].label), "Other");
			out_summary->lines[index].xp += out_summary->total_gained_xp - listed;
		}
	}
}

void firefight_stats_notify_enemy_kill(
	long killer_player_index,
	short category,
	boolean headshot,
	boolean melee,
	boolean grenade,
	boolean boss_or_heavy,
	long points)
{
	struct player_datum const *killer = player_try_and_get(killer_player_index);
	long kill_xp = 50L;

	/* (the profile's are its player's kills: not a bot's or another machine's) */
	if (!killer || killer->local_player_index == NONE)
		return;
	load_profile_records(NONE);
	if (headshot)
		kill_xp += 25L;
	if (melee || grenade)
		kill_xp += 25L;
	if (boss_or_heavy)
		kill_xp += 100L;
	s_session_combat_xp += kill_xp;
	session_xp_note("Kills", kill_xp, 1L);
	firefight_stats_add_xp(kill_xp, NULL);

	advance_challenge_metric(category, _chal_metric_kills, 1);
	if (headshot)
		advance_challenge_metric(category, _chal_metric_headshots, 1);
	if (melee)
	{
		advance_challenge_metric(category, _chal_metric_melee, 1);
		advance_challenge_metric(category, _chal_metric_close_combat, 1);
	}
	if (grenade)
	{
		advance_challenge_metric(category, _chal_metric_grenades, 1);
		advance_challenge_metric(category, _chal_metric_close_combat, 1);
	}
	if (boss_or_heavy)
		advance_challenge_metric(category, _chal_metric_heavy_kills, 1);
	if (points > 0)
		advance_challenge_metric(category, _chal_metric_score, points);
	g_profile_records_dirty = TRUE;
}

short firefight_stats_get_challenges(
	long profile_index,
	short category,
	short period,
	struct firefight_challenge_view *out_list,
	short max_count)
{
	short active_idx[CHALLENGES_PER_BUCKET];
	short count;
	short i;

	if (!out_list || max_count <= 0)
		return 0;
	load_profile_records(profile_index);
	count = select_active_challenge_indices(category, period, active_idx, MIN(max_count, (short)CHALLENGES_PER_BUCKET));
	for (i = 0; i < count; i++)
	{
		short idx = active_idx[i];
		struct challenge_template_def const *def = &s_challenge_pool[idx];
		long prog = (period == _ff_challenge_period_daily) ? g_daily_progress[idx] : g_weekly_progress[idx];
		boolean done = is_challenge_done_bit(period, idx) || prog >= def->target;

		snprintf(out_list[i].title, sizeof(out_list[i].title), "%s", def->title);
		snprintf(out_list[i].description, sizeof(out_list[i].description), "%s", def->description);
		out_list[i].progress = MIN(prog, def->target);
		out_list[i].target = def->target;
		out_list[i].xp_reward = def->xp_reward;
		out_list[i].completed = done;
	}
	return count;
}

void firefight_stats_get_challenge_summary(
	long profile_index,
	short *out_daily_done,
	short *out_daily_total,
	short *out_weekly_done,
	short *out_weekly_total)
{
	short cat, k;
	short d_done = 0, d_total = 0;
	short w_done = 0, w_total = 0;

	load_profile_records(profile_index);
	for (cat = 0; cat < FIREFIGHT_CHALLENGE_CATEGORY_COUNT; cat++)
	{
		short d_idx[CHALLENGES_PER_BUCKET];
		short w_idx[CHALLENGES_PER_BUCKET];
		short dc = select_active_challenge_indices(cat, _ff_challenge_period_daily, d_idx, CHALLENGES_PER_BUCKET);
		short wc = select_active_challenge_indices(cat, _ff_challenge_period_weekly, w_idx, CHALLENGES_PER_BUCKET);

		d_total += dc;
		for (k = 0; k < dc; k++)
		{
			if (is_challenge_done_bit(_ff_challenge_period_daily, d_idx[k]) ||
				g_daily_progress[d_idx[k]] >= s_challenge_pool[d_idx[k]].target)
			{
				d_done++;
			}
		}

		w_total += wc;
		for (k = 0; k < wc; k++)
		{
			if (is_challenge_done_bit(_ff_challenge_period_weekly, w_idx[k]) ||
				g_weekly_progress[w_idx[k]] >= s_challenge_pool[w_idx[k]].target)
			{
				w_done++;
			}
		}
	}

	if (out_daily_done)
		*out_daily_done = d_done;
	if (out_daily_total)
		*out_daily_total = d_total;
	if (out_weekly_done)
		*out_weekly_done = w_done;
	if (out_weekly_total)
		*out_weekly_total = w_total;
}
