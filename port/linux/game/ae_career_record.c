/* The career record (see the header). Pure code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_career_record.h"
#include "ae_career_xp_rules.h"
#include "ae_social_rules.h"
#include "ae_social_match_rules.h"

static char const *const ladder_key[2] = {"career", "online"};

void ae_career_init(struct ae_career *career)
{
	memset(career, 0, sizeof(*career));
}

void ae_career_id(char const *playlist, char *id, size_t size)
{
	size_t at = 0;

	if (!size)
		return;
	for (; playlist && *playlist && at + 1 < size && at + 1 < AE_CAREER_ID; playlist++)
	{
		unsigned char c = (unsigned char)*playlist;

		id[at++] = (c == '|' || c < 32 || c == 127) ? '?' : (char)c;
	}
	id[at] = 0;
}

struct ae_career_entry *ae_career_find(struct ae_career *career, int ladder, char const *playlist)
{
	char id[AE_CAREER_ID];
	int index;

	if (ladder < 0 || ladder > 1)
		return NULL;
	ae_career_id(playlist, id, sizeof(id));
	for (index = 0; index < career->count[ladder]; index++)
		if (!strcmp(career->entry[ladder][index].playlist, id))
			return &career->entry[ladder][index];
	return NULL;
}

struct ae_career_entry *ae_career_entry(struct ae_career *career, int ladder, char const *playlist)
{
	struct ae_career_entry *entry = ae_career_find(career, ladder, playlist);

	if (entry)
		return entry;
	if (ladder < 0 || ladder > 1 || career->count[ladder] >= AE_CAREER_PLAYLISTS)
		return NULL;
	entry = &career->entry[ladder][career->count[ladder]++];
	memset(entry, 0, sizeof(*entry));
	ae_career_id(playlist, entry->playlist, sizeof(entry->playlist));
	return entry;
}

static void note_quit(struct ae_career_entry *entry, unsigned long clock)
{
	entry->quits++;
	if (entry->quit_stamps >= AE_CAREER_QUIT_HISTORY)
	{
		memmove(entry->quit_stamp, entry->quit_stamp + 1, sizeof(entry->quit_stamp[0]) * (AE_CAREER_QUIT_HISTORY - 1));
		entry->quit_stamps = AE_CAREER_QUIT_HISTORY - 1;
	}
	entry->quit_stamp[entry->quit_stamps++] = clock;
}

int ae_career_record_game(struct ae_career *career, int ladder, char const *playlist, long change, int won, unsigned long clock)
{
	struct ae_career_entry *entry = ae_career_entry(career, ladder, playlist);

	if (!entry || !entry->playlist[0])
		return 0;
	ae_rating_apply(&entry->rating, change);
	entry->games++;
	if (won)
		entry->wins++;
	entry->last = clock;
	return 1;
}

int ae_career_record_quit(struct ae_career *career, int ladder, char const *playlist, long change, unsigned long clock)
{
	struct ae_career_entry *entry = ae_career_entry(career, ladder, playlist);

	if (!entry || !entry->playlist[0])
		return 0;
	ae_rating_apply(&entry->rating, change);
	entry->games++;
	note_quit(entry, clock);
	entry->last = clock;
	return 1;
}

int ae_career_record_result(struct ae_career *career, int ladder, struct ae_rating_result const *result, int self,
	unsigned long clock)
{
	struct ae_career_entry *entry;
	long change;

	if (!result || self < 0 || self >= result->count || !result->player[self].rated)
		return 0;
	entry = ae_career_entry(career, ladder, result->playlist);
	if (!entry || !entry->playlist[0])
		return 0;
	change = ae_rating_result_change(result, self, entry->rating.points);
	ae_rating_apply(&entry->rating, change);
	entry->games++;
	if (result->player[self].place == 0 && !(result->player[self].quit && !result->player[self].rejoined))
		entry->wins++;
	if (ae_rating_result_counts_quit(result, self))
		note_quit(entry, clock);
	entry->last = clock;
	return 1;
}

int ae_career_overall_level(struct ae_career const *career, int ladder)
{
	int index, best = 0;

	if (ladder < 0 || ladder > 1)
		return 0;
	for (index = 0; index < career->count[ladder]; index++)
		if (career->entry[ladder][index].rating.highest > best)
			best = career->entry[ladder][index].rating.highest;
	return best;
}

/* ---- the social lines (version 2) ---- */

