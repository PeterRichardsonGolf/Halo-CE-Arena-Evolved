/*
AE_MCC_RANK_RULES.H

The rank pictures of the player's own MCC install, as pure rules (port/linux/tests/ae_career_xp_test.c runs them): its main menu
picture pack (data/ui/texturepacks/mainmenuandcampaigntexturepack) holds a small (52 by 52) and a large (152 by 152) picture of each
rank 1 .. 50, named SMALLRANKICON_001 .. 050 and LARGERANKICON_001 .. 050, in the same index and pixel layout as the medal pack
(ae_medal_pack_rules.h). Every length and offset is checked against the file it sits in. Nothing of MCC's files ships with the game.

MCC's files hold no rank names to read: its career list (careerdb.xml) has none, and its text tables are keyed by hash. A rank shows
as its picture, or as the plain number when there is none.
*/

#ifndef __AE_MCC_RANK_RULES_H
#define __AE_MCC_RANK_RULES_H

#include <stddef.h>

#include "ae_medal_pack_rules.h"

enum
{
	AE_MCC_RANK_FIRST = 1,
	AE_MCC_RANK_LAST = 50,
	/* a rank picture's sides, at most (the large one is 152) */
	AE_MCC_RANK_PICTURE_MOST = 256,
	/* the records of the index that are looked through */
	AE_MCC_RANK_INDEX_MOST = 4096
};

/* the picture's name in the index: SMALLRANKICON_007 (large 0) or LARGERANKICON_007 (large 1); 0 when the rank is out of 1 .. 50 or the
   place is too small */
int ae_mcc_rank_name(int rank, int large, char *out, size_t size);
/* the record of the picture of a rank in the index file; 1 when found, read and its sides are those of a rank picture, else 0 */
int ae_mcc_rank_record(unsigned char const *index, size_t index_size, int rank, int large, struct ae_medal_record *record);
/* whether the record's picture can be decoded: its blocks wholly inside the pixels' file, sides whole blocks, at most the most */
int ae_mcc_rank_record_fits(struct ae_medal_record const *record, size_t pixels_size);

#endif
