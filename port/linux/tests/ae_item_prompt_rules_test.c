/* ae_item_prompt_rules_test.c: the item prompt's words, sizes, fit, place and fade (ae_item_prompt_rules.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_item_prompt_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.01f)

/* a stand-in for the face: 0.55 em a character */
static float measure(wchar_t const *text, float px)
{
	return (float)wcslen(text) * 0.55f * px;
}

/* a chip as wide as it is tall */
static float square_chip(int chip_px, int action_px, void *context)
{
	(void)action_px;
	(void)context;
	return (float)chip_px;
}

static void sizes_test(void)
{
	struct ae_ip_sizes s;
	float k;

	/* the default: 70 % in one view at 1080 lines, k 1: the spec's 22 / 18 / 25 / 6 / 6 */
	k = ae_ip_scale(0.49f, 1080.0f);
	CHECK(NEAR(k, 1.0f));
	ae_ip_sizes_get(k, &s);
	CHECK(s.name == 22 && s.action == 18 && s.chip == 25 && s.gap == 6 && s.row_gap == 6);
	/* 50 %: 17 over 16, chip 23, gaps 5 and 4 */
	ae_ip_sizes_get(ae_ip_scale(0.7f * 0.5f, 1080.0f) , &s);
	CHECK(s.name == 17 && s.action == 16 && s.chip == 23 && s.gap == 5 && s.row_gap == 4);
	/* 100 %: 31 / 26 / 36 / 9 / 9 */
	ae_ip_sizes_get(ae_ip_scale(0.7f * 1.0f, 1080.0f), &s);
	CHECK(s.name == 31 && s.action == 26 && s.chip == 36 && s.gap == 9 && s.row_gap == 9);
	/* 3440 x 1440: 50 % 21 / 17 / 24 / 6 / 6, 70 % 29 / 24 / 33 / 8 / 8, 100 % 42 / 34 / 48 / 11 / 11 */
	ae_ip_sizes_get(ae_ip_scale(0.35f, 1440.0f), &s);
	CHECK(s.name == 21 && s.action == 17 && s.chip == 24 && s.gap == 6 && s.row_gap == 6);
	ae_ip_sizes_get(ae_ip_scale(0.49f, 1440.0f), &s);
	CHECK(s.name == 29 && s.action == 24 && s.chip == 33 && s.gap == 8 && s.row_gap == 8);
	ae_ip_sizes_get(ae_ip_scale(0.7f, 1440.0f), &s);
	CHECK(s.name == 42 && s.action == 34 && s.chip == 48 && s.gap == 11 && s.row_gap == 11);
	/* 720p: 50 and 70 % 17 / 16 / 23 / 5 / 4, 100 % 21 / 17 / 24 / 6 / 6 */
	ae_ip_sizes_get(ae_ip_scale(0.35f, 720.0f), &s);
	CHECK(s.name == 17 && s.action == 16 && s.chip == 23 && s.gap == 5 && s.row_gap == 4);
	ae_ip_sizes_get(ae_ip_scale(0.49f, 720.0f), &s);
	CHECK(s.name == 17 && s.action == 16 && s.chip == 23 && s.gap == 5 && s.row_gap == 4);
	ae_ip_sizes_get(ae_ip_scale(0.7f, 720.0f), &s);
	CHECK(s.name == 21 && s.action == 17 && s.chip == 24 && s.gap == 6 && s.row_gap == 6);
	/* the floors hold at any k, the name stays over the action, the chip's parity is the capitals' */
	{
		int step;

		for (step = 0; step <= 40; step++)
		{
			k = 0.2f + (float)step * 0.05f;
			ae_ip_sizes_get(k, &s);
			CHECK(s.action >= 16 && s.name >= s.action + 1 && s.row_gap >= 4);
			CHECK(((s.chip ^ (int)floorf(0.70f * (float)s.action + 0.5f)) & 1) == 0);
			CHECK(s.gap == (int)floorf((float)s.action / 3.0f + 0.5f));
		}
	}
	/* no scale: the floors */
	ae_ip_sizes_get(0.0f, &s);
	CHECK(s.action == 16 && s.name == 17 && s.row_gap == 4);
	CHECK(ae_ip_scale(0.0f, 1080.0f) == 0.0f && NEAR(ae_ip_scale(0.49f, 0.0f), 1.0f));
	/* the limit follows the view, not WAYPOINT SIZE; the top is the fixed 93 x s under the middle */
	CHECK(NEAR(ae_ip_width_limit(1.0f), 338.0f) && NEAR(ae_ip_width_limit(1440.0f / 1080.0f), 338.0f * 1440.0f / 1080.0f));
	CHECK(NEAR(ae_ip_top(540.0f, 1.0f), 633.0f) && NEAR(ae_ip_top(720.0f, 1440.0f / 1080.0f), 720.0f + 124.0f));
}

