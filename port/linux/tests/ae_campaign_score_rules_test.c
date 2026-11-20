#include <stdio.h>
#include <string.h>
#include "ae_campaign_score_rules.h"
#include "ae_campaign_medals_rules.h"
#include "ae_medals.h"
#include "campaign_score_values.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* actor_types.h's order, as the values' table has it */
enum { ELITE = 0, JACKAL, GRUNT, HUNTER, ENGINEER, ASSASSIN, PLAYER, MARINE, CREW, COMBAT, INFECTION, CARRIER, MONITOR, SENTINEL, NONE_TYPE, MOUNTED };

static struct campaign_score_values const *const v = &campaign_score_values;
static struct ae_campaign_score_log log_a, log_b;

static unsigned char const ONE = 1 << AE_CAMPAIGN_SCORE_STYLE_ONE_SHOT, HEAD = 1 << AE_CAMPAIGN_SCORE_STYLE_HEADSHOT,
	MELEE = 1 << AE_CAMPAIGN_SCORE_STYLE_MELEE, BACK = 1 << AE_CAMPAIGN_SCORE_STYLE_BACKSMACK;

static void points_tests(void)
{
	long major = ae_campaign_score_actor_points(v->enemies, v->enemy_count, ELITE, "elite major");
	long grunt = ae_campaign_score_actor_points(v->enemies, v->enemy_count, GRUNT, "grunt minor");

	CHECK(major == 40 && grunt == 10);
	/* (a type's default for a variant no word matches; a type not in the table: nothing) */
	CHECK(ae_campaign_score_actor_points(v->enemies, v->enemy_count, ELITE, "elite zealot") == 30);
	CHECK(ae_campaign_score_actor_points(v->enemies, v->enemy_count, MONITOR, "monitor") == 0);
	CHECK(ae_campaign_score_actor_points(v->enemies, v->enemy_count, MARINE, "marine") == 0);
	/* (allies: negative, a captain's more) */
	CHECK(ae_campaign_score_actor_points(v->allies, v->ally_count, MARINE, "marine") == -50);
	CHECK(ae_campaign_score_actor_points(v->allies, v->ally_count, MARINE, "marine captain") == -100);
	CHECK(ae_campaign_score_match_points(v->vehicles, v->vehicle_count, "vehicles\\wraith\\wraith") == 200);
	CHECK(ae_campaign_score_match_points(v->vehicles, v->vehicle_count, "vehicles\\warthog") == 0);
	/* the style: regular 1, one shot 1.25, backsmack 1.5 and not a melee too; styles multiply */
	CHECK(ae_campaign_score_style_multiplier(v, 0) == 1.0f && ae_campaign_score_style_multiplier(v, ONE) == 1.25f);
	CHECK(ae_campaign_score_style_multiplier(v, BACK) == 1.5f);
	CHECK(ae_campaign_score_style_multiplier(v, ONE | HEAD) == 1.25f * 1.25f);
	/* points = base x style x difficulty x skulls, rounded: an elite major, one shot + headshot, on each difficulty */
	CHECK(ae_campaign_score_kill_points(v, 40, ONE | HEAD, 0, 1.0f) == 31);
	CHECK(ae_campaign_score_kill_points(v, 40, ONE | HEAD, 1, 1.0f) == 63);
	CHECK(ae_campaign_score_kill_points(v, 40, ONE | HEAD, 2, 1.0f) == 125);
	CHECK(ae_campaign_score_kill_points(v, 40, ONE | HEAD, 3, 1.0f) == 250);
	CHECK(ae_campaign_score_kill_points(v, 10, 0, 1, 1.0f) == 10 && ae_campaign_score_kill_points(v, 10, MELEE, 1, 1.0f) == 13);
	/* a difficulty out of range is pinned; nothing scores for a base of 0 or less */
	CHECK(ae_campaign_score_kill_points(v, 10, 0, 9, 1.0f) == 40 && ae_campaign_score_kill_points(v, 10, 0, -3, 1.0f) == 5);
	CHECK(ae_campaign_score_kill_points(v, 0, HEAD, 1, 1.0f) == 0 && ae_campaign_score_kill_points(v, -50, 0, 1, 1.0f) == 0);
	/* the skulls' multiplier (the seam: 1.0 today): it scales a kill as a skull on does */
	CHECK(ae_campaign_score_kill_points(v, 10, 0, 1, 1.5f) == 15 && ae_campaign_score_kill_points(v, 10, 0, 1, 1.0f) == 10);
	/* medals: the values' points, times the difficulty when scaled */
	CHECK(ae_campaign_score_medal_points(v, "double_kill", 1, 1.0f) == 25);
	CHECK(ae_campaign_score_medal_points(v, "double_kill", 3, 1.0f) == 100);
	CHECK(ae_campaign_score_medal_points(v, "nonesuch", 1, 1.0f) == 0 && ae_campaign_score_medal_points(v, NULL, 1, 1.0f) == 0);
	/* an enemy the table does not know: the stated default when it is hostile, nothing when it is not */
	CHECK(ae_campaign_score_unknown_points(1) == AE_CAMPAIGN_SCORE_UNKNOWN_POINTS && ae_campaign_score_unknown_points(0) == 0);
	/* the revert's cost: the values' (negative), none when it is off */
	CHECK(ae_campaign_score_revert_points(v, 1) == -25 && ae_campaign_score_revert_points(v, 0) == 0);
}

