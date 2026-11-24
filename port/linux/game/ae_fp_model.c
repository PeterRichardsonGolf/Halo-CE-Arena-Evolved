/*
AE_FP_MODEL.C
The map's weapon tags corrected in place (ae_fp_model_rules.h). Runs on every map load path.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "cache/cache_files.h"
#include "items/weapon_definitions.h"
#include "models/model_definitions.h"
#include "tag_files/tag_files.h"
#include "ae_fp_model.h"
#include "ae_fp_model_rules.h"

long tag_loaded(long group_tag, const char *name);

void ae_fp_model_new_map(void)
{
	struct tag_iterator iterator;
	long weapon_definition_index;

	tag_iterator_new(&iterator, WEAPON_DEFINITION_TAG);
	while ((weapon_definition_index = tag_iterator_next(&iterator)) != NONE)
	{
		struct weapon_definition *weapon = weapon_definition_get(weapon_definition_index);
		struct weapon_interface_definition *interface_definition = &weapon->weapon.interface_definition;
		long graph_index = interface_definition->first_person_animations.index;
		char const *graph_name = graph_index == NONE ? NULL : tag_get_name(graph_index);
		long replacement = NONE;

		if (graph_name && graph_name[0] && interface_definition->first_person_model.index != NONE)
			replacement = tag_loaded(MODELS_GROUP_TAG, graph_name);
		if (ae_fp_model_needs_replacing(weapon->object.model.index, interface_definition->first_person_model.index,
			graph_name && graph_name[0], replacement))
		{
			interface_definition->first_person_model.index = replacement;
			error(_error_silent, "ae fp model: weapon %s: first-person model is its world model, %s is used",
				tag_get_name(weapon_definition_index), graph_name);
		}
	}
}
