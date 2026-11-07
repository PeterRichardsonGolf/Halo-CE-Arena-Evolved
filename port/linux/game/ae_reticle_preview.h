/*
AE_RETICLE_PREVIEW.H

The live preview card of HUD > RETICLE and its CROSSHAIR pages (ae_reticle_preview.c): the crosshair at 1:1 on a plain swatch
(X cycles DARK, LIGHT, SKY, GRASS, SAND, SNOW, METAL), a 4x magnifier of its middle in the corner, the three colour states and
the three motion states under it.
*/

#ifndef __AE_RETICLE_PREVIEW_H
#define __AE_RETICLE_PREVIEW_H

#include "ae_crosshair_rules.h"

int ae_reticle_preview_backgrounds(void);
const char *ae_reticle_preview_background_name(int index);

/* draws the card in x, y, width by height (layout units); xh NULL: the weapon's own reticle (a line says so). caption: a line
under the swatch (NULL: the crosshair's summary). background: 0 to backgrounds - 1. Returns the height used */
float ae_reticle_preview_draw(float x, float y, float width, float height, const struct ae_crosshair *xh, int background,
	const char *caption);

/* (0.7.7) the CROSSHAIR screen's parts: a background's colour; the stage (the swatch, the crosshair 1:1 in its middle, the
magnifier at 4x in its top-left corner with "4X", "1:1" in its top-right; unit: the menu's u for the badges); the six state
tiles (NORMAL, ENEMY, ALLY, AT REST, MOVING, FIRING) in columns (6: one row; 3: two), each tile_height tall with its label
band under the swatch, gap apart. xh NULL: the stage's swatch alone, no tiles */
unsigned int ae_reticle_preview_background_rgba(int index);
void ae_reticle_preview_stage(float x, float y, float width, float height, const struct ae_crosshair *xh, int background,
	float unit);
void ae_reticle_preview_tiles(float x, float y, float width, float tile_height, float gap, int columns,
	const struct ae_crosshair *xh, int background, float unit);

#endif
