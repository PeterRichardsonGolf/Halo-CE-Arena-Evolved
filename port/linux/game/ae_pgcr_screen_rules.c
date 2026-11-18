/* The post-game report screen's decisions (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_medal_pack_rules.h"
#include "ae_medals.h"
#include "ae_pgcr_screen_rules.h"

#define DASH "\xE2\x80\x94"

/* ---------- the medals: points and keys from ae_medal_ids.h */

static int const medal_points[AE_MEDAL_COUNT] = {
#define AE_MEDAL(name, key, points) points,
#include "ae_medal_ids.h"
#undef AE_MEDAL
};

static char const *const medal_keys[AE_MEDAL_COUNT] = {
#define AE_MEDAL(name, key, points) key,
#include "ae_medal_ids.h"
#undef AE_MEDAL
};

int ae_pgr_medal_points(int medal)
{
	return medal >= 0 && medal < AE_MEDAL_COUNT ? medal_points[medal] : 0;
}

enum ae_pgr_family ae_pgr_medal_family(int medal)
{
	switch (medal)
	{
	case AE_MEDAL_DOUBLE_KILL:
	case AE_MEDAL_TRIPLE_OF_EACH_KILL:
	case AE_MEDAL_OVERKILL:
	case AE_MEDAL_KILLTACULAR:
	case AE_MEDAL_KILLTROCITY:
	case AE_MEDAL_KILLIMANJARO:
	case AE_MEDAL_KILLTASTROPHE:
	case AE_MEDAL_KILLPOCALYPSE:
	case AE_MEDAL_KILLIONAIRE:
		return AE_PGR_FAMILY_MULTIKILL;
	case AE_MEDAL_KILLING_SPREE:
	case AE_MEDAL_KILLING_FRENZY:
	case AE_MEDAL_RUNNING_RIOT:
	case AE_MEDAL_RAMPAGE:
	case AE_MEDAL_UNTOUCHABLE_ROW_WIT_UNTOUCHABLE:
	case AE_MEDAL_INVINCIBLE:
	case AE_MEDAL_SNIPER_RIFLE_W_SPREE:
	case AE_MEDAL_SHARPSHOOTER_RIFLE_W_SHARPSHOOTER:
	case AE_MEDAL_SHOTGUN_SHOTGUN_SPREE:
	case AE_MEDAL_OPEN_SEASON:
	case AE_MEDAL_SPLATTER_ROW_WIT_SPREE:
	case AE_MEDAL_VEHICLE_ROW_WIT_MANSLAUGHTER:
	case AE_MEDAL_STEAKTACULAR:
	case AE_MEDAL_JUGGERNAUT_SPREE:
	case AE_MEDAL_INCONCEIVABLE:
	case AE_MEDAL_UNFRIGGENBELIEVABLE_ROW_WIT_UNFRIGGENBELIEVABLE:
		return AE_PGR_FAMILY_SPREE;
	case AE_MEDAL_FLAG_CARRIER_KILL:
	case AE_MEDAL_FLAG_TAKEN:
	case AE_MEDAL_FLAG_RETURNED:
	case AE_MEDAL_FLAG_CAPTURED:
	case AE_MEDAL_HAIL_TO_THE_KING:
	case AE_MEDAL_BALL_KILL:
	case AE_MEDAL_KILLED_JUGGERNAUT:
	case AE_MEDAL_IMMOVABLE_OBJECT:
	case AE_MEDAL_FLAG_RUNNER:
	case AE_MEDAL_FLAG_CHAMPION:
	case AE_MEDAL_HILL_DOMINANCE:
	case AE_MEDAL_VEHICLE_DOMINANCE:
	case AE_MEDAL_FIRST_POIINT:
	case AE_MEDAL_HILL_DEFENSE:
	case AE_MEDAL_HILL_OFFENSE:
	case AE_MEDAL_BALL_HOLDER:
	case AE_MEDAL_BALL_KEEPER:
	case AE_MEDAL_BALL_MASTER:
	case AE_MEDAL_BALL_HOG:
	case AE_MEDAL_MAGIC_HANDS:
	case AE_MEDAL_BALL_CARRIER_KILL:
	case AE_MEDAL_FIRST_TOUCH:
	case AE_MEDAL_LAST_STRIKE:
	case AE_MEDAL_FLAG_KILL:
	case AE_MEDAL_SNEAK_KING:
	case AE_MEDAL_FUMBLE:
	case AE_MEDAL_FLAG_JOUST:
	case AE_MEDAL_GOAL_LINE_STAND:
	case AE_MEDAL_STOPPED_SHORT:
	case AE_MEDAL_STRAIGHT_BALLING:
	case AE_MEDAL_HILL_GUARDIAN:
		return AE_PGR_FAMILY_OBJECTIVE;
	}
	return AE_PGR_FAMILY_SKILL;
}

