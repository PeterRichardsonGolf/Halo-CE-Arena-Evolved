/*
AE_FP_MODEL_RULES.H

A WEAPON WHOSE FIRST-PERSON MODEL IS ITS WORLD MODEL (port/linux/tests/ae_fp_model_rules_test.c). The Social Map Pack's
shotgun names the weapon's world model as its first-person model, so the first person shows the hands and no gun. The
first-person animation graph (weapons\shotgun\fp\fp) has a model of its own name, and that is the first-person model.
This is the rule MCC maps get in mcc_tag_validate.c; ae_fp_model.c runs it on every map's weapons.

The rule: the first-person model is set, equals the world model, the first-person animations are named, and a model of
that name exists and is not the one in use. Every other weapon is left as it is.
*/

#ifndef __AE_FP_MODEL_RULES_H
#define __AE_FP_MODEL_RULES_H

/* indexes are tag indexes (-1 = none); the replacement is the model tag found by the graph's name, -1 if there is none */
static inline int ae_fp_model_needs_replacing(long model, long first_person, int graph_named, long replacement)
{
	return first_person != -1 && first_person == model && graph_named && replacement != -1 && replacement != first_person;
}

#endif
