/*
AE_MOTD_RULES.H

Arena Evolved's MESSAGE OF THE DAY (menus phase 1, spec §7): when the main menu opens on it first, the message the AE
master posts (GET /v1/motd, docs/master_protocol.md: a JSON body signed with the master's list key), its text's
paragraphs and bullets, and the state file that remembers what was seen (<save root>/ae_motd.txt, "key value" lines).
Pure (port/linux/tests/ae_motd_rules_test.c); the fetch is ae_motd_fetch.c, the screen ae_screen_motd.c.

Shown first at launch: a newer install than the last one seen (also the very first run): its release's screen; else a
message whose rev was not closed, once a day. The message's limits: a title of 60 characters, a text of 1200 characters
and 24 lines ("- " starts a bullet, a blank line ends a paragraph).
*/

#ifndef __AE_MOTD_RULES_H
#define __AE_MOTD_RULES_H

#include <stddef.h>

enum { AE_MOTD_NONE, AE_MOTD_UPDATE, AE_MOTD_MESSAGE };
enum { AE_MOTD_OK, AE_MOTD_BAD_SIGNATURE, AE_MOTD_EXPIRED, AE_MOTD_MALFORMED };
enum { AE_MOTD_FROM_FRESH, AE_MOTD_FROM_CACHE, AE_MOTD_FROM_NOTES };

#define AE_MOTD_TITLE_LIMIT 60
#define AE_MOTD_TEXT_LIMIT 1200
#define AE_MOTD_LINES_LIMIT 24

struct ae_motd
{
	long rev;
	char title[AE_MOTD_TITLE_LIMIT + 4];
	char text[AE_MOTD_TEXT_LIMIT + 1];
	unsigned long posted, ends;
};

struct ae_motd_state
{
	char last_version[24];
	long closed_rev;          /* -1: none */
	char shown_day[12];       /* YYYY-MM-DD, local */
	char whats_new_opened[256];
	char whats_new_seen[24];  /* WHAT'S NEW: every release up to this one read ("" before the first run's seed) */
};

struct ae_motd_block { int bullet; char text[400]; };

/* what opens first at launch: AE_MOTD_UPDATE, AE_MOTD_MESSAGE or AE_MOTD_NONE */
int ae_motd_should_show(struct ae_motd_state const *s, const char *installed_version, int have_message, int message_rev,
	const char *today);
/* versions as numbers ("0.10.0" after "0.9.0"; a "-beta" or other suffix ignored): <0, 0, >0 */
int ae_motd_version_compare(const char *a, const char *b);
/* the message shown: the master's fresh one, else the saved one, else the release's notes */
int ae_motd_pick_source(int have_fresh, int have_cache);
/* the text's paragraphs and bullets into out; -1 over the limits */
int ae_motd_parse_text(const char *text, struct ae_motd_block *out, int maximum);
/* the master's body checked: its signature (128 hex digits, Ed25519 over the body's bytes with key), its end (ends: 0
none), its fields. AE_MOTD_OK and out filled, else why not */
int ae_motd_check(const char *body, size_t length, const char *signature_hex, const unsigned char key[32],
	unsigned long now, struct ae_motd *out);
/* the state file: read (missing keys empty, closed_rev -1; 1 always) and written (its length, 0 when it does not fit) */
int ae_motd_state_read(const char *text, struct ae_motd_state *s);
int ae_motd_state_write(struct ae_motd_state const *s, char *out, int size);

#endif
