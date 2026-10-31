/*
AE_TEXT_HIRES.H

The high-res text's fonts as AE lays its text out by them (port/linux/game/ae_text_layout.h): the TrueType fonts'
own advances, and their tabular digits. Overpass carries same-width digits as the OpenType feature tnum (one GSUB
single substitution for the ten digits), which stb_truetype does not apply; text_hires.c's glyph cache asks for a
character's glyph through ae_text_tabular_glyph (an AE hook line), and text_hires_advance (an AE hook line in
text_hires.c, beside its fonts) gives the layout the same glyph's advance. Include after stb_truetype.h.
*/

#ifndef AE_TEXT_HIRES_H
#define AE_TEXT_HIRES_H

/* the glyph tnum puts in place of glyph in a font whose GSUB table is the length bytes at gsub, or glyph itself:
single substitutions only (lookup type 1, through extensions, type 7). Every read is held inside the table; the
table's offset and length come from the font's own table records (pure: the unit tests' too) */
int ae_text_tabular_glyph_in(unsigned char const *gsub, unsigned long length, int glyph);

/* a character's glyph in the font: its tabular one where the font has it; 0 if the font has no glyph for it */
int ae_text_tabular_glyph(struct stbtt_fontinfo const *info, unsigned long code);

/* the advance of a character's glyph (ae_text_tabular_glyph's), in font units; -1 if the font has no glyph for it */
int ae_text_font_advance(struct stbtt_fontinfo const *info, unsigned long code);

/* the advance of a character in a high-res text font (text_hires_font's), in units of the 480 lines, as its glyph is
drawn (narrowed with it); a negative number if the font has no glyph for it (defined in text_hires.c) */
float text_hires_advance(long font, unsigned long code);

#endif
