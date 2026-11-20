/* The offline social career's core, glued to the game (see the header). */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "../src/ae_shipped_files.h"
#include "ae_dev_cmd_rules.h"
#include "ae_dev_events.h"
#include "ae_glue_career.h"
#include "ae_glue_profiles.h"
#include "ae_glue_bot_records.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_service_record.h"
#include "ae_social_rules.h"
#include "ae_stage_map.h"

char const *config_string(char const *name);
int config_write(char const *name, char const *value);

void platform_log(char const *format, ...);
boolean game_in_progress(void);
boolean main_menu_is_active(void);

enum
{
	TEXT_SIZE = 65536,
	LOCAL_PLAYERS = 4,
	MODE_IDS = 16
};

/* a build that ships no social.txt (Android, the web): stock maps only, two playlists */
static char const built_in[] =
	"# social: team-slayer\n# label: TEAM SLAYER\n# about: Four against four. Slayer.\n# teams: 2 x 4\n# party: 4\n"
	"damnation:file=AE TEAM SLY  # weight 2\nprisoner:file=AE TEAM SLY  # weight 2\nhangemhigh:file=AE TEAM SLY  # weight 2\n"
	"chillout:file=AE TEAM SLY  # weight 2\nbeavercreek:file=AE TEAM SLY  # weight 2\n\n"
	"# social: free-for-all\n# label: FREE FOR ALL\n# about: Eight players, every one for themselves.\n# players: 8\n# party: 1\n"
	"prisoner:file=AE FFA SLAY  # weight 2\nwizard:file=AE FFA SLAY  # weight 2\nchillout:file=AE FFA SLAY  # weight 2\n"
	"hangemhigh:file=AE FFA SLAY  # weight 2\n";

static struct
{
	struct ae_social_playlist lists[AE_SOCIAL_PLAYLISTS];
	int count;
	int loaded;
	/* the maps a DEV command hides: all of the pack's, or the one named */
	int hide_pack;
	char hide_map[AE_SOCIAL_MAP];
	/* the bots' mode of each playlist, this run */
	char mode_id[MODE_IDS][AE_SOCIAL_ID];
	int mode[MODE_IDS];
	int modes;
} state;

static char const *const mode_words[AE_SOCIAL_SKILL_STEPS + 1] =
	{"auto", "recruit", "easy", "normal", "heroic", "elite", "legendary", "demon"};
static int same_text(char const *a, char const *b);
static void modes_read_config(void);

static void load(void)
{
	static char text[TEXT_SIZE];

	if (state.loaded)
		return;
	state.loaded = 1;
	state.count = ae_shipped_file_read("social.txt", text, (long)sizeof(text)) > 0 ?
		ae_social_playlists_parse(text, state.lists, AE_SOCIAL_PLAYLISTS) : 0;
	if (!state.count)
	{
		platform_log("ae social: no social.txt: the built-in playlists");
		state.count = ae_social_playlists_parse(built_in, state.lists, AE_SOCIAL_PLAYLISTS);
	}
	else
		platform_log("ae social: social.txt: %d playlists", state.count);
	modes_read_config();
}

int ae_social_playlist_count(void)
{
	load();
	return state.count;
}

struct ae_social_playlist const *ae_social_playlist_at(int index)
{
	load();
	return index >= 0 && index < state.count ? &state.lists[index] : NULL;
}

struct ae_social_playlist const *ae_social_playlist_by_id(char const *id)
{
	int index;

	load();
	index = ae_social_playlist_find(state.lists, state.count, id);
	return index >= 0 ? &state.lists[index] : NULL;
}

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

int ae_social_map_here(void *context, char const *map)
{
	char const *at = strchr(map, '@');

	(void)context;
	if (state.hide_pack && at && same_text(at, "@smp"))
		return 0;
	if (state.hide_map[0])
	{
		size_t length = at ? (size_t)(at - map) : strlen(map);

		if (strlen(state.hide_map) == length && !strncmp(map, state.hide_map, length))
			return 0;
	}
	return ae_stage_map_file_present(map);
}

void ae_social_maps_hide(char const *what)
{
	state.hide_pack = 0;
	state.hide_map[0] = 0;
	if (!what || !what[0] || same_text(what, "none"))
		return;
	if (same_text(what, "smp"))
		state.hide_pack = 1;
	else
		snprintf(state.hide_map, sizeof(state.hide_map), "%s", what);
}

