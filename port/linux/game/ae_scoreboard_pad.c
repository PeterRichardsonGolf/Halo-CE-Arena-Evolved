/* AE_SCOREBOARD_PAD.C: the open scoreboard's pages from each controller's D-pad (ae_scoreboard_pad_rules.h) */

#include "cseries/cseries.h"
#include "ae_dev_mode.h"
#include "ae_scoreboard_pad_rules.h"
#include "ae_scoreboard.h"

#define PORTS 4

int platform_scoreboard_is_open(void);
void platform_scoreboard_add_pages(long pages);
unsigned long system_milliseconds(void);

static struct ae_sb_pad pads[PORTS];

/* DEV MODE keeps BACK + D-pad down (the dev camera: BACK is what opens the board): no pad scroll there */
boolean ae_scoreboard_pad_scrolls(void)
{
	return (boolean)!ae_dev_mode_on();
}

void ae_scoreboard_pad(int port, unsigned short *buttons)
{
	long pages;

	if (port < 0 || port >= PORTS)
		return;
	pages = ae_sb_pad_take(&pads[port], ae_scoreboard_pad_scrolls() && platform_scoreboard_is_open(), buttons,
		system_milliseconds());
	if (pages)
		platform_scoreboard_add_pages(pages);
}
