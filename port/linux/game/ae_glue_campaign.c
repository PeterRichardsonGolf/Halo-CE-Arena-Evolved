/*
AE_GLUE_CAMPAIGN.C

AE's CAMPAIGN glue. See ae_glue_campaign.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"
#include "saved games/game_state.h"
#include "saved games/player_profile.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "ae_glue_campaign.h"
#include "ae_companion.h"
#include "firing_range.h"
#include "ae_range_rules.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "ae_coop_rules.h"
#include "ae_glue_profiles.h"
#include "ae_input.h"
#include "ae_saved_game_file.h"
#include "mcc_cache.h"
#include "mcc_maps.h"
#include "mcc_progress.h"
#include "mcc_workshop.h"
#include "../src/mcc_manifest.h"

void platform_log(char const *format, ...);
int config_boolean(char const *name);
unsigned long system_milliseconds(void);
/* main.c (declared as menu_functions.c does) */
short main_get_solo_level_from_name(char const *name);
boolean main_menu_is_active(void);
/* networking/network_game_globals.c's, declared as menu_functions.c's coop_begin does */
void dispose_global_network_game_client(void);
void dispose_global_network_game_server(void);
void network_game_accept_remote_connections(boolean accept_remote_connections);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
boolean ui_widget_port_saved_game(char const **map_name, short *level, short *difficulty);

typedef char verify_levels[AE_CAMPAIGN_LEVELS == NUMBER_OF_SINGLE_PLAYER_LEVELS ? 1 : -1];

static const char *const difficulty_names[AE_CAMPAIGN_DIFFICULTIES] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };
static int host_left_pending;

const char *ae_campaign_difficulty_name(short difficulty)
{
	return difficulty >= 0 && difficulty < AE_CAMPAIGN_DIFFICULTIES ? difficulty_names[difficulty] : "";
}

int ae_campaign_host_left_mark(void)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;

	host_left_pending = 0;
	/* A host has its own client too. Only another machine's co-op client
	gets this dialog; ordinary network games keep the engine's error. */
	if (!global_network_game_server_get() && game && !game->variant.game_engine_index &&
		ae_coop_level_of_map(game->map.name) >= 0)
	{
		host_left_pending = 1;
	}
	return host_left_pending;
}

int ae_campaign_host_left_take(const char **title, const char **body)
{
	if (!host_left_pending)
		return 0;
	host_left_pending = 0;
	ae_coop_host_left_text(title, body);
	return 1;
}

/* the game's text on one line in capitals (its line breaks spaces, runs of spaces one: as string_get) */
static void narrow(wchar_t const *wide, char *text, int size)
{
	int length = 0;

	for (; wide && *wide && length < size - 1; wide++)
	{
		wchar_t character = *wide == '\r' || *wide == '\n' || *wide == '\t' ? ' ' : *wide;
		char c = character >= 0x20 && character < 0x7F ? (char)character : '?';

		if (c >= 'a' && c <= 'z')
			c = (char)(c - 32);
		if (c != ' ' || (length && text[length - 1] != ' '))
			text[length++] = c;
	}
	while (length && text[length - 1] == ' ')
		length--;
	text[length] = 0;
}

/* player 1's profile (the active one, else the one used last, else the first; with make, one made when this PC has
none), made player 1's on controller 0 */
static int player1_profile(struct player_profile *profile, long *index, int make)
{
	long profile_index = player_ui_get_active_player_profile_index(0);

	if (profile_index == NONE || !player_profile_get(profile_index, profile))
	{
		profile_index = ae_profile_player1_fallback(profile);
		if (profile_index == NONE)
		{
			/* (no profile on this PC at all: one made, "Player", as a first run would; PROFILES renames it) */
			int made = -1;

			/* (the API's int is the index's 32 bits: -1 is NONE, as ae_glue_profiles.c's file_index) */
			if (!make || !ae_profile_new("Player", &made).ok || made == -1)
				return 0;
			profile_index = (long)(unsigned int)made;
			if (!player_profile_get(profile_index, profile))
				return 0;
			platform_log("ae campaign: no profile on this PC: made 'Player'");
		}
	}
	player_ui_set_active_player_profile(0, profile_index, profile);
	player_ui_set_single_player_local_player_controller(0, 0);
	ae_input_controller_assigned(0);
	*index = profile_index;
	return 1;
}

