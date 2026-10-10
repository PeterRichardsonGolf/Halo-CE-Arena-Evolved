/*
AE_MOUSE.H

The mouse look of Arena Evolved's MCC style, as pure functions (no engine or
config calls: port/linux/tests/ae_mouse_test.c runs them). The style row and
the numbers come from config.toml (ae_glue_mouse.c reads them and hooks into
the platform layer and the player's control).

CLASSIC is the port's own: 0.0022 radians per count times the multiplier.
MCC is Halo: The Master Chief Collection's number: the sensitivity / 45
degrees per count, the same for yaw and pitch, which puts 1.0 about 1% above
CS:GO and other Source games (0.022 degrees per count).
*/

#ifndef __AE_MOUSE_H
#define __AE_MOUSE_H

enum
{
	AE_MOUSE_CLASSIC = 0,
	AE_MOUSE_MCC = 1
};

/* the classic radians per count at multiplier 1 (xinput_sdl.c's) */
#define AE_MOUSE_CLASSIC_SCALE 0.0022f
/* one MCC sensitivity unit in radians per count (1 / 45 degree) */
#define AE_MOUSE_MCC_UNIT 0.00038785094f
/* MCC value of the classic multiplier 1.0: (0.0022 rad in degrees) * 45 */
#define AE_MOUSE_SEED_RATIO 5.6723f
#define AE_MOUSE_MCC_MINIMUM 0.1f
#define AE_MOUSE_MCC_MAXIMUM 10.0f
#define AE_MOUSE_SCALE_MINIMUM 0.1f
#define AE_MOUSE_SCALE_MAXIMUM 2.0f

/* "mcc" (any case) is MCC, anything else classic */
int ae_mouse_style_from_text(char const *text);

/* radians per count: classic is 0.0022 x the multiplier, MCC the value x pi / 8100 */
float ae_mouse_classic_radians(float multiplier);
float ae_mouse_mcc_radians(float value);

/* the MCC value that keeps a classic multiplier's feel: x 5.6723, one decimal, 0.1 to 10 */
float ae_mouse_seed(float classic_multiplier);

/* a value pinned to the legal range, 0.1 to 10 (sensitivity), 0.1 to 2 (the scales) */
float ae_mouse_clamp_sensitivity(float value);
float ae_mouse_clamp_scale(float value);

/* what the zoomed and vehicle scales multiply the look by: 1 in classic style, whatever the keys say */
float ae_mouse_zoom_factor(int style, float scale, int zoomed);
float ae_mouse_vehicle_factor(int style, float scale, int in_vehicle);

/* MCC's look for the counts since the last call: the same radians per count for yaw and pitch (the vertical
sensitivity key is not read), pitch inverted when invert */
void ae_mouse_mcc_look(float value, float x, float y, int invert, float *yaw, float *pitch);

/* debug.test_input "mouse:<dx>,<dy>[,zoom]" (counts, may be fractions and negative; zoom holds the zoom button):
TRUE, the counts and whether to zoom when it is one */
int ae_mouse_test_parse(char const *setting, float *dx, float *dy, int *zoom);

#endif
