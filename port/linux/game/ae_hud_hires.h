/*
AE_HUD_HIRES.H

A map's HUD digits made for a high-resolution HUD (Halo Custom Edition's Gearbox maps rebuilt with one:
ui/hud/counter.hud_number with a digit sheet twice the size and a screen digit width of 18 where the stock tag has
9, meant to be drawn at half scale) are drawn at the stock digits' size: hud_draw.c's hud_draw_numbers scales its
digits, their spacing and its trailing m by this factor (an AE hook line). Every number drawn from the map's digits
(the waypoints' distances, the ammo counters of the classic HUD, the HUD messages' counts) comes out as on a stock map.
*/

#ifndef __AE_HUD_HIRES_H
#define __AE_HUD_HIRES_H

/* the stock digit tag's screen digit width (ui/hud/counter.hud_number) */
#define AE_HUD_STOCK_DIGIT_WIDTH 9
/* a screen digit width this wide or wider is a high-resolution HUD's */
#define AE_HUD_HIRES_DIGIT_WIDTH 14

/* the factor for digits whose tag spaces them screen_digit_width apart: 1 for a stock-sized tag */
float ae_hud_number_scale(int screen_digit_width);

/* A HUD multitexture overlay's SUBTRACT, as the renderer's blend function (hud_draw.c, an AE hook line). The only
stock overlays are the sniper rifle's range bars, whose ticks fade in with the zoom: the Xbox's tag multiplies its mask
by the ticks (no zoom, no bars), Halo PC's (a Custom Edition map's) subtracts them, and the Xbox's subtract (the mask
less the ticks) then draws the bars at every zoom, the ticks cut out of them. On a Custom Edition map it is drawn as
the Xbox's multiply; an Xbox map's keeps the Xbox's subtract. */
int ae_hud_multitexture_subtract(int custom_edition, int xbox_subtract, int multiply);

#endif
