#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_menu_music_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_modes(void)
{
	struct ae_menu_music_legacy legacy;

	CHECK(ae_menu_music_mode_parse("game_menus") == AE_MUSIC_MODE_GAME_MENUS);
	CHECK(ae_menu_music_mode_parse("one_song") == AE_MUSIC_MODE_ONE_SONG);
	CHECK(ae_menu_music_mode_parse("my_playlist") == AE_MUSIC_MODE_MY_PLAYLIST);
	CHECK(ae_menu_music_mode_parse("off") == AE_MUSIC_MODE_OFF);
	CHECK(ae_menu_music_mode_parse("classic") == -1);
	CHECK(ae_menu_music_mode_parse("") == -1);
	CHECK(ae_menu_music_mode_parse(NULL) == -1);
	CHECK(!strcmp(ae_menu_music_mode_value(AE_MUSIC_MODE_MY_PLAYLIST), "my_playlist"));
	CHECK(!strcmp(ae_menu_music_mode_label(AE_MUSIC_MODE_GAME_MENUS), "GAME MENUS"));
	CHECK(!strcmp(ae_menu_music_mode_label(AE_MUSIC_MODE_ONE_SONG), "ONE SONG"));
	CHECK(!strcmp(ae_menu_music_mode_label(AE_MUSIC_MODE_MY_PLAYLIST), "MY PLAYLIST"));
	CHECK(!strcmp(ae_menu_music_mode_label(AE_MUSIC_MODE_OFF), "OFF"));
	CHECK(!strcmp(ae_menu_music_mode_value(99), "game_menus"));
	/* the old values */
	CHECK(ae_menu_music_legacy("classic", &legacy) && legacy.mode == AE_MUSIC_MODE_ONE_SONG && !legacy.all_songs);
	CHECK(ae_menu_music_legacy("mcc_shuffle", &legacy) && legacy.mode == AE_MUSIC_MODE_MY_PLAYLIST && legacy.shuffle &&
		legacy.all_songs);
	CHECK(ae_menu_music_legacy("mcc_order", &legacy) && legacy.mode == AE_MUSIC_MODE_MY_PLAYLIST && !legacy.shuffle &&
		legacy.all_songs);
	CHECK(!ae_menu_music_legacy("off", &legacy));
	CHECK(!ae_menu_music_legacy("my_playlist", &legacy));
	CHECK(!ae_menu_music_legacy(NULL, &legacy));
}

static void test_playlist(void)
{
	static struct ae_menu_music_playlist list;
	char text[256];

	ae_menu_music_playlist_parse("", &list);
	CHECK(list.count == 0);
	ae_menu_music_playlist_parse("halo3:a/b;haloreach:c; ;halo3:a/b;;ce:title ", &list);
	CHECK(list.count == 3);
	CHECK(!strcmp(list.ids[0], "halo3:a/b") && !strcmp(list.ids[1], "haloreach:c") && !strcmp(list.ids[2], "ce:title"));
	CHECK(ae_menu_music_playlist_serialize(&list, text, sizeof(text)));
	CHECK(!strcmp(text, "halo3:a/b;haloreach:c;ce:title"));
	/* a text that does not fit is refused whole */
	CHECK(!ae_menu_music_playlist_serialize(&list, text, 10));
	/* add: at the end, once */
	CHECK(ae_menu_music_playlist_add(&list, "halo3:x") == 3);
	CHECK(ae_menu_music_playlist_add(&list, "halo3:x") == -1);
	CHECK(list.count == 4);
	CHECK(ae_menu_music_playlist_find(&list, "haloreach:c") == 1);
	CHECK(ae_menu_music_playlist_find(&list, "nope") == -1);
	/* move: one place, kept inside */
	CHECK(ae_menu_music_playlist_move(&list, 1, -1) == 0);
	CHECK(!strcmp(list.ids[0], "haloreach:c") && !strcmp(list.ids[1], "halo3:a/b"));
	CHECK(ae_menu_music_playlist_move(&list, 0, -1) == 0);
	CHECK(ae_menu_music_playlist_move(&list, 3, 1) == 3);
	CHECK(ae_menu_music_playlist_move(&list, 2, 1) == 3);
	CHECK(!strcmp(list.ids[3], "ce:title") && !strcmp(list.ids[2], "halo3:x"));
	/* remove */
	CHECK(ae_menu_music_playlist_remove(&list, 0));
	CHECK(list.count == 3 && !strcmp(list.ids[0], "halo3:a/b"));
	CHECK(!ae_menu_music_playlist_remove(&list, 9));
	/* a bad entry (too long, control characters) is dropped on reading */
	{
		char big[300], line[400];
		memset(big, 'a', sizeof(big) - 1);
		big[sizeof(big) - 1] = 0;
		snprintf(line, sizeof(line), "x:y;%s;z:w\tq;ok:1", big);
		ae_menu_music_playlist_parse(line, &list);
		CHECK(list.count == 2 && !strcmp(list.ids[0], "x:y") && !strcmp(list.ids[1], "ok:1"));
	}
	/* the maximum */
	{
		char *many = (char *)malloc((size_t)(AE_MUSIC_PLAYLIST_MAXIMUM + 20) * 12);
		int index;

		many[0] = 0;
		for (index = 0; index < AE_MUSIC_PLAYLIST_MAXIMUM + 20; index++)
			sprintf(many + strlen(many), "g:s%d;", index);
		ae_menu_music_playlist_parse(many, &list);
		CHECK(list.count == AE_MUSIC_PLAYLIST_MAXIMUM);
		CHECK(ae_menu_music_playlist_add(&list, "new:one") == -1);
		free(many);
	}
}

