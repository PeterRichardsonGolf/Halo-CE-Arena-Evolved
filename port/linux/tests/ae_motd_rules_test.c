#include <stdio.h>
#include <string.h>
#include "monocypher-ed25519.h"
#include "ae_motd_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned char secret[64], key[32];

static void sign_hex(const char *body, char *hex)
{
	unsigned char signature[64];
	int index;

	crypto_ed25519_sign(signature, secret, (const unsigned char *)body, strlen(body));
	for (index = 0; index < 64; index++)
		sprintf(hex + index * 2, "%02x", signature[index]);
}

int main(void)
{
	unsigned char seed[32];
	struct ae_motd_state state;
	struct ae_motd motd;
	struct ae_motd_block blocks[16];
	char hex[129], text[1400];
	const char *body = "{\"rev\":7,\"title\":\"Season 2\",\"text\":\"New maps.\\n\\n- BTB on Sidewinder\\n- COMP ladder\","
		"\"posted\":1794400000,\"ends\":1794990000}";
	int index;

	memset(seed, 7, sizeof(seed));
	crypto_ed25519_key_pair(secret, key, seed);
	memset(&state, 0, sizeof(state));

	/* motd_first_run_shows: no last version (the first run): the update's screen */
	CHECK(ae_motd_should_show(&state, "0.9.0", 0, 0, "2026-11-11") == AE_MOTD_UPDATE);
	/* motd_update_shows_twice_same_day: a newer install shows even on a day a message was shown */
	snprintf(state.last_version, sizeof(state.last_version), "0.8.1");
	snprintf(state.shown_day, sizeof(state.shown_day), "2026-11-11");
	state.closed_rev = 7;
	CHECK(ae_motd_should_show(&state, "0.9.0", 1, 7, "2026-11-11") == AE_MOTD_UPDATE);
	/* motd_same_rev_same_day: nothing */
	snprintf(state.last_version, sizeof(state.last_version), "0.9.0");
	CHECK(ae_motd_should_show(&state, "0.9.0", 1, 7, "2026-11-11") == AE_MOTD_NONE);
	/* the same rev closed: never again, any day */
	CHECK(ae_motd_should_show(&state, "0.9.0", 1, 7, "2026-11-12") == AE_MOTD_NONE);
	/* motd_new_rev_next_day */
	CHECK(ae_motd_should_show(&state, "0.9.0", 1, 8, "2026-11-12") == AE_MOTD_MESSAGE);
	/* a new rev the same day it was shown: tomorrow */
	CHECK(ae_motd_should_show(&state, "0.9.0", 1, 8, "2026-11-11") == AE_MOTD_NONE);
	/* no message, nothing new: nothing; an older install never shows the update */
	CHECK(ae_motd_should_show(&state, "0.9.0", 0, 0, "2026-11-12") == AE_MOTD_NONE);
	CHECK(ae_motd_should_show(&state, "0.8.1", 0, 0, "2026-11-12") == AE_MOTD_NONE);
	/* versions: 0.10.0 after 0.9.0; a -beta / -dev suffix ignored */
	CHECK(ae_motd_version_compare("0.10.0", "0.9.0") > 0 && ae_motd_version_compare("0.8.1-beta", "0.8.1") == 0 &&
		ae_motd_version_compare("0.8.0", "0.8.1-beta-dev") < 0);

	/* motd_offline_uses_cache: no fresh message, a cached one: the cache; neither: the release notes */
	CHECK(ae_motd_pick_source(1, 1) == AE_MOTD_FROM_FRESH);
	CHECK(ae_motd_pick_source(0, 1) == AE_MOTD_FROM_CACHE);
	CHECK(ae_motd_pick_source(0, 0) == AE_MOTD_FROM_NOTES);

	/* a good message */
	sign_hex(body, hex);
	CHECK(ae_motd_check(body, strlen(body), hex, key, 1794500000UL, &motd) == AE_MOTD_OK);
	CHECK(motd.rev == 7 && !strcmp(motd.title, "Season 2") && motd.ends == 1794990000UL);
	CHECK(!strcmp(motd.text, "New maps.\n\n- BTB on Sidewinder\n- COMP ladder"));
	/* motd_bad_signature: another key's, a changed byte, a short signature */
	{
		char changed[512];

		snprintf(changed, sizeof(changed), "%s", body);
		changed[10] = 'X';
		CHECK(ae_motd_check(changed, strlen(changed), hex, key, 1794500000UL, &motd) == AE_MOTD_BAD_SIGNATURE);
		CHECK(ae_motd_check(body, strlen(body), "abcd", key, 1794500000UL, &motd) == AE_MOTD_BAD_SIGNATURE);
		CHECK(ae_motd_check(body, strlen(body), NULL, key, 1794500000UL, &motd) == AE_MOTD_BAD_SIGNATURE);
	}
	/* motd_expired: at or past its end */
	CHECK(ae_motd_check(body, strlen(body), hex, key, 1794990000UL, &motd) == AE_MOTD_EXPIRED);
	/* malformed: signed but no title */
	{
		const char *bad = "{\"rev\":3,\"text\":\"x\",\"posted\":1}";

		sign_hex(bad, hex);
		CHECK(ae_motd_check(bad, strlen(bad), hex, key, 5UL, &motd) == AE_MOTD_MALFORMED);
	}

	/* motd_bullets_and_paragraphs: "- " lines are bullets, a blank line ends a paragraph, lines of one paragraph join */
	CHECK(ae_motd_parse_text("First line\nsame paragraph.\n\nSecond.\n- one\n- two", blocks, 16) == 4);
	CHECK(!blocks[0].bullet && !strcmp(blocks[0].text, "First line same paragraph."));
	CHECK(!blocks[1].bullet && !strcmp(blocks[1].text, "Second."));
	CHECK(blocks[2].bullet && !strcmp(blocks[2].text, "one") && blocks[3].bullet && !strcmp(blocks[3].text, "two"));
	/* motd_text_too_long: over 1200 characters or 24 lines */
	memset(text, 'a', 1201);
	text[1201] = 0;
	CHECK(ae_motd_parse_text(text, blocks, 16) == -1);
	text[0] = 0;
	for (index = 0; index < 25; index++)
		strcat(text, "- x\n");
	CHECK(ae_motd_parse_text(text, blocks, 16) == -1);

	/* the state file: key value lines, read back as written */
	{
		struct ae_motd_state back;
		char file[512];

		snprintf(state.whats_new_opened, sizeof(state.whats_new_opened), "0.8.1,0.9.0");
		snprintf(state.whats_new_seen, sizeof(state.whats_new_seen), "0.8.0");
		CHECK(ae_motd_state_write(&state, file, (int)sizeof(file)) > 0);
		CHECK(ae_motd_state_read(file, &back) == 1);
		CHECK(!strcmp(back.last_version, "0.9.0") && back.closed_rev == 7 && !strcmp(back.shown_day, "2026-11-11") &&
			!strcmp(back.whats_new_opened, "0.8.1,0.9.0") && !strcmp(back.whats_new_seen, "0.8.0"));
		CHECK(ae_motd_state_read("", &back) == 1 && !back.last_version[0] && back.closed_rev == -1);
	}

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
