/*
AE_HEIGHT_FADE.H

A draw faded by height on its model, for HIT FEEDBACK's shield effects
(port/linux/game/ae_shield_fx.c): the draws that follow ae_height_fade_set
keep their colors only under a height, with a soft edge, and (with a band)
only over the band's own length under it. The height is the model's own z, at
rest, read from the plasma shader's noise coordinate (oT0.z = scale * z +
offset; rasterizer_xbox_plasma_energy.c).

d3d8_gl.c carries the five numbers to every program as two uniforms and
nv2a_vsh.c adds the lines below to each vertex shader (their AE hook lines).
A scale of 0 is off, as it is for every other draw.
*/

#ifndef __AE_HEIGHT_FADE_H
#define __AE_HEIGHT_FADE_H

/* inverse softness, height, offset, scale; then the band's length (0: none) */
extern float ae_height_fade_value[5];

void ae_height_fade_set(float inverse_softness, float height, float offset, float scale, float band);
void ae_height_fade_clear(void);

#define AE_HEIGHT_FADE_UNIFORMS \
	"uniform vec4 ae_height_fade;\n" \
	"uniform float ae_height_band;\n"

#define AE_HEIGHT_FADE_GLSL \
	"\tif (ae_height_fade.w != 0.0)\n" \
	"\t{\n" \
	"\t\tfloat model_z = (oT0.z - ae_height_fade.z) / ae_height_fade.w;\n" \
	"\t\tfloat faded = clamp((ae_height_fade.y - model_z) * ae_height_fade.x, 0.0, 1.0);\n" \
	"\t\tif (ae_height_band > 0.0)\n" \
	"\t\t\tfaded *= clamp((model_z - (ae_height_fade.y - ae_height_band)) / ae_height_band, 0.0, 1.0);\n" \
	"\t\toD0 *= faded;\n" \
	"\t\toD1 *= faded;\n" \
	"\t}\n"

#endif /* __AE_HEIGHT_FADE_H */
