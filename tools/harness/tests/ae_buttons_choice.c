/*
AE_BUTTONS_CHOICE.C (test)

The PlayStation button icons' decisions (port/linux/src/ae_buttons_rules.c,
DamnationCE v0.3.41's feature in AE's files): a whole icon draws the
PlayStation texture only while a PlayStation controller is the last used
(else upstream's choice stands: its high-res texture or the map's pixels); a
sheet's placeholder, handed out only then, always draws it; a sheet's sprite
goes to the placeholder only for the texture's sequences, on the pixels it
was drawn for. test_ae_buttons.py takes the enum (config.inc) and the code
(under_test.inc).
*/

#include "harness.h"
#include "config.inc"
#include "under_test.inc"

/* xinput_sdl.c's schemes */
enum { SCHEME_KEYBOARD, SCHEME_XBOX, SCHEME_PLAYSTATION, SCHEME_NINTENDO };

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	CASE("scheme")
	{
		CHECK(ae_buttons_scheme_playstation(SCHEME_PLAYSTATION), "a PlayStation pad is not PlayStation");
		CHECK(!ae_buttons_scheme_playstation(SCHEME_KEYBOARD), "the keyboard counts as PlayStation");
		CHECK(!ae_buttons_scheme_playstation(SCHEME_XBOX), "an Xbox pad counts as PlayStation");
		CHECK(!ae_buttons_scheme_playstation(SCHEME_NINTENDO), "a Nintendo pad counts as PlayStation");
		return 0;
	}
	/* a whole icon: PlayStation's texture only with a PlayStation pad */
	CASE("whole")
	{
		CHECK(ae_buttons_draw_playstation(AE_BUTTONS_WHOLE, 1), "a PlayStation player gets the Xbox icon");
		CHECK(!ae_buttons_draw_playstation(AE_BUTTONS_WHOLE, 0),
			"a keyboard or Xbox player gets the PlayStation icon (upstream's choice must stand)");
		return 0;
	}
	/* a placeholder (handed out only while wanted) always draws it; anything else never */
	CASE("placeholder")
	{
		CHECK(ae_buttons_draw_playstation(AE_BUTTONS_PLACEHOLDER, 0), "a placeholder drew nothing");
		CHECK(ae_buttons_draw_playstation(AE_BUTTONS_PLACEHOLDER, 1), "a placeholder drew nothing");
		CHECK(!ae_buttons_draw_playstation(AE_BUTTONS_NONE, 1), "a texture that is no icon drew the PlayStation one");
		CHECK(!ae_buttons_draw_playstation(AE_BUTTONS_NONE, 0), "a texture that is no icon drew the PlayStation one");
		return 0;
	}
	/* a sheet's sprites: sequences 0-3 (A, B, X, Y) of the texture's, PlayStation only, its pixels checked */
	CASE("sprites")
	{
		unsigned int sprites = 0xFu;

		CHECK(ae_buttons_sprite_wanted(1, sprites, 0, 1), "A's sprite not taken with a PlayStation pad");
		CHECK(ae_buttons_sprite_wanted(1, sprites, 3, 1), "Y's sprite not taken with a PlayStation pad");
		CHECK(!ae_buttons_sprite_wanted(0, sprites, 0, 1), "A's sprite taken without a PlayStation pad");
		CHECK(!ae_buttons_sprite_wanted(1, sprites, 4, 1), "another sequence's sprite taken");
		CHECK(!ae_buttons_sprite_wanted(1, sprites, 0, 0), "a sprite taken on pixels the texture was not drawn for");
		CHECK(!ae_buttons_sprite_wanted(1, sprites, -1, 1), "a negative sequence taken");
		CHECK(!ae_buttons_sprite_wanted(1, 0xFFFFFFFFu, 32, 1), "a sequence past 31 taken");
		return 0;
	}
	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}
