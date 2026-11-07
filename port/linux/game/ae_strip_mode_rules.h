/*
AE_STRIP_MODE_RULES.H

Which of the score strip's modes (ae_strip_model.h) a gametype uses, as pure decisions: ae_strip_engine.c asks, from the
variant's numbers. A round mode (ELIMINATION: ae_round.h) is always the rounds strip (alive cells, round pips), whatever
its lives say; Bomb is the rounds strip too (read_bomb). Unit-tested (port/linux/tests/ae_strip_mode_rules_test.c).
*/
#ifndef AE_STRIP_MODE_RULES_H
#define AE_STRIP_MODE_RULES_H

#include "ae_strip_model.h"

/* the Slayer engine: a round mode, else kill in order (phantoms), else lives (elimination's lives line), else slayer */
enum ae_strip_mode ae_strip_mode_for_slayer(boolean round_mode, boolean kill_in_order, long lives);
/* the Oddball engine: the ball type's mode (terminator: juggernaut, magic: accumulation, else oddball) */
enum ae_strip_mode ae_strip_mode_for_ball(boolean terminator, boolean magic);
/* the Race engine: rally or race */
enum ae_strip_mode ae_strip_mode_for_race(boolean rally);

#endif
