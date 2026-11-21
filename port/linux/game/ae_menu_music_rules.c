/*
AE_MENU_MUSIC_RULES.C

MENU MUSIC's rules (ae_menu_music_rules.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_menu_music_rules.h"

#define BULLET "\xE2\x80\xA2"

static char lower_of(char c)
{
	return (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
}

static char upper_of(char c)
{
	return (char)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
}

/* ---------- modes */

static char const *const mode_values[AE_MUSIC_MODES] = { "game_menus", "one_song", "my_playlist", "off" };
static char const *const mode_labels[AE_MUSIC_MODES] = { "GAME MENUS", "ONE SONG", "MY PLAYLIST", "OFF" };

char const *ae_menu_music_mode_value(int mode)
{
	return mode >= 0 && mode < AE_MUSIC_MODES ? mode_values[mode] : mode_values[0];
}

char const *ae_menu_music_mode_label(int mode)
{
	return mode >= 0 && mode < AE_MUSIC_MODES ? mode_labels[mode] : mode_labels[0];
}

int ae_menu_music_mode_parse(char const *text)
{
	int mode;

	if (!text)
		return -1;
	for (mode = 0; mode < AE_MUSIC_MODES; mode++)
		if (!strcmp(text, mode_values[mode]))
			return mode;
	return -1;
}

int ae_menu_music_mode_step(int mode, int direction)
{
	if (mode < 0 || mode >= AE_MUSIC_MODES)
		mode = 0;
	return (mode + (direction < 0 ? AE_MUSIC_MODES - 1 : 1)) % AE_MUSIC_MODES;
}

int ae_menu_music_legacy(char const *text, struct ae_menu_music_legacy *legacy)
{
	if (!text)
		return 0;
	if (!strcmp(text, "classic"))
	{
		legacy->mode = AE_MUSIC_MODE_ONE_SONG;
		legacy->shuffle = 1;
		legacy->all_songs = 0;
		return 1;
	}
	if (!strcmp(text, "mcc_shuffle") || !strcmp(text, "mcc_order"))
	{
		legacy->mode = AE_MUSIC_MODE_MY_PLAYLIST;
		legacy->shuffle = !strcmp(text, "mcc_shuffle");
		legacy->all_songs = 1;
		return 1;
	}
	return 0;
}

/* ---------- the playlist */

int ae_menu_music_id_valid(char const *id)
{
	size_t length, index;
	char const *colon;

	if (!id)
		return 0;
	length = strlen(id);
	if (length < 3 || length >= AE_MUSIC_ID)
		return 0;
	for (index = 0; index < length; index++)
		if ((unsigned char)id[index] < 0x20 || id[index] == 0x7F || id[index] == ';')
			return 0;
	colon = strchr(id, ':');
	return colon && colon != id && colon[1] != 0;
}

void ae_menu_music_playlist_parse(char const *text, struct ae_menu_music_playlist *list)
{
	char id[AE_MUSIC_ID + 1];

	list->count = 0;
	while (text && *text)
	{
		size_t length = 0, start;
		char const *end = strchr(text, ';');
		size_t span = end ? (size_t)(end - text) : strlen(text);

		start = 0;
		while (start < span && text[start] == ' ')
			start++;
		length = span;
		while (length > start && text[length - 1] == ' ')
			length--;
		length -= start;
		if (length > 0 && length < sizeof(id))
		{
			memcpy(id, text + start, length);
			id[length] = 0;
			if (ae_menu_music_id_valid(id) && ae_menu_music_playlist_find(list, id) < 0 &&
				list->count < AE_MUSIC_PLAYLIST_MAXIMUM)
				memcpy(list->ids[list->count++], id, length + 1);
		}
		text = end ? end + 1 : text + span;
	}
}

int ae_menu_music_playlist_serialize(struct ae_menu_music_playlist const *list, char *text, size_t size)
{
	size_t used = 0;
	int index;

	if (!size)
		return 0;
	text[0] = 0;
	for (index = 0; index < list->count; index++)
	{
		size_t length = strlen(list->ids[index]);

		if (used + length + (index ? 1 : 0) + 1 > size)
		{
			text[0] = 0;
			return 0;
		}
		if (index)
			text[used++] = ';';
		memcpy(text + used, list->ids[index], length + 1);
		used += length;
	}
	return 1;
}

