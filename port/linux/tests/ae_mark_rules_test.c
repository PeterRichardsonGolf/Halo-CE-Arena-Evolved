#include <math.h>
#include <stdio.h>
#include "ae_mark_rules.h"
#include "ae_wheel_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void press_test(void)
{
	struct ae_mark_press press = { 0 };
	int light = 0;
	long now;

	/* a tap marks when it ends, and the flashlight never sees it */
	CHECK(ae_mark_press_step(&press, 1, 0, 100, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 1, 0, 103, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 0, 0, 104, &light) == 1 && light == 1);
	/* a hold is the flashlight's from its hold time, and does not mark */
	CHECK(ae_mark_press_step(&press, 1, 0, 200, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS - 1, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 1, 0, 200 + AE_MARK_HOLD_TICKS, &light) == 0 && light == 1);
	CHECK(ae_mark_press_step(&press, 0, 0, 200 + AE_MARK_HOLD_TICKS + 1, &light) == 0 && light == 1);
	/* a press something else takes passes through and does not mark, even when short */
	CHECK(ae_mark_press_step(&press, 1, 1, 300, &light) == 0 && light == 1);
	CHECK(ae_mark_press_step(&press, 0, 0, 301, &light) == 0 && light == 1);
	/* taken is read at the press's start only */
	CHECK(ae_mark_press_step(&press, 1, 0, 400, &light) == 0);
	CHECK(ae_mark_press_step(&press, 1, 1, 401, &light) == 0 && light == 0);
	CHECK(ae_mark_press_step(&press, 0, 0, 402, &light) == 1);
	/* idle: nothing, and the flashlight sees its (up) button */
	for (now = 500; now < 510; now++)
		CHECK(ae_mark_press_step(&press, 0, 0, now, &light) == 0 && light == 1);
}

static void life_test(void)
{
	CHECK(ae_mark_alpha(-1) == 0.0f);
	CHECK(ae_mark_alpha(0) > 0.0f && ae_mark_alpha(0) < 1.0f);
	CHECK(ae_mark_alpha(AE_MARK_APPEAR_TICKS) == 1.0f);
	CHECK(ae_mark_alpha(AE_MARK_FADE_AT_TICKS - 1) == 1.0f);
	CHECK(ae_mark_alpha(AE_MARK_LIFE_TICKS - 1) > 0.0f && ae_mark_alpha(AE_MARK_LIFE_TICKS - 1) < 0.1f);
	CHECK(ae_mark_alpha(AE_MARK_LIFE_TICKS) == 0.0f);
	CHECK(ae_mark_appear_scale(0) > 1.1f && ae_mark_appear_scale(0) <= 1.35f);
	CHECK(ae_mark_appear_scale(AE_MARK_APPEAR_TICKS - 1) == 1.0f);
	CHECK(ae_mark_appear_scale(AE_MARK_APPEAR_TICKS) == 1.0f);
}

static void clamp_test(void)
{
	float x = 10.0f, y = -5.0f;

	/* inside and on screen: stays */
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 0) == 0 && x == 10.0f && y == -5.0f);
	/* outside: onto the ellipse, the same direction */
	x = 300.0f;
	y = 0.0f;
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 0) == 1 && fabsf(x - 100.0f) < 0.01f && y == 0.0f);
	/* off screen, however near the middle: onto the ellipse */
	x = 0.0f;
	y = -2.0f;
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 1) == 1 && x == 0.0f && fabsf(y + 50.0f) < 0.01f);
	/* dead behind */
	x = 0.0f;
	y = 0.0f;
	CHECK(ae_mark_clamp(&x, &y, 100.0f, 50.0f, 1) == 1 && y == 50.0f);
}

static void same_test(void)
{
	CHECK(ae_mark_same(AE_MARK_KIND_PLACE, -1, AE_MARK_KIND_PLACE, -1, 0.1f) == 1);
	CHECK(ae_mark_same(AE_MARK_KIND_PLACE, -1, AE_MARK_KIND_PLACE, -1, 1.0f) == 0);
	CHECK(ae_mark_same(AE_MARK_KIND_ENEMY, 5, AE_MARK_KIND_ENEMY, 5, 99.0f) == 1);
	CHECK(ae_mark_same(AE_MARK_KIND_ENEMY, 5, AE_MARK_KIND_ENEMY, 6, 0.0f) == 0);
	CHECK(ae_mark_same(AE_MARK_KIND_ITEM, 5, AE_MARK_KIND_ENEMY, 5, 0.0f) == 0);
}

