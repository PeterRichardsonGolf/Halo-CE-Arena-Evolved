/*
AE_OVERLAY_PING.H

The ping on the performance line (owner 2026-10-29: in place of the latency meter's own corner): in a network game
being played, display.performance_ping (on by default) adds "PING 45 MS" to display.performance's line, the host's
"MAX PING" of its slowest client, "PING --" while the host is not heard (latency_meter.c's measures). Nothing in the
menus, with the line off, or in a local game.
*/

#ifndef __AE_OVERLAY_PING_H
#define __AE_OVERLAY_PING_H

/* the words for a round trip (NONE: none measured yet), by host or client; written to text */
void ae_overlay_ping_format(char *text, long size, long ping, int host, int problem);
/* whether the performance line has the ping (display.performance not "off", display.performance_ping on): the
round trips are then refreshed as for the latency meter (network_distributed.c), whose own corner is not drawn */
int ae_overlay_ping_wanted(void);
/* appends them to the performance line (main.c main_framerate_render), when shown */
void ae_overlay_ping_append(char *line, long size);

#endif
