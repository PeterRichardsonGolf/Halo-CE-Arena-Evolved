/*
AE_FP_MODEL.H
What the AE hook line in source/items/weapons.c calls (ae_fp_model_rules.h; port/linux/game/ae_hooks.txt lists it).
Included after cseries.h.
*/

#ifndef __AE_FP_MODEL_H
#define __AE_FP_MODEL_H

/* weapons_initialize_for_new_map: a weapon whose first-person model is its world model gets its own (one log line each) */
void ae_fp_model_new_map(void);

#endif