int ae_menu_music_playlist_find(struct ae_menu_music_playlist const *list, char const *id)
{
	int index;

	for (index = 0; index < list->count; index++)
		if (!strcmp(list->ids[index], id))
			return index;
	return -1;
}

int ae_menu_music_playlist_add(struct ae_menu_music_playlist *list, char const *id)
{
	if (!ae_menu_music_id_valid(id) || list->count >= AE_MUSIC_PLAYLIST_MAXIMUM ||
		ae_menu_music_playlist_find(list, id) >= 0)
		return -1;
	memcpy(list->ids[list->count], id, strlen(id) + 1);
	return list->count++;
}

int ae_menu_music_playlist_remove(struct ae_menu_music_playlist *list, int index)
{
	if (index < 0 || index >= list->count)
		return 0;
	memmove(list->ids[index], list->ids[index + 1], (size_t)(list->count - index - 1) * AE_MUSIC_ID);
	list->count--;
	return 1;
}

int ae_menu_music_playlist_move(struct ae_menu_music_playlist *list, int index, int direction)
{
	int other = index + (direction < 0 ? -1 : 1);
	char swap[AE_MUSIC_ID];

	if (index < 0 || index >= list->count || other < 0 || other >= list->count)
		return index;
	memcpy(swap, list->ids[index], AE_MUSIC_ID);
	memcpy(list->ids[index], list->ids[other], AE_MUSIC_ID);
	memcpy(list->ids[other], swap, AE_MUSIC_ID);
	return other;
}

/* ---------- songs: keys, ids, names */

int ae_menu_music_song_key(char const *path, size_t size, char *key, size_t key_size)
{
	char lower[256], rest[256];
	char const *music, *at;
	size_t index, length = 0;
	int cinematic;

	if (!path || !key_size)
		return 0;
	for (index = 0; index < size && index + 1 < sizeof(lower) && path[index]; index++)
	{
		char c = lower_of(path[index]);

		lower[index] = c == '\\' ? '/' : c;
	}
	lower[index] = 0;
	music = strstr(lower, "sound/music/");
	if (music)
		at = music + 12;
	else if ((music = strstr(lower, "/music/")) != NULL)
		at = music + 7;
	else if (strstr(lower, "sound/cinematics/") && strrchr(lower, '/') && strstr(strrchr(lower, '/') + 1, "music"))
		at = strrchr(lower, '/') + 1;
	else
		return 0;
	cinematic = 0;
	(void)cinematic;
	snprintf(rest, sizeof(rest), "%s", at);
	/* (the song's own folders, up to the pieces': in, loop, out, their alternates; a file ends it) */
	at = rest;
	key[0] = 0;
	while (*at)
	{
		char const *slash = strchr(at, '/');
		size_t part = slash ? (size_t)(slash - at) : strlen(at);
		int stop = (part == 2 && !strncmp(at, "in", 2)) || (part == 4 && !strncmp(at, "loop", 4)) ||
			(part == 3 && !strncmp(at, "out", 3)) || (part == 8 && !strncmp(at, "alt_loop", 8)) ||
			(part == 6 && !strncmp(at, "alt_in", 6)) || (part == 7 && !strncmp(at, "alt_out", 7)) ||
			(part >= 4 && !strncmp(at + part - 4, ".aif", 4));

		if (stop)
			break;
		if (length && length + 1 < key_size)
			key[length++] = '/';
		if (length + part + 1 > key_size)
			return 0;
		memcpy(key + length, at, part);
		length += part;
		key[length] = 0;
		at = slash ? slash + 1 : at + part;
	}
	if (!length)
	{
		/* (a file: its whole name, as the song) */
		if (strlen(rest) + 1 > key_size)
			return 0;
		memcpy(key, rest, strlen(rest) + 1);
		length = strlen(key);
	}
	/* (a file's own name: the last part only) */
	if (strchr(key, '/') && strlen(key) > 4 && !strcmp(key + strlen(key) - 4, ".aif"))
		memmove(key, strrchr(key, '/') + 1, strlen(strrchr(key, '/') + 1) + 1);
	return key[0] != 0;
}

