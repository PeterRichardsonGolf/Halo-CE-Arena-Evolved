/*
AE_JUMP_RULES.H

The gametype's JUMP (game_variant_options.jump_percent) without the engine: its values and how a player's jump
multiplier comes from it and a map layout's jump line (ae_jump.c; unit test ae_jump_rules_test.c).
*/

#ifndef __AE_JUMP_RULES_H
#define __AE_JUMP_RULES_H

/* the stored byte: 0 Halo CE's jump (old gametypes, every builder's zero), else the percent itself, 50 to 200 */
#define AE_JUMP_STOCK 0
#define AE_JUMP_MINIMUM 50
#define AE_JUMP_MAXIMUM 200

/* the percent a stored byte plays: 100 for 0 or anything out of range */
static inline int ae_jump_percent_of(unsigned char stored)
{
	return stored >= AE_JUMP_MINIMUM && stored <= AE_JUMP_MAXIMUM ? stored : 100;
}

/* a player's jump percent: a map layout's jump line (layout_percent, 0 when none) replaces the gametype's, as the map
is tuned to it; the gametype's only for a player whose machine has it (has_it: a machine of this game; the host's
own players and a guest's own always), Halo CE's for the rest, who would move otherwise on their own machine */
static inline int ae_jump_player_percent(int layout_percent, unsigned char stored, int has_it)
{
	if (layout_percent > 0)
		return layout_percent;
	return has_it ? ae_jump_percent_of(stored) : 100;
}

/* whether another machine's build (its Delta HELLO's build name, HALO_PRODUCT_SHORT_NAME " " its version: "Arena
Evolved 0.7.3-beta", "... -dev") plays the gametype's JUMP: Arena Evolved 0.7.3 or later. 0.5.0 to 0.7.2 are AE too
(their join requests carry AE's mark) but zero the byte as reserved and jump as Halo CE, so a host bounding their
moves by the gametype's jump rejected their own (Codex review 2026-11-01) */
static inline int ae_jump_build_has_it(const char *build)
{
	static const char prefix[] = "Arena Evolved ";
	int part[3] = { 0, 0, 0 }, at = 0, digits = 0;
	const char *c;

	if (!build)
		return 0;
	for (c = prefix; *c; c++, build++)
		if (*build != *c)
			return 0;
	for (; *build && at < 3; build++)
	{
		if (*build >= '0' && *build <= '9')
		{
			part[at] = part[at] * 10 + (*build - '0');
			digits++;
			if (part[at] > 9999)
				return 0;
		}
		else if (*build == '.' && digits)
		{
			at++;
			digits = 0;
		}
		else
			break;
	}
	if (!digits && at < 3)
		return 0;
	if (part[0] != 0)
		return part[0] > 0;
	if (part[1] != 7)
		return part[1] > 7;
	return part[2] >= 3;
}

#endif // __AE_JUMP_RULES_H
