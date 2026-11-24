/*
SEED_RULES_TEST_LAUNCH.C (test)

A built-in gametype's AE block (the seed rules: SHOOTABLE GRENADES, the modern flag rules; a preset's table) through a TEST
RUN map test's launch, as the game runs it: network_test.c's network_test_saved_gametype finds the gametype built in, the
AE menus' test launch opens the GAME OPTIONS copy (ae_glue_setup_options.c's ae_setup_edit_begin_rules) and applies it
(ae_setup_edit_apply, whose player_ui_set_game_variant chooses the block by the stored name: ae_h2_presets.c's
ae_h2_preset_choose_variant, the hook in player_ui.c). The game then plays ae_variant_ext_game()'s settings. From
test_seed_rules_test_launch.py's generated includes.
*/

#include "harness.h"

#include <stdarg.h>
#include <wchar.h>

#include "ae_variant_ext_rules.h"
#include "ae_seed_ext_rules.h"
#include "rules.inc"

#define MAXIMUM_GAME_VARIANT_NAME_LENGTH 12
#define MAX_GAMENAME 128
#define AE_SAVED_GAMES_MAXIMUM 8
#define NUMBEROF(array) (sizeof(array) / sizeof((array)[0]))
#define csmemset memset

struct game_variant
{
	wchar_t human_readable_game_description[MAXIMUM_GAME_VARIANT_NAME_LENGTH];
	long game_engine_index;
};

struct game_variant_options
{
	short loadout;
};

struct ae_weapon_selection_bytes
{
	unsigned char bytes[4];
};

static void platform_log(char const *format, ...)
{
	(void)format;
}

/* ---------- the network (an offline game: the settings are the chosen ones) */

enum
{
	_game_connection_local,
	_game_connection_network_client,
	_game_connection_network_server,
};

static short game_connection(void)
{
	return _game_connection_local;
}

/* ---------- ae_variant_ext.c's state */

static struct ae_variant_ext chosen;
static struct ae_variant_ext received;

/* (Arena Evolved) the campaign hook (ae_campaign_score.c): no campaign level here */
static int campaign_level(void) { return 0; }
#include "ext.inc"

static int pending_profiles;

static void ae_variant_ext_pending_profile(long playlist_profile_index)
{
	(void)playlist_profile_index;
	pending_profiles++;
}

/* ---------- the built-in gametypes: AE CTF and AE TEAM SLY casual, AE COMP TS not, AE PRESET a NEW MODES preset with a table
of its own (best of 3) */

struct ae_h2_preset
{
	char const *stored;
};

enum { PRESET_INDEX = 3 };

static struct ae_h2_preset const preset = { "AE PRESET" };

static int ae_h2_preset_find_wide(wchar_t const *stored)
{
	return stored && !wcscmp(stored, L"AE PRESET") ? PRESET_INDEX : -1;
}

static struct ae_h2_preset const *ae_h2_preset_get(int index)
{
	return index == PRESET_INDEX ? &preset : NULL;
}

static void ae_h2_preset_ext(int index, struct ae_variant_ext *ext)
{
	ae_variant_ext_default(ext);
	if (index == PRESET_INDEX)
		ext->rounds = AE_EXT_ROUNDS_BEST_OF_3;
}

static unsigned int arena_gametype_seed_ext_flags(wchar_t const *stored)
{
	if (!wcscmp(stored, L"AE CTF"))
		return AE_SEED_EXT_AE_RULES | AE_SEED_EXT_CASUAL | AE_SEED_EXT_CTF;
	if (!wcscmp(stored, L"AE TEAM SLY"))
		return AE_SEED_EXT_AE_RULES | AE_SEED_EXT_CASUAL;
	if (!wcscmp(stored, L"AE PRESET"))
		return AE_SEED_EXT_AE_RULES | AE_SEED_EXT_CASUAL | AE_SEED_EXT_NEW_MODE;
	return 0;
}

static boolean arena_gametype_start_build(wchar_t const *stored, struct game_variant *variant,
	struct game_variant_options *options)
{
	if (wcscmp(stored, L"AE CTF") && wcscmp(stored, L"AE TEAM SLY") && wcscmp(stored, L"AE COMP TS") &&
		wcscmp(stored, L"AE PRESET"))
	{
		return FALSE;
	}
	memset(variant, 0, sizeof(*variant));
	memset(options, 0, sizeof(*options));
	wcsncpy(variant->human_readable_game_description, stored, MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1);
	variant->game_engine_index = !wcscmp(stored, L"AE CTF") ? 1 : 2;
	return TRUE;
}

#include "presets.inc"

/* ---------- the menus: player_ui.c's player_ui_set_game_variant, its AE hook */

static void player_ui_set_game_variant(struct game_variant *variant)
{
	ae_h2_preset_choose_variant(variant);
}

static void player_ui_set_game_variant_options(struct game_variant_options const *options)
{
	(void)options;
}

static void ae_weapon_selection_chosen_get(struct ae_weapon_selection_bytes *bytes)
{
	memset(bytes, 0, sizeof(*bytes));
}

static void ae_weapon_selection_choose_bytes(struct ae_weapon_selection_bytes const *bytes)
{
	(void)bytes;
}

