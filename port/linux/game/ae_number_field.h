/*
AE_NUMBER_FIELD.H

The typed whole-number fields of the AE menus, in one place (0.7.6; no engine includes, unit test:
port/linux/tests/ae_number_field_test.c). A range says what may be typed into a row (SETTINGS' FOV, GAME OPTIONS'
KILLS TO WIN ...): the least and the most, the unit words that follow the number, and the word a zero stands for. The
rows keep their lists (every list value stays reachable); a typed number is one more value, held to the range.

GAME OPTIONS' rows are stored in the gametype's own fields (a long, a short, a byte); an option's entry names the
display unit's size in the stored one and the stored field's greatest value, and the unit test holds every range to
it: nothing typed can be a value the field cannot take.
*/

#ifndef __AE_NUMBER_FIELD_H
#define __AE_NUMBER_FIELD_H

#include <stddef.h>

/* zero_word: what a typed 0 reads as ("UNLIMITED"), NULL when 0 is not a value of the row (it is held to the least);
unit_one / unit_many: the words after the number ("MINUTE" / "MINUTES"), NULL for none */
struct ae_number_range
{
	long minimum, maximum;
	const char *unit_one, *unit_many, *zero_word;
};

enum { AE_NUMBER_DIGITS = 4 };

/* whether a text may stand in a number field as it is typed: digits only, at most digits of them (empty too) */
int ae_number_typing_ok(const char *typed, int digits);
/* the typed text's value (digits only, 1 to 9 of them); 0 when it has none */
int ae_number_parse(const char *typed, long *value);
/* held to the range; 0 stays 0 when the range has a zero word */
long ae_number_clamp(struct ae_number_range const *range, long value);
/* the value as shown: the zero word for 0, else the number and its unit word ("1 MINUTE", "10 MINUTES", "35") */
void ae_number_format(struct ae_number_range const *range, long value, char *text, size_t size);
/* "5 TO 500", with the zero word's "0 FOR UNLIMITED" ahead when the range has one */
void ae_number_hint(struct ae_number_range const *range, char *text, size_t size);

/* GAME OPTIONS' rows that take a typed number (ae_setup_options.h's AE_SO_*): the range in display units, scale: the
stored value per display unit (RESPAWN TIME is stored in ticks, 30 to a second), field_maximum: the stored field's
greatest value. 0 for a row that does not */
struct ae_number_option { struct ae_number_range range; long scale, field_maximum; };
int ae_number_option(int option, struct ae_number_option *out);

#endif