static void time_tests(void)
{
	long par = ae_campaign_score_par_seconds_of_level(v, "levels\\A10\\A10", 1);

	CHECK(par == 900 && ae_campaign_score_par_seconds_of_level(v, "levels\\a10\\a10", 0) == 720);
	CHECK(ae_campaign_score_par_seconds_of_level(v, "levels\\a10\\a10", 3) == 1620);
	CHECK(ae_campaign_score_par_seconds_of_level(v, "levels\\a10\\a10", 9) == 1620);
	CHECK(ae_campaign_score_par_seconds_of_level(v, "mcc_maps\\custom\\custom", 1) == -1 &&
		ae_campaign_score_par_seconds_of_level(v, NULL, 1) == -1);
	/* a level with no par time: x1 and no fraction */
	CHECK(ae_campaign_score_time_multiplier_of(v, -1, 999999) == 1.0f && ae_campaign_score_par_fraction_of(-1, 5) < 0.0f);
	/* the brackets, by the time over the par time: bonus under, penalty over */
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 450 * 30) == 4.0f);   /* 0.5 */
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 540 * 30) == 3.0f);   /* 0.6 */
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 720 * 30) == 2.0f);   /* 0.8 */
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 900 * 30) == 1.0f);   /* at par */
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 900 * 30 + 1) == 0.8f);
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 1350 * 30) == 0.8f);  /* 1.5 */
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 5000 * 30) == 0.5f);
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, 0) == 4.0f);
	/* the final score: the multiplier on points above nothing; a penalty total is left as it is */
	CHECK(ae_campaign_score_final(1000, 4.0f) == 4000 && ae_campaign_score_final(101, 0.5f) == 51);
	CHECK(ae_campaign_score_final(-75, 4.0f) == -75 && ae_campaign_score_final(0, 4.0f) == 0);
}

