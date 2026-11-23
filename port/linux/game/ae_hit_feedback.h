/*
AE_HIT_FEEDBACK.H

HIT FEEDBACK (the rules: ae_hit_feedback_rules.h): what the AE hook lines call
(port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
*/

#ifndef __AE_HIT_FEEDBACK_H
#define __AE_HIT_FEEDBACK_H

#include "ae_hit_feedback_rules.h"

struct game_variant;
struct widget_instance;

/* the gametype's byte: bit 0 of universal_variant's pad5 (ae_hit_feedback_rules.h) */
#define AE_HIT_FEEDBACK_VARIANT_BYTE(variant) ((variant)->universal_variant.pad5)

/* TRUE in a multiplayer game whose gametype has HIT FEEDBACK: ARENA EVOLVED */
boolean ae_hit_feedback_active(void);
/* a game begins: its rule in debug.txt */
void ae_hit_feedback_log_rules(void);

/* the gametype editor's ARENA OPTIONS row: its spinner shown from the variant, or (save) written to it */
void ae_hit_feedback_menu_rows(struct widget_instance *list, struct game_variant *variant, boolean save);

#endif /* __AE_HIT_FEEDBACK_H */