static void test_ids(void)
{
	char key[128], id[AE_MUSIC_ID];

	CHECK(ae_menu_music_song_key("sound\\music\\misc\\halo3_trench\\track1", 64, key, sizeof(key)) &&
		!strcmp(key, "misc/halo3_trench/track1"));
	CHECK(ae_menu_music_song_key("Sound\\Music\\Numbers\\9M_Full\\Track1\\loop\\x", 64, key, sizeof(key)) &&
		!strcmp(key, "numbers/9m_full/track1"));
	/* a cinematic's music file keeps its file name */
	CHECK(ae_menu_music_song_key("sound\\cinematics\\010la\\music\\010la_music_1_3.aif", 128, key, sizeof(key)) &&
		!strcmp(key, "010la_music_1_3.aif"));
	CHECK(!ae_menu_music_song_key("sound\\ambient\\wind", 40, key, sizeof(key)));
	CHECK(!ae_menu_music_song_key(NULL, 0, key, sizeof(key)));
	/* songs only: not the ambience, the stems, the stingers */
	CHECK(ae_menu_music_key_is_song("misc/halo3_trench/track1"));
	CHECK(ae_menu_music_key_is_song("main_menu_delta/delta_menu"));
	CHECK(!ae_menu_music_key_is_song("amb/glue/a_glue_low/track1"));
	CHECK(!ae_menu_music_key_is_song("sustain/x/track1"));
	CHECK(!ae_menu_music_key_is_song("amb/perc/perc96/track1"));
	CHECK(!ae_menu_music_key_is_song("stingers"));
	CHECK(!ae_menu_music_key_is_song("050lb_mus2f_heard_1_2/rear_speakers"));
	CHECK(!ae_menu_music_key_is_song("invasion_temp_cues/x"));
	CHECK(!ae_menu_music_key_is_song("atlas/misc/new2/track1/alt_trans_in"));
	CHECK(!ae_menu_music_key_is_song("elevator_music_split_a/track1"));
	CHECK(!ae_menu_music_key_is_song("atlas/misc/x/microglue/track1"));
	CHECK(!ae_menu_music_key_is_song("atlas/firefight/ambrhythm/track1"));
	CHECK(ae_menu_music_key_is_song("tension/tplse2/track1"));
	CHECK(!ae_menu_music_key_is_song(""));
	/* ids */
	CHECK(ae_menu_music_make_id("halo3", "misc/halo3_trench/track1", id, sizeof(id)) &&
		!strcmp(id, "halo3:misc/halo3_trench/track1"));
	CHECK(ae_menu_music_make_id("HaloReach", "Reach_Menu/Track1", id, sizeof(id)) && !strcmp(id, "haloreach:reach_menu/track1"));
	CHECK(!ae_menu_music_make_id("halo3", "misc/halo3_trench/track1", id, 10));
	CHECK(!ae_menu_music_make_id("", "a", id, sizeof(id)));
	/* the menu songs */
	CHECK(ae_menu_music_is_menu_song("halo3:main_menu_delta/delta_menu"));
	CHECK(ae_menu_music_is_menu_song("halo3odst:atlas/menu/track1"));
	CHECK(ae_menu_music_is_menu_song("haloreach:reach_menu/track1"));
	CHECK(ae_menu_music_is_menu_song("ce:title"));
	CHECK(!ae_menu_music_is_menu_song("halo3:misc/halo3_trench/track1"));
	CHECK(!ae_menu_music_is_menu_song("halo3odst:main_menu_delta/delta_menu"));
	CHECK(ae_menu_music_id_valid("halo3:a/b") && !ae_menu_music_id_valid("nocolon") && !ae_menu_music_id_valid(""));
}

