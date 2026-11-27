/* Arena Evolved joining a Content Evolved host: the signature's classes, the long record's size, the record made this
game's, the Firefight options read out of it, and that this game's own sizes and versions are what they were
(port/linux/game/ae_ce_host_rules.c). The fixtures are laid out as the reference's struct game_variant_options is. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ae_ce_host_rules.h"
#include "../include/halo_port_limits.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the reference's struct game_variant_options (source/game/game_engine.h of the Content Evolved build), 0x58 bytes */
struct reference_options
{
	int16_t time_limit;
	int16_t friendly_fire;
	int16_t friendly_fire_penalty;
	int16_t vehicle_respawn_time;
	uint8_t auto_team_balance;
	uint8_t radar_players;
	uint8_t vehicle_set[2];
	uint8_t vehicle_counts[2][6];
	uint8_t loadout;
	uint8_t primary_weapon;
	uint8_t secondary_weapon;
	uint8_t no_map_weapons;
	uint8_t mode;
	uint8_t hardcore;
	uint8_t infection[6];
	uint8_t gun_game_order;
	uint8_t gun_game_melee_demotes;
	uint8_t force_team_spawns;
	uint8_t floodfection[4];
	uint8_t mode_variant;
	uint32_t firefight_custom_skulls;
	int16_t firefight_generator_health;
	int16_t firefight_score_life_threshold;
	uint8_t firefight_enemy_health_scale;
	uint8_t firefight_enemy_shield_scale;
	uint8_t firefight_bonus_seconds;
	uint8_t firefight_faction_diplomacy;
	uint32_t firefight_tag_profile;
	uint32_t firefight_starting_skulls[2];
	uint16_t firefight_unit_toggles;
	uint8_t firefight_set_escalation;
	uint8_t firefight_dropship_mode;
	uint8_t firefight_next_wave_mode;
	uint8_t firefight_player_faction_mode;
	uint8_t firefight_vehicle_spawns;
	uint8_t firefight_plasma_flinch;
	uint8_t firefight_unused_vehicle;
	uint8_t firefight_prep_countdown;
	uint8_t firefight_skull_progression_mode;
	uint8_t firefight_player_shields;
	uint8_t firefight_player_speed;
	uint8_t firefight_player_gravity;
	uint8_t firefight_infinite_ammo;
	uint8_t firefight_map_content;
	uint8_t firefight_wave_size_scale;
	uint8_t firefight_boss_wave_mode;
	uint8_t firefight_enemy_loadout;
	uint8_t firefight_friendly_ai;
};

typedef char verify_reference_size[sizeof(struct reference_options) == 0x58 ? 1 : -1];

/* where the pieces sit in a record at the network's slots (the record's header holds the engine index) */
#define PLAYERS_END HALO_PORT_NETWORK_GAME_PLAYERS_END
#define VARIANT_OPTIONS (PLAYERS_END + 0xA)
#define ENGINE_OFFSET 0x28
#define AE_ENGINE_FIREFIGHT 7

static struct ae_ce_host_layout layout(void)
{
	struct ae_ce_host_layout l;

	l.ae_size = HALO_PORT_NETWORK_GAME_SIZE;
	l.variant_options = VARIANT_OPTIONS;
	l.engine_index = ENGINE_OFFSET;
	l.engine_index_size = 4;
	l.ae_engine_firefight = AE_ENGINE_FIREFIGHT;
	l.ae_jump_percent = offsetof(struct reference_options, floodfection);
	return l;
}

static unsigned char ce_record[HALO_PORT_NETWORK_GAME_SIZE + 0x2C];
static unsigned char ae_record[HALO_PORT_NETWORK_GAME_SIZE];

