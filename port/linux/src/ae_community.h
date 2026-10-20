/*
AE_COMMUNITY.H

Arena Evolved: the community services' opt-in (0.3.5; rules in ae_community_rules.h). Whether this copy may use the
services the ChupathingyCE team runs at network.browser_url (halo.milenko.org), read from network.community_services,
and the opt-in's answer written to config.toml. The game list's thread (browser.c) and the stats' (event_upload.c)
ask ae_community_services_on before they contact that server; the game's side (ae_community_screen.c,
browser_screen.c) asks too. Plain types: the game's units and the platform's both call it. Without the game browser
(configure.py --no-game-browser) there are no services: off, and nothing to write.
*/

#ifndef __AE_COMMUNITY_H
#define __AE_COMMUNITY_H

#include "ae_community_rules.h"

/* network.community_services as an ae_community_rules.h state (AE_COMMUNITY_ASK without the game browser) */
int ae_community_state(void);

/* whether the services may be used now: network.community_services is "yes" (any thread; the log says once when a
change of it turns them off or on) */
int ae_community_services_on(void);

/* the opt-in's answer (ae_community_answer: use, and the screen's LIST HOSTED GAMES row) written to config.toml at
once: network.community_services, list_hosted_games, report_joined_games and report_events. 0 if a write failed */
int ae_community_write_answer(int use, int list_row_on);

#endif
