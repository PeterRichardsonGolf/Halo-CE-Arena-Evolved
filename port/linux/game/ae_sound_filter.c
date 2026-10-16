/*
AE_SOUND_FILTER.C

NHE SOUND FILTER (ae_sound_filter.h).
*/

#include "cseries.h"
#include "tag_files/tag_files.h"
#include "../src/port_config.h"

#include "ae_sound_filter.h"

#include <string.h>

/* the stock maps' sound tags NHE 1.0 muted: the plasma pistol's charge (it shares the plasma rifle's sounds; the
plasma rifle never charges), its start, and the needler's shot */
static const char *const muted_names[] =
{
	"sound\\sfx\\weapons\\plasma rifle\\charging",
	"sound\\sfx\\weapons\\plasma rifle\\startcharge",
	"sound\\sfx\\weapons\\needler\\fire",
};

void platform_log(char const *format, ...);

/* (the muted sound's place in muted_names, else -1) */
static int muted_index(const char *name)
{
	size_t index;

	for (index = 0; name && index < sizeof(muted_names) / sizeof(muted_names[0]); index++)
	{
		if (!strcmp(name, muted_names[index]))
			return (int)index;
	}
	return -1;
}

int ae_sound_filter_muted_name(const char *name)
{
	return muted_index(name) >= 0;
}

float ae_sound_filter_gain(long definition_index)
{
	/* (the setting read again only when a setting changed) */
	static unsigned long changes_seen = (unsigned long)-1;
	static int on;

	if (config_changes() != changes_seen)
	{
		changes_seen = config_changes();
		on = config_boolean("audio.nhe_sound_filter") != 0;
	}
	if (!on || definition_index == NONE)
		return 1.0f;
	{
		/* (each muted sound's first time logged, once a run: the filter seen working) */
		static unsigned char logged[sizeof(muted_names) / sizeof(muted_names[0])];
		int index = muted_index(tag_get_name(definition_index));

		if (index < 0)
			return 1.0f;
		if (!logged[index])
		{
			logged[index] = 1;
			platform_log("nhe sound filter: muted %s", muted_names[index]);
		}
		return 0.0f;
	}
}