static void make_ce_record(void)
{
	struct reference_options options;
	unsigned long index;

	for (index = 0; index < sizeof(ce_record); index++)
		ce_record[index] = (unsigned char)(index * 31 + 7);
	/* (the engine index, a long: CE's Firefight is 6) */
	memset(ce_record + ENGINE_OFFSET, 0, 4);
	ce_record[ENGINE_OFFSET] = 6;
	memset(&options, 0, sizeof(options));
	options.time_limit = 15;
	options.friendly_fire = 1;
	options.loadout = 2;
	options.primary_weapon = 3;
	options.secondary_weapon = 4;
	options.force_team_spawns = 1;
	options.floodfection[0] = 0x55;
	options.floodfection[3] = 0x66;
	options.mode_variant = 2;
	options.firefight_custom_skulls = 0x01020304;
	options.firefight_generator_health = 1500;
	options.firefight_score_life_threshold = 300;
	options.firefight_enemy_health_scale = 3;
	options.firefight_enemy_shield_scale = 2;
	options.firefight_bonus_seconds = 45;
	options.firefight_faction_diplomacy = 1;
	options.firefight_tag_profile = 0;
	options.firefight_starting_skulls[0] = 0x0A0B0C0D;
	options.firefight_starting_skulls[1] = 0x80000001;
	options.firefight_unit_toggles = 0x1234;
	options.firefight_set_escalation = 1;
	options.firefight_dropship_mode = 2;
	options.firefight_next_wave_mode = 3;
	options.firefight_player_faction_mode = 4;
	options.firefight_vehicle_spawns = 5;
	options.firefight_plasma_flinch = 0x82;
	options.firefight_unused_vehicle = 6;
	options.firefight_prep_countdown = 7;
	options.firefight_skull_progression_mode = 8;
	options.firefight_player_shields = 9;
	options.firefight_player_speed = 10;
	options.firefight_player_gravity = 11;
	options.firefight_infinite_ammo = 12;
	options.firefight_map_content = 13;
	options.firefight_wave_size_scale = 14;
	options.firefight_boss_wave_mode = 15;
	options.firefight_enemy_loadout = 16;
	options.firefight_friendly_ai = 17;
	memcpy(ce_record + VARIANT_OPTIONS, &options, sizeof(options));
}

static void test_this_games_numbers_are_what_they_were(void)
{
	struct ae_ce_host_layout l = layout();

	/* (nothing of what this game advertises, sends or accepts moves: its network version, its range, its record) */
	AE_CHECK(HALO_PORT_NETWORK_VERSION == 44);
	AE_CHECK(HALO_PORT_NETWORK_VERSION_MINIMUM == 44);
	AE_CHECK(HALO_PORT_NETWORK_VERSION_MAXIMUM == 44);
	AE_CHECK(HALO_PORT_NETWORK_GAME_SIZE == PLAYERS_END + 0x3A + 64);
	AE_CHECK(HALO_PORT_NETWORK_GAME_SIZE == 13200);
	AE_CHECK(HALO_PORT_NETWORK_GAME_LOCAL_DATA_OFFSET == VARIANT_OPTIONS + AE_CE_HOST_OPTIONS_AE_SIZE);
	/* (the reference's record: PLAYERS_END + 0x66 + 64) */
	AE_CHECK(ae_ce_host_record_size(&l) == PLAYERS_END + 0x66 + 64);
	AE_CHECK(ae_ce_host_record_size(&l) == HALO_PORT_NETWORK_GAME_SIZE + 0x2C);
	AE_CHECK(ae_ce_host_record_size(&l) != HALO_PORT_NETWORK_GAME_SIZE);
	AE_CHECK(ae_ce_host_record_size(NULL) == 0);
}

