/*
AE_MCC_MUSIC.H

The menus' music (audio.menu_music, MENU MUSIC's modes: ae_menu_music_rules.h): each game's menu song, one song, the
player's own playlist, or none, from the songs in the player's own MCC installation's sound banks and Halo CE's title
theme (the game's own). Nothing of MCC's is shipped: the banks are read where the installation keeps them
(ae_mcc_root). Their rules: ae_mcc_music_rules.h.
*/

#ifndef __AE_MCC_MUSIC_H
#define __AE_MCC_MUSIC_H

/* the main menu's music starts (ui_start_main_menu_music, the game's thread): 1 when audio.menu_music takes it (the
songs, or none for OFF), so the title music is not started; 0 for the title music */
int ae_mcc_music_menu_start(void);
/* the main menu's music stops (ui_stop_main_menu_music): 1 when it was this, faded out; 0 when it was the title
music */
int ae_mcc_music_menu_stop(void);
/* each frame, the game's thread (DirectSoundDoWork): a changed audio.menu_music swaps the playing music; the music's
volume */
void ae_mcc_music_update(void);
/* the mixer's thread: the music added to output (frames of channels, 2 or 6: the front pair), at the master volume */
void ae_mcc_music_mix(float *output, unsigned long frames, unsigned long channels, float master_volume);

/* ---------- the songs, for the screens (ae_screen_music.c): the game's thread */

/* the catalog: entry 0 is Halo CE's title theme, entries 1 and up the songs of the installation's banks (each game's
menu song first among its songs), the games in name order. A catalog is read by a worker, once, the first time it is
asked for (or the menus' music wants a song) */
enum { AE_MCC_MUSIC_CATALOG_NOT_ASKED, AE_MCC_MUSIC_CATALOG_LOADING, AE_MCC_MUSIC_CATALOG_READY,
	AE_MCC_MUSIC_CATALOG_NONE };
#define AE_MCC_MUSIC_NEXT 3
void ae_mcc_music_catalog_request(void);
int ae_mcc_music_catalog_state(void);
/* none found: the installation looked at again (after ae_mcc_redetect); the state goes back to LOADING */
void ae_mcc_music_catalog_retry(void);
/* DEV MODE's source of songs (menu music <source>): 0 the installation, 1 stand-in songs, 2 none; the catalog read again */
void ae_mcc_music_dev_source(int which);
/* entries (1 with no catalog: the title theme); the song id, its length in seconds (0 for the title theme: it loops);
the entry of an id, -1 for none */
int ae_mcc_music_catalog_count(void);
char const *ae_mcc_music_catalog_id(int entry);
long ae_mcc_music_catalog_seconds(int entry);
int ae_mcc_music_catalog_find(char const *id);

/* what plays now: the menus' song (entry, -1 none: the game's own title music plays alone, or the music is off), its
place and length in seconds (length 0: a loop), the songs after it, and the preview (entry, -1 none) */
struct ae_mcc_music_now
{
	int entry;
	long position, length;
	int next[AE_MCC_MUSIC_NEXT], next_count;
	int preview;
	long preview_position;
};
void ae_mcc_music_now(struct ae_mcc_music_now *now);
/* X: the next song of the sequence */
void ae_mcc_music_next(void);
/* a song's preview from its start at MUSIC VOLUME over the menus' music, which waits; -1 stops it */
void ae_mcc_music_preview(int entry);

/* ---------- MCC's sound banks for other units (ae_announcer.c): the game's thread */

struct ae_mcc_bank;
/* <installation>/<folder>/fmod/pc/<file>, its streams read; NULL when there is none, or no installation */
struct ae_mcc_bank *ae_mcc_bank_open(char const *folder, char const *file);
void ae_mcc_bank_close(struct ae_mcc_bank *bank);
/* the first stream of that name, -1 for none */
int ae_mcc_bank_find(struct ae_mcc_bank const *bank, char const *name);
/* a stream decoded whole to 16-bit samples (ae_mcc_bank_free them: the C library's, which a unit using cseries' tracked
allocator must not pass to its free); 0 when it cannot be (not Vorbis this decodes, too long) */
int ae_mcc_bank_decode(struct ae_mcc_bank *bank, int stream, short **samples, unsigned long *frames,
	unsigned long *channels, unsigned long *rate);
void ae_mcc_bank_free(short *samples);
/* the installation's game folders with a spoken-lines bank (fmod/pc/english.fsb), in name order; how many */
int ae_mcc_bank_games(char (*names)[64], int maximum);

#endif
