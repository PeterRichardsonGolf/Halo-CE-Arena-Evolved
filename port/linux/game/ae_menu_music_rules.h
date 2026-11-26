/*
AE_MENU_MUSIC_RULES.H

The rules of MENU MUSIC (SETTINGS > AUDIO > MENU MUSIC, ae_screen_music.c; the playing: ae_mcc_music.c), with no game,
file or thread: the four modes and the old audio.menu_music values they replace, the playlist's text (song ids joined
with ";", audio.menu_music_playlist), a song's id and its name, the songs that count as songs, what plays in what
order, which mode a choice switches to, and the texts the screens show. Unit tested:
port/linux/tests/ae_menu_music_rules_test.c.

A song's id is its game's folder in the MCC installation, a colon, and the song's path under sound\music\ with forward
slashes, lower case ("halo3:misc/halo3_trench/track1"). Halo CE's own title theme is "ce:title". An id stays the same
across MCC's language banks. A song has a name: its soundtrack title where that is known for certain, a cutscene's music by the mission it plays in
and its cue, a short plain name from AE's table, else its tidied folder name.
*/

#ifndef __AE_MENU_MUSIC_RULES_H
#define __AE_MENU_MUSIC_RULES_H

#include <stddef.h>
#include <stdint.h>

#define AE_MUSIC_ID 112                    /* a song id's bytes, with its 0 */
#define AE_MUSIC_PLAYLIST_MAXIMUM 256
#define AE_MUSIC_TITLE_ID "ce:title"

enum
{
	AE_MUSIC_MODE_GAME_MENUS,
	AE_MUSIC_MODE_ONE_SONG,
	AE_MUSIC_MODE_MY_PLAYLIST,
	AE_MUSIC_MODE_OFF,
	AE_MUSIC_MODES
};

/* the config value of a mode ("game_menus", "one_song", "my_playlist", "off"; game_menus for a number out of range) and
its chip's label */
char const *ae_menu_music_mode_value(int mode);
char const *ae_menu_music_mode_label(int mode);
/* the mode a config value names; -1 for none (an old value, or nothing) */
int ae_menu_music_mode_parse(char const *text);
/* the mode left or right of this one, round the four (direction -1 or 1) */
int ae_menu_music_mode_step(int mode, int direction);

/* an old audio.menu_music value ("classic", "mcc_shuffle", "mcc_order") and what it becomes: classic is ONE SONG (the
title theme); the MCC ones are MY PLAYLIST holding every song, shuffled or in order. 1 when text is one of them */
struct ae_menu_music_legacy { int mode, shuffle, all_songs; };
int ae_menu_music_legacy(char const *text, struct ae_menu_music_legacy *legacy);

/* ---------- the playlist */

struct ae_menu_music_playlist
{
	int count;
	char ids[AE_MUSIC_PLAYLIST_MAXIMUM][AE_MUSIC_ID];
};
/* an id a config text can hold: a game, a colon, a path; no control characters or ";", shorter than AE_MUSIC_ID */
int ae_menu_music_id_valid(char const *id);
/* the playlist's text read: ids between ";" (spaces round them dropped), the invalid and the repeated left out, at most
AE_MUSIC_PLAYLIST_MAXIMUM */
void ae_menu_music_playlist_parse(char const *text, struct ae_menu_music_playlist *list);
/* the text for the config; 0 (nothing usable written) when it does not fit */
int ae_menu_music_playlist_serialize(struct ae_menu_music_playlist const *list, char *text, size_t size);
int ae_menu_music_playlist_find(struct ae_menu_music_playlist const *list, char const *id);
/* the id added at the end: its place; -1 when it is there already, invalid or the list is full */
int ae_menu_music_playlist_add(struct ae_menu_music_playlist *list, char const *id);
int ae_menu_music_playlist_remove(struct ae_menu_music_playlist *list, int index);
/* the song one place up (-1) or down (1): its new place (the same at either end) */
int ae_menu_music_playlist_move(struct ae_menu_music_playlist *list, int index, int direction);

/* ---------- songs: ids and names */