static void test_classes(void)
{
	unsigned ours = HALO_PORT_NETWORK_VERSION;
	unsigned flags;

	/* a plain host of any version is no CE host, whatever its flags: every rule of this game stays */
	for (flags = 0; flags < 256; flags++)
	{
		AE_CHECK(ae_ce_host_classify(44, 0, flags, ours) == AE_CE_HOST_NOT_CE);
		AE_CHECK(ae_ce_host_classify(44, 44, flags, ours) == AE_CE_HOST_NOT_CE);
		AE_CHECK(ae_ce_host_classify(38, 0, flags, ours) == AE_CE_HOST_NOT_CE);
		AE_CHECK(ae_ce_host_classify(0, 0, flags, ours) == AE_CE_HOST_NOT_CE);
		AE_CHECK(ae_ce_host_classify(0x4777, 44, flags, ours) == AE_CE_HOST_NOT_CE); /* a Forge host's */
		AE_CHECK(ae_ce_host_classify(0xFEFF, 44, flags, ours) == AE_CE_HOST_NOT_CE);
	}
	AE_CHECK(!ae_ce_host_signed(44) && !ae_ce_host_signed(0x4777) && !ae_ce_host_signed(0xFEFF) && !ae_ce_host_signed(0x1FF00));
	AE_CHECK(ae_ce_host_signed(0xFF00 | 112) && ae_ce_host_mod_of(0xFF00 | 112) == 112);
	/* CE's Firefight host: its mod version, this network version */
	AE_CHECK(ae_ce_host_classify(0xFF00 | 112, 44, 0x01, ours) == AE_CE_HOST_JOIN);
	AE_CHECK(ae_ce_host_classify(0xFF00 | 112, 44, 0x01 | AE_CE_HOST_FLAG_SKULLS | AE_CE_HOST_FLAG_RANDOMIZER, ours) == AE_CE_HOST_JOIN);
	AE_CHECK(ae_ce_host_class_joinable(AE_CE_HOST_JOIN));
	/* a host of another mod version, another network version, or a shared Tag Injector profile: told why */
	AE_CHECK(ae_ce_host_classify(0xFF00 | 111, 44, 0x01, ours) == AE_CE_HOST_MOD_OLDER);
	AE_CHECK(ae_ce_host_classify(0xFF00 | 113, 44, 0x01, ours) == AE_CE_HOST_MOD_NEWER);
	AE_CHECK(ae_ce_host_classify(0xFF00 | 112, 43, 0x01, ours) == AE_CE_HOST_BASE_OTHER);
	AE_CHECK(ae_ce_host_classify(0xFF00 | 112, 0, 0x01, ours) == AE_CE_HOST_BASE_OTHER);
	AE_CHECK(ae_ce_host_classify(0xFF00 | 112, 44, 0x01 | AE_CE_HOST_FLAG_TAG_SHARE, ours) == AE_CE_HOST_TAG_SHARE);
	AE_CHECK(!ae_ce_host_class_joinable(AE_CE_HOST_MOD_OLDER) && !ae_ce_host_class_joinable(AE_CE_HOST_MOD_NEWER));
	AE_CHECK(!ae_ce_host_class_joinable(AE_CE_HOST_BASE_OTHER) && !ae_ce_host_class_joinable(AE_CE_HOST_TAG_SHARE));
	AE_CHECK(!ae_ce_host_class_joinable(AE_CE_HOST_NOT_CE));
	/* (the join test of this game over the version a CE host is stored under: the host's network version, joined; the
	announced number alone would read as a newer version) */
	AE_CHECK(halo_port_advertised_joinable(44, HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG, HALO_PORT_NETWORK_VERSION_MINIMUM,
		HALO_PORT_NETWORK_VERSION_MAXIMUM));
	AE_CHECK(!halo_port_advertised_joinable(0xFF00 | 112, HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG,
		HALO_PORT_NETWORK_VERSION_MINIMUM, HALO_PORT_NETWORK_VERSION_MAXIMUM));
	/* the advertisement's constants are the reference's */
	AE_CHECK(HALO_PORT_ADVERTISED_SKULLS_FLAG == AE_CE_HOST_FLAG_SKULLS);
	AE_CHECK(HALO_PORT_ADVERTISED_SKULLS_OFFSET == 15 && AE_CE_HOST_BASE_VERSION_OFFSET == 24);
	AE_CHECK(AE_CE_HOST_BASE_VERSION_OFFSET + 2 <= 0x1A);
	/* (no flag of a modded game is one of this game's own) */
	AE_CHECK(!(AE_CE_HOST_FLAG_RANDOMIZER & (HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG | HALO_PORT_ADVERTISED_IN_PROGRESS_FLAG |
		HALO_PORT_ADVERTISED_DEDICATED_FLAG)));
}

static void test_listing(void)
{
	/* a game that is not modded is listed as it was, whatever its stamp's byte; a modded one only of this mod version */
	AE_CHECK(AE_CE_HOST_LISTING_FORMAT == 2);
	AE_CHECK(ae_ce_host_listing_taken(0, 0) && ae_ce_host_listing_taken(0, 112) && ae_ce_host_listing_taken(0, 255));
	AE_CHECK(ae_ce_host_listing_taken(1, 112));
	AE_CHECK(!ae_ce_host_listing_taken(1, 111) && !ae_ce_host_listing_taken(1, 113) && !ae_ce_host_listing_taken(1, 0));
	/* the engine: CE's Firefight, 6, is this game's 7 in a modded game's listing only (a plain game's 6 is Bomb) */
	AE_CHECK(ae_ce_host_listing_engine(1, 6, AE_ENGINE_FIREFIGHT) == AE_ENGINE_FIREFIGHT);
	AE_CHECK(ae_ce_host_listing_engine(0, 6, AE_ENGINE_FIREFIGHT) == 6);
	AE_CHECK(ae_ce_host_listing_engine(1, 1, AE_ENGINE_FIREFIGHT) == 1);
	AE_CHECK(AE_CE_HOST_ENGINE_AE_FIREFIGHT == AE_ENGINE_FIREFIGHT);
}