static void spam_test(void)
{
	struct ae_mark_spam spam = { 0 };
	long now = 1000;
	int index;

	for (index = 1; index < AE_MARK_SPAM_LIMIT; index++)
		CHECK(ae_mark_spam_take(&spam, now++) == (index == AE_MARK_SPAM_WARN_AT ? AE_MARK_SPAM_WARN : AE_MARK_SPAM_OK));
	CHECK(ae_mark_spam_take(&spam, now) == AE_MARK_SPAM_NOW_BLOCKED);
	CHECK(ae_mark_spam_take(&spam, now + 1) == AE_MARK_SPAM_STILL_BLOCKED);
	CHECK(ae_mark_spam_take(&spam, now + AE_MARK_SPAM_BLOCK_TICKS - 1) == AE_MARK_SPAM_STILL_BLOCKED);
	CHECK(ae_mark_spam_take(&spam, now + AE_MARK_SPAM_BLOCK_TICKS) == AE_MARK_SPAM_OK);
	/* marks out of the window do not count: a steady pace never blocks */
	{
		struct ae_mark_spam steady = { 0 };

		for (index = 0; index < 200; index++)
			CHECK(ae_mark_spam_take(&steady, 5000 + index * 60) == AE_MARK_SPAM_OK);
	}
}

static void host_allows_test(void)
{
	/* an enemy within range with a clear line; none without one, nor past the range */
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ENEMY, 99.0f * 99.0f, 1) == 1);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ENEMY, 100.0f * 100.0f, 1) == 1);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ENEMY, 99.0f * 99.0f, 0) == 0);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ENEMY, 101.0f * 101.0f, 1) == 0);
	/* a place or an item: the range alone, whatever the line */
	CHECK(ae_mark_host_allows(AE_MARK_KIND_PLACE, 99.0f * 99.0f, 0) == 1);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_PLACE, 99.0f * 99.0f, 1) == 1);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_PLACE, 101.0f * 101.0f, 1) == 0);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ITEM, 99.0f * 99.0f, 0) == 1);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ITEM, 101.0f * 101.0f, 1) == 0);
	/* a kind that is none */
	CHECK(ae_mark_host_allows(AE_MARK_KIND_COUNT, 1.0f, 1) == 0);
	CHECK(ae_mark_host_allows(-1, 1.0f, 1) == 0);
}

