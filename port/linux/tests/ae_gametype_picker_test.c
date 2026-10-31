#include <stdio.h>
#include <string.h>
#include "ae_gametype_picker.h"
#include "ae_setup_options.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* (static: the model is about 100 KB) */
static struct ae_gtp gtp;
static struct ae_setup_maps maps;

static short add(const char *name, short folder, short order, int engine, int ae)
{
	struct ae_gtp_gametype *gametype = &gtp.gametypes[gtp.count];

	memset(gametype, 0, sizeof(*gametype));
	snprintf(gametype->name, sizeof(gametype->name), "%s", name);
	snprintf(gametype->stored, sizeof(gametype->stored), "%.11s", name);
	gametype->folder = folder;
	gametype->order = order;
	gametype->engine = engine;
	gametype->ae = ae;
	return gtp.count++;
}

static void add_map(const char *name, int modes, char size, unsigned int groups)
{
	struct ae_setup_map *map = &maps.maps[maps.count++];

	memset(map, 0, sizeof(*map));
	snprintf(map->name, sizeof(map->name), "%s", name);
	map->facts = AE_SETUP_FACTS_READ;
	map->modes = modes;
	map->size = size;
	map->groups[0] = groups;
}

static int find_row(struct ae_gtp_row const *rows, short count, short folder, short gametype)
{
	short index;

	for (index = 0; index < count; index++)
		if (rows[index].folder == folder && rows[index].gametype == gametype)
			return index;
	return -1;
}

