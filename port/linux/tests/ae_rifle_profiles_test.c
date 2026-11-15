#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_rifle_profiles.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 0.0005)

int main(void)
{
	struct ae_rifle_profile const *br = ae_rifle_profile(AE_RIFLE_BATTLE_RIFLE);
	struct ae_rifle_profile const *evo = ae_rifle_profile(AE_RIFLE_EVO_RIFLE);
	struct ae_rifle_profile const *dmr = ae_rifle_profile(AE_RIFLE_DMR);
	struct ae_rifle_profile const *pdw = ae_rifle_profile(AE_RIFLE_PDW);
	struct ae_rifle_profile const *ravager = ae_rifle_profile(AE_RIFLE_RAVAGER);
	struct ae_rifle_profile const *hj = ae_rifle_profile(AE_RIFLE_HJ_MAGNUM);

	/* the behaviours (ae_pack_format.h): 1 the burst rifle, 2 the EVO RIFLE, 3 the DMR, 4 the burst PDW; the tags' own
	(0) and unknown ones are none */
	CHECK(ae_rifle_kind_of_behaviour(0) == AE_RIFLE_NONE);
	CHECK(ae_rifle_kind_of_behaviour(1) == AE_RIFLE_BATTLE_RIFLE);
	CHECK(ae_rifle_kind_of_behaviour(2) == AE_RIFLE_EVO_RIFLE);
	CHECK(ae_rifle_kind_of_behaviour(3) == AE_RIFLE_DMR);
	CHECK(ae_rifle_kind_of_behaviour(4) == AE_RIFLE_PDW);
	CHECK(ae_rifle_kind_of_behaviour(5) == AE_RIFLE_RAVAGER);
	CHECK(ae_rifle_kind_of_behaviour(6) == AE_RIFLE_NONE);
	CHECK(ae_rifle_profile(AE_RIFLE_NONE) == NULL && ae_rifle_profile(AE_RIFLE_KINDS) == NULL);
	/* a weapon by its behaviour and its key: the behaviour's kind whatever the key; the tags' own behaviour (0) the HJ
	MAGNUM by its key alone (the registry's magnum_scoped, the cache's id 10), not its alias, a near key or none */
	CHECK(ae_rifle_kind_of_weapon(0, "magnum_scoped") == AE_RIFLE_HJ_MAGNUM);
	CHECK(ae_rifle_kind_of_weapon(0, "scoped_magnum") == AE_RIFLE_NONE);
	CHECK(ae_rifle_kind_of_weapon(0, "magnum_scoped2") == AE_RIFLE_NONE);
	CHECK(ae_rifle_kind_of_weapon(0, "magnum") == AE_RIFLE_NONE);
	CHECK(ae_rifle_kind_of_weapon(0, "") == AE_RIFLE_NONE && ae_rifle_kind_of_weapon(0, NULL) == AE_RIFLE_NONE);
	CHECK(ae_rifle_kind_of_weapon(0, "evo_rifle") == AE_RIFLE_NONE);
	CHECK(ae_rifle_kind_of_weapon(2, "evo_rifle") == AE_RIFLE_EVO_RIFLE);
	CHECK(ae_rifle_kind_of_weapon(1, NULL) == AE_RIFLE_BATTLE_RIFLE);
	CHECK(ae_rifle_kind_of_weapon(3, "magnum_scoped") == AE_RIFLE_DMR);
	CHECK(ae_rifle_kind_of_weapon(6, "magnum_scoped") == AE_RIFLE_NONE);
	CHECK(br && evo && dmr && pdw && ravager && hj);
	if (!br || !evo || !dmr || !pdw || !ravager || !hj)
		return 1;
	/* the three rifles, their rounds 2 ticks apart, their damage the same near and far, x1.06 on the shield and x1.5
	on the body; the PDW no rifle */
	CHECK(br->rifle && evo->rifle && dmr->rifle && !pdw->rifle);
	CHECK(br->round_ticks == 2 && evo->round_ticks == 2 && dmr->round_ticks == 2);
	CHECK(NEAR(br->round_damage_slowed, br->round_damage) && NEAR(evo->round_damage_slowed, evo->round_damage) &&
		NEAR(dmr->round_damage_slowed, dmr->round_damage));
	CHECK(NEAR(br->shield_modifier, 1.06f) && NEAR(br->body_modifier, 1.5f) && NEAR(evo->shield_modifier, 1.06f) &&
		NEAR(dmr->body_modifier, 1.5f));
	CHECK(NEAR(br->bullet_final_speed, br->bullet_speed) && NEAR(evo->bullet_final_speed, evo->bullet_speed) &&
		NEAR(dmr->bullet_final_speed, dmr->bullet_speed));
	/* pulls at the quickest: the BATTLE RIFLE's 14 ticks apart, the EVO RIFLE's 11, the DMR's 10 */
	CHECK(ae_rifle_pull_spacing_ticks(br) == 14 && ae_rifle_pull_spacing_ticks(evo) == 11 &&
		ae_rifle_pull_spacing_ticks(dmr) == 10 && ae_rifle_pull_spacing_ticks(NULL) == 0);

	/* the casual BATTLE RIFLE: AE's 3x (approved in play 10-30) */
	CHECK(br->rounds_a_pull == 3 && !br->keeps_pull && br->recovery_ticks == 8);
	CHECK(NEAR(br->round_damage, 6.5f));
	CHECK(br->rounds_loaded == 36 && br->rounds_total_initial == 144 && br->rounds_total_maximum == 216);
	CHECK(NEAR(br->autoaim_degrees, 2.2f) && NEAR(br->autoaim_distance, 18.0f));
	CHECK(NEAR(br->magnetism_degrees, 5.75f) && NEAR(br->magnetism_distance, 30.0f));
	CHECK(NEAR(br->error_lower_degrees, 0.0f) && NEAR(br->error_upper_degrees, 0.0f) && NEAR(br->zoom_seen, 3.0f));
	CHECK(NEAR(br->bullet_speed, 400.0f) && NEAR(br->bullet_range, 40.0f));
	/* bursts 14 ticks apart, the 4th burst's last round 46 ticks after the first: 1.53 s */
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_BATTLE_RIFLE) == 46);

	/* the competitive EVO RIFLE: 17.875 a round (x1.06 = 18.9475 on the shield: 4 break CE's 75 with 0.79 over, the pop
	shows on the 4th), no spill: the 4th carries nothing so a head hit does not kill, the 5th to the head kills; the
	body alone: after the break 17.875 x1.5 = 26.8 a round, 3 rounds kill 75 (the 7th round) */
	CHECK(evo->rounds_a_pull == 1 && evo->keeps_pull && evo->recovery_ticks == 9);
	CHECK(NEAR(evo->round_damage, 6.5f * 11.0f / 4.0f) && NEAR(evo->round_damage, 17.875f));
	CHECK(evo->round_damage * evo->shield_modifier * 4.0f > 75.0f &&
		evo->round_damage * evo->shield_modifier * 3.0f < 75.0f);
	CHECK(evo->round_damage * evo->body_modifier * 3.0f > 75.0f && evo->round_damage * evo->body_modifier * 2.0f < 75.0f);
	CHECK(evo->no_spill && !dmr->no_spill && !ae_rifle_profile(AE_RIFLE_BATTLE_RIFLE)->no_spill);
	CHECK(evo->rounds_loaded == 15 && evo->rounds_total_initial == 60 && evo->rounds_total_maximum == 75);
	CHECK(evo->rounds_between_tracers == 0);
	CHECK(NEAR(evo->autoaim_degrees, 2.0f) && NEAR(evo->autoaim_distance, 18.0f));
	CHECK(NEAR(evo->magnetism_degrees, 5.0f) && NEAR(evo->magnetism_distance, 25.0f));
	CHECK(NEAR(evo->error_lower_degrees, 0.0f) && NEAR(evo->zoom_seen, 2.0f));
	/* a pull every 11 ticks (2.727 a second, the Evo's 2.715): 44 ticks, 1.47 s */
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_EVO_RIFLE) == 44);

	/* the zero-bloom DMR: 3 a second, 17.875 a round (4 break the shield with 0.79 over and it
	spills: the 4th to the head kills), a constant 0.15 degree spread, 3x, the fast bullet */
	CHECK(dmr->rounds_a_pull == 1 && dmr->keeps_pull && dmr->recovery_ticks == 8);
	CHECK(NEAR(dmr->round_damage, 6.5f * 11.0f / 4.0f));
	CHECK(dmr->round_damage * dmr->shield_modifier * 4.0f > 75.0f &&
		dmr->round_damage * dmr->shield_modifier * 3.0f < 75.0f);
	CHECK(dmr->rounds_loaded == 15 && dmr->rounds_total_initial == 45 && dmr->rounds_total_maximum == 60);
	CHECK(NEAR(dmr->autoaim_degrees, 2.25f) && NEAR(dmr->autoaim_distance, 20.0f));
	CHECK(NEAR(dmr->magnetism_degrees, 7.0f) && NEAR(dmr->magnetism_distance, 25.0f));
	CHECK(NEAR(dmr->error_lower_degrees, 0.15f) && NEAR(dmr->error_upper_degrees, 0.15f));
	CHECK(NEAR(dmr->zoom_seen, 3.0f) && NEAR(dmr->bullet_speed, 3000.0f) && NEAR(dmr->bullet_range, 250.0f));
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_DMR) == 30);
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_NONE) == 0);
	/* the perfect medal's pulls: the EVO RIFLE's 5 (no spill), the DMR's 4, a burst's 4 */
	CHECK(ae_rifle_perfect_pulls(AE_RIFLE_EVO_RIFLE) == 5);
	CHECK(ae_rifle_perfect_pulls(AE_RIFLE_DMR) == 4);
	CHECK(ae_rifle_perfect_pulls(AE_RIFLE_BATTLE_RIFLE) == 4);
	CHECK(ae_rifle_perfect_pulls(AE_RIFLE_NONE) == 0);

	/* the burst PDW (0.7.6): 3 rounds a pull a tick apart, the pull lost in the recovery, pulls 16 ticks apart; no zoom,
	no reticle growth, no readouts */
	CHECK(pdw->rounds_a_pull == 3 && pdw->round_ticks == 1 && !pdw->keeps_pull && pdw->recovery_ticks == 12);
	CHECK(ae_rifle_pull_spacing_ticks(pdw) == 16);
	/* (the owner: its perfect kill at mid range "just under the BR", strictly slower than the BATTLE RIFLE's) the
	killing round at mid range, the 11th (the 4th burst's 2nd), 3 pulls and a tick after the first: 49 ticks, past the
	BATTLE RIFLE's 46 (its 12th round, the 4th burst's last); close, the 8th (the 3rd burst's 2nd), 33 ticks */
	CHECK(3 * ae_rifle_pull_spacing_ticks(pdw) + 1 > ae_rifle_perfect_kill_ticks(AE_RIFLE_BATTLE_RIFLE));
	CHECK(3 * ae_rifle_pull_spacing_ticks(pdw) + 1 == 49 && 2 * ae_rifle_pull_spacing_ticks(pdw) + 1 == 33);
	CHECK(NEAR(pdw->zoom_seen, 0.0f) && NEAR(pdw->reticle_growth, 0.0f) && !pdw->scope_readouts);
	CHECK(pdw->rounds_loaded == 36 && pdw->rounds_total_initial == 144 && pdw->rounds_total_maximum == 144);
	CHECK(pdw->rounds_between_tracers == 2);
	CHECK(NEAR(pdw->autoaim_degrees, 4.0f) && NEAR(pdw->autoaim_distance, 14.0f));
	CHECK(NEAR(pdw->magnetism_degrees, 8.0f) && NEAR(pdw->magnetism_distance, 20.0f));
	CHECK(NEAR(pdw->error_lower_degrees, 0.4f) && NEAR(pdw->error_upper_degrees, 1.2f));
	CHECK(NEAR(pdw->bullet_speed, 400.0f) && NEAR(pdw->bullet_final_speed, 240.0f) && NEAR(pdw->bullet_range, 40.0f));
	/* the owner's numbers on a full shield (CE's 75 and 75, the shield's excess carried on), x1.0 on both: up close 20 a
	round, the 8th kills (the 3rd burst's 2nd round, one to spare) and 2 bursts do not; once slowed 14, the 11th (the
	4th burst's 2nd) and 3 bursts do not */
	CHECK(NEAR(pdw->shield_modifier, 1.0f) && NEAR(pdw->body_modifier, 1.0f));
	CHECK(NEAR(pdw->round_damage, 20.0f) && NEAR(pdw->round_damage_slowed, 14.0f));
	CHECK(8.0f * pdw->round_damage >= 150.0f && 7.0f * pdw->round_damage < 150.0f && 6.0f * pdw->round_damage < 150.0f);
	CHECK(11.0f * pdw->round_damage_slowed >= 150.0f && 10.0f * pdw->round_damage_slowed < 150.0f &&
		9.0f * pdw->round_damage_slowed < 150.0f);
	/* the 4th burst's last round 2 ticks after its first, 3 pulls 16 apart: 50 ticks */
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_PDW) == 50);
	/* a pull's span: its 3 rounds (2 ticks) and one more, 100 ms */
	CHECK(ae_rifle_pull_ms(pdw) == 100);

	/* the RAVAGER (0.7.6): no rifle; its rounds the tags' own (its bolts, damage, heat, battery, zoom, aim assist,
	charged shot), so the profile holds only the pacing: 3 bolts a tap 4 ticks apart (7.5 a second), a lost tap in
	the recovery, no bloom, no engine zoom, reticle growth or readouts; every other rifle's rounds are the engine's */
	CHECK(!ravager->rifle && ravager->tags_rounds);
	CHECK(!br->tags_rounds && !evo->tags_rounds && !dmr->tags_rounds && !pdw->tags_rounds);
	CHECK(ravager->rounds_a_pull == 3 && ravager->round_ticks == 4 && !ravager->keeps_pull && ravager->recovery_ticks == 12);
	CHECK(NEAR(30.0f / ravager->round_ticks, 7.5f));
	CHECK(NEAR(ravager->error_lower_degrees, ravager->error_upper_degrees));
	CHECK(NEAR(ravager->zoom_seen, 0.0f) && NEAR(ravager->reticle_growth, 0.0f) && !ravager->scope_readouts);
	/* the tap's first bolt leaves on the trigger's release (the tags' charging trigger), a tick after the press: taps
	23 ticks apart at the quickest (0.77 s), a burst's last bolt 8 ticks after its first and the next tap's first bolt
	15 ticks (0.5 s) after it */
	CHECK(ae_rifle_pull_spacing_ticks(ravager) == 23);
	CHECK(ae_rifle_pull_spacing_ticks(ravager) - (ravager->rounds_a_pull - 1) * ravager->round_ticks == 15);
	/* 7 bolts kill a full player (the tags' 16 a hit and 6.5 of splash on the shield; 4 strip it): the 7th is the
	3rd tap's first, 2 taps after the first bolt: 46 ticks, 1.53 s */
	CHECK(2 * ae_rifle_pull_spacing_ticks(ravager) == 46);
	CHECK((7 + ravager->rounds_a_pull - 1) / ravager->rounds_a_pull == 3);
	/* a tap's span: its 3 bolts (8 ticks) and one more, 300 ms */
	CHECK(ae_rifle_pull_ms(ravager) == 300);

	/* the HJ MAGNUM (0.8.0, the owner 11-09): a pistol, no rifle (no rifle finish, kill feed rifle name or medal role);
	one round a pull, the pull kept in the recovery; the EVO RIFLE's round: 17.875 (x1.06 = 18.9475 on the shield: 3
	leave the shield up, 4 break CE's 75 with 0.79 over), no spill (the 4th carries nothing, the 5th to the head kills),
	the body alone 7 (after the break 26.8 a round, 3 kill 75) */
	CHECK(!hj->rifle && hj->rounds_a_pull == 1 && hj->round_ticks == 2 && hj->keeps_pull);
	CHECK(NEAR(hj->round_damage, 17.875f) && NEAR(hj->round_damage_slowed, hj->round_damage));
	CHECK(NEAR(hj->round_damage, evo->round_damage));
	CHECK(NEAR(hj->shield_modifier, 1.06f) && NEAR(hj->body_modifier, 1.5f));
	CHECK(hj->round_damage * hj->shield_modifier * 3.0f < 75.0f);
	CHECK(hj->round_damage * hj->shield_modifier * 4.0f > 75.0f);
	CHECK(hj->round_damage * hj->shield_modifier * 4.0f - 75.0f < 1.0f);
	CHECK(hj->round_damage * hj->body_modifier * 3.0f > 75.0f && hj->round_damage * hj->body_modifier * 2.0f < 75.0f);
	CHECK(hj->no_spill && hj->tags_handling && !hj->tags_rounds);
	CHECK(!br->tags_handling && !evo->tags_handling && !dmr->tags_handling && !pdw->tags_handling &&
		!ravager->tags_handling);
	/* no spread, no bloom; pulls 8 ticks apart (3.75 a second, 0.267 s: the target's 0.28 s, 3.57, lies between AE's 8
	and 9 ticks; 4 spacings of 8 make 32 ticks, 1.07 s, nearer its 1.12 s than 9's 36, 1.2 s) */
	CHECK(NEAR(hj->error_lower_degrees, 0.0f) && NEAR(hj->error_upper_degrees, 0.0f));
	CHECK(hj->recovery_ticks == 6 && ae_rifle_pull_spacing_ticks(hj) == 8);
	CHECK(NEAR(30.0f / ae_rifle_pull_spacing_ticks(hj), 3.75f));
	CHECK(fabs(4.0 * 8 / 30.0 - 1.12) < fabs(4.0 * 9 / 30.0 - 1.12));
	/* the perfect kill: 5 pulls (4 pop the shield, the 5th to the head kills), the 5th 32 ticks after the first */
	CHECK(ae_rifle_perfect_pulls(AE_RIFLE_HJ_MAGNUM) == 5);
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_HJ_MAGNUM) == 32);
	/* its magazine, zoom, aim assist, tracers and bullet are its tags' (the profile's are 0: none written), no engine
	zoom, reticle growth or readouts */
	CHECK(hj->rounds_loaded == 0 && hj->rounds_total_initial == 0 && hj->rounds_total_maximum == 0);
	CHECK(NEAR(hj->autoaim_degrees, 0.0f) && NEAR(hj->magnetism_degrees, 0.0f) && NEAR(hj->bullet_range, 0.0f));
	CHECK(NEAR(hj->zoom_seen, 0.0f) && NEAR(hj->reticle_growth, 0.0f) && !hj->scope_readouts);
	CHECK(ae_rifle_pull_ms(hj) == 33);

	/* one map holds every pack at once (DEV MODE, the TEST RUN lists): the battle rifle (slot 1), the DMR (6), the EVO
	RIFLE (7), the burst PDW (16) and the RAVAGER (20) all set up, each found by its tag with its own profile; room for
	every kind; a full table refuses the next, a tag of none and a kind of none are refused */
	{
		static struct ae_rifle_weapons weapons;
		static long const tags[] = { 101, 106, 107, 116, 120 };
		static enum ae_rifle_kind const kinds[] = { AE_RIFLE_BATTLE_RIFLE, AE_RIFLE_DMR, AE_RIFLE_EVO_RIFLE, AE_RIFLE_PDW,
			AE_RIFLE_RAVAGER };
		short index;

		CHECK(AE_RIFLE_WEAPONS_A_MAP >= AE_RIFLE_KINDS && AE_RIFLE_WEAPONS_A_MAP >= 5);
		memset(&weapons, 0, sizeof(weapons));
		for (index = 0; index < 5; index++)
			CHECK(ae_rifle_weapons_add(&weapons, tags[index], index == 4 ? -1 : 200 + index, kinds[index]) == index);
		CHECK(weapons.count == 5);
		for (index = 0; index < 5; index++)
			CHECK(ae_rifle_weapons_profile(&weapons, tags[index]) == ae_rifle_profile(kinds[index]));
		CHECK(ae_rifle_weapons_profile(&weapons, 120)->tags_rounds && weapons.projectiles[4] == -1);
		CHECK(ae_rifle_weapons_profile(&weapons, 999) == NULL && ae_rifle_weapons_profile(&weapons, -1) == NULL);
		CHECK(ae_rifle_weapons_add(&weapons, -1, -1, AE_RIFLE_DMR) == -1);
		CHECK(ae_rifle_weapons_add(&weapons, 130, -1, AE_RIFLE_NONE) == -1);
		while (weapons.count < AE_RIFLE_WEAPONS_A_MAP)
			CHECK(ae_rifle_weapons_add(&weapons, 300 + weapons.count, -1, AE_RIFLE_BATTLE_RIFLE) >= 5);
		CHECK(ae_rifle_weapons_add(&weapons, 400, -1, AE_RIFLE_BATTLE_RIFLE) == -1 && weapons.count == AE_RIFLE_WEAPONS_A_MAP);
		CHECK(ae_rifle_weapons_profile(&weapons, 120) == ravager);
	}

	/* the aim reticles' growth a shot: the EVO RIFLE's whole reticle 35 % for 0.3 s (back before its next pull, 11 ticks
	later); the BATTLE RIFLE's ticks 35 % for 0.3 s once a burst (back before the next, 14 ticks later) with its ring
	(item 0) fixed; the DMR's none */
	CHECK(NEAR(evo->reticle_growth, 0.35f) && evo->reticle_growth_ms == 300 && evo->reticle_fixed_items == 0);
	CHECK(evo->reticle_growth_ms < 11 * 1000 / 30);
	CHECK(NEAR(br->reticle_growth, 0.35f) && br->reticle_growth_ms == 300 && br->reticle_fixed_items == 0x01);
	CHECK(br->reticle_growth_ms < 14 * 1000 / 30);
	/* a pull's span: a burst's 3 rounds (4 ticks) and one more, under the next burst; a single shot's one tick */
	CHECK(ae_rifle_pull_ms(br) == 166 && ae_rifle_pull_ms(br) < 14 * 1000 / 30);
	CHECK(ae_rifle_pull_ms(evo) == 33);
	CHECK(NEAR(dmr->reticle_growth, evo->reticle_growth) && dmr->reticle_growth_ms == evo->reticle_growth_ms);
	/* (the DMR's middle, its reticle's first item, stays put; its outer ring spreads) */
	CHECK(dmr->reticle_fixed_items == 0x01);
	/* the zoom overlay's live readouts: the EVO RIFLE's and the DMR's (their overlay has the frames), not the BR's */
	CHECK(evo->scope_readouts && dmr->scope_readouts && !br->scope_readouts);
	{
		char text[16];

		ae_rifle_zoom_text(evo->zoom_seen, text, sizeof(text));
		CHECK(strcmp(text, "2x") == 0);
		ae_rifle_zoom_text(1.6f, text, sizeof(text));
		CHECK(strcmp(text, "1.6x") == 0);
		ae_rifle_zoom_text(10.0f, text, sizeof(text));
		CHECK(strcmp(text, "10x") == 0);
	}
	/* the shape: from its size at the shot up to the peak in 40 ms, then back to 1, eased; none without growth */
	CHECK(NEAR(ae_rifle_reticle_scale(1.0f, 0.35f, 300, 0), 1.0f));
	CHECK(NEAR(ae_rifle_reticle_scale(1.2f, 0.35f, 300, 0), 1.2f));
	CHECK(NEAR(ae_rifle_reticle_scale(1.0f, 0.35f, 300, AE_RIFLE_RETICLE_RISE_MS), 1.35f));
	CHECK(ae_rifle_reticle_scale(1.0f, 0.35f, 300, 20) > 1.0f && ae_rifle_reticle_scale(1.0f, 0.35f, 300, 20) < 1.35f);
	CHECK(ae_rifle_reticle_scale(1.0f, 0.35f, 300, 100) > ae_rifle_reticle_scale(1.0f, 0.35f, 300, 200));
	CHECK(ae_rifle_reticle_scale(1.0f, 0.35f, 300, 299) >= 1.0f);
	CHECK(NEAR(ae_rifle_reticle_scale(1.0f, 0.35f, 300, 300), 1.0f) && NEAR(ae_rifle_reticle_scale(1.3f, 0.35f, 300, 5000), 1.0f));
	CHECK(NEAR(ae_rifle_reticle_scale(1.0f, 0.0f, 0, 10), 1.0f) && NEAR(ae_rifle_reticle_scale(1.0f, 0.5f, 30, 10), 1.0f));

	if (failures)
		printf("%d failure(s)\n", failures);
	else
		printf("ae_rifle_profiles_test: ok\n");
	return failures ? 1 : 0;
}