struct ae_career_match *ae_career_match_find(struct ae_career *career, char const *profile, char const *playlist)
{
	char key[AE_CAREER_ID], id[AE_CAREER_ID];
	int index;

	ae_career_id(profile, key, sizeof(key));
	ae_career_id(playlist, id, sizeof(id));
	for (index = 0; index < career->match_count; index++)
		if (!strcmp(career->match[index].profile, key) && !strcmp(career->match[index].playlist, id))
			return &career->match[index];
	return NULL;
}

struct ae_career_match *ae_career_match_entry(struct ae_career *career, char const *profile, char const *playlist)
{
	struct ae_career_match *entry = ae_career_match_find(career, profile, playlist);

	if (entry)
		return entry;
	if (career->match_count >= AE_CAREER_SOCIAL)
		return NULL;
	entry = &career->match[career->match_count++];
	memset(entry, 0, sizeof(*entry));
	ae_career_id(profile, entry->profile, sizeof(entry->profile));
	ae_career_id(playlist, entry->playlist, sizeof(entry->playlist));
	return entry;
}

struct ae_career_social *ae_career_social_find(struct ae_career *career, char const *profile, char const *playlist)
{
	char key[AE_CAREER_ID], id[AE_CAREER_ID];
	int index;

	ae_career_id(profile, key, sizeof(key));
	ae_career_id(playlist, id, sizeof(id));
	for (index = 0; index < career->social_count; index++)
		if (!strcmp(career->social[index].profile, key) && !strcmp(career->social[index].playlist, id))
			return &career->social[index];
	return NULL;
}

struct ae_career_social *ae_career_social_entry(struct ae_career *career, char const *profile, char const *playlist)
{
	struct ae_career_social *entry = ae_career_social_find(career, profile, playlist);

	if (entry)
		return entry;
	if (career->social_count >= AE_CAREER_SOCIAL)
		return NULL;
	entry = &career->social[career->social_count++];
	memset(entry, 0, sizeof(*entry));
	ae_career_id(profile, entry->profile, sizeof(entry->profile));
	ae_career_id(playlist, entry->playlist, sizeof(entry->playlist));
	return entry;
}

long ae_career_social_total_exp(struct ae_career const *career, char const *profile)
{
	char key[AE_CAREER_ID];
	long total = 0;
	int index;

	ae_career_id(profile, key, sizeof(key));
	for (index = 0; index < career->social_count; index++)
		if (!strcmp(career->social[index].profile, key))
			total += career->social[index].exp;
	return total > AE_SOCIAL_OVERALL_EXP_MAX ? AE_SOCIAL_OVERALL_EXP_MAX : total;
}

struct ae_career_pending *ae_career_pending_find(struct ae_career *career, char const *profile)
{
	char key[AE_CAREER_ID];
	int index;

	ae_career_id(profile, key, sizeof(key));
	for (index = 0; index < career->pending_count; index++)
		if (!strcmp(career->pending[index].profile, key))
			return &career->pending[index];
	return NULL;
}

int ae_career_pending_set(struct ae_career *career, char const *profile, char const *playlist, unsigned long clock)
{
	struct ae_career_pending *entry = ae_career_pending_find(career, profile);

	if (!entry)
	{
		if (career->pending_count >= AE_CAREER_PENDING)
			return 0;
		entry = &career->pending[career->pending_count++];
		memset(entry, 0, sizeof(*entry));
		ae_career_id(profile, entry->profile, sizeof(entry->profile));
	}
	ae_career_id(playlist, entry->playlist, sizeof(entry->playlist));
	entry->start = clock;
	return 1;
}

int ae_career_pending_clear(struct ae_career *career, char const *profile)
{
	struct ae_career_pending *entry = ae_career_pending_find(career, profile);
	int index;

	if (!entry)
		return 0;
	index = (int)(entry - career->pending);
	memmove(entry, entry + 1, sizeof(*entry) * (size_t)(career->pending_count - index - 1));
	career->pending_count--;
	return 1;
}