static void reply_rules_test(void)
{
	/* OK and DIBS: an active mark, the same team */
	CHECK(ae_mark_reply_allowed(AE_CALL_OK, 1, 1, AE_MARK_KIND_PLACE, 0, 0) == 1);
	CHECK(ae_mark_reply_allowed(AE_CALL_OK, 0, 1, AE_MARK_KIND_PLACE, 0, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_OK, 1, 0, AE_MARK_KIND_PLACE, 0, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_DIBS, 1, 1, AE_MARK_KIND_ITEM, 1, 0) == 1);
	CHECK(ae_mark_reply_allowed(AE_CALL_DIBS, 0, 1, AE_MARK_KIND_ITEM, 1, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_DIBS, 1, 0, AE_MARK_KIND_ITEM, 1, 0) == 0);
	/* said once: not again */
	CHECK(ae_mark_reply_allowed(AE_CALL_OK, 1, 1, AE_MARK_KIND_ENEMY, 0, 1) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_DIBS, 1, 1, AE_MARK_KIND_ITEM, 1, 1) == 0);
	/* dibs: an item with its object, nothing else */
	CHECK(ae_mark_reply_allowed(AE_CALL_DIBS, 1, 1, AE_MARK_KIND_ENEMY, 1, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_DIBS, 1, 1, AE_MARK_KIND_PLACE, 1, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_DIBS, 1, 1, AE_MARK_KIND_ITEM, 0, 0) == 0);
	/* never mind: only after an OK, on a mark still active */
	CHECK(ae_mark_reply_allowed(AE_CALL_NEVER_MIND, 1, 1, AE_MARK_KIND_PLACE, 0, 1) == 1);
	CHECK(ae_mark_reply_allowed(AE_CALL_NEVER_MIND, 1, 1, AE_MARK_KIND_PLACE, 0, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_NEVER_MIND, 0, 1, AE_MARK_KIND_PLACE, 0, 1) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_NEVER_MIND, 1, 0, AE_MARK_KIND_PLACE, 0, 1) == 0);
	/* cancel: the marker's own (same_team says the caller made the mark) */
	CHECK(ae_mark_reply_allowed(AE_CALL_CANCEL, 1, 1, AE_MARK_KIND_ENEMY, 0, 0) == 1);
	CHECK(ae_mark_reply_allowed(AE_CALL_CANCEL, 1, 0, AE_MARK_KIND_ENEMY, 0, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_CANCEL, 0, 1, AE_MARK_KIND_ENEMY, 0, 0) == 0);
	/* the verbs that are not replies */
	CHECK(ae_mark_reply_allowed(AE_CALL_MARK, 1, 1, AE_MARK_KIND_PLACE, 0, 0) == 0);
	CHECK(ae_mark_reply_allowed(AE_CALL_WHEEL, 1, 1, AE_MARK_KIND_PLACE, 0, 0) == 0);
	CHECK(ae_mark_reply_allowed(-1, 1, 1, AE_MARK_KIND_PLACE, 0, 0) == 0);
	CHECK(ae_mark_reply_allowed(99, 1, 1, AE_MARK_KIND_PLACE, 0, 0) == 0);
}

static void ok_extension_test(void)
{
	long life = AE_MARK_LIFE_TICKS;

	/* each OK adds 90 ticks, and no more than 300 from the mark's birth */
	life = ae_mark_life_after_ok(life, 1000, 1010);
	CHECK(life == 240);
	life = ae_mark_life_after_ok(life, 1000, 1020);
	CHECK(life == 300);
	life = ae_mark_life_after_ok(life, 1000, 1030);
	CHECK(life == 300);
	/* a mark that is over is not brought back */
	CHECK(ae_mark_life_after_ok(150, 1000, 1150) == 150);
	CHECK(ae_mark_life_after_ok(150, 1000, 1400) == 150);
	/* the last tick of its life still counts */
	CHECK(ae_mark_life_after_ok(150, 1000, 1149) == 240);
	/* a clock gone back is no reason to refuse */
	CHECK(ae_mark_life_after_ok(150, 1000, 900) == 240);
}

static void effective_age_test(void)
{
	/* an unextended mark ages as it did */
	CHECK(ae_mark_effective_age(0, AE_MARK_LIFE_TICKS) == 0);
	CHECK(ae_mark_effective_age(140, AE_MARK_LIFE_TICKS) == 140);
	CHECK(ae_mark_effective_age(AE_MARK_LIFE_TICKS, AE_MARK_LIFE_TICKS) == AE_MARK_LIFE_TICKS);
	/* no life set: the fixed one */
	CHECK(ae_mark_effective_age(140, 0) == 140);
	/* an extended one holds full until its own fade, then fades the same way, ending at its life */
	CHECK(ae_mark_alpha(ae_mark_effective_age(200, 240)) == 1.0f);
	CHECK(ae_mark_alpha(ae_mark_effective_age(239, 240)) > 0.0f);
	CHECK(ae_mark_alpha(ae_mark_effective_age(239, 240)) < 0.1f);
	CHECK(ae_mark_alpha(ae_mark_effective_age(240, 240)) == 0.0f);
	CHECK(ae_mark_alpha(ae_mark_effective_age(300, 300)) == 0.0f);
	CHECK(ae_mark_alpha(ae_mark_effective_age(299, 300)) > 0.0f);
	CHECK(ae_mark_alpha(ae_mark_effective_age(100, 300)) == 1.0f);
	/* the appearance is the first ticks' still */
	CHECK(ae_mark_effective_age(1, 300) == 1);
	/* the fade follows the extended mark's age without going back */
	CHECK(ae_mark_effective_age(300 - 17, 300) == AE_MARK_LIFE_TICKS - 17);
	CHECK(ae_mark_effective_age(-1, 300) == -1);
}

