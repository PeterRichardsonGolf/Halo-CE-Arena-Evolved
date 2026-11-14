/*
AE_DEFAULTS_OFFER.H

NEW SETTINGS (0.7.8): the one question an existing config.toml gets when this version changes some default settings it
still has at the old ones (port_config.c's config_old_defaults, config_find_new_defaults): an attention dialog over the main
menu, "This version changes some default settings:", one plain line a change, USE THE NEW SETTINGS or KEEP MINE (B keeps).
The answer is written (game.defaults_answered), so it is asked once. A new install has the new defaults and no question.

Its log: "defaults offer: queued (N changes)" when it opens, "defaults offer: the player chose the new settings" / "to
keep theirs"; HALO_DEFAULTS_ANSWER=use or keep answers it as if clicked (the tests).
*/

#ifndef __AE_DEFAULTS_OFFER_H
#define __AE_DEFAULTS_OFFER_H

#include "ae_widgets.h"

/* opens the dialog over the stack when a change is pending and it has not been shown in this run; 1 when it opened */
int ae_defaults_offer_open(struct ae_density const *density);

#endif
