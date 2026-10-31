/*
AE_BUTTONS.H

Arena Evolved: PlayStation button icons (from DamnationCE v0.3.41, CC0). While a
PlayStation controller is the one last used (platform_input_scheme 2), the game's
button icons show its buttons: A, B, X and Y as cross, circle, square and
triangle. Everything is AE's own, beside upstream's high-res HUD
(hud_hires.c), which it does not change: its own table (ae_button_glyphs[],
generated from port/assets/buttons/playstation by tools/ae_button_glyphs.py
through tools/embed_assets.py's AE hook), its own textures, decoded only once
a PlayStation controller is used, and AE hooks in:

- xbox_textures.c: a whole icon (the Xbox menus' a_butn, b_butn, x_butn,
  y_butn, a_butn_sm, b_butn_sm) is known by its pixels as they are uploaded
  (ae_buttons_uploaded: the bitmap at that address, its pixels' CRC the one
  the texture was drawn for); while a PlayStation controller is the last
  used, its texture is drawn in the icon's place (ae_buttons_drawn,
  ae_buttons_entry_texture). Otherwise upstream's own choice stands
  (display.high_res_text's texture, or the map's pixels).
- hud_hires_tags.c: a sheet's sprites (the icons in the menus' text,
  hud_msg_icons_sm, and in the in-game messages, combined\hud_msg_icons)
  are drawn, while a PlayStation controller is the last used, from a
  placeholder bitmap of AE's (ae_buttons_sprite_bitmap, port/linux/game/
  ae_buttons_tags.c) whose texture is the sheet's PlayStation one.

The decisions are ae_buttons_rules.c's (pure; tools/harness test_ae_buttons.py).
*/

#ifndef __AE_BUTTONS_H
#define __AE_BUTTONS_H

/* an embedded PlayStation texture: the bitmap it stands for (its group tag,
its index there, its size, the CRC-32 of its first mip level's pixels as the
English Xbox maps have them), sprites: the sequences of a sheet it is drawn
for (a bit each), or 0: the whole bitmap; an 8-bit RGBA PNG. own: not a
PlayStation texture but AE's own picture, which no map has, drawn from its
placeholder whatever the pad (ae_buttons_own_bitmap, port/linux/game/
ae_buttons_tags.h: the HUD's key prompts' sheets, tools/ae_key_pictures.py) */
struct ae_button_glyph
{
	const char *tag;
	int bitmap;
	unsigned int width, height;
	unsigned int crc;
	unsigned int sprites;
	const unsigned int *png;
	unsigned int png_size;
	int own;
	/* which controller's art (AE_BUTTON_FAMILY_*) */
	int family;
};

/* a texture's family: the keys' and AE's other own pictures (drawn whatever the pad), PlayStation's, Nintendo's. The
PlayStation glyphs are the ones AE finds in the maps; a Nintendo glyph is another picture of the same bitmap
(tools/ae_button_glyphs.py nintendo), drawn in its place while a Nintendo pad is the last used */
enum
{
	AE_BUTTON_FAMILY_OWN = 0,
	AE_BUTTON_FAMILY_PLAYSTATION = 1,
	AE_BUTTON_FAMILY_NINTENDO = 2,
};

extern const struct ae_button_glyph ae_button_glyphs[];
extern const unsigned int ae_button_glyph_count;

/* ---------- the decisions (ae_buttons_rules.c, pure) */

enum
{
	/* what a texture entry is to AE: nothing, a whole icon, a sheet's placeholder */
	AE_BUTTONS_NONE = 0,
	AE_BUTTONS_WHOLE = 1,
	AE_BUTTONS_PLACEHOLDER = 2,
};

/* whether an entry of that kind draws the PlayStation texture now (a placeholder
is only handed out while one is wanted: ae_buttons_sprite_wanted) */
int ae_buttons_draw_playstation(int kind, int playstation_now);
/* whether a sheet's sprite is drawn from AE's placeholder: a PlayStation
controller the last used, the sequence one of the texture's, and the sheet's
pixels those it was drawn for */
int ae_buttons_sprite_wanted(int playstation_now, unsigned int sprites, int sequence_index, int pixels_match);
/* a Custom Edition map's message icons sheet with the Xbox's sequences after its own (upstream's ce_hud.c, from ui.map;
the button icons name them past first_sequence): the Xbox's sequence a sequence of it is, or -1 (one of the map's own) */
int ae_buttons_ce_sequence(int sequence_index, int first_sequence);
/* the controller scheme (xinput_sdl.c's platform_input_scheme) is a PlayStation one's */
int ae_buttons_scheme_playstation(int scheme);
/* the family (AE_BUTTON_FAMILY_*) of the pad the scheme is: PlayStation's, Nintendo's, or 0 (the keyboard, an Xbox-like
pad: the game's own Xbox icons stand). The menus' device choice (ui_overlay.c) is the scheme, one family per device */
int ae_buttons_scheme_family(int scheme);
/* whether a glyph of that family is the picture drawn while the pad's family is the one now (a glyph is its own's, or
the alternate of the PlayStation glyph standing for the same bitmap) */
int ae_buttons_glyph_is_drawn(int glyph_family, int family_now);

/* ---------- the platform's side (ae_buttons.c) */

/* a PlayStation or a Nintendo controller is the one last used (its art stands in the Xbox icons' place; 0 without
the game browser, which keeps it) */
int ae_buttons_playstation_now(void);
/* the family of the pad last used (ae_buttons_scheme_family of the scheme; 0 without the game browser) */
int ae_buttons_family_now(void);
/* whether the pixels at address (guest virtual; first mip level level0_size
bytes) are those glyph's texture was drawn for */
int ae_buttons_pixels_match(long glyph, unsigned long address, unsigned long level0_size);
/* xbox_textures.c: the pixels of the texture whose D3D Data is data were
uploaded from address: which icon they are, if any (none forgets data) */
void ae_buttons_uploaded(unsigned long data, unsigned long address, unsigned long width, unsigned long height,
	unsigned long level0_size);
/* xbox_textures.c: whether the texture whose D3D Data is data draws its
PlayStation texture now, and that texture (its levels set, drawn as a
high-res one) */
int ae_buttons_drawn(unsigned long data);
struct xgpu_texture_description;
unsigned int ae_buttons_entry_texture(unsigned long data, struct xgpu_texture_description *description);
/* ae_buttons_tags.c: a sheet's placeholder (its D3D texture; NULL forgets
the glyph's) */
void ae_buttons_register_placeholder(long glyph, const unsigned long *texture);
/* the glyph's texture decoded (on first use); FALSE if it cannot be */
int ae_buttons_glyph_ready(long glyph);

/* ---------- the game's side (port/linux/game/ae_buttons_tags.c) */

/* hud_hires_tags.c: a map's tags loaded, unloaded */
void ae_buttons_tags_loaded(void);
void ae_buttons_tags_unloaded(void);
/* the whole icon glyph whose bitmap's pixels are at address with this size, or -1 */
long ae_buttons_whole_at(unsigned long address, unsigned long width, unsigned long height);

#endif
