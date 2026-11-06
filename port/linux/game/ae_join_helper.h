/*
AE_JOIN_HELPER.H

The join helper (0.7.5; mockups-075-toggles/final/6): a join that did not happen says why, in plain words, over the
screen the player joined from (the server browser, JOIN BY ADDRESS, JOIN WITH INVITE), with one action for the cause
(ae_join_helper_rules.h's): UPDATE, JOIN AGAIN / RETRY / TRY AGAIN, or OK; a Workshop map adds COPY WORKSHOP LINK.
The dialog is ae_screen_attention.c's.
*/

#ifndef __AE_JOIN_HELPER_H
#define __AE_JOIN_HELPER_H

#include "ae_join_helper_rules.h"
#include "ae_widgets.h"

struct ae_join_helper_request
{
	int cause;                     /* AE_JH_* */
	char const *detail;            /* the game's own words (AE_JH_GENERIC, AE_JH_MAP) */
	char const *map;               /* the game's map name (<name>@ws<item>: the Workshop item) */
	char const *item_title;        /* the map's title (NULL: its name) */
	struct ae_density density;
	/* JOIN AGAIN / RETRY / TRY AGAIN: join the same game again (NULL: the button is OK) */
	void (*again)(void *context);
	void *context;
};

/* shows it; 0 when it could not be opened */
int ae_join_helper_show(struct ae_join_helper_request const *request);

/* a join that failed with the lobby's reason (AE_STR_ERR_*, ae_glue_lobby.h's ae_lobby_join_failure_reason): the cause
a rejection code the host sent (seen in the last few seconds) gives, else the reason's. Shown; 1 when it was (the
caller does not show its own error), 0 when this failure keeps the caller's words (AE_JH_GENERIC) */
int ae_join_helper_failed(int reason, char const *detail, struct ae_density const *density,
	void (*again)(void *context), void *context);

/* (network_client_manager.c's hook) the host sent rejection code while an Arena Evolved join waits: noted, and the
join screen shows it; ae_join_helper_taken says whether the game's own error box is left out */
void ae_join_helper_rejected(unsigned short code);
int ae_join_helper_taken(void);

/* UPDATE: asks for a newer version as the game does at its start (an install, or on a Mac its download page) */
void ae_join_helper_update(void);

#endif
