/*
AE_CAMPAIGN_SAVE_RULES.H

The name of a custom campaign's saved game file (a campaign of an MCC package of mcc_maps): its own file in the player's
profile folder beside the stock campaign's savegame.bin, so no campaign's saved game ever takes another's place. The
name is savegame-<package>.bin, the package folder's name written so that two folders never share a file:
lower-case letters, digits and '-' as they are, every other byte '_' and its two lower-case hex digits (so an upper-case
letter, a dot or a separator never gets to the file system as itself). A name that would be longer than
AE_CAMPAIGN_SAVE_NAME_KEPT keeps its first part and ends in "_h" and the whole name's 32-bit FNV-1a hash ('h' is no hex
digit: such a name is never another folder's plain one). A file name valid on Windows too: it starts "savegame-" (no
reserved device name) and ends ".bin". Pure (port/linux/tests/ae_campaign_save_rules_test.c).
*/

#ifndef __AE_CAMPAIGN_SAVE_RULES_H
#define __AE_CAMPAIGN_SAVE_RULES_H

enum
{
	/* the written package name's most characters before it is cut and hashed */
	AE_CAMPAIGN_SAVE_NAME_KEPT = 40,
	/* "savegame-" + the written name (at most 40) + ".bin" + the end */
	AE_CAMPAIGN_SAVE_FILE_SIZE = 64
};

/* the file of a package folder's campaign (file: size bytes, AE_CAMPAIGN_SAVE_FILE_SIZE will do): 1, or 0 with file ""
for no package (NULL or "") or a file too small */
int ae_campaign_save_file_name(const char *package, char *file, int size);

#endif
