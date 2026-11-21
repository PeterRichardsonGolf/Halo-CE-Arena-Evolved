/*
AE_CAMPAIGN_SCORE_RULES.C

The campaign's score's rules: see ae_campaign_score_rules.h. Derived from AGPL-3.0-or-later code (README credits).
*/

#include <string.h>

#include "ae_campaign_score_rules.h"
#include "campaign_score_values.h"

#define FILE_MAGIC 0x53434541UL /* "AECS" */
#define FILE_VERSION 1UL

void ae_campaign_score_lower(char const *name, char *out, long size)
{
	long index = 0;

	if (size <= 0)
		return;
	for (; name && index < size - 1 && name[index]; index++)
		out[index] = (char)((name[index] >= 'A' && name[index] <= 'Z') ? name[index] + 32 : name[index]);
	out[index] = 0;
}

static int name_has(char const *lower, char const *const *words)
{
	for (; words && *words; words++)
	{
		if (strstr(lower, *words))
			return 1;
	}
	return 0;
}

long ae_campaign_score_match_points(struct campaign_score_match_value const *values, short count, char const *lower)
{
	short index;

	for (index = 0; values && index < count; index++)
	{
		if (name_has(lower, values[index].words))
			return values[index].points;
	}
	return 0;
}

long ae_campaign_score_actor_points(struct campaign_score_actor_value const *values, short count, short actor_type,
	char const *lower)
{
	short index;

	for (index = 0; values && index < count; index++)
	{
		long points;

		if (values[index].actor_type != actor_type)
			continue;
		points = ae_campaign_score_match_points(values[index].variants, values[index].variant_count, lower);
		return points ? points : values[index].points;
	}
	return 0;
}

float ae_campaign_score_style_multiplier(struct campaign_score_values const *values, unsigned char style)
{
	float const multipliers[AE_CAMPAIGN_SCORE_STYLE_COUNT] =
	{
		values->style_one_shot_one_kill, values->style_headshot, values->style_melee, values->style_backsmack,
	};
	int highest = values->style_combine == _campaign_score_combine_highest;
	int any = 0, bit;
	float multiplier = 1.0f;

	for (bit = 0; bit < AE_CAMPAIGN_SCORE_STYLE_COUNT; bit++)
	{
		if (!(style & (1 << bit)))
			continue;
		if (highest)
			multiplier = any ? (multiplier > multipliers[bit] ? multiplier : multipliers[bit]) : multipliers[bit];
		else
			multiplier *= multipliers[bit];
		any = 1;
	}
	return any ? multiplier : values->style_regular;
}

static int pin_difficulty(int difficulty)
{
	return difficulty < 0 ? 0 : difficulty >= AE_CAMPAIGN_SCORE_DIFFICULTIES ? AE_CAMPAIGN_SCORE_DIFFICULTIES - 1 : difficulty;
}

long ae_campaign_score_kill_points(struct campaign_score_values const *values, long base, unsigned char style, int difficulty,
	float skulls_multiplier)
{
	if (base <= 0)
		return 0;
	return (long)((float)base * ae_campaign_score_style_multiplier(values, style) *
		values->difficulty[pin_difficulty(difficulty)] * skulls_multiplier + 0.5f);
}

long ae_campaign_score_medal_points(struct campaign_score_values const *values, char const *medal_key, int difficulty,
	float skulls_multiplier)
{
	float points = 0.0f;
	short index;

	for (index = 0; medal_key && index < values->medal_count; index++)
	{
		if (!strcmp(values->medals[index].medal, medal_key))
		{
			points = (float)values->medals[index].points;
			break;
		}
	}
	if (values->medal_scaled)
		points *= values->difficulty[pin_difficulty(difficulty)] * skulls_multiplier;
	return (long)(points + 0.5f);
}

long ae_campaign_score_unknown_points(int hostile)
{
	return hostile ? AE_CAMPAIGN_SCORE_UNKNOWN_POINTS : 0;
}

long ae_campaign_score_revert_points(struct campaign_score_values const *values, int revert_cost_on)
{
	return revert_cost_on ? values->penalty_checkpoint_revert : 0;
}

long ae_campaign_score_par_seconds_of_level(struct campaign_score_values const *values, char const *level_name, int difficulty)
{
	char lower[128];
	char const *last;
	long index;

	ae_campaign_score_lower(level_name, lower, sizeof(lower));
	last = lower;
	for (index = 0; lower[index]; index++)
	{
		if (lower[index] == '\\' || lower[index] == '/')
			last = lower + index + 1;
	}
	for (index = 0; values->par_times && index < values->par_time_count; index++)
	{
		if (!strcmp(values->par_times[index].level, last))
			return (long)((float)values->par_times[index].seconds * values->par_difficulty[pin_difficulty(difficulty)] + 0.5f);
	}
	return -1;
}