static void test_widths(void)
{
	/* the options table: 33 numbers, 1 byte but the skulls' (4), the generator and life threshold (2), the unit toggles (2),
	the profile's hash (4) */
	unsigned long total = 0;
	int index;

	for (index = 0; index < AE_CE_HOST_VALUES; index++)
		total += ae_ce_host_value_widths[index];
	AE_CHECK(AE_CE_HOST_VALUES == 33);
	AE_CHECK(total == 4 * 4 + 2 * 3 + 33 - 7);
	AE_CHECK(ae_ce_host_value_widths[AE_CE_HOST_VALUE_TAG_PROFILE] == 4);
}

static void test_convert(void)
{
	struct ae_ce_host_layout l = layout();
	unsigned long values[AE_CE_HOST_VALUES];
	unsigned long index;
	unsigned long tail = HALO_PORT_NETWORK_GAME_SIZE - VARIANT_OPTIONS - AE_CE_HOST_OPTIONS_AE_SIZE;

	make_ce_record();
	memset(ae_record, 0xEE, sizeof(ae_record));
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, &l, values) == AE_CE_HOST_RECORD_OK);
	/* everything before the options, and the options this game also has, as CE sent them (but the engine index) */
	for (index = 0; index < VARIANT_OPTIONS + offsetof(struct reference_options, floodfection); index++)
	{
		if (index >= ENGINE_OFFSET && index < ENGINE_OFFSET + 4)
			continue;
		AE_CHECK(ae_record[index] == ce_record[index]);
	}
	/* the engine index: this game's Firefight */
	AE_CHECK(ae_record[ENGINE_OFFSET] == AE_ENGINE_FIREFIGHT && !ae_record[ENGINE_OFFSET + 1] && !ae_record[ENGINE_OFFSET + 2] &&
		!ae_record[ENGINE_OFFSET + 3]);
	/* CE's floodfection and mode_variant bytes, which this game reads as its own other options: zero */
	for (index = offsetof(struct reference_options, floodfection); index < AE_CE_HOST_OPTIONS_AE_SIZE; index++)
		AE_CHECK(ae_record[VARIANT_OPTIONS + index] == 0);
	/* the record's tail (local data, the MCC hashes) moved up over CE's Firefight bytes */
	for (index = 0; index < tail; index++)
		AE_CHECK(ae_record[VARIANT_OPTIONS + AE_CE_HOST_OPTIONS_AE_SIZE + index] == ce_record[VARIANT_OPTIONS + 0x58 + index]);
	/* the options, in this game's table's order */
	AE_CHECK(values[0] == 1 && values[1] == 2 && values[2] == 3 && values[3] == 4);
	AE_CHECK(values[4] == 0x01020304 && values[5] == 1500 && values[6] == 300);
	AE_CHECK(values[7] == 3 && values[8] == 2 && values[9] == 45 && values[10] == 1);
	AE_CHECK(values[11] == 0 && values[12] == 0x0A0B0C0D && values[13] == 0x80000001 && values[14] == 0x1234);
	AE_CHECK(values[15] == 1 && values[16] == 2 && values[17] == 3 && values[18] == 4 && values[19] == 5);
	AE_CHECK(values[20] == 0x82 && values[21] == 6 && values[22] == 7 && values[23] == 8 && values[24] == 9);
	AE_CHECK(values[25] == 10 && values[26] == 11 && values[27] == 12 && values[28] == 13 && values[29] == 14);
	AE_CHECK(values[30] == 15 && values[31] == 16 && values[32] == 17);
}

