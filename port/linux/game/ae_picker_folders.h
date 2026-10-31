/*
AE_PICKER_FOLDERS.H

The player's own folders of maps and of gametypes (ae_picker_folders.c), for CHANGE MAP and CHANGE GAMETYPE (plan
.superpowers/sdd/2026-10-27/menus-p2-pickers.md, section 1): one store a kind, kept in a text file in the save root's
private folder (the glue's: ae_glue_picker_stores.c). Pure C, no engine includes (unit test:
port/linux/tests/ae_picker_folders_test.c, under the sanitizers).

- Folder 0 is FAVOURITES, always there; it cannot be renamed or deleted. The others follow it A-Z (letters' case
  aside), so a folder's index changes when one is made or renamed: the calls that do so return the new one.
- A folder's name: 1 to 32 of A-Z a-z 0-9, space, - _ and ', no space at either end, unique letters' case aside.
- An entry is an id (a map: the game's list name, <file>@<family>; a gametype: AE's seeded name, a built-in's name,
  or saved:<stored name> for the player's own). An id can sit in several folders; deleting a folder deletes no map or
  gametype; an id whose map or gametype is gone stays in its folder (the lists hide it).
- At most 64 folders, 500 entries a folder and 8192 entries in all.

The file:

	# AE folders (maps)
	folder = Favourites
	map = bloodgulch
	folder = Infection maps
	map = ratrace@ce

with "gametype = <id>" lines in the gametypes' file. A file the reader cannot take (a line of no known kind, an entry
before any folder) is refused whole: the glue keeps it aside as <file>.bad and starts afresh. Lines past the limits,
a bad folder name, a repeated entry are dropped (counted in dropped), the rest read.
*/

#ifndef __AE_PICKER_FOLDERS_H
#define __AE_PICKER_FOLDERS_H

enum ae_pf_kind { AE_PF_MAPS, AE_PF_GAMETYPES, AE_PF_KINDS };
enum
{
	AE_PF_FOLDERS = 64, AE_PF_FOLDER_ENTRIES = 500, AE_PF_ENTRIES = 8192,
	/* a name: 32 characters and its end */
	AE_PF_NAME = 33, AE_PF_NAME_LETTERS = 32,
	AE_PF_ID = 64,
	AE_PF_FAVOURITES = 0,
	/* the writer's largest text: every entry a line ("gametype = " and an id), the folders' lines, the heading */
	AE_PF_TEXT = AE_PF_ENTRIES * (AE_PF_ID + 12) + AE_PF_FOLDERS * (AE_PF_NAME + 12) + 64
};
/* a name checked (ae_pf_name_check) */
enum ae_pf_name { AE_PF_NAME_OK, AE_PF_NAME_EMPTY, AE_PF_NAME_CHARACTERS, AE_PF_NAME_TAKEN, AE_PF_NAME_LONG };

struct ae_pf_entry { short folder; char id[AE_PF_ID]; };
struct ae_picker_folders
{
	int kind;
	short folder_count;
	char names[AE_PF_FOLDERS][AE_PF_NAME];
	int entry_count;
	struct ae_pf_entry entries[AE_PF_ENTRIES];
	int dropped;                 /* lines the last read dropped */
};

/* an empty store of a kind: FAVOURITES alone */
void ae_pf_init(struct ae_picker_folders *store, int kind);
/* a file's text read into the store (made empty first); 1 when taken, 0 when refused (the store is then empty) */
int ae_pf_read(struct ae_picker_folders *store, int kind, const char *text);
/* the file's text into buffer: its length, or -1 when it does not fit (nothing usable written) */
long ae_pf_write(struct ae_picker_folders const *store, char *buffer, long size);

/* a folder's name ("Favourites" for folder 0), "" out of range */
const char *ae_pf_name(struct ae_picker_folders const *store, short folder);
/* whether name may be a folder's: except, a folder whose own name it may be (a rename), -1 none */
int ae_pf_name_check(struct ae_picker_folders const *store, const char *name, short except);
/* the folder of a name (letters' case aside), -1 none */
short ae_pf_find(struct ae_picker_folders const *store, const char *name);
/* "New Folder", else "New Folder 2", "New Folder 3" ... the first not taken */
void ae_pf_new_name(struct ae_picker_folders const *store, char *name, int size);
/* a folder made: its index (A-Z among the others), -1 refused (the name, or 64 folders) */
short ae_pf_folder_add(struct ae_picker_folders *store, const char *name);
/* a folder renamed (not FAVOURITES): its new index, -1 refused */
short ae_pf_folder_rename(struct ae_picker_folders *store, short folder, const char *name);
/* a folder deleted with its entries (not FAVOURITES): 1 done */
int ae_pf_folder_delete(struct ae_picker_folders *store, short folder);

/* an id in a folder */
int ae_pf_has(struct ae_picker_folders const *store, short folder, const char *id);
/* an id put in a folder: 1 put, 0 there already, -1 refused (no such folder, a bad id, the limits) */
int ae_pf_put(struct ae_picker_folders *store, short folder, const char *id);
/* an id taken out of a folder: 1 taken, 0 it was not there */
int ae_pf_take(struct ae_picker_folders *store, short folder, const char *id);
/* a folder's entries */
short ae_pf_count(struct ae_picker_folders const *store, short folder);

#endif
