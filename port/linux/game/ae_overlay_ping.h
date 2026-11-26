/*
AE_OVERLAY_PING.H

PING (display.ping, SETTINGS > HUD > OVERLAYS; this PC's, not each player's): in a network game being played, the
overlay line ends with "PING 45 MS", the host's "MAX PING" of its slowest client, "PING --" while the host is not heard
(latency_meter.c's measures); NUMBER AND BARS adds four rising bars after it, the old latency meter folded in. With
PERFORMANCE off the line holds the ping alone, at the same POSITION. Nothing in the menus or in a local game.

display.ping is "off", "number" or "number_and_bars". An earlier version's settings carry over once: PERFORMANCE on with
display.performance_ping (then on by default) is "number", and a profile with display.latency_meter on makes it
"number_and_bars" (ae_overlay_ping_profile_loaded).
*/

#ifndef __AE_OVERLAY_PING_H
#define __AE_OVERLAY_PING_H

union rectangle2d;

enum { AE_OVERLAY_PING_OFF, AE_OVERLAY_PING_NUMBER, AE_OVERLAY_PING_BARS };
/* the bars' marker in the line: a word of its own (AE's HUD style draws the bars there; CLASSIC's line has none) */
#define AE_OVERLAY_PING_BARS_MARK '\x01'
/* the bars (four) and the gaps between them, in multiples of the line's text size */
#define AE_OVERLAY_PING_BAR_WIDTH 0.18f
#define AE_OVERLAY_PING_BAR_GAP 0.09f
#define AE_OVERLAY_PING_BAR_HEIGHT 0.72f

/* the words for a round trip (NONE: none measured yet), by host or client; written to text */
void ae_overlay_ping_format(char *text, long size, long ping, int host, int problem);
/* display.ping as a mode (AE_OVERLAY_PING_*); anything else is off */
int ae_overlay_ping_mode_of(const char *value);
int ae_overlay_ping_mode(void);
/* whether the overlay line has the ping (any mode but off): the round trips are then refreshed as for the latency
meter (network_distributed.c), whose own corner is never drawn */
int ae_overlay_ping_wanted(void);
/* whether the scoreboard's pings are coloured and timed as the meter's (NUMBER AND BARS) */
int ae_overlay_ping_bars_wanted(void);
/* appends them to the overlay line (main.c main_framerate_render), where there is a game being played */
void ae_overlay_ping_append(char *line, long size);
/* AE's HUD style (ae_hud_style_draw.c): the bars at x (screen pixels), their bottom at baseline, text_size the line's
size; returns their width (draws nothing when draw is 0) */
float ae_overlay_ping_bars_hud(float x, float baseline, float text_size, int draw);
/* CLASSIC's line (main.c, after it is drawn): the bars beside it, off the line's end that is not at the corner
(extent: where it was drawn, right: it is at the right corner) */
void ae_overlay_ping_bars_classic(const union rectangle2d *extent, int drawn, int right);
/* a profile's display.latency_meter as it is read (ae_glue_player_settings.c): "true" with display.ping not chosen yet
makes it NUMBER AND BARS, once */
void ae_overlay_ping_profile_loaded(const char *latency_meter);

#endif
