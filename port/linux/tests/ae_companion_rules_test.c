#include <math.h>
#include <stdio.h>
#include "ae_companion_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	float offsets[AE_COMPANION_ENTRY_CANDIDATES][2];
	int count, index;

	/* facing +x: the first spot is 2.5 behind (-x), a slot's own side first (slot 0 right: -y, slot 1 left: +y) */
	count = ae_companion_entry_candidates(1.0f, 0.0f, 0, offsets);
	CHECK(count == AE_COMPANION_ENTRY_CANDIDATES);
	CHECK(fabsf(offsets[0][0] + 2.5f) < 1e-4f && fabsf(offsets[0][1] + 1.0f) < 1e-4f);
	ae_companion_entry_candidates(1.0f, 0.0f, 1, offsets);
	CHECK(fabsf(offsets[0][0] + 2.5f) < 1e-4f && fabsf(offsets[0][1] - 1.0f) < 1e-4f);
	/* any facing length; facing +y: behind is -y */
	ae_companion_entry_candidates(0.0f, 7.0f, 2, offsets);
	CHECK(fabsf(offsets[2][0]) < 1e-4f && fabsf(offsets[2][1] + 2.5f) < 1e-4f);
	/* no facing: along x */
	ae_companion_entry_candidates(0.0f, 0.0f, 0, offsets);
	CHECK(fabsf(offsets[2][0] + 2.5f) < 1e-4f);
	/* every candidate is out of view, and none on the player */
	for (index = 0; index < AE_COMPANION_ENTRY_CANDIDATES; index++)
	{
		ae_companion_entry_candidates(0.6f, -0.8f, index % 4, offsets);
		CHECK(ae_companion_entry_unseen(0.6f, -0.8f, offsets[index][0], offsets[index][1]));
		CHECK(offsets[index][0] * offsets[index][0] + offsets[index][1] * offsets[index][1] > 1.0f);
	}
	/* in view: ahead, or to the side; behind: out of it */
	CHECK(!ae_companion_entry_unseen(1.0f, 0.0f, 3.0f, 0.0f) && !ae_companion_entry_unseen(1.0f, 0.0f, 0.0f, 3.0f));
	CHECK(ae_companion_entry_unseen(1.0f, 0.0f, -3.0f, 0.5f) && !ae_companion_entry_unseen(1.0f, 0.0f, 0.0f, 0.0f));
	/* floors: flat and level, yes; too steep, too high or low, or no room above, no */
	CHECK(ae_companion_entry_floor(1.0f, 0.3f, 0.0f, 3.0f) && ae_companion_entry_floor(0.7f, -1.5f, 0.0f, 1.2f));
	CHECK(!ae_companion_entry_floor(0.69f, 0.0f, 0.0f, 3.0f) && !ae_companion_entry_floor(1.0f, 1.6f, 0.0f, 3.0f));
	CHECK(!ae_companion_entry_floor(1.0f, -1.6f, 0.0f, 3.0f) && !ae_companion_entry_floor(1.0f, 0.0f, 0.0f, 1.1f));
	/* catch-up: far, 8 s without coming nearer, unseen, on foot: yes; any one missing: no */
	CHECK(ae_companion_catch_up(25.0f, 240, 1, 0));
	CHECK(!ae_companion_catch_up(20.0f, 240, 1, 0) && !ae_companion_catch_up(25.0f, 239, 1, 0));
	CHECK(!ae_companion_catch_up(25.0f, 240, 0, 0) && !ae_companion_catch_up(25.0f, 240, 1, 1));
	/* stuck indoors: cut off 3.6 away (the pipes) or 7 away, 5 s out of view, 10 s in view */
	CHECK(ae_companion_stuck_warp(1, 1, 3.6f, 150, 1, 0, 0) && !ae_companion_stuck_warp(1, 1, 3.6f, 149, 1, 0, 0));
	CHECK(!ae_companion_stuck_warp(1, 1, 3.6f, 299, 0, 0, 0) && ae_companion_stuck_warp(1, 1, 3.6f, 300, 0, 0, 0));
	CHECK(ae_companion_stuck_warp(1, 0, 7.0f, 150, 1, 0, 0) && !ae_companion_stuck_warp(1, 0, 5.0f, 999, 1, 0, 0));
	CHECK(!ae_companion_stuck_warp(0, 1, 3.6f, 999, 1, 0, 0) && !ae_companion_stuck_warp(1, 1, 2.0f, 999, 1, 0, 0));
	CHECK(!ae_companion_stuck_warp(1, 1, 3.6f, 999, 1, 1, 0) && !ae_companion_stuck_warp(1, 1, 3.6f, 999, 1, 0, 1));
	/* the track: a straight deck along x, a crumb each 0.75 */
	{
		float track[AE_COMPANION_TRACK_MOST][3];
		int crumb;

		for (crumb = 0; crumb < 10; crumb++)
		{
			track[crumb][0] = 20.0f - 0.75f * (float)crumb;
			track[crumb][1] = -98.0f;
			track[crumb][2] = 57.0f;
		}
		/* off to the side within 3: walks to the nearest crumb, not past it */
		CHECK(ae_companion_track_step(track, 10, 17.0f, -99.5f, 57.0f) == 4);
		/* on a crumb: the next newer one; on the newest: that one */
		CHECK(ae_companion_track_step(track, 10, 17.0f, -98.0f, 57.0f) == 5);
		CHECK(ae_companion_track_step(track, 10, 13.25f, -98.0f, 57.0f) == 9);
		/* far from it, or far below (fallen under the deck): not on the track */
		CHECK(ae_companion_track_step(track, 10, 17.0f, -94.0f, 57.0f) == -1);
		CHECK(ae_companion_track_step(track, 10, 17.0f, -98.0f, 50.0f) == -1);
		CHECK(ae_companion_track_step(track, 0, 17.0f, -98.0f, 57.0f) == -1);
		/* behind the player at the newest crumb (13.25): the newest at least 2.5 back, then the next for a second member */
		CHECK(ae_companion_track_behind(track, 10, 13.25f, -98.0f, 0) == 5);
		CHECK(ae_companion_track_behind(track, 10, 13.25f, -98.0f, 1) == 4);
		CHECK(ae_companion_track_behind(track, 3, 18.5f, -98.0f, 0) == -1);
		/* the floor's kind: on an object only an object holds */
		CHECK(ae_companion_entry_floor_kind(0, 0) && ae_companion_entry_floor_kind(1, 1) && !ae_companion_entry_floor_kind(1, 0));
	}
	/* the track's crumbs and their cues: a jump marks the crumb before it, a crouch its own crumb */
	{
		float track[AE_COMPANION_TRACK_MOST][3];
		unsigned char flags[AE_COMPANION_TRACK_MOST];
		int count = 0, at;

		CHECK(ae_companion_track_push(track, flags, &count, 0.0f, 0.0f, 0.0f, 0, 0) == 1 && count == 1 && flags[0] == 0);
		CHECK(ae_companion_track_push(track, flags, &count, 0.3f, 0.0f, 0.0f, 0, 0) == 0 && count == 1);
		CHECK(ae_companion_track_push(track, flags, &count, 2.0f, 0.0f, 0.0f, 1, 0) == 1 && count == 2 &&
			flags[0] == AE_COMPANION_TRACK_JUMPED && flags[1] == 0);
		CHECK(ae_companion_track_push(track, flags, &count, 2.8f, 0.0f, 0.0f, 0, 1) == 1 && flags[2] == AE_COMPANION_TRACK_CROUCHED);
		/* (a step of more than 4: a teleport, the track starts again, unmarked) */
		CHECK(ae_companion_track_push(track, flags, &count, 9.0f, 0.0f, 0.0f, 1, 0) == 1 && count == 1 && flags[0] == 0);
		/* (full: the oldest crumb goes with its cue) */
		for (at = 1; at < AE_COMPANION_TRACK_MOST + 3; at++)
			ae_companion_track_push(track, flags, &count, 9.0f + 0.8f * (float)at, 0.0f, 0.0f, at == AE_COMPANION_TRACK_MOST + 1, 0);
		CHECK(count == AE_COMPANION_TRACK_MOST && fabsf(track[0][0] - (9.0f + 0.8f * 3.0f)) < 0.01f);
		CHECK(flags[AE_COMPANION_TRACK_MOST - 3] == AE_COMPANION_TRACK_JUMPED && flags[AE_COMPANION_TRACK_MOST - 4] == 0);
	}
	{
		float track[4][3] = { { 0.0f, 0.0f, 0.0f }, { 0.75f, 0.0f, 0.0f }, { 1.5f, 0.0f, 0.0f }, { 2.25f, 0.0f, 0.0f } };
		unsigned char flags[4] = { 0, AE_COMPANION_TRACK_JUMPED, 0, AE_COMPANION_TRACK_CROUCHED };

		/* (at the crumb before the barrier: it jumps; 1.35 short of it: not yet) */
		CHECK(ae_companion_track_cue(track, flags, 4, 2, 0.9f, 0.1f) == AE_COMPANION_TRACK_JUMPED);
		CHECK(ae_companion_track_cue(track, flags, 4, 2, -0.6f, 0.0f) == 0);
		CHECK(ae_companion_track_cue(track, flags, 4, 3, 1.5f, 0.0f) == AE_COMPANION_TRACK_CROUCHED);
		CHECK(ae_companion_track_cue(track, flags, 4, 0, 0.0f, 0.0f) == 0 && ae_companion_track_cue(track, flags, 4, -1, 0.0f, 0.0f) == 0);
		CHECK(ae_companion_track_cue(track, flags, 4, 4, 2.25f, 0.0f) == 0);
	}
	/* narrow ground: falls away on two opposite sides (a deck along x: +y and -y); one side or two neighbours only: not */
	{
		int deck[8] = { 0, 0, 1, 0, 0, 0, 1, 0 }, ledge[8] = { 0, 0, 1, 0, 0, 0, 0, 0 }, corner[8] = { 0, 0, 1, 1, 0, 0, 0, 0 };
		int open[8] = { 0 }, diagonal[8] = { 0, 1, 0, 0, 0, 1, 0, 0 };
		float offsets[AE_COMPANION_DECK_CANDIDATES][2];

		CHECK(ae_companion_narrow(deck) && ae_companion_narrow(diagonal));
		CHECK(!ae_companion_narrow(ledge) && !ae_companion_narrow(corner) && !ae_companion_narrow(open));
		/* deck spots, facing -x (walking the deck from +x): the first is 1.5 straight behind (+x), then 2.5 there */
		CHECK(ae_companion_deck_candidates(-1.0f, 0.0f, offsets) == AE_COMPANION_DECK_CANDIDATES);
		CHECK(fabsf(offsets[0][0] - 1.5f) < 1e-4f && fabsf(offsets[0][1]) < 1e-4f);
		CHECK(fabsf(offsets[1][0] - 2.5f) < 1e-4f && fabsf(offsets[1][1]) < 1e-4f);
		/* the last ones are ahead of the player (-x) */
		CHECK(offsets[AE_COMPANION_DECK_CANDIDATES - 1][0] < -1.0f);
	}
	/* the intro ride: no level on the list yet; forced, any level */
	CHECK(!ae_companion_intro_ride("b30", 0) && !ae_companion_intro_ride("a10", 0) && !ae_companion_intro_ride(NULL, 0));
	CHECK(!ae_companion_intro_ride("", 0) && ae_companion_intro_ride("b30", 1) && ae_companion_intro_ride(NULL, 1));
	if (failures)
		return 1;
	/* where a squad may be: a hosted campaign game only; a multiplayer lobby (no engine yet), the menus, a client, an engine
	game: not */
	CHECK(ae_companion_may_ask(1, 1, 0, 0, 0, 1, 0));
	CHECK(!ae_companion_may_ask(0, 1, 0, 0, 0, 1, 0));	/* (a multiplayer map's lobby: not a solo scenario) */
	CHECK(!ae_companion_may_ask(1, 1, 0, 1, 0, 1, 0));	/* (the menus) */
	CHECK(!ae_companion_may_ask(1, 1, 1, 0, 0, 1, 0));	/* (a game engine's game) */
	CHECK(!ae_companion_may_ask(1, 1, 0, 0, 1, 1, 0));	/* (a client) */
	CHECK(!ae_companion_may_ask(1, 0, 0, 0, 0, 1, 0));	/* (no AI) */
	CHECK(!ae_companion_may_ask(1, 1, 0, 0, 0, 0, 0));	/* (no server) */
	CHECK(!ae_companion_may_ask(1, 1, 0, 0, 0, 1, 1));	/* (a dedicated server) */
	/* the count: a test's next-level count holds while the menu's AI SQUAD is what it was; the menu changed after it: the menu
	wins; a DEV MODE count over the menu's; clamped */
	CHECK(ae_companion_wanted_count(2, 0, -1, 0, 4) == 2);
	CHECK(ae_companion_wanted_count(2, 0, -1, 3, 4) == 3);
	CHECK(ae_companion_wanted_count(-1, 0, 2, 3, 4) == 2);
	CHECK(ae_companion_wanted_count(-1, 0, -1, 3, 4) == 3);
	CHECK(ae_companion_wanted_count(-1, 0, -1, 9, 4) == 4);
	CHECK(ae_companion_wanted_count(-1, 0, -1, -2, 4) == 0);
	/* PLAYER DOWN: CHECKPOINT counts the humans only, RESPAWN the squad too; an unknown word is CHECKPOINT */
	CHECK(ae_companion_player_down_counts(0, 0));
	CHECK(!ae_companion_player_down_counts(1, 0));
	CHECK(ae_companion_player_down_counts(1, 1));
	CHECK(ae_companion_player_down_respawn("respawn"));
	CHECK(!ae_companion_player_down_respawn("checkpoint"));
	CHECK(!ae_companion_player_down_respawn(""));
	CHECK(!ae_companion_player_down_respawn(NULL));
	/* a player taken away by a revert is made again after a while, not at once (a join's own player comes a few ticks late) */
	CHECK(!ae_companion_rejoin_wanted(0, 1000));
	CHECK(!ae_companion_rejoin_wanted(1, AE_COMPANION_REJOIN_TICKS - 1));
	CHECK(ae_companion_rejoin_wanted(1, AE_COMPANION_REJOIN_TICKS));
	/* only the squad's own players leave */
	CHECK(ae_companion_may_remove(1, 0));
	CHECK(!ae_companion_may_remove(0, 0));	/* (a multiplayer bot on the squad's machine slot) */
	CHECK(!ae_companion_may_remove(1, 1));
	printf("ae_companion_rules: ok\n");
	return 0;
}