/* a bank stream's source path (a bank's .info record, "sound\\music\\misc\\halo3_trench\\track1\\...") as the song's key:
lower case, forward slashes, the path under music\ up to its in / loop / out folders; a cinematic's music file keeps
its file name. 0 when the path is not music */
int ae_menu_music_song_key(char const *path, size_t size, char *key, size_t key_size);
/* whether a key is a song (not a level's ambience, a sustain, a stem or a sting: amb, sustain, glue, microglue, pad,
pulse, perc, ambrhythm, stingers, invasion_temp_cues, alt_trans*, rear_speakers, elevator_music_split*) */
int ae_menu_music_key_is_song(char const *key);
/* "<folder>:<key>" lower case; 0 when empty or it does not fit */
int ae_menu_music_make_id(char const *folder, char const *key, char *id, size_t size);
/* a piece of a sound (a permutation: its file's name, lower case, ".aif" dropped) from its source path; 0 for none */
int ae_menu_music_piece_stem(char const *path, size_t size, char *stem, size_t stem_size);
/* whether a menu sound is also listed by its pieces, each its own song with its own title (Halo 3's main menu moves
between six; the other games' have no titled pieces) */
int ae_menu_music_lists_pieces(char const *id);
/* a piece's id: its sound's id, a slash and the piece's stem; 0 when empty or it does not fit */
int ae_menu_music_piece_id(char const *song_id, char const *stem, char *id, size_t size);
/* each game's own menu song (the MENU badge, and what GAME MENUS plays): ce:title, Halo 3's main_menu_delta/delta_menu,
ODST's atlas/menu/track1, Reach's reach_menu/track1 */
int ae_menu_music_is_menu_song(char const *id);
/* the song's name, in capitals: the table's, else the folder name tidied (the last folder that is not "trackN", ".aif"
and "_nl" dropped, "_" as a space); cut to fit size */
void ae_menu_music_name(char const *id, char *name, size_t size);
/* the sections of the song lists (a game's songs, in this order): the menu songs, the cutscenes' music, the layers by mood
(from their folders), and the rest */
enum
{
	AE_MUSIC_MOOD_MENU, AE_MUSIC_MOOD_CUTSCENES, AE_MUSIC_MOOD_HEROIC, AE_MUSIC_MOOD_ACTION, AE_MUSIC_MOOD_TENSION,
	AE_MUSIC_MOOD_POIGNANT, AE_MUSIC_MOOD_AMBIENT, AE_MUSIC_MOOD_OTHER, AE_MUSIC_MOODS
};
int ae_menu_music_mood(char const *id);
/* a section's label in capitals (OTHER for a number out of range) */
char const *ae_menu_music_mood_label(int mood);
/* the catalog's ids in order (count of them): a layer (a song of the HEROIC to AMBIENT sections) is then named for its
mood and its number among that mood's songs of its game, "TENSION 4". NULL, 0: none known (the folder's names again) */
void ae_menu_music_catalog_set(char const *const *ids, int count);

/* the song's game: short (HALO CE, HALO 3, ODST, REACH) and in full (HALO: COMBAT EVOLVED, HALO 3, HALO 3: ODST,
HALO: REACH); a game this does not know shows its folder name in capitals (kept in a static buffer) */
char const *ae_menu_music_game_short(char const *id);
char const *ae_menu_music_game_full(char const *id);
/* the games' order in lists: Halo CE first, then MCC's order; a game not known last */
int ae_menu_music_game_order(char const *id);

/* ---------- play order */

/* a song of the catalog (ae_mcc_music.c's, the title theme first) */
struct ae_menu_music_entry
{
	char id[AE_MUSIC_ID];
	int menu;                              /* its game's own menu song */
};
/* what plays: indices into entries, at most maximum; returns how many. GAME MENUS: every menu song, in order or
shuffled; ONE SONG: song; MY PLAYLIST: the list's songs that are there (the others counted in *missing), in order or
shuffled; OFF: none. A shuffle is made by seed and never starts with previous_last (an index, -1 none) when another
song can. ONE SONG with its song gone, MY PLAYLIST with none of its songs there or an empty list: GAME MENUS; *effective
says which mode it is (NULL: not wanted) */
int ae_menu_music_sequence(int mode, int shuffle, struct ae_menu_music_entry const *entries, int count,
	char const *song, struct ae_menu_music_playlist const *list, uint32_t seed, int previous_last, int *out, int maximum,
	int *effective, int *missing);
/* order[0..count-1] shuffled by seed, never starting with previous_last when another song can (the player's own, at the end
of a pass) */
void ae_menu_music_reshuffle(int *order, int count, uint32_t seed, int previous_last);
/* the next place in a sequence of count, round */
int ae_menu_music_advance(int index, int count);
/* the mode after a song is added to a list of count_before songs: the first song of an empty list makes it MY PLAYLIST;
no later one switches anything */
int ae_menu_music_mode_after_add(int mode, int count_before);
/* the mode after the playlist screen is left: MY PLAYLIST left empty goes back to before (GAME MENUS when that was MY
PLAYLIST too) */
int ae_menu_music_mode_after_leave(int mode, int before, int count);
/* why SHUFFLE cannot be changed (shown in its value's place), NULL when it can: "NEEDS MCC", "MUSIC IS OFF", "ONE SONG
ONLY" */
char const *ae_menu_music_shuffle_reason(int mode, int no_mcc);

/* ---------- the screens' words */

/* "6:10", "12:23" */
void ae_menu_music_time_text(long seconds, char *text, size_t size);
/* "14 SONGS • 55 MIN", "1 SONG • 6 MIN", "EMPTY" */
void ae_menu_music_playlist_summary(int count, long seconds, char *text, size_t size);
/* SETTINGS' row: the mode, " • SHUFFLE" when it shuffles (GAME MENUS, MY PLAYLIST); ONE SONG: its song ("TRENCH •
HALO 3"); with no MCC, a saved ONE SONG or MY PLAYLIST says " • NEEDS MCC" */
void ae_menu_music_summary(int mode, int shuffle, char const *song, int no_mcc, char *text, size_t size);

#endif