void ae_campaign_level_name(short level, char *text, int size)
{
	long names = tag_loaded('ustr', "pc\\main_menu\\map_list");

	text[0] = 0;
	if (level < 0 || level >= AE_CAMPAIGN_LEVELS)
		return;
	if (names != NONE)
		narrow(unicode_string_list_get_string(names, level), text, size);
	if (!text[0])
		snprintf(text, (size_t)size, "%s", main_get_solo_level_name(level));
}

short ae_campaign_level_of_map(const char *map)
{
	return ae_coop_level_of_map(map);
}

int ae_campaign_read(struct ae_campaign *campaign)
{
	struct player_profile profile;
	long index;
	short level, highest_level, highest_difficulty, marker;
	char const *map_name;

	memset(campaign, 0, sizeof(*campaign));
	campaign->saved_level = campaign->saved_difficulty = -1;
	for (level = 0; level < AE_CAMPAIGN_LEVELS; level++)
		ae_campaign_level_name(level, campaign->levels[level].name, sizeof(campaign->levels[level].name));
	campaign->difficulty = main_get_difficulty();
	if (!player1_profile(&profile, &index, 0))
	{
		campaign->levels[0].reached = 1;
		return 0;
	}
	campaign->have_profile = 1;
	narrow(profile.player_name, campaign->profile, sizeof(campaign->profile));
	player_profile_get_highest_completed_solo_level(&profile, &highest_level, &highest_difficulty);
	for (level = 0; level < AE_CAMPAIGN_LEVELS; level++)
	{
		byte flags = profile.single_player_map_flags[level];

		campaign->levels[level].reached = flags || level == highest_level + 1 || level == 0;
		for (marker = 0; marker < 3; marker++)
			campaign->levels[level].finished[marker + 1] = (flags >> (marker + 1)) & 1;
	}
	if (ui_widget_port_saved_game(&map_name, &campaign->saved_level, &campaign->saved_difficulty))
		platform_log("ae campaign: saved game: level %d, %s", campaign->saved_level + 1,
			ae_campaign_difficulty_name(campaign->saved_difficulty));
	else
		campaign->saved_level = campaign->saved_difficulty = -1;
	return 1;
}

/* as menu_functions.c's campaign_start */
static void start(char const *map_name, short difficulty)
{
	main_set_map_name(map_name);
	main_set_difficulty(difficulty);
	game_connection_set(0);
	main_menu_switch_to_single_player();
	player_ui_remember_player1_profile(TRUE);
}

/* a level played alone with companions: the game is hosted as the co-op of one machine that takes no other (the way the engine adds
players to a game in progress, which the companions need), the level chosen and the player added as the lobby's A does, then
started at once. ae_campaign_hosted_update takes the steps a moment apart. */
/* (session: a level hosted alone for companions is going on; played: its game has run; between: seconds since the game ended; asked: the
start asked again since) */
static struct { int phase; real waited; short level; short difficulty; int session, played, requested; real between, asked; } hosted;

boolean ui_widget_port_cooperative_level_choose(char const *map_name, short difficulty);

static void hosted_start(short level, short difficulty)
{
	ui_widgets_close_all();
	dispose_global_network_game_client();
	dispose_global_network_game_server();
	network_game_accept_remote_connections(FALSE);
	game_engine_playlist_initialize();
	if (create_global_network_game_server())
	{
		game_engine_playlist_begin();
		game_connection_set(_game_connection_network_server);
	}
	if (!global_network_game_client_get())
		create_global_network_game_client();
	hosted.phase = 1;
	hosted.session = 1;
	hosted.played = 0;
	hosted.between = hosted.asked = 0.0f;
	hosted.waited = 0.0f;
	hosted.level = level;
	hosted.difficulty = difficulty;
	player_spawn_count = 1;
	platform_log("ae campaign: level %d hosted alone for its companions", level + 1);
}