static void panel_tests(void)
{
	long tick = 0;
	float next = 0.0f;
	struct ae_campaign_score_summary sum;

	CHECK(ae_campaign_score_par_ticks_of(900) == 27000 && ae_campaign_score_par_ticks_of(-1) == -1 && ae_campaign_score_par_ticks_of(0) == -1);
	/* the multiplier holds up to and including the bracket's limit: it drops on the next tick */
	CHECK(ae_campaign_score_next_drop_of(v, 900, 0, &tick, &next) == 1 && tick == 13501 && next == 3.0f);
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, tick - 1) == 4.0f && ae_campaign_score_time_multiplier_of(v, 900, tick) == 3.0f);
	CHECK(ae_campaign_score_next_drop_of(v, 900, 27000, &tick, &next) == 1 && tick == 27001 && next == 0.8f);
	CHECK(ae_campaign_score_time_multiplier_of(v, 900, tick - 1) == 1.0f && ae_campaign_score_time_multiplier_of(v, 900, tick) == 0.8f);
	CHECK(ae_campaign_score_next_drop_of(v, 900, 27001 + 13000, &tick, &next) == 1 && tick == 40501 && next == 0.5f);
	CHECK(ae_campaign_score_next_drop_of(v, 900, 40501, &tick, &next) == 0 && ae_campaign_score_next_drop_of(v, -1, 0, &tick, &next) == 0);
	ae_campaign_score_log_reset(&log_a);
	ae_campaign_score_log_append(&log_a, 1, AE_CAMPAIGN_SCORE_ENTRY_KILL, 1, 0, 100, HEAD | ONE);
	ae_campaign_score_log_append(&log_a, 2, AE_CAMPAIGN_SCORE_ENTRY_KILL, 1, 1, 100, MELEE);
	ae_campaign_score_log_append(&log_a, 3, AE_CAMPAIGN_SCORE_ENTRY_KILL, 1, 2, 30, BACK);
	ae_campaign_score_log_append(&log_a, 4, AE_CAMPAIGN_SCORE_ENTRY_DEATH, -1, 2, -25, 0);
	ae_campaign_score_log_append(&log_a, 5, AE_CAMPAIGN_SCORE_ENTRY_REVERT, -1, 0, -25, 0);
	ae_campaign_score_log_append(&log_a, 6, AE_CAMPAIGN_SCORE_ENTRY_ALLY, 2, 0, -50, 0);
	ae_campaign_score_log_append(&log_a, 7, AE_CAMPAIGN_SCORE_ENTRY_MEDAL, 0, 1, 25, 0);
	/* places: slot 1 has 125, slot 0 has 25, slot 2 has 5, slot 3 none; equal points share a place */
	CHECK(ae_campaign_score_place(&log_a, 1) == 1 && ae_campaign_score_place(&log_a, 0) == 2 && ae_campaign_score_place(&log_a, 2) == 3);
	CHECK(ae_campaign_score_place(&log_a, 3) == 0 && ae_campaign_score_place(&log_a, -1) == 0 && ae_campaign_score_place(&log_a, 99) == 0);
	ae_campaign_score_log_append(&log_a, 8, AE_CAMPAIGN_SCORE_ENTRY_KILL, 1, 3, 5, 0);
	CHECK(ae_campaign_score_place(&log_a, 2) == 3 && ae_campaign_score_place(&log_a, 3) == 3);
	ae_campaign_score_summarize(&log_a, 0, &sum);
	CHECK(sum.kills == 1 && sum.headshots == 1 && sum.one_shot_kills == 1 && sum.reverts == 1 && sum.allies_killed == 1 && sum.points == 25 &&
		sum.kill_points == 100 && sum.penalty_points == -75 && sum.penalty_count == 2 && sum.revert_points == -25 && sum.ally_points == -50);
	ae_campaign_score_summarize(&log_a, -1, &sum);
	CHECK(sum.kills == 4 && sum.melee_kills == 1 && sum.backsmack_kills == 1 && sum.deaths == 1 && sum.medals == 1 && sum.medal_points == 25 &&
		sum.points == ae_campaign_score_log_team_total(&log_a) && sum.penalty_points == -100 && sum.penalty_count == 3 && sum.death_points == -25);
}