int ae_career_social_has_room(struct ae_career const *career, char const *profile, char const *playlist)
{
	struct ae_career *writable = (struct ae_career *)career;

	return (ae_career_social_find(writable, profile, playlist) || career->social_count < AE_CAREER_SOCIAL) &&
		(ae_career_match_find(writable, profile, playlist) || career->match_count < AE_CAREER_SOCIAL);
}

int ae_career_social_commit(struct ae_career *career, char const *profile, char const *playlist, int outcome, long match_points,
	unsigned long clock)
{
	if (!ae_career_social_has_room(career, profile, playlist))
		return 0;
	return ae_career_social_record(career, profile, playlist, outcome, clock) &&
		ae_career_match_record(career, profile, playlist, match_points, clock);
}

void ae_career_pending_settle(struct ae_career *career, int crashed, unsigned long clock, int *forgiven, int *quits)
{
	int forgave = 0, quit = 0;

	while (career->pending_count > 0)
	{
		struct ae_career_pending game = career->pending[0];

		/* (taken out by its place, not looked up by its profile: a profile a find cleans to another, one read with a
		control character in it, is never found, and the loop never ended) */
		memmove(&career->pending[0], &career->pending[1], sizeof(career->pending[0]) * (size_t)(career->pending_count - 1));
		career->pending_count--;
		if (!crashed)
		{
			struct ae_career_match const *match = ae_career_match_find(career, game.profile, game.playlist);
			long points = match ? match->points : ae_career_match_start(career, game.profile), games = match ? match->games : 0;
			int level = ae_social_match_level(points), result = -1;
			long change = ae_social_match_change(points, games, &level, &result, 1);

			if (ae_career_social_commit(career, game.profile, game.playlist, AE_SOCIAL_QUIT, ae_social_match_apply(points, change),
				clock))
			{
				quit++;
				continue;
			}
		}
		forgave++;
	}
	if (forgiven)
		*forgiven = forgave;
	if (quits)
		*quits = quit;
}

int ae_career_social_record(struct ae_career *career, char const *profile, char const *playlist, int outcome,
	unsigned long clock)
{
	struct ae_career_social *entry = ae_career_social_entry(career, profile, playlist);

	if (!entry || !entry->profile[0] || !entry->playlist[0])
		return 0;
	entry->exp = ae_social_exp_after(entry->exp, outcome);
	entry->games++;
	if (outcome == AE_SOCIAL_WIN)
		entry->wins++;
	else if (outcome == AE_SOCIAL_TIE)
		entry->ties++;
	else if (outcome == AE_SOCIAL_QUIT)
		entry->quits++;
	entry->last = clock;
	ae_career_pending_clear(career, profile);
	return 1;
}

int ae_career_match_record(struct ae_career *career, char const *profile, char const *playlist, long points,
	unsigned long clock)
{
	struct ae_career_match *entry = ae_career_match_entry(career, profile, playlist);

	if (!entry || !entry->profile[0] || !entry->playlist[0])
		return 0;
	entry->points = points < 0 ? 0 : points > 4999 ? 4999 : points;
	entry->games++;
	entry->last = clock;
	return 1;
}

int ae_career_social_reset(struct ae_career *career, char const *profile, char const *playlist)
{
	char key[AE_CAREER_ID], id[AE_CAREER_ID];
	int index, removed = 0;

	ae_career_id(profile, key, sizeof(key));
	if (playlist)
		ae_career_id(playlist, id, sizeof(id));
	for (index = career->match_count - 1; index >= 0; index--)
		if (!strcmp(career->match[index].profile, key) && (!playlist || !strcmp(career->match[index].playlist, id)))
		{
			memmove(&career->match[index], &career->match[index + 1],
				sizeof(career->match[0]) * (size_t)(career->match_count - index - 1));
			career->match_count--;
			removed++;
		}
	for (index = career->social_count - 1; index >= 0; index--)
		if (!strcmp(career->social[index].profile, key) && (!playlist || !strcmp(career->social[index].playlist, id)))
		{
			memmove(&career->social[index], &career->social[index + 1],
				sizeof(career->social[0]) * (size_t)(career->social_count - index - 1));
			career->social_count--;
			removed++;
		}
	if (!playlist || (ae_career_pending_find(career, key) && !strcmp(ae_career_pending_find(career, key)->playlist, id)))
		removed += ae_career_pending_clear(career, key);
	return removed;
}

