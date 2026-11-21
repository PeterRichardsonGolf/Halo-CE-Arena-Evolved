/*
AE_CTF_RETURN_RULES.C

See ae_ctf_return_rules.h.
*/

#include "ae_ctf_return_rules.h"
#include "ae_variant_ext_rules.h"

long ae_ctf_return_seconds(int mode, unsigned char reset_time)
{
	if (reset_time)
		return reset_time;
	return mode == AE_EXT_CTF_RETURN_H2 || mode == AE_EXT_CTF_RETURN_STAND ? 30 : 17;
}

long ae_ctf_return_step(int mode, int defenders, int enemies)
{
	if (enemies > 0)
		return 0;
	if (mode == AE_EXT_CTF_RETURN_H2)
		return 1;
	if (mode == AE_EXT_CTF_RETURN_STAND)
	{
		if (defenders > AE_CTF_RETURN_DEFENDERS_MOST)
			defenders = AE_CTF_RETURN_DEFENDERS_MOST;
		return 1 + AE_CTF_RETURN_BOOST * (defenders > 0 ? defenders : 0);
	}
	return 0;
}

int ae_ctf_return_touch_returns(int mode)
{
	return mode != AE_EXT_CTF_RETURN_STAND;
}

int ae_sudden_death_hold(int in_play, long now, long *last_in_play, long grace_ticks)
{
	if (*last_in_play > now)
		*last_in_play = -1;
	if (in_play)
		*last_in_play = now;
	return *last_in_play >= 0 && now - *last_in_play <= grace_ticks;
}

int ae_ctf_hold_pickup(unsigned char ctf_flags, int flag_team, int player_team)
{
	return (ctf_flags & AE_EXT_CTF_HOLD_PICKUP) && flag_team >= 0 && flag_team != player_team;
}