static void log_tests(void)
{
	ae_campaign_score_log_reset(&log_a);
	CHECK(ae_campaign_score_log_append(&log_a, 10, AE_CAMPAIGN_SCORE_ENTRY_KILL, 7, 0, 100, ONE));
	CHECK(ae_campaign_score_log_append(&log_a, 12, AE_CAMPAIGN_SCORE_ENTRY_KILL, 7, 1, 40, HEAD));
	CHECK(ae_campaign_score_log_append(&log_a, 20, AE_CAMPAIGN_SCORE_ENTRY_DEATH, -1, 1, -25, 0));
	CHECK(ae_campaign_score_log_append(&log_a, 25, AE_CAMPAIGN_SCORE_ENTRY_REVERT, -1, 0, -25, 0));
	CHECK(ae_campaign_score_log_append(&log_a, 30, AE_CAMPAIGN_SCORE_ENTRY_MEDAL, AE_CAMPAIGN_MEDAL_DOUBLE_KILL, 0, 25, 0));
	/* co-op: one shared score, or each player's own; the revert's cost is a player's */
	CHECK(ae_campaign_score_log_total(&log_a, 0) == 100 - 25 + 25 && ae_campaign_score_log_total(&log_a, 1) == 15);
	CHECK(ae_campaign_score_log_team_total(&log_a) == 115);
	CHECK(ae_campaign_score_log_points(&log_a, 0, 0) == 115 && ae_campaign_score_log_points(&log_a, 1, 0) == 115);
	CHECK(ae_campaign_score_log_points(&log_a, 0, 1) == 100 && ae_campaign_score_log_points(&log_a, 1, 1) == 15);
	CHECK(ae_campaign_score_log_points(&log_a, -1, 1) == 115);
	CHECK(ae_campaign_score_log_count(&log_a, 1, AE_CAMPAIGN_SCORE_ENTRY_DEATH) == 1 &&
		ae_campaign_score_log_count(&log_a, -1, AE_CAMPAIGN_SCORE_ENTRY_KILL) == 2);
	CHECK(ae_campaign_score_log_style_count(&log_a, -1, AE_CAMPAIGN_SCORE_STYLE_HEADSHOT) == 1 &&
		ae_campaign_score_log_style_count(&log_a, 0, AE_CAMPAIGN_SCORE_STYLE_HEADSHOT) == 0);
	CHECK(ae_campaign_score_type_competitive("competitive") && !ae_campaign_score_type_competitive("cooperative") &&
		!ae_campaign_score_type_competitive(NULL) && !ae_campaign_score_type_competitive(""));
	/* a slot or a kind out of range, and a full log, are refused */
	CHECK(!ae_campaign_score_log_append(&log_a, 0, AE_CAMPAIGN_SCORE_ENTRY_KILL, 0, -1, 1, 0));
	CHECK(!ae_campaign_score_log_append(&log_a, 0, AE_CAMPAIGN_SCORE_ENTRY_KILL, 0, AE_CAMPAIGN_SCORE_PLAYERS, 1, 0));
	CHECK(!ae_campaign_score_log_append(&log_a, 0, AE_CAMPAIGN_SCORE_ENTRY_COUNT, 0, 0, 1, 0));
	ae_campaign_score_log_reset(&log_b);
	while (log_b.count < AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES)
		CHECK(ae_campaign_score_log_append(&log_b, 1, AE_CAMPAIGN_SCORE_ENTRY_KILL, 0, 0, 1, 0));
	CHECK(!ae_campaign_score_log_append(&log_b, 1, AE_CAMPAIGN_SCORE_ENTRY_KILL, 0, 0, 1, 0) &&
		log_b.count == AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES);
}

static unsigned char bytes[AE_CAMPAIGN_SCORE_FILE_MAXIMUM_SIZE + 64];