static void test_moods(void)
{
	char name[64];
	char const *ids[] = { "ce:title", "halo3:main_menu_delta/delta_menu", "halo3:tension/bed1/track1",
		"halo3:amb/glue/a_glue_low/track1", "halo3:tension/bed2/track1", "halo3:numbers/9m_full/track1",
		"haloreach:tension/bed1/track1", "haloreach:heroic/10_kurz02_nobrass/track1" };

	CHECK(ae_menu_music_mood("ce:title") == AE_MUSIC_MOOD_MENU);
	CHECK(ae_menu_music_mood("haloreach:reach_menu/track1") == AE_MUSIC_MOOD_MENU);
	CHECK(ae_menu_music_mood("halo3:010la_music_1_3.aif") == AE_MUSIC_MOOD_CUTSCENES);
	CHECK(ae_menu_music_mood("halo3:050lb_mus2f_heard_1_2/rear_speakers") == AE_MUSIC_MOOD_CUTSCENES);
	CHECK(ae_menu_music_mood("haloreach:heroic/unrec_loperc/track1") == AE_MUSIC_MOOD_HEROIC);
	CHECK(ae_menu_music_mood("haloreach:action/vga_mt/track2") == AE_MUSIC_MOOD_ACTION);
	CHECK(ae_menu_music_mood("haloreach:perc/01perc/track1") == AE_MUSIC_MOOD_ACTION);
	CHECK(ae_menu_music_mood("halo3odst:atlas/firefight/shiny/track1") == AE_MUSIC_MOOD_ACTION);
	CHECK(ae_menu_music_mood("halo3:amb/perc/perc85/track1") == AE_MUSIC_MOOD_ACTION);
	CHECK(ae_menu_music_mood("haloreach:tension/bed2/track1") == AE_MUSIC_MOOD_TENSION);
	CHECK(ae_menu_music_mood("haloreach:poignant/cv/track1") == AE_MUSIC_MOOD_POIGNANT);
	CHECK(ae_menu_music_mood("halo3:amb/spooky/a_spook/track1") == AE_MUSIC_MOOD_AMBIENT);
	CHECK(ae_menu_music_mood("haloreach:sustain/glue/g_full/track1") == AE_MUSIC_MOOD_AMBIENT);
	CHECK(ae_menu_music_mood("halo3odst:atlas/glue/cm1/track1") == AE_MUSIC_MOOD_AMBIENT);
	CHECK(ae_menu_music_mood("halo3:numbers/9m_full/track1") == AE_MUSIC_MOOD_OTHER);
	CHECK(ae_menu_music_mood("halo3:misc/oaph/track1") == AE_MUSIC_MOOD_OTHER);
	CHECK(!strcmp(ae_menu_music_mood_label(AE_MUSIC_MOOD_POIGNANT), "POIGNANT"));
	CHECK(!strcmp(ae_menu_music_mood_label(AE_MUSIC_MOODS), "OTHER"));
	/* before the catalog is known a layer has its folder's name */
	ae_menu_music_name("halo3:tension/bed2/track1", name, sizeof(name));
	CHECK(!strcmp(name, "TENSION BED"));
	/* a layer: its mood and its number among that mood's songs of its game, in catalog order */
	ae_menu_music_catalog_set(ids, (int)(sizeof(ids) / sizeof(ids[0])));
	ae_menu_music_name("halo3:tension/bed1/track1", name, sizeof(name));
	CHECK(!strcmp(name, "TENSION 1"));
	ae_menu_music_name("halo3:tension/bed2/track1", name, sizeof(name));
	CHECK(!strcmp(name, "TENSION 2"));
	ae_menu_music_name("halo3:amb/glue/a_glue_low/track1", name, sizeof(name));
	CHECK(!strcmp(name, "AMBIENT 1"));
	ae_menu_music_name("haloreach:tension/bed1/track1", name, sizeof(name));
	CHECK(!strcmp(name, "TENSION 1"));
	ae_menu_music_name("haloreach:heroic/10_kurz02_nobrass/track1", name, sizeof(name));
	CHECK(!strcmp(name, "HEROIC 1"));
	/* titled, and other, songs keep their names; an id and its mood stay the same */
	ae_menu_music_name("halo3:main_menu_delta/delta_menu", name, sizeof(name));
	CHECK(!strcmp(name, "HALO 3 MAIN MENU"));
	ae_menu_music_name("halo3:numbers/9m_full/track1", name, sizeof(name));
	CHECK(!strcmp(name, "OUT OF SHADOW"));
	ae_menu_music_catalog_set(NULL, 0);
	ae_menu_music_name("halo3:tension/bed2/track1", name, sizeof(name));
	CHECK(!strcmp(name, "TENSION BED"));
}

