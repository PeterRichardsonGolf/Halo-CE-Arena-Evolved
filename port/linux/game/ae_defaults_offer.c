/*
AE_DEFAULTS_OFFER.C

NEW SETTINGS (ae_defaults_offer.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/port_config.h"
#include "ae_defaults_offer.h"
#include "ae_screen_attention.h"

void platform_log(char const *format, ...);

enum { OFFER_LINES = 24, OFFER_LINE = 96, OFFER_TEXT = 1024 };

static int offered;
static char lines[OFFER_LINES][OFFER_LINE];
static char const *shown[OFFER_LINES];

static void picked(short button, void *context)
{
	(void)context;
	/* (B and anything but the first button keep the player's settings) */
	if (button == 0)
	{
		platform_log("defaults offer: the player chose the new settings");
		config_new_defaults_answer(1);
	}
	else
	{
		platform_log("defaults offer: the player chose to keep theirs");
		config_new_defaults_answer(0);
	}
}

int ae_defaults_offer_open(struct ae_density const *density)
{
	static char text[OFFER_TEXT];
	struct ae_attention_spec spec;
	char const *scripted = getenv("HALO_DEFAULTS_ANSWER");
	char const *line;
	short count = 0;
	int changes;

	if (offered)
		return 0;
	changes = config_new_defaults_pending(text, sizeof(text));
	if (!changes)
	{
		offered = 1;
		return 0;
	}
	for (line = text; *line && count < OFFER_LINES;)
	{
		size_t length = strcspn(line, "\n");

		if (length)
		{
			snprintf(lines[count], sizeof(lines[count]), "%.*s", (int)length, line);
			shown[count] = lines[count];
			count++;
		}
		line += length + (line[length] ? 1 : 0);
	}
	memset(&spec, 0, sizeof(spec));
	spec.title = "NEW SETTINGS";
	spec.body = "This version changes some default settings:";
	spec.list = shown;
	spec.list_count = count;
	spec.strong = "Use the new settings, or keep yours?";
	spec.buttons[0] = "USE THE NEW SETTINGS";
	spec.buttons[1] = "KEEP MINE";
	spec.button_count = 2;
	spec.focus = 0;
	spec.danger = -1;
	spec.back = 1;
	spec.note = "You can change any of them later in Settings.";
	spec.back_label = "Keep mine";
	if (density)
		spec.density = *density;
	spec.picked = picked;
	if (!ae_attention_open(&spec))
		return 0;
	offered = 1;
	platform_log("defaults offer: queued (%d changes)", (int)count);
	if (scripted && (!strcmp(scripted, "use") || !strcmp(scripted, "keep")))
	{
		platform_log("defaults offer: scripted answer %s", scripted);
		ae_attention_pick(!strcmp(scripted, "use") ? 0 : 1);
	}
	return 1;
}