/* a level won in a hosted game ends the round and the lobby is back with the campaign's next level chosen: a level played alone goes on
into it at once, with the companions asked for again, and stops after the last level (the lobby would wrap to the first) */
static void hosted_between(real seconds)
{
	struct network_game *game;
	short next;

	if (game_in_progress() && !main_menu_is_active())
	{
		hosted.played = 1;
		hosted.requested = 0;
		hosted.between = hosted.asked = 0.0f;
		return;
	}
	if (!hosted.played)
		return;
	if (!global_network_game_server_get() || !global_network_game_client_get())
	{
		/* (the player left: the session is over) */
		hosted.session = hosted.played = 0;
		return;
	}
	hosted.between += seconds;
	game = network_game_get_game();
	next = game ? main_get_solo_level_from_name(game->map.name) : NONE;
	if (next == NONE || next < hosted.level || (next == hosted.level && !hosted.requested))
	{
		if (hosted.between > 3.0f)
		{
			platform_log("ae campaign: the campaign's last level is done: no next level is started");
			hosted.session = hosted.played = hosted.requested = 0;
		}
		return;
	}
	/* (the start asked for again every few seconds until the game runs) */
	if (hosted.between >= 3.0f && hosted.asked <= 0.0f)
	{
		if (!hosted.requested)
			platform_log("ae campaign: level %d goes on from the hosted lobby", next + 1);
		hosted.level = next;
		hosted.requested = 1;
		hosted.asked = 6.0f;
		/* (a later game's lobby holds its countdown until the map choice is done: let go, as the test host does) */
		network_game_server_pause_countdown(global_network_game_server_get(), FALSE);
		network_game_client_request_immediate_start();
	}
	else if (hosted.asked > 0.0f)
		hosted.asked -= seconds;
}

void ae_campaign_hosted_update(real seconds)
{
	if (hosted.session)
		hosted_between(seconds);
	if (!hosted.phase)
		return;
	hosted.waited += seconds;
	if (!global_network_game_server_get() || !global_network_game_client_get())
	{
		if (hosted.waited > 5.0f)
		{
			platform_log("ae campaign: the hosted game has no server: given up");
			hosted.phase = 0;
		}
		return;
	}
	if (hosted.phase == 1 && hosted.waited >= 1.0f)
	{
		if (!ui_widget_port_cooperative_level_choose(main_get_solo_level_name(hosted.level), hosted.difficulty))
		{
			platform_log("ae campaign: the hosted level was refused: given up");
			hosted.phase = 0;
			return;
		}
		hosted.phase = 2;
	}
	else if (hosted.phase == 2 && hosted.waited >= 2.0f)
	{
		if (network_game_client_add_player(global_network_game_client_get(), 0))
			hosted.phase = 3;
		else
		{
			platform_log("ae campaign: the player could not be added: given up");
			hosted.phase = 0;
		}
	}
	else if (hosted.phase == 3 && hosted.waited >= 3.0f)
	{
		network_game_client_request_immediate_start();
		platform_log("ae campaign: the hosted level starts");
		hosted.phase = 0;
	}
}

int ae_campaign_start(struct ae_campaign const *campaign, short level, short difficulty)
{
	struct player_profile profile;
	long index;

	/* (0.9.3: every level plays, reached or not; reached only says what the profile has played) */
	if (level < 0 || level >= AE_CAMPAIGN_LEVELS || !player1_profile(&profile, &index, 1))
		return 0;
	platform_log("ae campaign: start level %d on %s", level + 1, ae_campaign_difficulty_name(difficulty));
	/* (with companions asked for: hosted alone, where players can be added) */
	if (ae_companion_asked_for_start())
	{
		hosted_start(level, (short)(difficulty < 0 ? 0 : difficulty > 3 ? 3 : difficulty));
		return 1;
	}
	/* (a co-op start before it, then B out of a lobby or the pause: one player, as main_menu_initialize makes it) */
	player_spawn_count = 1;
	start(main_get_solo_level_name(level), (short)(difficulty < 0 ? 0 : difficulty > 3 ? 3 : difficulty));
	return 1;
}

int ae_campaign_range_map(char *why, int size)
{
	char path[256];
	unsigned long header[2] = { 0, 0 };
	unsigned long bytes_read = 0;
	HANDLE file;

	why[0] = 0;
	if (mcc_level_name(firing_range_map_name()))
		snprintf(why, (size_t)size, "Blood Gulch plays from MCC's files here, not as an Xbox map");
	else
	{
		cache_files_map_file_path("bloodgulch", path, (long)sizeof(path));
		file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
			snprintf(why, (size_t)size, "no Xbox-format bloodgulch.map in maps");
		else
		{
			/* (the header's version, the second word: 5 is an Xbox map; a Custom Edition one is another) */
			if (!ReadFile(file, header, sizeof(header), &bytes_read, NULL) || bytes_read != sizeof(header) || header[1] != 5)
				snprintf(why, (size_t)size, "bloodgulch.map is not an Xbox-format map (version %lu)", header[1]);
			CloseHandle(file);
		}
	}
	if (why[0])
		platform_log("ae campaign: the firing range is not opened: %s", why);

	return why[0] ? 0 : 1;
}

void ae_range_cancel(void)
{
	if (firing_range_requested())
	{
		platform_log("ae campaign: the firing range request is dropped");
		firing_range_tags_loaded("", NONE);
	}
}