static void test_names(void)
{
	char name[64];

	/* the soundtrack titles of the songs whose title is known */
	ae_menu_music_name("halo3:main_menu_delta/delta_menu", name, sizeof(name));
	CHECK(!strcmp(name, "HALO 3 MAIN MENU"));
	ae_menu_music_name("haloreach:reach_menu/track1", name, sizeof(name));
	CHECK(!strcmp(name, "REACH MAIN MENU"));
	ae_menu_music_name("halo3odst:atlas/menu/track1", name, sizeof(name));
	CHECK(!strcmp(name, "ODST MAIN MENU"));
	ae_menu_music_name("halo3:main_menu_delta/delta_menu", name, sizeof(name));
	CHECK(!strcmp(name, "HALO 3 MAIN MENU"));
	ae_menu_music_name("ce:title", name, sizeof(name));
	CHECK(!strcmp(name, "HALO"));
	/* the table's */
	ae_menu_music_name("halo3:misc/halo3_trench/track1", name, sizeof(name));
	CHECK(!strcmp(name, "HALO FINALE"));
	/* a cutscene's cue: the mission where it plays, and the cue */
	ae_menu_music_name("halo3:050lb_mus1_oldfrnd_4_40.aif", name, sizeof(name));
	CHECK(!strcmp(name, "FLOODGATE \xE2\x80\xA2 OLD FRIEND"));
	ae_menu_music_name("haloreach:m45_jorgsac.aif", name, sizeof(name));
	CHECK(!strcmp(name, "LONG NIGHT OF SOLACE \xE2\x80\xA2 JORGE SACRIFICE"));
	ae_menu_music_name("halo3odst:sc110m_out_sc2_sh3_fr1.aif", name, sizeof(name));
	CHECK(!strcmp(name, "UPLIFT RESERVE \xE2\x80\xA2 CUTSCENE 1"));
	/* a mission folder's own mapping is not shared across games: a cutscene file of no known mission keeps its file name */
	ae_menu_music_name("halo3odst:c300m_sc1_sh1_fr2.aif", name, sizeof(name));
	CHECK(!strcmp(name, "C300M SC1 SH1 FR2"));
	/* the fallback: the tidied folder name */
	ae_menu_music_name("halo3:numbers/10m/track1", name, sizeof(name));
	CHECK(!strcmp(name, "10M"));
	ae_menu_music_name("halo3odst:atlas/misc/rain4/track1", name, sizeof(name));
	CHECK(!strcmp(name, "RAIN4"));
	ae_menu_music_name("haloreach:tension/bed2_plse/track1", name, sizeof(name));
	CHECK(!strcmp(name, "BED2 PLSE"));
	ae_menu_music_name("halo3:100xx_music_1_1.aif", name, sizeof(name));
	CHECK(!strcmp(name, "100XX MUSIC 1 1"));
	ae_menu_music_name("halo3:misc/oaph/track1", name, sizeof(name));
	CHECK(name[0] && strlen(name) < sizeof(name));
	/* too small a buffer: cut, still ends */
	ae_menu_music_name("halo3:numbers/11full/track1", name, 4);
	CHECK(!strcmp(name, "11F"));
	/* an id with no table entry gets its own upper-cased name */
	ae_menu_music_name("odd", name, sizeof(name));
	CHECK(!strcmp(name, "ODD"));
	/* games */
	CHECK(!strcmp(ae_menu_music_game_short("halo3:a"), "HALO 3"));
	CHECK(!strcmp(ae_menu_music_game_short("halo3odst:a"), "ODST"));
	CHECK(!strcmp(ae_menu_music_game_short("haloreach:a"), "REACH"));
	CHECK(!strcmp(ae_menu_music_game_short("ce:title"), "HALO CE"));
	CHECK(!strcmp(ae_menu_music_game_full("halo3odst:a"), "HALO 3: ODST"));
	CHECK(!strcmp(ae_menu_music_game_full("haloreach:a"), "HALO: REACH"));
	CHECK(!strcmp(ae_menu_music_game_full("ce:title"), "HALO: COMBAT EVOLVED"));
	CHECK(ae_menu_music_game_order("ce:title") < ae_menu_music_game_order("halo3:x"));
	CHECK(ae_menu_music_game_order("halo3:x") < ae_menu_music_game_order("halo3odst:x"));
	CHECK(ae_menu_music_game_order("halo3odst:x") < ae_menu_music_game_order("haloreach:x"));
}

