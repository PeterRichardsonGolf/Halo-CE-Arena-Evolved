#include <stdio.h>
#include <string.h>
#include "ae_setup_options.h"
#include "ae_spawn_modern.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a fake gametype: each option's value index now and where the edit started (-1: not in this build) */
struct fake { int now[AE_SETUP_OPTIONS], base[AE_SETUP_OPTIONS]; };

static int fake_get(void *context, int option)
{
	return ((struct fake *)context)->now[option];
}

static int fake_changed(void *context, int option)
{
	struct fake *fake = context;

	return fake->now[option] != fake->base[option];
}

static void fake_init(struct fake *fake, struct ae_setup_access *access, int engine)
{
	int option;

	for (option = 0; option < AE_SETUP_OPTIONS; option++)
		fake->now[option] = fake->base[option] = 0;
	/* (free for all: TEAM PLAY NO; vehicles DEFAULT, the loadout CATEGORY) */
	fake->now[AE_SO_TEAM_PLAY] = fake->base[AE_SO_TEAM_PLAY] = 1;
	access->get = fake_get;
	access->changed = fake_changed;
	access->context = fake;
	access->engine = engine;
	access->ae_rules = 0;
	access->nhe_here = 0;
}

static int has(short const *entries, short count, short entry)
{
	short index;

	for (index = 0; index < count; index++)
	{
		if (entries[index] == entry)
			return 1;
	}
	return 0;
}

static short index_of(short const *entries, short count, short entry)
{
	short index;

	for (index = 0; index < count; index++)
	{
		if (entries[index] == entry)
			return index;
	}
	return -1;
}

