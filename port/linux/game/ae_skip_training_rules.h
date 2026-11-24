/*
AE_SKIP_TRAINING_RULES.H

SKIP TRAINING (game.campaign_skip_training, off by default): the first level's startup script (mission_a10) plays the
cryo wake-up, the look calibration and the walk to the bridge (its tutorial_setup) only when the game is alone on
normal; in co-op, or on heroic and up, it takes its own short way instead (fast_setup: the cryo exit, no tutorial
prompts, no first-contact scene). The rule answers the script's game_difficulty_get with a difficulty above normal for
that one script, so a game on normal takes the short way. Pure (no engine includes;
port/linux/tests/ae_skip_training_rules_test.c).
*/

#ifndef AE_SKIP_TRAINING_RULES_H
#define AE_SKIP_TRAINING_RULES_H

/* the level and the script that read the difficulty to choose the training */
#define AE_SKIP_TRAINING_LEVEL "a10"
#define AE_SKIP_TRAINING_SCRIPT "mission_a10"

/* the difficulty (0 easy .. 3 legendary, the engine's _game_difficulty_level order) the script is told: real, except
that normal becomes heroic for the level's startup script (the level by its short name or its path) when the setting is on; any other script, level or setting
is told the real one */
int ae_skip_training_difficulty(int enabled, int real_difficulty, const char *level, const char *script);

#endif
