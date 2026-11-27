/*
AE_FORGE_POINTER.INC

Content Evolved's Forge menus' pointer (platform_forge_pointer), #included in sdl_platform.c after the scoreboard's: the
scoreboard's pointer machinery frees the mouse for as long as a Forge menu is open, and its motion, clicks and wheel go
to the menu, none to the aim and the triggers.
*/

#ifndef HALO_ANDROID
BOOL platform_forge_pointer(BOOL wanted, struct platform_ui_pointer *pointer)
{
	BOOL active;

	pthread_mutex_lock(&input_lock);
	if (wanted && !scoreboard_pointer_active && !input_state.ui_pointer && platform_window)
		scoreboard_pointer_start();
	else if (!wanted && forge_pointer_wanted)
		scoreboard_pointer_stop();
	forge_pointer_wanted = wanted;
	active = wanted && scoreboard_pointer_active;
	*pointer = scoreboard_pointer;
	scoreboard_pointer.moved = FALSE;
	scoreboard_pointer.left_clicks = 0;
	scoreboard_pointer.right_clicks = 0;
	scoreboard_pointer.wheel_steps = 0;
	pthread_mutex_unlock(&input_lock);
	return active;
}
#endif