int main(void)
{
	struct fake fake;
	struct ae_setup_access access;
	struct ae_setup_group_info groups[AE_SETUP_GROUPS];
	short entries[160], count, index, first, last_header;
	int option, group;

	/* every row: a label, a group, values, and one help or one per value (NAME's value is the screen's text) */
	ae_setup_name_set("AE SLAYER");
	for (option = 0; option < AE_SETUP_OPTIONS; option++)
	{
		struct ae_setup_row const *row = ae_setup_row(option);

		CHECK(row != NULL);
		if (!row)
			continue;
		CHECK(row->label && row->label[0]);
		CHECK(row->group >= 0 && row->group < AE_SETUP_GROUPS);
		CHECK(row->value_count > 0 && row->values);
		CHECK(row->help_count == 1 || row->help_count == row->value_count);
		for (index = 0; index < row->value_count; index++)
			CHECK(ae_setup_value_text(option, index)[0] && ae_setup_help(option, index)[0]);
	}
	CHECK(ae_setup_row(-1) == NULL && ae_setup_row(AE_SETUP_OPTIONS) == NULL);
	CHECK(!strcmp(ae_setup_value_text(AE_SO_LIVES, 99), ""));

	/* free-for-all Slayer: its score row, no team rules but TEAM PLAY, red's vehicles named VEHICLE SET */
	fake_init(&fake, &access, AE_SETUP_ENGINE_SLAYER);
	count = ae_setup_entries(&access, entries, 160);
	CHECK(count > 0 && entries[0] == -1 - AE_SG_MATCH && entries[1] == AE_SO_NAME &&
		entries[2] == AE_SO_KILLS_TO_WIN);
	CHECK(!has(entries, count, AE_SO_CAPTURES_TO_WIN) && !has(entries, count, AE_SO_OBJECTIVES));
	CHECK(has(entries, count, AE_SO_TEAM_PLAY) && !has(entries, count, AE_SO_FRIENDLY_FIRE));
	CHECK(!has(entries, count, AE_SO_VEHICLE_SET_BLUE) && !has(entries, count, AE_SO_VEHICLE_COUNT_RED));
	CHECK(!strcmp(ae_setup_label(&access, AE_SO_VEHICLE_SET_RED), "VEHICLE SET"));
	/* (VEHICLES DESTRUCTIBLE, an AE rule, right after VEHICLE RESPAWN TIME: ON, OFF) */
	{
		int at = index_of(entries, count, AE_SO_VEHICLE_RESPAWN);

		CHECK(at >= 0 && entries[at + 1] == AE_SO_VEHICLES_DESTRUCTIBLE);
		CHECK(ae_setup_row(AE_SO_VEHICLES_DESTRUCTIBLE)->source == AE_SRC_AE && ae_setup_row(AE_SO_VEHICLES_DESTRUCTIBLE)->group == AE_SG_VEHICLES);
		CHECK(!strcmp(ae_setup_value_text(AE_SO_VEHICLES_DESTRUCTIBLE, 0), "ON") &&
			!strcmp(ae_setup_value_text(AE_SO_VEHICLES_DESTRUCTIBLE, 1), "OFF"));
	}
	CHECK(!strcmp(ae_setup_group_name(&access, AE_SG_MODE), "SLAYER"));
	CHECK(has(entries, count, AE_SO_WEAPON_SET) && !has(entries, count, AE_SO_PRIMARY));
	/* groups in order, each header once, never two headers in a row, never one last */
	last_header = -1;
	for (index = 0; index < count; index++)
	{
		if (!ae_setup_entry_is_header(entries[index]))
			continue;
		CHECK(ae_setup_entry_group(entries[index]) > last_header);
		CHECK(index + 1 < count && !ae_setup_entry_is_header(entries[index + 1]));
		last_header = (short)ae_setup_entry_group(entries[index]);
	}
	/* (NHE's rows only on an NHE map) */
	CHECK(last_header == AE_SG_SOUND);
	access.nhe_here = 1;
	count = ae_setup_entries(&access, entries, 160);
	CHECK(has(entries, count, AE_SO_NHE_MODE) && entries[count - 3] == -1 - AE_SG_NHE);
	access.nhe_here = 0;
	count = ae_setup_entries(&access, entries, 160);
	ae_setup_groups(&access, entries, count, groups);
	/* (SLAYER: its three, and OpenCE's MODE; Infection's and Gun Game's rows while it is theirs) */
	CHECK(groups[AE_SG_TEAMS].rows == 1 && groups[AE_SG_MATCH].rows == 5 && groups[AE_SG_MODE].rows == 4);
	CHECK(has(entries, count, AE_SO_OPENCE_MODE) && has(entries, count, AE_SO_HARDCORE) &&
		!has(entries, count, AE_SO_INFECTION_ZOMBIES) && !has(entries, count, AE_SO_GUN_GAME_ORDER));
	fake.now[AE_SO_OPENCE_MODE] = AE_SETUP_OPENCE_INFECTION;
	count = ae_setup_entries(&access, entries, 160);
	CHECK(has(entries, count, AE_SO_INFECTION_ZOMBIES) && has(entries, count, AE_SO_INFECTION_SHIELDS) &&
		!has(entries, count, AE_SO_GUN_GAME_MELEE));
	fake.now[AE_SO_OPENCE_MODE] = AE_SETUP_OPENCE_GUN_GAME;
	count = ae_setup_entries(&access, entries, 160);
	CHECK(has(entries, count, AE_SO_GUN_GAME_ORDER) && has(entries, count, AE_SO_GUN_GAME_MELEE) &&
		!has(entries, count, AE_SO_INFECTION_ZOMBIES));
	fake.now[AE_SO_OPENCE_MODE] = 0;
	count = ae_setup_entries(&access, entries, 160);
	ae_setup_groups(&access, entries, count, groups);
	CHECK(groups[AE_SG_MATCH].header == 0 && ae_setup_changes(&access) == 0);
	/* turning TEAM PLAY on brings the team rules, blue's vehicles and RED VEHICLES */
	fake.now[AE_SO_TEAM_PLAY] = AE_SETUP_TEAM_PLAY_YES;
	count = ae_setup_entries(&access, entries, 160);
	CHECK(has(entries, count, AE_SO_FRIENDLY_FIRE) && has(entries, count, AE_SO_VEHICLE_SET_BLUE));
	CHECK(!strcmp(ae_setup_label(&access, AE_SO_VEHICLE_SET_RED), "RED VEHICLES"));
	CHECK(ae_setup_changes(&access) == 1);
	ae_setup_groups(&access, entries, count, groups);
	CHECK(groups[AE_SG_TEAMS].rows == 5 && groups[AE_SG_TEAMS].changed == 1 && groups[AE_SG_RESPAWN].changed == 0);

	/* CTF: always team, no TEAM PLAY row, its objectives row */
	fake_init(&fake, &access, AE_SETUP_ENGINE_CTF);
	count = ae_setup_entries(&access, entries, 160);
	CHECK(!has(entries, count, AE_SO_TEAM_PLAY) && has(entries, count, AE_SO_FRIENDLY_FIRE));
	CHECK(has(entries, count, AE_SO_CAPTURES_TO_WIN) && has(entries, count, AE_SO_OBJECTIVES));
	CHECK(!has(entries, count, AE_SO_DEATH_BONUS) && !has(entries, count, AE_SO_BALL_MELEE));
	/* a change to a row of another mode is not counted (the gametype has no such rule) */
	fake.now[AE_SO_DEATH_BONUS] = 1;
	CHECK(ae_setup_changes(&access) == 0);

	/* the rows of GAME OPTIONS' second pass: the packs, PACK WEAPONS ON MAP, STARTING GRENADES, BOTTOMLESS CLIP in
	EQUIPMENT (a gametype from before reads 0 = the stock value in each), NAME in MATCH; the packs and PACK WEAPONS ON
	MAP on the WEAPON PACKS screen, one WEAPON PACKS row in EQUIPMENT opening it */
	{
		static const short packs[] = { AE_SO_PACK_BATTLE_RIFLE, AE_SO_PACK_SMG, AE_SO_PACK_SMG_SILENCED,
			AE_SO_PACK_CARBINE, AE_SO_PACK_AR_SILENCED, AE_SO_PACK_DMR,
			AE_SO_PACK_EVO_RIFLE };
		static const short equipment[] = { AE_SO_PACKS_ON_MAP, AE_SO_STARTING_GRENADES, AE_SO_BOTTOMLESS_CLIP };
		short row_index, page[16], page_count;

		fake_init(&fake, &access, AE_SETUP_ENGINE_SLAYER);
		count = ae_setup_entries(&access, entries, 160);
		page_count = ae_setup_packs_entries(&access, page, 16);
		CHECK(AE_SO_PACK_FIRST == AE_SO_PACK_BATTLE_RIFLE && AE_SO_PACK_EVO_RIFLE == AE_SO_PACK_FIRST + AE_SETUP_PACKS - 1);
		/* (EQUIPMENT: WEAPON PACKS right after GRENADES ON MAP; no pack row, no PACK WEAPONS ON MAP) */
		index = index_of(entries, count, AE_SO_GRENADES_ON_MAP);
		CHECK(index >= 0 && entries[index + 1] == AE_SO_WEAPON_PACKS &&
			ae_setup_row(AE_SO_WEAPON_PACKS)->source == AE_SRC_AE);
		CHECK(!has(entries, count, AE_SO_PACKS_ON_MAP));
		/* (the screen: PACKS, seven packs, PACK WEAPONS ON MAP, START WEAPONS, PRIMARY, SECONDARY) */
		CHECK(page_count == 12 && page[0] == -1 - AE_SETUP_SECTION_PACKS && page[8] == AE_SO_PACKS_ON_MAP &&
			page[9] == -1 - AE_SETUP_SECTION_START && page[10] == AE_SO_PRIMARY && page[11] == AE_SO_SECONDARY);
		CHECK(!strcmp(ae_setup_section_name(&access, AE_SETUP_SECTION_PACKS), "PACKS") &&
			!strcmp(ae_setup_section_name(&access, AE_SETUP_SECTION_START), "START WEAPONS") &&
			!strcmp(ae_setup_section_name(&access, AE_SG_WEAPONS), "WEAPONS"));
		CHECK(ae_setup_row_step(page, page_count, 8, 1) == 10 && ae_setup_row_step(page, page_count, 10, -1) == 8);
		/* (a short list: no header left without its row) */
		CHECK(ae_setup_packs_entries(&access, page, 10) == 9 && page[8] == AE_SO_PACKS_ON_MAP);
		page_count = ae_setup_packs_entries(&access, page, 16);
		for (row_index = 0; row_index < AE_SETUP_PACKS; row_index++)
		{
			CHECK(!has(entries, count, packs[row_index]) && has(page, page_count, packs[row_index]));
			CHECK(ae_setup_row(packs[row_index])->group == AE_SG_WEAPONS);
			CHECK(ae_setup_row(packs[row_index])->value_count == 2 &&
				ae_setup_row(packs[row_index])->source == AE_SRC_AE);
			CHECK(!strcmp(ae_setup_value_text(packs[row_index], 0), "OFF") && !strcmp(ae_setup_value_text(packs[row_index], 1), "ON"));
			fake.now[packs[row_index]] = 1;
			CHECK(ae_setup_changes(&access) == 1);
			fake.now[packs[row_index]] = 0;
		}
		for (row_index = 0; row_index < 3; row_index++)
		{
			CHECK((has(entries, count, equipment[row_index]) || has(page, page_count, equipment[row_index])) &&
				ae_setup_row(equipment[row_index])->group ==
					(equipment[row_index] == AE_SO_STARTING_GRENADES ? AE_SG_LOADOUT : AE_SG_WEAPONS));
			fake.now[equipment[row_index]] = 1;
			CHECK(ae_setup_changes(&access) == 1);
			fake.now[equipment[row_index]] = 0;
		}
		CHECK(ae_setup_row(AE_SO_STARTING_GRENADES)->value_count == 6 &&
			!strcmp(ae_setup_value_text(AE_SO_STARTING_GRENADES, 0), "GAME'S OWN") &&
			!strcmp(ae_setup_value_text(AE_SO_STARTING_GRENADES, 1), "NONE") &&
			!strcmp(ae_setup_value_text(AE_SO_STARTING_GRENADES, 5), "4"));
		/* the loadout rows: the game's ten weapons, then one a pack slot in slot order */
		CHECK(ae_setup_row(AE_SO_PRIMARY)->value_count == AE_SETUP_WEAPON_PACK_FIRST + AE_SETUP_PACKS &&
			ae_setup_row(AE_SO_SECONDARY)->value_count == AE_SETUP_WEAPON_PACK_FIRST + AE_SETUP_PACKS);
		CHECK(!strcmp(ae_setup_value_text(AE_SO_PRIMARY, 9), "NEEDLER") &&
			!strcmp(ae_setup_value_text(AE_SO_PRIMARY, AE_SETUP_WEAPON_PACK_FIRST), "BATTLE RIFLE") &&
			!strcmp(ae_setup_value_text(AE_SO_SECONDARY, AE_SETUP_WEAPON_PACK_FIRST + 4), "SUPPRESSED AR") &&
			!strcmp(ae_setup_value_text(AE_SO_PRIMARY, AE_SETUP_WEAPON_PACK_FIRST + 6), "EVO RIFLE"));
		/* NAME: one value, the text the screen gave (cut at the buffer) */
		CHECK(has(entries, count, AE_SO_NAME) && ae_setup_row(AE_SO_NAME)->group == AE_SG_MATCH);
		CHECK(ae_setup_row(AE_SO_NAME)->value_count == 1 && ae_setup_row(AE_SO_NAME)->source == AE_SRC_CLASSIC);
		ae_setup_name_set("AE TEAM SLY");
		CHECK(!strcmp(ae_setup_value_text(AE_SO_NAME, 0), "AE TEAM SLY"));
		ae_setup_name_set("A NAME OF MUCH TOO MANY CHARACTERS");
		CHECK(strlen(ae_setup_value_text(AE_SO_NAME, 0)) == 15);
		ae_setup_name_set(NULL);
		CHECK(!strcmp(ae_setup_value_text(AE_SO_NAME, 0), "") && !strcmp(ae_setup_value_text(AE_SO_NAME, 1), ""));
		fake.now[AE_SO_NAME] = -1;
		count = ae_setup_entries(&access, entries, 160);
		CHECK(!has(entries, count, AE_SO_NAME));
	}

	/* WEAPON PACKS: the summary, each pack on's rank (every pack on can start), what PRIMARY / SECONDARY offer (the
	stock ten, the packs on, and always the value they have) */
	{
		char text[64], help[320];
		short offered[24], offered_count;

		fake_init(&fake, &access, AE_SETUP_ENGINE_SLAYER);
		ae_setup_packs_summary(&access, text, sizeof(text));
		CHECK(!strcmp(text, "NONE"));
		fake.now[AE_SO_PACK_SMG] = fake.now[AE_SO_PACK_CARBINE] = 1;
		ae_setup_packs_summary(&access, text, sizeof(text));
		CHECK(!strcmp(text, "SMG, CARBINE"));
		CHECK(ae_setup_pack_source(&access, 2) == 1 && ae_setup_pack_source(&access, 4) == 2 &&
			ae_setup_pack_source(&access, 1) == 0);
		/* (the BR on: the carbine is the third, and can still start) */
		fake.now[AE_SO_PACK_BATTLE_RIFLE] = 1;
		ae_setup_packs_summary(&access, text, sizeof(text));
		CHECK(!strcmp(text, "3 ON"));
		CHECK(ae_setup_pack_source(&access, 1) == 1 && ae_setup_pack_source(&access, 2) == 2 &&
			ae_setup_pack_source(&access, 4) == 3 && ae_setup_pack_on(&access, 4));
		CHECK(ae_setup_pack_help(&access, AE_SO_PACK_CARBINE, 1, help, sizeof(help)) &&
			strstr(help, "it can also be a start weapon"));
		CHECK(ae_setup_pack_help(&access, AE_SO_PACK_SMG, 1, help, sizeof(help)) &&
			strstr(help, "it can also be a start weapon"));
		CHECK(ae_setup_pack_help(&access, AE_SO_PACK_AR_SILENCED, 0, help, sizeof(help)) &&
			strstr(help, "Not installed on this PC"));
		CHECK(ae_setup_pack_help(&access, AE_SO_PACK_AR_SILENCED, 1, help, sizeof(help)) &&
			strstr(help, "turns the pack on"));
		CHECK(ae_setup_pack_help(&access, AE_SO_PRIMARY, 1, help, sizeof(help)) == NULL);
		/* (one long name alone is shown; NONE when all are off) */
		fake.now[AE_SO_PACK_BATTLE_RIFLE] = fake.now[AE_SO_PACK_SMG] = fake.now[AE_SO_PACK_CARBINE] = 0;
		fake.now[AE_SO_PACK_SMG_SILENCED] = 1;
		ae_setup_packs_summary(&access, text, sizeof(text));
		CHECK(!strcmp(text, "SUPPRESSED SMG"));
		/* (PRIMARY: the ten, then the silenced SMG only) */
		fake.now[AE_SO_PRIMARY] = 3;
		offered_count = ae_setup_offered_values(&access, AE_SO_PRIMARY, offered, 24);
		CHECK(offered_count == 11 && offered[9] == 9 && offered[10] == AE_SETUP_WEAPON_PACK_FIRST + 2);
		CHECK(ae_setup_value_step(&access, AE_SO_PRIMARY, 9, 1) == AE_SETUP_WEAPON_PACK_FIRST + 2);
		/* (round past the ends: the last offered's right is the first) */
		CHECK(ae_setup_value_step(&access, AE_SO_PRIMARY, AE_SETUP_WEAPON_PACK_FIRST + 2, 1) == 0);
		CHECK(ae_setup_value_step(&access, AE_SO_PRIMARY, AE_SETUP_WEAPON_PACK_FIRST + 2, -1) == 9);
		CHECK(ae_setup_value_step(&access, AE_SO_PRIMARY, 0, -1) == AE_SETUP_WEAPON_PACK_FIRST + 2);
		/* (a saved gametype's pack weapon whose pack is off still shows: never reset silently) */
		fake.now[AE_SO_SECONDARY] = AE_SETUP_WEAPON_PACK_FIRST + 3;
		CHECK(ae_setup_value_offered(&access, AE_SO_SECONDARY, AE_SETUP_WEAPON_PACK_FIRST + 3) &&
			!ae_setup_value_offered(&access, AE_SO_SECONDARY, AE_SETUP_WEAPON_PACK_FIRST + 4) &&
			!ae_setup_value_offered(&access, AE_SO_PRIMARY, AE_SETUP_WEAPON_PACK_FIRST + 3));
		/* (other rows: every value; their step as before) */
		CHECK(ae_setup_value_offered(&access, AE_SO_LIVES, 1) && ae_setup_value_step(&access, AE_SO_LIVES, 0, 1) == 1);
		CHECK(!ae_setup_value_offered(&access, AE_SO_LIVES, 99));
		/* (all seven on: PRIMARY offers the first five packs, the fifth too; the sixth and seventh cannot start) */
		for (option = AE_SO_PACK_FIRST; option < AE_SO_PACK_FIRST + AE_SETUP_PACKS; option++)
			fake.now[option] = 1;
		fake.now[AE_SO_PRIMARY] = 0;
		offered_count = ae_setup_offered_values(&access, AE_SO_PRIMARY, offered, 24);
		CHECK(offered_count == 15 && offered[14] == AE_SETUP_WEAPON_PACK_FIRST + 4);
		CHECK(ae_setup_pack_source(&access, 5) == 5 && ae_setup_pack_source(&access, 6) == 0 &&
			ae_setup_pack_source(&access, 7) == 0);
		/* (no pack row in this build: no WEAPON PACKS row) */
		fake_init(&fake, &access, AE_SETUP_ENGINE_SLAYER);
		for (option = AE_SO_PACK_FIRST; option < AE_SO_PACK_FIRST + AE_SETUP_PACKS; option++)
			fake.now[option] = -1;
		fake.now[AE_SO_PACKS_ON_MAP] = -1;
		count = ae_setup_entries(&access, entries, 160);
		CHECK(!has(entries, count, AE_SO_WEAPON_PACKS));
	}

	/* a custom loadout: the weapons, and WEAPON SET only on AE's rules */
	fake_init(&fake, &access, AE_SETUP_ENGINE_SLAYER);
	fake.now[AE_SO_LOADOUT] = AE_SETUP_LOADOUT_CUSTOM;
	count = ae_setup_entries(&access, entries, 160);
	CHECK(has(entries, count, AE_SO_PRIMARY) && has(entries, count, AE_SO_SECONDARY));
	CHECK(!has(entries, count, AE_SO_WEAPON_SET));
	access.ae_rules = 1;
	count = ae_setup_entries(&access, entries, 160);
	CHECK(has(entries, count, AE_SO_WEAPON_SET));

	/* a CUSTOM vehicle set: its six counts right under it */
	fake_init(&fake, &access, AE_SETUP_ENGINE_KING);
	fake.now[AE_SO_VEHICLE_SET_RED] = AE_SETUP_VEHICLE_SET_CUSTOM;
	count = ae_setup_entries(&access, entries, 160);
	index = index_of(entries, count, AE_SO_VEHICLE_SET_RED);
	CHECK(index >= 0 && index + 6 < count);
	for (option = 0; option < AE_SETUP_VEHICLE_KINDS; option++)
		CHECK(entries[index + 1 + option] == AE_SO_VEHICLE_COUNT_RED + option);
	CHECK(!strcmp(ae_setup_group_name(&access, AE_SG_MODE), "KING OF THE HILL"));

	/* a rule this build lacks is hidden and never counted */
	fake_init(&fake, &access, AE_SETUP_ENGINE_SLAYER);
	fake.now[AE_SO_SENSOR_RANGE] = -1;
	count = ae_setup_entries(&access, entries, 160);
	CHECK(!has(entries, count, AE_SO_SENSOR_RANGE) && ae_setup_changes(&access) == 0);

	/* LB / RB: group to group, wrapping; up / down past headers */
	fake_init(&fake, &access, AE_SETUP_ENGINE_ODDBALL);
	count = ae_setup_entries(&access, entries, 160);
	access.nhe_here = 1;
	count = ae_setup_entries(&access, entries, 160);
	first = ae_setup_group_first_row(entries, count, AE_SG_RESPAWN);
	CHECK(first > 0 && entries[first] == AE_SO_LIVES);
	CHECK(ae_setup_group_step(entries, count, 1, 1) == first);
	CHECK(ae_setup_group_step(entries, count, first, -1) == 1);
	CHECK(entries[ae_setup_group_step(entries, count, 1, -1)] == AE_SO_NHE_MODE);   /* MATCH back to NHE */
	CHECK(ae_setup_group_step(entries, count, ae_setup_group_first_row(entries, count, AE_SG_NHE), 1) == 1);
	CHECK(ae_setup_group_at(entries, count, first) == AE_SG_RESPAWN && ae_setup_group_at(entries, count, 0) == AE_SG_MATCH);
	CHECK(ae_setup_row_step(entries, count, (short)(first - 2), 1) == first);       /* the last MATCH row down: LIVES */
	CHECK(ae_setup_row_step(entries, count, first, -1) == first - 2);
	CHECK(ae_setup_row_step(entries, count, 1, -1) == 1);                           /* the top stays */
	CHECK(ae_setup_row_step(entries, count, (short)(count - 1), 1) == count - 1);   /* the bottom too */
	CHECK(ae_setup_group_first_row(entries, count, AE_SETUP_GROUPS) == -1);
	CHECK(!strcmp(ae_setup_group_name(&access, AE_SG_MODE), "ODDBALL") && has(entries, count, AE_SO_BALL_MELEE));
	/* every group of an oddball game shows */
	for (group = 0; group < AE_SETUP_GROUPS; group++)
		CHECK(ae_setup_group_first_row(entries, count, group) > 0);

	/* a Bomb gametype (AE GRIFBALL, AE BOMB): its score as ROUNDS TO WIN, its own rules under BOMB, OBJECTIVES; a
	team game whatever TEAM PLAY reads, which it never shows; no other engine's rows */
	fake_init(&fake, &access, AE_SETUP_ENGINE_BOMB);
	count = ae_setup_entries(&access, entries, 160);
	CHECK(ae_setup_team(&access) && !has(entries, count, AE_SO_TEAM_PLAY));
	CHECK(has(entries, count, AE_SO_ROUNDS_TO_WIN) && !has(entries, count, AE_SO_KILLS_TO_WIN) &&
		!has(entries, count, AE_SO_CAPTURES_TO_WIN) && !has(entries, count, AE_SO_LAPS_TO_WIN));
	CHECK(!strcmp(ae_setup_group_name(&access, AE_SG_MODE), "BOMB"));
	first = ae_setup_group_first_row(entries, count, AE_SG_MODE);
	CHECK(first > 0 && entries[first] == AE_SO_BOMB_ARM_TIME);
	for (option = AE_SO_BOMB_ARM_TIME; option <= AE_SO_BOMB_TRAITS; option++)
		CHECK(entries[first + option - AE_SO_BOMB_ARM_TIME] == option);
	CHECK(entries[first + AE_SO_BOMB_TRAITS - AE_SO_BOMB_ARM_TIME + 1] == AE_SO_OBJECTIVES);
	CHECK(!has(entries, count, AE_SO_ASSAULT) && !has(entries, count, AE_SO_BALL_MELEE) &&
		!has(entries, count, AE_SO_DEATH_BONUS));
	/* (the team rows shown: FRIENDLY FIRE, BLUE VEHICLES) */
	CHECK(has(entries, count, AE_SO_FRIENDLY_FIRE) && has(entries, count, AE_SO_VEHICLE_SET_BLUE));
	CHECK(!strcmp(ae_setup_value_text(AE_SO_ROUNDS_TO_WIN, 2), "3") &&
		!strcmp(ae_setup_value_text(AE_SO_BOMB_ARM_TIME, 0), "ON CONTACT") &&
		!strcmp(ae_setup_value_text(AE_SO_BOMB_TRAITS, 1), "GRIFBALL"));
	CHECK(ae_setup_row(AE_SO_BOMB_RESET_TIME)->help_count == ae_setup_row(AE_SO_BOMB_RESET_TIME)->value_count &&
		ae_setup_help(AE_SO_BOMB_ROUND_TIME, 5)[0]);
	/* (and no Bomb row in another engine's game) */
	fake_init(&fake, &access, AE_SETUP_ENGINE_CTF);
	count = ae_setup_entries(&access, entries, 160);
	for (option = AE_SO_BOMB_ARM_TIME; option <= AE_SO_BOMB_TRAITS; option++)
		CHECK(!has(entries, count, option));
	CHECK(!has(entries, count, AE_SO_ROUNDS_TO_WIN));
	fake_init(&fake, &access, AE_SETUP_ENGINE_ODDBALL);
	count = ae_setup_entries(&access, entries, 160);

	/* a list too short for everything: no header without its row */
	access.nhe_here = 0;
	count = ae_setup_entries(&access, entries, 4);
	CHECK(count == 4 && entries[0] == -1 - AE_SG_MATCH && entries[3] != -1 - AE_SG_RESPAWN);
	CHECK(ae_setup_entries(&access, entries, 7) == 6);
	count = ae_setup_entries(&access, entries, 8);
	CHECK(count == 8 && entries[6] == -1 - AE_SG_RESPAWN && entries[7] == AE_SO_LIVES);
	CHECK(ae_setup_entries(NULL, entries, 10) == 0 && ae_setup_entries(&access, entries, 0) == 0);

	/* the display order lists every option once */
	{
		static char seen[AE_SETUP_OPTIONS];

		memset(seen, 0, sizeof(seen));
		CHECK(ae_setup_display_order_count() == AE_SETUP_OPTIONS);
		for (index = 0; index < ae_setup_display_order_count(); index++)
		{
			int at = ae_setup_display_order_at(index);

			CHECK(at >= 0 && at < AE_SETUP_OPTIONS && !seen[at]);
			if (at >= 0 && at < AE_SETUP_OPTIONS)
				seen[at] = 1;
		}
	}

	/* each row's source and badge: Halo CE's own plain, OpenCE's and ChupathingyCE's COMMUNITY, ours AE, NHE's NHE */
	CHECK(ae_setup_row(AE_SO_LIVES)->source == AE_SRC_CLASSIC && !ae_setup_source_badge(ae_setup_row(AE_SO_LIVES))[0]);
	CHECK(ae_setup_row(AE_SO_HARDCORE)->source == AE_SRC_COMMUNITY && ae_setup_row(AE_SO_MAP_WEAPONS)->source == AE_SRC_COMMUNITY &&
		ae_setup_row(AE_SO_BOMB_ARM_TIME)->source == AE_SRC_COMMUNITY && ae_setup_row(AE_SO_OPENCE_MODE)->source == AE_SRC_COMMUNITY);
	CHECK(ae_setup_row(AE_SO_SPAWNS)->source == AE_SRC_AE && ae_setup_row(AE_SO_SPAWNS)->group == AE_SG_RESPAWN);
	CHECK(!strcmp(ae_setup_source_badge(ae_setup_row(AE_SO_SPAWNS)), "AE") &&
		!strcmp(ae_setup_source_badge(ae_setup_row(AE_SO_HARDCORE)), "COMMUNITY") &&
		!strcmp(ae_setup_source_badge(ae_setup_row(AE_SO_NHE_MODE)), "NHE") && !ae_setup_source_badge(NULL)[0]);
	CHECK(!strcmp(ae_setup_value_text(AE_SO_SPAWNS, 0), "CLASSIC") && !strcmp(ae_setup_value_text(AE_SO_SPAWNS, 1), "MODERN") &&
		!strcmp(ae_setup_help(AE_SO_SPAWNS, 0), "Halo CE's own spawns.") && strstr(ae_setup_help(AE_SO_SPAWNS, 1), "Modern spawns"));

	/* the filter chips: counts, lists, the focused row's group kept, CHANGED empty until a change, NHE anywhere */
	fake_init(&fake, &access, AE_SETUP_ENGINE_SLAYER);
	{
		short rows_total, classic, community, ae, nhe, listed;
		short filtered[160];

		count = ae_setup_entries(&access, entries, 160);
		rows_total = ae_setup_filter_count(&access, AE_SF_ALL);
		for (index = 0, listed = 0; index < count; index++)
			listed = (short)(listed + !ae_setup_entry_is_header(entries[index]));
		CHECK(rows_total == listed && rows_total > 0);
		classic = ae_setup_filter_count(&access, AE_SF_CLASSIC);
		community = ae_setup_filter_count(&access, AE_SF_COMMUNITY);
		ae = ae_setup_filter_count(&access, AE_SF_AE);
		nhe = ae_setup_filter_count(&access, AE_SF_NHE);
		CHECK(classic > 0 && community > 0 && ae > 0 && nhe == 2);
		/* (ALL is the three sources' rows: NHE's only on an NHE map) */
		CHECK(classic + community + ae == rows_total);
		CHECK(ae_setup_filter_count(&access, AE_SF_CHANGED) == 0);
		count = ae_setup_entries_filtered(&access, AE_SF_NHE, filtered, 160);
		CHECK(count == 3 && filtered[0] == -1 - AE_SG_NHE && filtered[1] == AE_SO_NHE_MODE && filtered[2] == AE_SO_NHE_EXTRAS);
		count = ae_setup_entries_filtered(&access, AE_SF_AE, filtered, 160);
		for (index = 0; index < count; index++)
			CHECK(ae_setup_entry_is_header(filtered[index]) || ae_setup_row(filtered[index])->source == AE_SRC_AE);
		CHECK(has(filtered, count, AE_SO_SPAWNS) && !has(filtered, count, AE_SO_LIVES));
		/* (a header only with a row; the groups' counts follow the filter) */
		ae_setup_groups(&access, filtered, count, groups);
		CHECK(groups[AE_SG_RESPAWN].rows == 2 && groups[AE_SG_MATCH].rows == 1 && groups[AE_SG_NHE].header < 0);
		fake.now[AE_SO_SPAWNS] = 1;
		CHECK(ae_setup_filter_count(&access, AE_SF_CHANGED) == 1);
		count = ae_setup_entries_filtered(&access, AE_SF_CHANGED, filtered, 160);
		CHECK(count == 2 && filtered[0] == -1 - AE_SG_RESPAWN && filtered[1] == AE_SO_SPAWNS);
		access.nhe_here = 1;
		CHECK(ae_setup_filter_count(&access, AE_SF_ALL) == rows_total + 2);
		CHECK(ae_setup_entries_filtered(&access, 99, filtered, 160) == 0 && ae_setup_filter_count(&access, -1) == 0);
		CHECK(!strcmp(ae_setup_filter_name(AE_SF_COMMUNITY), "COMMUNITY") && !strcmp(ae_setup_filter_name(AE_SF_ALL), "ALL") &&
			strstr(ae_setup_filter_help(AE_SF_COMMUNITY), "OpenCE and ChupathingyCE") && !ae_setup_filter_name(9)[0]);
	}

	/* SPAWNS shares universal_variant.ball_melee with BALL MELEE: bits 4-5 and the low nibble, each kept by the other */
	{
		unsigned char byte = 0;

		CHECK(ae_ball_melee_of(0) == 0 && ae_spawns_of(0) == AE_SPAWNS_CLASSIC);
		byte = ae_spawns_set(byte, AE_SPAWNS_MODERN);
		CHECK(byte == 0x10 && ae_ball_melee_of(byte) == 0 && ae_spawns_of(byte) == AE_SPAWNS_MODERN);
		byte = ae_ball_melee_set(byte, 1);
		CHECK(byte == 0x11 && ae_ball_melee_of(byte) == 1 && ae_spawns_of(byte) == AE_SPAWNS_MODERN);
		byte = ae_spawns_set(byte, AE_SPAWNS_CLASSIC);
		CHECK(byte == 0x01 && ae_ball_melee_of(byte) == 1 && ae_spawns_of(byte) == AE_SPAWNS_CLASSIC);
		/* (a gametype from before: LETHAL 1 and STOCK 0 read as ever, CLASSIC spawns) */
		CHECK(ae_ball_melee_of(1) == 1 && ae_spawns_of(1) == AE_SPAWNS_CLASSIC && ae_spawns_of(0) == AE_SPAWNS_CLASSIC);
		/* (the other bits, 6-7, are not touched) */
		CHECK(ae_spawns_set(0xC0, AE_SPAWNS_MODERN) == 0xD0 && ae_ball_melee_set(0xF0, 0) == 0xF0);
	}

	if (failures) return 1;
	printf("ae_setup_options: ok\n");
	return 0;
}
