/*
AE_SAVED_LIMITS.H

AE's limits on saved games (seed revision 11, 0.2.6). Halo's memory unit lists at most 100 saved games of any type
(z:\saved\hdmu.map, saved_game_files.c): the player's profiles and gametypes from u:\UDATA, then the built-in ones
appended last (the 26 stock gametypes, default_playlist, and the 2 default profiles). Past the limit the entries
enumerated last are left out (the stock gametypes first), no new saved game can be made, and the start-up check
reports "too many saved games" at one folder less than the limit.

AE seeds 86 gametypes (arena_gametypes.c), so a seeded root is 86 + 28 built-ins + the player's profiles: 115 with
one profile. The limit is 160: 45 more entries for profiles and the player's own gametypes. The gametype lists
(menu_functions.c MAXIMUM_GAMETYPES, the Xbox menus' multiplayer settings list, network_test.c, arena_gametypes.c)
hold as many, so no list can be shorter than the gametypes the memory unit lists.

Every place the limit applies (tools/test_ae_saved_limits.py checks each):
- saved_game_files.c MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT (mapfile entries, the new-file
  check, the start-up folder count, the asserts on a file's index; the index has 12 bits, up to 4095)
- ui_widget_event_handler_functions.c's multiplayer settings list (its list items and the count it asks for)
- menu_functions.c MAXIMUM_GAMETYPES, network_test.c's saved gametype search
- arena_gametypes.c ARENA_GAMETYPES_MAXIMUM_SAVED and its warning

An older build (before 0.2.6, limit 100) on a root past 100 entries: it lists the root's own saved games first, so
the profiles and gametypes stay, but the stock gametypes and the default profiles past entry 100 are not listed,
and it makes no new saved game (no new profile, no gametype saved in the editor) while the root has 100 or more.
At 99 folders or more it shows "too many saved games" at start.
*/

#ifndef __AE_SAVED_LIMITS_H
#define __AE_SAVED_LIMITS_H

/* the saved games of any type a memory unit lists (upstream: 100) */
#define AE_SAVED_GAMES_MAXIMUM 160
/* the limit before AE raised it (what older builds have) */
#define AE_SAVED_GAMES_OLD_MAXIMUM 100

#endif
