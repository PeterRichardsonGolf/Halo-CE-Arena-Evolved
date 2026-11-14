/*
AE_SHIELD_POP_SOUND_RULES.H

Which sound a shield pop plays, and when its choice is logged. No engine includes: the unit test in
port/linux/tests/ae_shield_pop_sound_rules_test.c compiles these alone.
*/

#ifndef __AE_SHIELD_POP_SOUND_RULES_H
#define __AE_SHIELD_POP_SOUND_RULES_H

enum ae_shield_pop_sound_source
{
	/* nothing to play (the map holds no stock sound either) */
	_ae_shield_pop_sound_none = 0,
	/* the player's own MCC install's shield-break sound, read at run time */
	_ae_shield_pop_sound_mcc,
	/* the stock game's shield depleted sound, the map's own */
	_ae_shield_pop_sound_stock,
};

/* the MCC sound when it could be read, else the stock one when the map has it, else none */
enum ae_shield_pop_sound_source ae_shield_pop_sound_pick(int mcc_readable, int stock_available);

/* whether the fallback is worth a log line now: once per session, and only for the stock fallback (*logged is the
caller's flag, set here) */
int ae_shield_pop_sound_log_fallback(enum ae_shield_pop_sound_source source, int *logged);

#endif
