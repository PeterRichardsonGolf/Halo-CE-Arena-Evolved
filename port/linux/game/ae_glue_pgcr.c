/* The post-game report screen's contact with the game (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "tag_files/tag_files.h"
#include "ae_glue_pgcr.h"
#include "ae_bomb_assault.h"
#include "ae_glue_service_record.h"
#include "bots.h"
#include "ae_lan_bots.h"
#include "ae_glue_postgame.h"
#include "ae_pgcr_data.h"
#include "ae_scoreboard.h"
#include "ae_strip_model.h"

/* game_engine.c's ranking (populate_statistic_buffer, _postgame_statistic_ranking 0): ae_scoreboard_stat's layout */
long populate_statistic_buffer(struct ae_scoreboard_stat *statistic_buffer, long statistic, boolean inverse);
#define RANKING 0
#define MAXIMUM_RANKED 128
#define TEAM_RED 0xC0392BFFu
#define TEAM_BLUE 0x2E86DEFFu
#define NEUTRAL 0x84939FFFu

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);

static struct ae_pgr_report kept;
/* the service record takes a game once: when its report is first kept with the host's summaries, else a few seconds after */
enum { RECORD_WAIT_MS = 4000 };
static int recorded;
static unsigned long first_kept_ms;

/* UTF-16 to UTF-8 */
static void utf8(wchar_t const *text, char *out, int size)
{
	int at = 0;

	for (; text && *text && at < size - 4; text++)
	{
		unsigned long c = (unsigned long)(unsigned short)*text;

		if (c < 0x80)
			out[at++] = (char)c;
		else if (c < 0x800)
		{
			out[at++] = (char)(0xC0 | (c >> 6));
			out[at++] = (char)(0x80 | (c & 0x3F));
		}
		else
		{
			out[at++] = (char)(0xE0 | (c >> 12));
			out[at++] = (char)(0x80 | ((c >> 6) & 0x3F));
			out[at++] = (char)(0x80 | (c & 0x3F));
		}
	}
	out[at] = 0;
}

static void ordinal(long place, char *out, int size)
{
	wchar_t wide[16];

	ae_strip_format_place(place + 1, wide, NUMBEROF(wide));
	utf8(wide, out, size);
}

static void score_text(long score, int clock, char *out, int size)
{
	if (clock)
	{
		long seconds = MAX(score, 0) / TICKS_PER_SECOND;

		snprintf(out, size, "%ld:%02ld", seconds / 60, seconds % 60);
	}
	else
		snprintf(out, size, "%ld", score);
}

static unsigned int rgba_of(real red, real green, real blue)
{
	return ((unsigned int)(PIN(red, 0.0f, 1.0f) * 255.0f + 0.5f) << 24) |
		((unsigned int)(PIN(green, 0.0f, 1.0f) * 255.0f + 0.5f) << 16) |
		((unsigned int)(PIN(blue, 0.0f, 1.0f) * 255.0f + 0.5f) << 8) | 0xFF;
}

static void fill_row(struct ae_pgr_row *row, struct ae_scoreboard_stat const *stat, int teams, int clock,
	enum ae_pgr_mode mode, long place_rank)
{
	struct player_datum *player = player_try_and_get(stat->player_index);
	wchar_t buffer[48];
	wchar_t const *name;
	struct ae_pgcr_summary const *summary;
	int index;

