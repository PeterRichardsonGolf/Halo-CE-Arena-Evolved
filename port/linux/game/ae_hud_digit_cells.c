/*
AE_HUD_DIGIT_CELLS.C

CE's counter digits laid out at any size (ae_hud_digit_cells.h). Pure.
*/

#include "ae_hud_digit_cells.h"

/* each glyph's ink across its sprite cell and the cell's width over its height, measured on the sheet's
high-resolution copy (port/assets/hud/hud_counter_numbers__0.png, alpha over 40 of 255): CE's own sheet, the
same on every stock map */
struct digit_glyph { wchar_t character; short frame; real left, right, aspect; };
static const struct digit_glyph glyphs[] = {
	{ L'0', 0, 0.1944f, 0.7917f, 1.5f },
	{ L'1', 1, 0.2847f, 0.6250f, 1.5f },
	{ L'2', 2, 0.2083f, 0.8056f, 1.5f },
	{ L'3', 3, 0.2083f, 0.8056f, 1.5f },
	{ L'4', 4, 0.3092f, 0.7632f, 19.0f / 12.0f },
	{ L'5', 5, 0.2083f, 0.8056f, 1.5f },
	{ L'6', 6, 0.2222f, 0.8194f, 1.5f },
	{ L'7', 7, 0.4444f, 0.8056f, 1.5f },
	{ L'8', 8, 0.2222f, 0.8194f, 1.5f },
	{ L'9', 9, 0.2222f, 0.8194f, 1.5f },
	{ L'.', 10, 0.2500f, 0.3542f, 1.5f },
	{ L':', 11, 0.2500f, 0.4722f, 1.5f },
	{ L'-', 12, 0.2292f, 0.5903f, 1.5f },
};

/* the colon's table (spec 14.15 part 7): CL = the digit left of the colon's ink gap to the dots, CR = the right
digit's, as shares of the digit height */
static const real colon_left[10] = { 0.312f, 0.081f, 0.306f, 0.312f, 0.206f, 0.306f, 0.306f, 0.100f, 0.312f, 0.312f };
static const real colon_right[10] = { 0.500f, 0.269f, 0.500f, 0.500f, 0.375f, 0.500f, 0.500f, 0.250f, 0.500f, 0.500f };

real ae_digits_colon_shift(wchar_t left, wchar_t right)
{
	if (left < L'0' || left > L'9' || right < L'0' || right > L'9')
		return 0.0f;
	return (colon_left[left - L'0'] - colon_right[right - L'0']) / 2.0f;
}

static const struct digit_glyph *glyph_of(wchar_t character)
{
	unsigned i;

	for (i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
		if (glyphs[i].character == character)
			return &glyphs[i];
	return 0;
}

short ae_digit_frame(wchar_t character)
{
	const struct digit_glyph *glyph = glyph_of(character);

	return glyph ? glyph->frame : -1;
}

boolean ae_digit_ink(wchar_t character, real *left, real *right)
{
	const struct digit_glyph *glyph = glyph_of(character);

	if (!glyph)
		return FALSE;
	if (left)
		*left = glyph->left;
	if (right)
		*right = glyph->right;
	return TRUE;
}

real ae_digit_advance(wchar_t character, real cap_px, real cell_px)
{
	if (cell_px > 0.0f)
		return cell_px;
	if (character == L':')
		return AE_DIGITS_COLON_ADVANCE * cap_px;
	if (character == L' ')
		return AE_DIGITS_SPACE_ADVANCE * cap_px;
	return AE_DIGITS_ADVANCE * cap_px;
}

real ae_digits_measure_cells(const wchar_t *text, real cap_px, real cell_px)
{
	real width = 0.0f;

	while (text && *text)
		width += ae_digit_advance(*text++, cap_px, cell_px);
	return width;
}

void ae_digit_quad(const wchar_t *text, long index, real x, real baseline_y, real cap_px, real cell_px,
	boolean ink_left, real aspect, struct ae_digit_quad *quad)
{
	const struct digit_glyph *glyph;
	real cell_x = x, height, width, advance;
	long i;

	quad->visible = FALSE;
	quad->x0 = quad->x1 = x;
	quad->y0 = quad->y1 = baseline_y;
	if (!text || index < 0)
		return;
	for (i = 0; i < index; i++)
	{
		if (!text[i])
			return;
		cell_x += ae_digit_advance(text[i], cap_px, cell_px);
	}
	glyph = glyph_of(text[index]);
	if (!text[index] || !glyph)
		return;
	advance = ae_digit_advance(text[index], cap_px, cell_px);
	height = cap_px / (AE_DIGITS_CAP_BOTTOM - AE_DIGITS_CAP_TOP);
	width = height * (aspect > 0.0f ? aspect : glyph->aspect);
	quad->x0 = ink_left ? cell_x - width * glyph->left :
		cell_x + advance / 2.0f - width * (glyph->left + glyph->right) / 2.0f;
	if (text[index] == L':' && cell_px <= 0.0f && index > 0 && !ink_left)
		quad->x0 += ae_digits_colon_shift(text[index - 1], text[index + 1]) * cap_px;
	quad->x1 = quad->x0 + width;
	quad->y0 = baseline_y - cap_px - height * AE_DIGITS_CAP_TOP;
	quad->y1 = quad->y0 + height;
	quad->visible = TRUE;
}

boolean ae_digits_ink_bounds_cells(const wchar_t *text, real cap_px, real cell_px, boolean ink_left, real *left,
	real *right)
{
	boolean found = FALSE;
	long i;

	for (i = 0; text && text[i]; i++)
	{
		struct ae_digit_quad quad;
		const struct digit_glyph *glyph = glyph_of(text[i]);

		ae_digit_quad(text, i, 0.0f, 0.0f, cap_px, cell_px, ink_left, 0.0f, &quad);
		if (!quad.visible || !glyph)
			continue;
		if (!found && left)
			*left = quad.x0 + (quad.x1 - quad.x0) * glyph->left;
		if (right)
			*right = quad.x0 + (quad.x1 - quad.x0) * glyph->right;
		found = TRUE;
	}
	return found;
}