static void call_kind_for_old_builds_test(void)
{
	/* ENEMY is an enemy; every call of a place, and the NEED calls (at the caller's feet), a place */
	CHECK(ae_mark_old_build_kind(AE_WHEEL_ENEMY) == AE_MARK_KIND_ENEMY);
	CHECK(ae_mark_old_build_kind(AE_WHEEL_GOING) == AE_MARK_KIND_PLACE);
	CHECK(ae_mark_old_build_kind(AE_WHEEL_ATTACK) == AE_MARK_KIND_PLACE);
	CHECK(ae_mark_old_build_kind(AE_WHEEL_DEFEND) == AE_MARK_KIND_PLACE);
	CHECK(ae_mark_old_build_kind(AE_WHEEL_WATCH) == AE_MARK_KIND_PLACE);
	CHECK(ae_mark_old_build_kind(AE_WHEEL_WEAPON) == AE_MARK_KIND_PLACE);
	CHECK(ae_mark_old_build_kind(AE_WHEEL_RIDE) == AE_MARK_KIND_PLACE);
	/* SMART keeps what the cast found: -1 */
	CHECK(ae_mark_old_build_kind(AE_WHEEL_SMART) == -1);
	/* an entry that is none */
	CHECK(ae_mark_old_build_kind(AE_WHEEL_NONE) == -1);
	CHECK(ae_mark_old_build_kind(AE_WHEEL_COUNT) == -1);
}

static void repeat_test(void)
{
	/* the same call again steps up, twice at most; another call starts over */
	CHECK(ae_mark_repeats_after(0, 0) == 0);
	CHECK(ae_mark_repeats_after(1, 0) == 1);
	CHECK(ae_mark_repeats_after(1, 1) == 2);
	CHECK(ae_mark_repeats_after(1, 2) == 2);
	CHECK(ae_mark_repeats_after(0, 2) == 0);
}

static void double_free_test(void)
{
	/* a double tap rides the count of the tap before it: only just after one, and once */
	CHECK(ae_mark_double_free(0, 1000) == 0);
	CHECK(ae_mark_double_free(1001, 1010) == 1);
	CHECK(ae_mark_double_free(1001, 1000 + AE_MARK_DOUBLE_FREE_TICKS) == 1);
	CHECK(ae_mark_double_free(1001, 1001 + AE_MARK_DOUBLE_FREE_TICKS) == 0);
	CHECK(ae_mark_double_free(1001, 900) == 0);
}

static void reach_before_line_test(void)
{
	float big = 1e30f, infinity = INFINITY, nan = NAN;

	/* the line is asked for only for an enemy within range: whatever is out of range, infinite or not a number never gets a ray */
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ENEMY, 50.0f * 50.0f) == 1);
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ENEMY, 100.0f * 100.0f) == 1);
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ENEMY, 101.0f * 101.0f) == 0);
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ENEMY, big) == 0);
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ENEMY, infinity) == 0);
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ENEMY, nan) == 0);
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ENEMY, -1.0f) == 0);
	/* a place or an item has no line */
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_PLACE, 10.0f) == 0);
	CHECK(ae_mark_host_line_wanted(AE_MARK_KIND_ITEM, 10.0f) == 0);
	/* the same distances are refused by the host's whole rule */
	CHECK(ae_mark_host_allows(AE_MARK_KIND_PLACE, infinity, 1) == 0);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ENEMY, big, 1) == 0);
	CHECK(ae_mark_host_allows(AE_MARK_KIND_ENEMY, nan, 1) == 0);
}

static void spot_finite_test(void)
{
	CHECK(ae_mark_spot_finite(1.0f, -2.0f, 3.0f) == 1);
	CHECK(ae_mark_spot_finite(0.0f, 0.0f, 0.0f) == 1);
	CHECK(ae_mark_spot_finite(NAN, 0.0f, 0.0f) == 0);
	CHECK(ae_mark_spot_finite(0.0f, INFINITY, 0.0f) == 0);
	CHECK(ae_mark_spot_finite(0.0f, 0.0f, -INFINITY) == 0);
}

