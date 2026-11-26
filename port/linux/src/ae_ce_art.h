/*
AE_SKULL_ART.H

Content Evolved's skull and medal pictures (port/assets/skulls, port/assets/ce_medals, tools/ae_ce_art.py): the table the generated source holds
(tools/embed_assets.py's AE hook). Each picture is raw RGBA, rows top first, 4 bytes a pixel in memory order; its name is the
skull's key (halo_skulls.h's skull_descriptions), or "medal_" and a campaign medal's key. The skulls screen and the end card draw
them where MCC's install has no picture.
*/

#ifndef __AE_CE_ART_H
#define __AE_CE_ART_H

struct ae_ce_art
{
	const char *name;
	const unsigned int *rgba;
	unsigned int width, height;
};

extern const struct ae_ce_art ae_ce_art[];
extern const unsigned int ae_ce_art_count;

#endif