int ae_menu_music_key_is_song(char const *key)
{
	static char const *const words[] = { "amb", "sustain", "glue", "microglue", "pad", "pulse", "perc", "ambrhythm",
		"stingers", "invasion_temp_cues", "rear_speakers" };
	char const *at = key;

	if (!key || !key[0])
		return 0;
	while (*at)
	{
		char const *slash = strchr(at, '/');
		size_t part = slash ? (size_t)(slash - at) : strlen(at), index;

		for (index = 0; index < sizeof(words) / sizeof(words[0]); index++)
			if (strlen(words[index]) == part && !strncmp(at, words[index], part))
				return 0;
		if (part >= 9 && !strncmp(at, "alt_trans", 9))
			return 0;
		if (part >= 20 && !strncmp(at, "elevator_music_split", 20))
			return 0;
		at = slash ? slash + 1 : at + part;
	}
	return 1;
}

int ae_menu_music_make_id(char const *folder, char const *key, char *id, size_t size)
{
	size_t folder_length, key_length, index;

	if (!folder || !key || !folder[0] || !key[0])
		return 0;
	folder_length = strlen(folder);
	key_length = strlen(key);
	if (folder_length + 1 + key_length + 1 > size)
		return 0;
	for (index = 0; index < folder_length; index++)
		id[index] = lower_of(folder[index]);
	id[folder_length] = ':';
	for (index = 0; index < key_length; index++)
		id[folder_length + 1 + index] = lower_of(key[index]);
	id[folder_length + 1 + key_length] = 0;
	return 1;
}

int ae_menu_music_piece_stem(char const *path, size_t size, char *stem, size_t stem_size)
{
	size_t length = 0, start, index;

	if (!path || !stem || !stem_size)
		return 0;
	while (length < size && path[length])
		length++;
	start = length;
	while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\')
		start--;
	if (length - start >= 4 && !strncmp(path + length - 4, ".aif", 4))
		length -= 4;
	if (length <= start || length - start + 1 > stem_size)
		return 0;
	for (index = start; index < length; index++)
		stem[index - start] = lower_of(path[index]);
	stem[length - start] = 0;
	return 1;
}

int ae_menu_music_lists_pieces(char const *id)
{
	return id && !strcmp(id, "halo3:main_menu_delta/delta_menu");
}

int ae_menu_music_piece_id(char const *song_id, char const *stem, char *id, size_t size)
{
	size_t song_length, stem_length;

	if (!song_id || !stem || !stem[0] || !song_id[0])
		return 0;
	song_length = strlen(song_id);
	stem_length = strlen(stem);
	if (song_length + 1 + stem_length + 1 > size)
		return 0;
	memcpy(id, song_id, song_length);
	id[song_length] = '/';
	memcpy(id + song_length + 1, stem, stem_length + 1);
	return 1;
}

int ae_menu_music_is_menu_song(char const *id)
{
	return !strcmp(id, AE_MUSIC_TITLE_ID) || !strcmp(id, "halo3:main_menu_delta/delta_menu") ||
		!strcmp(id, "halo3odst:atlas/menu/track1") || !strcmp(id, "haloreach:reach_menu/track1");
}

