/*
AE_SKIP_TRAINING.H

SKIP TRAINING (game.campaign_skip_training): see ae_skip_training_rules.h. The hs function game_difficulty_get asks
ae_skip_training_difficulty for the difficulty a script is told.
*/

#ifndef AE_SKIP_TRAINING_H
#define AE_SKIP_TRAINING_H

/* the difficulty the running script is told (the engine's real one, unless the rule changes it) */
short ae_skip_training_difficulty_told(short real_difficulty);

/* one line for the DEV MODE command: the setting, and how many times the rule changed the answer */
void ae_skip_training_status(char *text, unsigned long size);

#endif
