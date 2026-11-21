/*
AE_CE_UI_CHECK.H

Whether a Custom Edition map's pause menu widgets can be walked and patched
safely (ae_glue_ce_maps.c): every widget, block and tag index the patches
reach lies in the map's tag cache. A downloaded map is anyone's, and Custom
Edition's own checks (ce_map_checks.c) cover only what the port reads, not
the menu widgets, which the game reads through guarded accessors. Pure C over
a copy of the tag cache's bytes, unit tested in ae_ce_ui_check_test.c.
*/

#ifndef __AE_CE_UI_CHECK_H
#define __AE_CE_UI_CHECK_H

#include <stddef.h>
#include <stdint.h>

/* the tags as the game has them: the tag cache's bytes (the Xbox addresses
base to base + size) and the tag instance table (0x20 bytes each, little
endian: group, two parent groups, index, name, base address, two unused) */
struct ae_ce_ui_image
{
	const unsigned char *bytes;
	uint32_t base;
	uint32_t size;
	const unsigned char *instances;
	int32_t instance_count;
};

enum
{
	AE_CE_UI_NONE = -1,
	/* the most screens, children or event handlers of one block, the deepest
	walk (widgets are checked once, so a loop ends there) and the most widgets visited */
	AE_CE_UI_MAXIMUM_BLOCK_COUNT = 256,
	AE_CE_UI_MAXIMUM_DEPTH = 64,
	AE_CE_UI_MAXIMUM_VISITS = 8192,
};

/* whether the widget collection (a 'Soul' tag, or AE_CE_UI_NONE) and the
screens (widget definitions, each may be AE_CE_UI_NONE), with every widget
under them, are whole in the image: 1; else 0, with why in why */
int ae_ce_ui_check(const struct ae_ce_ui_image *image, int32_t collection_tag, const int32_t *screen_tags,
	int screen_count, char *why, size_t why_size);

#endif