/* Soundtrack titles (the game's menu songs) and AE's own short names, keyed by the song's path under sound/music (a
cutscene's music: its file name). */
static struct { char const *key, *name; } const names[] =
{
	/* each game's main menu, as a whole (Halo 3's is also listed by its pieces, below) */
	{ "main_menu_delta/delta_menu", "HALO 3 MAIN MENU" },
	{ "atlas/menu/track1", "ODST MAIN MENU" },
	{ "reach_menu/track1", "REACH MAIN MENU" },
	/* Halo 3's main menu moves between these pieces (a piece: its sound's folder and its file) */
	{ "main_menu_delta/delta_menu/opening_monks", "OPENING MONKS" },
	{ "main_menu_delta/delta_menu/unforgotten", "NEVER FORGET" },
	{ "main_menu_delta/delta_menu/new_dreamy", "CHOOSE WISELY" },
	{ "main_menu_delta/delta_menu/from_spot", "STARRY NIGHT" },
	{ "main_menu_delta/delta_menu/edh1", "ENOUGH DEAD HEROES" },
	{ "main_menu_delta/delta_menu/monks_and_fx", "MONKS AND EFFECTS" },
	/* the songs of a level's music: its soundtrack title, or the level where it plays */
	{ "numbers/1m_full/track1", "SIERRA 117" },
	{ "numbers/2m_full/track1", "CROW'S NEST" },
	{ "numbers/6m_full/track1", "ANOTHER WALK" },
	{ "numbers/9m_full/track1", "OUT OF SHADOW" },
	{ "numbers/5_full_nl/track1", "BLACK TIDE" },
	{ "misc/bia_lo/track1", "THE STORM" },
	{ "misc/bia_nl/track1", "FOLLOW OUR BROTHERS" },
	{ "misc/edh2_nl/track1", "BRUTES" },
	{ "misc/oaph/track1", "BEHOLD A PALE HORSE" },
	{ "atlas/misc/flowy_full_nl/track1", "CAST ASIDE" },
	{ "atlas/misc/cp_reasonpad_plus/track1", "POSTPARTUM" },
	{ "atlas/misc/badidea_full/track1", "INTO THE BREACH" },
	{ "atlas/misc/variation1a_nl/track1", "MERCY PLEA" },
	{ "atlas/misc/originalrain_nl/track1", "MOMBASA STREETS" },
	/* cutscene music: the mission it plays in, and the cue */
	{ "010la_music_1_3.aif", "SIERRA 117 \xE2\x80\xA2 CUTSCENE 1" },
	{ "010la_music2_1_18.aif", "SIERRA 117 \xE2\x80\xA2 CUTSCENE 2" },
	{ "020la_mus_1_2.aif", "CROW'S NEST \xE2\x80\xA2 CUTSCENE 1" },
	{ "020la_mus_prof1_1_3.aif", "CROW'S NEST \xE2\x80\xA2 CUTSCENE 2" },
	{ "020la_mus_hall_2_2.aif", "CROW'S NEST \xE2\x80\xA2 HALL" },
	{ "040lb_music_1_13.aif", "THE STORM \xE2\x80\xA2 CUTSCENE 1" },
	{ "040lb_mus_portal_1_4.aif", "THE STORM \xE2\x80\xA2 PORTAL" },
	{ "050lb_mus1_oldfrnd_4_40.aif", "FLOODGATE \xE2\x80\xA2 OLD FRIEND" },
	{ "050lb_mus2f_heard_1_2/front_speakers", "FLOODGATE \xE2\x80\xA2 CUTSCENE 2 FRONT" },
	{ "050lb_mus2f_heard_1_2/rear_speakers", "FLOODGATE \xE2\x80\xA2 CUTSCENE 2 REAR" },
	{ "070lc_music_1_3.aif", "THE ARK \xE2\x80\xA2 CUTSCENE 1" },
	{ "070lc_music_barrier_8_1.aif", "THE ARK \xE2\x80\xA2 BARRIER" },
	{ "100lc_music_crash_1_1.aif", "THE COVENANT \xE2\x80\xA2 CRASH" },
	{ "100lc_music_hall_2_1.aif", "THE COVENANT \xE2\x80\xA2 HALL" },
	{ "100lc_music_prov_1_2.aif", "THE COVENANT \xE2\x80\xA2 CUTSCENE 1" },
	{ "100ld_music_a_lies_3_13.aif", "THE COVENANT \xE2\x80\xA2 CUTSCENE 2" },
	{ "100ld_music_take_1_21.aif", "THE COVENANT \xE2\x80\xA2 CUTSCENE 3" },
	{ "100ld_music_tent_2_1.aif", "THE COVENANT \xE2\x80\xA2 CUTSCENE 4" },
	{ "c100m_sc1_sh1_fr2.aif", "PREPARE TO DROP \xE2\x80\xA2 CUTSCENE 1" },
	{ "c100m_sc4_sh2_fr28.aif", "PREPARE TO DROP \xE2\x80\xA2 CUTSCENE 2" },
	{ "c200bm_sh1_fr1.aif", "EPILOGUE \xE2\x80\xA2 CUTSCENE 1" },
	{ "c200m_sh1_fr69.aif", "EPILOGUE \xE2\x80\xA2 CUTSCENE 2" },
	{ "l200m_out_sc_sh1_fr1.aif", "DATA HIVE \xE2\x80\xA2 CUTSCENE 1" },
	{ "l200m_super_sc2_sh2_fr161.aif", "DATA HIVE \xE2\x80\xA2 CUTSCENE 2" },
	{ "l200m_super_sc2alt_sh5_fr14.aif", "DATA HIVE \xE2\x80\xA2 CUTSCENE 3" },
	{ "sc100m_intsc_sh8_fr2.aif", "TAYARI PLAZA \xE2\x80\xA2 CUTSCENE 1" },
	{ "sc100m_out_sh2_fr45.aif", "TAYARI PLAZA \xE2\x80\xA2 CUTSCENE 2" },
	{ "sc110m_out_sc2_sh3_fr1.aif", "UPLIFT RESERVE \xE2\x80\xA2 CUTSCENE 1" },
	{ "sc120m_int_sc_title.aif", "KIZINGO BOULEVARD \xE2\x80\xA2 CUTSCENE 1" },
	{ "sc130m_out_sh1_fr2.aif", "ONI ALPHA SITE \xE2\x80\xA2 CUTSCENE 1" },
	{ "sc140m_int_sc_title.aif", "NMPD HQ \xE2\x80\xA2 CUTSCENE 1" },
	{ "sc140m_out_sh4_fr196.aif", "NMPD HQ \xE2\x80\xA2 CUTSCENE 2" },
	{ "sc150m_int_sc_title.aif", "KIKOWANI STATION \xE2\x80\xA2 CUTSCENE 1" },
	{ "sc150m_out_sh1_fr33.aif", "KIKOWANI STATION \xE2\x80\xA2 CUTSCENE 2" },
	{ "m20_sword_music_sc11_sh1_fr1.aif", "ONI: SWORD BASE \xE2\x80\xA2 CUTSCENE" },
	{ "m35_big_push_mus_sh1_fr1.aif", "TIP OF THE SPEAR \xE2\x80\xA2 BIG PUSH" },
	{ "m45_blastoff2.aif", "LONG NIGHT OF SOLACE \xE2\x80\xA2 BLAST OFF" },
	{ "m45_jorgsac.aif", "LONG NIGHT OF SOLACE \xE2\x80\xA2 JORGE SACRIFICE" },
	{ "m45_katsplan1_mus_sh5_fr5.aif", "LONG NIGHT OF SOLACE \xE2\x80\xA2 KAT'S PLAN 1" },
	{ "m45_katsplan1_music.aif", "LONG NIGHT OF SOLACE \xE2\x80\xA2 KAT'S PLAN 2" },
	{ "m45_pitstop_strings.aif", "LONG NIGHT OF SOLACE \xE2\x80\xA2 PIT STOP" },
	{ "m50_reunited_music_s2.aif", "EXODUS \xE2\x80\xA2 REUNITED" },
	{ "m50_wake_open.aif", "EXODUS \xE2\x80\xA2 WAKE" },
	{ "m52_airlift_music.aif", "NEW ALEXANDRIA \xE2\x80\xA2 AIRLIFT" },
	{ "m60_returnsword_music.aif", "THE PACKAGE \xE2\x80\xA2 RETURN TO SWORD" },
	{ "m70_carter_music.aif", "THE PILLAR OF AUTUMN \xE2\x80\xA2 CARTER" },
	{ "m70_carter_sac_music.aif", "THE PILLAR OF AUTUMN \xE2\x80\xA2 CARTER DEATH" },
	/* short names of the songs that are not a cutscene's */
	{ "misc/halo3_trench/track1", "HALO FINALE" },
	{ "misc/perilous_full/track1", "MAKE THEM PAY" },
	{ "misc/bia_brass/track1", "LAST OF THE BRAVE" },
	{ "misc/t&r_overture_nl/track1", "RECONCILED" },
	{ "misc/t&r_piano/track1", "THEME, PIANO" },
	{ "misc/t&r_lo_noperc/track1", "THEME, LOW STRINGS" },
	{ "end_credits", "ROLL CALL" },
	{ "atlas/misc/cp_driving_plus/track1", "DRIVING" },
	{ "atlas/misc/vari1a_groove/track1", "GROOVE" },
	{ "atlas/firefight/shiny/track1", "SHINY" },
	{ "exit/track1", "EXIT" },
	{ "heroic/10_kurz02_nobrass/track1", "HEROIC" },
	{ "poignant/cv/track1", "POIGNANT" },
	{ "poignant/teaser2a_full_nl/track1", "TEASER" },
	{ "tension/03_mystery_mt/track1", "MYSTERY" },
	{ "tension/bed2/track1", "TENSION BED" },
	{ "reach_firefight/firefight_in/track1", "FIREFIGHT" },
};

