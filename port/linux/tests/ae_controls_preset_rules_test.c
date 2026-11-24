#include <stdio.h>
#include <string.h>

#include "ae_controls_preset_rules.h"
#include "../src/controller_layout.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int known(int game_preset)
{
	return ae_controls_layout_of(game_preset) >= 0;
}

int main(void)
{
	int preset;
	int step;

	/* the game's 0 .. 5 are the layouts' 0 .. 5; custom both ways */
	for (preset = 0; preset < CONTROLLER_PRESET_CUSTOM; preset++)
	{
		CHECK(ae_controls_layout_of(preset) == preset);
		CHECK(ae_controls_game_preset_of(preset) == preset);
	}
	CHECK(ae_controls_layout_of(AE_CONTROLS_GAME_CUSTOM) == CONTROLLER_PRESET_CUSTOM);
	CHECK(ae_controls_game_preset_of(CONTROLLER_PRESET_CUSTOM) == AE_CONTROLS_GAME_CUSTOM);
	CHECK(AE_CONTROLS_GAME_CUSTOM == AE_CONTROLS_GAME_PRESETS - 1);
	/* the game's own (FISHSTICKS .. GREEN FINGERS) and numbers out of range: no layout */
	for (preset = CONTROLLER_PRESET_CUSTOM; preset < AE_CONTROLS_GAME_CUSTOM; preset++)
		CHECK(ae_controls_layout_of(preset) == -1);
	CHECK(ae_controls_layout_of(-1) == -1 && ae_controls_layout_of(16) == -1 && ae_controls_layout_of(42) == -1);
	CHECK(ae_controls_game_preset_of(-1) == -1 && ae_controls_game_preset_of(CONTROLLER_PRESET_COUNT) == -1);

	/* stepping over the known ones in the game's order, wrapping: 0 1 2 3 4 5 15 */
	CHECK(ae_controls_step(0, 1) == 1);
	CHECK(ae_controls_step(5, 1) == AE_CONTROLS_GAME_CUSTOM);
	CHECK(ae_controls_step(AE_CONTROLS_GAME_CUSTOM, 1) == 0);
	CHECK(ae_controls_step(0, -1) == AE_CONTROLS_GAME_CUSTOM);
	CHECK(ae_controls_step(AE_CONTROLS_GAME_CUSTOM, -1) == 5);
	CHECK(ae_controls_step(3, -5) == 2);
	CHECK(ae_controls_step(3, 7) == 4);
	/* from the game's own: the next known one either way */
	CHECK(ae_controls_step(6, 1) == AE_CONTROLS_GAME_CUSTOM);
	CHECK(ae_controls_step(6, -1) == 5);
	CHECK(ae_controls_step(14, 1) == AE_CONTROLS_GAME_CUSTOM);
	CHECK(ae_controls_step(10, -1) == 5);
	CHECK(ae_controls_step(-3, 1) == 0);
	CHECK(ae_controls_step(40, 1) == 0);
	CHECK(ae_controls_step(40, -1) == AE_CONTROLS_GAME_CUSTOM);
	CHECK(ae_controls_step(-3, -1) == AE_CONTROLS_GAME_CUSTOM);
	/* (every step, from any number, lands on a known one: never out of range) */
	for (preset = -2; preset < AE_CONTROLS_GAME_PRESETS + 2; preset++)
	{
		for (step = -1; step <= 1; step += 2)
			CHECK(known(ae_controls_step(preset, step)));
	}
	/* (the known ones' full round, each once) */
	{
		int seen[AE_CONTROLS_GAME_PRESETS] = { 0 };
		int count;

		preset = 0;
		for (count = 0; count < CONTROLLER_PRESET_CUSTOM + 1; count++)
		{
			seen[preset]++;
			preset = ae_controls_step(preset, 1);
		}
		CHECK(preset == 0);
		for (count = 0; count < AE_CONTROLS_GAME_PRESETS; count++)
			CHECK(seen[count] == known(count));
	}

	/* the names: the game's, the known ones as controller_layout.c's titles */
	CHECK(!strcmp(ae_controls_preset_title(6), "FISHSTICKS"));
	CHECK(!strcmp(ae_controls_preset_title(14), "GREEN FINGERS"));
	CHECK(!strcmp(ae_controls_preset_title(AE_CONTROLS_GAME_CUSTOM), "CUSTOM"));
	CHECK(!strcmp(ae_controls_preset_title(-1), "DEFAULT") && !strcmp(ae_controls_preset_title(99), "DEFAULT"));
	for (preset = 0; preset < AE_CONTROLS_GAME_PRESETS; preset++)
	{
		if (known(preset))
			CHECK(!strcmp(ae_controls_preset_title(preset), controller_layout_preset_title(ae_controls_layout_of(preset))));
	}

	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
