/* MCC installation paths, independent of Xbox and Custom Edition loaders. */
#ifndef MCC_PACKAGE_H
#define MCC_PACKAGE_H
#include <stdint.h>
#include <stdio.h>
/* Canonical identities fit the engine's existing 64-byte map-name field. */
int mcc_package_name(char const *name, char canonical[64]);
/* While set, the package `name` reads its own map and resource maps from the folder
 * `folder` (forward slashes, relative to the data root: mcc_staging/<package>) so a downloaded
 * package can be fingerprinted before it is installed. NULL clears it. */
void mcc_package_stage_redirect(char const *name, char const *folder);
int mcc_package_map_path(char const *name, char path[128]);
int mcc_package_resource_path(char const *name, unsigned type, char path[128]);
/* Package first, MCC root only on ENOENT; path records the selected/failed file. */
FILE *mcc_package_resource_open(char const *name, unsigned type, char path[128]);
/* Base candidates in loader order: legacy root, singleplayer, multiplayer. */
#define MCC_PACKAGE_BASE_SETS 3
int mcc_package_base_resource_path(unsigned type,unsigned set,char path[128]);
/* Retain a present package override, otherwise use one fixed local base set. */
int mcc_package_resource_candidate(char const *name,unsigned type,unsigned set,char path[128]);
int mcc_package_shared_resource(char const *path, unsigned type);
/* The package Forge saves maps made on MCC maps in (mcc_maps\forge\<name>,
 * port/forge's forge_export_mcc_scenario_map): each is its base map's file
 * with Forge's trailer, which names the base level ("Base Map: mcc_maps\...").
 * It holds none of the base's resources: those are read as the base map's
 * are (its package's own, else MCC's _shared sets), never copied. */
#define MCC_PACKAGE_FORGE "forge"
/* The base level of a map in the Forge package (canonical, 64 characters);
 * 0 for any other map, or one whose trailer names none. */
int mcc_package_forge_base(char const *name, char base[64]);
/* Forget what mcc_package_forge_base read: a Forge map was saved. */
void mcc_package_forge_forget(void);
#endif
