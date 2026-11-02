/*
AE_MEDAL_PACK_RULES.H

The medal pictures' files, as pure rules (port/linux/tests/ae_medal_pack_rules_test.c runs them): the picture pack's index and
its pixels, the career list's medal rows, the pictures' block compression, and a medal's name from its key. The files are the
player's own, read from their MCC install (ae_medal_pack.c); nothing of them ships with the game. Every length and offset in
them is checked against the size of the file it sits in: a file that does not fit gives no picture, never a read past its end.
*/

#ifndef __AE_MEDAL_PACK_RULES_H
#define __AE_MEDAL_PACK_RULES_H

#include <stddef.h>

/* the index file (medalstexturepack.perm.bin): a header of 0x40 bytes, then one record of 480 bytes for each picture, in a row
that ends at the first record whose name does not start with MEDAL_ */
#define AE_MEDAL_INDEX_MAGIC 0xCDBFA090u
#define AE_MEDAL_INDEX_FIRST 0x40u
#define AE_MEDAL_INDEX_STRIDE 480u
/* the bytes of a record that are read: the last record of a file need hold no more than these */
#define AE_MEDAL_INDEX_USED 0x54u
#define AE_MEDAL_INDEX_MOST 1024
/* the pixels' file (medalstexturepack.temp.bin): a header of 16 bytes, then the pictures' blocks */
#define AE_MEDAL_PIXELS_MAGIC 0x5E73CDD7u
#define AE_MEDAL_PIXELS_HEADER 16u
/* a picture: a whole number of 4 by 4 blocks, at most this many pixels each way (the texture is this square) */
#define AE_MEDAL_PICTURE_MOST 128
#define AE_MEDAL_NAME_SIZE 48

struct ae_medal_record
{
	char name[AE_MEDAL_NAME_SIZE];	/* MEDAL_OPENSEASON */
	unsigned int width, height;
	unsigned int size;				/* its bytes in the pixels' file: one byte a pixel, 16 to a block */
	unsigned long offset;			/* where, from the start of that file */
};

/* how many records the index holds: 0 for a file that is too short or not an index */
int ae_medal_index_count(unsigned char const *index, size_t index_size);
/* record number i (0 up to the count): 1 when it is read. Its name is ended and holds printable characters only */
int ae_medal_index_record(unsigned char const *index, size_t index_size, int i, struct ae_medal_record *record);
/* the first record whose name is a career list's icon name (Medal_OpenSeason reads MEDAL_OPENSEASON): the case of the letters and
the underscores are left out of the match. -1 for none */
int ae_medal_index_find(unsigned char const *index, size_t index_size, char const *icon);

/* whether the pixels' file is one (its header) */
int ae_medal_pixels_valid(unsigned char const *pixels, size_t pixels_size);
/* whether a record's picture can be drawn: a size this reader decodes, its blocks wholly inside the pixels' file */
int ae_medal_record_fits(struct ae_medal_record const *record, size_t pixels_size);

/* a picture's blocks (BC3: 8 bytes of alpha, 8 of colour to each 4 by 4) as width * height pixels of r, g, b, a, row by row from
the top. 0 when the size is not width * height bytes or a side is not a whole number of blocks; nothing is written then */
int ae_medal_decode_bc3(unsigned char const *blocks, size_t size, unsigned int width, unsigned int height,
	unsigned char *rgba);

/* the career list's medal row with this id (medal_open_season): its small icon's name. 1 when found, with the name whole in
icon; 0 for none, or a name that does not fit. The list is a text of any length, not ended: only xml_size bytes are read */
int ae_medal_career_icon(char const *xml, size_t xml_size, char const *id, char *icon, size_t icon_size);

/* a name from a medal's key: medal_open_season reads Open Season (upper: OPEN SEASON). Written whole into out, ended, cut short
if it does not fit; returns the length written. A key of none gives an empty name */
int ae_medal_name_from_key(char const *key, char *out, size_t out_size, int upper);

#endif /* __AE_MEDAL_PACK_RULES_H */
