#include <stdio.h>
#include <string.h>
#include "ae_seed_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_variant_ext ext, none;

	ae_variant_ext_default(&none);

	/* a casual game: shootable grenades, nothing else */
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_CASUAL, &ext);
	CHECK(ext.shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON);
	CHECK(ext.split_join == AE_EXT_SPLIT_JOIN_ON);
	ext.shootable_grenades = 0;
	ext.split_join = 0;
	CHECK(ae_variant_ext_is_default(&ext));

	/* a casual capture the flag: the modern flag rules too */
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_CASUAL | AE_SEED_EXT_CTF, &ext);
	CHECK(ext.shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON);
	CHECK(ext.ctf_return == AE_EXT_CTF_RETURN_H2);
	CHECK(ext.ctf_flags & AE_EXT_CTF_HOLD_PICKUP);
	CHECK(ext.ctf_flags & AE_EXT_CTF_MASSIVE_DAMAGE);
	CHECK(ext.ctf_flags & AE_EXT_CTF_NO_DRIVING);
	CHECK(!(ext.ctf_flags & AE_EXT_CTF_SLOW_CARRIER));
	CHECK(ext.ctf_own_flag_waypoint == AE_EXT_WAYPOINT_SPOTTED);
	CHECK(ae_variant_ext_sanitize(&ext));

	/* a NEW MODES preset keeps its own flag rules (CTF CLASSIC's touch return) and takes the grenades */
	ae_variant_ext_default(&ext);
	ext.ctf_flags = AE_EXT_CTF_TOUCH_RETURN;
	ae_seed_ext_apply(AE_SEED_EXT_CASUAL | AE_SEED_EXT_CTF | AE_SEED_EXT_NEW_MODE, &ext);
	CHECK(ext.shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON);
	CHECK(ext.ctf_flags == AE_EXT_CTF_TOUCH_RETURN && ext.ctf_return == AE_EXT_CTF_RETURN_CE &&
		ext.ctf_own_flag_waypoint == AE_EXT_WAYPOINT_GAMETYPE);

	/* the table's own bytes of a merge stay (AE KING's uncontested hill) */
	ae_variant_ext_default(&ext);
	ext.king_flags = AE_EXT_KING_UNCONTESTED;
	ae_seed_ext_apply(AE_SEED_EXT_CASUAL, &ext);
	CHECK(ext.king_flags == AE_EXT_KING_UNCONTESTED && ext.shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON);

	/* a comp or classic game (no casual flag), even a capture the flag one: untouched */
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_CTF, &ext);
	CHECK(!memcmp(&ext, &none, sizeof(ext)));
	ae_seed_ext_apply(0, &ext);
	CHECK(!memcmp(&ext, &none, sizeof(ext)));

	/* FALL STUN is off in every AE gametype (casual, comp, drills, training, NEW MODES), casual or not */
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_AE_RULES | AE_SEED_EXT_CASUAL, &ext);
	CHECK(ext.fall_stun == AE_EXT_FALL_STUN_OFF && ext.shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON);
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_AE_RULES, &ext);
	CHECK(ext.fall_stun == AE_EXT_FALL_STUN_OFF && ext.shootable_grenades == 0 && ext.split_join == 0);
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_AE_RULES | AE_SEED_EXT_CTF, &ext);
	CHECK(ext.fall_stun == AE_EXT_FALL_STUN_OFF && ext.ctf_return == AE_EXT_CTF_RETURN_CE);
	/* a gametype with no AE rules (NHE's, CLASSIC COMP, a player's own) keeps FALL STUN on */
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_CASUAL, &ext);
	CHECK(ext.fall_stun == AE_EXT_FALL_STUN_ON);
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(0, &ext);
	CHECK(ext.fall_stun == AE_EXT_FALL_STUN_ON);

	printf(failures ? "FAILED\n" : "PASS\n");
	return failures != 0;
}