static void tiers_test(void)
{
	/* a lower tier number wins whatever the score */
	CHECK(ae_mark_candidate_better(AE_TIER_ENEMY, 0.9f, AE_TIER_PICKUP, 0.0f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_PICKUP, 0.0f, AE_TIER_ENEMY, 0.9f) == 0);
	/* equal tiers go to the smaller part; a tie keeps the first */
	CHECK(ae_mark_candidate_better(AE_TIER_PICKUP, 0.2f, AE_TIER_PICKUP, 0.5f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_PICKUP, 0.5f, AE_TIER_PICKUP, 0.2f) == 0);
	CHECK(ae_mark_candidate_better(AE_TIER_PICKUP, 0.3f, AE_TIER_PICKUP, 0.3f) == 0);
	/* a marker (tier 3) beats a weapon (tier 5) and loses to an enemy (tier 1) */
	CHECK(ae_mark_candidate_better(AE_TIER_MARKER, 0.8f, AE_TIER_PICKUP, 0.1f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_PICKUP, 0.1f, AE_TIER_MARKER, 0.8f) == 0);
	CHECK(ae_mark_candidate_better(AE_TIER_ENEMY, 0.8f, AE_TIER_MARKER, 0.1f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_MARKER, 0.1f, AE_TIER_ENEMY, 0.8f) == 0);
	/* the order of the tiers */
	CHECK(AE_TIER_ENEMY < AE_TIER_BODY && AE_TIER_BODY < AE_TIER_MARKER && AE_TIER_MARKER < AE_TIER_NAMED &&
		AE_TIER_NAMED < AE_TIER_PICKUP && AE_TIER_PICKUP < AE_TIER_GROUND);
	/* a body beats an objective, a vehicle or a pickup only when nearer the aim (the weapon its player dropped lies on it) */
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.1f, AE_TIER_PICKUP, 0.5f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_PICKUP, 0.5f, AE_TIER_BODY, 0.1f) == 0);
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.5f, AE_TIER_PICKUP, 0.1f) == 0);
	CHECK(ae_mark_candidate_better(AE_TIER_PICKUP, 0.1f, AE_TIER_BODY, 0.5f) == 1);
	/* (a vehicle, a flag and a hill are all tier 4) */
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.5f, AE_TIER_NAMED, 0.1f) == 0);
	CHECK(ae_mark_candidate_better(AE_TIER_NAMED, 0.1f, AE_TIER_BODY, 0.5f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.1f, AE_TIER_NAMED, 0.5f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_NAMED, 0.5f, AE_TIER_BODY, 0.1f) == 0);
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.3f, AE_TIER_PICKUP, 0.3f) == 0);
	/* a body beats the ground at any part, a marker still beats a pickup, and an enemy a body */
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.99f, AE_TIER_GROUND, 0.0f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_GROUND, 0.0f, AE_TIER_BODY, 0.99f) == 0);
	CHECK(ae_mark_candidate_better(AE_TIER_MARKER, 0.9f, AE_TIER_PICKUP, 0.1f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.1f, AE_TIER_MARKER, 0.9f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_ENEMY, 0.9f, AE_TIER_BODY, 0.1f) == 1);
	CHECK(ae_mark_candidate_better(AE_TIER_BODY, 0.1f, AE_TIER_ENEMY, 0.9f) == 0);
	/* anything beats nothing (tier 0) */
	CHECK(ae_mark_candidate_better(AE_TIER_GROUND, 0.0f, 0, 0.0f) == 1);
	CHECK(ae_mark_candidate_better(0, 0.0f, AE_TIER_GROUND, 0.0f) == 0);
}

