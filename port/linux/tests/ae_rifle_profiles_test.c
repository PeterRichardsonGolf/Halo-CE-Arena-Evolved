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

	/* the behaviours (ae_pack_format.h): 1 the burst rifle, 2 the EVO RIFLE, 3 the DMR; the tags' own (0) and unknown
	ones are no AE rifle */
	CHECK(ae_rifle_kind_of_behaviour(0) == AE_RIFLE_NONE);
	CHECK(ae_rifle_kind_of_behaviour(1) == AE_RIFLE_BATTLE_RIFLE);
	CHECK(ae_rifle_kind_of_behaviour(2) == AE_RIFLE_EVO_RIFLE);
	CHECK(ae_rifle_kind_of_behaviour(3) == AE_RIFLE_DMR);
	CHECK(ae_rifle_kind_of_behaviour(4) == AE_RIFLE_NONE);
	CHECK(ae_rifle_profile(AE_RIFLE_NONE) == NULL && ae_rifle_profile(AE_RIFLE_KINDS) == NULL);
	CHECK(br && evo && dmr);
	if (!br || !evo || !dmr)
		return 1;

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

	/* the competitive EVO RIFLE: 4 rounds pop CE's shield, the 5th to the head kills */
	CHECK(evo->rounds_a_pull == 1 && evo->keeps_pull && evo->recovery_ticks == 9);
	CHECK(NEAR(evo->round_damage, 6.5f * 11.0f / 4.0f));
	CHECK(NEAR(evo->round_damage * 1.06f * 4.0f, 75.79f) && evo->round_damage * 1.06f * 3.0f < 75.0f);
	CHECK(evo->rounds_loaded == 15 && evo->rounds_total_initial == 60 && evo->rounds_total_maximum == 75);
	CHECK(evo->rounds_between_tracers == 0);
	CHECK(NEAR(evo->autoaim_degrees, 2.0f) && NEAR(evo->autoaim_distance, 18.0f));
	CHECK(NEAR(evo->magnetism_degrees, 5.0f) && NEAR(evo->magnetism_distance, 25.0f));
	CHECK(NEAR(evo->error_lower_degrees, 0.0f) && NEAR(evo->zoom_seen, 2.0f));
	/* a pull every 11 ticks (2.727 a second, the Evo's 2.715): 44 ticks, 1.47 s */
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_EVO_RIFLE) == 44);

	/* the zero-bloom DMR: 3 a second, the EVO RIFLE's damage, a constant 0.15 degree spread, 3x, the fast bullet */
	CHECK(dmr->rounds_a_pull == 1 && dmr->keeps_pull && dmr->recovery_ticks == 8);
	CHECK(NEAR(dmr->round_damage, evo->round_damage));
	CHECK(dmr->rounds_loaded == 15 && dmr->rounds_total_initial == 45 && dmr->rounds_total_maximum == 60);
	CHECK(NEAR(dmr->autoaim_degrees, 2.25f) && NEAR(dmr->autoaim_distance, 20.0f));
	CHECK(NEAR(dmr->magnetism_degrees, 7.0f) && NEAR(dmr->magnetism_distance, 25.0f));
	CHECK(NEAR(dmr->error_lower_degrees, 0.15f) && NEAR(dmr->error_upper_degrees, 0.15f));
	CHECK(NEAR(dmr->zoom_seen, 3.0f) && NEAR(dmr->bullet_speed, 3000.0f) && NEAR(dmr->bullet_range, 250.0f));
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_DMR) == 40);
	CHECK(ae_rifle_perfect_kill_ticks(AE_RIFLE_NONE) == 0);

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