unsigned int ae_pgr_family_color(enum ae_pgr_family family)
{
	switch (family)
	{
	case AE_PGR_FAMILY_MULTIKILL: return 0x5FC8ECFFu;
	case AE_PGR_FAMILY_SPREE: return 0xB298FFFFu;
	case AE_PGR_FAMILY_SKILL: return 0x84D6A0FFu;
	default: return 0xB8D6FFFFu;
	}
}

int ae_pgr_medal_name(int medal, char *out, size_t size)
{
	if (size)
		out[0] = 0;
	if (medal < 0 || medal >= AE_MEDAL_COUNT || !size)
		return 0;
	return ae_medal_name_from_key(medal_keys[medal], out, size, 1);
}

int ae_pgr_medals_sorted(struct ae_pgcr_summary const *summary, int *medals, int *counts)
{
	int count = 0, index, at;

	if (!summary || !summary->valid)
		return 0;
	for (index = 0; index < summary->medal_count && index < AE_PGCR_SUMMARY_MEDALS; index++)
	{
		int id = summary->medals[index].id, n = summary->medals[index].count;

		if (id >= AE_MEDAL_COUNT || n <= 0)
			continue;
		/* an insertion by points (more first), then the lower number first */
		for (at = count; at > 0; at--)
		{
			int other = medals[at - 1];

			if (medal_points[other] > medal_points[id] || (medal_points[other] == medal_points[id] && other < id))
				break;
			medals[at] = medals[at - 1];
			counts[at] = counts[at - 1];
		}
		medals[at] = id;
		counts[at] = n;
		count++;
	}
	return count;
}

int ae_pgr_medal_total(struct ae_pgcr_summary const *summary)
{
	int index, total = 0;

	if (!summary || !summary->valid)
		return 0;
	for (index = 0; index < summary->medal_count && index < AE_PGCR_SUMMARY_MEDALS; index++)
	{
		if (summary->medals[index].id < AE_MEDAL_COUNT)
			total += summary->medals[index].count;
	}
	return total;
}

int ae_pgr_strip_rest(struct ae_pgcr_summary const *summary, int shown_medals)
{
	int medals[AE_PGCR_SUMMARY_MEDALS], counts[AE_PGCR_SUMMARY_MEDALS], count, index, rest;

	count = ae_pgr_medals_sorted(summary, medals, counts);
	rest = ae_pgr_medal_total(summary);
	for (index = 0; index < shown_medals && index < count; index++)
		rest -= counts[index];
	return rest > 0 ? rest : 0;
}

/* ---------- the cursor */

int ae_pgr_cursor_step(struct ae_pgr_report const *report, int cursor, int delta)
{
	int next = cursor + delta;

	if (report->rows <= 0)
		return 0;
	if (next < 0)
		next = 0;
	if (next >= report->rows)
		next = report->rows - 1;
	return next;
}

int ae_pgr_row_team(struct ae_pgr_report const *report, int row)
{
	return row >= 0 && row < report->rows && report->row[row].team > 0 ? 1 : 0;
}

int ae_pgr_cursor_jump(struct ae_pgr_report const *report, int cursor, int direction)
{
	if (report->rows <= 0)
		return 0;
	if (report->teams)
	{
		int team = ae_pgr_row_team(report, cursor), other = 1 - team;
		int rank = cursor - report->team[team].first;

		if (!report->team[other].count)
			return cursor;
		if (rank >= report->team[other].count)
			rank = report->team[other].count - 1;
		return report->team[other].first + rank;
	}
	if (report->rows > AE_PGR_PAGE)
		return ae_pgr_cursor_step(report, cursor, direction * AE_PGR_PAGE);
	return cursor;
}

