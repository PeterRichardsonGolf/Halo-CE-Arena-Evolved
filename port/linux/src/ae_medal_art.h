/*
AE_MEDAL_ART.H

Arena Evolved's own medal pictures (port/assets/medals, tools/ae_medal_art.py): the table the generated source holds
(tools/embed_assets.py's AE hook). Each picture is raw RGBA, rows top first, 4 bytes a pixel in memory order; its name is the
medal's key (port/linux/game/ae_medal_ids.h) without "medal_", with three_sixty for "medal_360". The game draws them only where
MCC's install is read and has no picture for the medal (port/linux/game/ae_medal_pack.c).
*/

#ifndef __AE_MEDAL_ART_H
#define __AE_MEDAL_ART_H

struct ae_medal_art
{
	const char *name;
	const unsigned int *rgba;
	unsigned int width, height;
};

extern const struct ae_medal_art ae_medal_art[];
extern const unsigned int ae_medal_art_count;

#endif
