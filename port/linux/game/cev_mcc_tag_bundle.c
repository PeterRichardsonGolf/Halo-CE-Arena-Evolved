/*
CEV_MCC_TAG_BUNDLE.C

The Custom Edition copy's mcc_tag_bundle.c (cev/): an MCC map's tags laid out as a Custom Edition map's, to be
taken out as bundles.
*/

#include "cev/cev_names.h"

/* (mcc_cache.c: a map's file opened as the loader opens a level's) */
struct mcc_cache_source;
struct mcc_cache_identity;
void *mcc_cache_file_open(char const *name, struct mcc_cache_source *source, struct mcc_cache_identity *identity);
void mcc_cache_file_close(void *file);

#include "cev/mcc_tag_bundle.c"
