/*
AE_FORGE_INPUT.INC

Content Evolved's Forge reads the first controller's state as the game gets it (port/forge/src/forge_hooks.c's
XInputGetState), #included in xinput_sdl.c after controller_port: the port of a controller's handle, and port 0's
gamepad as it was pressed and as a custom controller layout (controller_remap_input) made it. Forge's binds name the
buttons where they sit (D-pad Right goes into Forge mode), where a custom layout keeps only the game's actions.
*/

int xinput_device_port(HANDLE device)
{
	return controller_port(device);
}

static XINPUT_GAMEPAD forge_unmapped_pad, forge_remapped_pad;
static BOOL forge_pad_remapped;

/* port 0's gamepad before the layout, at this read */
static void ae_forge_input_unmapped(int port, XINPUT_GAMEPAD const *pad)
{
	if (port == 0)
		forge_unmapped_pad = *pad;
}

/* ... and after it, whether the layout changed it */
static void ae_forge_input_remapped(int port, XINPUT_GAMEPAD const *pad, int captured, BOOL gamepad)
{
	if (port == 0)
	{
		forge_remapped_pad = *pad;
		forge_pad_remapped = !captured && gamepad &&
			memcmp(&forge_unmapped_pad, &forge_remapped_pad, sizeof(forge_remapped_pad)) != 0;
	}
}

/* a state XInputGetState gave port 0, made the buttons pressed again for Forge: what the layout made of them taken
out, what was pressed put back */
void xinput_forge_unmapped(XINPUT_GAMEPAD *pad)
{
	int index;

	if (!forge_pad_remapped)
		return;
	pad->wButtons = (WORD)((pad->wButtons & ~forge_remapped_pad.wButtons) |
		(forge_unmapped_pad.wButtons & ~vote_held_buttons));
	for (index = 0; index < 8; index++)
	{
		BYTE value = pad->bAnalogButtons[index];

		if (forge_remapped_pad.bAnalogButtons[index] && value <= forge_remapped_pad.bAnalogButtons[index])
			value = 0;
		if (forge_unmapped_pad.bAnalogButtons[index] > value)
			value = forge_unmapped_pad.bAnalogButtons[index];
		pad->bAnalogButtons[index] = value;
	}
}
