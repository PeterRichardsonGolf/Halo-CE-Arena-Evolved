/*
AE_SAVED_GAME_FILE.H

Which file of the player's profile folder holds a level's saved game (the persistent storage of
game_state_xbox.c): the stock campaign's and every level not of an MCC package keep savegame.bin; a package
campaign's mission (mcc_maps\<package>\<map>) keeps its own savegame-<package>.bin (ae_campaign_save_rules.h), so
no campaign's saved game is ever written over another's. The level is the one named by
ae_saved_game_file_level, else the scenario loaded (the one being saved, or resumed by main_new_map).
*/

#ifndef __AE_SAVED_GAME_FILE_H
#define __AE_SAVED_GAME_FILE_H

/* the level whose file the next reads and writes use (a menu asking about a campaign's saved game with no map of
it loaded); NULL: the scenario loaded again */
void ae_saved_game_file_level(const char *level);

/* the file's name in the profile folder for the level asked (ae_saved_game_file_level) or loaded, or NULL: a level
of mcc_maps whose package cannot be named, which keeps none */
const char *ae_saved_game_file(void);

/* the level's file may be read: savegame.bin always (as the game did), a package campaign's own only once it is in
player 1's profile folder at a saved game's size (a read never makes one, nor fills a short one with zeros; a short one
is a log line), none never */
int ae_saved_game_file_readable(void);

/* the level's saved game file is a package campaign's own: 1, else 0 */
int ae_saved_game_file_custom(const char *level);

#endif
