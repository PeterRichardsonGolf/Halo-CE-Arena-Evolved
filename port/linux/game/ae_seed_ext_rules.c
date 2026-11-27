#include "ae_seed_ext_rules.h"

void ae_seed_ext_apply(unsigned int flags, struct ae_variant_ext *ext)
{
	if (flags & AE_SEED_EXT_AE_RULES)
		ext->fall_stun = AE_EXT_FALL_STUN_OFF;
	if (!(flags & AE_SEED_EXT_CASUAL))
		return;
	ext->shootable_grenades = AE_EXT_SHOOTABLE_GRENADES_ON;
	ext->split_join = AE_EXT_SPLIT_JOIN_ON;
	if ((flags & AE_SEED_EXT_CTF) && !(flags & AE_SEED_EXT_NEW_MODE))
	{
		ext->ctf_return = AE_EXT_CTF_RETURN_H2;
		ext->ctf_flags = (unsigned char)(ext->ctf_flags | AE_EXT_CTF_HOLD_PICKUP | AE_EXT_CTF_MASSIVE_DAMAGE |
			AE_EXT_CTF_NO_DRIVING);
		ext->ctf_own_flag_waypoint = AE_SEED_EXT_FLAG_WAYPOINT;
	}
}
