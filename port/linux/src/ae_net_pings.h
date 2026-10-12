/*
AE_NET_PINGS.H

Arena Evolved: the server browser's pings (DamnationCE v0.3.41's, carried in
p2p.c, p2p_lobby.c and p2p_signal.c) under AE's rules:

- SERVER PINGS (network.browser_pings, Settings > Network) is an opt-in, off
  by default, as listing hosted games and sharing stats are: timing a host's
  ping makes the same connection a join makes, so each listed host sees this
  machine's address. Off, no host is probed (p2p_lobby.c's update_probes, an
  AE hook) and the rows show what they showed before.
- REFRESH times again only a host not probed in the last
  AE_BROWSER_PROBE_REFRESH_MINIMUM milliseconds (pressing it again and again
  sends no burst of join requests to every host).
- A shown ping is coloured by how good it is (DamnationCE's thresholds and
  colours); a failed or closed game shows FAILED or CLOSED, never a ping.

The decisions are static inline, so p2p_lobby.c and the menus take them
without a link (tools/p2p_lobby_check.c builds p2p_lobby.c alone);
port/linux/tests/ae_net_pings_test.c checks them.
*/

#ifndef __AE_NET_PINGS_H
#define __AE_NET_PINGS_H

#include <stddef.h>

enum
{
	AE_BROWSER_PING_GOOD = 80,
	AE_BROWSER_PING_FAIR = 160,
	AE_BROWSER_PING_COLOR_GOOD = 0x5ED38C,
	AE_BROWSER_PING_COLOR_FAIR = 0xE8C547,
	AE_BROWSER_PING_COLOR_POOR = 0xE86A5A,
	AE_BROWSER_PROBE_REFRESH_MINIMUM = 30000,
};

/* network.browser_pings (port/linux/src/ae_net_pings.c) */
int ae_browser_pings_on(void);

/* whether REFRESH times a host again: never probed, or probed (probed_time,
p2p_now's milliseconds, 0: never) at least AE_BROWSER_PROBE_REFRESH_MINIMUM ago */
static inline int ae_browser_probe_refresh_due(unsigned long probed_time, unsigned long now)
{
	return !probed_time || (unsigned long)(now - probed_time) >= (unsigned long)AE_BROWSER_PROBE_REFRESH_MINIMUM;
}

/* the ping a row shows (-1: none, the row's own text stands): pings on, one
known, and the game neither failed nor closed */
static inline int ae_browser_ping_shown(int pings_on, int ping, int failed_or_closed)
{
	return pings_on && ping >= 0 && !failed_or_closed ? ping : -1;
}

/* text: the ping shown, in milliseconds (size wide characters); FALSE (text
untouched) if none is */
static inline int ae_browser_ping_text_of(int pings_on, int ping, int failed_or_closed, wchar_t *text, int size)
{
	char digits[16];
	int shown = ae_browser_ping_shown(pings_on, ping, failed_or_closed);
	int length = 0;
	int index;

	if (shown < 0 || !text || size < 2)
		return 0;
	if (shown > 99999)
		shown = 99999;
	do
	{
		digits[length++] = (char)('0' + shown % 10);
		shown /= 10;
	}
	while (shown && length < (int)sizeof(digits));
	for (index = 0; index < length && index < size - 1; index++)
		text[index] = (wchar_t)digits[length - 1 - index];
	text[index] = 0;
	return 1;
}

/* rgb (0 to 1): the shown ping's colour, green under AE_BROWSER_PING_GOOD,
yellow under AE_BROWSER_PING_FAIR, red above; FALSE (rgb untouched) if no
ping is shown */
static inline int ae_browser_ping_rgb_of(int pings_on, int ping, int failed_or_closed, float *rgb)
{
	int shown = ae_browser_ping_shown(pings_on, ping, failed_or_closed);
	unsigned long color;

	if (shown < 0 || !rgb)
		return 0;
	color = shown < AE_BROWSER_PING_GOOD ? AE_BROWSER_PING_COLOR_GOOD : shown < AE_BROWSER_PING_FAIR ?
		AE_BROWSER_PING_COLOR_FAIR : AE_BROWSER_PING_COLOR_POOR;
	rgb[0] = (float)((color >> 16) & 0xFF) / 255.0f;
	rgb[1] = (float)((color >> 8) & 0xFF) / 255.0f;
	rgb[2] = (float)(color & 0xFF) / 255.0f;
	return 1;
}

/* the menus' hook lines: the same, with network.browser_pings read */
static inline int ae_browser_ping_text(int ping, int failed_or_closed, wchar_t *text, int size)
{
	return ae_browser_ping_text_of(ae_browser_pings_on(), ping, failed_or_closed, text, size);
}

static inline int ae_browser_ping_rgb(int ping, int failed_or_closed, float *rgb)
{
	return ae_browser_ping_rgb_of(ae_browser_pings_on(), ping, failed_or_closed, rgb);
}

#endif
