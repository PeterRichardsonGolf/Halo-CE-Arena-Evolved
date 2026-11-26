/*
AE_PAK_RULES.H

Reading entries of an Unreal Engine 4 pak file (version 7) as pure rules (port/linux/tests/ae_career_pak_test.c runs them): the footer
at the file's end names the index; the index (not encrypted) lists every entry's name, place and size. Only plain entries are taken
(no compression, no encryption): a file that is cut short, changed by an update or not what it should be gives no entry, never a
read past a buffer or past the file. The caller reads the footer and the index, then only the entries it wants: a pak is many GB.
Nothing of a player's pak ships with the game.
*/

#ifndef __AE_PAK_RULES_H
#define __AE_PAK_RULES_H

#include <stddef.h>

enum
{
	AE_PAK_FOOTER = 61,
	/* the entry record that starts each entry's data in the file (version 7, not compressed) */
	AE_PAK_RECORD = 53,
	AE_PAK_VERSION = 7,
	/* the most entries and the largest index read */
	AE_PAK_MOST_ENTRIES = 1000000,
	AE_PAK_MOST_INDEX = 64 * 1024 * 1024
};

struct ae_pak_entry
{
	/* where the entry's data starts in the file (after its record) and its length */
	unsigned long long data_offset, size;
};

/* the footer's last AE_PAK_FOOTER bytes of a file of file_size bytes: 1 with the index's place; 0 for a footer that is not a version 7
   pak's, an encrypted index, or an index outside the file */
int ae_pak_footer(unsigned char const *footer, size_t footer_size, unsigned long long file_size, unsigned long long *index_offset,
	unsigned long long *index_size);
/* the entry of a path (as the index names it, case as written) in the index's bytes: 1 with its place, 0 for none, an entry that is
   compressed or encrypted, or one whose data is not inside the file */
int ae_pak_index_find(unsigned char const *index, size_t index_size, unsigned long long file_size, char const *path,
	struct ae_pak_entry *entry);
/* the AE_PAK_RECORD bytes read at an entry's start (data_offset - AE_PAK_RECORD) agree with the index: not compressed, this size */
int ae_pak_record_ok(unsigned char const *record, size_t record_size, unsigned long long entry_size);

#endif