void ae_range_gate(char const *scenario_name)
{
	if (!firing_range_requested())
		return;
	if (!ae_range_rules_applies(1, scenario_name ? ae_range_rules_same_path(firing_range_map_name(), scenario_name) : 1,
		network_game_get_game() != NULL, global_network_game_server_get() != NULL, network_game_distributed_client()))
	{
		platform_log("ae campaign: the firing range request is dropped: %s is not the range's map played alone",
			scenario_name ? scenario_name : "this game");
		firing_range_tags_loaded("", NONE);
	}
}

void ae_range_tags_loaded(char const *scenario_name, long scenario_tag_index)
{
	ae_range_gate(scenario_name);
	firing_range_tags_loaded(scenario_name, scenario_tag_index);
}

int ae_campaign_start_range(short difficulty)
{
	struct player_profile profile;
	long index;

	if (!player1_profile(&profile, &index, 1))
		return 0;
	platform_log("ae campaign: the firing range starts on %s", ae_campaign_difficulty_name(difficulty));
	player_spawn_count = 1;
	start(firing_range_map_name(), (short)(difficulty < 0 ? 0 : difficulty > 3 ? 3 : difficulty));
	return 1;
}

/* the start for player 1 and the seated pads (ae_campaign_start_players'), of a map named by its level name; what: the
log's name of it */
static int start_players(char const *map, char const *what, short difficulty, unsigned pads)
{
	struct player_profile profile, profiles[AE_COOP_CONTROLLERS];
	long index, indexes[AE_COOP_CONTROLLERS];
	short controllers[AE_COOP_CONTROLLERS], controller, count = 0, seat;

	/* (every pad's profile read before any slot is written: seat k takes the profile of the k-th pad, and a pad on
	controller 4 alone is seat 2 whose slot is another controller's) */
	for (controller = 1; controller < AE_COOP_CONTROLLERS; controller++)
	{
		if (!(pads & (1u << controller)))
			continue;
		if (!ae_coop_second_controller_ok(controller))
			return AE_COOP_ERR_SECOND_CONTROLLER;
		controllers[count] = controller;
		indexes[count] = player_ui_get_active_player_profile_index(controller);
		player_ui_get_active_player_profile(controller, &profiles[count]);
		count++;
	}
	/* (menu_functions.c's coop_begin: a lobby left for the menus keeps its network game a while, and co-op is not one) */
	dispose_global_network_game_client();
	dispose_global_network_game_server();
	network_game_accept_remote_connections(FALSE);
	player_spawn_count = 1;
	player_ui_reset_single_player_local_player_controllers();
	if (!player1_profile(&profile, &index, 1))
		return AE_COOP_ERR_NO_PROFILE;
	/* (each pad's profile, chosen on its own controller's slot in the lobby's card, is that player's: coop_player2_choose;
	players 2-4 in the order of their controllers) */
	for (seat = 0; seat < count; seat++)
	{
		player_ui_set_single_player_local_player_controller((short)(seat + 1), controllers[seat]);
		player_ui_set_active_player_profile((short)(seat + 1), indexes[seat], &profiles[seat]);
		ae_input_controller_assigned((short)(seat + 1));
	}
	player_spawn_count = (short)(count + 1);
	platform_log("ae campaign: co-op start %s on %s (%d players)", what, ae_campaign_difficulty_name(difficulty), count + 1);
	for (seat = 0; seat < count; seat++)
		platform_log("ae campaign: player %d on controller %d", seat + 2, controllers[seat] + 1);
	start(map, (short)(difficulty < 0 ? 0 : difficulty > 3 ? 3 : difficulty));
	return AE_COOP_ERR_NONE;
}

int ae_campaign_start_players(struct ae_campaign const *campaign, short level, short difficulty, unsigned pads)
{
	char what[16];

	if (!pads)
		return ae_campaign_start(campaign, level, difficulty) ? AE_COOP_ERR_NONE : AE_COOP_ERR_NO_PROFILE;
	if (level < 0 || level >= AE_CAMPAIGN_LEVELS)
		return AE_COOP_ERR_NO_PROFILE;
	snprintf(what, sizeof(what), "level %d", level + 1);
	return start_players(main_get_solo_level_name(level), what, difficulty, pads);
}

