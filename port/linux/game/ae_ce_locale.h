/*
AE_CE_LOCALE.H

Arena Evolved: OpenCE's Custom Edition loc.map strings for the PC menus
(display.menu_language "maps", menu_tags.c), which OpenCE reads with its
own Custom Edition layer (custom_edition_cache.c). Arena Evolved's Custom
Edition maps are ChupathingyCE's loader's, which has no such reader yet, so
this answers that there is no loc.map: the menus stay in English.
*/

#ifndef __AE_CE_LOCALE_H
#define __AE_CE_LOCALE_H

/* the strings of one unicode string list tag of loc.map, as OpenCE's: each
string's UTF-16 characters, its terminator included, lengths[index] of them
from strings[index], one block released with free() */
struct custom_edition_string_list
{
	long count;
	unsigned short const **strings;
	long *lengths;
};

/* FALSE: no loc.map to read (see above) */
boolean custom_edition_locale_open(void);
void custom_edition_locale_close(void);
/* NULL: none */
struct custom_edition_string_list *custom_edition_locale_strings(char const *name);

#endif
