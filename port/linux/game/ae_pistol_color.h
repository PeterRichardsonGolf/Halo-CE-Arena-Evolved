/*
AE_PISTOL_COLOR.H

PISTOL COLOR and PISTOL TRIM (VIEW > WEAPON COSMETICS, display.pistol_color,
display.pistol_trim): your first-person pistol in your own two colours
(ae_pistol_color.c). rasterizer_xbox_models.c's AE hook asks it.
*/

#ifndef __AE_PISTOL_COLOR_H
#define __AE_PISTOL_COLOR_H

/* the change colour a model's shader (its tag's data) is drawn with: for the stock pistol's first-person shaders in a
view whose player chose a colour, that colour; else as it was */
void ae_pistol_color_apply(void const *shader, boolean first_person, real_rgb_color *change_color);
/* the multipurpose map such a shader is drawn with: none (the renderer's default, which masks no colour) for the silver
body while its player chose a COLOR; else its own */
long ae_pistol_color_multipurpose(void const *shader, boolean first_person, long multipurpose_map_index);

#endif /* __AE_PISTOL_COLOR_H */
