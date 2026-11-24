/*
AE_MCC_MAP_MEMO_RULES.H

Whether an MCC map is here (ae_mcc_map_present), remembered a while, as pure
rules (port/linux/tests/ae_mcc_map_memo_rules_test.c runs them;
ae_glue_mcc_maps.c applies them). Finding out opens the map's file and reads
its header; a screen asks for every map of a playlist each frame (the AE
TOURNAMENT card counts a playlist's maps twice a frame), and on Windows each
open of a map file can cost a millisecond or more.

- A NAME NOT KEPT, or kept under another Workshop catalog generation (a rescan
  changed the catalog), is checked.
- FRESH: an answer less than 2 seconds old is used as it is.
- OLDER: an answer from 2 to 10 seconds old is checked again, at most 2 such
  checks a frame (the others use the answer kept), so a screen's whole list is
  checked again over a few frames, never all in one. A frame is one frame of
  the menus' drawing (its number), or 100 ms when the number does not move.
- STALE: an answer 10 seconds old or more is always checked.
- The names are kept in a table of 256, each looked for in 8 places from its
  hash (FNV-1a); when all 8 are taken, the oldest answer there gives way (it is
  checked again the next time). A name too long to keep is always checked.
*/

#ifndef __AE_MCC_MAP_MEMO_RULES_H
#define __AE_MCC_MAP_MEMO_RULES_H

enum
{
	AE_MCC_MAP_MEMO_ENTRIES = 256,
	AE_MCC_MAP_MEMO_PLACES = 8,
	AE_MCC_MAP_MEMO_IDENTITY = 64,
	AE_MCC_MAP_MEMO_FRESH_MILLISECONDS = 2000,
	AE_MCC_MAP_MEMO_STALE_MILLISECONDS = 10000,
	AE_MCC_MAP_MEMO_CHECKS_A_FRAME = 2,
	AE_MCC_MAP_MEMO_FRAME_MILLISECONDS = 100
};

struct ae_mcc_map_memo_entry
{
	char identity[AE_MCC_MAP_MEMO_IDENTITY];
	int used;
	int present;
	unsigned long generation;
	unsigned long time;
};

struct ae_mcc_map_memo
{
	struct ae_mcc_map_memo_entry entries[AE_MCC_MAP_MEMO_ENTRIES];
	/* (the frame whose older answers are being checked again: its number, its start, how many so far) */
	unsigned int frame;
	unsigned long frame_time;
	int frame_known;
	int frame_checks;
};

/* 1 with *present set: the answer kept stands. 0: check the map, then keep the answer (ae_mcc_map_memo_rules_keep).
   now: milliseconds (any clock that wraps as an unsigned long); frame: the menus' frame number */
int ae_mcc_map_memo_rules_find(struct ae_mcc_map_memo *memo, char const *identity, unsigned long generation,
	unsigned long now, unsigned int frame, int *present);
/* where a name's search in the table starts */
int ae_mcc_map_memo_rules_slot(char const *identity);
void ae_mcc_map_memo_rules_keep(struct ae_mcc_map_memo *memo, char const *identity, unsigned long generation,
	unsigned long now, int present);

#endif