int ae_pgr_cursor_start(struct ae_pgr_report const *report)
{
	int row;

	for (row = 0; row < report->rows; row++)
	{
		if (report->row[row].me)
			return row;
	}
	return 0;
}

/* ---------- words */

void ae_pgr_number_text(long value, char *out, size_t size)
{
	if (value == AE_PGR_UNKNOWN)
		snprintf(out, size, DASH);
	else
		snprintf(out, size, "%ld", value);
}

void ae_pgr_kd_text(long kills, long deaths, char *out, size_t size)
{
	if (kills < 0 || deaths < 0)
		snprintf(out, size, DASH);
	else if (deaths == 0)
		snprintf(out, size, "%ld.00", kills);
	else
	{
		/* (rounded to hundredths in integers, so every machine reads the same) */
		long hundredths = (kills * 1000 / deaths + 5) / 10;

		snprintf(out, size, "%ld.%02ld", hundredths / 100, hundredths % 100);
	}
}

void ae_pgr_accuracy_text(long fired, long hit, char *out, size_t size)
{
	if (fired <= 0 || hit < 0)
		snprintf(out, size, DASH);
	else
	{
		long percent = (hit * 200 + fired) / (2 * fired);

		snprintf(out, size, "%ld%%", percent > 100 ? 100 : percent);
	}
}

void ae_pgr_kills_text(long kills, char *out, size_t size)
{
	snprintf(out, size, "%ld kill%s", kills, kills == 1 ? "" : "s");
}

void ae_pgr_weapon_name(char const *path, char *out, size_t size)
{
	char const *parts[8];
	size_t lengths[8], at = 0;
	int count = 0, index;
	char const *name;

	if (!size)
		return;
	out[0] = 0;
	if (!path || !path[0])
		return;
	while (path[at] && count < 8)
	{
		size_t start = at;

		while (path[at] && path[at] != '\\' && path[at] != '/')
			at++;
		if (at > start)
		{
			parts[count] = path + start;
			lengths[count++] = at - start;
		}
		if (path[at])
			at++;
	}
	if (!count)
		return;
	index = count - 1;
	if (count >= 2 && ((lengths[0] == 7 && !strncmp(parts[0], "weapons", 7)) ||
		(lengths[0] == 8 && !strncmp(parts[0], "vehicles", 8))))
		index = 1;
	name = parts[index];
	for (at = 0; at < lengths[index] && at + 1 < size; at++)
	{
		char c = name[at];

		out[at] = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
	}
	out[at] = 0;
}

int ae_pgr_points_add_up(struct ae_pgr_row const *row, long *kills, long *suicides, long *betrayals)
{
	if (row->kills < 0 || row->suicides < 0 || row->betrayals < 0)
		return 0;
	*kills = row->kills;
	*suicides = row->suicides;
	*betrayals = row->betrayals;
	return row->kills - row->suicides - row->betrayals == row->score_value;
}

/* ---------- the made-up report */

static char const *const demo_names[16] = { "Reaver", "Vale", "Odin", "Kite", "Moss", "Juno", "Pike", "Wren", "Sable", "Thorn",
	"Quill", "Marsh", "Dune", "Ember", "Flint", "Gale" };
static char const *const demo_weapons[6] = { "SNIPER RIFLE", "BATTLE RIFLE", "FRAG GRENADE", "ASSAULT RIFLE", "SHOTGUN", "WARTHOG" };
static int const demo_medal_ids[10] = { AE_MEDAL_KILLTACULAR, AE_MEDAL_TRIPLE_OF_EACH_KILL, AE_MEDAL_OVERKILL,
	AE_MEDAL_KILLING_SPREE, AE_MEDAL_HEADSHOT, AE_MEDAL_DOUBLE_KILL, AE_MEDAL_SPLATTER_ROW_WIT_SPREE, AE_MEDAL_RAMPAGE,
	AE_MEDAL_PERFECT, AE_MEDAL_KILL };

static unsigned int const demo_armour[8] = { 0xC0392BFFu, 0x2E86DEFFu, 0x3FA34DFFu, 0xE6B422FFu, 0x8E44ADFFu, 0xE67E22FFu, 0x48C9B0FFu,
	0xD0D3D4FFu };

