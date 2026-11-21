#include <stdio.h>
#include <string.h>

#include "ae_bots_style_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void preset_tests(void)
{
	struct ae_bots_style style;

	CHECK(ae_bots_style_preset(AE_STYLE_PUSHER, AE_STYLE_WEAPON_SHOTGUN, &style) && style.aggression == 2 &&
		style.jumpiness == 1 && style.patience == -1 && style.weapon == AE_STYLE_WEAPON_SHOTGUN);
	CHECK(ae_bots_style_preset(AE_STYLE_ANCHOR, 0, &style) && style.aggression == -2 && style.patience == 2);
	CHECK(!ae_bots_style_preset(AE_STYLE_PRESETS, 0, &style) && !ae_bots_style_preset(0, AE_STYLE_WEAPONS, &style) &&
		!ae_bots_style_preset(-1, 0, &style));
	CHECK(ae_bots_style_preset_of_name("pusher") == AE_STYLE_PUSHER && ae_bots_style_preset_of_name("Careful") == AE_STYLE_CAREFUL);
	CHECK(ae_bots_style_preset_of_name("camper") == -1 && ae_bots_style_preset_of_name(NULL) == -1);
	CHECK(ae_bots_style_weapon_of_name("battle_rifle") == AE_STYLE_WEAPON_BATTLE_RIFLE &&
		ae_bots_style_weapon_of_name("NONE") == AE_STYLE_WEAPON_NONE && ae_bots_style_weapon_of_name("bfg") == -1);
	CHECK(!strcmp(ae_bots_style_preset_name(AE_STYLE_HOPPER), "HOPPER") && !strcmp(ae_bots_style_preset_name(9), "?"));
	CHECK(!strcmp(ae_bots_style_weapon_name(AE_STYLE_WEAPON_ROCKETS), "rockets"));
}

/* a hash's style: every preset and weapon turns up, about as weighted, and the same hash the same style */
static void hash_tests(void)
{
	int counts[AE_STYLE_PRESETS], weapons[AE_STYLE_WEAPONS], index;
	struct ae_bots_style a, b;
	unsigned long hash = 12345;

	memset(counts, 0, sizeof counts);
	memset(weapons, 0, sizeof weapons);
	for (index = 0; index < 10000; index++)
	{
		hash = (hash * 2654435761UL + 0x9E3779B9UL) & 0xFFFFFFFFUL;
		ae_bots_style_from_hash(hash, &a);
		counts[a.preset]++;
		weapons[a.weapon]++;
	}
	CHECK(counts[AE_STYLE_STEADY] > 2500 && counts[AE_STYLE_STEADY] < 3500);
	CHECK(counts[AE_STYLE_HOPPER] > 700 && counts[AE_STYLE_HOPPER] < 1300);
	CHECK(counts[AE_STYLE_PUSHER] > 1600 && counts[AE_STYLE_ANCHOR] > 1600 && counts[AE_STYLE_CAREFUL] > 1600);
	CHECK(weapons[AE_STYLE_WEAPON_NONE] > 2800 && weapons[AE_STYLE_WEAPON_NONE] < 3900);
	for (index = 1; index < AE_STYLE_WEAPONS; index++)
		CHECK(weapons[index] > 500);
	ae_bots_style_from_hash(0xDEADBEEFUL, &a);
	ae_bots_style_from_hash(0xDEADBEEFUL, &b);
	CHECK(!memcmp(&a, &b, sizeof a));
}

/* the knobs: mild, and none past its range */
static void knob_tests(void)
{
	struct ae_bots_style pusher, anchor, odd;

	ae_bots_style_preset(AE_STYLE_PUSHER, AE_STYLE_WEAPON_SHOTGUN, &pusher);
	ae_bots_style_preset(AE_STYLE_ANCHOR, AE_STYLE_WEAPON_NONE, &anchor);
	CHECK(ae_bots_style_confidence(&pusher, 0.5f) > 0.61f && ae_bots_style_confidence(&pusher, 0.5f) < 0.63f);
	CHECK(ae_bots_style_confidence(&anchor, 0.5f) < 0.39f && ae_bots_style_confidence(NULL, 0.5f) == 0.5f);
	/* (jump delay: PUSHER +1 step 0.85, ANCHOR -1 step 1.15; 0 stays 0) */
	CHECK(ae_bots_style_jump_delay(&pusher, 100) == 85 && ae_bots_style_jump_delay(&anchor, 100) == 115 &&
		ae_bots_style_jump_delay(&pusher, 0) == 0 && ae_bots_style_jump_delay(NULL, 100) == 100);
	/* (halts: PUSHER -1 step 0.75, ANCHOR +2 steps 1.5) */
	CHECK(ae_bots_style_halt(&pusher, 40) == 30 && ae_bots_style_halt(&anchor, 40) == 60 && ae_bots_style_halt(&anchor, 0) == 0);
	/* (a step past the range counts as the range's end) */
	odd = pusher;
	odd.jumpiness = 9;
	CHECK(ae_bots_style_jump_delay(&odd, 100) == 70);
	/* (the favourite: + 2 for its own class; flags and the like are never wanted more) */
	CHECK(ae_bots_style_weapon_value(&pusher, "weapons\\shotgun\\shotgun", 8) == 10);
	CHECK(ae_bots_style_weapon_value(&pusher, "weapons\\sniper rifle\\sniper rifle", 8) == 8);
	CHECK(ae_bots_style_weapon_value(&anchor, "weapons\\shotgun\\shotgun", 8) == 8);
	CHECK(ae_bots_style_weapon_value(&pusher, "weapons\\flag\\flag", -1) == -1);
	CHECK(ae_bots_style_weapon_of_tag("weapons\\plasma pistol\\plasma pistol") == AE_STYLE_WEAPON_NONE);
	CHECK(ae_bots_style_weapon_of_tag("weapons\\pistol\\pistol") == AE_STYLE_WEAPON_PISTOL);
	CHECK(ae_bots_style_weapon_of_tag("weapons\\plasma rifle\\plasma rifle") == AE_STYLE_WEAPON_PLASMA_RIFLE);
	CHECK(ae_bots_style_weapon_of_tag("weapons\\assault rifle\\assault rifle") == AE_STYLE_WEAPON_ASSAULT_RIFLE);
	CHECK(ae_bots_style_weapon_of_tag("ae\\weapons\\battle_rifle\\battle_rifle") == AE_STYLE_WEAPON_BATTLE_RIFLE);
	CHECK(ae_bots_style_weapon_of_tag("weapons\\rocket launcher\\rocket launcher") == AE_STYLE_WEAPON_ROCKETS);
	CHECK(ae_bots_style_weapon_of_tag(NULL) == AE_STYLE_WEAPON_NONE);
}

int main(void)
{
	preset_tests();
	hash_tests();
	knob_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_bots_style_rules: ok\n");
	return failures != 0;
}