int ae_campaign_start_mission(struct ae_campaign const *campaign, const char *level, short difficulty, unsigned pads)
{
	struct player_profile profile;
	long index;
	short at = mcc_maps_find(level);

	(void)campaign;
	/* (a custom campaign's map only: never a stock level's name) */
	if (at == NONE || !mcc_maps_campaign(at))
	{
		platform_log("ae campaign: workshop mission %s not started: not a campaign map of mcc_maps", level ? level : "");
		return AE_COOP_ERR_NO_PROFILE;
	}
	level = mcc_maps_level_name(at);
	if (pads)
		return start_players(level, level, difficulty, pads);
	if (!player1_profile(&profile, &index, 1))
		return AE_COOP_ERR_NO_PROFILE;
	platform_log("ae campaign: workshop start %s on %s", level, ae_campaign_difficulty_name(difficulty));
	player_spawn_count = 1;
	start(level, (short)(difficulty < 0 ? 0 : difficulty > 3 ? 3 : difficulty));
	return AE_COOP_ERR_NONE;
}

/* the profile whose progress CAMPAIGN shows: player 1's, else the one used last */
static long progress_profile(void)
{
	long profile = player_ui_get_active_player_profile_index(0);

	return profile != NONE ? profile : player_ui_get_player1_last_used_profile_index();
}

void ae_campaign_workshop_finished(const char *level, int finished[AE_CAMPAIGN_DIFFICULTIES])
{
	unsigned flags = level ? mcc_progress_menu_flags(progress_profile(), level, system_milliseconds()) : 0;
	short marker;

	memset(finished, 0, sizeof(int) * AE_CAMPAIGN_DIFFICULTIES);
	for (marker = 0; marker < 3; marker++)
		finished[marker + 1] = (flags >> (marker + 1)) & 1;
}

struct workshop_faults
{
	char *first;
	int size, count;
};

/* a fault: one log line, the first kept for the page */
static void workshop_fault(void *context, const char *package, const char *fault)
{
	struct workshop_faults *faults = context;

	platform_log("ae campaign: workshop package %s: %s", package, fault);
	if (!faults->count++ && faults->first && faults->size > 0)
		snprintf(faults->first, (size_t)faults->size, "%s", fault);
}

int ae_campaign_workshop_list(struct ae_ws_campaign *campaigns, int maximum, char *fault, int fault_size, int *fault_count)
{
	struct workshop_faults faults;
	static struct ae_ws_package packages[AE_WS_CAMPAIGNS + 8];
	static struct mcc_manifest manifest;
	short rows, row, index;
	int count = 0, mission, made;
	char package[64], reason[128];

	faults.first = fault;
	faults.size = fault_size;
	faults.count = 0;
	if (fault && fault_size > 0)
		fault[0] = 0;
	if (fault_count)
		*fault_count = 0;
	if (!campaigns || maximum <= 0 || !config_boolean("game.mcc_campaigns"))
		return 0;
	/* (read again each time: a package installed since the last look is listed) */
	mcc_maps_rescan();
	rows = mcc_maps_type_count(TRUE);
	for (row = 0; row < rows && count < (int)NUMBEROF(packages); row++)
	{
		char const *level = mcc_maps_level_name(mcc_maps_type_index(row, TRUE));
		struct ae_ws_package *entry;
		int known = 0, result;

		/* (a loose map of mcc_maps is no package: no manifest) */
		if (!level || !mcc_workshop_package_folder(level, package))
			continue;
		for (index = 0; index < count && !known; index++)
			known = !csstrcasecmp(packages[index].package, package);
		if (known)
			continue;
		entry = &packages[count++];
		memset(entry, 0, sizeof(*entry));
		snprintf(entry->package, sizeof(entry->package), "%s", package);
		reason[0] = 0;
		result = mcc_manifest_for_level(level, &manifest, reason);
		entry->manifest = result == 1 ? AE_WS_MANIFEST_OK : result < 0 ? AE_WS_MANIFEST_DAMAGED : AE_WS_MANIFEST_NONE;
		snprintf(entry->reason, sizeof(entry->reason), "%s", reason);
		if (result != 1)
			continue;
		snprintf(entry->title, sizeof(entry->title), "%s", manifest.title);
		entry->count = manifest.count < AE_WS_PACKAGE_MISSIONS ? manifest.count : AE_WS_PACKAGE_MISSIONS;
		for (mission = 0; mission < entry->count; mission++)
		{
			struct ae_ws_package_mission *to = &entry->missions[mission];
			int written;

			snprintf(to->name, sizeof(to->name), "%s", manifest.missions[mission].name);
			snprintf(to->title, sizeof(to->title), "%s", manifest.missions[mission].title);
			written = snprintf(to->level, sizeof(to->level), "%s%s\\%s", MCC_MAPS_LEVEL_PREFIX, package, to->name);
			index = written > 0 && written < (int)sizeof(to->level) ? mcc_maps_find(to->level) : NONE;
			to->map = index == NONE ? AE_WS_MAP_MISSING : mcc_maps_campaign(index) ? AE_WS_MAP_CAMPAIGN :
				AE_WS_MAP_MULTIPLAYER;
			if (to->map != AE_WS_MAP_MISSING)
				snprintf(to->level, sizeof(to->level), "%s", mcc_maps_level_name(index));
		}
	}
	made = ae_ws_campaigns_make(packages, count, campaigns, maximum, workshop_fault, &faults);
	if (fault_count)
		*fault_count = faults.count;
	return made;
}

