/*
AE_FIREFIGHT_VARIANTS_RULES.H

A Firefight variant the player saved, as pure data (no engine includes; tests/ae_firefight_variants_rules_test.c): its name, the
value of each of Firefight's rules (the rows of ae_setup_options.h, in order), the skulls it starts with and its item set. The
saved ones are one text file, a line each (ae_firefight_variants.c); a ninth replaces the oldest. A Firefight variant is never a
multiplayer gametype file, so no multiplayer list holds one.
*/

#ifndef __AE_FIREFIGHT_VARIANTS_RULES_H
#define __AE_FIREFIGHT_VARIANTS_RULES_H

#include <stddef.h>

enum
{
	AE_FFV_NAME = 12,       /* a name's characters with its end: 11 at most, as a gametype's */
	AE_FFV_ROWS = 45,       /* the rules (the glue checks it against AE_SETUP_FF_ROWS) */
	AE_FFV_SAVED = 8,       /* the variants kept */
	AE_FFV_LINE = 1024
};

struct ae_ffv_record
{
	char name[AE_FFV_NAME];
	/* each rule's stored value, in the rows' order (the two rows that open a screen have 0 here) */
	long raw[AE_FFV_ROWS];
	/* the skulls it starts with, the two words of enum skull's bits */
	unsigned long skulls[2];
	/* its item set's hash, 0 for none */
	unsigned long item_set;
};

/* a typed name made fit: printable ASCII, no bar, spaces trimmed and made single, 11 characters at most; 0 when nothing is left */
int ae_ffv_name_clean(const char *typed, char *name, size_t size);
/* two names the same, letters' case aside */
int ae_ffv_name_same(const char *a, const char *b);

/* the record as one line of text (no line end); 1 when it fits */
int ae_ffv_format(struct ae_ffv_record const *record, char *text, size_t size);
/* a line read: 1 when it is a whole record (a hostile line is refused, never half read) */
int ae_ffv_parse(const char *line, struct ae_ffv_record *record);

/* the saved file: its text into records (up to maximum, a bad line left out), and the records as text */
int ae_ffv_file_read(const char *text, struct ae_ffv_record *records, int maximum);
int ae_ffv_file_write(struct ae_ffv_record const *records, int count, char *text, size_t size);

/* a record added to the list: a name already there is replaced in place; else it goes last, and the oldest (the first) is dropped
when the list is full. Returns the new count; dropped (may be NULL) gets 1 when one was dropped */
int ae_ffv_list_add(struct ae_ffv_record *records, int count, struct ae_ffv_record const *record, int *dropped);
/* the record at index taken out: the new count */
int ae_ffv_list_remove(struct ae_ffv_record *records, int count, int index);

#endif
