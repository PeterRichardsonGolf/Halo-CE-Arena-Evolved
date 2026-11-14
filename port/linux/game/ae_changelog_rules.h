/*
AE_CHANGELOG_RULES.H

Arena Evolved's WHAT'S NEW (menus phase 1): the releases of the CHANGELOG.md shipped beside the game, by their
"## <version> - <date>" headings (newest first, as the file has them; "## Unreleased" and other headings are none), each
release's text the lines after its heading up to the next "## "; and how many releases the badge counts as unseen.
Pure (port/linux/tests/ae_changelog_rules_test.c); the screen is ae_screen_motd.c's WHAT'S NEW.
*/

#ifndef __AE_CHANGELOG_RULES_H
#define __AE_CHANGELOG_RULES_H

#include <stddef.h>

struct ae_release_entry
{
	char version[24], date[16];
	size_t start, end;    /* the release's text: text + start up to text + end */
};

/* the releases into out (at most maximum), newest first; how many */
int ae_changelog_releases(const char *text, struct ae_release_entry *out, int maximum);
/* the releases up to the installed version not in opened_csv ("0.8.1-beta,0.9.0-beta"); with nothing opened yet (the
first run) the installed release alone */
int ae_whats_new_unseen(struct ae_release_entry const *releases, int count, const char *installed, const char *opened_csv);
/* version added to opened_csv (size bytes); 0 when it was there already or does not fit */
int ae_whats_new_mark(char *opened_csv, int size, const char *version);

/* (the badge's state: every release up to seen counts as read, opened_csv the ones above it opened out of order, so the
list stays short) the first run's seen: the release before the installed one ("0" when it is the oldest) */
void ae_whats_new_seed(struct ae_release_entry const *releases, int count, const char *installed, char *seen, int size);
/* the releases above seen, up to the installed one, not opened: the badge's N */
int ae_whats_new_count(struct ae_release_entry const *releases, int count, const char *installed, const char *seen,
	const char *opened_csv);
/* version opened: kept in opened_csv, then seen moves up over every release opened right above it (they leave the
list) */
void ae_whats_new_open_release(struct ae_release_entry const *releases, int count, const char *installed,
	const char *version, char *seen, int seen_size, char *opened_csv, int opened_size);

/* whether the line starting at at (inside text) comes right after a blank line, with "\n" or "\r\n" line ends (a
paragraph's start); the first line comes after none */
int ae_changelog_after_blank_line(const char *text, const char *at);

/* (0.8.2) a CHANGELOG read that filled its buffer was cut mid-release (maybe mid-character): end the text before its last
"## " release heading, newest first, so only whole releases show. A text with one heading or none is kept as is.
Returns the length kept */
long ae_changelog_trim_partial(char *text);

#endif
