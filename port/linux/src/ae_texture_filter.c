/*
AE_TEXTURE_FILTER.C

ANISOTROPIC FILTERING (ae_texture_filter.h).
*/

#include "xgpu.h"
#include "port_config.h"
#include "ae_texture_filter.h"

#include <string.h>

int ae_texture_anisotropy_level(const char *setting)
{
	static const char *const names[] = { "2x", "4x", "8x", "16x" };
	static const int levels[] = { 2, 4, 8, 16 };
	int index;

	for (index = 0; setting && index < (int)(sizeof(levels) / sizeof(levels[0])); index++)
	{
		if (!strcmp(setting, names[index]))
			return levels[index];
	}
	return 1;
}

void ae_texture_anisotropy(DWORD *min_filter, DWORD *anisotropy, DWORD mip_filter, int hires)
{
	/* (read again only when a setting has changed: this runs for every stage of every draw) */
	static unsigned long changes_seen = (unsigned long)-1;
	static int level = 1;

	if (config_changes() != changes_seen)
	{
		int read = ae_texture_anisotropy_level(config_string("display.anisotropic"));

		changes_seen = config_changes();
		if (read != level)
			platform_log("texture filter: anisotropic %dx (display.anisotropic)", read);
		level = read;
	}
	if (level <= 1 || hires || *min_filter != D3DTEXF_LINEAR || mip_filter == D3DTEXF_NONE)
		return;
	*min_filter = D3DTEXF_ANISOTROPIC;
	*anisotropy = (DWORD)level;
}
