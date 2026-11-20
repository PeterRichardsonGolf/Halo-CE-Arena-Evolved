/* ae_footer_canon_rules_test.c: the button canon's footers and the footer band's curve (ae_footer_canon_rules.h). */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_footer_canon_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, tolerance) do { double x_ = (a), y_ = (b); if (fabs(x_ - y_) > (tolerance)) { \
	printf("FAIL %s:%d %s = %f, wanted %f\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static struct ae_canon_footer const *footer_named(const char *name)
{
	int index;

	for (index = 0; index < ae_canon_footer_count; index++)
		if (!strcmp(ae_canon_footers[index].screen, name))
			return &ae_canon_footers[index];
	return NULL;
}

static const char *label_of(const char *screen, int button)
{
	struct ae_canon_footer const *footer = footer_named(screen);
	int index;

	for (index = 0; footer && index < footer->count; index++)
		if (footer->prompts[index].button == button)
			return footer->prompts[index].label;
	return NULL;
}

static void test_table_follows_the_canon(void)
{
	int index;

	for (index = 0; index < ae_canon_footer_count; index++)
	{
		struct ae_canon_footer const *footer = &ae_canon_footers[index];

		if (ae_canon_footer_check(footer->screen, footer->prompts, footer->count))
		{
			printf("FAIL footer %s: code %d\n", footer->screen, ae_canon_footer_check(footer->screen, footer->prompts, footer->count));
			failures++;
		}
	}
}

static void test_playlists_footers(void)
{
	/* (0.9.4 PLAYLISTS: A uses the playlist, X manages the focused one, Y starts a new one; inside a playlist X removes a game
	and Y uses the playlist) */
	CHECK(label_of("PLAYLISTS", AE_CANON_A) && !strcmp(label_of("PLAYLISTS", AE_CANON_A), "Use"));
	CHECK(label_of("PLAYLISTS", AE_CANON_X) && !strcmp(label_of("PLAYLISTS", AE_CANON_X), "Manage"));
	CHECK(label_of("PLAYLISTS", AE_CANON_Y) && !strcmp(label_of("PLAYLISTS", AE_CANON_Y), "New playlist"));
	CHECK(label_of("PLAYLISTS", AE_CANON_B) && !strcmp(label_of("PLAYLISTS", AE_CANON_B), "Back"));
	CHECK(label_of("PLAYLIST games", AE_CANON_X) && !strcmp(label_of("PLAYLIST games", AE_CANON_X), "Remove"));
	CHECK(label_of("PLAYLIST games", AE_CANON_Y) && !strcmp(label_of("PLAYLIST games", AE_CANON_Y), "Use"));
}

static void test_b_labels(void)
{
	CHECK(ae_canon_b_label_allowed(NULL, "Back") && ae_canon_b_label_allowed(NULL, "Cancel"));
	CHECK(ae_canon_b_label_allowed(NULL, "Leave") && ae_canon_b_label_allowed(NULL, "Close"));
	CHECK(!ae_canon_b_label_allowed(NULL, "Done") && !ae_canon_b_label_allowed(NULL, "Hide"));
	CHECK(!ae_canon_b_label_allowed(NULL, "Clear search"));
	/* (the screen-pinned exceptions: each only on its own screen) */
	CHECK(!ae_canon_b_label_allowed(NULL, "Decline") && ae_canon_b_label_allowed("INVITE", "Decline"));
	CHECK(ae_canon_b_label_allowed("MAIN MENU", "Quit") && !ae_canon_b_label_allowed("SETTINGS", "Quit") &&
		!ae_canon_b_label_allowed(NULL, "Quit"));
	CHECK(ae_canon_b_label_allowed("PAUSE", "Resume") && !ae_canon_b_label_allowed("MAIN MENU", "Resume") &&
		!ae_canon_b_label_allowed(NULL, "Resume"));
	CHECK(!ae_canon_b_label_allowed("PAUSE", "Decline") && !ae_canon_b_label_allowed(NULL, NULL));
	CHECK(ae_canon_x_exception("SETTINGS group page", "Player") && ae_canon_x_exception("CROSSHAIR", "Background"));
	CHECK(!ae_canon_x_exception("SETTINGS", "Player") && !ae_canon_x_exception("GAME OPTIONS", "Background"));
}

static void test_check_finds_faults(void)
{
	struct ae_canon_prompt twice[] = { { AE_CANON_A, "Join" }, { AE_CANON_X, "Refresh" }, { AE_CANON_X, "Search" },
		{ AE_CANON_B, "Back" } };
	struct ae_canon_prompt bad_b[] = { { AE_CANON_A, "Select" }, { AE_CANON_B, "Done" } };
	struct ae_canon_prompt no_b[] = { { AE_CANON_A, "Select" } };
	struct ae_canon_prompt empty[] = { { AE_CANON_A, "" }, { AE_CANON_B, "Back" } };

	CHECK(ae_canon_footer_check(NULL, twice, 4) == 1);
	CHECK(ae_canon_footer_check(NULL, bad_b, 2) == 2);
	CHECK(ae_canon_footer_check(NULL, no_b, 1) == 3);
	CHECK(ae_canon_footer_check(NULL, empty, 2) == 4);
}

static void test_changed_screens(void)
{
	/* the server browser: Y refreshes (the LAN's: searches again), LB / RB filter, LT / RT page; no address, no X */
	CHECK(!strcmp(label_of("SERVER BROWSER", AE_CANON_Y), "Refresh"));
	CHECK(!strcmp(label_of("JOIN LAN GAMES", AE_CANON_Y), "Search again"));
	CHECK(!strcmp(label_of("SERVER BROWSER", AE_CANON_LB_RB), "Filter"));
	CHECK(!strcmp(label_of("SERVER BROWSER", AE_CANON_LT_RT), "Page"));
	CHECK(!label_of("SERVER BROWSER", AE_CANON_X) && !label_of("SERVER BROWSER", AE_CANON_DPAD));
	/* CHANGE GAMETYPE: X manages an own gametype, "Folders" on a built-in one; no SELECT */
	CHECK(!strcmp(label_of("CHANGE GAMETYPE own", AE_CANON_X), "Manage"));
	CHECK(!strcmp(label_of("CHANGE GAMETYPE built-in", AE_CANON_X), "Folders"));
	CHECK(!label_of("CHANGE GAMETYPE own", AE_CANON_SELECT));
	/* GAME OPTIONS: X resets the row, Y saves as new, B Back */
	CHECK(!strcmp(label_of("GAME OPTIONS", AE_CANON_X), "Reset"));
	CHECK(!strcmp(label_of("GAME OPTIONS", AE_CANON_Y), "Save as new"));
	CHECK(!strcmp(label_of("GAME OPTIONS", AE_CANON_B), "Back"));
	/* SETTINGS: the player is LB / RB, no X */
	CHECK(!strcmp(label_of("SETTINGS", AE_CANON_LB_RB), "Player"));
	CHECK(!label_of("SETTINGS", AE_CANON_X));
	/* the named exceptions: X on the group pages and the crosshair, Quit on the main menu, Resume in the pause */
	CHECK(!strcmp(label_of("SETTINGS group page", AE_CANON_X), "Player") && !strcmp(label_of("CROSSHAIR", AE_CANON_X), "Background"));
	CHECK(!strcmp(label_of("MAIN MENU", AE_CANON_B), "Quit") && !strcmp(label_of("PAUSE", AE_CANON_B), "Resume"));
	/* an invite keeps Decline */
	CHECK(!strcmp(label_of("INVITE", AE_CANON_B), "Decline"));
}

static void test_band_curve(void)
{
	/* 125 u tall, 40 u of fade (power 1.6), then 0.80 */
	NEAR(ae_footer_band_alpha(0.0f), 0.0, 1e-6);
	NEAR(ae_footer_band_alpha(-5.0f), 0.0, 1e-6);
	NEAR(ae_footer_band_alpha(20.0f), 0.80 * pow(0.5, 1.6), 1e-5);
	NEAR(ae_footer_band_alpha(40.0f), 0.80, 1e-6);
	NEAR(ae_footer_band_alpha(125.0f), 0.80, 1e-6);
	CHECK(ae_footer_band_alpha(10.0f) < ae_footer_band_alpha(20.0f));
	CHECK(ae_footer_band_alpha(20.0f) < ae_footer_band_alpha(39.0f));
	CHECK(AE_FOOTER_BAND_HEIGHT == 125.0f && AE_FOOTER_BAND_RAMP == 40.0f && AE_FOOTER_BAND_RGB == 0x070A10u);
}

int main(void)
{
	test_table_follows_the_canon();
	test_b_labels();
	test_playlists_footers();
	test_check_finds_faults();
	test_changed_screens();
	test_band_curve();
	if (failures)
	{
		printf("%d failed\n", failures);
		return 1;
	}
	printf("ok\n");
	return 0;
}