static void words_test(void)
{
	wchar_t before[AE_IP_BEFORE_SIZE], after[AE_IP_AFTER_SIZE];

	CHECK(ae_ip_words(AE_IP_PICKUP, NULL, before, after, AE_IP_AFTER_SIZE) && !wcscmp(before, L"Hold") && !wcscmp(after, L"to pick up"));
	CHECK(ae_ip_words(AE_IP_SWAP, NULL, before, after, AE_IP_AFTER_SIZE) && !wcscmp(before, L"Hold") && !wcscmp(after, L"to swap"));
	CHECK(ae_ip_words(AE_IP_ENTER, L"driver", before, after, AE_IP_AFTER_SIZE) && !wcscmp(before, L"Press") &&
		!wcscmp(after, L"to enter driver seat"));
	CHECK(ae_ip_words(AE_IP_ENTER, NULL, before, after, AE_IP_AFTER_SIZE) && !wcscmp(after, L"to enter"));
	CHECK(ae_ip_words(AE_IP_FLIP, NULL, before, after, AE_IP_AFTER_SIZE) && !wcscmp(after, L"to flip"));
	CHECK(ae_ip_words(AE_IP_DEVICE, NULL, before, after, AE_IP_AFTER_SIZE) && !wcscmp(after, L"to touch device"));
	CHECK(ae_ip_words(AE_IP_CUSTOM, L"open the door", before, after, AE_IP_AFTER_SIZE) && !wcscmp(after, L"to open the door"));
	CHECK(ae_ip_words(AE_IP_SWITCH, NULL, before, after, AE_IP_AFTER_SIZE) && !wcscmp(before, L"Press") && !wcscmp(after, L"to switch"));
	/* no kind, no words */
	CHECK(!ae_ip_words(AE_IP_NONE, NULL, before, after, AE_IP_AFTER_SIZE) && !before[0] && !after[0]);
	CHECK(!ae_ip_words(AE_IP_KINDS, NULL, before, after, AE_IP_AFTER_SIZE));
	/* a small buffer: cut, ended */
	CHECK(ae_ip_words(AE_IP_ENTER, L"passenger", before, after, 12) && wcslen(after) < 12);
	/* which have a name row */
	CHECK(ae_ip_kind_named(AE_IP_PICKUP) && ae_ip_kind_named(AE_IP_SWAP) && ae_ip_kind_named(AE_IP_ENTER) &&
		ae_ip_kind_named(AE_IP_FLIP) && ae_ip_kind_named(AE_IP_SWITCH));
	CHECK(!ae_ip_kind_named(AE_IP_DEVICE) && !ae_ip_kind_named(AE_IP_CUSTOM) && !ae_ip_kind_named(AE_IP_NONE));
	/* capitals, cut to the buffer */
	{
		wchar_t name[8];

		ae_ip_upper(L"Warthog", name, 8);
		CHECK(!wcscmp(name, L"WARTHOG"));
		ae_ip_upper(L"rocket launcher", name, 8);
		CHECK(!wcscmp(name, L"ROCKET "));
	}
}