int ae_social_profile_key(short local_player, char *key, size_t size)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count = 0, each;
	long index;

	if (local_player < 0 || local_player >= LOCAL_PLAYERS || size < 2)
		return 0;
	index = player_ui_get_active_player_profile_index(local_player);
	if (index == NONE || !ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
		return 0;
	for (each = 0; each < count; each++)
		if (profiles[each].index == index)
		{
			ae_sr_key(profiles[each].name, key, size);
			return key[0] != 0;
		}
	return 0;
}

int ae_social_profile_name(short local_player, char *name, size_t size)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count = 0, each;
	long index;

	if (local_player < 0 || local_player >= LOCAL_PLAYERS || size < 2)
		return 0;
	index = player_ui_get_active_player_profile_index(local_player);
	if (index == NONE || !ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
		return 0;
	for (each = 0; each < count; each++)
		if (profiles[each].index == index)
		{
			snprintf(name, size, "%s", profiles[each].name);
			return name[0] != 0;
		}
	return 0;
}

static unsigned long clock_now(void);

/* ---------- EXP, rank, match rating, mode */

long ae_social_profile_exp(char const *profile, char const *playlist)
{
	struct ae_career_social *entry = ae_career_social_find(ae_career_store(), profile, playlist);

	return entry ? entry->exp : 0;
}

int ae_social_profile_rank(char const *profile, char const *playlist)
{
	return ae_social_rank(ae_social_profile_exp(profile, playlist));
}

long ae_social_profile_exp_total(char const *profile)
{
	return ae_career_social_total_exp(ae_career_store(), profile);
}

int ae_social_profile_overall_rank(char const *profile)
{
	return ae_social_overall_rank(ae_social_profile_exp_total(profile));
}

long ae_social_profile_match_points(char const *profile, char const *playlist)
{
	struct ae_career_match *entry = ae_career_match_find(ae_career_store(), profile, playlist);

	return entry ? entry->points : ae_career_match_start(ae_career_store(), profile);
}

int ae_social_profile_experience(char const *profile)
{
	return ae_career_experience_get(ae_career_store(), profile);
}

int ae_social_profile_experience_set(char const *profile, int experience, int restart)
{
	struct ae_career *career = ae_career_store();
	struct ae_career before = *career;
	int moved = 0;

	if (!ae_career_experience_set(career, profile, experience))
		return 0;
	if (restart)
		moved = ae_career_match_restart(career, profile, clock_now());
	if (!ae_career_store_save())
	{
		*career = before;
		platform_log("ae social: calibration: career.txt could not be written: the answer is not kept");
		return 0;
	}
	platform_log("ae social: calibration %s %s%s", profile, experience > 0 ? ae_social_experience_name(experience) : "unset",
		restart ? (moved ? ": every playlist started again" : ": starts at the new answer") : "");
	return 1;
}

long ae_social_profile_match_games(char const *profile, char const *playlist)
{
	struct ae_career_match *entry = ae_career_match_find(ae_career_store(), profile, playlist);

	return entry ? entry->games : 0;
}

int ae_social_mode(char const *playlist)
{
	int index;

	load();
	for (index = 0; index < state.modes; index++)
		if (same_text(state.mode_id[index], playlist))
			return state.mode[index];
	return AE_SOCIAL_MODE_AUTO;
}

void ae_social_mode_set(char const *playlist, int mode)
{
	int index;

	if (mode < 0 || mode > AE_SOCIAL_SKILL_STEPS)
		return;
	load();
	for (index = 0; index < state.modes; index++)
		if (same_text(state.mode_id[index], playlist))
		{
			state.mode[index] = mode;
			return;
		}
	if (state.modes >= MODE_IDS)
		return;
	snprintf(state.mode_id[state.modes], sizeof(state.mode_id[0]), "%s", playlist);
	state.mode[state.modes++] = mode;
}

/* the modes kept in config.toml (social.skills: "team-slayer=heroic,doubles=elite"): read with the playlists, written by the menus */
static void modes_read_config(void)
{
	char const *value = config_string("social.skills");
	char text[512], *entry, *next;

	if (!value || !value[0])
		return;
	snprintf(text, sizeof(text), "%s", value);
	for (entry = text; entry && *entry; entry = next)
	{
		char *equals;
		int mode;

		next = strchr(entry, ',');
		if (next)
			*next++ = 0;
		equals = strchr(entry, '=');
		if (!equals)
			continue;
		*equals = 0;
		for (mode = 0; mode <= AE_SOCIAL_SKILL_STEPS; mode++)
			if (same_text(equals + 1, mode_words[mode]))
				break;
		if (mode <= AE_SOCIAL_SKILL_STEPS && state.modes < MODE_IDS)
		{
			snprintf(state.mode_id[state.modes], sizeof(state.mode_id[0]), "%s", entry);
			state.mode[state.modes++] = mode;
		}
	}
}