/* ---------- the sections of the song lists */

static char const *const mood_labels[AE_MUSIC_MOODS] =
	{ "MENU", "CUTSCENES", "HEROIC", "ACTION", "TENSION", "POIGNANT", "AMBIENT", "OTHER" };

char const *ae_menu_music_mood_label(int mood)
{
	return mood_labels[mood >= 0 && mood < AE_MUSIC_MOODS ? mood : AE_MUSIC_MOOD_OTHER];
}

/* whether the folder part (length bytes at part) is the word */
static int part_is(char const *part, size_t length, char const *word)
{
	return strlen(word) == length && !strncmp(part, word, length);
}

int ae_menu_music_mood(char const *id)
{
	char const *key = strchr(id, ':');
	char const *at;
	size_t length = 0;
	int ambient = 0;

	if (ae_menu_music_is_menu_song(id))
		return AE_MUSIC_MOOD_MENU;
	/* (a piece of a menu sound that is listed by its pieces) */
	{
		char const *last = strrchr(id, '/');
		char whole[AE_MUSIC_ID];

		if (last && (size_t)(last - id) < sizeof(whole))
		{
			memcpy(whole, id, (size_t)(last - id));
			whole[last - id] = 0;
			if (ae_menu_music_lists_pieces(whole))
				return AE_MUSIC_MOOD_MENU;
		}
	}
	key = key ? key + 1 : id;
	/* (a cutscene's music: a file, not a folder of tracks; or a pair of speakers' tracks) */
	{
		char const *slash = strchr(key, '/');
		size_t first = slash ? (size_t)(slash - key) : strlen(key);

		if (first >= 4 && !strncmp(key + first - 4, ".aif", 4))
			return AE_MUSIC_MOOD_CUTSCENES;
		if (slash && (!strcmp(slash + 1, "front_speakers") || !strcmp(slash + 1, "rear_speakers")))
			return AE_MUSIC_MOOD_CUTSCENES;
	}
	for (at = key; *at; at += length + (at[length] == '/' ? 1 : 0))
	{
		for (length = 0; at[length] && at[length] != '/'; length++)
			;
		if (part_is(at, length, "heroic"))
			return AE_MUSIC_MOOD_HEROIC;
		if (part_is(at, length, "tension"))
			return AE_MUSIC_MOOD_TENSION;
		if (part_is(at, length, "poignant"))
			return AE_MUSIC_MOOD_POIGNANT;
		if (part_is(at, length, "action") || part_is(at, length, "perc") || part_is(at, length, "pulse") ||
			part_is(at, length, "firefight") || part_is(at, length, "reach_firefight"))
			return AE_MUSIC_MOOD_ACTION;
		if (part_is(at, length, "amb") || part_is(at, length, "glue") || part_is(at, length, "sustain") ||
			part_is(at, length, "ambrhythm"))
			ambient = 1;
	}
	return ambient ? AE_MUSIC_MOOD_AMBIENT : AE_MUSIC_MOOD_OTHER;
}