/* the soundtrack titles of songs by their files: a sound's folder (or a cutscene's file) and its title */
static void test_reference_titles(void)
{
	static struct { char const *id, *title; } const titled[] =
	{
		{ "halo3:numbers/6m_full/track1", "ANOTHER WALK" },
		{ "halo3:numbers/1m_full/track1", "SIERRA 117" },
		{ "halo3:numbers/2m_full/track1", "CROW'S NEST" },
		{ "halo3:numbers/9m_full/track1", "OUT OF SHADOW" },
		{ "halo3:numbers/5_full_nl/track1", "BLACK TIDE" },
		{ "halo3:misc/bia_brass/track1", "LAST OF THE BRAVE" },
		{ "halo3:misc/bia_lo/track1", "THE STORM" },
		{ "halo3:misc/bia_nl/track1", "FOLLOW OUR BROTHERS" },
		{ "halo3:misc/edh2_nl/track1", "BRUTES" },
		{ "halo3:misc/perilous_full/track1", "MAKE THEM PAY" },
		{ "halo3:misc/oaph/track1", "BEHOLD A PALE HORSE" },
		{ "halo3:misc/t&r_overture_nl/track1", "RECONCILED" },
		{ "halo3:misc/halo3_trench/track1", "HALO FINALE" },
		{ "halo3:end_credits", "ROLL CALL" },
		{ "halo3odst:atlas/misc/originalrain_nl/track1", "MOMBASA STREETS" },
		{ "halo3odst:atlas/misc/flowy_full_nl/track1", "CAST ASIDE" },
		{ "halo3odst:atlas/misc/cp_reasonpad_plus/track1", "POSTPARTUM" },
		{ "halo3odst:atlas/misc/badidea_full/track1", "INTO THE BREACH" },
		{ "halo3odst:atlas/misc/variation1a_nl/track1", "MERCY PLEA" },
		{ "halo3odst:misc/bia_brass/track1", "LAST OF THE BRAVE" },
	};
	char name[64];
	size_t index;

	for (index = 0; index < sizeof(titled) / sizeof(titled[0]); index++)
	{
		ae_menu_music_name(titled[index].id, name, sizeof(name));
		if (strcmp(name, titled[index].title))
			printf("title of %s: %s, want %s\n", titled[index].id, name, titled[index].title);
		CHECK(!strcmp(name, titled[index].title));
	}
}

static void test_menu_pieces(void)
{
	char name[64], id[AE_MUSIC_ID], stem[32];
	static struct { char const *stem, *title; } const pieces[] =
	{
		{ "opening_monks", "OPENING MONKS" }, { "unforgotten", "NEVER FORGET" }, { "new_dreamy", "CHOOSE WISELY" },
		{ "from_spot", "STARRY NIGHT" }, { "edh1", "ENOUGH DEAD HEROES" }, { "monks_and_fx", "MONKS AND EFFECTS" }
	};
	size_t index;

	/* a piece's file name, lower case, without ".aif" */
	CHECK(ae_menu_music_piece_stem("data\\sound\\music\\main_menu_delta\\delta_menu\\loop\\New_Dreamy.aif", 100, stem,
		sizeof(stem)) && !strcmp(stem, "new_dreamy"));
	CHECK(ae_menu_music_piece_stem("sound/music/atlas/menu/track1/in/in.aif", 100, stem, sizeof(stem)) && !strcmp(stem, "in"));
	CHECK(!ae_menu_music_piece_stem("sound\\music\\x\\", 100, stem, sizeof(stem)));
	CHECK(!ae_menu_music_piece_stem(NULL, 0, stem, sizeof(stem)));
	/* only Halo 3's menu sound is listed by its pieces: the others have no titled pieces */
	CHECK(ae_menu_music_lists_pieces("halo3:main_menu_delta/delta_menu"));
	CHECK(!ae_menu_music_lists_pieces("halo3odst:atlas/menu/track1"));
	CHECK(!ae_menu_music_lists_pieces("haloreach:reach_menu/track1"));
	CHECK(!ae_menu_music_lists_pieces("halo3:misc/halo3_trench/track1"));
	/* a piece's id: the sound's id and the permutation */
	CHECK(ae_menu_music_piece_id("halo3:main_menu_delta/delta_menu", "from_spot", id, sizeof(id)) &&
		!strcmp(id, "halo3:main_menu_delta/delta_menu/from_spot"));
	CHECK(!ae_menu_music_piece_id("halo3:main_menu_delta/delta_menu", "", id, sizeof(id)));
	CHECK(!ae_menu_music_piece_id("halo3:main_menu_delta/delta_menu", "from_spot", id, 20));
	/* each piece has its title, sits with the menu songs, and is not the game's menu song itself */
	for (index = 0; index < sizeof(pieces) / sizeof(pieces[0]); index++)
	{
		CHECK(ae_menu_music_piece_id("halo3:main_menu_delta/delta_menu", pieces[index].stem, id, sizeof(id)));
		ae_menu_music_name(id, name, sizeof(name));
		CHECK(!strcmp(name, pieces[index].title));
		CHECK(ae_menu_music_mood(id) == AE_MUSIC_MOOD_MENU);
		CHECK(!ae_menu_music_is_menu_song(id));
		CHECK(ae_menu_music_id_valid(id));
	}
}

