#include "ae_strip_mode_rules.h"

enum ae_strip_mode ae_strip_mode_for_slayer(boolean round_mode, boolean kill_in_order, long lives)
{
	if (round_mode)
		return _ae_strip_rounds;
	if (kill_in_order)
		return _ae_strip_phantoms;
	return lives > 0 ? _ae_strip_elimination : _ae_strip_slayer;
}

enum ae_strip_mode ae_strip_mode_for_ball(boolean terminator, boolean magic)
{
	return terminator ? _ae_strip_juggernaut : magic ? _ae_strip_accumulation : _ae_strip_oddball;
}

enum ae_strip_mode ae_strip_mode_for_race(boolean rally)
{
	return rally ? _ae_strip_rally : _ae_strip_race;
}