	memset(row, 0, sizeof(*row));
	row->slot = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(stat->player_index);
	row->team = -1;
	row->kills = stat->kills;
	row->deaths = stat->deaths;
	row->assists = stat->assists;
	row->betrayals = row->suicides = row->shots_fired = row->shots_hit = AE_PGR_UNKNOWN;
	for (index = 0; index < AE_PGR_OBJECTIVES; index++)
		row->objective[index] = AE_PGR_UNKNOWN;
	row->rank = teams ? place_rank : (long)(stat->place & 0x7F);
	if (!teams)
		ordinal(stat->place & 0x7F, row->place, sizeof(row->place));
	else
		snprintf(row->place, sizeof(row->place), "%ld", place_rank + 1);
	if (!player)
		return;
	row->me = player->local_player_index != NONE;
	row->quit = player->quit_out_of_game ? 1 : 0;
	name = ae_game_engine_scoreboard_name(stat->player_index, buffer, NUMBEROF(buffer));
	utf8(name, row->name, sizeof(row->name));
	if (!row->name[0])
		snprintf(row->name, sizeof(row->name), "Player");
	/* (a bot is no player of the record or the cards: bots.c's, the host's LAN bots (ae_lan_bots.c), or, on a joiner, the
	"BOT " the host puts before their names) */
	row->bot = (player->local_player_index == NONE && bots_machine_is_bot(player->network_player_data.machine_index)) ||
		ae_lan_bots_network_player(&player->network_player_data) || !strncmp(row->name, "BOT ", 4) ? 1 : 0;
	row->score_value = game_engine->get_player_score ? game_engine->get_player_score(stat->player_index, FALSE) : stat->score;
	score_text(row->score_value, clock, row->score, sizeof(row->score));
	row->betrayals = player->statistics.friendly_fire_kills;
	row->suicides = player->statistics.suicides;
	row->shots_fired = player->statistics.shots_fired;
	row->shots_hit = player->statistics.shots_hit;
	if (mode == AE_PGR_MODE_FLAG || mode == AE_PGR_MODE_BOMB)
	{
		row->objective[0] = player->statistics.multiplayer_statistics.ctf_statistics.flag_grabs;
		row->objective[1] = mode == AE_PGR_MODE_FLAG ?
			player->statistics.multiplayer_statistics.ctf_statistics.flag_returns : AE_PGR_UNKNOWN;
		row->objective[2] = player->statistics.multiplayer_statistics.ctf_statistics.flag_scores;
	}
	if (teams)
	{
		long team = PIN(player->team_index, 0, 1);

		row->chip = team == 0 ? TEAM_RED : TEAM_BLUE;
	}
	else
	{
		union real_rgb_color armour;

		row->chip = game_engine_player_get_change_color(&armour, stat->player_index) ?
			rgba_of(armour.red, armour.green, armour.blue) : NEUTRAL;
	}
	summary = ae_pgcr_data_get(row->slot);
	if (summary)
	{
		row->summary = *summary;
		for (index = 0; index < summary->weapon_count && index < AE_PGCR_SUMMARY_WEAPONS; index++)
		{
			char const *path = summary->weapons[index].tag != NONE ? tag_get_name(summary->weapons[index].tag) : NULL;

			ae_pgr_weapon_name(path, row->weapon_name[index], sizeof(row->weapon_name[index]));
		}
	}
}

static int team_before(long score_a, long score_b, int team_a, int team_b)
{
	/* the higher score first; a tie keeps red first */
	return score_a > score_b || (score_a == score_b && team_a < team_b);
}

