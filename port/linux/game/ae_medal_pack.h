/*
AE_MEDAL_PACK.H

The medals' pictures and names (ae_medals.h), from the player's own MCC install: its medal picture pack (Data/UI/texturepacks)
and its career list (Data/careerdb), found as game.mcc_path finds the install. They are read the first time one is asked for, a
picture is made a texture the first time it is drawn and kept. Without the install, or when it has no picture for a medal, there is
none: the medal is shown by its name, except where Arena Evolved has a picture of its own (port/assets/medals): that is drawn for a medal
MCC's files have none of, only while MCC's files are read. Nothing of MCC's files ships with the game, is copied or leaves the machine.
*/

#ifndef __AE_MEDAL_PACK_H
#define __AE_MEDAL_PACK_H

#include <wchar.h>

#include "cseries.h"

struct bitmap_data;

/* whether a medal has a picture in the install's files (the files read, the picture's place in them checked; no texture made) */
boolean ae_medal_pack_has_picture(int medal);
/* a medal's picture as a bitmap of the game's, with the part of it the picture fills (0..1, the rest is clear); NULL for none, and
for a game with no renderer. Made when first asked for. Call while the HUD draws */
struct bitmap_data const *ae_medal_pack_picture(int medal, real_rectangle2d *clip);
/* the picture as AE's menus draw it (ae_draw_image: the image's id, 0 for none): the post-game report's. Made once and kept; at most
64 images are live in the drawing, and one it freed for a newer is made again when next asked for */
int ae_medal_pack_menu_image(int medal);
/* a medal's name from its key (Open Season; upper: OPEN SEASON); empty for a medal with no key */
/* the colour of its name under its picture (the alpha left as it is) */
void ae_medal_pack_caption(int medal, real_argb_color *color);
/* every picture read and made now, in a map's load, so that the first medal does not stall the game. Nothing on a server */
void ae_medal_pack_preload(void);
void ae_medal_pack_name(int medal, wchar_t *name, int size, boolean upper);
/* (0.8.1) a medal's line of what earns it, from the career list as its pictures are (ae_medal_career_desc), read once and
kept: 1 with the text in out, 0 for none (no install, no line, or one this does not read). Menus' thread */
enum { AE_MEDAL_DESC_SIZE = 160 };
int ae_medal_pack_desc(int medal, char *out, int size);

#endif /* __AE_MEDAL_PACK_H */