static void test_text(void)
{
	char text[64];

	ae_menu_music_time_text(370, text, sizeof(text));
	CHECK(!strcmp(text, "6:10"));
	ae_menu_music_time_text(59, text, sizeof(text));
	CHECK(!strcmp(text, "0:59"));
	ae_menu_music_time_text(743, text, sizeof(text));
	CHECK(!strcmp(text, "12:23"));
	ae_menu_music_time_text(-5, text, sizeof(text));
	CHECK(!strcmp(text, "0:00"));
	ae_menu_music_playlist_summary(0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "EMPTY"));
	ae_menu_music_playlist_summary(14, 55 * 60 + 20, text, sizeof(text));
	CHECK(!strcmp(text, "14 SONGS \xE2\x80\xA2 55 MIN"));
	ae_menu_music_playlist_summary(1, 370, text, sizeof(text));
	CHECK(!strcmp(text, "1 SONG \xE2\x80\xA2 6 MIN"));
	ae_menu_music_playlist_summary(2, 40, text, sizeof(text));
	CHECK(!strcmp(text, "2 SONGS \xE2\x80\xA2 1 MIN"));
	ae_menu_music_summary(AE_MUSIC_MODE_GAME_MENUS, 1, NULL, 0, text, sizeof(text));
	CHECK(!strcmp(text, "GAME MENUS \xE2\x80\xA2 SHUFFLE"));
	ae_menu_music_summary(AE_MUSIC_MODE_GAME_MENUS, 0, NULL, 0, text, sizeof(text));
	CHECK(!strcmp(text, "GAME MENUS"));
	ae_menu_music_summary(AE_MUSIC_MODE_MY_PLAYLIST, 1, NULL, 0, text, sizeof(text));
	CHECK(!strcmp(text, "MY PLAYLIST \xE2\x80\xA2 SHUFFLE"));
	ae_menu_music_summary(AE_MUSIC_MODE_OFF, 1, NULL, 0, text, sizeof(text));
	CHECK(!strcmp(text, "OFF"));
	ae_menu_music_summary(AE_MUSIC_MODE_ONE_SONG, 1, "halo3:misc/halo3_trench/track1", 0, text, sizeof(text));
	CHECK(!strcmp(text, "HALO FINALE \xE2\x80\xA2 HALO 3"));
	/* MCC gone: a saved choice says why */
	ae_menu_music_summary(AE_MUSIC_MODE_MY_PLAYLIST, 1, NULL, 1, text, sizeof(text));
	CHECK(!strcmp(text, "MY PLAYLIST \xE2\x80\xA2 NEEDS MCC"));
	ae_menu_music_summary(AE_MUSIC_MODE_ONE_SONG, 1, "halo3:x", 1, text, sizeof(text));
	CHECK(!strcmp(text, "ONE SONG \xE2\x80\xA2 NEEDS MCC"));
	ae_menu_music_summary(AE_MUSIC_MODE_GAME_MENUS, 1, NULL, 1, text, sizeof(text));
	CHECK(!strcmp(text, "GAME MENUS \xE2\x80\xA2 SHUFFLE"));
}

static struct ae_menu_music_entry catalog[8];
static int catalog_count;

static void entry(char const *id, int menu)
{
	snprintf(catalog[catalog_count].id, sizeof(catalog[catalog_count].id), "%s", id);
	catalog[catalog_count].menu = menu;
	catalog_count++;
}

