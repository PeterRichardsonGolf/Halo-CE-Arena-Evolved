/*
AE_FORGE_POINTER_WHEEL.INC

A Forge menu's pointer takes the mouse's wheel (ae_forge_pointer.inc), #included in sdl_platform.c's wheel event.
*/

#ifndef HALO_ANDROID
			if (forge_pointer_wanted && scoreboard_pointer_active)
			{
				scoreboard_pointer.wheel_steps += event.wheel.y > 0.0f ? 1 : event.wheel.y < 0.0f ? -1 : 0;
				break;
			}
#endif