void ae_social_modes_save(void)
{
	char text[512] = "";
	int index;

	for (index = 0; index < state.modes; index++)
	{
		size_t used = strlen(text);

		if (state.mode[index] <= 0)
			continue;
		snprintf(text + used, sizeof(text) - used, "%s%s=%s", used ? "," : "", state.mode_id[index], mode_words[state.mode[index]]);
	}
	config_write("social.skills", text);
}

/* ---------- a game */

static unsigned long clock_now(void)
{
	return (unsigned long)time(NULL);
}

int ae_social_roster_for(char const *playlist, char const *profile, int humans, unsigned long seed, struct ae_social_roster *roster)
{
	struct ae_social_playlist const *list = ae_social_playlist_by_id(playlist);
	struct ae_social_shape shape;

	if (!list)
		return 0;
	long points = ae_social_profile_match_points(profile, playlist);

	shape.teams = list->teams;
	shape.size = list->size;
	/* (0.9.4) a split screen party draws at the average of its players' ratings; one human: his own */
	if (humans > 1)
	{
		long party[AE_SOCIAL_LOCAL];
		int local, count = humans < AE_SOCIAL_LOCAL ? humans : AE_SOCIAL_LOCAL;

		for (local = 0; local < count; local++)
		{
			char key[AE_SR_KEY];

			party[local] = local == 0 ? points : ae_social_screen_key((short)local, key, sizeof(key)) ?
				ae_social_profile_match_points(key, playlist) : ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_UNSET);
		}
		points = ae_social_party_points(party, count);
	}
	return ae_social_roster_draw(&shape, humans, points, ae_social_mode(playlist), seed, roster);
}

int ae_social_pending_begin(char const *profile, char const *playlist)
{
	struct ae_career *career = ae_career_store();
	struct ae_career before = *career;
	int saved;

	/* (the crash mark first: a crash after the line is written forgives the game, one before it has no game to forgive) */
	if (!ae_career_crash_mark_arm())
		platform_log("ae social: the crash mark could not be armed");
	if (!ae_career_pending_set(career, profile, playlist, clock_now()))
	{
		if (before.pending_count == 0)
			ae_career_crash_mark_disarm();
		return 0;
	}
	saved = ae_career_store_save();
	if (!saved)
	{
		*career = before;
		if (career->pending_count == 0)
			ae_career_crash_mark_disarm();
	}
	return saved;
}

/* no game pending any more: the crash mark is not needed */
static void pending_disarm(struct ae_career const *career)
{
	if (career->pending_count == 0)
		ae_career_crash_mark_disarm();
}

int ae_social_pending_end(char const *profile)
{
	struct ae_career *career = ae_career_store();
	struct ae_career before = *career;
	int saved;

	if (!ae_career_pending_clear(career, profile))
		return 0;
	saved = ae_career_store_save();
	if (saved)
		pending_disarm(career);
	else
		*career = before;
	return saved;
}