static void wide_key_test(void)
{
	/* under 20 px, 4 letters or more: the name as a word */
	CHECK(ae_ip_chip_is_word(17, L"SHIFT") && ae_ip_chip_is_word(19, L"Enter") && ae_ip_chip_is_word(17, L"SPACE") &&
		ae_ip_chip_is_word(17, L"Ctrl"));
	/* TAB and single letters stay chips; 20 and over stay chips */
	CHECK(!ae_ip_chip_is_word(17, L"TAB") && !ae_ip_chip_is_word(17, L"E") && !ae_ip_chip_is_word(17, L"F5"));
	CHECK(!ae_ip_chip_is_word(20, L"SHIFT") && !ae_ip_chip_is_word(25, L"SPACE") && !ae_ip_chip_is_word(48, L"ENTER"));
	/* a mouse button is a picture; a key with a number is by its letters ("Num 7": 3) */
	CHECK(!ae_ip_chip_is_word(17, L"Mouse 1") && !ae_ip_chip_is_word(17, L"Num 7"));
	CHECK(!ae_ip_chip_is_word(17, NULL));
}

static void fit_test(void)
{
	struct ae_ip_text text;
	struct ae_ip_layout layout;
	float k = 1.0f, limit = 338.0f;
	int action;

	memset(&text, 0, sizeof(text));
	wcscpy(text.name, L"ROCKET LAUNCHER");
	wcscpy(text.before, L"Hold");
	wcscpy(text.after, L"to swap");
	/* it fits at 70 %: nothing steps */
	ae_ip_layout_get(k, limit, &text, measure, square_chip, NULL, &layout);
	CHECK(layout.drawn.action == 18 && layout.drawn.name == 22 && layout.drawn.chip == 25 && layout.drawn.gap == 6);
	CHECK(!wcscmp(layout.name, L"ROCKET LAUNCHER") && layout.row_width <= limit);
	CHECK(NEAR(layout.row_width, measure(L"Hold", 18) + 6 + 25 + 6 + measure(L"to swap", 18)));
	/* a long action at 100 %: the action steps down a pixel at a time, chip and gaps follow, the name stays */
	wcscpy(text.before, L"Press");
	wcscpy(text.after, L"to enter passenger seat");
	ae_ip_layout_get(ae_ip_scale(0.7f, 1080.0f), limit, &text, measure, square_chip, NULL, &layout);
	CHECK(layout.nominal.action == 26 && layout.drawn.action < 26 && layout.drawn.action >= 16);
	CHECK(layout.row_width <= limit);
	action = layout.drawn.action;
	{
		struct ae_ip_layout one_more;
		struct ae_ip_sizes up;

		/* the first size that fits: one more would not */
		ae_ip_sizes_for_action(&layout.nominal, action + 1, &up);
		CHECK(measure(L"Press", (float)(action + 1)) + (float)up.gap * 2.0f + (float)up.chip + measure(text.after, (float)(action + 1)) > limit);
		(void)one_more;
	}
	CHECK(layout.drawn.chip == layout.drawn.action * 139 / 100 || layout.drawn.chip == layout.drawn.action * 139 / 100 + 1 ||
		layout.drawn.chip == layout.drawn.action * 139 / 100 - 1);
	CHECK(layout.drawn.name == layout.nominal.name && layout.nominal.row_gap == layout.drawn.row_gap);
	/* the name in its nominal size: its baseline and the chip's middle do not move when the action steps down */
	{
		struct ae_ip_place stepped, whole_row;
		struct ae_ip_text short_text = text;

		wcscpy(short_text.after, L"to swap");
		ae_ip_layout_get(ae_ip_scale(0.7f, 1080.0f), limit, &short_text, measure, square_chip, NULL, &layout);
		CHECK(layout.drawn.action == 26);
		ae_ip_place_get(960.0f, 633.0f, &layout, &whole_row);
		ae_ip_layout_get(ae_ip_scale(0.7f, 1080.0f), limit, &text, measure, square_chip, NULL, &layout);
		ae_ip_place_get(960.0f, 633.0f, &layout, &stepped);
		CHECK(NEAR(whole_row.name_baseline, stepped.name_baseline) && NEAR(whole_row.chip_middle, stepped.chip_middle));
	}
	/* a very long action stops at the floor 16 */
	wcscpy(text.after, L"to enter a seat with a very long word after it");
	ae_ip_layout_get(1.0f, limit, &text, measure, square_chip, NULL, &layout);
	CHECK(layout.drawn.action == 16 && layout.drawn.chip == 23 && layout.drawn.gap == 5);
	/* a long name: down in size, never under the action's, then cut */
	memset(&text, 0, sizeof(text));
	wcscpy(text.name, L"WWWWWWWWWWWWWWWWWWWWWWWW");
	wcscpy(text.before, L"Hold");
	wcscpy(text.after, L"to pick up");
	ae_ip_layout_get(ae_ip_scale(0.7f, 1080.0f), limit, &text, measure, square_chip, NULL, &layout);
	CHECK(layout.drawn.name >= layout.drawn.action && layout.drawn.name < layout.nominal.name);
	CHECK(layout.name_width <= limit);
	ae_ip_layout_get(ae_ip_scale(0.7f, 1080.0f), 100.0f, &text, measure, square_chip, NULL, &layout);
	CHECK(layout.drawn.name == layout.drawn.action && wcsstr(layout.name, L"...") && layout.name_width <= 100.0f);
	/* no name: the action row alone, laid out the same */
	text.name[0] = 0;
	ae_ip_layout_get(1.0f, limit, &text, measure, square_chip, NULL, &layout);
	CHECK(!layout.name[0] && layout.name_width == 0.0f && layout.drawn.action == 18);
	/* a chip that is a word is as wide as its word at the action size (the callback's) */
	{
		struct ae_ip_layout narrow, wide;

		wcscpy(text.name, L"PISTOL");
		ae_ip_layout_get(1.0f, 1000.0f, &text, measure, square_chip, NULL, &narrow);
		CHECK(NEAR(narrow.chip_width, 25.0f));
		(void)wide;
	}
	/* no measure or no text: the nominal sizes, nothing drawn wide */
	ae_ip_layout_get(1.0f, limit, NULL, measure, square_chip, NULL, &layout);
	CHECK(layout.drawn.action == 18 && layout.row_width == 0.0f);
}

