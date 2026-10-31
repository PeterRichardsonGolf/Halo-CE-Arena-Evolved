/*
AE_GLUE_SOUND.C

Plays the AE menus' frame sound (ae_sound.h ae_glue_sound_play): CE's own UI
feedback sounds, the ones ui_widget.c's ui_play_audio_feedback_sound plays for
the game's menus (sound\sfx\ui\cursor, forward, back, flag_failure, from the
loaded maps' tags), through the same unspatialized impulse, scaled by
audio.arena_menus_volume (ae_settings_menu_volume; 0 plays nothing, logged "(muted)"). While
debug.ae_test_screen is set, each sound played is logged "ae sound: NAME" for
the tests.

(0.4.3) The lobby countdown's beeps (ae_glue_sound_countdown): tones made in
code (ae_lobby_countdown.h ae_countdown_tone, no asset) on the UI voice, the
callouts' and the horn's (platform_ui_voice_start, ae_horn.c's pattern), at
the menus' volume; not through ae_sound_request, whose one sound a frame would
drop a beep under a cursor or a back on the same frame. Logged "ae sound:
countdown tick N" / "countdown go" under debug.ae_test_screen.
*/

#include "cseries.h"
#include "ae_hooks.h"
#include "ae_lobby_countdown.h"
#include "ae_sound.h"

/* tag_files/tag_groups.h and sound/game_sound.h (declared here as the game's other port units do: their headers need
the game's math types first) */
long tag_loaded(long group_tag, const char *name);
long unspatialized_impulse_sound_new(long definition_index, float scale);
long config_integer(char const *name);
void platform_log(char const *format, ...);
/* dsound_sdl.c (platform.h, declared here as ae_horn.c does) */
void platform_ui_voice_start(short const *samples, unsigned long frames, unsigned long channels,
	unsigned long sample_rate);

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
	/* (the sound definition group, 'snd!': sound/sound_definitions.h SOUND_DEFINITION_TAG) */
	definition = tag_loaded('snd!', tags[sound]);
	if (definition != NONE && volume > 0.0f)
		unspatialized_impulse_sound_new(definition, volume);
	if (logging < 0)
		logging = config_integer("debug.ae_test_screen") != 0;
	if (logging)
		platform_log("ae sound: %s%s", names[sound],
			definition == NONE ? " (its tag is not loaded)" : volume <= 0.0f ? " (muted)" : "");
}

void ae_glue_sound_countdown(
	int beep,
	short seconds)
{
	/* (each beep's tone made once; the one played is a scaled copy, which must stay while the voice plays it: a beep
	a second, each well under one, so one copy a kind) */
	static short tones[AE_BEEP_GO + 1][AE_BEEP_FRAMES], played[AE_BEEP_GO + 1][AE_BEEP_FRAMES];
	static unsigned long frames[AE_BEEP_GO + 1];
	static int logging = -1;
	float volume;
	unsigned long at;

	if (beep <= AE_BEEP_NONE || beep > AE_BEEP_GO)
		return;
	if (!frames[beep])
		frames[beep] = ae_countdown_tone(beep, tones[beep], AE_BEEP_FRAMES, AE_BEEP_RATE);
	volume = ae_settings_menu_volume();
	if (volume > 1.0f)
		volume = 1.0f;
	if (volume > 0.0f && frames[beep])
	{
		for (at = 0; at < frames[beep]; at++)
			played[beep][at] = (short)((float)tones[beep][at] * volume);
		platform_ui_voice_start(played[beep], frames[beep], 1, AE_BEEP_RATE);
	}
	if (logging < 0)
		logging = config_integer("debug.ae_test_screen") != 0;
	if (!logging)
		return;
	if (beep == AE_BEEP_GO)
		platform_log("ae sound: countdown go%s", volume <= 0.0f ? " (muted)" : "");
	else
		platform_log("ae sound: countdown tick %d%s%s", (int)seconds, beep == AE_BEEP_SOFT ? " (soft)" : "",
			volume <= 0.0f ? " (muted)" : "");
}
