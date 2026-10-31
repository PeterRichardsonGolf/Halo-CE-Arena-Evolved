/*
AE_MCC_BOOT.H

(Arena Evolved) booting with no Xbox data: the main menu's map, "ui", read from MCC's halo1/maps/ui.map through
GulchCE's MCC loader when the data root has no maps\ui.map (ae_mcc_boot_source, ae_mcc.h). The level keeps its
name, "ui", which the menus' code checks for; the loader takes it as an MCC level (mcc_cache.c, mcc_package.c:
"(Arena Evolved)" lines) and opens "mcc:\halo1\ui.map" (ae_mcc_translate_path). MCC's stock multiplayer maps are
read in place the same way, by their identity <name>@mcc.
*/

#ifndef __AE_MCC_BOOT_H
#define __AE_MCC_BOOT_H

/* whether the main menu comes from MCC's ui.map (decided once, the first time asked, and logged) */
int ae_mcc_boot_from_mcc(void);
/* whether a level name ("ui", or its scenario's path, "levels\ui\ui") is the main menu's, read from MCC's ui.map */
int ae_mcc_boot_ui(char const *name);
/* whether a level is one of MCC's own maps read in place: the main menu from MCC's ui.map (above), a stock
multiplayer map from its halo1/maps, <name>@mcc, or a Workshop item's map the start's catalog found,
<name>@ws<item id> (ae_map_kind.h; mcc:\\ws\\<item id>\\<name>.map), with MCC found and game.mcc_use not "no".
GulchCE's loader takes it as an MCC level with no package (mcc_cache.c, mcc_package.c) */
int ae_mcc_install_level(char const *name);
/* such a level's identity: "ui", <name>@mcc or <name>@ws<item id> (from its scenario's path too: levels\test\<identity>\<identity>) */
char const *ae_mcc_install_level_identity(char const *name);
/* cache_files.c's missing map message (an AE hook): for MCC's maps, <name>@mcc and <name>@ws<item id>, what the game
needs instead of "copy <file>.map into maps": MCC's own files (Halo: MCC in Steam, Settings > MAP FILES), the
Workshop item subscribed to */
void ae_mcc_stock_missing_message(char const *file, char *message, long size);
/* GulchCE's loader's reason a level's file could not be opened (missing: 1) or read, for MCC's own maps read in
place (ui, <name>@mcc, <name>@ws<item id>): what to do in Steam instead of "install the level in its mcc_maps
package folder"; reason untouched for any other level */
void ae_mcc_install_level_problem(char const *name, int missing, char *reason, unsigned long size);
/* such a level's file as the engine opens it, "mcc:\halo1\<name>.map" (ae_mcc_translate_path) (1) */
int ae_mcc_install_level_path(char const *name, char *path, unsigned long size);

#endif
