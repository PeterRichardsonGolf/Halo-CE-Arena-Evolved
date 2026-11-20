/*
AE_MARK_PROMPT.H

The AIM PROMPT (ae_mark_prompt.c): the mark button's picture and a word (ENEMY, SNIPER, GO HERE, OK...) over what a press of it
would act on, one a view. It follows ae_marks_aim (ae_mark_targets.h), which looks again every 3 ticks, so what it names is
checked on every draw: the player alive, the object still there. Where it sits is ae_mark_prompt_rules.c's (unit tested); the
button's picture is ae_key_prompts.c's.

Shown with display.mark_prompt (HUD > RETICLE, each player's, ON) and the profile's IN-GAME HELP on. Hidden while the view's
wheel is open, while dead, with the scoreboard up and in a cinematic. On a marker the prompt is the first row of the marker's
block (ae_mark_prompt_row); until the block draws it, the row stands by the marker's glyph.

Include after cseries.h.
*/

#ifndef __AE_MARK_PROMPT_H
#define __AE_MARK_PROMPT_H

#include <wchar.h>

/* hud.c, after the wheel: this local player's prompt, in the view being drawn */
void ae_mark_prompt_draw(short local_player_index);

/* whether this player has the prompt on: display.mark_prompt, and the profile's hints on. The wheel's legend of hints follows it too */
boolean ae_mark_prompt_enabled(short local_player_index);

/* the word of the prompt this player's aim has now, for a marker's block to draw as its top row (the button's picture is
ae_key_prompts_draw_chip's: AE_CHIP_MARK). FALSE when there is no prompt (off, hidden, nothing aimed at). Up to size - 1
characters, then 0 */
boolean ae_mark_prompt_row(short local_player_index, wchar_t *word, int size);

/* a marker's block (ae_marks_draw, which runs before the prompt) says in this frame that it drew the prompt's row, so the
prompt draws none of its own; the next frame starts again */
void ae_mark_prompt_block_takes(short local_player_index, boolean takes);

#endif
