#include <stdio.h>
#include <string.h>
#include "ae_changelog_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const char sample[] =
	"# Changelog\n\nIntro.\n\n## Unreleased\n\n### Added\n- next\n\n"
	"## 0.9.0-beta - 2026-11-20\n\n### Added\n- MATCHMAKING\n\n### Fixed\n- a crash\n\n"
	"## 0.8.1-beta - 2026-11-12\n\n### Fixed\n- ragdolls\n\n"
	"## not a release\n\n- skipped\n\n"
	"## 0.8.0-beta - 2026-11-09\n\n### Added\n- servers\n";

int main(void)
{
	/* (review 0.8.1: a CHANGELOG.md with Windows line ends joined paragraphs across their blank lines) the line at at
	follows a blank one, with "\n" or "\r\n" line ends; the first line follows none */
	{
		static const char unix_text[] = "one\n\ntwo\nthree";
		static const char dos_text[] = "one\r\n\r\ntwo\r\nthree";

		CHECK(ae_changelog_after_blank_line(unix_text, unix_text + 5));
		CHECK(!ae_changelog_after_blank_line(unix_text, unix_text + 9));
		CHECK(ae_changelog_after_blank_line(dos_text, dos_text + 7));
		CHECK(!ae_changelog_after_blank_line(dos_text, dos_text + 12));
		CHECK(!ae_changelog_after_blank_line(unix_text, unix_text));
		CHECK(!ae_changelog_after_blank_line(unix_text, unix_text + 1));
	}
	struct ae_release_entry releases[8];
	int count;

	/* changelog_three_releases: Unreleased skipped, newest first, each release's text from after its heading to the
	next heading of its level */
	count = ae_changelog_releases(sample, releases, 8);
	CHECK(count == 3);
	CHECK(!strcmp(releases[0].version, "0.9.0-beta") && !strcmp(releases[0].date, "2026-11-20"));
	CHECK(!strcmp(releases[1].version, "0.8.1-beta") && !strcmp(releases[2].version, "0.8.0-beta"));
	CHECK(strstr(sample + releases[0].start, "### Added\n- MATCHMAKING") == sample + releases[0].start + 1);
	CHECK(releases[0].end <= (size_t)(strstr(sample, "## 0.8.1") - sample));
	CHECK(releases[2].end == strlen(sample));
	/* changelog_bad_heading_skipped: "## not a release" is no release, and its lines belong to none */
	CHECK(releases[1].end <= (size_t)(strstr(sample, "## not a release") - sample));
	CHECK(ae_changelog_releases(NULL, releases, 8) == 0 && ae_changelog_releases("", releases, 8) == 0);
	CHECK(ae_changelog_releases(sample, releases, 2) == 2);

	count = ae_changelog_releases(sample, releases, 8);
	/* unseen_first_run_counts_installed_only: nothing opened yet: the installed release alone */
	CHECK(ae_whats_new_unseen(releases, count, "0.9.0-beta", "") == 1);
	/* unseen_counts_unopened: releases up to the installed one, not opened */
	CHECK(ae_whats_new_unseen(releases, count, "0.9.0-beta", "0.8.0-beta") == 2);
	CHECK(ae_whats_new_unseen(releases, count, "0.9.0-beta", "0.8.0-beta,0.8.1-beta,0.9.0-beta") == 0);
	/* (a release newer than the install never counts) */
	CHECK(ae_whats_new_unseen(releases, count, "0.8.1-beta", "0.8.0-beta") == 1);
	/* the opened list: a version added once */
	{
		char opened[64] = "";

		CHECK(ae_whats_new_mark(opened, (int)sizeof(opened), "0.9.0-beta") && !strcmp(opened, "0.9.0-beta"));
		CHECK(ae_whats_new_mark(opened, (int)sizeof(opened), "0.8.1-beta") && !strcmp(opened, "0.9.0-beta,0.8.1-beta"));
		CHECK(!ae_whats_new_mark(opened, (int)sizeof(opened), "0.9.0-beta"));
	}
	/* (review fix) seen up to a version: the first run seeds it with the release before the installed one, opening a
	release moves it on while the releases above it are opened, so the opened list stays short and the badge clears */
	{
		char seen[24] = "", opened[64] = "";

		count = ae_changelog_releases(sample, releases, 8);
		ae_whats_new_seed(releases, count, "0.9.0-beta", seen, (int)sizeof(seen));
		CHECK(!strcmp(seen, "0.8.1-beta"));
		CHECK(ae_whats_new_count(releases, count, "0.9.0-beta", seen, opened) == 1);
		/* opening the installed release clears it (it was 40 NEW on the real file) */
		ae_whats_new_open_release(releases, count, "0.9.0-beta", "0.9.0-beta", seen, (int)sizeof(seen), opened,
			(int)sizeof(opened));
		CHECK(ae_whats_new_count(releases, count, "0.9.0-beta", seen, opened) == 0);
		CHECK(!strcmp(seen, "0.9.0-beta") && !opened[0]);
		/* opening older releases never brings a badge back */
		ae_whats_new_open_release(releases, count, "0.9.0-beta", "0.8.0-beta", seen, (int)sizeof(seen), opened,
			(int)sizeof(opened));
		CHECK(ae_whats_new_count(releases, count, "0.9.0-beta", seen, opened) == 0 && !opened[0]);
		/* an update to a newer install: its release alone is new */
		CHECK(ae_whats_new_count(releases, count, "0.9.0-beta", "0.8.0-beta", "") == 2);
		/* a release above seen opened out of order is kept until the ones under it are opened */
		snprintf(seen, sizeof(seen), "0.8.0-beta");
		ae_whats_new_open_release(releases, count, "0.9.0-beta", "0.9.0-beta", seen, (int)sizeof(seen), opened,
			(int)sizeof(opened));
		CHECK(!strcmp(seen, "0.8.0-beta") && !strcmp(opened, "0.9.0-beta"));
		CHECK(ae_whats_new_count(releases, count, "0.9.0-beta", seen, opened) == 1);
		ae_whats_new_open_release(releases, count, "0.9.0-beta", "0.8.1-beta", seen, (int)sizeof(seen), opened,
			(int)sizeof(opened));
		CHECK(!strcmp(seen, "0.9.0-beta") && !opened[0] && ae_whats_new_count(releases, count, "0.9.0-beta", seen, opened) == 0);
		/* the oldest release installed: seeded as all seen but itself */
		seen[0] = 0;
		ae_whats_new_seed(releases, count, "0.8.0-beta", seen, (int)sizeof(seen));
		CHECK(!strcmp(seen, "0") && ae_whats_new_count(releases, count, "0.8.0-beta", seen, "") == 1);
	}
	/* (0.8.2) a CHANGELOG that filled the buffer: cut back to its last whole release */
	{
		char cut[] = "# Changelog\n\n## 0.8.2-beta\n- a\n\n## 0.8.1-beta\n- b\n\n## 0.8.0-beta\n- par";
		char none[] = "no releases here at all";
		char first[] = "## 0.8.2-beta\n- a but cut";

		CHECK(ae_changelog_trim_partial(cut) == (long)strlen("# Changelog\n\n## 0.8.2-beta\n- a\n\n## 0.8.1-beta\n- b\n\n"));
		CHECK(!strcmp(cut, "# Changelog\n\n## 0.8.2-beta\n- a\n\n## 0.8.1-beta\n- b\n\n"));
		CHECK(ae_changelog_trim_partial(none) == (long)strlen("no releases here at all") && !strcmp(none, "no releases here at all"));
		/* (the only release is the cut one: kept, a partial page beats an empty one) */
		CHECK(ae_changelog_trim_partial(first) == (long)strlen("## 0.8.2-beta\n- a but cut"));
		CHECK(ae_changelog_trim_partial(NULL) == 0);
	}
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