/* a layer's number among its mood's songs of its game: kept by a hash of the id (the catalog's order), 0 for none */
#define NUMBERS_MAXIMUM 1100
static struct { uint64_t hash; unsigned short number; } numbers[NUMBERS_MAXIMUM];
static int number_count;

static uint64_t id_hash(char const *id)
{
	uint64_t hash = 1469598103934665603ull;

	for (; *id; id++)
		hash = (hash ^ (unsigned char)*id) * 1099511628211ull;
	return hash;
}

static int is_layer_mood(int mood)
{
	return mood >= AE_MUSIC_MOOD_HEROIC && mood <= AE_MUSIC_MOOD_AMBIENT;
}

void ae_menu_music_catalog_set(char const *const *ids, int count)
{
	int index, other;

	number_count = 0;
	for (index = 0; ids && index < count && number_count < NUMBERS_MAXIMUM; index++)
	{
		int mood = ae_menu_music_mood(ids[index]), number = 1;
		size_t game = strcspn(ids[index], ":");

		if (!is_layer_mood(mood))
			continue;
		for (other = 0; other < index; other++)
			if (ae_menu_music_mood(ids[other]) == mood && strcspn(ids[other], ":") == game &&
				!strncmp(ids[other], ids[index], game))
				number++;
		numbers[number_count].hash = id_hash(ids[index]);
		numbers[number_count++].number = (unsigned short)number;
	}
}

static int layer_number(char const *id)
{
	uint64_t hash = id_hash(id);
	int index;

	for (index = 0; index < number_count; index++)
		if (numbers[index].hash == hash)
			return numbers[index].number;
	return 0;
}

static void copy_cut(char *name, size_t size, char const *text)
{
	if (!size)
		return;
	snprintf(name, size, "%s", text);
}

