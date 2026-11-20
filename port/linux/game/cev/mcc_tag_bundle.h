/*
MCC_TAG_BUNDLE.H

(port) An MCC map's tags opened for the Tag Injector to take out as tag
bundles (tag_bundle.h, tag_injector.c), laid out as a Custom Edition map's
are once loaded, converted and checked (custom_edition_cache_bundle_open),
so that every bundle is one any level plays as it plays a Custom Edition
map's: its models gbxmodels, its bitmaps' pixels Halo PC's, its sounds
Xbox ADPCM. mcc_tag_bundle.c tells how.
*/

#ifndef __MCC_TAG_BUNDLE_H
#define __MCC_TAG_BUNDLE_H

struct tag_bundle_source;

/* Opens the MCC map of `level_name` (as mcc_maps.c names it: "mcc_maps\...")
for its tags to be taken out, while no MCC map is played: its tags at the
base they were written for, their metadata made this build's
(mcc_tags_prepare, as a level's), its bitmaps' pixels and, of the tags
`wanted` names (a flag for each of the first `wanted_count` tags, by
absolute index; NULL: none), its sounds' samples, made Halo PC's and Xbox
ADPCM in the bundle's own offsets, and checked by the MCC validator
(mcc_tags_validate_bundle). Its tag header, laid out as a Custom Edition
map's (tag_injector.c's struct injector_map_header), or NULL after logging
why. Opening it again, or another, lets the one before go. */
void *mcc_tag_bundle_open(
	char const *level_name,
	unsigned char const *wanted,
	long wanted_count);
/* The map mcc_tag_bundle_open opened, as tag_bundle_write reads one (all of
it zero when none is). */
void mcc_tag_bundle_source(
	struct tag_bundle_source *source);
/* Whether a bundle of these tags (absolute indices, as tag_bundle_tags
lists them) would hold MCC's own content, which is never sent to another
machine: TRUE when the map is not a Steam Workshop item's, or any of them
reads its pixels or samples from MCC's shared resource maps */
int mcc_tag_bundle_own_content(
	long const *tags,
	long count);
void mcc_tag_bundle_close(
	void);
int mcc_tag_bundle_opened(
	void);

#endif
