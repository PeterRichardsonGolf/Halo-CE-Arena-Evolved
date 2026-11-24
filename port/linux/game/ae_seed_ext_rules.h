/* The AE block a built-in casual or BTB gametype carries besides its variant (rc2): SHOOTABLE GRENADES and JOIN IN PROGRESS: SPLIT SCREEN on, and on the
capture the flag ones the modern flag rules (H2 FLAG RETURN, FLAG PICKUP by holding, FLAG MELEE KILLS, no DRIVE WITH FLAG, the
own flag's waypoint when spotted). The slower carrier stays off. Every AE gametype (casual, comp, drills, training, NEW MODES;
not NHE's, which is the CLASSIC COMP style) also has FALL STUN off. A built-in's block is the preset table's
(ae_h2_presets_rules.h) with these over it. No game dependency: unit-tested (port/linux/tests/ae_seed_ext_rules_test.c). */
#ifndef AE_SEED_EXT_RULES_H
#define AE_SEED_EXT_RULES_H

#include "ae_variant_ext_rules.h"

enum
{
	/* what a gametype is (arena_gametype_names.c's arena_gametype_seed_ext_flags says it of a stored name) */
	AE_SEED_EXT_CASUAL = 1,   /* the AE CASUAL style: the AE, AE BR and NEW MODES sets, not AE COMP, the drills, TRAINING or NHE's */
	AE_SEED_EXT_CTF = 2,      /* its engine is capture the flag */
	AE_SEED_EXT_NEW_MODE = 4, /* a NEW MODES preset: its flag rules are its own (CTF CLASSIC's touch return) */
	AE_SEED_EXT_AE_RULES = 8, /* an AE gametype of any style (arena_gametype_has_ae_rules): not NHE's or a stock one */
	/* the own flag's waypoint rule: away from home, a carried flag only while a teammate sees its carrier */
	AE_SEED_EXT_FLAG_WAYPOINT = AE_EXT_WAYPOINT_SPOTTED
};

/* the seed rules over ext (the table's bytes stay: only these fields are set); flags without AE_SEED_EXT_CASUAL: no casual rules; FALL STUN off with AE_SEED_EXT_AE_RULES */
void ae_seed_ext_apply(unsigned int flags, struct ae_variant_ext *ext);

#endif