int main(void)
{
	struct ae_gtp_row rows[64];
	short count, ffa, ffa_br, team, team_br, ctf2, ctf2_br, octagon, octagon_snipe, nhe_ctf, stock_ctf, mine, indices[8];
	short have, read;
	char text[AE_GTP_START];
	unsigned int prefer[AE_SETUP_MAP_GROUP_WORDS] = { 1u };

	/* the start's words */
	ae_gtp_start_text("BR", 1, AE_GTW_ASSAULT_RIFLE, 0, text, sizeof(text));
	CHECK(!strcmp(text, "BR START"));
	ae_gtp_start_text(NULL, 1, AE_GTW_PISTOL, 0, text, sizeof(text));
	CHECK(!strcmp(text, "PISTOL START"));
	ae_gtp_start_text(NULL, 0, AE_GTW_PISTOL, 0, text, sizeof(text));
	CHECK(!strcmp(text, "AR START"));               /* the category's: the set's assault rifle */
	ae_gtp_start_text(NULL, 0, 0, 4, text, sizeof(text));
	CHECK(!strcmp(text, "SNIPER START"));           /* the sniping set */
	ae_gtp_start_text(NULL, 0, 0, 6, text, sizeof(text));
	CHECK(!strcmp(text, "ROCKET START"));
	ae_gtp_start_text(NULL, 0, 0, 1, text, sizeof(text));
	CHECK(!strcmp(text, "PISTOL START"));           /* the pistols set */
	CHECK(ae_gtp_weapon_in_set(AE_GTW_PISTOL, 1) == AE_GTW_PLASMA_PISTOL);
	CHECK(ae_gtp_weapon_in_set(AE_GTW_SHOTGUN, 4) == AE_GTW_SHOTGUN);
	CHECK(!strcmp(ae_gtp_weapon_name(AE_GTW_ASSAULT_RIFLE), "ASSAULT RIFLE") && !ae_gtp_weapon_name(99)[0]);
	CHECK(!strcmp(ae_gtp_folder_name(AE_GTP_FOLDER_ACTION_SACK), "AE ACTION SACK") && !ae_gtp_folder_name(-1)[0]);
	CHECK(!strcmp(ae_gtp_filter_name(AE_GTF_ODDBALL), "ODDBALL"));
	CHECK(ae_gtp_filter_of_engine(AE_SETUP_ENGINE_ODDBALL) == AE_GTF_ODDBALL &&
		ae_gtp_filter_of_engine(0) == AE_GTF_ALL);
	/* (Bomb, engine 6: AE GRIFBALL and AE BOMB, the stock Bomb / Grifball / Open Pitch) */
	CHECK(!strcmp(ae_gtp_filter_name(AE_GTF_BOMB), "BOMB") && AE_GTF_BOMB == AE_GTP_FILTERS - 1 &&
		ae_gtp_filter_of_engine(AE_SETUP_ENGINE_BOMB) == AE_GTF_BOMB && !ae_gtp_filter_name(AE_GTP_FILTERS)[0]);

	/* the gametypes: a pair in AE FFA and one in AE 4V4 whose labels would clash, a 2v2 pair given BR first, the
	OCTAGON drills (their BR is no pair's), an NHE one, a stock one, the player's own */
	memset(&gtp, 0, sizeof(gtp));
	ffa_br = add("FFA AE BR SLAYER", AE_GTP_FOLDER_FFA, 1, AE_SETUP_ENGINE_SLAYER, 1);
	ffa = add("FFA AE SLAYER", AE_GTP_FOLDER_FFA, 0, AE_SETUP_ENGINE_SLAYER, 1);
	team = add("TEAM AE SLAYER", AE_GTP_FOLDER_4V4, 0, AE_SETUP_ENGINE_SLAYER, 1);
	team_br = add("TEAM AE BR SLAYER", AE_GTP_FOLDER_4V4, 1, AE_SETUP_ENGINE_SLAYER, 1);
	ctf2 = add("TEAM AE 2V2 CTF", AE_GTP_FOLDER_2V2, 0, AE_SETUP_ENGINE_CTF, 1);
	ctf2_br = add("TEAM AE BR 2V2 CTF", AE_GTP_FOLDER_2V2, 1, AE_SETUP_ENGINE_CTF, 1);
	octagon = add("FFA AE OCTAGON BR", AE_GTP_FOLDER_PRACTICE, 0, AE_SETUP_ENGINE_SLAYER, 1);
	octagon_snipe = add("FFA AE OCTAGON BR SNIPE", AE_GTP_FOLDER_PRACTICE, 1, AE_SETUP_ENGINE_SLAYER, 1);
	nhe_ctf = add("TEAM NHE CTF 3 7.5S", AE_GTP_FOLDER_NHE, 0, AE_SETUP_ENGINE_CTF, 0);
	stock_ctf = add("CTF", AE_GTP_FOLDER_STOCK, 0, AE_SETUP_ENGINE_CTF, 0);
	mine = add("NIGHT SNIPE", AE_GTP_FOLDER_MINE, 0, AE_SETUP_ENGINE_ODDBALL, 0);
	ae_gtp_finish(&gtp);

	/* pairs: by the BR word after AE; the drills' BR is no pair's */
	CHECK(gtp.gametypes[ffa].twin == ffa_br && gtp.gametypes[ffa_br].twin == ffa && gtp.gametypes[ffa_br].br);
	CHECK(gtp.gametypes[ctf2].twin == ctf2_br && ae_gtp_base(&gtp, ctf2_br) == ctf2 && ae_gtp_base(&gtp, ctf2) == ctf2);
	CHECK(gtp.gametypes[octagon].twin < 0 && !gtp.gametypes[octagon].br && gtp.gametypes[octagon_snipe].twin < 0);
	CHECK(ae_gtp_has_arrows(&gtp, ffa) && !ae_gtp_has_arrows(&gtp, octagon) && !ae_gtp_has_arrows(&gtp, nhe_ctf));
	/* labels: the BR word out, TEAM / FFA out unless two would read the same */
	CHECK(!strcmp(gtp.gametypes[ctf2].row, "AE 2V2 CTF") && !strcmp(gtp.gametypes[ctf2_br].row, "AE 2V2 CTF"));
	CHECK(!strcmp(gtp.gametypes[ffa].row, "FFA AE SLAYER") && !strcmp(gtp.gametypes[team].row, "TEAM AE SLAYER"));
	CHECK(!strcmp(gtp.gametypes[team_br].row, "TEAM AE SLAYER"));
	CHECK(!strcmp(gtp.gametypes[octagon].row, "AE OCTAGON BR") && !strcmp(gtp.gametypes[nhe_ctf].row, "NHE CTF 3 7.5S"));
	CHECK(!strcmp(gtp.gametypes[stock_ctf].row, "CTF"));

	/* counts: pairs once */
	CHECK(ae_gtp_count(&gtp, AE_GTF_ALL, NULL) == 8);
	CHECK(ae_gtp_count(&gtp, AE_GTF_CTF, NULL) == 3);
	CHECK(ae_gtp_count(&gtp, AE_GTF_ODDBALL, NULL) == 1 && ae_gtp_count(&gtp, AE_GTF_RACE, NULL) == 0);
	CHECK(ae_gtp_folder_count(&gtp, AE_GTP_FOLDER_FFA, AE_GTF_ALL, NULL) == 1);

	/* the list closed: FAVOURITES (listed when empty), the home folders' headers in order, the empty ones out */
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, -1, rows, 64);
	CHECK(count == 8);
	CHECK(rows[0].folder == AE_GTP_FOLDER_FAVOURITES && rows[1].folder == AE_GTP_FOLDER_MINE && rows[1].gametype < 0 &&
		rows[7].folder == AE_GTP_FOLDER_STOCK);
	CHECK(ae_gtp_header_row(rows, count, AE_GTP_FOLDER_BTB) < 0);
	CHECK(!strcmp(ae_gtp_folder_name(AE_GTP_FOLDER_OBJECTIVE), "AE OBJECTIVE"));
	/* AE FFA open: its one row (the pair's base) after its header */
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_FFA, rows, 64);
	CHECK(count == 9 && rows[3].folder == AE_GTP_FOLDER_FFA && rows[3].gametype == ffa);
	CHECK(ae_gtp_row_gametype(&gtp, rows[3]) == ffa);
	/* the toggle: the pair's BR one shown, and back */
	CHECK(ae_gtp_toggle(&gtp, rows[3]) && ae_gtp_row_gametype(&gtp, rows[3]) == ffa_br);
	CHECK(ae_gtp_toggle(&gtp, rows[3]) && ae_gtp_row_gametype(&gtp, rows[3]) == ffa);
	CHECK(!ae_gtp_toggle(&gtp, rows[0]));                 /* a header */
	/* the CTF filter: only the folders with CTF (FAVOURITES listed still) */
	count = ae_gtp_rows(&gtp, AE_GTF_CTF, NULL, -1, rows, 64);
	CHECK(count == 4 && rows[1].folder == AE_GTP_FOLDER_2V2 && rows[3].folder == AE_GTP_FOLDER_STOCK);
	/* LT / RT: folder to folder, wrapping */
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_2V2, rows, 64);
	CHECK(ae_gtp_folder_step(rows, count, ae_gtp_header_row(rows, count, AE_GTP_FOLDER_2V2), 1) ==
		ae_gtp_header_row(rows, count, AE_GTP_FOLDER_4V4));
	CHECK(ae_gtp_folder_step(rows, count, 0, -1) == ae_gtp_header_row(rows, count, AE_GTP_FOLDER_STOCK));
	CHECK(ae_gtp_folder_step(rows, count, find_row(rows, count, AE_GTP_FOLDER_2V2, ctf2), 1) ==
		ae_gtp_header_row(rows, count, AE_GTP_FOLDER_4V4));

	/* RECENT: the gametypes played, newest first, each once (owner 2026-10-28: RECENT per picker); FAVOURITES: the
	pair's */
	ae_gtp_recent_put(&gtp, ctf2_br);
	ae_gtp_recent_put(&gtp, stock_ctf);
	ae_gtp_recent_put(&gtp, ctf2_br);                     /* (in RECENT already: not put again) */
	ae_gtp_recent_put(&gtp, 999);                         /* (no such gametype: not put) */
	CHECK(gtp.recent_count == 2 && gtp.recent[0] == ctf2_br && gtp.recent[1] == stock_ctf);
	/* a battle rifle pair is one row: the half played last (put first); the older half, either one, is not put */
	ae_gtp_recent_put(&gtp, ctf2);
	CHECK(gtp.recent_count == 2 && gtp.recent[0] == ctf2_br);
	ae_gtp_favourite(&gtp, team_br);
	CHECK(gtp.gametypes[team].favourite && !gtp.gametypes[team_br].favourite);
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, -1, rows, 64);
	CHECK(rows[0].folder == AE_GTP_FOLDER_RECENT && rows[0].gametype < 0 && rows[1].gametype == 0 &&
		rows[2].gametype == 1);
	CHECK(ae_gtp_row_gametype(&gtp, rows[1]) == ctf2_br && ae_gtp_row_gametype(&gtp, rows[2]) == stock_ctf);
	CHECK(rows[3].folder == AE_GTP_FOLDER_FAVOURITES && rows[3].gametype < 0 && rows[4].folder == AE_GTP_FOLDER_MINE);
	CHECK(ae_gtp_folder_step(rows, count, 1, 1) == 3);  /* (from RECENT's row: FAVOURITES' header) */
	CHECK(ae_gtp_folder_step(rows, count, 3, -1) == count - 1);
	/* RECENT folded: its header only, whatever folder is open; open by default; unfolded again */
	CHECK(!gtp.recent_folded);
	gtp.recent_folded = 1;
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, -1, rows, 64);
	CHECK(rows[0].folder == AE_GTP_FOLDER_RECENT && rows[0].gametype < 0 && rows[1].folder == AE_GTP_FOLDER_FAVOURITES);
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_FAVOURITES, rows, 64);
	CHECK(rows[0].folder == AE_GTP_FOLDER_RECENT && rows[1].folder == AE_GTP_FOLDER_FAVOURITES && rows[2].gametype == team);
	gtp.recent_folded = 0;
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, -1, rows, 64);
	CHECK(rows[1].gametype == 0 && rows[3].folder == AE_GTP_FOLDER_FAVOURITES);
	/* FAVOURITES open: its pair's base */
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_FAVOURITES, rows, 64);
	CHECK(rows[4].folder == AE_GTP_FOLDER_FAVOURITES && rows[4].gametype == team);
	/* a RECENT row has no toggle: the one played is its own */
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, -1, rows, 64);
	CHECK(!ae_gtp_toggle(&gtp, rows[1]) && ae_gtp_row_gametype(&gtp, rows[1]) == ctf2_br);
	/* filtered: RECENT keeps what suits */
	count = ae_gtp_rows(&gtp, AE_GTF_SLAYER, NULL, -1, rows, 64);
	CHECK(rows[0].folder == AE_GTP_FOLDER_FAVOURITES);
	ae_gtp_favourite(&gtp, team);
	CHECK(!gtp.gametypes[team].favourite);

	/* the player's own folders: after FAVOURITES, listed empty, the empty one's line when open */
	gtp.own_count = 2;
	strcpy(gtp.own_names[0], "LAN night");
	strcpy(gtp.own_names[1], "Zed");
	gtp.gametypes[ae_gtp_base(&gtp, ctf2_br)].own = 1u;
	CHECK(!strcmp(ae_gtp_folder_label(&gtp, AE_GTP_FOLDER_OWN + 1), "Zed") && ae_gtp_folder_is_own(AE_GTP_FOLDER_OWN));
	CHECK(!ae_gtp_folder_label(&gtp, AE_GTP_FOLDER_OWN + 2)[0] && !strcmp(ae_gtp_folder_label(&gtp, AE_GTP_FOLDER_NHE), "NHE"));
	CHECK(ae_gtp_folder_count(&gtp, AE_GTP_FOLDER_OWN, AE_GTF_ALL, NULL) == 1);
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_OWN + 1, rows, 64);
	CHECK(rows[3].folder == AE_GTP_FOLDER_FAVOURITES && rows[4].folder == AE_GTP_FOLDER_OWN &&
		rows[5].folder == AE_GTP_FOLDER_OWN + 1 && rows[6].gametype == AE_GTP_ROW_EMPTY &&
		rows[7].folder == AE_GTP_FOLDER_MINE);
	CHECK(ae_gtp_row_gametype(&gtp, rows[6]) < 0 && ae_gtp_folder_step(rows, count, 6, 1) == 7);
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_OWN, rows, 64);
	CHECK(rows[5].folder == AE_GTP_FOLDER_OWN && rows[5].gametype == ctf2);
	gtp.own_count = 0;

	/* search: flat, each row in its folder, the BR word found through the twin, case aside */
	CHECK(ae_gtp_match("AE 2V2 CTF", "ctf") == 7 && ae_gtp_match("AE", "qzx") < 0 && ae_gtp_match("AE", "") < 0);
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, "ctf", -1, rows, 64);
	CHECK(count == 3 && rows[0].gametype == ctf2 && rows[0].folder == AE_GTP_FOLDER_2V2);
	CHECK(rows[1].gametype == nhe_ctf && rows[2].gametype == stock_ctf);
	CHECK(ae_gtp_count(&gtp, AE_GTF_ALL, "ctf") == 3 && ae_gtp_count(&gtp, AE_GTF_SLAYER, "ctf") == 0);
	count = ae_gtp_rows(&gtp, AE_GTF_ALL, "br slayer", -1, rows, 64);
	CHECK(count == 2 && rows[0].gametype == ffa && rows[1].gametype == team);
	CHECK(ae_gtp_rows(&gtp, AE_GTF_ALL, "qzx", -1, rows, 64) == 0);
	CHECK(ae_gtp_rows(&gtp, AE_GTF_ALL, "night", -1, rows, 64) == 1 && rows[0].gametype == mine &&
		rows[0].folder == AE_GTP_FOLDER_MINE);

	/* PLAYS WELL ON: the mode, the size, the preferred group first, a name once */
	memset(&maps, 0, sizeof(maps));
	add_map("Blood Gulch", AE_SETUP_MODE_CTF | AE_SETUP_MODE_SLAYER, 'l', 0);
	add_map("Prisoner", AE_SETUP_MODE_SLAYER, 's', 1u);
	add_map("Battle Creek", AE_SETUP_MODE_CTF | AE_SETUP_MODE_SLAYER, 'm', 1u);
	add_map("Battle Creek", AE_SETUP_MODE_CTF, 'm', 0);
	add_map("Hotbox", AE_SETUP_MODE_CTF, 's', 0);
	maps.maps[maps.count].facts = AE_SETUP_FACTS_NONE;
	maps.count++;
	count = ae_gtp_plays_well(&maps, AE_SETUP_ENGINE_CTF, 2, prefer, indices, 8, &have, &read);
	CHECK(count == 2 && indices[0] == 2 && indices[1] == 4);
	CHECK(have == 4 && read == 5);
	count = ae_gtp_plays_well(&maps, AE_SETUP_ENGINE_CTF, 8, prefer, indices, 8, &have, &read);
	CHECK(count == 1 && indices[0] == 0);
	CHECK(ae_gtp_plays_well(&maps, AE_SETUP_ENGINE_RACE, 0, NULL, indices, 8, &have, &read) == 0 && have == 0);
	CHECK(ae_gtp_plays_well(&maps, AE_SETUP_ENGINE_SLAYER, 0, prefer, indices, 1, NULL, NULL) == 1 && indices[0] == 1);

	/* OpenCE's built-in gametypes: their own folder, OPENCE, after NHE and before STOCK; Infection is Slayer's */
	memset(&gtp, 0, sizeof(gtp));
	{
		short opence_infection, opence_stock;

		opence_stock = add("SLAYER", AE_GTP_FOLDER_STOCK, 0, AE_SETUP_ENGINE_SLAYER, 0);
		opence_infection = add("INFECTION", AE_GTP_FOLDER_OPENCE, 1, AE_SETUP_ENGINE_SLAYER, 0);
		ae_gtp_finish(&gtp);
		count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, -1, rows, 64);
		CHECK(count == 3 && rows[1].folder == AE_GTP_FOLDER_OPENCE && rows[2].folder == AE_GTP_FOLDER_STOCK);
		CHECK(!strcmp(ae_gtp_folder_name(AE_GTP_FOLDER_OPENCE), "OPENCE"));
		CHECK(ae_gtp_count(&gtp, AE_GTF_SLAYER, NULL) == 2);
		count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_OPENCE, rows, 64);
		CHECK(count == 4 && rows[2].gametype == opence_infection && rows[3].folder == AE_GTP_FOLDER_STOCK);
		(void)opence_stock;
	}

	/* (0.7.0) the START switch: PISTOL, BR and EVO ones of a name make one group of three; left / right step through
	it both ways, wrapping; a group without its BR one steps between the two it has; the pips' count and place */
	{
		short plain, br, evo, lone, lone_evo, place_check;
		int place = -1;

		memset(&gtp, 0, sizeof(gtp));
		evo = add("TEAM AE EVO 2V2 CTF", AE_GTP_FOLDER_2V2, 2, AE_SETUP_ENGINE_CTF, 1);
		plain = add("TEAM AE 2V2 CTF", AE_GTP_FOLDER_2V2, 0, AE_SETUP_ENGINE_CTF, 1);
		br = add("TEAM AE BR 2V2 CTF", AE_GTP_FOLDER_2V2, 1, AE_SETUP_ENGINE_CTF, 1);
		lone = add("TEAM AE 2V2 KING", AE_GTP_FOLDER_2V2, 3, AE_SETUP_ENGINE_KING, 1);
		lone_evo = add("TEAM AE EVO 2V2 KING", AE_GTP_FOLDER_2V2, 4, AE_SETUP_ENGINE_KING, 1);
		ae_gtp_finish(&gtp);
		CHECK(gtp.gametypes[evo].evo && !gtp.gametypes[evo].br && gtp.gametypes[br].br && !gtp.gametypes[plain].evo);
		CHECK(gtp.gametypes[plain].twin == br && gtp.gametypes[plain].evo_twin == evo && gtp.gametypes[evo].evo_twin == plain);
		CHECK(ae_gtp_base(&gtp, evo) == plain && ae_gtp_base(&gtp, br) == plain && ae_gtp_base(&gtp, lone_evo) == lone);
		CHECK(!strcmp(gtp.gametypes[evo].row, "AE 2V2 CTF"));
		CHECK(ae_gtp_start_place(&gtp, plain, &place) == 3 && place == 0);
		CHECK(ae_gtp_start_place(&gtp, evo, &place) == 3 && place == 2);
		CHECK(ae_gtp_start_place(&gtp, lone_evo, &place) == 2 && place == 1);
		CHECK(ae_gtp_has_arrows(&gtp, plain) && ae_gtp_has_arrows(&gtp, lone));
		CHECK(ae_gtp_count(&gtp, AE_GTF_ALL, NULL) == 2);
		count = ae_gtp_rows(&gtp, AE_GTF_ALL, NULL, AE_GTP_FOLDER_2V2, rows, 64);
		for (place_check = 0; place_check < count && rows[place_check].gametype != plain; place_check++)
			;
		CHECK(place_check < count);
		if (place_check < count)
		{
			struct ae_gtp_row row = rows[place_check];

			CHECK(ae_gtp_step(&gtp, row, 1) && ae_gtp_row_gametype(&gtp, row) == br);
			CHECK(ae_gtp_step(&gtp, row, 1) && ae_gtp_row_gametype(&gtp, row) == evo);
			CHECK(ae_gtp_step(&gtp, row, 1) && ae_gtp_row_gametype(&gtp, row) == plain);
			CHECK(ae_gtp_step(&gtp, row, -1) && ae_gtp_row_gametype(&gtp, row) == evo);
			CHECK(ae_gtp_step(&gtp, row, -1) && ae_gtp_row_gametype(&gtp, row) == br);
		}
		for (place_check = 0; place_check < count && rows[place_check].gametype != lone; place_check++)
			;
		if (place_check < count)
		{
			struct ae_gtp_row row = rows[place_check];

			CHECK(ae_gtp_step(&gtp, row, 1) && ae_gtp_row_gametype(&gtp, row) == lone_evo);
			CHECK(ae_gtp_step(&gtp, row, 1) && ae_gtp_row_gametype(&gtp, row) == lone);
			CHECK(ae_gtp_step(&gtp, row, -1) && ae_gtp_row_gametype(&gtp, row) == lone_evo);
		}
		CHECK(ae_gtp_suits(&gtp, plain, AE_GTF_ALL, "evo"));
	}

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_gametype_picker_test: ok\n");
	return failures != 0;
}
