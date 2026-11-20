/*
AE_SECTION_FOLDS.H

Which sections of CHANGE MAP's open folders the player folded (0.7.6): a folder opens into labelled sections, each
folds with A and keeps its choice, as RECENT does. One list for the session, text in and text out; the glue
(ae_glue_picker_stores.c) keeps it in section-folds.txt of the private folder. Pure C, no engine includes (unit test:
port/linux/tests/ae_section_folds_test.c).

Format: UTF-8, '#' starts a comment line, one folded section a line: <picker><TAB><folder>/<section>. A line that
does not fit is skipped. The key is the folder's name and the section's, so OTHER of two folders are two sections.
*/

#ifndef __AE_SECTION_FOLDS_H
#define __AE_SECTION_FOLDS_H

enum { AE_SECTION_FOLDS_MAXIMUM = 256, AE_SECTION_FOLDS_KEY = 96, AE_SECTION_FOLDS_PICKER = 12,
	AE_SECTION_FOLDS_TEXT = 32 * 1024 };

struct ae_section_folds
{
	short count;
	char picker[AE_SECTION_FOLDS_MAXIMUM][AE_SECTION_FOLDS_PICKER];
	char key[AE_SECTION_FOLDS_MAXIMUM][AE_SECTION_FOLDS_KEY];
};

void ae_section_folds_init(struct ae_section_folds *folds);
/* text in: 1 when read (bad lines skipped), 0 for a NULL */
int ae_section_folds_read(struct ae_section_folds *folds, const char *text);
/* text out: its length, -1 when it does not fit */
long ae_section_folds_write(struct ae_section_folds const *folds, char *buffer, long size);
/* whether the picker's section is folded */
int ae_section_folds_get(struct ae_section_folds const *folds, const char *picker, const char *key);
/* folds or opens it; 1 when that changed the list, 0 when it was so already or the list is full */
int ae_section_folds_set(struct ae_section_folds *folds, const char *picker, const char *key, int folded);
/* the key of a folder's section ("<folder>/<section>"), cut to fit */
void ae_section_folds_key(char *key, int size, const char *folder, const char *section);

#endif