static void test_sequence(void)
{
	static struct ae_menu_music_playlist list;
	int order[16], effective, missing, count, index, seen[8], round;

	catalog_count = 0;
	entry("ce:title", 1);
	entry("halo3:menu", 1);
	entry("halo3:a", 0);
	entry("halo3odst:menu", 1);
	entry("haloreach:menu", 1);
	entry("haloreach:b", 0);

	/* GAME MENUS in order: every menu song, in catalog order */
	count = ae_menu_music_sequence(AE_MUSIC_MODE_GAME_MENUS, 0, catalog, catalog_count, "", &list, 1, -1, order, 16,
		&effective, &missing);
	CHECK(count == 4 && order[0] == 0 && order[1] == 1 && order[2] == 3 && order[3] == 4);
	CHECK(effective == AE_MUSIC_MODE_GAME_MENUS && missing == 0);
	/* shuffled: the same songs, and never the song that just played first */
	for (round = 0; round < 200; round++)
	{
		memset(seen, 0, sizeof(seen));
		count = ae_menu_music_sequence(AE_MUSIC_MODE_GAME_MENUS, 1, catalog, catalog_count, "", &list,
			(unsigned)round * 7919u + 1u, 3, order, 16, &effective, &missing);
		CHECK(count == 4);
		for (index = 0; index < count; index++)
			seen[order[index]]++;
		CHECK(seen[0] == 1 && seen[1] == 1 && seen[3] == 1 && seen[4] == 1);
		CHECK(order[0] != 3);
	}
	/* only the title theme (no MCC): one song, even shuffled, repeating is all there is */
	catalog_count = 1;
	count = ae_menu_music_sequence(AE_MUSIC_MODE_GAME_MENUS, 1, catalog, catalog_count, "", &list, 5, 0, order, 16,
		&effective, &missing);
	CHECK(count == 1 && order[0] == 0);
	catalog_count = 0;
	entry("ce:title", 1);
	entry("halo3:menu", 1);
	entry("halo3:a", 0);
	entry("halo3odst:menu", 1);
	entry("haloreach:menu", 1);
	entry("haloreach:b", 0);
	/* ONE SONG */
	count = ae_menu_music_sequence(AE_MUSIC_MODE_ONE_SONG, 1, catalog, catalog_count, "halo3:a", &list, 5, -1, order, 16,
		&effective, &missing);
	CHECK(count == 1 && order[0] == 2 && effective == AE_MUSIC_MODE_ONE_SONG);
	/* a song gone: GAME MENUS plays */
	count = ae_menu_music_sequence(AE_MUSIC_MODE_ONE_SONG, 1, catalog, catalog_count, "halo3:gone", &list, 5, -1, order,
		16, &effective, &missing);
	CHECK(count == 4 && effective == AE_MUSIC_MODE_GAME_MENUS);
	/* MY PLAYLIST in order: the list's, the missing skipped and counted */
	ae_menu_music_playlist_parse("haloreach:b;gone:1;halo3:a;ce:title;gone:2", &list);
	count = ae_menu_music_sequence(AE_MUSIC_MODE_MY_PLAYLIST, 0, catalog, catalog_count, "", &list, 5, -1, order, 16,
		&effective, &missing);
	CHECK(count == 3 && order[0] == 5 && order[1] == 2 && order[2] == 0 && missing == 2);
	CHECK(effective == AE_MUSIC_MODE_MY_PLAYLIST);
	/* shuffled: the same three */
	memset(seen, 0, sizeof(seen));
	count = ae_menu_music_sequence(AE_MUSIC_MODE_MY_PLAYLIST, 1, catalog, catalog_count, "", &list, 12345, 5, order, 16,
		&effective, &missing);
	CHECK(count == 3);
	for (index = 0; index < count; index++)
		seen[order[index]]++;
	CHECK(seen[5] == 1 && seen[2] == 1 && seen[0] == 1 && order[0] != 5);
	/* none of the list found, or an empty list: GAME MENUS */
	ae_menu_music_playlist_parse("gone:1;gone:2", &list);
	count = ae_menu_music_sequence(AE_MUSIC_MODE_MY_PLAYLIST, 0, catalog, catalog_count, "", &list, 5, -1, order, 16,
		&effective, &missing);
	CHECK(count == 4 && effective == AE_MUSIC_MODE_GAME_MENUS && missing == 2);
	ae_menu_music_playlist_parse("", &list);
	count = ae_menu_music_sequence(AE_MUSIC_MODE_MY_PLAYLIST, 0, catalog, catalog_count, "", &list, 5, -1, order, 16,
		&effective, &missing);
	CHECK(count == 4 && effective == AE_MUSIC_MODE_GAME_MENUS);
	/* OFF */
	count = ae_menu_music_sequence(AE_MUSIC_MODE_OFF, 1, catalog, catalog_count, "", &list, 5, -1, order, 16, &effective,
		&missing);
	CHECK(count == 0 && effective == AE_MUSIC_MODE_OFF);
	/* the room: at most maximum */
	count = ae_menu_music_sequence(AE_MUSIC_MODE_GAME_MENUS, 0, catalog, catalog_count, "", &list, 5, -1, order, 2,
		&effective, &missing);
	CHECK(count == 2);
	/* the same seed, the same order */
	{
		int again[16], first[16];

		ae_menu_music_sequence(AE_MUSIC_MODE_GAME_MENUS, 1, catalog, catalog_count, "", &list, 77, -1, first, 16,
			&effective, &missing);
		ae_menu_music_sequence(AE_MUSIC_MODE_GAME_MENUS, 1, catalog, catalog_count, "", &list, 77, -1, again, 16,
			&effective, &missing);
		CHECK(!memcmp(first, again, sizeof(int) * 4));
	}
}

