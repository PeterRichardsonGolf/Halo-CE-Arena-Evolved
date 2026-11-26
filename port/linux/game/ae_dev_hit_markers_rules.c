/*
AE_DEV_HIT_MARKERS_RULES.C

Pure rules for DEV MODE's hit markers (ae_dev_hit_markers_rules.h). No engine includes.
*/

#include "ae_dev_hit_markers_rules.h"

void ae_dev_hit_marker_shape(long elapsed_ms, int kill, float *alpha, float *scale)
{
	long hold = kill ? AE_DEV_HIT_MARKER_KILL_HOLD_MS : AE_DEV_HIT_MARKER_HIT_HOLD_MS;
	long fade = kill ? AE_DEV_HIT_MARKER_KILL_FADE_MS : AE_DEV_HIT_MARKER_HIT_FADE_MS;

	*scale = 1.0f;
	if (elapsed_ms < 0 || elapsed_ms >= hold + fade)
	{
		*alpha = 0.0f;
		return;
	}
	*alpha = elapsed_ms < hold ? 1.0f : 1.0f - (float)(elapsed_ms - hold) / (float)fade;
	if (kill && elapsed_ms < AE_DEV_HIT_MARKER_KILL_POP_MS)
	{
		float t = (float)elapsed_ms / (float)AE_DEV_HIT_MARKER_KILL_POP_MS;

		/* (eased out: fast first, settling) */
		*scale = AE_DEV_HIT_MARKER_KILL_POP_SCALE - (AE_DEV_HIT_MARKER_KILL_POP_SCALE - 1.0f) * (1.0f - (1.0f - t) * (1.0f - t));
	}
}

int ae_dev_hit_marker_counts(int owner_local, int own_unit, int own_vehicle, float damage, int kill, int already_dead)
{
	if (!owner_local || own_unit || own_vehicle)
		return 0;
	if (already_dead)
		return kill;
	return kill || damage > 0.0f;
}

float ae_dev_hit_marker_half_size(float reticle_half_px, float half_default_px, float gap_px, float scale)
{
	float half = reticle_half_px > 0.0f ? reticle_half_px : half_default_px * scale;

	return half + gap_px * scale;
}
