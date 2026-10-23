#include <stdio.h>
#include "ae_weapon_slots.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define BR AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_BATTLE_RIFLE)
#define SMG AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_SMG)
#define SILENCED AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_SMG_SILENCED)
#define CARBINE AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_CARBINE)
#define AR_SILENCED AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_AR_SILENCED)

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
	CHECK(ae_weapon_slots_mask(0x20) == 0 && ae_weapon_slots_mask(0x7F) == 0x1F);
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
	CHECK(ae_weapon_slots_source(BR, AE_WEAPON_SLOT_SMG) == 0 && ae_weapon_slots_source(BR, 0) == 0);

	/* the rank's bits: 1 and 2 in pack_loadout alone (as 0.3.6-0.3.8 wrote them), 3-5 source 3 and the flags' two
	bits a loadout slot from bit 12; other bits kept */
	{
		unsigned char loadout = 0xF0;
		unsigned long flags = 0x80010FFFul;
		unsigned int rank;

		for (rank = 0; rank <= 5; rank++)
		{
			ae_weapon_slots_rank_set(&loadout, &flags, 0, rank);
			ae_weapon_slots_rank_set(&loadout, &flags, 1, 5 - rank);
			CHECK(ae_weapon_slots_rank(loadout, flags, 0) == rank && ae_weapon_slots_rank(loadout, flags, 1) == 5 - rank);
			CHECK((loadout & 0xF0) == 0xF0 && (flags & ~AE_WEAPON_SLOTS_LATER_MASK) == 0x80010FFFul);
		}
		loadout = 0x01 | (2 << 2);
		flags = 0;
		CHECK(ae_weapon_slots_rank(loadout, flags, 0) == 1 && ae_weapon_slots_rank(loadout, flags, 1) == 2);
		ae_weapon_slots_rank_set(&loadout, &flags, 0, 4);
		CHECK((loadout & 3) == 3 && flags == (1ul << 12) && ae_weapon_slots_rank(loadout, flags, 0) == 4);
		/* (a later field of 3 is no rank: stock) */
		CHECK(ae_weapon_slots_rank(3, 3ul << 12, 0) == 0);
	}

	/* remapping keeps each loadout slot's pack */
	{
		unsigned char loadout;
		unsigned long flags;

		/* primary the SMG (rank 1 of SMG|CARBINE), secondary the carbine (2); the BR joins: SMG 2, carbine 3 */
		loadout = 0x20 | 0x01 | (2 << 2);
		flags = 0;
		ae_weapon_slots_loadout_remap(SMG | CARBINE, BR | SMG | CARBINE, &loadout, &flags);
		CHECK(loadout == (0x20 | 2 | (3 << 2)) && flags == 0 && ae_weapon_slots_rank(loadout, flags, 1) == 3);
		/* the SMG leaves: the carbine, was the second, is the first; the primary goes back to stock */
		loadout = 0x80 | 0x01 | (2 << 2);
		flags = 0;
		ae_weapon_slots_loadout_remap(SMG | CARBINE, CARBINE, &loadout, &flags);
		CHECK(loadout == (0x80 | (1 << 2)) && flags == 0);
		/* the silenced AR fifth of five; the BR leaves: fourth */
		loadout = 3;
		flags = 2ul << 12;
		ae_weapon_slots_loadout_remap(0x1F, 0x1E, &loadout, &flags);
		CHECK(ae_weapon_slots_rank(loadout, flags, 0) == 4 && ae_weapon_slots_loadout(0x1E, 4) == AE_WEAPON_SLOT_AR_SILENCED);
		loadout = 0x55;
		flags = 0;
		ae_weapon_slots_loadout_remap(0, BR, &loadout, &flags);
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
	/* what a game needs: the highest of its slots' levels; a host's check (capability < required) */
	CHECK(ae_weapon_slots_required(0) == 0);
	CHECK(ae_weapon_slots_required(BR) == 2);
	CHECK(ae_weapon_slots_required(BR | SMG) == 3 && ae_weapon_slots_required(SILENCED) == 3);
	CHECK(ae_weapon_slots_required(BR | CARBINE) == 4);
	/* (every mask: a machine with exactly those packs passes a game that names them, and a game naming more than
	a machine's level refuses it) */
	for (byte = 0; byte < 32; byte++)
	{
		unsigned int game;

		for (game = 0; game < 32; game++)
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
