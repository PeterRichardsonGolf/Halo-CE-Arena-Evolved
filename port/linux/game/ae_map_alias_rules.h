/*
AE_MAP_ALIAS_RULES.H

The converted copies of the Social Map Pack and Pro Edition maps (<file>@smp, <file>@pro) and the Steam Workshop items they were
made from (<file>@ws<item id>): which old name a Workshop map had. Data kept under the old name (a layout file beside a map,
a picture) is looked up under it when the Workshop map's own name has none. Pure code: port/linux/tests/ae_map_alias_rules_test.c
runs it.
*/

#ifndef __AE_MAP_ALIAS_RULES_H
#define __AE_MAP_ALIAS_RULES_H

/* the converted copy's name ("warren@smp") of a Workshop map's identity ("warren_mcc debug flycam pm@ws2900072240"), any case;
   NULL when the map has none */
char const *ae_map_alias_old_name(char const *identity);
/* the Workshop identity of a converted copy's name, NULL when it has none */
char const *ae_map_alias_new_name(char const *old_name);
/* the file part of the old name ("warren" of "warren@smp"), lowercase, into file; 0 when the identity has no old name or the
   file does not fit */
int ae_map_alias_old_file(char const *identity, char *file, int size);
/* how many pairs the table holds */
int ae_map_alias_count(void);
/* the pair at index: old name and identity; 0 past the end */
int ae_map_alias_get(int index, char const **old_name, char const **identity);

#endif
