/*
AE_MCC_MUSIC.H

The menus' music from the player's own MCC installation (audio.menu_music): the songs in its sound banks, shuffled or
in order, in place of the title music, or no music at all. Nothing of MCC's is shipped: the banks are read where the
installation keeps them (ae_mcc_root). Their rules: ae_mcc_music_rules.h.
*/

#ifndef __AE_MCC_MUSIC_H
#define __AE_MCC_MUSIC_H

/* the main menu's music starts (ui_start_main_menu_music, the game's thread): 1 when audio.menu_music takes it (MCC
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
