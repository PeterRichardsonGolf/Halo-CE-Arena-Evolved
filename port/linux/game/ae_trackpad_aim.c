/*
AE_TRACKPAD_AIM.C

The Steam Controller's right trackpad added to the view (ae_trackpad_aim.h). The turn comes from
port/linux/src/xinput_sdl.c's halo_linux_trackpad_look.
*/

#include "cseries.h"
#include "units/units.h"
#include "input/input_abstraction.h"
#include "ae_trackpad_aim.h"

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(HALO_SWITCH_CONSOLE)
int halo_linux_trackpad_look(short gamepad_index, float *yaw, float *pitch);

void ae_trackpad_aim(short gamepad_index, long unit_index, short zoom_level, float *yaw, float *pitch)
{
	float turn_yaw, turn_pitch;

	if (!halo_linux_trackpad_look(gamepad_index, &turn_yaw, &turn_pitch))
		return;
	/* (the profile's look invert, as the stick's) */
	if (input_abstraction_port_look_inverted(gamepad_index))
		turn_pitch = -turn_pitch;
	if (unit_index != NONE && zoom_level != NONE)
	{
		float inverse_zoom = 1.f / unit_get_zoom_magnification(unit_index, zoom_level);

		turn_yaw *= inverse_zoom;
		turn_pitch *= inverse_zoom;
	}
	*yaw += turn_yaw;
	*pitch += turn_pitch;
}
#else
void ae_trackpad_aim(short gamepad_index, long unit_index, short zoom_level, float *yaw, float *pitch)
{
	(void)gamepad_index;
	(void)unit_index;
	(void)zoom_level;
	(void)yaw;
	(void)pitch;
}
#endif