static void demo_row(struct ae_pgr_row *row, int index, int teams, int group, int rank, int total)
{
	int medal, kills = 24 - index * 2 - rank, deaths = 9 + index + (index % 3), index_medals = 7 - (rank > 6 ? 6 : rank);

	memset(row, 0, sizeof(*row));
	row->slot = index;
	row->team = (short)(teams ? group : -1);
	row->me = index == 3;
	row->rank = rank;
	row->chip = teams ? (group == 0 ? 0xC0392BFFu : 0x2E86DEFFu) : demo_armour[index % 8];
	snprintf(row->name, sizeof(row->name), "%s", demo_names[index % 16]);
	if (teams)
		snprintf(row->place, sizeof(row->place), "%d", rank + 1);
	else
		snprintf(row->place, sizeof(row->place), "%d%s", rank + 1, rank == 0 ? "ST" : rank == 1 ? "ND" : rank == 2 ? "RD" : "TH");
	row->kills = kills < 0 ? 0 : kills;
	row->deaths = deaths;
	row->assists = 2 + index % 5;
	row->betrayals = index == 7 ? 1 : 0;
	row->suicides = index == 1 ? 1 : 0;
	row->score_value = row->kills - row->suicides - row->betrayals;
	snprintf(row->score, sizeof(row->score), "%ld", row->score_value);
	row->shots_fired = 300 + index * 20;
	row->shots_hit = row->shots_fired * (45 - index) / 100;
	row->objective[0] = row->objective[1] = row->objective[2] = AE_PGR_UNKNOWN;
	row->summary.valid = 1;
	row->summary.slot = index;
	row->summary.best_spree = (unsigned short)(3 + index % 4);
	row->summary.multikills = (unsigned short)(1 + index % 5);
	row->summary.headshots = (unsigned short)(2 + index % 6);
	row->summary.weapon_count = 3;
	for (medal = 0; medal < 3; medal++)
	{
		row->summary.weapons[medal].tag = -1;
		row->summary.weapons[medal].kills = (unsigned short)(8 - medal * 3 - (index % 3));
		snprintf(row->weapon_name[medal], sizeof(row->weapon_name[medal]), "%s", demo_weapons[(index + medal * 2) % 6]);
	}
	row->summary.killed_most = (unsigned char)((index + 1) % total);
	row->summary.killed_most_count = (unsigned char)(3 + index % 3);
	row->summary.killed_by_most = (unsigned char)((index + 3) % total);
	row->summary.killed_by_most_count = (unsigned char)(2 + index % 4);
	for (medal = 0; medal < index_medals && medal < 10; medal++)
	{
		row->summary.medals[row->summary.medal_count].id = (unsigned char)demo_medal_ids[(medal + index) % 10];
		row->summary.medals[row->summary.medal_count++].count = (unsigned char)(1 + (medal * 2 + index) % 4);
	}
}

int ae_pgr_demo_report(struct ae_pgr_report *report, char const *kind)
{
	int teams, count, group, rank, index = 0, per_team;

	if (!kind)
		return 0;
	if (!strncmp(kind, "ffa", 3))
	{
		teams = 0;
		count = atoi(kind + 3);
		per_team = 0;
	}
	else if (kind[0] >= '1' && kind[0] <= '9' && strchr(kind, 'v'))
	{
		teams = 1;
		per_team = atoi(kind);
		count = per_team * 2;
	}
	else
		return 0;
	if (count < 1 || count > AE_PGR_ROWS)
		return 0;
	memset(report, 0, sizeof(*report));
	report->valid = 1;
	report->demo = 1;
	report->teams = teams;
	report->mode = AE_PGR_MODE_SLAYER;
	snprintf(report->score_label, sizeof(report->score_label), "SCORE");
	snprintf(report->gametype, sizeof(report->gametype), "%s", teams ? "AE TEAM SLY" : "AE FFA SLAY");
	snprintf(report->map, sizeof(report->map), "HANG 'EM HIGH");
	snprintf(report->played, sizeof(report->played), "12:04");
	if (teams)
	{
		snprintf(report->banner, sizeof(report->banner), "RED TEAM WINS");
		snprintf(report->banner_score, sizeof(report->banner_score), "50 - 45");
		report->banner_color = 0xC0392BFFu;
		for (group = 0; group < AE_PGR_TEAMS; group++)
		{
			struct ae_pgr_team *team = &report->team[group];

			team->index = group;
			team->first = index;
			team->color = group == 0 ? 0xC0392BFFu : 0x2E86DEFFu;
			snprintf(team->name, sizeof(team->name), "%s TEAM", group == 0 ? "RED" : "BLUE");
			snprintf(team->score, sizeof(team->score), "%d", group == 0 ? 50 : 45);
			team->score_value = group == 0 ? 50 : 45;
			for (rank = 0; rank < per_team; rank++, index++)
			{
				demo_row(&report->row[index], index, 1, group, rank, count);
				team->kills += report->row[index].kills;
				team->deaths += report->row[index].deaths;
				team->assists += report->row[index].assists;
			}
			team->count = per_team;
		}
	}
	else
	{
		snprintf(report->banner, sizeof(report->banner), "%s WINS", "REAVER");
		snprintf(report->banner_score, sizeof(report->banner_score), "25");
		report->banner_color = 0x10161CF2u;
		report->banner_stripe = 0xC0392BFFu;
		for (rank = 0; rank < count; rank++, index++)
			demo_row(&report->row[index], index, 0, 0, rank, count);
		snprintf(report->you, sizeof(report->you), "YOU: 4TH");
	}
	report->banner_text_color = 0xFFFFFFFFu;
	report->rows = count;
	report->have_summaries = 1;
	ae_pgr_decide_winners(report);
	return 1;
}

