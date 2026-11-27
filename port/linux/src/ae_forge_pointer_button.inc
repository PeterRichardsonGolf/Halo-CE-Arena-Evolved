/*
AE_FORGE_POINTER_BUTTON.INC

A Forge menu's pointer takes the mouse's buttons (ae_forge_pointer.inc), #included in sdl_platform.c's button event: its
clicks go to the menu, none to the game.
*/

#ifndef HALO_ANDROID
			if (!input_state.ui_pointer && forge_pointer_wanted && scoreboard_pointer_active)
			{
				if (event.button.down && event.button.button == SDL_BUTTON_LEFT)
				{
					scoreboard_pointer.left_clicks++;
					scoreboard_pointer.click_x = event.button.x;
					scoreboard_pointer.click_y = event.button.y;
				}
				else if (event.button.down && event.button.button == SDL_BUTTON_RIGHT)
				{
					scoreboard_pointer.right_clicks++;
				}
				break;
			}
#endif
