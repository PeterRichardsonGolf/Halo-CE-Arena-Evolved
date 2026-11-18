#include <math.h>
#include <stdio.h>

#include "ae_follow_camera_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1.0e-4)

int main(void)
{
	float value, angle;
	int frame;

	/* half-life: half the way in one, three quarters in two; nothing in no time; all of it with none */
	CHECK(NEAR(ae_follow_blend(0.1f, 0.1f), 0.5f));
	CHECK(NEAR(ae_follow_blend(0.1f, 0.2f), 0.75f));
	CHECK(ae_follow_blend(0.1f, 0.0f) == 0.0f && ae_follow_blend(0.0f, 0.01f) == 1.0f);
	CHECK(NEAR(ae_follow_smooth(0.0f, 10.0f, 0.1f, 0.1f), 5.0f));
	/* the same at any frame rate: 144 frames of 1/144 s end where 30 of 1/30 s do */
	value = 0.0f;
	for (frame = 0; frame < 144; frame++)
		value = ae_follow_smooth(value, 1.0f, AE_FOLLOW_TURN_HALF_LIFE, 1.0f / 144.0f);
	angle = 0.0f;
	for (frame = 0; frame < 30; frame++)
		angle = ae_follow_smooth(angle, 1.0f, AE_FOLLOW_TURN_HALF_LIFE, 1.0f / 30.0f);
	CHECK(NEAR(value, angle));

	/* angles go the shorter way round: from 3.0 to -3.0 rad (0.28 apart through pi) half way is pi, not 0 */
	angle = ae_follow_smooth_angle(3.0f, -3.0f, 0.1f, 0.1f);
	CHECK(fabs(fabs(angle) - 3.14159) < 0.001);
	angle = ae_follow_smooth_angle(-3.0f, 3.0f, 0.1f, 0.1f);
	CHECK(fabs(fabs(angle) - 3.14159) < 0.001);
	/* and settle on the target */
	angle = 0.5f;
	for (frame = 0; frame < 200; frame++)
		angle = ae_follow_smooth_angle(angle, -2.5f, 0.07f, 1.0f / 60.0f);
	CHECK(NEAR(angle, -2.5f));

	/* the glide: 0, half, 1 at the ends and middle; no speed at the ends; never past 0..1 */
	CHECK(ae_follow_glide(0.0f) == 0.0f && ae_follow_glide(1.0f) == 1.0f);
	CHECK(NEAR(ae_follow_glide(0.5f), 0.5f));
	CHECK(ae_follow_glide(0.01f) < 0.0001f && ae_follow_glide(0.99f) > 0.9999f);
	CHECK(ae_follow_glide(-1.0f) == 0.0f && ae_follow_glide(2.0f) == 1.0f);
	for (frame = 1; frame <= 100; frame++)
		CHECK(ae_follow_glide(frame / 100.0f) >= ae_follow_glide((frame - 1) / 100.0f));

	/* pitch: level aim looks a little down; it follows a share of the aim, within its limits */
	CHECK(NEAR(ae_follow_pitch(0.0f), -AE_FOLLOW_PITCH_DOWN));
	CHECK(NEAR(ae_follow_pitch(0.5f), 0.5f * AE_FOLLOW_PITCH_SHARE - AE_FOLLOW_PITCH_DOWN));
	CHECK(ae_follow_pitch(-1.5f) == AE_FOLLOW_PITCH_LOWEST && ae_follow_pitch(1.5f) == AE_FOLLOW_PITCH_HIGHEST);

	/* the death camera (owner 11-11: on the body till the respawn): a press (after 3 s dead) leaves it, with ALWAYS after
	any death and with LONG WAITS on a long wait; on its own only when no respawn is coming (out of lives, the next
	round's), after 5 s; never with OFF */
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 3.0f, 0, 1, 0));
	CHECK(ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 3.0f, 1, 1, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 0.5f, 1, 1, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 2.9f, 1, 1, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 15.0f, 0, 1, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 30.0f, 1, 0, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_OFF, 30.0f, 1, 1, 1));
	CHECK(ae_follow_leave_body(AE_FOLLOW_SETTING_ALWAYS, 3.0f, 1, 0, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_ALWAYS, 2.0f, 1, 0, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_ALWAYS, 4.0f, 0, 0, 0));
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_ALWAYS, 60.0f, 0, 1, 0));
	/* no respawn coming: on its own after 5 s */
	CHECK(!ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 4.9f, 0, 1, 1) &&
		ae_follow_leave_body(AE_FOLLOW_SETTING_LONG_WAITS, 5.0f, 0, 1, 1));
	CHECK(ae_follow_leave_body(AE_FOLLOW_SETTING_ALWAYS, 5.0f, 0, 1, 1));
	/* the death camera's ease and pitch; switches glide only near and in sight */
	CHECK(ae_follow_ease_in_out(0.0f) == 0.0f && ae_follow_ease_in_out(1.0f) == 1.0f && NEAR(ae_follow_ease_in_out(0.5f), 0.5f));
	CHECK(ae_follow_ease_in_out(0.25f) < 0.25f && ae_follow_ease_in_out(0.75f) > 0.75f && ae_follow_ease_in_out(2.0f) == 1.0f);
	CHECK(NEAR(ae_death_pitch(0.0f, 10.0f), AE_DEATH_PITCH));
	CHECK(ae_death_pitch(5.0f, 5.0f) > AE_DEATH_PITCH && ae_death_pitch(-50.0f, 1.0f) < AE_DEATH_PITCH && ae_death_pitch(-50.0f, 1.0f) >= AE_DEATH_PITCH_LOWEST);
	CHECK(ae_follow_switch_glides(2.0f, 1) && !ae_follow_switch_glides(2.0f, 0) && !ae_follow_switch_glides(5.0f, 1));

	/* (owner 11-11: a self-kill looked at the last death's killer) a kill counts for this death only when recorded at
	most a second before its camera began (30 ticks a second); an older one is the last death's, -1 none */
	CHECK(ae_death_killer_is_this_death(1000, 1000, 30) && ae_death_killer_is_this_death(1003, 1000, 30));
	CHECK(ae_death_killer_is_this_death(970, 1000, 30) && !ae_death_killer_is_this_death(969, 1000, 30));
	CHECK(!ae_death_killer_is_this_death(400, 1000, 30) && !ae_death_killer_is_this_death(-1, 1000, 30));
	/* the killer it looks at: none for one's own kill, none for no killer */
	CHECK(ae_death_killer_to_watch(7, 3) == 7 && ae_death_killer_to_watch(3, 3) == -1 && ae_death_killer_to_watch(-1, 3) == -1);

	printf("%d failures\n", failures);
	return failures != 0;
}
