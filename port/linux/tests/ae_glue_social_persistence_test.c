#include <stdio.h>
#include <string.h>

#include "ae_glue_career.h"
#include "ae_glue_social.h"
#include "ae_social_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_career career;
static int save_succeeds;
static int armed, disarmed;

int halo_linux_printf(const char *format, ...)
{
	(void)format;
	return 0;
}

/* (the game's headers rename snprintf: the result line's " kd K/D" text is built with it; the test reads no text) */
int halo_linux_snprintf(char *buffer, size_t size, const char *format, ...)
{
	(void)format;
	if (size)
		buffer[0] = 0;
	return 0;
}

void platform_log(char const *format, ...)
{
	(void)format;
}

void ae_dev_event(char const *kind, char const *format, ...)
{
	(void)kind;
	(void)format;
}

struct ae_career *ae_career_store(void)
{
	return &career;
}

int ae_career_store_save(void)
{
	return save_succeeds;
}

int ae_career_crash_mark_arm(void)
{
	armed++;
	return 1;
}

void ae_career_crash_mark_disarm(void)
{
	disarmed++;
}

static void reset(void)
{
	ae_career_init(&career);
	save_succeeds = 0;
	armed = 0;
	disarmed = 0;
}

static void test_failed_begin_rolls_back_and_disarms(void)
{
	reset();
	CHECK(!ae_social_pending_begin("chief", "team-slayer"));
	CHECK(career.pending_count == 0);
	CHECK(armed == 1);
	CHECK(disarmed == 1);
}

static void test_failed_end_restores_pending(void)
{
	reset();
	CHECK(ae_career_pending_set(&career, "chief", "team-slayer", 10));
	CHECK(!ae_social_pending_end("chief"));
	CHECK(career.pending_count == 1);
	CHECK(disarmed == 0);
}

static void test_failed_result_restores_career(void)
{
	struct ae_career before;
	struct ae_social_roster roster;
	struct ae_social_result result;

	reset();
	CHECK(ae_career_social_commit(&career, "chief", "team-slayer", AE_SOCIAL_WIN, AE_SOCIAL_MATCH_START, 1));
	CHECK(ae_career_pending_set(&career, "chief", "team-slayer", 10));
	before = career;
	memset(&roster, 0, sizeof(roster));
	roster.count = 2;
	roster.teams = 0;
	roster.seat[0].level = 10;
	roster.seat[1].level = 10;
	CHECK(ae_social_result_apply("chief", "team-slayer", AE_SOCIAL_WIN, &roster, 0, NULL, -1, -1, &result));
	CHECK(!result.saved);
	CHECK(!memcmp(&career, &before, sizeof(career)));
	CHECK(disarmed == 0);
}

/* (0.9.4) the calibration answer: kept when the file is written, put back (with the ratings) when it is not */
static void test_experience_set_and_rollback(void)
{
	struct ae_career before;

	reset();
	CHECK(ae_social_profile_experience("chief") == AE_SOCIAL_EXPERIENCE_UNSET);
	CHECK(ae_social_profile_match_points("chief", "team-slayer") == AE_SOCIAL_MATCH_START);
	save_succeeds = 1;
	CHECK(ae_social_profile_experience_set("chief", AE_SOCIAL_EXPERIENCE_NEW, 0));
	CHECK(ae_social_profile_experience("chief") == AE_SOCIAL_EXPERIENCE_NEW);
	CHECK(ae_social_profile_match_points("chief", "team-slayer") == ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_NEW));
	CHECK(ae_career_social_commit(&career, "chief", "team-slayer", AE_SOCIAL_WIN, 2500, 1));
	CHECK(ae_social_profile_match_points("chief", "team-slayer") == 2500);
	CHECK(ae_social_profile_experience_set("chief", AE_SOCIAL_EXPERIENCE_A_LOT, 1));
	CHECK(ae_social_profile_match_points("chief", "team-slayer") == ae_social_calibration_start(AE_SOCIAL_EXPERIENCE_A_LOT));
	CHECK(ae_social_profile_match_games("chief", "team-slayer") == 0);
	CHECK(ae_social_profile_exp("chief", "team-slayer") > 0);
	before = career;
	save_succeeds = 0;
	CHECK(!ae_social_profile_experience_set("chief", AE_SOCIAL_EXPERIENCE_SOME, 1));
	CHECK(!memcmp(&career, &before, sizeof(career)));
}

int main(void)
{
	test_experience_set_and_rollback();
	test_failed_begin_rolls_back_and_disarms();
	test_failed_end_restores_pending();
	test_failed_result_restores_career();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
