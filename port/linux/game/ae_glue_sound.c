/*
AE_GLUE_SOUND.C

Plays the AE menus' frame sound (ae_sound.h ae_glue_sound_play): CE's own UI
feedback sounds, the ones ui_widget.c's ui_play_audio_feedback_sound plays for
the game's menus (sound\sfx\ui\cursor, forward, back, flag_failure, from the
loaded maps' tags), through the same unspatialized impulse, scaled by
audio.arena_menus_volume (ae_settings_menu_volume; 0 plays nothing). While
debug.ae_test_screen is set, each sound played is logged "ae sound: NAME" for
the tests.
*/

#include "cseries.h"
#include "ae_hooks.h"
#include "ae_sound.h"

/* tag_files/tag_groups.h and sound/game_sound.h (declared here as the game's other port units do: their headers need
the game's math types first) */
long tag_loaded(long group_tag, const char *name);
long unspatialized_impulse_sound_new(long definition_index, float scale);
long config_integer(char const *name);
void platform_log(char const *format, ...);

void ae_glue_sound_play(
	int sound)
{
	static const char *const names[] = { NULL, "cursor", "forward", "back", "failure" };
	static const char *const tags[] =
	{
		NULL, "sound\\sfx\\ui\\cursor", "sound\\sfx\\ui\\forward", "sound\\sfx\\ui\\back",
		"sound\\sfx\\ui\\flag_failure",
	};
	static int logging = -1;
	float volume;
	long definition;

	if (sound <= AE_SOUND_NONE || sound > AE_SOUND_FAILURE)
		return;
	volume = ae_settings_menu_volume();
	if (volume <= 0.0f)
		return;
	/* (the sound definition group, 'snd!': sound/sound_definitions.h SOUND_DEFINITION_TAG) */
	definition = tag_loaded('snd!', tags[sound]);
	if (definition != NONE)
		unspatialized_impulse_sound_new(definition, volume);
	if (logging < 0)
		logging = config_integer("debug.ae_test_screen") != 0;
	if (logging)
		platform_log("ae sound: %s%s", names[sound], definition == NONE ? " (its tag is not loaded)" : "");
}
