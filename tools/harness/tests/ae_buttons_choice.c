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
	/* one family per device: PlayStation pad, Nintendo pad, nothing for the keyboard and Xbox-like pads; a glyph of a
	family is drawn only while that family's pad is the last used */
	CASE("family")
	{
		CHECK(ae_buttons_scheme_family(SCHEME_PLAYSTATION) == AE_BUTTON_FAMILY_PLAYSTATION, "a PlayStation pad's family");
		CHECK(ae_buttons_scheme_family(SCHEME_NINTENDO) == AE_BUTTON_FAMILY_NINTENDO, "a Nintendo pad is not Nintendo");
		CHECK(ae_buttons_scheme_family(SCHEME_KEYBOARD) == AE_BUTTON_FAMILY_OWN, "the keyboard has a pad family");
		CHECK(ae_buttons_scheme_family(SCHEME_XBOX) == AE_BUTTON_FAMILY_OWN, "an Xbox-like pad has a family");
		CHECK(ae_buttons_glyph_is_drawn(AE_BUTTON_FAMILY_NINTENDO, AE_BUTTON_FAMILY_NINTENDO), "Nintendo art not drawn for it");
		CHECK(!ae_buttons_glyph_is_drawn(AE_BUTTON_FAMILY_PLAYSTATION, AE_BUTTON_FAMILY_NINTENDO),
			"PlayStation art drawn for a Nintendo pad");
		CHECK(!ae_buttons_glyph_is_drawn(AE_BUTTON_FAMILY_NINTENDO, AE_BUTTON_FAMILY_PLAYSTATION),
			"Nintendo art drawn for a PlayStation pad");
		CHECK(ae_buttons_glyph_is_drawn(AE_BUTTON_FAMILY_PLAYSTATION, AE_BUTTON_FAMILY_PLAYSTATION), "PlayStation art not drawn for it");
		CHECK(!ae_buttons_glyph_is_drawn(AE_BUTTON_FAMILY_OWN, AE_BUTTON_FAMILY_OWN), "art drawn for the Xbox icons' own pad");
		CHECK(!ae_buttons_glyph_is_drawn(AE_BUTTON_FAMILY_NINTENDO, AE_BUTTON_FAMILY_OWN), "Nintendo art drawn for an Xbox pad");
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
	/* a Custom Edition map's message icons: the Xbox's sequences (from ui.map) after the map's own 20 */
	CASE("ce_sequence")
	{
		CHECK(ae_buttons_ce_sequence(20, 20) == 0, "the first of the Xbox's sequences is not A's");
		CHECK(ae_buttons_ce_sequence(23, 20) == 3, "the fourth of the Xbox's sequences is not Y's");
		CHECK(ae_buttons_ce_sequence(1, 20) == -1, "one of the map's own sequences taken for the Xbox's");
		CHECK(ae_buttons_ce_sequence(1, -1) == -1, "a sequence taken with no Xbox icons in the map");
		CHECK(ae_buttons_sprite_wanted(1, 0xFu, ae_buttons_ce_sequence(21, 20), 1),
			"B's sprite of the Xbox's copy not taken with a PlayStation pad");
		CHECK(!ae_buttons_sprite_wanted(1, 0xFu, ae_buttons_ce_sequence(2, 20), 1),
			"the map's own sequence 2 drawn as the PlayStation square");
		return 0;
	}
	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}
