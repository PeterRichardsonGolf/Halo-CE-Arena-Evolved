#include <stdio.h>
#include <string.h>
#include "ae_theater_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int same(const char *a, const char *b) { return strcmp(a, b) == 0; }

int main(void)
{
	char text[64];
	static const int players[3] = { 0, 2, 5 };

	ae_theater_time_text(0, text, sizeof(text)); CHECK(same(text, "0:00"));
	ae_theater_time_text(61, text, sizeof(text)); CHECK(same(text, "1:01"));
	ae_theater_time_text(3599, text, sizeof(text)); CHECK(same(text, "59:59"));
	ae_theater_time_text(3600, text, sizeof(text)); CHECK(same(text, "1:00:00"));
	ae_theater_time_text(-5, text, sizeof(text)); CHECK(same(text, "0:00"));
	ae_theater_time_text(61, text, 3); CHECK(strlen(text) == 2);
	ae_theater_time_text(61, NULL, 0);

	ae_theater_title_text("20261119-071225_bloodgulch.ocreplay", text, sizeof(text)); CHECK(same(text, "BLOODGULCH  2026-11-19 07:12"));
	ae_theater_title_text("20261119-071225_h2_warlock.ocreplay", text, sizeof(text)); CHECK(same(text, "H2 WARLOCK  2026-11-19 07:12"));
	ae_theater_title_text("20261119-071225_.ocreplay", text, sizeof(text)); CHECK(same(text, "MAP  2026-11-19 07:12"));
	ae_theater_title_text("somefile.ocreplay", text, sizeof(text)); CHECK(same(text, "somefile.ocreplay"));
	ae_theater_title_text("2026111x-071225_a.ocreplay", text, sizeof(text)); CHECK(same(text, "2026111x-071225_a.ocreplay"));
	ae_theater_title_text("20261119-071225_bloodgulch.ocreplay", text, 8); CHECK(strlen(text) == 7);
	ae_theater_title_text(NULL, text, sizeof(text)); CHECK(text[0] == 0);

	CHECK(ae_theater_speed_step(100, 1) == 200 && ae_theater_speed_step(100, -1) == 50);
	CHECK(ae_theater_speed_step(400, 1) == 400 && ae_theater_speed_step(25, -1) == 25);
	CHECK(ae_theater_speed_step(25, 1) == 50 && ae_theater_speed_step(200, 1) == 400);
	CHECK(ae_theater_speed_step(130, 1) == 100 && ae_theater_speed_step(170, -1) == 200);
	ae_theater_speed_text(100, text, sizeof(text)); CHECK(same(text, "1x"));
	ae_theater_speed_text(25, text, sizeof(text)); CHECK(same(text, "0.25x"));
	ae_theater_speed_text(50, text, sizeof(text)); CHECK(same(text, "0.5x"));
	ae_theater_speed_text(400, text, sizeof(text)); CHECK(same(text, "4x"));
	ae_theater_speed_text(130, text, sizeof(text)); CHECK(same(text, "130%"));

	CHECK(ae_theater_skip_target(30, 1, 60) == 40 && ae_theater_skip_target(30, -1, 60) == 20);
	CHECK(ae_theater_skip_target(5, -1, 60) == 0 && ae_theater_skip_target(55, 1, 60) == 60);
	CHECK(ae_theater_skip_target(5, 1, 0) == 0 && ae_theater_skip_target(5, 0, 60) == 5 && ae_theater_skip_target(5, 1, -3) == 0);

	CHECK(ae_theater_progress(0, 60) == 0.0f && ae_theater_progress(30, 60) == 0.5f && ae_theater_progress(90, 60) == 1.0f);
	CHECK(ae_theater_progress(5, 0) == 0.0f && ae_theater_progress(-5, 60) == 0.0f);

	CHECK(ae_theater_next_player(players, 3, 0, 0) == 2 && ae_theater_next_player(players, 3, 5, 0) == 0);
	CHECK(ae_theater_next_player(players, 3, 0, 1) == 5 && ae_theater_next_player(players, 3, 2, 1) == 0);
	CHECK(ae_theater_next_player(players, 3, -1, 0) == 0 && ae_theater_next_player(players, 3, -1, 1) == 5);
	CHECK(ae_theater_next_player(players, 3, 1, 0) == 2 && ae_theater_next_player(players, 3, 1, 1) == 0);
	CHECK(ae_theater_next_player(players, 3, 7, 0) == 0 && ae_theater_next_player(players, 3, 7, 1) == 5);
	CHECK(ae_theater_next_player(players, 1, 0, 0) == 0 && ae_theater_next_player(players, 0, 0, 0) == -1);
	CHECK(ae_theater_next_player(NULL, 3, 0, 0) == -1);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
