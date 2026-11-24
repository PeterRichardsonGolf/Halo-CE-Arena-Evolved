/*
AE_RANGE_RULES.H

Whether the firing range (firing_range.c) applies to a map being loaded (pure: unit test port/linux/tests/ae_range_rules_test.c).
The range is asked for by DEV MODE's `range` and plays alone: it never applies in a network game, a hosted game, a game of a
dedicated server or one joined as a client, and only on the map path it was asked for.
*/

#ifndef __AE_RANGE_RULES_H
#define __AE_RANGE_RULES_H

/* whether the loaded map is the range: asked for, the map path the one asked for (path_matches), and no network game of any
kind (a network game's game, a server, or a client of someone's) */
int ae_range_rules_applies(int requested, int path_matches, int network_game, int network_server, int network_client);
/* whether two map paths are the same one (any case, either slash) */
int ae_range_rules_same_path(char const *asked, char const *loaded);

#endif /* __AE_RANGE_RULES_H */