static void test_refusals(void)
{
	struct ae_ce_host_layout l = layout();
	unsigned long values[AE_CE_HOST_VALUES];
	struct reference_options options;
	struct ae_ce_host_layout bad;

	/* only CE's long record, of exactly that size: this game's own record (any host's) is no record of CE's */
	make_ce_record();
	AE_CHECK(ae_ce_host_record_convert(ce_record, HALO_PORT_NETWORK_GAME_SIZE, ae_record, &l, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record) - 1, ae_record, &l, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record) + 1, ae_record, &l, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	AE_CHECK(ae_ce_host_record_convert(NULL, sizeof(ce_record), ae_record, &l, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), NULL, &l, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, NULL, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	/* a layout that is not this build's (the jump byte's place) is no layout */
	bad = l;
	bad.ae_jump_percent = 38;
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, &bad, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	bad = l;
	bad.engine_index_size = 0;
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, &bad, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	bad = l;
	bad.engine_index = VARIANT_OPTIONS;
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, &bad, values) == AE_CE_HOST_RECORD_BAD_SIZE);
	/* a long record of another engine than CE's Firefight (Bomb's 6 here is CE's Firefight there; any other is not) */
	ce_record[ENGINE_OFFSET] = 3;
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, &l, values) == AE_CE_HOST_RECORD_NOT_FIREFIGHT);
	ce_record[ENGINE_OFFSET] = 7;
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, &l, values) == AE_CE_HOST_RECORD_NOT_FIREFIGHT);
	/* a Firefight game that plays a shared Tag Injector profile */
	make_ce_record();
	memcpy(&options, ce_record + VARIANT_OPTIONS, sizeof(options));
	options.firefight_tag_profile = 0xDEADBEEF;
	memcpy(ce_record + VARIANT_OPTIONS, &options, sizeof(options));
	AE_CHECK(ae_ce_host_record_convert(ce_record, sizeof(ce_record), ae_record, &l, values) == AE_CE_HOST_RECORD_TAG_PROFILE);
	AE_CHECK(values[AE_CE_HOST_VALUE_TAG_PROFILE] == 0xDEADBEEF);
}

/* a listing (tell_player 0) says nothing, logs nothing and asks for nothing, for every class and every missing-pack mix */
static void test_a_listing_costs_nothing(void)
{
	int host_class, randomizer, skulls;

	for (host_class = AE_CE_HOST_NOT_CE; host_class <= AE_CE_HOST_TAG_SHARE + 1; host_class++)
		for (randomizer = 0; randomizer < 2; randomizer++)
			for (skulls = 0; skulls < 2; skulls++)
			{
				struct ae_ce_host_join_verdict listed, joined;

				memset(&listed, 0xFF, sizeof(listed));
				memset(&joined, 0xFF, sizeof(joined));
				ae_ce_host_join_decide(host_class, randomizer, skulls, 0, &listed);
				ae_ce_host_join_decide(host_class, randomizer, skulls, 1, &joined);
				AE_CHECK(!listed.log && !listed.tell_player && !listed.request_packs);
				/* (the join's own answer is the listing's) */
				AE_CHECK(listed.joinable == joined.joinable && listed.say == joined.say);
			}
}

static void test_the_join_says_and_asks(void)
{
	struct ae_ce_host_join_verdict v;

	ae_ce_host_join_decide(AE_CE_HOST_NOT_CE, 1, 1, 1, &v);
	AE_CHECK(v.joinable && !v.log && !v.tell_player && !v.request_packs);
	ae_ce_host_join_decide(AE_CE_HOST_JOIN, 0, 0, 1, &v);
	AE_CHECK(v.joinable && v.log && !v.tell_player && !v.request_packs && v.say == AE_CE_HOST_SAY_JOINABLE);
	ae_ce_host_join_decide(AE_CE_HOST_JOIN, 1, 0, 1, &v);
	AE_CHECK(!v.joinable && v.log && v.tell_player && v.request_packs == AE_CE_HOST_PACK_RANDOMIZER &&
		v.say == AE_CE_HOST_SAY_PACKS_MISSING);
	ae_ce_host_join_decide(AE_CE_HOST_JOIN, 0, 1, 1, &v);
	AE_CHECK(v.request_packs == AE_CE_HOST_PACK_SKULLS);
	ae_ce_host_join_decide(AE_CE_HOST_JOIN, 1, 1, 1, &v);
	AE_CHECK(v.request_packs == (AE_CE_HOST_PACK_RANDOMIZER | AE_CE_HOST_PACK_SKULLS));
	ae_ce_host_join_decide(AE_CE_HOST_MOD_OLDER, 0, 0, 1, &v);
	AE_CHECK(!v.joinable && v.log && v.tell_player && !v.request_packs && v.say == AE_CE_HOST_SAY_MOD_VERSION_OLDER);
	ae_ce_host_join_decide(AE_CE_HOST_MOD_NEWER, 0, 0, 1, &v);
	AE_CHECK(v.say == AE_CE_HOST_SAY_MOD_VERSION_NEWER);
	ae_ce_host_join_decide(AE_CE_HOST_TAG_SHARE, 0, 0, 1, &v);
	AE_CHECK(!v.joinable && v.say == AE_CE_HOST_SAY_TAG_SHARE && v.tell_player);
	ae_ce_host_join_decide(AE_CE_HOST_BASE_OTHER, 0, 0, 1, &v);
	AE_CHECK(!v.joinable && v.say == AE_CE_HOST_SAY_NETWORK_VERSION && v.tell_player);
}

