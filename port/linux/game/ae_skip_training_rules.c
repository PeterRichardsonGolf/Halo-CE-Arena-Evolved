/*
AE_SKIP_TRAINING_RULES.C

SKIP TRAINING's rule (ae_skip_training_rules.h).
*/

#include <string.h>

#include "ae_skip_training_rules.h"

enum { DIFFICULTY_NORMAL = 1, DIFFICULTY_HEROIC = 2 };

int ae_skip_training_difficulty(int enabled, int real_difficulty, const char *level, const char *script)
{
	const char *leaf;

	if (!enabled || real_difficulty != DIFFICULTY_NORMAL || !level || !script)
		return real_difficulty;
	/* (the level is named by its path, levels\a10\a10, or by its short name: the last part of it counts) */
	for (leaf = level; *level; level++)
		if (*level == '\\' || *level == '/')
			leaf = level + 1;
	if (strcmp(leaf, AE_SKIP_TRAINING_LEVEL) || strcmp(script, AE_SKIP_TRAINING_SCRIPT))
		return real_difficulty;
	return DIFFICULTY_HEROIC;
}