int ae_social_result_apply(char const *profile, char const *playlist, int outcome, struct ae_social_roster const *roster, int self,
	int const *seat_place, int kills, int deaths, struct ae_social_result *result)
{
	struct ae_career *career = ae_career_store();
	struct ae_career before = *career;
	struct ae_social_result local;
	int levels[AE_SOCIAL_SEATS], results[AE_SOCIAL_SEATS], count = 0, seat;
	long games, change, exp_before, match_before;
	unsigned long clock = clock_now();
	char kd[32] = "";

	if (!result)
		result = &local;
	if (!roster || self < 0 || self >= roster->count || outcome < AE_SOCIAL_LOSS || outcome > AE_SOCIAL_QUIT)
		return 0;
	exp_before = ae_social_profile_exp(profile, playlist);
	match_before = ae_social_profile_match_points(profile, playlist);
	games = ae_social_profile_match_games(profile, playlist);
	/* (the opponents: another team's seats; a free for all's every other seat. A quit loses to every one) */
	for (seat = 0; seat < roster->count; seat++)
	{
		if (seat == self || (roster->teams > 0 && roster->seat[seat].team == roster->seat[self].team))
			continue;
		levels[count] = roster->seat[seat].level - 1;
		if (outcome == AE_SOCIAL_QUIT)
			results[count] = -1;
		else if (seat_place && roster->teams == 0)
			results[count] = seat_place[self] < seat_place[seat] ? 1 : seat_place[self] > seat_place[seat] ? -1 : 0;
		else
			results[count] = outcome == AE_SOCIAL_WIN ? 1 : outcome == AE_SOCIAL_TIE ? 0 : -1;
		count++;
	}
	change = ae_social_match_change(match_before, games, levels, results, count);
	if (kills >= 0 && deaths >= 0)
		snprintf(kd, sizeof(kd), " kd %d/%d", kills, deaths);
	/* (0.9.4) a placement game also counts kills and deaths; a quit or a game with no stats (kills < 0) does not) */
	if (kills >= 0 && deaths >= 0 && outcome != AE_SOCIAL_QUIT)
		change = ae_social_placement_change(change, games, kills, deaths);
	/* (both lines or neither: a full table refuses before anything moves) */
	if (!ae_career_social_commit(career, profile, playlist, outcome, ae_social_match_apply(match_before, change), clock))
	{
		platform_log("ae social: result %s: the career file is full: not counted", playlist);
		return 0;
	}
	result->outcome = outcome;
	result->exp_before = exp_before;
	result->exp_after = ae_social_profile_exp(profile, playlist);
	result->rank_before = ae_social_rank(exp_before);
	result->rank_after = ae_social_rank(result->exp_after);
	result->match_before = match_before;
	result->match_after = ae_social_profile_match_points(profile, playlist);
	result->saved = ae_career_store_save();
	if (result->saved)
		pending_disarm(career);
	else
	{
		*career = before;
		platform_log("ae social: result %s: career.txt could not be written: the progress is not saved", playlist);
	}
	platform_log("ae social: result %s %s: exp %ld to %ld (rank %d to %d), match %ld to %ld%s", playlist,
		outcome == AE_SOCIAL_WIN ? "win" : outcome == AE_SOCIAL_TIE ? "tie" : outcome == AE_SOCIAL_QUIT ? "quit" : "loss",
		result->exp_before, result->exp_after, result->rank_before, result->rank_after, result->match_before,
		result->match_after, kd);
	ae_dev_event("social_result", "\"id\":\"%s\",\"outcome\":\"%s\",\"exp_before\":%ld,\"exp_after\":%ld,\"rank\":%d,"
		"\"match_before\":%ld,\"match_after\":%ld,\"saved\":%d", playlist,
		outcome == AE_SOCIAL_WIN ? "win" : outcome == AE_SOCIAL_TIE ? "tie" : outcome == AE_SOCIAL_QUIT ? "quit" : "loss",
		result->exp_before, result->exp_after, result->rank_after, result->match_before, result->match_after, result->saved);
	return 1;
}

/* ---------- DEV MODE */

static int mode_of_word(char const *word)
{
	int mode;

	for (mode = 0; mode <= AE_SOCIAL_SKILL_STEPS; mode++)
		if (same_text(word, mode_words[mode]))
			return mode;
	return -1;
}

/* the playlists a command names: one (index into the list, or -1 for none) or all */
static int dev_playlists(char const *id, int *first, int *last, char *text, int size)
{
	load();
	if (same_text(id, "all"))
	{
		*first = 0;
		*last = state.count - 1;
		return 1;
	}
	*first = ae_social_playlist_find(state.lists, state.count, id);
	if (*first < 0)
	{
		snprintf(text, (size_t)size, "no social playlist \"%s\"", id);
		return 0;
	}
	*last = *first;
	return 1;
}

