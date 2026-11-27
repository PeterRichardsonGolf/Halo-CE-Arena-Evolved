/*
AE_FORGE_POINTER_GL.INC

Content Evolved's Forge menus' pointer in the window's UI coordinates (halo_forge_pointer_update), #included in d3d8_gl.c
after the scoreboard's: its position, clicks and wheel since the last call.
*/

#ifdef HALO_ANDROID
int halo_forge_pointer_update(int wanted, struct halo_ui_pointer *pointer)
{
	(void)wanted;
	(void)pointer;
	return 0;
}
#else
int halo_forge_pointer_update(int wanted, struct halo_ui_pointer *pointer)
{
	struct platform_ui_pointer state;

	memset(pointer, 0, sizeof(*pointer));
	if (!platform_forge_pointer(wanted != 0, &state) || !device.gl_ready)
		return 0;
	ui_point_from_window_on(state.x, state.y, FALSE, &pointer->x, &pointer->y);
	ui_point_from_window_on(state.click_x, state.click_y, FALSE, &pointer->click_x, &pointer->click_y);
	pointer->moved = state.moved != FALSE;
	pointer->left_clicks = (unsigned char)(state.left_clicks < 255 ? state.left_clicks : 255);
	pointer->right_clicks = (unsigned char)(state.right_clicks < 255 ? state.right_clicks : 255);
	pointer->wheel_steps = (signed char)(state.wheel_steps < -8 ? -8 : state.wheel_steps > 8 ? 8 : state.wheel_steps);
	return 1;
}
#endif