/* ---------- winners, rivals and the service record */

void ae_pgr_decide_winners(struct ae_pgr_report *report)
{
	int row, first = 0;

	for (row = 0; row < report->rows; row++)
		report->row[row].won = 0;
	if (report->teams)
	{
		long a = report->team[0].score_value, b = report->team[1].score_value;

		if (!report->team[0].count || (report->team[1].count && a <= b))
			return;
		for (row = report->team[0].first; row < report->team[0].first + report->team[0].count; row++)
			report->row[row].won = 1;
		return;
	}
	for (row = 0; row < report->rows; row++)
		first += report->row[row].rank == 0;
	if (first != 1)
		return;
	for (row = 0; row < report->rows; row++)
		report->row[row].won = report->row[row].rank == 0;
}

char const *ae_pgr_owner_name(struct ae_pgr_report const *report)
{
	int row;

	for (row = 0; row < report->rows; row++)
	{
		if (report->row[row].me)
			return report->row[row].name;
	}
	return "";
}

int ae_pgr_mark_rivals(struct ae_pgr_report *report, int (*is_rival)(void *context, char const *owner, char const *name),
	void *context)
{
	char const *owner = ae_pgr_owner_name(report);
	int row, count = 0;

	for (row = 0; row < report->rows; row++)
	{
		report->row[row].rival = 0;
		if (is_rival && owner[0] && !report->row[row].me && is_rival(context, owner, report->row[row].name))
		{
			report->row[row].rival = 1;
			count++;
		}
	}
	return count;
}

int ae_pgr_to_record_game(struct ae_pgr_report const *report, struct ae_sr_game *game)
{
	int row, count = 0;

	memset(game, 0, sizeof(*game));
	if (!report->valid || report->demo || report->rows <= 0)
		return 0;
	game->mode = (int)report->mode;
	for (row = 0; row < report->rows && count < AE_PGCR_PLAYERS; row++)
	{
		struct ae_pgr_row const *in = &report->row[row];
		struct ae_sr_game_player *out;
		int index;

		/* (a bot is not a player: nothing is kept of it) */
		if (in->bot)
			continue;
		out = &game->player[count++];

		snprintf(out->name, sizeof(out->name), "%s", in->name);
		out->slot = in->slot;
		out->me = in->me;
		out->won = in->won;
		out->quit = in->quit;
		out->kills = in->kills;
		out->deaths = in->deaths;
		out->assists = in->assists;
		if (!in->summary.valid)
			continue;
		out->medal_count = in->summary.medal_count;
		for (index = 0; index < in->summary.medal_count && index < AE_PGCR_SUMMARY_MEDALS; index++)
			out->medals[index] = in->summary.medals[index];
		out->versus_count = in->summary.versus_count;
		for (index = 0; index < in->summary.versus_count && index < AE_PGCR_SUMMARY_VERSUS; index++)
			out->versus[index] = in->summary.versus[index];
	}
	game->count = count;
	return count > 0;
}