static void name_fit_test(void)
{
	wchar_t out[AE_IP_NAME_SIZE];

	CHECK(!ae_ip_name_fit(L"SNIPER RIFLE", measure, 22.0f, 338.0f, out, AE_IP_NAME_SIZE) && !wcscmp(out, L"SNIPER RIFLE"));
	/* cut: three dots, the most letters that fit */
	CHECK(ae_ip_name_fit(L"SNIPER RIFLE", measure, 22.0f, 80.0f, out, AE_IP_NAME_SIZE));
	CHECK(wcsstr(out, L"...") && measure(out, 22.0f) <= 80.0f);
	{
		wchar_t one_more[AE_IP_NAME_SIZE];
		int keep = (int)wcslen(out) - 3;

		wmemcpy(one_more, L"SNIPER RIFLE", (size_t)keep + 1);
		one_more[keep + 1] = 0;
		wcscat(one_more, L"...");
		CHECK(measure(one_more, 22.0f) > 80.0f);
	}
	/* no room at all: the dots */
	CHECK(ae_ip_name_fit(L"SNIPER RIFLE", measure, 22.0f, 1.0f, out, AE_IP_NAME_SIZE) && !wcscmp(out, L"..."));
	/* a name past the buffer is cut to it */
	CHECK(!ae_ip_name_fit(L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", measure, 1.0f, 1000.0f, out, 8) && wcslen(out) == 7);
	CHECK(!ae_ip_name_fit(NULL, measure, 22.0f, 100.0f, out, AE_IP_NAME_SIZE) && !out[0]);
	/* the split line's room: the view less 16 s, the left and the rest of the line */
	CHECK(NEAR(ae_ip_split_room(960.0f, 130.0f, 0.85f, 250.0f), 960.0f - 13.6f - 130.0f - 250.0f));
	CHECK(ae_ip_split_room(300.0f, 130.0f, 0.85f, 250.0f) == 0.0f);
}

static void place_test(void)
{
	struct ae_ip_text text;
	struct ae_ip_layout layout;
	struct ae_ip_place place;
	float top;

	memset(&text, 0, sizeof(text));
	wcscpy(text.name, L"BATTLE RIFLE");
	wcscpy(text.before, L"Hold");
	wcscpy(text.after, L"to swap");
	ae_ip_layout_get(1.0f, 338.0f, &text, measure, square_chip, NULL, &layout);
	top = ae_ip_top(540.0f, 1.0f);
	ae_ip_place_get(960.0f, top, &layout, &place);
	/* the spec at 70 %: the capitals' top 633; the name's baseline 633 + 0.70 x 22 = 648 (15.4 rounds down); the chip's top 6 under it;
	the chip's middle 654 + 12.5; the action baseline from it */
	CHECK(NEAR(place.name_baseline, 648.0f));
	CHECK(NEAR(place.chip_middle, 666.5f));
	CHECK(NEAR(place.chip_top, 654.0f) || NEAR(place.chip_top, 654.0f + 1.0f));
	CHECK(NEAR(place.action_baseline, floorf(666.5f + 0.70f * 18.0f / 2.0f + 0.5f)));
	/* each row centred on the axis by its width, on whole pixels, within a pixel */
	CHECK(fabsf(place.name_x + layout.name_width / 2.0f - 960.0f) <= 0.5f);
	CHECK(fabsf(place.row_x + layout.row_width / 2.0f - 960.0f) <= 0.5f);
	CHECK(place.name_x == floorf(place.name_x) && place.row_x == floorf(place.row_x));
	/* the pieces in order, the chip with the gap each side */
	CHECK(NEAR(place.chip_x - (place.before_x + layout.before_width), 6.0f));
	CHECK(NEAR(place.after_x - (place.chip_x + layout.chip_width), 6.0f));
	/* the 100 % case: the chip's middle 682 (rounded) as in the spec's example */
	ae_ip_layout_get(ae_ip_scale(0.7f, 1080.0f), 338.0f, &text, measure, square_chip, NULL, &layout);
	ae_ip_place_get(960.0f, top, &layout, &place);
	CHECK(NEAR(floorf(place.chip_middle + 0.5f), 682.0f));
	/* no name: the action row where it was */
	{
		struct ae_ip_place bare;

		text.name[0] = 0;
		ae_ip_layout_get(ae_ip_scale(0.7f, 1080.0f), 338.0f, &text, measure, square_chip, NULL, &layout);
		ae_ip_place_get(960.0f, top, &layout, &bare);
		CHECK(NEAR(bare.chip_middle, place.chip_middle) && NEAR(bare.action_baseline, place.action_baseline));
	}
}

static void alpha_test(void)
{
	/* fades in over 3 ticks from the start */
	CHECK(NEAR(ae_ip_alpha(100, 100, 100, 0), 0.0f));
	CHECK(NEAR(ae_ip_alpha(101, 100, 101, 0), 1.0f / 3.0f));
	CHECK(NEAR(ae_ip_alpha(103, 100, 103, 0), 1.0f));
	CHECK(NEAR(ae_ip_alpha(200, 100, 200, 0), 1.0f));
	/* a new item while one is up, or one back in range: at once */
	CHECK(NEAR(ae_ip_alpha(100, 100, 100, 1), 1.0f));
	/* it holds 4 ticks after it was last true, then fades over 4 */
	CHECK(NEAR(ae_ip_alpha(204, 100, 200, 0), 1.0f));
	CHECK(NEAR(ae_ip_alpha(205, 100, 200, 0), 0.75f));
	CHECK(NEAR(ae_ip_alpha(206, 100, 200, 0), 0.5f));
	CHECK(NEAR(ae_ip_alpha(207, 100, 200, 0), 0.25f));
	CHECK(NEAR(ae_ip_alpha(208, 100, 200, 0), 0.0f));
	CHECK(NEAR(ae_ip_alpha(300, 100, 200, 0), 0.0f));
}

static void split_test(void)
{
	/* two views 20.5 px: chip 20, gap 7; four views 18 px: chip 17, gap 6; the floor 16 */
	CHECK(ae_ip_split_chip(20.5f) == 20 && ae_ip_split_chip(18.0f) == 17);
	CHECK(ae_ip_split_gap(20.5f) == 7 && ae_ip_split_gap(18.0f) == 6);
	CHECK(NEAR(ae_ip_split_name_gap(20.5f), 20.5f));
	CHECK(ae_ip_split_chip(16.0f) == 15);
	/* the chip is odd or even as the capitals are, so it centres on them */
	{
		float px;

		for (px = 16.0f; px <= 30.0f; px += 0.5f)
			CHECK(((ae_ip_split_chip(px) ^ (int)floorf(0.70f * px + 0.5f)) & 1) == 0);
	}
}

static void action_fit_test(void)
{
	struct ae_ip_text text;
	struct ae_ip_layout layout;
	wchar_t name[AE_IP_NAME_SIZE], after[AE_IP_AFTER_SIZE];
	float room, fixed, gap, px;
	int index;

	/* one view: a custom device's words wider than the limit at the 16 px floor are cut to what is left of it */
	memset(&text, 0, sizeof(text));
	wcscpy(text.before, L"Press");
	for (index = 0; index < 3; index++)
		wcscat(text.after, L"to open the door ");
	ae_ip_layout_get(1.0f, 338.0f, &text, measure, square_chip, NULL, &layout);
	CHECK(layout.drawn.action == 16);
	CHECK(layout.row_width <= 338.0f + 0.01f && wcsstr(layout.after, L"...") && wcslen(layout.after) < wcslen(text.after));
	CHECK(NEAR(layout.row_width, layout.before_width + (float)layout.drawn.gap * 2.0f + layout.chip_width + layout.after_width));
	CHECK(NEAR(layout.after_width, measure(layout.after, 16.0f)));
	/* a row that fits keeps its words whole */
	wcscpy(text.after, L"to swap");
	ae_ip_layout_get(1.0f, 338.0f, &text, measure, square_chip, NULL, &layout);
	CHECK(!wcscmp(layout.after, L"to swap") && !wcsstr(layout.after, L"..."));

	/* the split line, the worst case in a quarter view: 640 px wide (720 lines) at 0.5, left 66, the list's 16 px, a 24 character name and a long
	custom device's words. The name keeps what it needs, the words after the chip get the rest, and the line ends 16 s short of the edge */
	px = 16.0f;
	gap = (float)ae_ip_split_gap(px);
	fixed = measure(L"Press", px) + gap + (float)ae_ip_split_chip(px);
	room = 640.0f - 16.0f * 0.5f - 66.0f;
	{
		wchar_t const *long_name = L"WWWWWWWWWWWWWWWWWWWWWWWW";
		wchar_t const *long_words = L"to open the door that leads to the lower hangar and seal it";

		ae_ip_split_fit(room, px, fixed, gap, long_name, long_words, measure, px, name, after);
		CHECK(!wcscmp(name, long_name));
		CHECK(wcsstr(after, L"...") && wcslen(after) < wcslen(long_words));
		CHECK(measure(name, px) + px + fixed + gap + measure(after, px) <= room + 0.01f);
		/* the most that fits: one more letter would not */
		{
			wchar_t more[AE_IP_AFTER_SIZE];
			int keep = (int)wcslen(after) - 3;

			wmemcpy(more, long_words, (size_t)keep + 1);
			more[keep + 1] = 0;
			wcscat(more, L"...");
			CHECK(measure(name, px) + px + fixed + gap + measure(more, px) > room);
		}
		/* a name that is itself too long for the line goes first: cut, and the words get nothing but their dots */
		ae_ip_split_fit(250.0f, px, fixed, gap, long_name, long_words, measure, px, name, after);
		CHECK(wcsstr(name, L"...") && measure(name, px) + px + fixed + gap <= 250.0f + 0.01f);
		CHECK(!wcscmp(after, L"..."));
		/* short words: whole */
		ae_ip_split_fit(room, px, fixed, gap, L"PISTOL", L"to swap", measure, px, name, after);
		CHECK(!wcscmp(name, L"PISTOL") && !wcscmp(after, L"to swap"));
		/* no name: the words have the whole line; no words: nothing for them */
		ae_ip_split_fit(room, px, fixed, gap, L"", long_words, measure, px, name, after);
		CHECK(!name[0] && measure(after, px) + fixed + gap <= room + 0.01f);
		ae_ip_split_fit(room, px, fixed, 0.0f, L"PISTOL", L"", measure, px, name, after);
		CHECK(!wcscmp(name, L"PISTOL") && !after[0]);
	}
}

int main(void)
{
	sizes_test();
	words_test();
	wide_key_test();
	fit_test();
	name_fit_test();
	place_test();
	alpha_test();
	split_test();
	action_fit_test();
	if (!failures)
		printf("ae_item_prompt_rules_test: ok\n");
	return failures != 0;
}
