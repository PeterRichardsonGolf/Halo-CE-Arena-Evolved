/*
AE_MCC_OFFER.H

(0.7.9) The main menu's two MCC dialogs (ae_mcc_offer.c), each once: MCC FOUND, asked once more when Steam has MCC but an
earlier version's game.mcc_use is "no" (an earlier "ask" is turned on without asking: ae_mcc_platform.c); MCC NOT FOUND,
told once a version when Steam has no MCC. HALO_MCC_OFFER_ANSWER=use or not_now answers MCC FOUND as if clicked (tests).
*/

#ifndef __AE_MCC_OFFER_H
#define __AE_MCC_OFFER_H

#include "ae_widgets.h"

/* opens the one that is due over the stack (not twice a run); 1 when one opened */
int ae_mcc_offer_open(struct ae_density const *density);

#endif