void ae_glue_pgcr_collect(struct ae_pgr_report *report)
{
	static struct ae_scoreboard_stat ranked[MAXIMUM_RANKED];
	static int teams_of[MAXIMUM_RANKED];
	struct ae_postgame_facts facts;
	struct game_variant *variant = game_engine_get_variant();
	long count, index, rows_total = 0;
	int clock, order[AE_PGR_TEAMS], group;
	enum ae_pgr_mode mode = AE_PGR_MODE_OTHER;

	memset(report, 0, sizeof(*report));
	if (!game_engine_running() || !variant)
		return;
	/* (HALO_AE_PGCR_DEMO: a made-up report in the real post-game's place, to look at the screen) */
	if (ae_pgr_demo_report(report, getenv("HALO_AE_PGCR_DEMO")))
		return;
	ae_glue_postgame_facts(&facts);
	report->valid = 1;
	report->teams = facts.teams;
	switch (variant->game_engine_index)
	{
	case game_engine_slayer: mode = AE_PGR_MODE_SLAYER; break;
	case game_engine_ctf: mode = AE_PGR_MODE_FLAG; break;
	case game_engine_oddball: mode = AE_PGR_MODE_BALL; break;
	case game_engine_king: mode = AE_PGR_MODE_HILL; break;
	case game_engine_race: mode = AE_PGR_MODE_RACE; break;
	case game_engine_bomb: mode = AE_PGR_MODE_BOMB; break;
	}
	report->mode = mode;
	clock = mode == AE_PGR_MODE_BALL || mode == AE_PGR_MODE_HILL;
	snprintf(report->banner, sizeof(report->banner), "%s", facts.banner);
	snprintf(report->banner_score, sizeof(report->banner_score), "%s", facts.banner_score);
	report->banner_color = facts.banner_color;
	report->banner_text_color = facts.banner_text_color;
	report->banner_stripe = facts.banner_stripe;
	snprintf(report->gametype, sizeof(report->gametype), "%s", facts.gametype);
	snprintf(report->map, sizeof(report->map), "%s", facts.map);
	snprintf(report->played, sizeof(report->played), "%s", facts.played);
	snprintf(report->score_label, sizeof(report->score_label), "%s", mode == AE_PGR_MODE_BALL ? "BALL TIME" :
		mode == AE_PGR_MODE_HILL ? "HILL TIME" : "SCORE");
	if (mode == AE_PGR_MODE_FLAG)
	{
		snprintf(report->objective_label[0], AE_PGR_WORD, "FLAG GRABS");
		snprintf(report->objective_label[1], AE_PGR_WORD, "FLAG RETURNS");
		snprintf(report->objective_label[2], AE_PGR_WORD, "FLAG CAPTURES");
	}
	else if (mode == AE_PGR_MODE_BOMB)
	{
		snprintf(report->objective_label[0], AE_PGR_WORD, "BOMB PICKUPS");
		snprintf(report->objective_label[2], AE_PGR_WORD, "BOMB SCORES");
	}
	count = populate_statistic_buffer(ranked, RANKING, FALSE);
	if (count > MAXIMUM_RANKED)
		count = MAXIMUM_RANKED;
	if (count < 0)
		count = 0;
	for (index = 0; index < count; index++)
	{
		struct player_datum *player = player_try_and_get(ranked[index].player_index);

		teams_of[index] = report->teams && player ? (int)PIN(player->team_index, 0, 1) : 0;
	}
	if (report->teams)
	{
		long score0 = game_engine_get_team_score(0), score1 = game_engine_get_team_score(1);

		order[0] = team_before(score0, score1, 0, 1) ? 0 : 1;
		order[1] = 1 - order[0];
		for (group = 0; group < AE_PGR_TEAMS; group++)
		{
			struct ae_pgr_team *team = &report->team[group];
			int team_index = order[group];
			long rank = 0, team_score = game_engine_get_team_score(team_index);

			team->index = team_index;
			team->first = (int)rows_total;
			team->color = team_index == 0 ? TEAM_RED : TEAM_BLUE;
			snprintf(team->name, sizeof(team->name), "%s TEAM", team_index == 0 ? "RED" : "BLUE");
			score_text(team_score, clock, team->score, sizeof(team->score));
			team->score_value = team_score;
			for (index = 0; index < count; index++)
			{
				struct ae_pgr_row *row;

				if (teams_of[index] != team_index)
					continue;
				row = &report->row[rows_total];
				fill_row(row, &ranked[index], 1, clock, mode, rank++);
				row->team = (short)group;
				team->kills += row->kills;
				team->deaths += row->deaths;
				team->assists += row->assists;
				rows_total++;
			}
			team->count = (int)(rows_total - team->first);
		}
	}
	else
	{
		for (index = 0; index < count; index++)
			fill_row(&report->row[rows_total++], &ranked[index], 0, clock, mode, index);
	}
	report->rows = (int)rows_total;
	for (index = 0; index < rows_total; index++)
	{
		if (report->row[index].summary.valid)
			report->have_summaries = 1;
		if (report->row[index].me && !report->teams)
			snprintf(report->you, sizeof(report->you), "YOU: %s", report->row[index].place);
	}
	ae_pgr_decide_winners(report);
	/* (a round game: the teams' rounds won) */
	if (report->teams && ae_bomb_snd_shown())
	{
		report->rounds = 1;
		report->rounds_won[0] = game_engine_get_team_score(report->team[0].index);
		report->rounds_won[1] = game_engine_get_team_score(report->team[1].index);
	}
}

struct ae_pgr_report const *ae_glue_pgcr_kept(void)
{
	return kept.valid ? &kept : NULL;
}

void ae_glue_pgcr_keep(struct ae_pgr_report const *report)
{
	if (report && report->valid && report->rows > 0)
	{
		if (!kept.valid)
		{
			platform_log("ae pgcr: the report is kept for the lobby (%d players)", report->rows);
			first_kept_ms = system_milliseconds();
		}
		kept = *report;
		/* (the service record: this game once, with the summaries when they came) */
		if (!recorded && !report->demo &&
			(report->have_summaries || system_milliseconds() - first_kept_ms >= RECORD_WAIT_MS))
		{
			recorded = 1;
			ae_record_store_game(report);
		}
	}
}

void ae_glue_pgcr_forget(void)
{
	memset(&kept, 0, sizeof(kept));
	recorded = 0;
}
