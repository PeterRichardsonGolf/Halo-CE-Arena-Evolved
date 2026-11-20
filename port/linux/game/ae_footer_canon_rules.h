/*
AE_FOOTER_CANON_RULES.H

The 0.9.4 BUTTON CANON as pure rules (port/linux/tests/ae_footer_canon_rules_test.c runs them): what each footer
button means, which B labels are allowed, a table of the footers of the screens the canon changed, a check that a footer
follows the canon, and the footer band's curve (a soft dark band behind every AE menu footer: ae_widgets_nav.c
ae_widget_footer_band).

The canon: A accepts; B goes back (label Back, Cancel, Leave or Close; an invite's answer keeps Decline); X acts on the
focused row; Y acts on the screen; LB / RB move sideways between groups; LT / RT page a list or jump to its next
section. The keyboard's keys follow (X = F, Y = R, LB / RB = Q / E, LT / RT = PgUp / PgDn).
*/

#ifndef __AE_FOOTER_CANON_RULES_H
#define __AE_FOOTER_CANON_RULES_H

#include <math.h>

/* the footer band (spec units at 1080p, scaled with the UI): its height, the fade-in at its top, the curve's power, the
darkest alpha at the bottom, its colour 0xRRGGBB */
#define AE_FOOTER_BAND_HEIGHT 125.0f
#define AE_FOOTER_BAND_RAMP 40.0f
#define AE_FOOTER_BAND_POWER 1.6f
#define AE_FOOTER_BAND_ALPHA 0.80f
#define AE_FOOTER_BAND_RGB 0x070A10u

/* the band's alpha (0 to AE_FOOTER_BAND_ALPHA) at depth units under its top, both in spec units (0 at the top, the
maximum from AE_FOOTER_BAND_RAMP down; inline: ae_widgets_nav.c draws with it) */
static inline float ae_footer_band_alpha(float depth)
{
	if (depth <= 0.0f)
		return 0.0f;
	if (depth >= AE_FOOTER_BAND_RAMP)
		return AE_FOOTER_BAND_ALPHA;
	return AE_FOOTER_BAND_ALPHA * powf(depth / AE_FOOTER_BAND_RAMP, AE_FOOTER_BAND_POWER);
}

/* the buttons a footer prompt names (not ae_prompt's own enum: this file is pure) */
enum { AE_CANON_A, AE_CANON_B, AE_CANON_X, AE_CANON_Y, AE_CANON_LB_RB, AE_CANON_LT_RT, AE_CANON_DPAD, AE_CANON_STICK,
	AE_CANON_START, AE_CANON_SELECT };

struct ae_canon_prompt
{
	int button;
	const char *label;
};

/* a B label the canon allows on a screen: Back, Cancel, Leave or Close anywhere; the named exceptions only on their own
screen: Decline (INVITE: it answers someone), Quit (MAIN MENU: the one B that closes the program), Resume (PAUSE: the one B
that goes back into the game). screen NULL: the general set */
int ae_canon_b_label_allowed(const char *screen, const char *label);

/* the named exceptions to "LB / RB move sideways, X acts on the focused row": a screen where X keeps a screen-level job
because LB / RB are taken (SETTINGS group pages: X Player; CROSSHAIR: X Background). 1 when X may be that on the screen */
int ae_canon_x_exception(const char *screen, const char *label);

/* a footer's failure against the canon: 0 follows it; 1 a button named twice; 2 a B label outside the set; 3 no B; 4 a
label empty */
int ae_canon_footer_check(const char *screen, struct ae_canon_prompt const *prompts, int count);

/* the footers of the screens the canon changed (the table the test checks; the screens' source says the same) */
struct ae_canon_footer
{
	const char *screen;
	int count;
	struct ae_canon_prompt prompts[10];
};
extern struct ae_canon_footer const ae_canon_footers[];
extern const int ae_canon_footer_count;

#endif
