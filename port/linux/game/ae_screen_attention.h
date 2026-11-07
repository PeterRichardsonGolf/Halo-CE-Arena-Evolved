/*
AE_SCREEN_ATTENTION.H

The attention dialog (0.7.5): a popover over the screen the player is on, an ice top edge, a title, the reason in
plain words, an item block when one thing is named, one or two buttons and a note under them. Join refusals (the
join helper, ae_join_helper.c) and HOST ONLINE with a VPN up use it (mockups-075-toggles/final/6, 7).
*/

#ifndef __AE_SCREEN_ATTENTION_H
#define __AE_SCREEN_ATTENTION_H

#include "ae_widgets.h"

struct ae_attention_spec
{
	char const *title;             /* capitals */
	char const *body;              /* the reason, wrapped */
	char const *strong;            /* a line in white under it ("Turn the VPN off, then host online."), or NULL */
	/* the item block: its title, its two lines and the tile's words ("NOT ON THIS PC"); item_title NULL: none */
	char const *item_title, *item_line1, *item_line2, *item_tile;
	char const *buttons[2];        /* one or two */
	short button_count;
	short focus;                   /* the button focused first */
	short danger;                  /* the button drawn as the destructive secondary (the red's light ink in a red
	                                  frame), -1 none */
	short back;                    /* the button B picks, -1: B picks nothing (picked gets -1) */
	char const *note;              /* under the buttons, muted, or NULL */
	char const *back_label;        /* the footer's B word ("Back", "Stay on LAN") */
	struct ae_density density;
	/* called once, after the dialog is closed, with the button picked (or -1) */
	void (*picked)(short button, void *context);
	void *context;
};

/* opens it over the stack (its strings are copied); a dialog already open is replaced. 0 when the stack is full */
int ae_attention_open(struct ae_attention_spec const *spec);
/* whether one is open */
int ae_attention_is_open(void);

#endif
