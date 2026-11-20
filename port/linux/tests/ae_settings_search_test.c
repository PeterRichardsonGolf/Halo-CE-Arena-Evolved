/* ae_settings_search_test.c: FIND A SETTING's rules (ae_settings_search.h): the normalizer, word starts, inside matches,
the joined query, the tiers and their order, hidden items, the underline's letters, duplicate labels. */

#include <stdio.h>
#include <string.h>

#include "ae_settings_search.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_search_item const items[] =
{
	/* 0 */ { "CONTROLS", "CONTROLLER", "ZOOM", "", "TOGGLE, HOLD or BOTH.", 0 },
	/* 1 */ { "CONTROLS", "CONTROLLER", "ZOOM RESET", "", "Where the zoom starts.", 0 },
	/* 2 */ { "CONTROLS", "MOUSE", "ZOOM", "", "The mouse's zoom.", 0 },
	/* 3 */ { "CONTROLS", "MOUSE", "MOUSE SENSITIVITY", "SENS", "How fast the view turns.", 0 },
	/* 4 */ { "VIDEO", "DISPLAY", "V-SYNC", "TEARING", "Waits for the screen's refresh.", 0 },
	/* 5 */ { "VIEW", "FIRST PERSON", "FOV", "FIELD OF VIEW", "Wider sees more.", 0 },
	/* 6 */ { "CONTROLS", "GYRO", "GYRO AIMING", "", "Aim by tilting the pad.", 1 },
	/* 7 */ { "HUD", "NAMES", "PLAYER NAME SIZE", "", "How big names are. Zoom does not change it.", 0 },
	/* 8 */ { "ACCESSIBILITY", "MENUS", "AUTO-CENTER LOOK", "", "Looks back to the middle.", 0 },
	/* 9 */ { "AUDIO", "ADVANCED", "3D SOUND (HRTF)", "", "Sound in space.", 0 },
};
#define N ((short)(sizeof(items) / sizeof(items[0])))

static int get(void *context, short index, struct ae_search_item *item)
{
	(void)context;
	*item = items[index];
	return 1;
}

static short run(const char *query, struct ae_search_hit *hits, short *hidden)
{
	return ae_search_run(query, N, get, NULL, hits, 32, hidden);
}

int main(void)
{
	char out[64];
	short map[64], hidden;
	struct ae_search_hit hits[32];
	short count;

	/* the normalizer */
	CHECK(ae_search_normalize("V-Sync", out, sizeof(out), map) == 5 && !strcmp(out, "VSYNC"));
	CHECK(map[0] == 0 && map[1] == 2 && map[4] == 5);
	CHECK(ae_search_normalize("  3D SOUND (HRTF)  ", out, sizeof(out), NULL) == 13 && !strcmp(out, "3D SOUND HRTF"));
	CHECK(ae_search_normalize("A/B swap", out, sizeof(out), NULL) == 7 && !strcmp(out, "AB SWAP"));
	CHECK(ae_search_normalize("", out, sizeof(out), NULL) == 0 && !out[0]);
	CHECK(ae_search_normalize(NULL, out, sizeof(out), NULL) == 0);
	CHECK(ae_search_normalize("abcdef", out, 4, NULL) == 3 && !strcmp(out, "ABC"));

	/* an empty query has no hits */
	CHECK(run("", hits, &hidden) == 0 && hidden == 0);
	CHECK(run("  - ", hits, &hidden) == 0);

	/* zoom: the labels in table order (both ZOOMs are duplicates in two sections), then the help's mention */
	count = run("zoom", hits, &hidden);
	CHECK(count == 4);
	CHECK(hits[0].item == 0 && hits[0].tier == AE_SEARCH_LABEL && hits[0].match_start == 0 && hits[0].match_len == 4);
	CHECK(hits[1].item == 1 && hits[1].tier == AE_SEARCH_LABEL);
	CHECK(hits[2].item == 2 && hits[2].tier == AE_SEARCH_LABEL);
	CHECK(hits[3].item == 7 && hits[3].tier == AE_SEARCH_HELP && hits[3].match_len == 0);

	/* the section and group names: tier 2 (after the labels) */
	count = run("mouse", hits, &hidden);
	CHECK(count == 2);
	CHECK(hits[0].item == 3 && hits[0].tier == AE_SEARCH_LABEL);
	CHECK(hits[1].item == 2 && hits[1].tier == AE_SEARCH_SECTION);
	count = run("display", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 4 && hits[0].tier == AE_SEARCH_SECTION);

	/* every word, at a word start, any order, across label and section */
	count = run("zoom mouse", hits, &hidden);
	CHECK(count >= 1 && hits[0].item == 2 && hits[0].tier == AE_SEARCH_SECTION && hits[0].match_len == 4);
	count = run("reset zoom", hits, &hidden);
	CHECK(count >= 1 && hits[0].item == 1 && hits[0].tier == AE_SEARCH_LABEL);

	/* the underline: the matched word as the label is written */
	count = run("reset", hits, &hidden);
	CHECK(count >= 1 && hits[0].item == 1 && hits[0].match_start == 5 && hits[0].match_len == 5);
	count = run("center", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 8 && hits[0].match_start == 5 && hits[0].match_len == 6);

	/* hyphens fold: V-SYNC by "vsync", "v sync" and "v-sync"; inside a word by "sync" (ranked after word starts) */
	count = run("vsync", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 4 && hits[0].match_start == 0 && hits[0].match_len == 6);
	count = run("v sync", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 4);
	count = run("v-sync", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 4);
	count = run("sync", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 4 && hits[0].inside == 1 && hits[0].match_start == 2 && hits[0].match_len == 4);
	/* inside a word only for one word of 3 letters or more */
	CHECK(run("ync", hits, &hidden) == 1);
	CHECK(run("yn", hits, &hidden) == 0);
	CHECK(run("sy nc", hits, &hidden) == 0);

	/* keywords: tier 3 */
	count = run("tearing", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 4 && hits[0].tier == AE_SEARCH_KEYWORD && hits[0].match_len == 0);
	count = run("sens", hits, &hidden);
	CHECK(count >= 1 && hits[0].item == 3);
	count = run("field of view", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 5 && hits[0].tier == AE_SEARCH_KEYWORD);

	/* the help: tier 4, words of 3 letters or more only */
	count = run("refresh", hits, &hidden);
	CHECK(count == 1 && hits[0].item == 4 && hits[0].tier == AE_SEARCH_HELP);
	CHECK(run("wider", hits, &hidden) == 1);
	CHECK(run("do", hits, &hidden) == 0);

	/* a hidden item (it needs a device) is counted, never listed */
	count = run("gyro", hits, &hidden);
	CHECK(count == 0 && hidden == 1);
	count = run("tilting", hits, &hidden);
	CHECK(count == 0 && hidden == 1);
	count = run("zoom", hits, &hidden);
	CHECK(hidden == 0);

	/* none: no hits, nothing hidden */
	CHECK(run("zoomz", hits, &hidden) == 0 && hidden == 0);

	/* case and marks: "Zoom!" is zoom */
	CHECK(run("Zoom!", hits, &hidden) == 4);
	CHECK(run("hrtf", hits, &hidden) == 1);
	CHECK(run("3d", hits, &hidden) == 1);

	/* the hits stop at maximum */
	CHECK(ae_search_run("zoom", N, get, NULL, hits, 2, &hidden) == 2);

	if (!failures)
		printf("ae_settings_search_test: ok\n");
	return failures ? 1 : 0;
}
