/*
AE_RANGE_RULES.C

See ae_range_rules.h.
*/

#include <ctype.h>

#include "ae_range_rules.h"

int ae_range_rules_applies(int requested, int path_matches, int network_game, int network_server, int network_client)
{
	return requested && path_matches && !network_game && !network_server && !network_client;
}

int ae_range_rules_same_path(char const *asked, char const *loaded)
{
	if (!asked || !loaded)
		return 0;
	for (; *asked && *loaded; asked++, loaded++)
	{
		int a = *asked == '/' ? '\\' : tolower((unsigned char)*asked);
		int l = *loaded == '/' ? '\\' : tolower((unsigned char)*loaded);

		if (a != l)
			return 0;
	}
	return !*asked && !*loaded;
}
