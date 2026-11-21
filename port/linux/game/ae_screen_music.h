/*
AE_SCREEN_MUSIC.H

(rc2) AE's MENU MUSIC screens (ae_screen_music.c), opened from SETTINGS > AUDIO > MENU MUSIC:
MENU MUSIC (MODE as chips: GAME MENUS / ONE SONG / MY PLAYLIST / OFF; SHUFFLE; SONG and MY PLAYLIST, each a screen of its
own; MUSIC VOLUME; NOW PLAYING and UP NEXT on the right; LOOK AGAIN when no MCC is found), ONE SONG (ALL SONGS, one check:
A chooses the song and goes back, Y previews), MY PLAYLIST (ALL SONGS on the left, PLAY ORDER on the right: A adds a song or
takes it off, in PLAY ORDER A picks a song up and puts it down, X takes it off). Every change is saved at once
(audio.menu_music, _shuffle, _song, _playlist).

Its log: "ae menu music screen: open", "mode <MODE>", "shuffle <0|1>", "volume <n>", "song <id>", "playlist add <id>",
"playlist remove <id>", "playlist move <id> to <n>", "opened ONE SONG", "opened MY PLAYLIST (<n> songs)", "preview <id>",
"preview stopped", "defaults restored", "look again: <found|not found>", "closed". The songs' own lines (the mode and its
sequence, the song now playing, the preview): ae_mcc_music.c, "ae menu music: ...".
*/

#ifndef __AE_SCREEN_MUSIC_H
#define __AE_SCREEN_MUSIC_H

/* opens MENU MUSIC over the stack; 0 when the stack is full */
int ae_screen_music_open(void);
/* the screens' staging (DEV MODE's "music" command, the menus' test hook): which screen first, 0 MENU MUSIC, 1 ONE SONG,
2 MY PLAYLIST; 0 when it could not open */
int ae_screen_music_open_at(int which);
/* the screens closed, wherever they are on the stack (a staged scene starts again) */
void ae_screen_music_close(void);
/* SETTINGS' row's summary: "GAME MENUS • SHUFFLE", "TRENCH • HALO 3", "MY PLAYLIST • SHUFFLE", "OFF" */
void ae_screen_music_summary(char *text, int size);

#endif
