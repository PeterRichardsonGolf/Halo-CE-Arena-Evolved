/*
AE_SPAWNER_TEXT.H

port: AE weapon spawners (0.7.8). The line reader the spawner text files share (a weapon set file,
ae_weaponset_rules.h; a map's spawner sidecar, ae_spawner_file_rules.h): the layout reader's rules
(ae_layout_rules.c, copied so these pure files stand alone), lines of words split on blanks, `#` to the end of a
line a comment, a quoted word may hold blanks, limits as the layout's (64 KB, 512 lines, 200 characters a line). No
engine includes.
*/

#ifndef __AE_SPAWNER_TEXT_H
#define __AE_SPAWNER_TEXT_H

#include "ae_pure_types.h"

#define AE_SPAWNER_TEXT_MAXIMUM_FILE_SIZE 65536
#define AE_SPAWNER_TEXT_MAXIMUM_LINES 512
#define AE_SPAWNER_TEXT_MAXIMUM_LINE 200
#define AE_SPAWNER_TEXT_MAXIMUM_WORDS 48

struct ae_spawner_text
{
	char const *text;
	long length, position;
	int line_number; /* of the line in `words` (1 = the first line of the file) */
	char line[AE_SPAWNER_TEXT_MAXIMUM_LINE + 2];
	char *words[AE_SPAWNER_TEXT_MAXIMUM_WORDS];
	int count;
};

/* starts over a text; `ae_spawner_text_next` then yields each line that has words: 1 a line (words, count,
line_number set), 0 the end, -1 refused (the error says why: a file too big, too many lines, a line too long, a NUL, a
quote not closed) */
void ae_spawner_text_begin(struct ae_spawner_text *reader, char const *text, long length);
int ae_spawner_text_next(struct ae_spawner_text *reader, char *error, int error_size);

/* word helpers: equal case aside; a whole number in [low, high]; a real; an 8-hex-digit hash */
int ae_spawner_text_same(char const *a, char const *b);
int ae_spawner_text_whole(char const *word, long low, long high, long *value);
int ae_spawner_text_real(char const *word, real *value);
int ae_spawner_text_hash(char const *word, unsigned long *value);
/* a name word: 1 to `size - 1` characters of [A-Za-z0-9_:.-], copied */
int ae_spawner_text_name(char const *word, char *out, int size);
/* writes `text` and, when given, ` "word"` into error */
void ae_spawner_text_say(char *error, int error_size, char const *text, char const *word);

#endif /* __AE_SPAWNER_TEXT_H */