/* each event from each state: the state after, and whether the options kept from a host go */
static void test_state_rule(void)
{
	static struct { int state, event, next, clear; } const table[] =
	{
		/* a join starts: the new host's kind; an earlier CE host's options go */
		{ AE_CE_HOST_STATE_NOT_CE,   AE_CE_HOST_EVENT_JOIN_STARTED_CE,      AE_CE_HOST_STATE_CE,      0 },
		{ AE_CE_HOST_STATE_CE,       AE_CE_HOST_EVENT_JOIN_STARTED_CE,      AE_CE_HOST_STATE_CE,      1 },
		{ AE_CE_HOST_STATE_UNKNOWN,  AE_CE_HOST_EVENT_JOIN_STARTED_CE,      AE_CE_HOST_STATE_CE,      0 },
		{ AE_CE_HOST_STATE_REFUSED,  AE_CE_HOST_EVENT_JOIN_STARTED_CE,      AE_CE_HOST_STATE_CE,      0 },
		{ AE_CE_HOST_STATE_NOT_CE,   AE_CE_HOST_EVENT_JOIN_STARTED_OTHER,   AE_CE_HOST_STATE_NOT_CE,   0 },
		{ AE_CE_HOST_STATE_CE,       AE_CE_HOST_EVENT_JOIN_STARTED_OTHER,   AE_CE_HOST_STATE_NOT_CE,   1 },
		{ AE_CE_HOST_STATE_UNKNOWN,  AE_CE_HOST_EVENT_JOIN_STARTED_OTHER,   AE_CE_HOST_STATE_NOT_CE,   0 },
		{ AE_CE_HOST_STATE_REFUSED,  AE_CE_HOST_EVENT_JOIN_STARTED_OTHER,   AE_CE_HOST_STATE_NOT_CE,   0 },
		{ AE_CE_HOST_STATE_NOT_CE,   AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN, AE_CE_HOST_STATE_UNKNOWN,  0 },
		{ AE_CE_HOST_STATE_CE,       AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN, AE_CE_HOST_STATE_UNKNOWN,  1 },
		{ AE_CE_HOST_STATE_UNKNOWN,  AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN, AE_CE_HOST_STATE_UNKNOWN,  0 },
		{ AE_CE_HOST_STATE_REFUSED,  AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN, AE_CE_HOST_STATE_UNKNOWN,  0 },
		/* the record accepted: only a CE (or unknown) host's makes the state CE */
		{ AE_CE_HOST_STATE_NOT_CE,   AE_CE_HOST_EVENT_RECORD_ACCEPTED,      AE_CE_HOST_STATE_NOT_CE,   0 },
		{ AE_CE_HOST_STATE_CE,       AE_CE_HOST_EVENT_RECORD_ACCEPTED,      AE_CE_HOST_STATE_CE,       0 },
		{ AE_CE_HOST_STATE_UNKNOWN,  AE_CE_HOST_EVENT_RECORD_ACCEPTED,      AE_CE_HOST_STATE_CE,       0 },
		{ AE_CE_HOST_STATE_REFUSED,  AE_CE_HOST_EVENT_RECORD_ACCEPTED,      AE_CE_HOST_STATE_REFUSED,  0 },
		/* the record refused: told once, then refused until the next join */
		{ AE_CE_HOST_STATE_NOT_CE,   AE_CE_HOST_EVENT_RECORD_REFUSED,       AE_CE_HOST_STATE_NOT_CE,   0 },
		{ AE_CE_HOST_STATE_CE,       AE_CE_HOST_EVENT_RECORD_REFUSED,       AE_CE_HOST_STATE_REFUSED,  1 },
		{ AE_CE_HOST_STATE_UNKNOWN,  AE_CE_HOST_EVENT_RECORD_REFUSED,       AE_CE_HOST_STATE_REFUSED,  0 },
		{ AE_CE_HOST_STATE_REFUSED,  AE_CE_HOST_EVENT_RECORD_REFUSED,       AE_CE_HOST_STATE_REFUSED,  0 },
	};
	static int const leaving[] = { AE_CE_HOST_EVENT_REJECTED, AE_CE_HOST_EVENT_DISCONNECTED, AE_CE_HOST_EVENT_LEFT,
		AE_CE_HOST_EVENT_HOSTING, AE_CE_HOST_EVENT_SOLO_START };
	size_t index;
	int state, event;

	for (index = 0; index < sizeof(table) / sizeof(table[0]); index++)
	{
		int clear = -1;

		AE_CHECK(ae_ce_host_state_next(table[index].state, table[index].event, &clear) == table[index].next);
		AE_CHECK(clear == table[index].clear);
	}
	/* every way out: nothing of a CE host is kept (its options go); a refused host stays refused */
	for (index = 0; index < sizeof(leaving) / sizeof(leaving[0]); index++)
		for (state = AE_CE_HOST_STATE_NOT_CE; state <= AE_CE_HOST_STATE_REFUSED; state++)
		{
			int clear = -1;
			int next = ae_ce_host_state_next(state, leaving[index], &clear);

			AE_CHECK(next == (state == AE_CE_HOST_STATE_REFUSED ? AE_CE_HOST_STATE_REFUSED : AE_CE_HOST_STATE_NOT_CE));
			AE_CHECK(clear == (state == AE_CE_HOST_STATE_CE));
		}
	/* every event from every state is answered with a state (a null clear is fine) */
	for (state = -1; state <= AE_CE_HOST_STATE_REFUSED + 1; state++)
		for (event = 0; event < AE_CE_HOST_EVENTS; event++)
		{
			int next = ae_ce_host_state_next(state, event, NULL);

			AE_CHECK(next >= AE_CE_HOST_STATE_NOT_CE && next <= AE_CE_HOST_STATE_REFUSED);
		}
	/* the sequence of a refused record: told once, not again, and a new join is clean */
	{
		int clear;
		int now = ae_ce_host_state_next(AE_CE_HOST_STATE_NOT_CE, AE_CE_HOST_EVENT_JOIN_STARTED_CE, &clear);

		now = ae_ce_host_state_next(now, AE_CE_HOST_EVENT_RECORD_REFUSED, &clear);
		AE_CHECK(now == AE_CE_HOST_STATE_REFUSED);
		now = ae_ce_host_state_next(now, AE_CE_HOST_EVENT_REJECTED, &clear);
		AE_CHECK(now == AE_CE_HOST_STATE_REFUSED);
		now = ae_ce_host_state_next(now, AE_CE_HOST_EVENT_JOIN_STARTED_OTHER, &clear);
		AE_CHECK(now == AE_CE_HOST_STATE_NOT_CE);
	}
}

/* a client whose level's shared tags differ from the host's leaves; the same tags, or a replay's playback, stay */
static void test_tags_leave(void)
{
	AE_CHECK(!ae_ce_host_tags_leave(1200, 0x1234abcdUL, 1200, 0x1234abcdUL, 0));
	AE_CHECK(ae_ce_host_tags_leave(1201, 0x1234abcdUL, 1200, 0x1234abcdUL, 0));
	AE_CHECK(ae_ce_host_tags_leave(1200, 0x1234abcdUL, 1200, 0x1234abceUL, 0));
	AE_CHECK(!ae_ce_host_tags_leave(1201, 0x1234abcdUL, 1200, 0x0UL, 1));
	AE_CHECK(strstr(AE_CE_HOST_TAGS_MESSAGE, "The game was left.") != NULL);
}

int main(void)
{
	test_tags_leave();
	test_this_games_numbers_are_what_they_were();
	test_classes();
	test_listing();
	test_widths();
	test_convert();
	test_refusals();
	test_a_listing_costs_nothing();
	test_the_join_says_and_asks();
	test_state_rule();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_ce_host_rules_test: ok\n");
	return 0;
}