static void press_verb_test(void)
{
	/* your own marker: cancel, whatever else is true of it */
	CHECK(ae_mark_press_verb(1, 0, 0, 0) == AE_CALL_CANCEL);
	CHECK(ae_mark_press_verb(1, 1, 1, 1) == AE_CALL_CANCEL);
	/* a teammate's marker: OK; after your OK, NEVER MIND */
	CHECK(ae_mark_press_verb(0, 0, 0, 0) == AE_CALL_OK);
	CHECK(ae_mark_press_verb(0, 1, 0, 0) == AE_CALL_NEVER_MIND);
	/* a teammate's item mark: DIBS, till someone has it (then OK) */
	CHECK(ae_mark_press_verb(0, 0, 1, 0) == AE_CALL_DIBS);
	CHECK(ae_mark_press_verb(0, 0, 1, 1) == AE_CALL_OK);
	CHECK(ae_mark_press_verb(0, 1, 1, 0) == AE_CALL_NEVER_MIND);
}

static void own_target_test(void)
{
	/* the same object, or a place this near, is the player's own mark */
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_ITEM, 7, AE_MARK_KIND_ITEM, 7, 50.0f) == 1);
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_ITEM, 7, AE_MARK_KIND_ITEM, 8, 0.0f) == 0);
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_PLACE, -1, AE_MARK_KIND_PLACE, -1, 0.1f) == 1);
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_PLACE, -1, AE_MARK_KIND_PLACE, -1, 4.0f) == 0);
	/* an enemy is the same only where it was marked: one who has moved is a new mark */
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_ENEMY, 5, AE_MARK_KIND_ENEMY, 5, 0.5f) == 1);
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_ENEMY, 5, AE_MARK_KIND_ENEMY, 5, 25.0f) == 0);
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_ENEMY, 5, AE_MARK_KIND_ENEMY, 6, 0.0f) == 0);
	CHECK(ae_mark_press_is_own(AE_MARK_KIND_ITEM, 5, AE_MARK_KIND_ENEMY, 5, 0.0f) == 0);
}

static void reply_is_for_test(void)
{
	CHECK(ae_mark_reply_is_for(AE_CALL_OK, 5, 5, 100, 99) == 1);
	/* the mark that replaced it, at the same spot too; one past its life */
	CHECK(ae_mark_reply_is_for(AE_CALL_OK, 5, 6, 100, 99) == 0);
	CHECK(ae_mark_reply_is_for(AE_CALL_DIBS, 5, 5, 100, 100) == 0);
	CHECK(ae_mark_reply_is_for(AE_CALL_NEVER_MIND, 5, 6, 100, 99) == 0);
	/* the owner's cancel */
	CHECK(ae_mark_reply_is_for(AE_CALL_CANCEL, 5, 6, 100, 200) == 1);
	/* the count wraps inside its bits */
	CHECK(ae_mark_serial_next(0) == 1 && ae_mark_serial_next(AE_MARK_SERIAL_MASK) == 0);
	/* one byte holds both, and any byte off the network reads in bounds */
	CHECK(ae_mark_wire_counts(2, 63) == 254 && ae_mark_wire_repeats(254) == 2 && ae_mark_wire_serial(254) == 63);
	CHECK(ae_mark_wire_repeats(ae_mark_wire_counts(1, 40)) == 1 && ae_mark_wire_serial(ae_mark_wire_counts(1, 40)) == 40);
	CHECK(ae_mark_wire_repeats(ae_mark_wire_counts(9, 0)) == 2);
	CHECK(ae_mark_wire_repeats(3) == 2);
}

static void rule_bytes_test(void)
{
	int on = -1;

	ae_mark_rule_read(ae_mark_rule_flags(1), &on);
	CHECK(on == 1);
	ae_mark_rule_read(ae_mark_rule_flags(0), &on);
	CHECK(on == 0);
	/* an older host's byte */
	ae_mark_rule_read(0, &on);
	CHECK(on == 1);
}

static void walls_bit_test(void)
{
	CHECK(ae_marks_walls_of(0) == 0 && ae_marks_walls_of(0x7F) == 0);
	CHECK(ae_marks_walls_set(0x51, 1) == 0xD1 && ae_marks_walls_of(0xD1) == 1);
	CHECK(ae_marks_walls_set(0xD1, 0) == 0x51);
}