float ae_campaign_score_par_fraction_of(long par_seconds, long ticks)
{
	if (par_seconds <= 0)
		return -1.0f;
	return (float)ticks / (float)(par_seconds * AE_CAMPAIGN_SCORE_TICKS_PER_SECOND);
}

float ae_campaign_score_time_multiplier_of(struct campaign_score_values const *values, long par_seconds, long ticks)
{
	float ratio = ae_campaign_score_par_fraction_of(par_seconds, ticks);
	short index;

	if (ratio < 0.0f)
		return 1.0f;
	for (index = 0; values->time_brackets && index < values->time_bracket_count; index++)
	{
		if (values->time_brackets[index].up_to_par < 0.0f || ratio <= values->time_brackets[index].up_to_par)
			return values->time_brackets[index].multiplier;
	}
	return 1.0f;
}

long ae_campaign_score_final(long points, float time_multiplier)
{
	return points > 0 ? (long)((float)points * time_multiplier + 0.5f) : points;
}

/* ---------- the log */

void ae_campaign_score_log_reset(struct ae_campaign_score_log *log)
{
	log->count = 0;
	log->ticks = 0;
}

int ae_campaign_score_log_append(struct ae_campaign_score_log *log, long game_time, int kind, long variant_index, int slot,
	long points, unsigned char style)
{
	struct ae_campaign_score_entry *entry;

	if (log->count >= AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES || slot < 0 || slot >= AE_CAMPAIGN_SCORE_PLAYERS || kind < 0 ||
		kind >= AE_CAMPAIGN_SCORE_ENTRY_COUNT)
		return 0;
	entry = &log->entries[log->count++];
	entry->game_time = game_time;
	entry->variant_index = variant_index;
	entry->points = points;
	entry->player_slot = (short)slot;
	entry->kind = (unsigned char)kind;
	entry->style = style;
	return 1;
}

long ae_campaign_score_log_total(struct ae_campaign_score_log const *log, int slot)
{
	long total = 0, index;

	for (index = 0; index < log->count; index++)
	{
		if (log->entries[index].player_slot == slot)
			total += log->entries[index].points;
	}
	return total;
}

long ae_campaign_score_log_team_total(struct ae_campaign_score_log const *log)
{
	long total = 0, index;

	for (index = 0; index < log->count; index++)
		total += log->entries[index].points;
	return total;
}

long ae_campaign_score_log_points(struct ae_campaign_score_log const *log, int slot, int competitive)
{
	return competitive && slot >= 0 ? ae_campaign_score_log_total(log, slot) : ae_campaign_score_log_team_total(log);
}

long ae_campaign_score_log_count(struct ae_campaign_score_log const *log, int slot, int kind)
{
	long total = 0, index;

	for (index = 0; index < log->count; index++)
	{
		if ((slot < 0 || log->entries[index].player_slot == slot) && log->entries[index].kind == kind)
			total++;
	}
	return total;
}

long ae_campaign_score_log_style_count(struct ae_campaign_score_log const *log, int slot, int style_bit)
{
	long total = 0, index;

	for (index = 0; index < log->count; index++)
	{
		if ((slot < 0 || log->entries[index].player_slot == slot) && log->entries[index].kind == AE_CAMPAIGN_SCORE_ENTRY_KILL &&
			(log->entries[index].style & (1 << style_bit)))
			total++;
	}
	return total;
}

long ae_campaign_score_par_ticks_of(long par_seconds)
{
	return par_seconds > 0 ? par_seconds * AE_CAMPAIGN_SCORE_TICKS_PER_SECOND : -1;
}

