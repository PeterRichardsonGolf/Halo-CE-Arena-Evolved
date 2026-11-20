/*
AE_GAMETYPE_BLOCK_CACHE.H

The gametype files' blocks kept in memory (owner 2026-11-01: "a slight delay/freeze when you select things on the ae
menu"): CHANGE GAMETYPE's list read each of 233 gametypes' file three times as it opened, and each read opened the
saved games' list file too to find it (saved_game_files.c get_nth_entry_in_mapfile), about 1,400 file opens: a second
and more on Windows. A block read is kept until a gametype file's content changes (playlist_profile.c's content
generation: a write, a name, a delete) or the saved games' list does (an entry set, added or removed, a listing made
again).
*/

#ifndef AE_GAMETYPE_BLOCK_CACHE_H
#define AE_GAMETYPE_BLOCK_CACHE_H

/* (saved_game_files.c) the saved games' list file is written: every kept block forgotten */
void ae_saved_games_listing_changed(void);
/* (playlist_profile.c) what was read last from a gametype file for this content generation, its whole block
(AE_GAMETYPE_CACHE_BLOCK) or its variant (AE_GAMETYPE_CACHE_VARIANT), size bytes (512 at most): TRUE and the data,
else FALSE */
enum { AE_GAMETYPE_CACHE_BLOCK, AE_GAMETYPE_CACHE_VARIANT, NUMBER_OF_AE_GAMETYPE_CACHES };
int ae_gametype_cache_get(int kind, long profile_index, long content_generation, void *data, unsigned long size);
/* (playlist_profile.c) what was just read */
void ae_gametype_cache_put(int kind, long profile_index, long content_generation, void const *data, unsigned long size);

#endif
