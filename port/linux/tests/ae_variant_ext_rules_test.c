#include <stdio.h>
#include <string.h>
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_variant_ext ext;

	/* zero is the gametype's own play, and needs no block */
	ae_variant_ext_default(&ext);
	CHECK(ae_variant_ext_is_default(&ext));
	CHECK(ae_variant_ext_sanitize(&ext));
	CHECK(ae_ext_rounds_count(&ext) == 0 && ae_ext_rounds_to_win(&ext) == 0);
	CHECK(ae_ext_round_time_seconds(&ext) == 0);
	CHECK(ae_ext_weapon_respawn_ticks(&ext, 900) == 900);
	CHECK(ae_ext_king_move_seconds(&ext) == 0);
	CHECK(ae_ext_control_seconds(&ext) == 5 && ae_ext_contest_seconds(&ext) == 5);

	/* rounds: 1/2/4/6 played, best of 3/5/7 to a majority */
	ext.rounds = AE_EXT_ROUNDS_4;
	CHECK(ae_ext_rounds_count(&ext) == 4 && ae_ext_rounds_to_win(&ext) == 0);
	ext.rounds = AE_EXT_ROUNDS_BEST_OF_5;
	CHECK(ae_ext_rounds_count(&ext) == 5 && ae_ext_rounds_to_win(&ext) == 3);
	ext.rounds = AE_EXT_ROUNDS_BEST_OF_7;
	CHECK(ae_ext_rounds_to_win(&ext) == 4);
	CHECK(!ae_variant_ext_is_default(&ext));
	/* (first to 5 of 9: the round modes') */
	ext.rounds = AE_EXT_ROUNDS_BEST_OF_9;
	CHECK(ae_ext_rounds_count(&ext) == 9 && ae_ext_rounds_to_win(&ext) == 5);
	CHECK(ae_variant_ext_sanitize(&ext));

	/* weapons come back never, in half or double the map's time */
	ext.weapon_respawn = AE_EXT_WEAPON_RESPAWN_NONE;
	CHECK(ae_ext_weapon_respawn_ticks(&ext, 900) == -1);
	ext.weapon_respawn = AE_EXT_WEAPON_RESPAWN_HALF;
	CHECK(ae_ext_weapon_respawn_ticks(&ext, 900) == 450);
	ext.weapon_respawn = AE_EXT_WEAPON_RESPAWN_DOUBLE;
	CHECK(ae_ext_weapon_respawn_ticks(&ext, 900) == 1800);

	/* traits */
	CHECK(ae_ext_damage_taken_percent(1) == 50 && ae_ext_damage_taken_percent(0) == 100);
	CHECK(ae_ext_damage_dealt_percent(1) == 200 && ae_ext_damage_dealt_percent(0) == 100);
	CHECK(ae_ext_movement_percent(AE_EXT_MOVEMENT_SLOW) == 75 && ae_ext_movement_percent(AE_EXT_MOVEMENT_FAST) == 125);
	CHECK(ae_ext_movement_percent(AE_EXT_MOVEMENT_GAMETYPE) == 100);

	/* respawn modifier: none; inheritance takes the waiting teammate's time when shorter; cycling waits for the
	next wave, at least a second away */
	CHECK(ae_ext_respawn_wait_ticks(AE_EXT_RESPAWN_MODIFIER_NONE, 150, 30, 1000) == 150);
	CHECK(ae_ext_respawn_wait_ticks(AE_EXT_RESPAWN_MODIFIER_INHERITANCE, 150, 30, 1000) == 30);
	CHECK(ae_ext_respawn_wait_ticks(AE_EXT_RESPAWN_MODIFIER_INHERITANCE, 150, -1, 1000) == 150);
	CHECK(ae_ext_respawn_wait_ticks(AE_EXT_RESPAWN_MODIFIER_CYCLING, 150, -1, 1000) == 50);
	CHECK(ae_ext_respawn_wait_ticks(AE_EXT_RESPAWN_MODIFIER_CYCLING, 150, -1, 1190) == 160);
	CHECK(ae_ext_respawn_wait_ticks(AE_EXT_RESPAWN_MODIFIER_CYCLING, 0, -1, 1140) == 0);

	/* waypoints */
	CHECK(ae_ext_waypoint_shows(AE_EXT_WAYPOINT_GAMETYPE, 0, 0, 0) == -1);
	CHECK(ae_ext_waypoint_shows(AE_EXT_WAYPOINT_ALWAYS, 1, 0, 1) == 1);
	CHECK(ae_ext_waypoint_shows(AE_EXT_WAYPOINT_LOOSE, 0, 0, 0) == 1);
	CHECK(ae_ext_waypoint_shows(AE_EXT_WAYPOINT_LOOSE, 1, 0, 0) == 0);
	CHECK(ae_ext_waypoint_shows(AE_EXT_WAYPOINT_LOOSE, 0, 0, 1) == 0);
	CHECK(ae_ext_waypoint_shows(AE_EXT_WAYPOINT_HELD, 0, 1, 0) == 1);
	CHECK(ae_ext_waypoint_shows(AE_EXT_WAYPOINT_OFF, 1, 1, 1) == 0);

	/* territories: the setting, never past the map's hills or 8 */
	ext.territories = 5;
	CHECK(ae_ext_territories_count(&ext, 8) == 5);
	CHECK(ae_ext_territories_count(&ext, 3) == 3);
	ext.territories = 0;
	CHECK(ae_ext_territories_count(&ext, 12) == 8);

	/* another machine's or a file's garbage: out of range is the default */
	memset(&ext, 0xFF, sizeof(ext));
	CHECK(!ae_variant_ext_sanitize(&ext));
	CHECK(ext.rounds == 0 && ext.round_time_10s == 0 && ext.mode == 0 && ext.territories == 0);
	CHECK(ext.match_flags == 0x07 && ext.player_flags == 0x1F && ext.bomb_flags == 0xFF);
	CHECK(ext.round_flags == AE_EXT_ROUND_FLAGS && ext.zone_wait_s == 0 && ext.zone_shrink_s == 0 &&
		ext.revive_10ths == 0);
	CHECK(ext.reserved[0] == 0);
	CHECK(ae_variant_ext_sanitize(&ext));
	ae_variant_ext_default(&ext);
	ext.max_active_players = 1;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.max_active_players == 0);
	ext.king_move_10s = 6;
	CHECK(ae_variant_ext_sanitize(&ext) && ae_ext_king_move_seconds(&ext) == 60);

	/* the round modes' bytes: the mode, their flags and times in range kept, out of range the default */
	ae_variant_ext_default(&ext);
	ext.mode = AE_EXT_MODE_ELIMINATION;
	ext.round_flags = AE_EXT_ROUND_NO_REVIVE | AE_EXT_ROUND_NO_ZONE;
	ext.zone_wait_s = 45;
	ext.zone_shrink_s = 20;
	ext.revive_10ths = 25;
	ext.bleedout_s = 30;
	CHECK(ae_variant_ext_sanitize(&ext));
	CHECK(ext.mode == AE_EXT_MODE_ELIMINATION && ext.zone_wait_s == 45 && ext.revive_10ths == 25 && ext.bleedout_s == 30);
	ext.bleedout_s = 4;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.bleedout_s == 0);
	ext.bleedout_s = 30;
	ext.zone_wait_s = 5;
	ext.revive_10ths = 101;
	ext.round_flags = 0x80;
	CHECK(!ae_variant_ext_sanitize(&ext));
	CHECK(ext.zone_wait_s == 0 && ext.revive_10ths == 0 && ext.round_flags == 0 && ext.zone_shrink_s == 20);
	/* (a build before the mode: an unknown mode is NONE) */
	ext.mode = AE_EXT_MODE_COUNT;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.mode == AE_EXT_MODE_NONE);

	if (failures)
		return 1;
	printf("ae_variant_ext_rules_test: ok\n");
	return 0;
}