int ae_campaign_score_next_drop_of(struct campaign_score_values const *values, long par_seconds, long ticks, long *drop_tick,
	float *next_multiplier)
{
	float ratio = ae_campaign_score_par_fraction_of(par_seconds, ticks);
	long par_ticks = ae_campaign_score_par_ticks_of(par_seconds);
	short index;

	if (ratio < 0.0f)
		return 0;
	for (index = 0; values->time_brackets && index < values->time_bracket_count; index++)
	{
		float limit = values->time_brackets[index].up_to_par;

		if (limit < 0.0f || ratio <= limit)
		{
			long tick;

			if (limit < 0.0f || index + 1 >= values->time_bracket_count)
				return 0;
			/* (the multiplier holds while the time over the par time is at most the limit: it drops on the first tick past it) */
			tick = (long)((float)par_ticks * limit);
			while ((float)tick / (float)par_ticks <= limit)
				tick++;
			while (tick > 0 && (float)(tick - 1) / (float)par_ticks > limit)
				tick--;
			*drop_tick = tick;
			*next_multiplier = values->time_brackets[index + 1].multiplier;
			return 1;
		}
	}
	return 0;
}

int ae_campaign_score_place(struct ae_campaign_score_log const *log, int slot)
{
	long mine, index;
	int place = 1, other, seen[AE_CAMPAIGN_SCORE_PLAYERS] = { 0 };

	if (slot < 0 || slot >= AE_CAMPAIGN_SCORE_PLAYERS)
		return 0;
	for (index = 0; index < log->count; index++)
		seen[log->entries[index].player_slot] = 1;
	if (!seen[slot])
		return 0;
	mine = ae_campaign_score_log_total(log, slot);
	for (other = 0; other < AE_CAMPAIGN_SCORE_PLAYERS; other++)
	{
		if (other != slot && seen[other] && ae_campaign_score_log_total(log, other) > mine)
			place++;
	}
	return place;
}

void ae_campaign_score_summarize(struct ae_campaign_score_log const *log, int slot, struct ae_campaign_score_summary *summary)
{
	long index;

	memset(summary, 0, sizeof(*summary));
	for (index = 0; index < log->count; index++)
	{
		struct ae_campaign_score_entry const *entry = &log->entries[index];

		if (slot >= 0 && entry->player_slot != slot)
			continue;
		summary->points += entry->points;
		switch (entry->kind)
		{
		case AE_CAMPAIGN_SCORE_ENTRY_KILL:
			summary->kills++;
			summary->kill_points += entry->points;
			summary->headshots += (entry->style >> AE_CAMPAIGN_SCORE_STYLE_HEADSHOT) & 1;
			summary->melee_kills += (entry->style >> AE_CAMPAIGN_SCORE_STYLE_MELEE) & 1;
			summary->backsmack_kills += (entry->style >> AE_CAMPAIGN_SCORE_STYLE_BACKSMACK) & 1;
			summary->one_shot_kills += (entry->style >> AE_CAMPAIGN_SCORE_STYLE_ONE_SHOT) & 1;
			break;
		case AE_CAMPAIGN_SCORE_ENTRY_MEDAL:
			summary->medals++;
			summary->medal_points += entry->points;
			break;
		case AE_CAMPAIGN_SCORE_ENTRY_DEATH:
			summary->deaths++;
			summary->death_points += entry->points;
			break;
		case AE_CAMPAIGN_SCORE_ENTRY_REVERT:
			summary->reverts++;
			summary->revert_points += entry->points;
			break;
		case AE_CAMPAIGN_SCORE_ENTRY_BETRAYAL:
			summary->betrayals++;
			summary->betrayal_points += entry->points;
			break;
		case AE_CAMPAIGN_SCORE_ENTRY_ALLY:
			summary->allies_killed++;
			summary->ally_points += entry->points;
			break;
		default:
			break;
		}
		if (entry->points < 0 && entry->kind != AE_CAMPAIGN_SCORE_ENTRY_KILL && entry->kind != AE_CAMPAIGN_SCORE_ENTRY_MEDAL)
		{
			summary->penalty_points += entry->points;
			summary->penalty_count++;
		}
	}
}

int ae_campaign_score_type_competitive(char const *type)
{
	return type && !strcmp(type, "competitive");
}

/* ---------- the score file */

unsigned long ae_campaign_score_name_hash(char const *name)
{
	unsigned long hash = 2166136261UL;

	for (; name && *name; name++)
	{
		char c = *name;

		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		if (c == '/')
			c = '\\';
		hash = ((hash ^ (unsigned char)c) * 16777619UL) & 0xFFFFFFFFUL;
	}
	return hash;
}