static int ae_setup_current_map_is_nhe(void)
{
	return 0;
}

static short changes_now(void)
{
	return 0;
}

struct network_game_server;

static struct network_game_server *global_network_game_server_get(void)
{
	return NULL;
}

#define SERVER_STATE_PREGAME 1
#define network_game_server_get_state(server, unused) ((void)(server), 0)
#define network_game_server_change_game_variant(server, variant) ((void)(server), (void)(variant))
#define game_engine_override_game_variant(variant) ((void)(variant))

#include "setup.inc"

/* ---------- network_test.c: debug.network_test_gametype, no saved gametype of that name */

static struct
{
	char saved_gametype[32];
} network_test;

static short ui_widget_port_gametypes(long *indices, short maximum, short *last_used)
{
	(void)indices;
	(void)maximum;
	*last_used = 0;
	return 0;
}

static char const *arena_gametypes_migrated_name(char const *name)
{
	return name;
}

static boolean network_test_gametype_named(long profile_index, char const *wanted)
{
	(void)profile_index;
	(void)wanted;
	return FALSE;
}

#define playlist_profile_get(index, variant) ((void)(index), (void)(variant), FALSE)
#define playlist_profile_get_options(index, options) ((void)(index), (void)(options))

#include "network.inc"

/* a TEST RUN map test's game: its gametype found, the GAME OPTIONS copy opened and applied */
static struct ae_variant_ext const *launch(char const *name)
{
	struct game_variant variant;
	struct game_variant_options options;

	snprintf(network_test.saved_gametype, sizeof(network_test.saved_gametype), "%s", name);
	memset(&variant, 0, sizeof(variant));
	memset(&options, 0, sizeof(options));
	CHECK(network_test_saved_gametype(&variant, &options), "%s is found built in", name);
	CHECK(ae_setup_edit_begin_rules(&variant, &options), "the GAME OPTIONS copy of %s opens", name);
	CHECK(ae_setup_edit_apply(), "the GAME OPTIONS copy of %s is applied", name);
	return ae_variant_ext_game();
}

static int casual_ctf(struct ae_variant_ext const *ext)
{
	return ext->shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON && ext->ctf_return == AE_EXT_CTF_RETURN_H2 &&
		(ext->ctf_flags & AE_EXT_CTF_HOLD_PICKUP) && (ext->ctf_flags & AE_EXT_CTF_MASSIVE_DAMAGE) &&
		(ext->ctf_flags & AE_EXT_CTF_NO_DRIVING) && ext->ctf_own_flag_waypoint == AE_EXT_WAYPOINT_SPOTTED;
}

int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";
	struct ae_variant_ext const *ext;

	CASE("ctf")
	{
		ext = launch("AE CTF");
		CHECK(casual_ctf(ext), "AE CTF plays its seed rules: grenades %d, return %d, flags 0x%02x, waypoint %d",
			(int)ext->shootable_grenades, (int)ext->ctf_return, (unsigned)ext->ctf_flags, (int)ext->ctf_own_flag_waypoint);
	}
	else CASE("slayer")
	{
		ext = launch("AE TEAM SLY");
		CHECK(ext->shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON && !(ext->ctf_flags & AE_EXT_CTF_HOLD_PICKUP),
			"AE TEAM SLY plays shootable grenades and no flag rules: grenades %d, flags 0x%02x",
			(int)ext->shootable_grenades, (unsigned)ext->ctf_flags);
	}
	else CASE("comp")
	{
		ext = launch("AE COMP TS");
		CHECK(ae_variant_ext_is_default(ext), "AE COMP TS plays Halo CE's rules: grenades %d", (int)ext->shootable_grenades);
	}
	else CASE("again")
	{
		/* (the next test on the same gametype, loaded again: the name is the one chosen last) */
		launch("AE CTF");
		ext = launch("AE CTF");
		CHECK(casual_ctf(ext), "AE CTF loaded again keeps its seed rules: flags 0x%02x", (unsigned)ext->ctf_flags);
	}
	else CASE("after")
	{
		/* (a test after a game on another gametype's block, chosen in the menus: nothing of it carries over) */
		struct ae_variant_ext last;

		ae_h2_preset_ext_of_name(L"AE CTF", &last);
		ae_variant_ext_choose(&last);
		ext = launch("AE COMP TS");
		CHECK(ae_variant_ext_is_default(ext), "AE COMP TS after AE CTF plays Halo CE's rules: grenades %d, flags 0x%02x",
			(int)ext->shootable_grenades, (unsigned)ext->ctf_flags);
	}
	else CASE("preset")
	{
		ext = launch("AE PRESET");
		CHECK(ext->rounds == AE_EXT_ROUNDS_BEST_OF_3 && ext->shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON,
			"a casual preset plays its table and its seed rules: rounds %d, grenades %d", (int)ext->rounds,
			(int)ext->shootable_grenades);
	}
	else
	{
		fprintf(stderr, "unknown case %s\n", case_name);
		return 2;
	}
	CHECK(pending_profiles == 0, "no saved gametype's block waits (%d)", pending_profiles);
	return 0;
}
