#include <stdio.h>
#include <string.h>
#include "ae_daily_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[64], day[16];

	/* a day's name against today */
	ae_daily_day_label("2026-11-25", "2026-11-25", text, sizeof(text));
	CHECK(!strcmp(text, "TODAY"));
	ae_daily_day_label("2026-11-24", "2026-11-25", text, sizeof(text));
	CHECK(!strcmp(text, "YESTERDAY"));
	ae_daily_day_label("2026-11-23", "2026-11-25", text, sizeof(text));
	CHECK(!strcmp(text, "11-23"));
	/* across a month and a year, and a leap day */
	ae_daily_day_label("2025-12-31", "2026-01-01", text, sizeof(text));
	CHECK(!strcmp(text, "YESTERDAY"));
	CHECK(ae_daily_days_ago("2024-02-28", "2024-03-01") == 2);
	CHECK(ae_daily_days_ago("2026-11-26", "2026-11-25") == -1);
	CHECK(ae_daily_days_ago("2026-13-01", "2026-11-25") == -1);
	CHECK(ae_daily_days_ago("nonsense", "2026-11-25") == -1);
	ae_daily_day_label("nonsense", "2026-11-25", text, sizeof(text));
	CHECK(text[0] == 0);
	CHECK(ae_daily_day_shift("2026-03-01", -1, day, sizeof(day)) && !strcmp(day, "2026-02-28"));
	CHECK(ae_daily_day_shift("2024-03-01", -1, day, sizeof(day)) && !strcmp(day, "2024-02-29"));
	CHECK(ae_daily_day_shift("2026-12-31", 1, day, sizeof(day)) && !strcmp(day, "2027-01-01"));
	CHECK(!ae_daily_day_shift("2026-1-1", 1, day, sizeof(day)));
	CHECK(!ae_daily_day_shift("2026-11-25", 1, day, 5));

	/* the randomizer's line from a challenge's details */
	{
		char const full[4][64] = { "ENEMIES: ALL KINDS", "ENEMY WEAPONS: PISTOL ONLY", "PICKUPS: ANY", "GO CRAZY: ON" };
		char const part[3][64] = { "ENEMIES: ELITES ONLY, ALLIES TOO", "ENEMY WEAPONS: NOT RANDOMIZED", "PICKUPS: SHOTGUN ONLY" };

		ae_daily_randomizer_summary(full, 4, text, sizeof(text));
		CHECK(!strcmp(text, "ENEMIES \xE2\x80\xA2 WEAPONS \xE2\x80\xA2 PICKUPS \xE2\x80\xA2 GO CRAZY"));
		ae_daily_randomizer_summary(part, 3, text, sizeof(text));
		CHECK(!strcmp(text, "ENEMIES \xE2\x80\xA2 PICKUPS"));
		ae_daily_randomizer_summary(part, 0, text, sizeof(text));
		CHECK(!strcmp(text, "OFF"));
	}

	/* the countdown */
	ae_daily_countdown_text(9 * 3600 + 12 * 60, 1, text, sizeof(text));
	CHECK(!strcmp(text, "A NEW CHALLENGE IN 9 H 12 MIN"));
	ae_daily_countdown_text(47 * 60 - 20, 1, text, sizeof(text));
	CHECK(!strcmp(text, "A NEW CHALLENGE IN 47 MIN"));
	ae_daily_countdown_text(5, 1, text, sizeof(text));
	CHECK(!strcmp(text, "A NEW CHALLENGE IN 1 MIN"));
	ae_daily_countdown_text(0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "THIS DAY IS CLOSED"));
	ae_daily_countdown_text(600, 0, text, sizeof(text));
	CHECK(!strcmp(text, "TAKES SCORES FOR 10 MIN"));

	/* where the day comes from */
	CHECK(strstr(ae_daily_crumb(0, 0), "OFFLINE") && strstr(ae_daily_crumb(1, 0), "ONLINE") && strstr(ae_daily_crumb(1, 1), "NOT REACHED"));
	ae_daily_board_head(1, 0, text, sizeof(text));
	CHECK(strstr(text, "TODAY'S BOARD") && strstr(text, "THIS PC'S"));
	ae_daily_board_head(0, 1, text, sizeof(text));
	CHECK(strstr(text, "ITS BOARD") && strstr(text, "ONLINE"));
	ae_daily_play_text("11-23", text, sizeof(text));
	CHECK(!strcmp(text, "PLAY 11-23'S CHALLENGE"));
	CHECK(!strcmp(ae_daily_difficulty_name(3), "LEGENDARY") && ae_daily_difficulty_name(4)[0] == 0 && ae_daily_difficulty_name(-1)[0] == 0);

	/* online needs both the setting and the consent */
	CHECK(!ae_daily_online_on(0, "yes") && !ae_daily_online_on(1, "ask") && !ae_daily_online_on(1, "no") && !ae_daily_online_on(1, NULL) &&
		ae_daily_online_on(1, "yes"));
	/* the wording says what is sent */
	CHECK(strstr(ae_daily_online_help(), "install key") && strstr(ae_daily_online_help(), "profile") && strstr(ae_daily_online_help(), "nothing leaves"));

	/* the keys: the daily's own, never a multiplayer setting, never the install's key */
	CHECK(ae_daily_key_allowed("game.daily_challenge") && ae_daily_key_allowed("community.enabled") &&
		ae_daily_key_allowed("community.upload_scores") && ae_daily_key_allowed("community.server_mode"));
	CHECK(ae_daily_key_allowed("community.custom_server"));
	{
		char out[AE_DAILY_SERVER_MAXIMUM + 16];
		char tiny[10];

		/* (SERVER ADDRESS: https only, a bare host taken as https, spaces and a trailing slash dropped) */
		CHECK(ae_daily_server_clean("https://example.com", out, sizeof(out)) && !strcmp(out, "https://example.com"));
		CHECK(ae_daily_server_clean("  example.com/  ", out, sizeof(out)) && !strcmp(out, "https://example.com"));
		CHECK(ae_daily_server_clean("https://api.example.com:8443/v1/", out, sizeof(out)) && !strcmp(out, "https://api.example.com:8443/v1"));
		CHECK(!ae_daily_server_clean("http://example.com", out, sizeof(out)) && !out[0]);
		CHECK(!ae_daily_server_clean("ftp://example.com", out, sizeof(out)) && !ae_daily_server_clean("https://", out, sizeof(out)));
		CHECK(!ae_daily_server_clean("", out, sizeof(out)) && !ae_daily_server_clean("   ", out, sizeof(out)) &&
			!ae_daily_server_clean(NULL, out, sizeof(out)));
		CHECK(!ae_daily_server_clean("https://exa mple.com", out, sizeof(out)) && !ae_daily_server_clean("https:///path", out, sizeof(out)));
		CHECK(!ae_daily_server_clean("https://a.com/\"x", out, sizeof(out)) && !ae_daily_server_clean("https://https://a.com", out, sizeof(out)));
		CHECK(!ae_daily_server_clean("https://example.com", tiny, sizeof(tiny)));
		{
			char longer[AE_DAILY_SERVER_MAXIMUM + 2];

			memset(longer, 'a', sizeof(longer));
			longer[AE_DAILY_SERVER_MAXIMUM + 1] = 0;
			CHECK(!ae_daily_server_clean(longer, out, sizeof(out)));
		}
		ae_daily_server_address_shown("default", "https://a.com", out, sizeof(out));
		CHECK(!strcmp(out, "USES DEFAULT"));
		ae_daily_server_address_shown("custom", "", out, sizeof(out));
		CHECK(!strcmp(out, "NOT SET"));
		ae_daily_server_address_shown("custom", "https://a.com", out, sizeof(out));
		CHECK(!strcmp(out, "https://a.com"));
	}
	CHECK(!ae_daily_key_allowed("community.key") && !ae_daily_key_allowed("network.community_services") &&
		!ae_daily_key_allowed("game.multiplayer_score_hud") && !ae_daily_key_allowed(NULL));

	/* a challenge's keys: exact, never the skull progress, never a prefix trick */
	{
		static const char *const progress[] = { "game.skulls_unlock_mode", "game.skulls_unlocked", "game.skulls_all_unlocked",
			"game.skulls_instant_unlock" };
		static const char *const tricks[] = { "game.skulls_", "game.skulls ", "GAME.SKULLS", "game.skull", "game.skulls_active2",
			"game.skulls_active\\0", "game.randomizer_", "game.randomizer_seed", "game.randomizer_profile", "game.randomizer_extra_enemies",
			"game.randomizerx", "game.campaign_score", "community.key", "", " game.skulls" };
		size_t each;

		for (each = 0; each < sizeof(progress) / sizeof(progress[0]); each++)
			CHECK(!ae_daily_challenge_key_allowed(progress[each]));
		for (each = 0; each < sizeof(tricks) / sizeof(tricks[0]); each++)
			CHECK(!ae_daily_challenge_key_allowed(tricks[each]));
		CHECK(!ae_daily_challenge_key_allowed(NULL));
		CHECK(ae_daily_challenge_key_allowed("game.skulls") && ae_daily_challenge_key_allowed("game.skulls_active") &&
			ae_daily_challenge_key_allowed("game.skulls_drop_count") && ae_daily_challenge_key_allowed("game.randomizer_crazy_pods") &&
			ae_daily_challenge_key_allowed("game.randomizer_flood_combat_rank"));
	}

	/* a map loaded: the menus' and the challenge's own keep its settings, any other takes them away */
	CHECK(ae_daily_map_keeps_settings("ui", "a10") && ae_daily_map_keeps_settings("a10", "a10"));
	CHECK(!ae_daily_map_keeps_settings("b30", "a10") && !ae_daily_map_keeps_settings("", "a10") && !ae_daily_map_keeps_settings("bloodgulch", "a10"));
	CHECK(!ae_daily_map_keeps_settings(NULL, "a10") && !ae_daily_map_keeps_settings("a10", NULL) && !ae_daily_map_keeps_settings("a10", "") &&
		!ae_daily_map_keeps_settings("ui", ""));

	/* a day from a server: exactly YYYY-MM-DD, nothing that could end a request's path */
	CHECK(ae_daily_day_valid("2026-11-25") && ae_daily_day_valid("1999-01-31"));
	CHECK(!ae_daily_day_valid("2026-13-01") && !ae_daily_day_valid("2026-00-10") && !ae_daily_day_valid("2026-11-32") &&
		!ae_daily_day_valid("2026-11-00") && !ae_daily_day_valid("2026-11-2") && !ae_daily_day_valid("2026-11-255") &&
		!ae_daily_day_valid("2026/11/25") && !ae_daily_day_valid("2026-11-2\n") && !ae_daily_day_valid("../../1234") &&
		!ae_daily_day_valid("") && !ae_daily_day_valid(NULL) && !ae_daily_day_valid("2026-11-25\r\nX: 1"));

	/* the window of a server not reached */
	{
		int ids[AE_DAILY_UNREACHED_CHOICES], count;
		const char *words[AE_DAILY_UNREACHED_CHOICES], *key = NULL, *value = NULL;
		char body[512];

		CHECK(ae_daily_unreached_wanted(1, 1, 0));
		CHECK(!ae_daily_unreached_wanted(0, 1, 0) && !ae_daily_unreached_wanted(1, 0, 0) && !ae_daily_unreached_wanted(1, 1, 1));
		count = ae_daily_unreached_choices(0, ids, words);
		CHECK(count == 4 && ids[0] == AE_DAILY_UNREACHED_ADDRESS && ids[1] == AE_DAILY_UNREACHED_OFFLINE && ids[2] == AE_DAILY_UNREACHED_RETRY && ids[3] == AE_DAILY_UNREACHED_BACK);
		CHECK(!strcmp(words[2], "TRY AGAIN") && !strcmp(words[3], "BACK"));
		count = ae_daily_unreached_choices(1, ids, words);
		CHECK(count == 5 && ids[0] == AE_DAILY_UNREACHED_DEFAULT && !strcmp(words[0], "TRY THE DEFAULT SERVER") && ids[4] == AE_DAILY_UNREACHED_BACK);
		CHECK(ae_daily_unreached_write(AE_DAILY_UNREACHED_DEFAULT, &key, &value) && !strcmp(key, "community.server_mode") && !strcmp(value, "default"));
		CHECK(ae_daily_unreached_write(AE_DAILY_UNREACHED_OFFLINE, &key, &value) && !strcmp(key, "community.enabled") && !strcmp(value, "false"));
		CHECK(ae_daily_key_allowed(key));
		CHECK(!ae_daily_unreached_write(AE_DAILY_UNREACHED_ADDRESS, &key, &value) && !ae_daily_unreached_write(AE_DAILY_UNREACHED_RETRY, &key, &value) && !ae_daily_unreached_write(AE_DAILY_UNREACHED_BACK, &key, &value));
		CHECK(ae_daily_retry_shown(1, 2) && !ae_daily_retry_shown(0, 2) && !ae_daily_retry_shown(1, 1) && !ae_daily_retry_shown(1, 0));
		CHECK(!strcmp(ae_daily_unreached_reason("The community server can't be reached right now. Check your connection."), "The server did not answer."));
		CHECK(strstr(ae_daily_unreached_reason("That address is not a community server."), "bad answer"));
		CHECK(strstr(ae_daily_unreached_reason("The community server is having trouble. Try again later."), "could not give"));
		CHECK(!strcmp(ae_daily_unreached_reason(NULL), "The server did not answer."));
		ae_daily_unreached_body("https://example.com", "", 1, body, sizeof(body));
		CHECK(strstr(body, "https://example.com") && strstr(body, "TRY THE DEFAULT SERVER") && strstr(body, "nothing is sent") && strlen(body) < 400);
		ae_daily_unreached_body("https://example.com", "", 0, body, sizeof(body));
		CHECK(!strstr(body, "TRY THE DEFAULT SERVER") && strstr(body, "CHANGE THE ADDRESS"));
	}

	if (failures)
		return 1;
	printf("ae_daily_rules: ok\n");
	return 0;
}