static unsigned long crc32(unsigned char const *bytes, long size)
{
	unsigned long crc = 0xFFFFFFFFUL;
	long index;
	int bit;

	for (index = 0; index < size; index++)
	{
		crc ^= bytes[index];
		for (bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
	}
	return crc ^ 0xFFFFFFFFUL;
}

static void put32(unsigned char *at, unsigned long value)
{
	at[0] = (unsigned char)(value & 255);
	at[1] = (unsigned char)((value >> 8) & 255);
	at[2] = (unsigned char)((value >> 16) & 255);
	at[3] = (unsigned char)((value >> 24) & 255);
}

static unsigned long get32(unsigned char const *at)
{
	return (unsigned long)at[0] | ((unsigned long)at[1] << 8) | ((unsigned long)at[2] << 16) | ((unsigned long)at[3] << 24);
}

static long get_signed(unsigned char const *at)
{
	unsigned long value = get32(at);

	return value & 0x80000000UL ? -(long)((~value) & 0x7FFFFFFFUL) - 1 : (long)value;
}

long ae_campaign_score_file_encode(struct ae_campaign_score_log const *log, unsigned long binding, unsigned long level_hash,
	int competitive, unsigned char *bytes, long size)
{
	long total = AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + log->count * AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE +
		AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE;
	long index;

	if (size < total || log->count < 0 || log->count > AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES)
		return 0;
	memset(bytes, 0, (size_t)total);
	put32(bytes, FILE_MAGIC);
	put32(bytes + 4, FILE_VERSION);
	put32(bytes + 8, binding);
	put32(bytes + 12, level_hash);
	put32(bytes + 16, (unsigned long)log->ticks);
	put32(bytes + 20, (unsigned long)log->count);
	put32(bytes + 24, competitive ? 1UL : 0UL);
	for (index = 0; index < log->count; index++)
	{
		unsigned char *at = bytes + AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + index * AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE;
		struct ae_campaign_score_entry const *entry = &log->entries[index];

		put32(at, (unsigned long)entry->game_time);
		put32(at + 4, (unsigned long)entry->variant_index);
		put32(at + 8, (unsigned long)entry->points);
		at[12] = (unsigned char)(entry->player_slot & 255);
		at[13] = (unsigned char)((entry->player_slot >> 8) & 255);
		at[14] = entry->kind;
		at[15] = entry->style;
	}
	put32(bytes + total - AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE, crc32(bytes, total - AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE));
	return total;
}

int ae_campaign_score_file_decode(unsigned char const *bytes, long size, unsigned long binding, unsigned long level_hash,
	struct ae_campaign_score_log *log, int *competitive, char const **why)
{
	char const *reason = NULL;
	unsigned long count;
	long index;

	ae_campaign_score_log_reset(log);
	*competitive = 0;
	if (size > AE_CAMPAIGN_SCORE_FILE_MAXIMUM_SIZE)
		reason = "it is bigger than a score file can be";
	else if (size < AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE)
		reason = "it is too short";
	else if (get32(bytes) != FILE_MAGIC)
		reason = "it is not a score file";
	else if (get32(bytes + 4) != FILE_VERSION)
		reason = "it is of another version";
	else if (get32(bytes + 8) != binding)
		reason = "it is of another saved game";
	else if (get32(bytes + 12) != level_hash)
		reason = "it is of another level";
	else
	{
		count = get32(bytes + 20);
		if (count > (unsigned long)AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES)
			reason = "it holds too many entries";
		else if ((long)(AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + count * AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE +
			AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE) != size)
			reason = "its size is not its entries'";
		else if (crc32(bytes, size - AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE) != get32(bytes + size - AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE))
			reason = "it is damaged";
		else
		{
			for (index = 0; index < (long)count && !reason; index++)
			{
				unsigned char const *at = bytes + AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + index * AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE;
				int slot = at[12] | (at[13] << 8);

				if (slot >= AE_CAMPAIGN_SCORE_PLAYERS || at[14] >= AE_CAMPAIGN_SCORE_ENTRY_COUNT)
					reason = "an entry is out of range";
			}
			if (!reason)
			{
				for (index = 0; index < (long)count; index++)
				{
					unsigned char const *at = bytes + AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + index * AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE;
					struct ae_campaign_score_entry *entry = &log->entries[index];

					entry->game_time = get_signed(at);
					entry->variant_index = get_signed(at + 4);
					entry->points = get_signed(at + 8);
					entry->player_slot = (short)(at[12] | (at[13] << 8));
					entry->kind = at[14];
					entry->style = at[15];
				}
				log->count = (long)count;
				log->ticks = get_signed(bytes + 16);
				*competitive = get32(bytes + 24) == 1UL;
			}
		}
	}
	if (reason)
	{
		ae_campaign_score_log_reset(log);
		*why = reason;
		return 1;
	}
	*why = "";
	return 0;
}
