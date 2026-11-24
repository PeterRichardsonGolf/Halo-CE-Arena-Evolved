/*
AE_SKIP_TRAINING.C

SKIP TRAINING: see ae_skip_training_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "hs/hs.h"
#include "main/main.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"

#include "../src/port_config.h"
#include "ae_skip_training.h"
#include "ae_skip_training_rules.h"

void platform_log(char const *format, ...);

static unsigned long changed;

/* the level that is loaded. The main loop's map name is the last level picked for a game alone; a co-op game (the AI
squad's too) loads its own level and leaves that name as it was */
static const char *loaded_level(void)
{
	return global_scenario_index != NONE ? tag_get_name(global_scenario_index) : main_get_map_name();
}

short ae_skip_training_difficulty_told(short real_difficulty)
{
	int told;
	const char *script = hs_runtime_get_executing_thread_name();
	static int asked, asked_by_script;

	/* (the log tells the rule's inputs: the first askers, and the level's own script each of its first times) */
	if (asked < 6 || (script && !strcmp(script, AE_SKIP_TRAINING_SCRIPT) && asked_by_script < 4))
	{
		if (script && !strcmp(script, AE_SKIP_TRAINING_SCRIPT))
			asked_by_script++;
		asked++;
		platform_log("skip training: asked by %s on %s: setting %s, difficulty %d", script ? script : "(no script)",
			loaded_level(), config_boolean("game.campaign_skip_training") ? "on" : "off", (int)real_difficulty);
	}
	if (!config_boolean("game.campaign_skip_training"))
		return real_difficulty;
	told = ae_skip_training_difficulty(1, real_difficulty, loaded_level(), hs_runtime_get_executing_thread_name());
	if (told != real_difficulty)
	{
		changed++;
		platform_log("skip training: %s is told heroic, not normal (the short start)", AE_SKIP_TRAINING_SCRIPT);
	}
	return (short)told;
}

void ae_skip_training_status(char *text, unsigned long size)
{
	snprintf(text, size, "skip training %s (level %s's startup script, answers changed %lu)",
		config_boolean("game.campaign_skip_training") ? "on" : "off", AE_SKIP_TRAINING_LEVEL, changed);
}
