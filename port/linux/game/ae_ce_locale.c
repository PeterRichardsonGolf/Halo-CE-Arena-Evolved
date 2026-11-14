/*
AE_CE_LOCALE.C

Arena Evolved: the stand-in of OpenCE's Custom Edition loc.map strings
(ae_ce_locale.h): none, so display.menu_language "maps" shows English.
*/

#include "cseries/cseries.h"
#include "ae_ce_locale.h"

boolean custom_edition_locale_open(
	void)
{
	return FALSE;
}

void custom_edition_locale_close(
	void)
{
	return;
}

struct custom_edition_string_list *custom_edition_locale_strings(
	char const *name)
{
	(void)name;
	return NULL;
}
