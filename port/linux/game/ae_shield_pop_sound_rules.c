/* ae_shield_pop_sound_rules.c: which sound a shield pop plays. */

#include "ae_shield_pop_sound_rules.h"

enum ae_shield_pop_sound_source ae_shield_pop_sound_pick(int mcc_readable, int stock_available)
{
	if (mcc_readable)
		return _ae_shield_pop_sound_mcc;
	return stock_available ? _ae_shield_pop_sound_stock : _ae_shield_pop_sound_none;
}

int ae_shield_pop_sound_log_fallback(enum ae_shield_pop_sound_source source, int *logged)
{
	if (source != _ae_shield_pop_sound_stock || !logged || *logged)
		return 0;
	*logged = 1;
	return 1;
}
