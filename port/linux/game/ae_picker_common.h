/*
AE_PICKER_COMMON.H

What CHANGE MAP and CHANGE GAMETYPE share (ae_picker_common.c; AE menus Phase 2, the
approved design):

- the search field on the chip line's right (Y, keyboard R): where it goes (beside the chips' first line, never under
  320 u), idle (the Y mark and SEARCH), typing (a keyboard: the ring, the caret, ESC CLEARS) or AE's keyboard (a pad);
- the folder menu (X, keyboard F): a popover over the row, titled with the item's name: FAVOURITES and the player's
  folders with a tick on each the item is in (A puts it in or takes it out, the menu stays open), NEW FOLDER... (its
  name typed in place: a keyboard's keys, or AE's keyboard beside it) and, in one of the player's folders, REMOVE FROM
  it; on a folder row of the player's own, RENAME and DELETE, the delete asked again with KEEP IT first. A popover
  edge never cuts a row's words behind it: it moves to the middle of that row's gap;
- an empty folder's line, its X mark drawn in the words.

Positions are layout units of the whole frame (FULL density only); sizes u. Menus' thread only. No engine includes.

Log (with the screen's prefix, "ae change map" / "ae change gametype"): "typing", "keyboard open", "folder menu
'<item>'", "folder '<name>' made", "folder '<name>' renamed '<new>'", "folder '<name>' deleted", "folder name refused
(<why>)".
*/

#ifndef __AE_PICKER_COMMON_H
#define __AE_PICKER_COMMON_H

#include "ae_widgets.h"

enum { AE_PICKER_QUERY = 32 };

/* ---------- the search field */

struct ae_picker_search
{
	struct ae_text text;
	struct ae_field_edit edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	struct ae_rect well;            /* as last drawn (layout units) */
	const char *log;                /* the screen's log prefix */
};

/* the search's text (initial: "" none) */
void ae_picker_search_init(struct ae_picker_search *search, const char *log, const char *initial);
/* Y / R: a keyboard types into the field (menu keys off while it does), a pad gets AE's keyboard over bounds */
void ae_picker_search_begin(struct ae_picker_search *search, int device, struct ae_density const *density,
	struct ae_rect const *bounds, void const *holder);
/* the typing ended (keep: the text stays; else cleared) */
void ae_picker_search_end(struct ae_picker_search *search, int keep);
void ae_picker_search_clear(struct ae_picker_search *search);
/* this frame's typed keys (while typing); 0 when one was refused (the caller plays failure) */
int ae_picker_search_keys(struct ae_picker_search *search);
/* the screen left: an edit going on ends unchanged */
void ae_picker_search_leave(struct ae_picker_search *search);
static inline int ae_picker_search_editing(struct ae_picker_search const *search)
{
	return search->typing || search->keyboard_open;
}
/* the field's room on the chip line: the chips get room (the field never under 320 u: they wrap first), the field's
left after their first line's end (end: the first line's RB mark's right) or the panel's x, whichever is further */
float ae_picker_search_chip_room(struct ae_density const *density, float chips_x, float right, float rb_width);
float ae_picker_search_left(struct ae_density const *density, float first_line_end, float panel_x);
/* the field from x to right, centred on center_y; hits under hit_id (AE_PART_FIELD) */
void ae_picker_search_draw(struct ae_picker_search *search, struct ae_density const *density, float x, float right,
	float center_y, short hit_id);

/* ---------- the folder menu */

struct ae_folder_menu_spec
{
	int kind;                       /* AE_PF_MAPS, AE_PF_GAMETYPES (ae_picker_folders.h) */
	const char *log;                /* the screen's log prefix */
	const char *item;               /* the title: the map's, the gametype's or the folder's name */
	int actions;                    /* 0: the folders on an item; 1: RENAME / DELETE on a folder of the player's */
	short folder;                   /* actions: that folder; else the folder the row is in (REMOVE FROM), -1 none */
	struct ae_rect row;             /* the row it opens over (layout units) */
	struct ae_rect bounds;          /* the frame (layout units) */
	/* the list behind it (the edge rule): its rows' left, right incl. the scrollbar's strip, the first row's top,
	bottom, rows' height and pitch (layout units) */
	float list_x, list_right, rows_top, rows_bottom, row_height, pitch;
	struct ae_density density;
	/* the item in a folder; put in or taken out (1 in, 0 out, -1 refused); the folders changed (made, renamed,
	deleted: the screen's rows again; folder: the one made or renamed, where it is now, -1 for a delete) */
	int (*in)(short folder, void *context);
	int (*toggle)(short folder, void *context);
	void (*changed)(short folder, void *context);
	void *context;
};
/* opens the menu as a popover over the screen (AE_OWNER_PARENT); 0 when refused */
int ae_folder_menu_open(struct ae_folder_menu_spec const *spec);
int ae_folder_menu_is_open(void);
/* the footer's prompts while it is open (A Add or take out, B Done; A Select, B Back; ... ): their count */
short ae_folder_menu_prompts(struct ae_prompt *prompts, short maximum);

/* ---------- the empty folder's line */

/* "Nothing in this folder yet. Press [X] on a map to put it in one." (gametypes: "on a gametype"), no fill, BODY 21 u
MUTED from x, centred on center_y */
void ae_picker_empty_line(struct ae_density const *density, float x, float center_y, int kind);

#endif
