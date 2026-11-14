/*
AE_SPLIT_GRAPHICS.H

SPLIT GRAPHICS for the renderer (the rules: ae_split_graphics_rules.h). While two to four local players are in the
game, the renderer's reads of display.anti_aliasing, display.anisotropic, display.per_pixel_lighting and
display.model_detail go through here and get display.split_graphics' preset; otherwise (and with SAME) config.toml's
values. Nothing is written. The readers that keep a value until a setting changes compare ae_split_graphics_changes
in place of config_changes, so the swap applies from the frame the second view opens or the last extra view closes,
as a change in Settings applies (d3d8_gl.c: anti-aliasing between frames, multisampling's targets made again).
Plain C types only: the platform layer (port/linux/src) includes it.
*/

#ifndef AE_SPLIT_GRAPHICS_H
#define AE_SPLIT_GRAPHICS_H

/* config_changes(), with its top bit flipped while a preset is in play: changes when a setting does, or when split
screen starts or ends */
unsigned long ae_split_graphics_changes(void);

/* the value the renderer plays with: the preset's for the four settings in split screen, else config_string's */
const char *ae_split_graphics_string(const char *name);

/* the same for a true/false setting (config_boolean's) */
int ae_split_graphics_boolean(const char *name);

/* display.sharp_reflections as the renderer plays it (PERFORMANCE: false), read again when ae_split_graphics_changes
does: the secondary target (mirrors, active camouflage) at the 3D view's pixels, or at the Xbox's half (d3d8_gl.c) */
int ae_split_graphics_sharp_reflections(void);

#endif
