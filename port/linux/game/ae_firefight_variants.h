/*
AE_FIREFIGHT_VARIANTS.H

Firefight's variants as the FIREFIGHT lobby lists them: the eight built into the game (Content Evolved's) and the ones the player
saved (up to eight, one text file in the save root's private folder: ae_firefight_variants_rules.h). A Firefight variant is never
a multiplayer gametype file, so no multiplayer list holds one. Menus' thread only. Include after cseries.h.
*/

#ifndef __AE_FIREFIGHT_VARIANTS_H
#define __AE_FIREFIGHT_VARIANTS_H

#include "ae_firefight_variants_rules.h"

enum { AE_FFV_BUILTINS = 8, AE_FFV_LIST = AE_FFV_BUILTINS + AE_FFV_SAVED };

/* the built-in variants: the name firefight_build_variant_by_name knows, and the name the lists show */
const char *ae_ffv_builtin_key(int index);
const char *ae_ffv_builtin_name(int index);

/* the player's saved variants, read from the file: how many (up to maximum); written whole: 1 when it is written */
int ae_ffv_saved_load(struct ae_ffv_record *records, int maximum);
int ae_ffv_saved_store(struct ae_ffv_record const *records, int count);
/* a name a variant has already (a built-in's or a saved one's), letters' case aside */
int ae_ffv_name_taken(const char *name);
/* the lists' entry n: built-ins first, then the saved; its name written (11 characters or the built-in's), builtin 1 for a
built-in. Returns the entries there are */
int ae_ffv_entries(char (*names)[24], int *builtin, int maximum);

#endif
