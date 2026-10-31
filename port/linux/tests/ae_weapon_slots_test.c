#include <stdio.h>
#include "ae_weapon_slots.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define BR AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_BATTLE_RIFLE)
#define SMG AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_SMG)
#define SILENCED AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_SMG_SILENCED)
#define CARBINE AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_CARBINE)
#define AR_SILENCED AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_AR_SILENCED)
#define DMR AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_DMR)
#define EVO AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_EVO_RIFLE)

int main(void)
{
	unsigned int byte;

	/* the byte: 1 is the BR alone, as before the mask; 128 and over none; unknown slots' bits dropped */
	CHECK(ae_weapon_slots_mask(0) == 0);
	CHECK(ae_weapon_slots_mask(1) == BR);
	CHECK(ae_weapon_slots_mask(3) == (BR | SMG));
	CHECK(ae_weapon_slots_mask(0x0F) == (BR | SMG | SILENCED | CARBINE));
	CHECK(ae_weapon_slots_mask(0x10) == AR_SILENCED && ae_weapon_slots_mask(0x1F) == (BR | SMG | SILENCED | CARBINE |
		AR_SILENCED));
	/* (0.7.0: bits 6 and 7 the DMR and the EVO RIFLE: every bit of the byte a slot) */
	CHECK(ae_weapon_slots_mask(0x20) == DMR && ae_weapon_slots_mask(0x40) == EVO && ae_weapon_slots_mask(0x7F) == 0x7F);
	for (byte = 128; byte < 256; byte++)
		CHECK(ae_weapon_slots_mask((unsigned char)byte) == 0);

	/* the loadout's ranks: 1 the lowest named slot (the BR for the BR's byte, as before), 2 the second, ... 5 */
	CHECK(ae_weapon_slots_loadout(BR, 1) == AE_WEAPON_SLOT_BATTLE_RIFLE);
	CHECK(ae_weapon_slots_loadout(BR, 2) == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_loadout(BR, 0) == AE_WEAPON_SLOT_NONE && ae_weapon_slots_loadout(BR, 3) == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_loadout(BR | SMG | CARBINE, 3) == AE_WEAPON_SLOT_CARBINE);
	CHECK(ae_weapon_slots_loadout(0x1F, 5) == AE_WEAPON_SLOT_AR_SILENCED && ae_weapon_slots_loadout(0x1F, 6) == 0);
	CHECK(ae_weapon_slots_loadout(BR | SMG, 1) == AE_WEAPON_SLOT_BATTLE_RIFLE);
	CHECK(ae_weapon_slots_loadout(BR | SMG, 2) == AE_WEAPON_SLOT_SMG);
	CHECK(ae_weapon_slots_loadout(SMG | CARBINE, 1) == AE_WEAPON_SLOT_SMG);
	CHECK(ae_weapon_slots_loadout(SMG | CARBINE, 2) == AE_WEAPON_SLOT_CARBINE);
	CHECK(ae_weapon_slots_loadout(0, 1) == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_first(SILENCED | CARBINE) == AE_WEAPON_SLOT_SMG_SILENCED && ae_weapon_slots_first(0) == 0);

	/* a slot's rank among the named ones, the third and later too */
	CHECK(ae_weapon_slots_source(BR | SMG, AE_WEAPON_SLOT_BATTLE_RIFLE) == 1);
	CHECK(ae_weapon_slots_source(BR | SMG, AE_WEAPON_SLOT_SMG) == 2);
	CHECK(ae_weapon_slots_source(BR | SMG | CARBINE, AE_WEAPON_SLOT_CARBINE) == 3);
	CHECK(ae_weapon_slots_source(0x1F, AE_WEAPON_SLOT_AR_SILENCED) == 5);
	/* (seven packs named: the 6th and 7th have their ranks, and a loadout slot holds them too, 0.7.0: every pack can
	start) */
	CHECK(ae_weapon_slots_loadout(0x7F, 6) == AE_WEAPON_SLOT_DMR && ae_weapon_slots_loadout(0x7F, 7) == AE_WEAPON_SLOT_EVO_RIFLE);
	CHECK(ae_weapon_slots_loadout(EVO, 1) == AE_WEAPON_SLOT_EVO_RIFLE && ae_weapon_slots_source(BR | EVO, AE_WEAPON_SLOT_EVO_RIFLE) == 2);
	{
		unsigned char loadout = 0;
		unsigned long flags = 0;

		unsigned char extra = 0x31;

		/* (one slot alone at 6 or 7: the extra bit tells; ball_melee's other bits kept) */
		ae_weapon_slots_rank_set(&loadout, &flags, &extra, 0, 6);
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 6 && (extra & ~AE_WEAPON_SLOTS_EXTRA_BIT) == 0x31);
		ae_weapon_slots_rank_set(&loadout, &flags, &extra, 0, 7);
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 7 && (extra & AE_WEAPON_SLOTS_EXTRA_BIT));
		/* (both: PRIMARY 7, SECONDARY 6, then the other way round) */
		ae_weapon_slots_rank_set(&loadout, &flags, &extra, 1, 6);
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 7 && ae_weapon_slots_rank(loadout, flags, extra, 1) == 6);
		ae_weapon_slots_rank_set(&loadout, &flags, &extra, 0, 6);
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 6 && ae_weapon_slots_rank(loadout, flags, extra, 1) == 0);
		ae_weapon_slots_rank_set(&loadout, &flags, &extra, 1, 7);
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 6 && ae_weapon_slots_rank(loadout, flags, extra, 1) == 7);
		/* (a lower rank on one slot leaves the other's 6-7 as it was) */
		ae_weapon_slots_rank_set(&loadout, &flags, &extra, 0, 2);
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 2 && ae_weapon_slots_rank(loadout, flags, extra, 1) == 7);
		/* (an older build's bits: a later field of 3 and no extra bit reads as the 6th) */
		CHECK(ae_weapon_slots_rank(3, 3ul << 12, 0, 0) == 6);
		CHECK(ae_weapon_slots_rank(0, 0, 0, 0) == 0);
	}
	CHECK(ae_weapon_slots_source(BR, AE_WEAPON_SLOT_SMG) == 0 && ae_weapon_slots_source(BR, 0) == 0);

	/* the rank's bits: 1 and 2 in pack_loadout alone (as 0.3.6-0.3.8 wrote them), 3-5 source 3 and the flags' two
	bits a loadout slot from bit 12; other bits kept */
	{
		unsigned char loadout = 0xF0;
		unsigned long flags = 0x80010FFFul;
		unsigned char extra = 0;
		unsigned int rank;

		for (rank = 0; rank <= 5; rank++)
		{
			ae_weapon_slots_rank_set(&loadout, &flags, &extra, 0, rank);
			ae_weapon_slots_rank_set(&loadout, &flags, &extra, 1, 5 - rank);
			CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == rank && ae_weapon_slots_rank(loadout, flags, extra, 1) == 5 - rank);
			CHECK((loadout & 0xF0) == 0xF0 && (flags & ~AE_WEAPON_SLOTS_LATER_MASK) == 0x80010FFFul);
		}
		loadout = 0x01 | (2 << 2);
		flags = 0;
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 1 && ae_weapon_slots_rank(loadout, flags, extra, 1) == 2);
		ae_weapon_slots_rank_set(&loadout, &flags, &extra, 0, 4);
		CHECK((loadout & 3) == 3 && flags == (1ul << 12) && ae_weapon_slots_rank(loadout, flags, extra, 0) == 4);
	}

	/* remapping keeps each loadout slot's pack */
	{
		unsigned char loadout;
		unsigned long flags;
		unsigned char extra = 0;

		/* primary the SMG (rank 1 of SMG|CARBINE), secondary the carbine (2); the BR joins: SMG 2, carbine 3 */
		loadout = 0x20 | 0x01 | (2 << 2);
		flags = 0;
		ae_weapon_slots_loadout_remap(SMG | CARBINE, BR | SMG | CARBINE, &loadout, &flags, &extra);
		CHECK(loadout == (0x20 | 2 | (3 << 2)) && flags == 0 && ae_weapon_slots_rank(loadout, flags, extra, 1) == 3);
		/* the SMG leaves: the carbine, was the second, is the first; the primary goes back to stock */
		loadout = 0x80 | 0x01 | (2 << 2);
		flags = 0;
		ae_weapon_slots_loadout_remap(SMG | CARBINE, CARBINE, &loadout, &flags, &extra);
		CHECK(loadout == (0x80 | (1 << 2)) && flags == 0);
		/* the silenced AR fifth of five; the BR leaves: fourth */
		loadout = 3;
		flags = 2ul << 12;
		ae_weapon_slots_loadout_remap(0x1F, 0x1E, &loadout, &flags, &extra);
		CHECK(ae_weapon_slots_rank(loadout, flags, extra, 0) == 4 && ae_weapon_slots_loadout(0x1E, 4) == AE_WEAPON_SLOT_AR_SILENCED);
		loadout = 0x55;
		flags = 0;
		ae_weapon_slots_loadout_remap(0, BR, &loadout, &flags, &extra);
		CHECK(loadout == 0x50);
	}

	/* the capability: the highest level whose slots are all loaded; the BR's 2 as before */
	CHECK(ae_weapon_slots_capability(0) == 0);
	CHECK(ae_weapon_slots_capability(BR) == 2);
	CHECK(ae_weapon_slots_capability(BR | SMG) == 2);
	CHECK(ae_weapon_slots_capability(BR | SMG | SILENCED) == 3);
	CHECK(ae_weapon_slots_capability(SMG | SILENCED) == 0);
	CHECK(ae_weapon_slots_capability(BR | SMG | SILENCED | CARBINE) == 4);
	CHECK(ae_weapon_slots_capability(BR | CARBINE) == 2);
	/* (the silenced assault rifle's slot 5 is level 5: a machine of before, with the four, tells 4 and is refused only
	by a game that names it) */
	CHECK(ae_weapon_slots_capability(BR | SMG | SILENCED | CARBINE | AR_SILENCED) == 5);
	CHECK(ae_weapon_slots_capability(BR | SMG | SILENCED | AR_SILENCED) == 3);
	CHECK(ae_weapon_slots_required(AR_SILENCED) == 5 && ae_weapon_slots_required(SMG | AR_SILENCED) == 5);
	/* (0.7.0) the DMR 6, the EVO RIFLE 7: a machine with every pack tells 7; a 0.6.0 one with the five tells 5 */
	CHECK(ae_weapon_slots_capability(0x7F) == 7 && ae_weapon_slots_capability(0x3F) == 6);
	CHECK(ae_weapon_slots_capability(0x1F | EVO) == 5);
	CHECK(ae_weapon_slots_required(DMR) == 6 && ae_weapon_slots_required(EVO) == 7 && ae_weapon_slots_required(BR | EVO) == 7);
	/* what a game needs: the highest of its slots' levels; a host's check (capability < required) */
	CHECK(ae_weapon_slots_required(0) == 0);
	CHECK(ae_weapon_slots_required(BR) == 2);
	CHECK(ae_weapon_slots_required(BR | SMG) == 3 && ae_weapon_slots_required(SILENCED) == 3);
	CHECK(ae_weapon_slots_required(BR | CARBINE) == 4);
	/* (every mask: a machine with exactly those packs passes a game that names them, and a game naming more than
	a machine's level refuses it) */
	for (byte = 0; byte < 128; byte++)
	{
		unsigned int game;

		for (game = 0; game < 128; game++)
		{
			int pass = ae_weapon_slots_capability(byte) >= ae_weapon_slots_required(game);

			/* (passing means the machine has every slot the game names) */
			if (pass)
				CHECK((game & ~byte) == 0);
		}
	}

	/* map spawns: the pistol the BR, the assault rifle the SMG, only where on the map */
	CHECK(ae_weapon_slots_spawn(AE_WEAPON_STOCK_PISTOL, BR | SMG) == AE_WEAPON_SLOT_BATTLE_RIFLE);
	CHECK(ae_weapon_slots_spawn(AE_WEAPON_STOCK_ASSAULT_RIFLE, BR | SMG) == AE_WEAPON_SLOT_SMG);
	CHECK(ae_weapon_slots_spawn(AE_WEAPON_STOCK_ASSAULT_RIFLE, BR) == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_spawn(AE_WEAPON_STOCK_PISTOL, SMG | SILENCED) == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_spawn(AE_WEAPON_STOCK_NONE, BR | SMG) == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_spawn(7, 0x0F) == AE_WEAPON_SLOT_NONE);
	/* (the silenced assault rifle never takes the map's spawns: MAP SPAWNS are the SMG's) */
	CHECK(ae_weapon_slots_spawn(AE_WEAPON_STOCK_ASSAULT_RIFLE, AR_SILENCED) == AE_WEAPON_SLOT_NONE);

	/* layout aliases and their fallbacks */
	CHECK(ae_weapon_slots_alias("smg") == AE_WEAPON_SLOT_SMG);
	CHECK(ae_weapon_slots_alias("SMG") == AE_WEAPON_SLOT_SMG);
	CHECK(ae_weapon_slots_alias("br") == AE_WEAPON_SLOT_BATTLE_RIFLE);
	CHECK(ae_weapon_slots_alias("Battle_Rifle") == AE_WEAPON_SLOT_BATTLE_RIFLE);
	CHECK(ae_weapon_slots_alias("silenced_smg") == AE_WEAPON_SLOT_SMG_SILENCED);
	CHECK(ae_weapon_slots_alias("smg_silenced") == AE_WEAPON_SLOT_SMG_SILENCED);
	CHECK(ae_weapon_slots_alias("carbine") == AE_WEAPON_SLOT_CARBINE);
	CHECK(ae_weapon_slots_alias("ar_silenced") == AE_WEAPON_SLOT_AR_SILENCED);
	CHECK(ae_weapon_slots_alias("Silenced_AR") == AE_WEAPON_SLOT_AR_SILENCED);
	CHECK(ae_weapon_slots_alias("sm") == AE_WEAPON_SLOT_NONE && ae_weapon_slots_alias("smgs") == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_alias("") == AE_WEAPON_SLOT_NONE && ae_weapon_slots_alias(NULL) == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_alias("pistol") == AE_WEAPON_SLOT_NONE);
	CHECK(ae_weapon_slots_alias("dmr") == AE_WEAPON_SLOT_DMR && ae_weapon_slots_alias("DMR") == AE_WEAPON_SLOT_DMR);
	CHECK(ae_weapon_slots_alias("evo_rifle") == AE_WEAPON_SLOT_EVO_RIFLE && ae_weapon_slots_alias("evo") == AE_WEAPON_SLOT_EVO_RIFLE);
	CHECK(ae_weapon_slots_fallback(AE_WEAPON_SLOT_DMR) == AE_WEAPON_STOCK_PISTOL &&
		ae_weapon_slots_fallback(AE_WEAPON_SLOT_EVO_RIFLE) == AE_WEAPON_STOCK_PISTOL);
	CHECK(ae_weapon_slots_display(AE_WEAPON_SLOT_EVO_RIFLE)[0] == 'E' && ae_weapon_slots_name(AE_WEAPON_SLOT_DMR)[0] == 'd');
	CHECK(ae_weapon_slots_fallback(AE_WEAPON_SLOT_BATTLE_RIFLE) == AE_WEAPON_STOCK_PISTOL);
	CHECK(ae_weapon_slots_fallback(AE_WEAPON_SLOT_SMG) == AE_WEAPON_STOCK_ASSAULT_RIFLE);
	CHECK(ae_weapon_slots_fallback(AE_WEAPON_SLOT_SMG_SILENCED) == AE_WEAPON_STOCK_ASSAULT_RIFLE);
	CHECK(ae_weapon_slots_fallback(AE_WEAPON_SLOT_CARBINE) == AE_WEAPON_STOCK_PISTOL);
	CHECK(ae_weapon_slots_fallback(AE_WEAPON_SLOT_AR_SILENCED) == AE_WEAPON_STOCK_ASSAULT_RIFLE);
	CHECK(ae_weapon_slots_display(AE_WEAPON_SLOT_AR_SILENCED)[0] == 'S' &&
		ae_weapon_slots_name(AE_WEAPON_SLOT_AR_SILENCED)[0] == 'a');
	CHECK(ae_weapon_slots_fallback(0) == AE_WEAPON_STOCK_NONE && ae_weapon_slots_fallback(9) == AE_WEAPON_STOCK_NONE);
	CHECK(ae_weapon_slots_name(AE_WEAPON_SLOT_SMG)[0] == 's' && ae_weapon_slots_name(-1)[0] == '?');

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_weapon_slots_test: all passed\n");
	return failures != 0;
}
