/*
AE_SCREEN_RECORD_MEDALS.H

SERVICE RECORD's MEDALS page (0.8.1): every kind of medal of one player in a grid four wide, the ones not earned yet dimmed
with a count of 0, opened from the record page's MEDALS pane (ae_screen_record.h). Its rules are pure
(ae_record_medals_rules.h); its measures are the record page's 1920 x 1080 design units.

- The head: SERVICE RECORD and MEDALS, the player with the YOU or RIVAL tag, "216 MEDALS IN 7 GAMES ON THIS PC".
- The pane: "MEDALS 36 OF 118 KINDS" (the kinds earned of all of them) and the cells shown ("1-32 OF 118"); the
  CATEGORY chips (ALL, MULTIKILL, SPREE, SKILL, OBJECTIVE, each with its kinds earned) and the sort's word (RAREST FIRST
  or MOST EARNED); the grid, its last row under the fade while more follow, "N more" and the scrollbar; the detail strip
  of the focused medal (its category, its line of what earns it where the install's career list has one, its count and
  its points, or NOT EARNED YET).
- Pad and keys: the d-pad, the stick or the arrows move the cursor a cell (no wrap at the row ends); LT / RT (PgUp /
  PgDn) seven rows; LB / RB (Q / E) the category, round, back to the top; X (F) the sort, the cursor kept on its medal; B
  (Esc) back to the record page. A does nothing.
- Mouse: the hover wash on a cell and the detail strip follows it; a click puts the cursor there; the wheel scrolls a
  row a notch; the thumb drags and a click on the track pages; a click on a chip, the sort word or a prompt as its press.

The cursor, the category, the sort and the scroll are kept while the record page stays on the stack.

Log: "ae record: medals page (<kinds> of <all> kinds)" and "ae record: medals page closed".
*/

#ifndef __AE_SCREEN_RECORD_MEDALS_H
#define __AE_SCREEN_RECORD_MEDALS_H

struct ae_service_record;

/* opens the page of the player called name in record over the stack (viewer: the local player, for the YOU tag); 0 when
the stack is full or it is open. record must stay while the page is open */
int ae_screen_record_medals_open(short owner, struct ae_service_record const *record, char const *name, char const *viewer);
/* the record page left or opened again: the next open starts afresh (the first cell, ALL, RAREST FIRST) */
void ae_screen_record_medals_forget(void);
/* the page off the stack at once, when it is open (DEV MODE's record_medals) */
void ae_screen_record_medals_remove(void);

#endif
