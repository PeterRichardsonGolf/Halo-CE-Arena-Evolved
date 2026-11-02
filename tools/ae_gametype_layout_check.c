/*
AE_GAMETYPE_LAYOUT_CHECK.C

A check of where a gametype's rules live since OpenCE's network 36 (source/game/game_engine.h, source/saved
games/playlist_profile.c, playlist_display_name.h), built with the game's flags by tools/test_linux_port.py:

- OpenCE's PC options (struct game_variant_options) are 0x2C bytes, their first 0x1C bytes (GAME_VARIANT_OPTIONS_V1_SIZE)
  laid out as before OpenCE's gametypes, the mode and the rest of OpenCE's gametypes' options from 0x1C on: a saved
  gametype's version 1 options block (Arena Evolved 0.3.8 and before) is the same bytes at the same places;
- Arena Evolved's own rules are none of those bytes: they are the universal variant's flags from bit 6 (OpenCE's game
  uses bits 0-5) and its old pads (NHE MODE, STARTING GRENADES, BALL MELEE, WEAPON PACK, pad5, PACK LOADOUT), all in
  struct game_variant (0x68 bytes, unchanged), which the network game and a saved gametype carry as before;
- the saved gametype's 512 bytes: the variant and its signature, the 'GPVO' block (version 2: 0x100 to 0x148), then
  Arena Evolved's display name block at 0x150 (its legacy place, 0x140, after a version 1 block) inside the 512.

Prints the failures, or PASS.
*/

#define BUILDING_CSERIES
#include "cseries.h"
#include "game/game_engine.h"
#include "saved games/playlist_display_name.h"
#include "saved games/saved_game_files.h"

#include <stddef.h>
#include <stdio.h>

#undef printf

static int failures = 0;

static void check(int ok, char const *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

int main(void)
{
	/* (the options block's header and signature: playlist_profile.c) */
	unsigned long const header = 8, signature = 20, options_offset = 0x100;

	/* OpenCE's options: the old 0x1C bytes first, then OpenCE's gametypes' */
	check(sizeof(struct game_variant_options) == 0x2C, "the PC options are 0x2C bytes");
	check(GAME_VARIANT_OPTIONS_V1_SIZE == 0x1C, "version 1's options are 0x1C bytes");
	check(offsetof(struct game_variant_options, time_limit) == 0x00, "time limit at 0x00");
	check(offsetof(struct game_variant_options, vehicle_set) == 0x0A, "vehicle sets at 0x0A");
	check(offsetof(struct game_variant_options, loadout) == 0x18, "loadout at 0x18");
	check(offsetof(struct game_variant_options, no_map_weapons) == 0x1B, "no map weapons at 0x1B");
	check(offsetof(struct game_variant_options, mode) == GAME_VARIANT_OPTIONS_V1_SIZE, "OpenCE's mode at 0x1C");
	check(offsetof(struct game_variant_options, hardcore) == 0x1D, "hardcore at 0x1D");
	check(offsetof(struct game_variant_options, infection_initial_zombies) == 0x1E, "infection at 0x1E");
	check(offsetof(struct game_variant_options, gun_game_order) == 0x24, "gun game at 0x24");
	check(offsetof(struct game_variant_options, force_team_spawns) == 0x26, "OpenCE's FORCE TEAM SPAWNS at 0x26");
	check(offsetof(struct game_variant_options, reserved) == 0x27 &&
		sizeof(((struct game_variant_options *)0)->reserved) == 5, "OpenCE's reserved bytes 0x27-0x2B");

	/* Arena Evolved's rules: the variant's, unchanged */
	check(sizeof(struct game_variant) == 0x68, "the variant is 0x68 bytes");
	check(sizeof(struct universal_variant) == 0x30, "the universal variant is 0x30 bytes");
	check(offsetof(struct universal_variant, nhe_mode) == 0x01 && offsetof(struct universal_variant, starting_frags) == 0x02 &&
		offsetof(struct universal_variant, ball_melee) == 0x03, "NHE MODE, STARTING GRENADES, BALL MELEE: pads 0-2");
	check(offsetof(struct universal_variant, flags) == 0x04, "the flags at 0x04");
	check(offsetof(struct universal_variant, weapon_pack) == 0x0D && offsetof(struct universal_variant, pad5) == 0x0E &&
		offsetof(struct universal_variant, pack_loadout) == 0x0F, "WEAPON PACK, pad5, PACK LOADOUT: pads 4-6");
	check(_game_variant_generic_starting_equipment_bit == 5, "the game's own flags end at bit 5");
	check(_game_variant_sensor_range_first_bit == 6 && _game_variant_controller_crouch_bit == 8 &&
		_game_variant_vehicles_destructible_bit == 11 && _game_variant_pack_start_later_first_bit == 12,
		"Arena Evolved's flags from bit 6 (8 controller crouch, 11 vehicles destructible, 12-15 later packs)");
	check(_game_variant_drop_secondary_second_bit == 30, "Arena Evolved's flags end at bit 30 (31 reserved)");

	/* the saved gametype's 512 bytes */
	check(sizeof(struct game_variant) + signature <= options_offset, "the variant and its signature before 0x100");
	check(options_offset + header + sizeof(struct game_variant_options) + signature == 0x148,
		"the version 2 'GPVO' block ends at 0x148");
	check(PLAYLIST_DISPLAY_NAME_OFFSET >= 0x148, "the display name block after the version 2 'GPVO' block");
	check(PLAYLIST_DISPLAY_NAME_LEGACY_OFFSET >= options_offset + header + GAME_VARIANT_OPTIONS_V1_SIZE + signature,
		"the legacy display name block after a version 1 'GPVO' block");
	check(PLAYLIST_DISPLAY_NAME_LEGACY_OFFSET < 0x148,
		"a version 2 'GPVO' block runs over the legacy display name block's header (so it reads as none)");
	check(PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE <= SAVED_GAME_FILE_BLOCK_SIZE,
		"the display name block inside the 512 bytes");

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("PASS\n");
	return failures != 0;
}
