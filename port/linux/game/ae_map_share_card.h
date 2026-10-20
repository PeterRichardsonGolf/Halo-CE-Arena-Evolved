/*
AE_MAP_SHARE_CARD.H

Arena Evolved: the map downloads' card (look A of the map offer mockups): a card in the lobby's header column that
says which map is coming and how much of it is here, or that it is ready or did not come. ae_map_share_all.c decides
what it says (the guest's state); this draws it with AE's renderer (ae_draw.h, ae_map_share_card.c), in layout
units. A file of its own: AE's layout header (ae_layout.h) and the renderer's of the same name cannot be included
together.
*/

#ifndef __AE_MAP_SHARE_CARD_H
#define __AE_MAP_SHARE_CARD_H

/* what the card says */
struct ae_map_share_card
{
	char label[32];
	char counter[16];
	char name[80];
	char amount[48];
	char line[200];
	/* (the maps a done card names, when it names several: how many are here to name, and how many came) */
	char names[6][40];
	int name_count;
	int name_total;
	int has_bar;
	float progress;
	/* (did not download: its label is white) */
	int failed;
};

/* whether there is a renderer to draw with (none on the dedicated server) */
int ae_map_share_card_available(
	void);
/* the card at alpha (0 to 1), in the frame being drawn */
void ae_map_share_card_draw(
	struct ae_map_share_card const *card,
	float alpha);

#endif
