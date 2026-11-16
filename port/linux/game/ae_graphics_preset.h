/*
AE_GRAPHICS_PRESET.H

(0.9.0) VIDEO's GRAPHICS PRESET and SPLIT SCREEN's rules, pure (port/linux/tests/ae_graphics_preset_test.c). A preset
sets eight of VIDEO's settings at once: ANTI-ALIASING, ANISOTROPIC FILTERING, PER-PIXEL LIGHTING, MODEL DETAIL, SHADOW
RESOLUTION, PARTICLE LIMIT, DYNAMIC LIGHT LIMIT and HDR (not DRAW DISTANCE: it changes what a player sees). QUALITY is
the PC builds' defaults (port_config.c's), PERFORMANCE the Xbox's look (what Android and the browser default to) with
the lowest limits that still look normal. Nothing stores the preset: the eight settings say which it is, and any other
mix is CUSTOM. The menus (ae_screen_settings.c) show and set it; the engine takes the first four's values for split
screen (display.split_graphics; ae_split_graphics_rules.h).

display.big_sprite_limit is not a preset's: it is 8 both ways (port_config.c's default) and an Android setting.
*/

#ifndef AE_GRAPHICS_PRESET_H
#define AE_GRAPHICS_PRESET_H

#include <stddef.h>

enum { AE_GRAPHICS_QUALITY, AE_GRAPHICS_PERFORMANCE, AE_GRAPHICS_CUSTOM };
enum { AE_GRAPHICS_PRESET_ROWS = 8 };

/* the eight settings a preset sets (config.toml's keys) and their rows' labels, in VIDEO's order */
extern const char *const ae_graphics_preset_keys[AE_GRAPHICS_PRESET_ROWS];
extern const char *const ae_graphics_preset_labels[AE_GRAPHICS_PRESET_ROWS];

/* a preset's values, as config.toml's texts */
struct ae_graphics_preset_values
{
	const char *anti_aliasing;       /* display.anti_aliasing: "smaa" / "off" */
	const char *anisotropic;         /* display.anisotropic: "16x" / "off" */
	const char *per_pixel_lighting;  /* display.per_pixel_lighting: "true" / "false" */
	const char *model_detail;        /* display.model_detail: "native" / "stock" */
	const char *shadow_resolution;   /* display.shadow_resolution: "128" / "128" (a PC's default; both) */
	const char *particle_limit;      /* display.particle_limit: "4096" / "1024" */
	const char *dynamic_light_limit; /* display.dynamic_light_limit: "0" (no limit) / "4" */
	const char *hdr;                 /* display.hdr: NULL under QUALITY (left alone) / "false" */
};
/* performance nonzero: PERFORMANCE's values, else QUALITY's */
void ae_graphics_preset_values(int performance, struct ae_graphics_preset_values *values);
/* a preset's value of one of the eight keys; NULL for a key the presets do not set, or that this preset leaves alone
(HDR under QUALITY; the limits on Android and in a browser: values of NULL) */
const char *ae_graphics_preset_value(int performance, const char *key);

/* which preset the eight settings hold now (their texts in ae_graphics_preset_keys' order; case aside, NULL as ""):
AE_GRAPHICS_QUALITY, AE_GRAPHICS_PERFORMANCE, else AE_GRAPHICS_CUSTOM */
int ae_graphics_preset_of(const char *const now[AE_GRAPHICS_PRESET_ROWS]);
/* CUSTOM's rows set by hand: those that differ from the nearer preset (QUALITY when both are as near). Returns how
many; first (may be NULL) the first's index in ae_graphics_preset_keys, -1 for none */
int ae_graphics_preset_hand_set(const char *const now[AE_GRAPHICS_PRESET_ROWS], int *first);
/* the help of the row at CUSTOM: "CUSTOM: ANTI-ALIASING was set by hand. ..." or "CUSTOM: 2 rows were set by hand.
..." */
void ae_graphics_preset_custom_help(const char *const now[AE_GRAPHICS_PRESET_ROWS], char *text, size_t size);

/* (SPLIT SCREEN) whether two players play side by side, from display.split_two_players: "side_by_side" yes,
"top_and_bottom" no, "auto" (and any other text) yes on a screen 21:9 or wider (width / height at least 2.2), else
no. A size not known (0) counts as 16:9. The renderer's rule (ae_split_layout_rules.h ae_split_side_by_side) */
int ae_split_two_players_side_by_side(const char *two_players, long width, long height);

#endif