static struct ae_career_experience *experience_find(struct ae_career const *career, char const *profile)
{
	char key[AE_CAREER_ID];
	int index;

	ae_career_id(profile, key, sizeof(key));
	for (index = 0; index < career->experience_count; index++)
		if (!strcmp(career->experience[index].profile, key))
			return (struct ae_career_experience *)&career->experience[index];
	return NULL;
}

int ae_career_experience_get(struct ae_career const *career, char const *profile)
{
	struct ae_career_experience *entry = experience_find(career, profile);

	return entry ? entry->value : AE_SOCIAL_EXPERIENCE_UNSET;
}

int ae_career_experience_set(struct ae_career *career, char const *profile, int value)
{
	struct ae_career_experience *entry = experience_find(career, profile);

	if (value < AE_SOCIAL_EXPERIENCE_UNSET || value >= NUMBER_OF_AE_SOCIAL_EXPERIENCES)
		return 0;
	if (value == AE_SOCIAL_EXPERIENCE_UNSET)
	{
		if (entry)
		{
			int index = (int)(entry - career->experience);

			memmove(entry, entry + 1, sizeof(*entry) * (size_t)(career->experience_count - index - 1));
			career->experience_count--;
		}
		return 1;
	}
	if (!entry)
	{
		if (career->experience_count >= AE_CAREER_EXPERIENCES)
			return 0;
		entry = &career->experience[career->experience_count++];
		memset(entry, 0, sizeof(*entry));
		ae_career_id(profile, entry->profile, sizeof(entry->profile));
		if (!entry->profile[0])
		{
			career->experience_count--;
			return 0;
		}
	}
	entry->value = value;
	return 1;
}

long ae_career_match_start(struct ae_career const *career, char const *profile)
{
	return ae_social_calibration_start(ae_career_experience_get(career, profile));
}

int ae_career_match_restart(struct ae_career *career, char const *profile, unsigned long clock)
{
	char key[AE_CAREER_ID];
	long start = ae_career_match_start(career, profile);
	int index, moved = 0;

	ae_career_id(profile, key, sizeof(key));
	for (index = 0; index < career->match_count; index++)
		if (!strcmp(career->match[index].profile, key))
		{
			career->match[index].points = start;
			career->match[index].games = 0;
			career->match[index].last = clock;
			moved++;
		}
	return moved;
}

/* ---- career XP and challenges (version 3) ---- */

struct ae_career_xp *ae_career_xp_find(struct ae_career *career, char const *profile)
{
	char key[AE_CAREER_ID];
	int index;

	ae_career_id(profile, key, sizeof(key));
	for (index = 0; index < career->xp_count; index++)
		if (!strcmp(career->xp[index].profile, key))
			return &career->xp[index];
	return NULL;
}

struct ae_career_xp *ae_career_xp_entry(struct ae_career *career, char const *profile)
{
	struct ae_career_xp *entry = ae_career_xp_find(career, profile);

	if (entry)
		return entry;
	if (career->xp_count >= AE_CAREER_XP_PROFILES)
		return NULL;
	entry = &career->xp[career->xp_count++];
	memset(entry, 0, sizeof(*entry));
	ae_career_id(profile, entry->profile, sizeof(entry->profile));
	if (!entry->profile[0])
	{
		career->xp_count--;
		return NULL;
	}
	return entry;
}

long ae_career_xp_get(struct ae_career const *career, char const *profile)
{
	struct ae_career_xp const *entry = ae_career_xp_find((struct ae_career *)career, profile);

	return entry ? entry->xp : 0;
}

