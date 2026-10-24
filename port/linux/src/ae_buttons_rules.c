/*
AE_BUTTONS_RULES.C

Arena Evolved: the PlayStation button icons' decisions (ae_buttons.h), pure, so
tools/harness/tests/test_ae_buttons.py takes them as they are.
*/

#include "ae_buttons.h"

int ae_buttons_scheme_playstation(int scheme)
{
	/* (xinput_sdl.c: 0 the keyboard, 1 an Xbox-like pad, 2 a PlayStation pad, 3 a Nintendo pad) */
	return scheme == 2;
}

int ae_buttons_scheme_family(int scheme)
{
	/* (the same schemes) */
	if (scheme == 2)
		return AE_BUTTON_FAMILY_PLAYSTATION;
	if (scheme == 3)
		return AE_BUTTON_FAMILY_NINTENDO;
	return AE_BUTTON_FAMILY_OWN;
}

int ae_buttons_glyph_is_drawn(int glyph_family, int family_now)
{
	return family_now != AE_BUTTON_FAMILY_OWN && glyph_family == family_now;
}

int ae_buttons_draw_playstation(int kind, int playstation_now)
{
	switch (kind)
	{
	case AE_BUTTONS_PLACEHOLDER:
		return 1;
	case AE_BUTTONS_WHOLE:
		return playstation_now != 0;
	default:
		return 0;
	}
}

int ae_buttons_sprite_wanted(int playstation_now, unsigned int sprites, int sequence_index, int pixels_match)
{
	if (!playstation_now || sequence_index < 0 || sequence_index >= 32)
		return 0;
	if (!(sprites & (1u << sequence_index)))
		return 0;
	return pixels_match != 0;
}

int ae_buttons_ce_sequence(int sequence_index, int first_sequence)
{
	if (first_sequence < 0 || sequence_index < first_sequence)
		return -1;
	return sequence_index - first_sequence;
}
