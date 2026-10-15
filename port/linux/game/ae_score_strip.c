/*
AE_SCORE_STRIP.C

The score strip's drawing (ae_score_strip.h). For now the HUD's hook for the
counter digits' debug draw (ae_hud_digits.c); the strip itself is drawn here
from the next step on.
*/

#include "cseries/cseries.h"

#include "ae_hud_digits.h"
#include "ae_score_strip.h"

void ae_score_strip_draw(short local_player_index)
{
	ae_hud_digits_debug_draw(local_player_index);
}