struct ae_career_challenges *ae_career_challenges_find(struct ae_career *career, char const *profile)
{
	char key[AE_CAREER_ID];
	int index;

	ae_career_id(profile, key, sizeof(key));
	for (index = 0; index < career->challenge_count; index++)
		if (!strcmp(career->challenges[index].profile, key))
			return &career->challenges[index];
	return NULL;
}

struct ae_career_challenges *ae_career_challenges_entry(struct ae_career *career, char const *profile)
{
	struct ae_career_challenges *entry = ae_career_challenges_find(career, profile);

	if (entry)
		return entry;
	if (career->challenge_count >= AE_CAREER_XP_PROFILES)
		return NULL;
	entry = &career->challenges[career->challenge_count++];
	memset(entry, 0, sizeof(*entry));
	ae_career_id(profile, entry->profile, sizeof(entry->profile));
	if (!entry->profile[0])
	{
		career->challenge_count--;
		return NULL;
	}
	return entry;
}

static unsigned long checksum(char const *text, size_t length)
{
	unsigned long hash = 2166136261u;
	size_t index;

	for (index = 0; index < length; index++)
		hash = ((hash ^ (unsigned char)text[index]) * 16777619u) & 0xffffffffu;
	return hash;
}

long ae_career_write(struct ae_career const *career, char *buffer, long size)
{
	long at = 0;
	int ladder, index;
	int n;

	/* (the XP lines make a version 3 file; a file without them stays version 2, as a 0.9.x reader takes it) */
	n = snprintf(buffer, (size_t)size, "# AE career\nversion = %d\n",
		career->xp_count || career->challenge_count ? AE_CAREER_VERSION_XP : AE_CAREER_VERSION);
	if (n < 0 || n >= size)
		return -1;
	at = n;
	for (ladder = 0; ladder < 2; ladder++)
		for (index = 0; index < career->count[ladder]; index++)
		{
			struct ae_career_entry const *e = &career->entry[ladder][index];

			int stamp;

			n = snprintf(buffer + at, (size_t)(size - at), "%s = %s | %ld %d %d %ld %ld %ld %lu", ladder_key[ladder], e->playlist,
				e->rating.points, e->rating.level, e->rating.highest, e->games, e->wins, e->quits, e->last);
			if (n < 0 || n >= size - at)
				return -1;
			at += n;
			for (stamp = 0; stamp < e->quit_stamps; stamp++)
			{
				n = snprintf(buffer + at, (size_t)(size - at), "%s%lu", stamp ? " " : " | ", e->quit_stamp[stamp]);
				if (n < 0 || n >= size - at)
					return -1;
				at += n;
			}
			n = snprintf(buffer + at, (size_t)(size - at), "\n");
			if (n < 0 || n >= size - at)
				return -1;
			at += n;
		}
	for (index = 0; index < career->match_count; index++)
	{
		struct ae_career_match const *m = &career->match[index];

		n = snprintf(buffer + at, (size_t)(size - at), "match = %s | %s | %ld %ld %lu\n", m->profile, m->playlist, m->points,
			m->games, m->last);
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
	}
	for (index = 0; index < career->social_count; index++)
	{
		struct ae_career_social const *c = &career->social[index];

		n = snprintf(buffer + at, (size_t)(size - at), "social = %s | %s | %ld %ld %ld %ld %ld %lu\n", c->profile, c->playlist,
			c->exp, c->games, c->wins, c->ties, c->quits, c->last);
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
	}
	for (index = 0; index < career->pending_count; index++)
	{
		struct ae_career_pending const *g = &career->pending[index];

		n = snprintf(buffer + at, (size_t)(size - at), "pending = %s | %s | %lu\n", g->profile, g->playlist, g->start);
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
	}
	for (index = 0; index < career->experience_count; index++)
	{
		n = snprintf(buffer + at, (size_t)(size - at), "experience = %s | %d\n", career->experience[index].profile,
			career->experience[index].value);
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
	}
	for (index = 0; index < career->xp_count; index++)
	{
		n = snprintf(buffer + at, (size_t)(size - at), "xp = %s | %ld %lu\n", career->xp[index].profile, career->xp[index].xp,
			career->xp[index].last);
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
	}
	for (index = 0; index < career->challenge_count; index++)
	{
		struct ae_career_challenges const *c = &career->challenges[index];
		int each;

		n = snprintf(buffer + at, (size_t)(size - at), "challenges = %s | %ld %ld", c->profile, c->day, c->week);
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
		for (each = 0; each < AE_CAREER_CHALLENGE_SLOTS; each++)
		{
			n = snprintf(buffer + at, (size_t)(size - at), " | %s %ld %d", c->slot[each].kind[0] ? c->slot[each].kind : "-",
				c->slot[each].progress, c->slot[each].done ? 1 : 0);
			if (n < 0 || n >= size - at)
				return -1;
			at += n;
		}
		n = snprintf(buffer + at, (size_t)(size - at), "\n");
		if (n < 0 || n >= size - at)
			return -1;
		at += n;
	}
	n = snprintf(buffer + at, (size_t)(size - at), "checksum = %08lx\n", checksum(buffer, (size_t)at));
	if (n < 0 || n >= size - at)
		return -1;
	return at + n;
}