void ae_menu_music_name(char const *id, char *name, size_t size)
{
	char tidy[AE_MUSIC_ID];
	char const *key = strchr(id, ':');
	size_t index, length, end;
	char const *part;

	if (!size)
		return;
	if (!strcmp(id, AE_MUSIC_TITLE_ID))
	{
		copy_cut(name, size, "HALO");
		return;
	}
	key = key ? key + 1 : id;
	/* (a layer: its mood and its number, once the catalog is known) */
	if (is_layer_mood(ae_menu_music_mood(id)) && layer_number(id) > 0)
	{
		char text[32];

		snprintf(text, sizeof(text), "%s %d", ae_menu_music_mood_label(ae_menu_music_mood(id)), layer_number(id));
		copy_cut(name, size, text);
		return;
	}
	for (index = 0; index < sizeof(names) / sizeof(names[0]); index++)
		if (!strcmp(key, names[index].key))
		{
			copy_cut(name, size, names[index].name);
			return;
		}
	/* (the tidied folder name: the last part that is not "trackN") */
	end = strlen(key);
	for (;;)
	{
		size_t start = end;

		while (start > 0 && key[start - 1] != '/')
			start--;
		part = key + start;
		length = end - start;
		if (length >= 5 && !strncmp(part, "track", 5) && start > 0)
		{
			end = start - 1;
			continue;
		}
		break;
	}
	if (length >= 4 && !strncmp(part + length - 4, ".aif", 4))
		length -= 4;
	if (length > 3 && !strncmp(part + length - 3, "_nl", 3))
		length -= 3;
	if (length >= sizeof(tidy))
		length = sizeof(tidy) - 1;
	for (index = 0; index < length; index++)
		tidy[index] = part[index] == '_' ? ' ' : upper_of(part[index]);
	tidy[length] = 0;
	copy_cut(name, size, tidy[0] ? tidy : "SONG");
}

static struct { char const *folder, *short_name, *full; } const games[] =
{
	{ "ce", "HALO CE", "HALO: COMBAT EVOLVED" },
	{ "halo3", "HALO 3", "HALO 3" },
	{ "halo3odst", "ODST", "HALO 3: ODST" },
	{ "haloreach", "REACH", "HALO: REACH" },
};

static int game_index(char const *id)
{
	char const *colon = strchr(id, ':');
	size_t length = colon ? (size_t)(colon - id) : strlen(id), index;

	for (index = 0; index < sizeof(games) / sizeof(games[0]); index++)
		if (strlen(games[index].folder) == length && !strncmp(games[index].folder, id, length))
			return (int)index;
	return -1;
}

static char const *game_unknown(char const *id)
{
	static char text[64];
	char const *colon = strchr(id, ':');
	size_t length = colon ? (size_t)(colon - id) : strlen(id), index;

	if (length >= sizeof(text))
		length = sizeof(text) - 1;
	for (index = 0; index < length; index++)
		text[index] = upper_of(id[index]);
	text[length] = 0;
	return text;
}

char const *ae_menu_music_game_short(char const *id)
{
	int game = game_index(id);

	return game >= 0 ? games[game].short_name : game_unknown(id);
}

char const *ae_menu_music_game_full(char const *id)
{
	int game = game_index(id);

	return game >= 0 ? games[game].full : game_unknown(id);
}

int ae_menu_music_game_order(char const *id)
{
	int game = game_index(id);

	return game >= 0 ? game : (int)(sizeof(games) / sizeof(games[0]));
}

/* ---------- play order */

