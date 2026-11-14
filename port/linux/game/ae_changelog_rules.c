/*
AE_CHANGELOG_RULES.C

See ae_changelog_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_changelog_rules.h"
#include "ae_motd_rules.h"

/* "## <version> - <date>": the version starts with a digit, the date is YYYY-MM-DD */
static int heading_parse(const char *line, size_t length, struct ae_release_entry *entry)
{
	const char *dash, *end = line + length;
	size_t version, date;

	if (length < 4 || strncmp(line, "## ", 3) || line[3] < '0' || line[3] > '9')
		return 0;
	for (dash = line + 3; dash + 2 < end && strncmp(dash, " - ", 3); dash++)
		;
	if (dash + 2 >= end)
		return 0;
	version = (size_t)(dash - (line + 3));
	date = (size_t)(end - (dash + 3));
	if (!version || version >= sizeof(entry->version) || date != 10)
		return 0;
	memcpy(entry->version, line + 3, version);
	entry->version[version] = 0;
	memcpy(entry->date, dash + 3, date);
	entry->date[date] = 0;
	return 1;
}

int ae_changelog_releases(const char *text, struct ae_release_entry *out, int maximum)
{
	const char *at = text;
	int count = 0, open = 0;

	while (at && *at)
	{
		const char *end = strchr(at, '\n');
		size_t length = end ? (size_t)(end - at) : strlen(at);
		size_t trimmed = length && at[length - 1] == '\r' ? length - 1 : length;

		if (trimmed >= 3 && !strncmp(at, "## ", 3))
		{
			if (open)
				out[count - 1].end = (size_t)(at - text);
			open = 0;
			if (count < maximum && heading_parse(at, trimmed, &out[count]))
			{
				out[count].start = (size_t)(at - text) + length + (end ? 1 : 0);
				count++;
				open = 1;
			}
		}
		at += length + (end ? 1 : 0);
	}
	if (open)
		out[count - 1].end = strlen(text);
	return count;
}

static int opened(const char *csv, const char *version)
{
	size_t length = strlen(version);

	while (csv && *csv)
	{
		const char *comma = strchr(csv, ',');
		size_t size = comma ? (size_t)(comma - csv) : strlen(csv);

		if (size == length && !strncmp(csv, version, length))
			return 1;
		csv = comma ? comma + 1 : NULL;
	}
	return 0;
}

int ae_whats_new_unseen(struct ae_release_entry const *releases, int count, const char *installed, const char *opened_csv)
{
	int index, unseen = 0;

	if (!opened_csv || !*opened_csv)
	{
		for (index = 0; index < count; index++)
			if (!ae_motd_version_compare(releases[index].version, installed))
				return 1;
		return 0;
	}
	for (index = 0; index < count; index++)
		if (ae_motd_version_compare(releases[index].version, installed) <= 0 && !opened(opened_csv, releases[index].version))
			unseen++;
	return unseen;
}

void ae_whats_new_seed(struct ae_release_entry const *releases, int count, const char *installed, char *seen, int size)
{
	int index;

	/* (newest first: the first one older than the installed version) */
	for (index = 0; index < count; index++)
	{
		if (ae_motd_version_compare(releases[index].version, installed) < 0)
		{
			snprintf(seen, (size_t)size, "%s", releases[index].version);
			return;
		}
	}
	snprintf(seen, (size_t)size, "0");
}

int ae_whats_new_count(struct ae_release_entry const *releases, int count, const char *installed, const char *seen,
	const char *opened_csv)
{
	int index, unseen = 0;

	for (index = 0; index < count; index++)
		if (ae_motd_version_compare(releases[index].version, installed) <= 0 &&
			ae_motd_version_compare(releases[index].version, seen) > 0 && !opened(opened_csv, releases[index].version))
		{
			unseen++;
		}
	return unseen;
}

/* opened_csv without the versions at or under seen */
static void opened_prune(char *opened_csv, const char *seen)
{
	char kept[512] = "", *at = opened_csv;

	while (at && *at)
	{
		char *comma = strchr(at, ',');
		char version[32];
		size_t size = comma ? (size_t)(comma - at) : strlen(at);

		if (size < sizeof(version))
		{
			memcpy(version, at, size);
			version[size] = 0;
			if (ae_motd_version_compare(version, seen) > 0)
				snprintf(kept + strlen(kept), sizeof(kept) - strlen(kept), "%s%s", kept[0] ? "," : "", version);
		}
		at = comma ? comma + 1 : NULL;
	}
	strcpy(opened_csv, kept);
}

void ae_whats_new_open_release(struct ae_release_entry const *releases, int count, const char *installed,
	const char *version, char *seen, int seen_size, char *opened_csv, int opened_size)
{
	int index;

	if (ae_motd_version_compare(version, seen) <= 0)
		return;
	(void)ae_whats_new_mark(opened_csv, opened_size, version);
	/* (oldest first: seen moves over each release above it that was opened, and stops at the first not opened) */
	for (index = count - 1; index >= 0; index--)
	{
		char const *release = releases[index].version;

		if (ae_motd_version_compare(release, seen) <= 0 || ae_motd_version_compare(release, installed) > 0)
			continue;
		if (!opened(opened_csv, release))
			break;
		snprintf(seen, (size_t)seen_size, "%s", release);
	}
	opened_prune(opened_csv, seen);
}

int ae_whats_new_mark(char *opened_csv, int size, const char *version)
{
	size_t had = strlen(opened_csv);

	if (opened(opened_csv, version) || had + (had ? 1 : 0) + strlen(version) + 1 > (size_t)size)
		return 0;
	snprintf(opened_csv + had, (size_t)size - had, "%s%s", had ? "," : "", version);
	return 1;
}

int ae_changelog_after_blank_line(const char *text, const char *at)
{
	const char *previous;

	if (!text || !at || at - text < 2 || at[-1] != '\n')
		return 0;
	/* (the line before: its end at at - 1, a "\r" before that its CRLF's) */
	previous = at - 1;
	if (previous > text && previous[-1] == '\r')
		previous--;
	return previous > text && previous[-1] == '\n';
}

long ae_changelog_trim_partial(char *text)
{
	char *last = NULL, *at;

	if (!text)
		return 0;
	for (at = text; (at = strstr(at, "\n## ")) != NULL; at++)
		last = at + 1;
	/* (the last heading the first one: nothing whole before it but the title) */
	if (last && strstr(text, "## ") != last)
		*last = 0;
	return (long)strlen(text);
}