static void test_reshuffle(void)
{
	int order[5] = { 0, 1, 2, 3, 4 }, round, index, seen[5];

	for (round = 0; round < 100; round++)
	{
		int last = order[4];

		ae_menu_music_reshuffle(order, 5, (uint32_t)round * 31u + 3u, last);
		memset(seen, 0, sizeof(seen));
		for (index = 0; index < 5; index++)
			seen[order[index]]++;
		CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1 && seen[3] == 1 && seen[4] == 1);
		CHECK(order[0] != last);
	}
	/* one song: nothing to change */
	order[0] = 7;
	ae_menu_music_reshuffle(order, 1, 5, 7);
	CHECK(order[0] == 7);
}

static void test_mode_switching(void)
{
	/* the first song added to an empty playlist makes it the mode; later edits switch nothing */
	CHECK(ae_menu_music_mode_after_add(AE_MUSIC_MODE_GAME_MENUS, 0) == AE_MUSIC_MODE_MY_PLAYLIST);
	CHECK(ae_menu_music_mode_after_add(AE_MUSIC_MODE_OFF, 0) == AE_MUSIC_MODE_MY_PLAYLIST);
	CHECK(ae_menu_music_mode_after_add(AE_MUSIC_MODE_GAME_MENUS, 3) == AE_MUSIC_MODE_GAME_MENUS);
	CHECK(ae_menu_music_mode_after_add(AE_MUSIC_MODE_ONE_SONG, 1) == AE_MUSIC_MODE_ONE_SONG);
	/* leaving MY PLAYLIST empty: back to the mode before */
	CHECK(ae_menu_music_mode_after_leave(AE_MUSIC_MODE_MY_PLAYLIST, AE_MUSIC_MODE_ONE_SONG, 0) == AE_MUSIC_MODE_ONE_SONG);
	CHECK(ae_menu_music_mode_after_leave(AE_MUSIC_MODE_MY_PLAYLIST, AE_MUSIC_MODE_MY_PLAYLIST, 0) ==
		AE_MUSIC_MODE_GAME_MENUS);
	CHECK(ae_menu_music_mode_after_leave(AE_MUSIC_MODE_MY_PLAYLIST, AE_MUSIC_MODE_OFF, 2) == AE_MUSIC_MODE_MY_PLAYLIST);
	CHECK(ae_menu_music_mode_after_leave(AE_MUSIC_MODE_GAME_MENUS, AE_MUSIC_MODE_OFF, 0) == AE_MUSIC_MODE_GAME_MENUS);
	/* the next song, round the sequence */
	CHECK(ae_menu_music_advance(0, 4) == 1 && ae_menu_music_advance(3, 4) == 0 && ae_menu_music_advance(0, 1) == 0 &&
		ae_menu_music_advance(0, 0) == 0);
	/* a mode stepped by left / right: round the four */
	CHECK(ae_menu_music_mode_step(AE_MUSIC_MODE_GAME_MENUS, 1) == AE_MUSIC_MODE_ONE_SONG);
	CHECK(ae_menu_music_mode_step(AE_MUSIC_MODE_OFF, 1) == AE_MUSIC_MODE_GAME_MENUS);
	CHECK(ae_menu_music_mode_step(AE_MUSIC_MODE_GAME_MENUS, -1) == AE_MUSIC_MODE_OFF);
	/* SHUFFLE's row: why it is off */
	CHECK(ae_menu_music_shuffle_reason(AE_MUSIC_MODE_GAME_MENUS, 0) == NULL);
	CHECK(ae_menu_music_shuffle_reason(AE_MUSIC_MODE_MY_PLAYLIST, 0) == NULL);
	CHECK(!strcmp(ae_menu_music_shuffle_reason(AE_MUSIC_MODE_OFF, 0), "MUSIC IS OFF"));
	CHECK(!strcmp(ae_menu_music_shuffle_reason(AE_MUSIC_MODE_ONE_SONG, 0), "ONE SONG ONLY"));
	CHECK(!strcmp(ae_menu_music_shuffle_reason(AE_MUSIC_MODE_GAME_MENUS, 1), "NEEDS MCC"));
}

int main(void)
{
	test_modes();
	test_playlist();
	test_ids();
	test_names();
	test_reference_titles();
	test_menu_pieces();
	test_moods();
	test_text();
	test_sequence();
	test_reshuffle();
	test_mode_switching();
	if (failures)
	{
		printf("ae_menu_music_rules_test: %d failed\n", failures);
		return 1;
	}
	printf("ae_menu_music_rules_test: ok\n");
	return 0;
}