int ae_campaign_continue(struct ae_campaign const *campaign)
{
	struct player_profile profile;
	char const *map_name;
	short level, difficulty;
	long index;

	if (campaign->saved_level < 0 || !player1_profile(&profile, &index, 1) ||
		!ui_widget_port_saved_game(&map_name, &level, &difficulty))
		return 0;
	player_spawn_count = 1;
	platform_log("ae campaign: continue level %d on %s", level + 1, ae_campaign_difficulty_name(difficulty));
	start(map_name, difficulty);
	return 1;
}

/* the package folder of a level of mcc_maps, lower-cased ("" none) */
static void package_of(const char *level, char folder[64])
{
	int index;

	if (!mcc_workshop_package_folder(level, folder))
		folder[0] = 0;
	for (index = 0; folder[index]; index++)
		if (folder[index] >= 'A' && folder[index] <= 'Z')
			folder[index] = (char)(folder[index] + 'a' - 'A');
}

int ae_campaign_workshop_saved(const char *mission, char level[256], short *difficulty, char *why, int why_size)
{
	struct player_profile profile;
	long index;
	boolean corrupted = FALSE, found;
	char package[64], saved_package[64], message[512];

	level[0] = 0;
	*difficulty = -1;
	if (why && why_size > 0)
		why[0] = 0;
	if (!mission || !ae_saved_game_file_custom(mission) || !player1_profile(&profile, &index, 0))
		return 0;
	/* (upstream's mcc_saved_game_read: the saved game's header, from this campaign's own file) */
	ae_saved_game_file_level(mission);
	found = game_state_test_persistent_storage(level, difficulty, &corrupted);
	ae_saved_game_file_level(NULL);
	if (!found)
	{
		level[0] = 0;
		*difficulty = -1;
		if (corrupted)
		{
			platform_log("ae campaign: %s's saved game could not be read: not offered", mission);
			if (why && why_size > 0)
				snprintf(why, (size_t)why_size, "Its saved game could not be read.");
			return -1;
		}
		return 0;
	}
	package_of(mission, package);
	package_of(level, saved_package);
	if (!mcc_level_name(level) || !saved_package[0] || strcmp(package, saved_package))
	{
		platform_log("ae campaign: the saved game of %s's file is of another map (%s): not offered", mission, level);
		if (why && why_size > 0)
			snprintf(why, (size_t)why_size, "Its saved game is of another campaign.");
		level[0] = 0;
		return -1;
	}
	if (!mcc_cache_present(level, 0, message, sizeof(message)))
	{
		platform_log("ae campaign: the saved game's mission %s cannot load: %s", level, message);
		if (why && why_size > 0)
			snprintf(why, (size_t)why_size, "Its saved mission's map is missing or damaged. Install the package again.");
		return -1;
	}
	*difficulty = (short)PIN(*difficulty, 0, AE_CAMPAIGN_DIFFICULTIES - 1);
	return 1;
}

int ae_campaign_workshop_continue(const char *mission)
{
	struct player_profile profile;
	char level[256], why[128];
	short difficulty;
	long index;

	if (ae_campaign_workshop_saved(mission, level, &difficulty, why, sizeof(why)) != 1 ||
		!player1_profile(&profile, &index, 1))
		return 0;
	/* (upstream's campaign_continue: the package must still be whole, then the level started at the saved difficulty;
	main_new_map takes the saved game from the campaign's own file) */
	if (!mcc_cache_require(level, 0))
		return 0;
	player_spawn_count = 1;
	platform_log("ae campaign: continue %s on %s (the campaign's own saved game)", level,
		ae_campaign_difficulty_name(difficulty));
	start(level, difficulty);
	return 1;
}
