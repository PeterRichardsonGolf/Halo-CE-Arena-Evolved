/*
AE_TEXTURE_FILTER.H

ANISOTROPIC FILTERING (display.anisotropic, Video Setup > GRAPHICS): the game
never asks the Xbox for anisotropic filtering, so its floors and walls blur
at a grazing angle as on the Xbox; a level of 2x to 16x sharpens them. d3d8_gl.c's
sampler choice calls this (an AE hook line), after the game's own state.
Include after the XDK's types (DWORD).
*/

#ifndef AE_TEXTURE_FILTER_H
#define AE_TEXTURE_FILTER_H

/* a sampler state's minification filter and anisotropy, given the game's (and its mip filter; hires: a high-res HUD
texture, left alone): with a level set, a linear filtered, mipmapped texture's become anisotropic at that level (the
GL clamps it to what the card has); else unchanged. The level is part of the sampler's state, so a change made in the
menus takes effect at once */
void ae_texture_anisotropy(DWORD *min_filter, DWORD *anisotropy, DWORD mip_filter, int hires);
/* the level of display.anisotropic: 1 (off), 2, 4, 8 or 16 (pure: the unit tests' too) */
int ae_texture_anisotropy_level(const char *setting);

#endif