static void sight_test(void)
{
	CHECK(ae_mark_sight_due(-1, 100) == 1);
	CHECK(ae_mark_sight_due(100, 102) == 0 && ae_mark_sight_due(100, 103) == 1);
	/* a new game's clock */
	CHECK(ae_mark_sight_due(100, 5) == 1);
	CHECK(ae_mark_sight_held(-1, 100) == 0);
	CHECK(ae_mark_sight_held(100, 108) == 1 && ae_mark_sight_held(100, 109) == 0);
	CHECK(ae_mark_sight_held(100, 5) == 0);
}

static void place_test(void)
{
	static struct ae_mark_place_point const points[] =
	{
		{ AE_MARK_PLACE_RED_BASE, 0.0f, 0.0f, 0.0f },
		{ AE_MARK_PLACE_BLUE_BASE, 100.0f, 0.0f, 0.0f },
		{ 0, 3.0f, 0.0f, 0.0f },		/* (an item in the red base) */
		{ 0, 50.0f, 0.0f, 0.0f },
		{ 0, 58.0f, 0.0f, 0.0f },
	};
	struct ae_mark_place_point map[5];
	wchar_t name[24];
	int index;

	for (index = 0; index < 5; index++)
		map[index] = points[index];
	map[2].id = ae_mark_place_item_id(1, 0);
	map[3].id = ae_mark_place_item_id(0, 0);
	map[4].id = ae_mark_place_item_id(3, 0);
	/* a base's range, the item in it not named */
	CHECK(ae_mark_place_pick(map, 5, 3.0f, 0.0f, 0.0f) == AE_MARK_PLACE_RED_BASE);
	CHECK(ae_mark_place_pick(map, 5, 12.0f, 0.0f, 0.0f) == AE_MARK_PLACE_RED_BASE);
	/* the nearer item, in its range */
	CHECK(ae_mark_place_pick(map, 5, 53.0f, 0.0f, 0.0f) == map[3].id);
	CHECK(ae_mark_place_pick(map, 5, 55.0f, 0.0f, 0.0f) == map[4].id);
	/* nothing near */
	CHECK(ae_mark_place_pick(map, 5, 30.0f, 0.0f, 0.0f) == AE_MARK_PLACE_NONE);
	CHECK(ae_mark_place_pick(map, 0, 0.0f, 0.0f, 0.0f) == AE_MARK_PLACE_NONE);
	/* names */
	CHECK(ae_mark_place_name(AE_MARK_PLACE_BLUE_BASE, name, 24) == 1 && !wcscmp(name, L"BLUE BASE"));
	CHECK(ae_mark_place_name(ae_mark_place_item_id(0, 0), name, 24) == 1 && !wcscmp(name, L"ROCKETS"));
	CHECK(ae_mark_place_name(ae_mark_place_item_id(1, 1), name, 24) == 1 && !wcscmp(name, L"RED SNIPER"));
	CHECK(ae_mark_place_name(ae_mark_place_item_id(4, 2), name, 24) == 1 && !wcscmp(name, L"BLUE CAMO"));
	CHECK(ae_mark_place_name(ae_mark_place_item_id(1, 2), name, 4) == 1 && !wcscmp(name, L"BLU"));
	/* the distance alone: none, or an id from nowhere */
	CHECK(ae_mark_place_name(0, name, 24) == 0 && !name[0]);
	CHECK(ae_mark_place_name(255, name, 24) == 0 && !name[0]);
	CHECK(ae_mark_place_name(-1, name, 24) == 0 && !name[0]);
}

int main(void)
{
	reply_is_for_test();
	rule_bytes_test();
	walls_bit_test();
	sight_test();
	place_test();
	spam_test();
	press_test();
	life_test();
	clamp_test();
	same_test();
	host_allows_test();
	reply_rules_test();
	ok_extension_test();
	effective_age_test();
	call_kind_for_old_builds_test();
	repeat_test();
	double_free_test();
	reach_before_line_test();
	spot_finite_test();
	tiers_test();
	press_verb_test();
	own_target_test();
	if (!failures)
		printf("ae_mark_rules_test: ok\n");
	return failures != 0;
}