static int numbers(char const *text, long *out, int count)
{
	char *end;
	int index;

	for (index = 0; index < count; index++)
	{
		out[index] = strtol(text, &end, 10);
		if (end == text || out[index] < 0)
			return 0;
		text = end;
	}
	return 1;
}

/* a social line's "profile | playlist | numbers": the two names (trimmed, each non-empty and under the id's size) and the rest
   after the second bar; 0 when it is not that */
static int social_split(char *value, char **profile, char **playlist, char **numbers_at)
{
	char *first = strchr(value, '|'), *second, *end;

	if (!first)
		return 0;
	second = strchr(first + 1, '|');
	if (!second)
		return 0;
	*first = 0;
	*second = 0;
	*profile = value;
	*playlist = first + 1;
	while (**playlist == ' ')
		(*playlist)++;
	end = first;
	while (end > *profile && end[-1] == ' ')
		*--end = 0;
	end = second;
	while (end > *playlist && end[-1] == ' ')
		*--end = 0;
	*numbers_at = second + 1;
	return (*profile)[0] && (*playlist)[0] && strlen(*profile) < AE_CAREER_ID && strlen(*playlist) < AE_CAREER_ID;
}

/* a social line read into the tables; -1 for a line of a profile and playlist the tables already hold (an edited file), else 0 */
static int social_line(struct ae_career *career, char const *key, char *value)
{
	char *profile, *playlist, *rest;
	long v[6];

	if (!strcmp(key, "match"))
	{
		struct ae_career_match *m;

		if (career->match_count >= AE_CAREER_SOCIAL || !social_split(value, &profile, &playlist, &rest) || !numbers(rest, v, 3))
			return 0;
		if (ae_career_match_find(career, profile, playlist))
			return -1;
		m = &career->match[career->match_count++];
		memset(m, 0, sizeof(*m));
		snprintf(m->profile, sizeof(m->profile), "%s", profile);
		snprintf(m->playlist, sizeof(m->playlist), "%s", playlist);
		m->points = v[0] > 4999 ? 4999 : v[0];
		m->games = v[1];
		m->last = (unsigned long)v[2];
	}
	else if (!strcmp(key, "social"))
	{
		struct ae_career_social *c;

		if (career->social_count >= AE_CAREER_SOCIAL || !social_split(value, &profile, &playlist, &rest) || !numbers(rest, v, 6))
			return 0;
		if (ae_career_social_find(career, profile, playlist))
			return -1;
		c = &career->social[career->social_count++];
		memset(c, 0, sizeof(*c));
		snprintf(c->profile, sizeof(c->profile), "%s", profile);
		snprintf(c->playlist, sizeof(c->playlist), "%s", playlist);
		c->exp = v[0] > AE_SOCIAL_EXP_MAX ? AE_SOCIAL_EXP_MAX : v[0];
		c->games = v[1];
		c->wins = v[2];
		c->ties = v[3];
		c->quits = v[4];
		c->last = (unsigned long)v[5];
	}
	else if (!strcmp(key, "pending"))
	{
		struct ae_career_pending *g;

		if (!social_split(value, &profile, &playlist, &rest) || !numbers(rest, v, 1))
			return 0;
		/* (one a profile: a second line of the same profile is an edited file) */
		if (ae_career_pending_find(career, profile))
			return -1;
		if (career->pending_count >= AE_CAREER_PENDING)
			return 0;
		g = &career->pending[career->pending_count++];
		memset(g, 0, sizeof(*g));
		snprintf(g->profile, sizeof(g->profile), "%s", profile);
		snprintf(g->playlist, sizeof(g->playlist), "%s", playlist);
		g->start = (unsigned long)v[0];
	}
	else if (!strcmp(key, "experience"))
	{
		char *first = strchr(value, '|'), *end;
		long number;

		if (!first)
			return 0;
		*first = 0;
		end = first;
		while (end > value && end[-1] == ' ')
			*--end = 0;
		if (!value[0] || strlen(value) >= AE_CAREER_ID || !numbers(first + 1, &number, 1) ||
			number <= AE_SOCIAL_EXPERIENCE_UNSET || number >= NUMBER_OF_AE_SOCIAL_EXPERIENCES)
			return 0;
		if (experience_find(career, value))
			return -1;
		if (career->experience_count >= AE_CAREER_EXPERIENCES)
			return 0;
		ae_career_experience_set(career, value, (int)number);
	}
	else if (!strcmp(key, "xp"))
	{
		char *first = strchr(value, '|'), *end;
		long number[2];
		struct ae_career_xp *entry;

		if (!first)
			return 0;
		*first = 0;
		end = first;
		while (end > value && end[-1] == ' ')
			*--end = 0;
		if (!value[0] || strlen(value) >= AE_CAREER_ID || !numbers(first + 1, number, 2))
			return 0;
		if (ae_career_xp_find(career, value))
			return -1;
		entry = ae_career_xp_entry(career, value);
		if (!entry)
			return 0;
		entry->xp = number[0] > AE_XP_MAX ? AE_XP_MAX : number[0];
		entry->last = (unsigned long)number[1];
	}
	else if (!strcmp(key, "challenges"))
	{
		char *bar = strchr(value, '|'), *end, *piece;
		long number[2];
		struct ae_career_challenges *entry, fresh;
		int each = 0;

		if (!bar)
			return 0;
		*bar = 0;
		end = bar;
		while (end > value && end[-1] == ' ')
			*--end = 0;
		if (!value[0] || strlen(value) >= AE_CAREER_ID)
			return 0;
		memset(&fresh, 0, sizeof(fresh));
		piece = bar + 1;
		/* (the day and the week, then a slot after each bar: its kind's id, the counter and 0 or 1) */
		bar = strchr(piece, '|');
		if (bar)
			*bar = 0;
		if (!numbers(piece, number, 2))
			return 0;
		fresh.day = number[0];
		fresh.week = number[1];
		while (bar && each < AE_CAREER_CHALLENGE_SLOTS)
		{
			char *space, *next;
			long counted[2];
			size_t length;

			piece = bar + 1;
			bar = strchr(piece, '|');
			if (bar)
				*bar = 0;
			next = piece;
			while (*next == ' ')
				next++;
			space = strchr(next, ' ');
			if (!space)
				return 0;
			*space = 0;
			length = strlen(next);
			if (!length || length >= AE_CAREER_CHALLENGE_ID || !numbers(space + 1, counted, 2) || counted[1] > 1)
				return 0;
			snprintf(fresh.slot[each].kind, sizeof(fresh.slot[each].kind), "%s", strcmp(next, "-") ? next : "");
			fresh.slot[each].progress = counted[0];
			fresh.slot[each].done = (int)counted[1];
			each++;
		}
		if (ae_career_challenges_find(career, value))
			return -1;
		entry = ae_career_challenges_entry(career, value);
		if (!entry)
			return 0;
		entry->day = fresh.day;
		entry->week = fresh.week;
		memcpy(entry->slot, fresh.slot, sizeof(entry->slot));
	}
	return 0;
}

