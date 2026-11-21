/*
AE_RECORD_STEP_RULES.H

The players a SERVICE RECORD page steps through with LT / RT (Page Up / Page Down), as pure rules
(port/linux/tests/ae_record_step_rules_test.c runs them). The list is every player the record keeps: the viewer's own first,
then the others from the one seen most recently to the one seen longest ago (the order of the record keeps a tie). Stepping
wraps at both ends. A record of one player has nothing to step to.
*/

#ifndef __AE_RECORD_STEP_RULES_H
#define __AE_RECORD_STEP_RULES_H

#include "ae_service_record.h"

/* the record's players in stepping order: their indices into record->player into out (room for AE_SR_PLAYERS); the count.
viewer_key may be empty (no player first) */
int ae_rstep_order(struct ae_service_record const *record, char const *viewer_key, int *out);

/* the index into record->player of the player a step of delta (positive next, else previous) from the player of current_key
reaches, wrapping; -1 when there is none to step to (no players, or only the current one). A current_key the record does not keep
steps to the first (next) or the last (previous) of the list */
int ae_rstep_target(struct ae_service_record const *record, char const *viewer_key, char const *current_key, int delta);

#endif
