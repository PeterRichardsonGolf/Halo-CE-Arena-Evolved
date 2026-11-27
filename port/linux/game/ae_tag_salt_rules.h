/*
AE_TAG_SALT_RULES.H

The salt of a tag added to a map's tag table (the weapon packs and the weapon cache, ae_weapon_pack.c; the menus' tags,
menu_tags.c). Unit test: port/linux/tests/ae_tag_salt_rules_test.c.

A tag index is the tag's salt (its high 16 bits) and its place in the table (its low 16 bits). The engine's data arrays
give salts from 0x8000 up, and back to 0x8000 past 0xffff (data.c, datum_new). A map's tags have the salts its tool
gave them: a Custom Edition or MCC map's run 0xe174 up, one a tag, so a map of more than about 7,800 tags has none
left below 0xffff. Added tags take the salts after the table's highest, back to 0x8000 past 0xfffe, as the data arrays
do. No two tags share a tag index: each has its own place in the table.
*/

#ifndef AE_TAG_SALT_RULES_H
#define AE_TAG_SALT_RULES_H

enum
{
	/* the salts an added tag takes: AE_TAG_SALT_FIRST to AE_TAG_SALT_LAST (0xffff left out: with a place of 0xffff
	it would be NONE) */
	AE_TAG_SALT_FIRST = 0x8000,
	AE_TAG_SALT_LAST = 0xfffe,
};

/* the salt of the added tag after added ones before it, the table's highest salt highest (0 to 0xffff) */
long ae_tag_salt_after(long highest, long added);

#endif