int ae_career_read(struct ae_career *career, char const *text)
{
	char line[AE_CAREER_LINE];
	char const *start = text;
	int header = 0, summed = 0;

	ae_career_init(career);
	while (*text)
	{
		size_t length = strcspn(text, "\n");
		char const *line_start = text;
		char *equals, *key, *value, *end;

		if (length >= sizeof(line))
			goto bad;
		memcpy(line, text, length);
		line[length] = 0;
		text += length + (text[length] ? 1 : 0);
		if (length && line[length - 1] == '\r')
			line[length - 1] = 0;
		if (!line[0] || line[0] == '#')
			continue;
		equals = strchr(line, '=');
		if (!equals)
			goto bad;
		*equals = 0;
		key = line;
		value = equals + 1;
		while (*value == ' ')
			value++;
		end = key + strlen(key);
		while (end > key && end[-1] == ' ')
			*--end = 0;
		if (!header)
		{
			long version;

			if (strcmp(key, "version") || !numbers(value, &version, 1) || version < 1)
				goto bad;
			header = 1;
			career->newer = version > AE_CAREER_VERSION_MAX;
			continue;
		}
		/* (the checksum is the last line: anything after it, a second header or a second checksum is an edited file) */
		if (summed || !strcmp(key, "version"))
			goto edited;
		if (!strcmp(key, "checksum"))
		{
			char *stop;
			unsigned long given = strtoul(value, &stop, 16);

			if (stop == value || given != checksum(start, (size_t)(line_start - start)))
				goto edited;
			summed = 1;
			continue;
		}
		else
		{
			int ladder = !strcmp(key, "career") ? 0 : !strcmp(key, "online") ? 1 : -1;
			char *bar, *name;
			long v[7];
			struct ae_career_entry *entry;

			if (ladder < 0)
			{
				if (social_line(career, key, value) < 0)
					goto edited;
				continue;
			}
			bar = strchr(value, '|');
			if (!bar || career->count[ladder] >= AE_CAREER_PLAYLISTS)
				continue;
			*bar = 0;
			name = value;
			end = bar;
			while (end > name && end[-1] == ' ')
				*--end = 0;
			if (!name[0] || strlen(name) >= AE_CAREER_ID || !numbers(bar + 1, v, 7))
				continue;
			if (ae_career_find(career, ladder, name))
				goto edited;
			entry = &career->entry[ladder][career->count[ladder]++];
			memset(entry, 0, sizeof(*entry));
			snprintf(entry->playlist, sizeof(entry->playlist), "%s", name);
			ae_rating_state_init(&entry->rating, v[0] > AE_RATING_POINTS_MAX ? AE_RATING_POINTS_MAX : v[0]);
			entry->rating.level = v[1] < AE_RATING_LEVELS ? (int)v[1] : AE_RATING_LEVELS - 1;
			entry->rating.highest = v[2] < AE_RATING_LEVELS ? (int)v[2] : AE_RATING_LEVELS - 1;
			if (entry->rating.highest < entry->rating.level)
				entry->rating.highest = entry->rating.level;
			entry->games = v[3];
			entry->wins = v[4];
			entry->quits = v[5];
			entry->last = (unsigned long)v[6];
			/* (the quit clocks after a second "|") */
			bar = strchr(bar + 1, '|');
			if (bar)
			{
				char *p = bar + 1, *stop;

				while (entry->quit_stamps < AE_CAREER_QUIT_HISTORY)
				{
					unsigned long stamp = strtoul(p, &stop, 10);

					if (stop == p)
						break;
					entry->quit_stamp[entry->quit_stamps++] = stamp;
					p = stop;
				}
			}
		}
	}
	if (!header)
		goto bad;
	if (!summed && (career->count[0] || career->count[1] || career->match_count || career->social_count || career->pending_count ||
		career->experience_count || career->xp_count || career->challenge_count))
		goto edited;
	return AE_CAREER_READ_OK;
edited:
	ae_career_init(career);
	career->edited = 1;
	return AE_CAREER_READ_EDITED;
bad:
	ae_career_init(career);
	return AE_CAREER_READ_BAD;
}
