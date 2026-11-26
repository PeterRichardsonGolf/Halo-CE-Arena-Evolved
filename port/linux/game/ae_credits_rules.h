/*
AE_CREDITS_RULES.H

The text of Arena Evolved's CREDITS page (the TAB menu's), the credits.txt shipped beside the game (port/assets/credits.txt).
Its lines: "# <heading>" starts a section; "- <text>" starts a bullet; any other line starts a paragraph, or goes on
with the block above it (joined by a space) until a blank line or the next "- " or "# " line. Pure
(port/linux/tests/ae_credits_rules_test.c); the screen is ae_screen_credits.c.
*/

#ifndef __AE_CREDITS_RULES_H
#define __AE_CREDITS_RULES_H

enum { AE_CREDITS_HEADING, AE_CREDITS_PARAGRAPH, AE_CREDITS_BULLET };
enum { AE_CREDITS_TEXT_SIZE = 700 };

struct ae_credits_block
{
	short kind;
	char text[AE_CREDITS_TEXT_SIZE];
};

/* the blocks of text (a text longer than a block holds is cut at its end, whole words not kept), at most maximum; how
many. NULL or empty text: 0 */
int ae_credits_parse(const char *text, struct ae_credits_block *out, int maximum);

/* the page's text when no credits.txt ships (a build without one): who made it, the licence and the Halo files rule */
const char *ae_credits_fallback(void);

#endif