static uint32_t random_next(uint32_t *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

void ae_menu_music_reshuffle(int *order, int count, uint32_t seed, int previous_last)
{
	uint32_t state = seed ? seed : 0x9E3779B9u;
	int index;

	for (index = count - 1; index > 0; index--)
	{
		int other = (int)(random_next(&state) % (uint32_t)(index + 1)), swap = order[index];

		order[index] = order[other];
		order[other] = swap;
	}
	/* (no song twice in a row across the seam) */
	if (count > 1 && order[0] == previous_last)
	{
		int swap = order[0];

		order[0] = order[1];
		order[1] = swap;
	}
}

static int find_entry(struct ae_menu_music_entry const *entries, int count, char const *id)
{
	int index;

	if (!id || !id[0])
		return -1;
	for (index = 0; index < count; index++)
		if (!strcmp(entries[index].id, id))
			return index;
	return -1;
}

static int menus_sequence(struct ae_menu_music_entry const *entries, int count, int *out, int maximum)
{
	int index, written = 0;

	for (index = 0; index < count && written < maximum; index++)
		if (entries[index].menu)
			out[written++] = index;
	return written;
}

int ae_menu_music_sequence(int mode, int shuffled, struct ae_menu_music_entry const *entries, int count,
	char const *song, struct ae_menu_music_playlist const *list, uint32_t seed, int previous_last, int *out, int maximum,
	int *effective, int *missing)
{
	int written = 0, lost = 0, used = mode;

	if (missing)
		*missing = 0;
	if (mode == AE_MUSIC_MODE_OFF)
	{
		if (effective)
			*effective = mode;
		return 0;
	}
	if (mode == AE_MUSIC_MODE_ONE_SONG)
	{
		int found = find_entry(entries, count, song);

		if (found >= 0 && maximum > 0)
		{
			out[0] = found;
			written = 1;
		}
	}
	else if (mode == AE_MUSIC_MODE_MY_PLAYLIST)
	{
		int index;

		for (index = 0; index < list->count; index++)
		{
			int found = find_entry(entries, count, list->ids[index]);

			if (found < 0)
				lost++;
			else if (written < maximum)
				out[written++] = found;
		}
		if (missing)
			*missing = lost;
	}
	if (!written)
	{
		used = AE_MUSIC_MODE_GAME_MENUS;
		written = menus_sequence(entries, count, out, maximum);
	}
	if (shuffled && used != AE_MUSIC_MODE_ONE_SONG)
		ae_menu_music_reshuffle(out, written, seed, previous_last);
	if (effective)
		*effective = used;
	return written;
}

int ae_menu_music_advance(int index, int count)
{
	return count > 0 ? (index + 1) % count : 0;
}

int ae_menu_music_mode_after_add(int mode, int count_before)
{
	return count_before == 0 ? AE_MUSIC_MODE_MY_PLAYLIST : mode;
}

int ae_menu_music_mode_after_leave(int mode, int before, int count)
{
	if (mode == AE_MUSIC_MODE_MY_PLAYLIST && count == 0)
		return before == AE_MUSIC_MODE_MY_PLAYLIST ? AE_MUSIC_MODE_GAME_MENUS : before;
	return mode;
}

char const *ae_menu_music_shuffle_reason(int mode, int no_mcc)
{
	if (no_mcc)
		return "NEEDS MCC";
	if (mode == AE_MUSIC_MODE_OFF)
		return "MUSIC IS OFF";
	if (mode == AE_MUSIC_MODE_ONE_SONG)
		return "ONE SONG ONLY";
	return NULL;
}

/* ---------- the screens' words */

void ae_menu_music_time_text(long seconds, char *text, size_t size)
{
	if (seconds < 0)
		seconds = 0;
	snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

void ae_menu_music_playlist_summary(int count, long seconds, char *text, size_t size)
{
	long minutes = (seconds + 30) / 60;

	if (count <= 0)
	{
		snprintf(text, size, "EMPTY");
		return;
	}
	if (minutes < 1)
		minutes = 1;
	snprintf(text, size, "%d %s " BULLET " %ld MIN", count, count == 1 ? "SONG" : "SONGS", minutes);
}

void ae_menu_music_summary(int mode, int shuffle, char const *song, int no_mcc, char *text, size_t size)
{
	char name[64];

	if (mode == AE_MUSIC_MODE_OFF)
	{
		snprintf(text, size, "OFF");
		return;
	}
	if (mode == AE_MUSIC_MODE_ONE_SONG && song && song[0] && (!no_mcc || !strcmp(song, AE_MUSIC_TITLE_ID)))
	{
		ae_menu_music_name(song, name, sizeof(name));
		snprintf(text, size, "%s " BULLET " %s", name, ae_menu_music_game_short(song));
		return;
	}
	if (no_mcc && (mode == AE_MUSIC_MODE_ONE_SONG || mode == AE_MUSIC_MODE_MY_PLAYLIST))
	{
		snprintf(text, size, "%s " BULLET " NEEDS MCC", ae_menu_music_mode_label(mode));
		return;
	}
	if (mode == AE_MUSIC_MODE_ONE_SONG)
	{
		snprintf(text, size, "ONE SONG");
		return;
	}
	snprintf(text, size, "%s%s", ae_menu_music_mode_label(mode), shuffle ? " " BULLET " SHUFFLE" : "");
}