int ae_social_dev(struct ae_dev_cmd const *cmd, char *text, int size)
{
	char profile[AE_SR_KEY];
	char const *sub = cmd->word[0];
	int player = (int)cmd->number, first, last, each;

	if (!ae_social_profile_key((short)player, profile, sizeof(profile)))
		snprintf(profile, sizeof(profile), "dev-player%d", player + 1);
	if (!strcmp(sub, "maps"))
	{
		ae_social_maps_hide(cmd->word[2]);
		snprintf(text, (size_t)size, same_text(cmd->word[2], "none") ? "social maps: none hidden" : "social maps: %s hidden",
			cmd->word[2]);
		return 1;
	}
	if (!strcmp(sub, "calibration"))
	{
		char who[AE_SR_KEY];

		if (same_text(cmd->word[1], "me"))
			snprintf(who, sizeof(who), "%s", profile);
		else
			ae_sr_key(cmd->word[1], who, sizeof(who));
		if (!ae_social_profile_experience_set(who, (int)cmd->seed, 0))
		{
			snprintf(text, (size_t)size, "social calibration: not written");
			return 0;
		}
		snprintf(text, (size_t)size, "social calibration %s: %s", who, cmd->seed ? ae_social_experience_name((int)cmd->seed) : "unset");
		return 1;
	}
	if (!strcmp(sub, "mode"))
	{
		int mode = mode_of_word(cmd->word[2]);

		if (!dev_playlists(cmd->word[1], &first, &last, text, size))
			return 0;
		for (each = first; each <= last; each++)
			ae_social_mode_set(state.lists[each].id, mode);
		snprintf(text, (size_t)size, "social mode %s: %s", cmd->word[1], mode ? "FIXED" : "AUTO");
		return 1;
	}
	if (!strcmp(sub, "reset"))
	{
		int removed = 0;

		if (cmd->words < 2 || same_text(cmd->word[1], "all") || same_text(cmd->word[1], "player"))
			removed = ae_career_social_reset(ae_career_store(), profile, NULL);
		else
		{
			if (!dev_playlists(cmd->word[1], &first, &last, text, size))
				return 0;
			removed = ae_career_social_reset(ae_career_store(), profile, state.lists[first].id);
		}
		ae_career_store_save();
		snprintf(text, (size_t)size, "social reset: %d lines of %s taken away", removed, profile);
		return 1;
	}
	if (!strcmp(sub, "carnage"))
	{
		/* (0.9.4: a made-up tournament post-game's board, ae_glue_bot_records.h) */
		int cursor = (int)cmd->position[1];

		return ae_bot_records_dev_carnage(cmd->word[1], cmd->seed, (int)cmd->position[0],
			cursor > 0 && cursor < cmd->words ? cmd->word[cursor] : NULL, text, size);
	}
	if (!strcmp(sub, "postgame"))
	{
		/* (postgame win|tie|loss|rankup | place <p> <n>, then [<id>] [exp <n>]) */
		int at = !strcmp(cmd->word[1], "place") ? 4 : 2;
		char const *id = at < cmd->words && strcmp(cmd->word[at], "exp") && strcmp(cmd->word[at], "player") ? cmd->word[at] :
			(!strcmp(cmd->word[1], "place") ? "free-for-all" : "team-slayer");

		return ae_social_game_dev_postgame(cmd->word[1], id, (long)cmd->position[2], (int)cmd->position[0], (int)cmd->position[1],
			player, text, (size_t)size);
	}
	if (!strcmp(sub, "start"))
	{
		char const *map = NULL;
		int now = 0, vote = 0, left = -1, has_votes = 0, party = 0, votes[3] = { 0, 0, 0 };

		for (each = 2; each < cmd->words; each++)
		{
			if (!strcmp(cmd->word[each], "now"))
				now = 1;
			else if (!strcmp(cmd->word[each], "vote"))
				vote = 1;
			else if (!strcmp(cmd->word[each], "party") && each + 1 < cmd->words)
				party = atoi(cmd->word[++each]);
			else if (!strcmp(cmd->word[each], "left") && each + 1 < cmd->words)
				left = atoi(cmd->word[++each]);
			else if (!strcmp(cmd->word[each], "votes") && each + 1 < cmd->words)
				has_votes = ae_dev_cmd_votes(cmd->word[++each], votes);
			else if (!strcmp(cmd->word[each], "variant") && each + 1 < cmd->words)
			{
				char const *word = cmd->word[++each];

				ae_social_plan_force_start(!strcmp(word, "br") ? AE_SOCIAL_START_BR : !strcmp(word, "evo") ? AE_SOCIAL_START_EVO :
					!strcmp(word, "magnum") ? AE_SOCIAL_START_MAGNUM : !strcmp(word, "protomag") ? AE_SOCIAL_START_PROTOMAG :
					!strcmp(word, "none") ? AE_SOCIAL_START_NONE : -1);
			}
			else if (!strcmp(cmd->word[each], "map") && each + 1 < cmd->words)
				map = cmd->word[++each];
			else if (!strcmp(cmd->word[each], "seed"))
				each++;
		}
		if (game_in_progress() && !main_menu_is_active())
		{
			snprintf(text, (size_t)size, "social start: from the menus only (leave this game first)");
			return 0;
		}
		ae_social_dev_party(party);
		if (vote || left >= 0 || has_votes)
			return ae_social_game_dev_lobby_vote(cmd->word[1], cmd->seed, left, votes, has_votes, text, (size_t)size);
		return ae_social_game_dev_lobby(cmd->word[1], map, cmd->seed, now, text, (size_t)size);
	}
	if (!dev_playlists(cmd->word[1], &first, &last, text, size))
		return 0;
	if (!strcmp(sub, "exp"))
	{
		for (each = first; each <= last; each++)
		{
			struct ae_career_social *entry = ae_career_social_entry(ae_career_store(), profile, state.lists[each].id);

			if (!entry)
			{
				snprintf(text, (size_t)size, "social exp: the career file is full");
				return 0;
			}
			entry->exp = (long)cmd->seed;
		}
		ae_career_store_save();
		snprintf(text, (size_t)size, "social exp %s: %lu (rank %d)", cmd->word[1], cmd->seed, ae_social_rank((long)cmd->seed));
		return 1;
	}
	if (!strcmp(sub, "match"))
	{
		long points = cmd->words == 4 || (cmd->words == 6) ? ae_social_match_points_of_tier((int)cmd->seed) : (long)cmd->seed;

		for (each = first; each <= last; each++)
		{
			struct ae_career_match *entry = ae_career_match_entry(ae_career_store(), profile, state.lists[each].id);

			if (!entry)
			{
				snprintf(text, (size_t)size, "social match: the career file is full");
				return 0;
			}
			entry->points = points;
		}
		ae_career_store_save();
		snprintf(text, (size_t)size, "social match %s: %ld points, tier %d", cmd->word[1], points, ae_social_match_tier(points));
		return 1;
	}
	if (!strcmp(sub, "pending"))
	{
		int crashed;

		if (!ae_social_pending_begin(profile, state.lists[first].id))
		{
			snprintf(text, (size_t)size, "social pending: not written");
			return 0;
		}
		/* (as after a restart: the next read of career.txt finds the line: counted as a quit, or forgiven when the crash mark is
		there as well) */
		crashed = cmd->words >= 3 && same_text(cmd->word[2], "crashed");
		ae_career_crash_mark_write(crashed);
		ae_career_crash_mark_disarm();
		ae_career_store_forget();
		snprintf(text, (size_t)size, "social pending %s: written%s; the next read of career.txt finds it, as after a restart",
			state.lists[first].id, crashed ? " with the crash mark" : "");
		return 1;
	}
	if (!strcmp(sub, "roster"))
	{
		struct ae_social_roster roster;
		unsigned long seed = cmd->seed ? cmd->seed : (unsigned long)clock_now() % 99999999ul;
		char line[240], tiers[96] = "";
		int seat;

		if (first != last)
		{
			snprintf(text, (size_t)size, "social roster: name one playlist");
			return 0;
		}
		if (!ae_social_roster_for(state.lists[first].id, profile, 1, seed, &roster))
		{
			snprintf(text, (size_t)size, "social roster: %s does not fit a roster", state.lists[first].id);
			return 0;
		}
		ae_social_roster_text(&roster, state.lists[first].id, seed, line, (int)sizeof(line));
		for (seat = roster.humans; seat < roster.count; seat++)
		{
			char piece[8];

			snprintf(piece, sizeof(piece), "%sT%d", seat > roster.humans ? " " : "", roster.seat[seat].tier);
			strncat(tiers, piece, sizeof(tiers) - strlen(tiers) - 1);
		}
		platform_log("ae social: %s", line);
		ae_dev_event("social_roster", "\"id\":\"%s\",\"seed\":%lu,\"mode\":\"%s\",\"you\":%d,\"tiers\":\"%s\"",
			state.lists[first].id, seed, mode_words[roster.fixed], roster.player_tier, tiers);
		snprintf(text, (size_t)size, "ae social: %s", line);
		return 1;
	}
	snprintf(text, (size_t)size, "social: unknown command");
	return 0;
}
