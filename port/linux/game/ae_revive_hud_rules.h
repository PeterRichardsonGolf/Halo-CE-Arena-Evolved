/*
AE_REVIVE_HUD_RULES.H

The revive's HUD as pure rules (ae_revive_hud.c draws them; port/linux/tests/ae_revive_hud_rules_test.c runs them): the owner's
six-segment ring (the DOWNED helmet's, 2026-11-05) as spans of angle, how many of its segments a revive's progress fills white and
how many a bleed-out's time left keeps lit, the words of the downed player's own view, the distance in metres and the reach a
prompt asks for. HUD STYLE AE only. No game dependency.

The ring's six segments, clockwise from the top: the top right and the right upper (a quarter and a quarter), the right lower (the
long one), the left lower, the left upper and the top left. The gaps sit at 12, 3 and 9 o'clock and a wide one under the chin.
*/
#ifndef AE_REVIVE_HUD_RULES_H
#define AE_REVIVE_HUD_RULES_H

#include <wchar.h>

#define AE_REVIVE_SEGMENTS 6
/* the gap each segment gives up at either end of its span, degrees */
#define AE_REVIVE_SEGMENT_TRIM 4.5f
/* a world unit in metres (ten feet): the HUD's distances */
#define AE_REVIVE_METRES_PER_UNIT 3.048f

/* what the downed player's own view says */
enum
{
	AE_REVIVE_SELF_WAITING,		/* nobody is reviving them: WAITING FOR REVIVE, the nearest teammate or the bleed-out */
	AE_REVIVE_SELF_REVIVING		/* a teammate holds: "<name> is reviving you" */
};

/* segment index (0 the top right, clockwise) as the span of its arc in degrees clockwise from the top, the gap trimmed; FALSE for an index out of range */
int ae_revive_segment_span(int index, float *from_degrees, float *to_degrees);
/* the segments a progress (0-255, the wire's) fills white: 0 to 6, the whole ring at 255 */
int ae_revive_white_segments(int progress);
/* the segments a bleed-out keeps lit with left seconds of total (0 none lit, 6 all): a segment goes dark each sixth of the time gone;
the last second still shows one. The dark ones are the first ones (the ring drains clockwise from the top) */
int ae_revive_bleed_lit(int left_seconds, int total_seconds);
/* whether segment index is lit when lit are (the last ones of the ring stay when it drains) */
int ae_revive_segment_is_lit(int index, int lit);
/* whole metres for a distance in world units (at least 0) */
int ae_revive_metres(float world_units);
/* whether a teammate at an offset from the revive spot is in reach to hold (the host's rule: AE_ROUND_REVIVE_REACH across, the height up or down) */
int ae_revive_in_reach(float dx, float dy, float dz);
/* "BLEED-OUT 0:09" */
void ae_revive_bleed_text(int seconds, wchar_t *out, int size);
/* "NEAREST TEAMMATE 12m" */
void ae_revive_nearest_text(int metres, wchar_t *out, int size);
/* the metres as the marker's distance row: "24m" */
void ae_revive_distance_text(int metres, wchar_t *out, int size);
/* the mode of the own view from the reviver on the wire (-1 none) */
int ae_revive_self_mode(int reviver);

#endif
