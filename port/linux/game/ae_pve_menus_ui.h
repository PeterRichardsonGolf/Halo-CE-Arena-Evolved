/*
AE_PVE_MENUS_UI.H

What the campaign's rule screens share (ae_pve_menus_ui.c): their settings written through the allowed list
(ae_pve_menus_rules.h), the profile's name for a crumb, MCC's or Content Evolved's skull pictures, and the right-hand panel's
drawing. Include after cseries.h and ae_setup_flow.h.
*/

#ifndef __AE_PVE_MENUS_UI_H
#define __AE_PVE_MENUS_UI_H

#include "ae_setup_flow.h"

/* a setting written when it is one of the campaign rule screens' (ae_pve_menus_key_allowed): 1 written, 0 refused or failed */
int ae_pve_menus_ui_write(const char *key, const char *value);
/* a setting's text as the config holds it ("true", "50", "per_player"); "" when unknown. Valid until the next call */
const char *ae_pve_menus_ui_read(void *context, const char *key);
/* player 1's profile name in capitals ("NO PROFILE" when there is none) */
void ae_pve_menus_ui_profile(char *name, int size);
/* the skulls turned on that are had now (game.skulls_active, within the skulls the player has); the skulls had, and found */
int ae_pve_menus_ui_skulls_on_count(void);
/* whether MCC has a picture for every skull in a drawn set; an empty, malformed or incomplete set uses Content Evolved */
int ae_pve_menus_ui_skulls_use_mcc(short const *skulls, int count);
/* a skull's picture from the set's chosen source, its visible content fitted with its proportions kept and centred in the slot */
void ae_pve_menus_ui_skull_picture(int skull, int use_mcc, float x, float y, float width, float height, unsigned int tint);
/* one of Content Evolved's pictures by its name in the table (a skull's key, "medal_" and a medal's key) as an image id (0: none) */
int ae_pve_menus_ui_ce_picture(const char *name);
/* text centred on a line (size in u) */
void ae_pve_menus_ui_text(struct ae_flow_screen const *flow, int font, float size_u, float x, float center_y, int align,
	unsigned int color, const char *text);
/* text wrapped in width into at most lines lines from center_y down at pitch_u; the lines drawn */
int ae_pve_menus_ui_paragraph(struct ae_flow_screen const *flow, float size_u, float pitch_u, float x, float center_y, float width,
	int lines, unsigned int color, const char *text);
/* the right-hand panel's x and width for the flow */
void ae_pve_menus_ui_side(struct ae_flow_screen const *flow, float *x, float *width);
/* a panel's plate (u from the frame's top) */
void ae_pve_menus_ui_plate(struct ae_flow_screen const *flow, float x, float width, float top_u, float height_u);
/* the footer's prompts for a screen with only Back: a prompt list with the given extra ones first */
void ae_pve_menus_ui_back_prompt(struct ae_flow_screen *flow);


/* ---------- a screen of rows with a right-hand panel (SKULL RULES, SCORING, CO-OP OPTIONS): one class for all three */

struct ae_pvem_rows
{
	struct ae_flow_screen flow;
	/* the log's name ("skull rules") and the screen's title and crumb */
	const char *name, *title;
	char crumb[64];
	/* fills flow.rows and flow.count (called every frame, from the settings as they are) */
	void (*build)(struct ae_pvem_rows *rows);
	/* left (-1), right and A (1) on a live row */
	void (*step)(struct ae_pvem_rows *rows, int row, int direction);
	/* the A prompt's word for a row (NULL: "Change") */
	const char *(*a_word)(struct ae_pvem_rows *rows, int row);
	/* X and Y (NULL: no such prompt), their words */
	void (*x_action)(struct ae_pvem_rows *rows);
	const char *x_word;
	/* the right-hand panel, from x for width */
	void (*side)(struct ae_pvem_rows *rows, float x, float width);
	void *user;
	short hit_base;
	int dialog_pending;
	void (*dialog_open)(struct ae_pvem_rows *rows);
	/* set by build: no X prompt for now (nothing to act on) */
	short x_hidden;
	/* a panel behind the rows, for a list whose headings sit on the bare backdrop */
	short backing;
	/* Y (NULL: no such prompt), its word; y_hidden set by build: no Y prompt for now */
	void (*y_action)(struct ae_pvem_rows *rows);
	const char *y_word;
	short y_hidden;
};
/* pushes the screen with the data as the caller filled it (flow.hit_* are set here); 0 when the stack is full */
int ae_pve_menus_rows_open(struct ae_pvem_rows *rows);
/* a screen of these already open: closed */
void ae_pve_menus_rows_close(struct ae_pvem_rows *rows);
/* the flow's focus moved to the first row that is not a header */
void ae_pve_menus_rows_focus_first(struct ae_pvem_rows *rows);

#endif