static void file_tests(void)
{
	unsigned long level = ae_campaign_score_name_hash("levels\\a10\\a10");
	long size, index;
	int competitive = 0;
	char const *why = "";

	log_a.ticks = 4321;
	size = ae_campaign_score_file_encode(&log_a, 0xDEADBEEFUL, level, 1, bytes, sizeof(bytes));
	CHECK(size == AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + 5 * AE_CAMPAIGN_SCORE_FILE_ENTRY_SIZE + AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE);
	/* it reads back whole, negative points and times too */
	ae_campaign_score_log_reset(&log_b);
	CHECK(ae_campaign_score_file_decode(bytes, size, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 0 && !strcmp(why, ""));
	CHECK(competitive == 1 && log_b.count == 5 && log_b.ticks == 4321);
	CHECK(log_b.entries[2].points == -25 && log_b.entries[2].game_time == 20 && log_b.entries[2].variant_index == -1 &&
		log_b.entries[2].player_slot == 1 && log_b.entries[2].kind == AE_CAMPAIGN_SCORE_ENTRY_DEATH);
	CHECK(ae_campaign_score_log_team_total(&log_b) == 115 && log_b.entries[4].kind == AE_CAMPAIGN_SCORE_ENTRY_MEDAL);
	/* the level's name is hashed without its case or its separator */
	CHECK(ae_campaign_score_name_hash("LEVELS/A10/A10") == level && ae_campaign_score_name_hash("levels\\a30\\a30") != level);
	/* refused, with a reason, the log left empty */
	CHECK(ae_campaign_score_file_decode(bytes, size, 0xDEADBEF0UL, level, &log_b, &competitive, &why) == 1 && strstr(why, "another saved game") && log_b.count == 0);
	CHECK(ae_campaign_score_file_decode(bytes, size, 0xDEADBEEFUL, level + 1, &log_b, &competitive, &why) == 1 && strstr(why, "another level"));
	CHECK(ae_campaign_score_file_decode(bytes, size - 1, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 1 && strstr(why, "size"));
	CHECK(ae_campaign_score_file_decode(bytes, 10, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 1 && strstr(why, "short"));
	CHECK(ae_campaign_score_file_decode(bytes, 0, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 1);
	CHECK(ae_campaign_score_file_decode(bytes, AE_CAMPAIGN_SCORE_FILE_MAXIMUM_SIZE + 1, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 1 &&
		strstr(why, "bigger"));
	/* (every single flipped byte of the file is refused: the checksum, the range checks or the binding catch it) */
	for (index = 0; index < size; index++)
	{
		bytes[index] ^= 0x01;
		CHECK(ae_campaign_score_file_decode(bytes, size, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 1 && log_b.count == 0);
		bytes[index] ^= 0x01;
	}
	CHECK(ae_campaign_score_file_decode(bytes, size, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 0);
	/* a count that does not fit the size, even with a good checksum of it, is refused: no read past the buffer */
	bytes[20] = 0xFF; bytes[21] = 0xFF; bytes[22] = 0xFF; bytes[23] = 0x7F;
	CHECK(ae_campaign_score_file_decode(bytes, size, 0xDEADBEEFUL, level, &log_b, &competitive, &why) == 1 && strstr(why, "too many"));
	/* a log with nothing in it is a good file too; a buffer too small writes nothing */
	ae_campaign_score_log_reset(&log_b);
	CHECK(ae_campaign_score_file_encode(&log_b, 1, level, 0, bytes, sizeof(bytes)) ==
		AE_CAMPAIGN_SCORE_FILE_HEADER_SIZE + AE_CAMPAIGN_SCORE_FILE_TRAILER_SIZE);
	CHECK(ae_campaign_score_file_encode(&log_a, 1, level, 0, bytes, 40) == 0);
	/* a full log is the biggest file there is, and reads back */
	ae_campaign_score_log_reset(&log_b);
	while (log_b.count < AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES)
		ae_campaign_score_log_append(&log_b, 1, AE_CAMPAIGN_SCORE_ENTRY_KILL, 0, 15, -1, 0);
	size = ae_campaign_score_file_encode(&log_b, 7, level, 0, bytes, sizeof(bytes));
	CHECK(size == AE_CAMPAIGN_SCORE_FILE_MAXIMUM_SIZE);
	CHECK(ae_campaign_score_file_decode(bytes, size, 7, level, &log_a, &competitive, &why) == 0 && log_a.count == AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES);
}

static int has(struct ae_campaign_medal_award const *awards, int count, int medal)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (awards[index].medal == medal)
			return 1;
	}
	return 0;
}

static struct ae_campaign_kill kill_at(long ticks, long victim)
{
	struct ae_campaign_kill kill;

	memset(&kill, 0, sizeof(kill));
	kill.killer_slot = 0;
	kill.victim_unit = victim;
	kill.killer_object = -1;
	kill.ticks = ticks;
	kill.weapon = -1;
	kill.wheelman_slot = -1;
	return kill;
}

static void medal_tests(void)
{
	static struct ae_campaign_medals_state state;
	struct ae_campaign_medal_award awards[AE_CAMPAIGN_MEDALS_MOST_PER_KILL];
	struct ae_campaign_kill kill;
	int count, index;

	/* the weapons, by the damage effect's name's words */
	CHECK(ae_campaign_medals_weapon(v, "weapons\\sniper rifle\\sniper bullet") == 0);
	CHECK(ae_campaign_medals_weapon(v, "Weapons\\Shotgun\\Pellet") == 1 && ae_campaign_medals_weapon(v, "globals\\vehicle_killed_unit") == 4);
	CHECK(ae_campaign_medals_weapon(v, "weapons\\assault rifle\\bullet") == -1 && ae_campaign_medals_weapon(v, NULL) == -1);
	/* the first kill: First Strike; a second within the window: Double Kill; a third: Triple Kill */
	ae_campaign_medals_reset(&state);
	kill = kill_at(100, 1);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(count == 1 && awards[0].medal == AE_CAMPAIGN_MEDAL_FIRST_STRIKE && awards[0].slot == 0);
	kill = kill_at(100 + 3 * 30, 2);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(count == 1 && awards[0].medal == AE_CAMPAIGN_MEDAL_DOUBLE_KILL);
	kill = kill_at(100 + 6 * 30, 3);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(count == 1 && awards[0].medal == AE_CAMPAIGN_MEDAL_TRIPLE_KILL);
	/* outside the window: the multi-kill starts over; the fifth kill without dying: Killing Spree */
	kill = kill_at(100 + 30 * 30, 4);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(count == 0);
	kill = kill_at(100 + 31 * 30, 5);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_DOUBLE_KILL) && has(awards, count, AE_CAMPAIGN_MEDAL_KILLING_SPREE));
	/* a death ends the sprees and the multi-kill */
	ae_campaign_medals_player_died(&state, 0, 77);
	kill = kill_at(100 + 32 * 30, 6);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(count == 0 && state.players[0].spree == 1);
	/* Revenge: the unit that killed the player, killed; Avenger: the one that killed another player, killed by this one */
	kill = kill_at(2000, 77);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_REVENGE) && !has(awards, count, AE_CAMPAIGN_MEDAL_AVENGER));
	ae_campaign_medals_player_died(&state, 1, 88);
	kill = kill_at(5000, 88);
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_AVENGER) && !has(awards, count, AE_CAMPAIGN_MEDAL_REVENGE));
	/* a kill's style, its weapon's, the killer's state, and a driver's Wheelman */
	ae_campaign_medals_reset(&state);
	kill = kill_at(1, 1);
	kill.headshot = 1; kill.weapon = 0; kill.wheelman_slot = 2; kill.killer_dead = 1;
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_HEADSHOT) && has(awards, count, AE_CAMPAIGN_MEDAL_SNIPER_KILL) &&
		has(awards, count, AE_CAMPAIGN_MEDAL_DEATH_FROM_THE_GRAVE) && !has(awards, count, AE_CAMPAIGN_MEDAL_CLOSE_CALL));
	for (index = 0; index < count; index++)
		CHECK(awards[index].medal != AE_CAMPAIGN_MEDAL_WHEELMAN || awards[index].slot == 2);
	CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_WHEELMAN));
	kill = kill_at(2, 2);
	kill.melee = 1; kill.killer_nearly_dead = 1;
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_BEAT_DOWN) && has(awards, count, AE_CAMPAIGN_MEDAL_CLOSE_CALL));
	kill = kill_at(3, 3);
	kill.melee = 1; kill.backsmack = 1; kill.weapon = 0;
	count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
	/* (a melee blow has no weapon's medal; a backsmack is an Assassin, not a Beat Down) */
	CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_ASSASSIN) && !has(awards, count, AE_CAMPAIGN_MEDAL_BEAT_DOWN) &&
		!has(awards, count, AE_CAMPAIGN_MEDAL_SNIPER_KILL));
	/* a weapon's spree: the fifth and the tenth kill of a shotgun */
	ae_campaign_medals_reset(&state);
	for (index = 1; index <= 10; index++)
	{
		kill = kill_at(index * 1000, index);
		kill.weapon = 1;
		count = ae_campaign_medals_kill(&state, v, &kill, awards, 16);
		CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_SHOTGUN_SPREE) == (index == 5));
		CHECK(has(awards, count, AE_CAMPAIGN_MEDAL_OPEN_SEASON) == (index == 10));
	}
	/* a slot out of range earns nothing, and a short output is not overrun */
	kill = kill_at(1, 1);
	kill.killer_slot = AE_CAMPAIGN_MEDALS_PLAYERS;
	CHECK(ae_campaign_medals_kill(&state, v, &kill, awards, 16) == 0);
	ae_campaign_medals_reset(&state);
	kill = kill_at(1, 1);
	kill.headshot = 1; kill.melee = 1;
	CHECK(ae_campaign_medals_kill(&state, v, &kill, awards, 1) == 1);
	/* Perfection: no deaths and the values' kills */
	CHECK(ae_campaign_medals_perfection(v, 0, v->medal_perfection_kills) && !ae_campaign_medals_perfection(v, 1, 99) &&
		!ae_campaign_medals_perfection(v, 0, v->medal_perfection_kills - 1));
	/* the table: every medal has a key and a label, and the AE number is of ae_medal_ids.h's or -1 */
	for (index = 0; index < AE_CAMPAIGN_MEDAL_COUNT; index++)
	{
		CHECK(ae_campaign_medal_descriptions[index].key && ae_campaign_medal_descriptions[index].label);
		CHECK(ae_campaign_medal_descriptions[index].ae_medal >= -1 && ae_campaign_medal_descriptions[index].ae_medal < AE_MEDAL_COUNT);
	}
	count = 0;
	for (index = 0; index < AE_CAMPAIGN_MEDAL_COUNT; index++)
		count += ae_campaign_medal_descriptions[index].ae_medal < 0;
	CHECK(count == 3 && ae_campaign_medal_descriptions[AE_CAMPAIGN_MEDAL_STICK_SPREE].ae_medal == -1 &&
		ae_campaign_medal_descriptions[AE_CAMPAIGN_MEDAL_ASSASSIN].ae_medal == -1 &&
		ae_campaign_medal_descriptions[AE_CAMPAIGN_MEDAL_INCINERATION].ae_medal == -1);
	/* a medal's key is the values' key */
	for (index = 0; index < AE_CAMPAIGN_MEDAL_COUNT; index++)
		CHECK(ae_campaign_score_medal_points(v, ae_campaign_medal_descriptions[index].key, 0, 1.0f) > 0);
}

int main(void)
{
	points_tests();
	time_tests();
	log_tests();
	file_tests();
	panel_tests();
	medal_tests();
	if (failures)
		return 1;
	printf("ae_campaign_score_rules_test: ok\n");
	return 0;
}
