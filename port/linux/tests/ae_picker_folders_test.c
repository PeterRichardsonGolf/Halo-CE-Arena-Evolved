#include <stdio.h>
#include <string.h>
#include "ae_picker_folders.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* (static: a store is about 550 KB) */
static struct ae_picker_folders store, again;
static char text[AE_PF_TEXT];

int main(void)
{
	char name[AE_PF_NAME], id[AE_PF_ID];
	short infection, comp, index;
	int count;

	/* a new store: FAVOURITES alone */
	ae_pf_init(&store, AE_PF_MAPS);
	CHECK(store.folder_count == 1 && !strcmp(ae_pf_name(&store, AE_PF_FAVOURITES), "Favourites"));
	CHECK(!ae_pf_name(&store, 1)[0] && !ae_pf_name(&store, -1)[0]);

	/* names: 1-32 of A-Z a-z 0-9 space - _ ', no space at an end, unique letters' case aside */
	CHECK(ae_pf_name_check(&store, "Infection maps", -1) == AE_PF_NAME_OK);
	CHECK(ae_pf_name_check(&store, "Bob's_maps-2", -1) == AE_PF_NAME_OK);
	CHECK(ae_pf_name_check(&store, "", -1) == AE_PF_NAME_EMPTY && ae_pf_name_check(&store, NULL, -1) == AE_PF_NAME_EMPTY);
	CHECK(ae_pf_name_check(&store, "   ", -1) == AE_PF_NAME_EMPTY);
	CHECK(ae_pf_name_check(&store, " Lead", -1) == AE_PF_NAME_CHARACTERS);
	CHECK(ae_pf_name_check(&store, "Maps!", -1) == AE_PF_NAME_CHARACTERS);
	CHECK(ae_pf_name_check(&store, "a/b", -1) == AE_PF_NAME_CHARACTERS);
	CHECK(ae_pf_name_check(&store, "favourites", -1) == AE_PF_NAME_TAKEN);
	CHECK(ae_pf_name_check(&store, "123456789012345678901234567890123", -1) == AE_PF_NAME_LONG);
	CHECK(ae_pf_name_check(&store, "12345678901234567890123456789012", -1) == AE_PF_NAME_OK);

	/* folders A-Z after FAVOURITES; the new name's number */
	ae_pf_new_name(&store, name, sizeof(name));
	CHECK(!strcmp(name, "New Folder"));
	comp = ae_pf_folder_add(&store, "Comp practice");
	infection = ae_pf_folder_add(&store, "Infection maps");
	CHECK(comp == 1 && infection == 2);
	CHECK(ae_pf_folder_add(&store, "aardvark") == 1 && ae_pf_find(&store, "COMP PRACTICE") == 2);
	CHECK(ae_pf_folder_add(&store, "INFECTION MAPS") == -1 && ae_pf_folder_add(&store, "bad!") == -1);
	CHECK(ae_pf_folder_add(&store, "New Folder") > 0);
	ae_pf_new_name(&store, name, sizeof(name));
	CHECK(!strcmp(name, "New Folder 2"));

	/* entries: in several folders, once a folder */
	comp = ae_pf_find(&store, "Comp practice");
	infection = ae_pf_find(&store, "Infection maps");
	CHECK(ae_pf_put(&store, AE_PF_FAVOURITES, "bloodgulch") == 1 && ae_pf_put(&store, comp, "bloodgulch") == 1);
	CHECK(ae_pf_put(&store, comp, "bloodgulch") == 0 && ae_pf_put(&store, infection, "ratrace@ce") == 1);
	CHECK(ae_pf_put(&store, 99, "x") == -1 && ae_pf_put(&store, comp, "") == -1 && ae_pf_put(&store, comp, " x") == -1);
	CHECK(ae_pf_has(&store, comp, "bloodgulch") && !ae_pf_has(&store, infection, "bloodgulch"));
	CHECK(ae_pf_count(&store, AE_PF_FAVOURITES) == 1 && ae_pf_count(&store, comp) == 1);

	/* a rename moves the folder and its entries with it */
	index = ae_pf_folder_rename(&store, comp, "Zulu");
	CHECK(index == store.folder_count - 1 && ae_pf_has(&store, index, "bloodgulch"));
	CHECK(ae_pf_folder_rename(&store, AE_PF_FAVOURITES, "Mine") == -1);
	CHECK(ae_pf_folder_rename(&store, index, "zulu") == index);    /* (its own name, another case) */
	CHECK(!strcmp(ae_pf_name(&store, index), "zulu"));
	CHECK(ae_pf_folder_rename(&store, index, "Infection maps") == -1);

	/* a delete takes its entries, not the same ids elsewhere */
	CHECK(ae_pf_folder_delete(&store, index) && ae_pf_has(&store, AE_PF_FAVOURITES, "bloodgulch"));
	CHECK(!ae_pf_folder_delete(&store, AE_PF_FAVOURITES) && ae_pf_find(&store, "zulu") < 0);
	infection = ae_pf_find(&store, "Infection maps");
	CHECK(ae_pf_has(&store, infection, "ratrace@ce"));
	CHECK(ae_pf_take(&store, infection, "ratrace@ce") == 1 && ae_pf_take(&store, infection, "ratrace@ce") == 0);

	/* the file: written and read back the same */
	ae_pf_put(&store, infection, "hangemhigh");
	CHECK(ae_pf_write(&store, text, sizeof(text)) > 0);
	CHECK(strstr(text, "# AE folders (maps)\nfolder = Favourites\nmap = bloodgulch\n") == text);
	CHECK(ae_pf_read(&again, AE_PF_MAPS, text) && again.folder_count == store.folder_count && !again.dropped);
	CHECK(ae_pf_has(&again, ae_pf_find(&again, "Infection maps"), "hangemhigh"));
	CHECK(ae_pf_write(&store, text, 20) == -1);

	/* reading: CRLF, spaces, a bad name dropped with its entries; a refused file empty */
	CHECK(ae_pf_read(&again, AE_PF_GAMETYPES,
		"\xEF\xBB\xBF# x\r\n folder = Favourites \r\ngametype = AE SND\r\n\r\nfolder = bad!\r\ngametype = AE BOMB\r\n"
		"folder = LAN night\ngametype = saved:BR CTF 5CAP\ngametype = saved:BR CTF 5CAP\n"));
	CHECK(again.folder_count == 2 && again.dropped == 3 && ae_pf_has(&again, 0, "AE SND"));
	CHECK(ae_pf_has(&again, 1, "saved:BR CTF 5CAP") && ae_pf_count(&again, 1) == 1);
	CHECK(!ae_pf_read(&again, AE_PF_GAMETYPES, "map = bloodgulch\n") && again.folder_count == 1);
	CHECK(!ae_pf_read(&again, AE_PF_MAPS, "folder = Favourites\ngametype = x\n") && !again.entry_count);
	CHECK(!ae_pf_read(&again, AE_PF_MAPS, "\x01\x02 junk\n"));
	CHECK(ae_pf_read(&again, AE_PF_MAPS, "") && again.folder_count == 1);

	/* the limits: 64 folders, 500 entries a folder */
	ae_pf_init(&store, AE_PF_MAPS);
	for (count = 1; count < AE_PF_FOLDERS; count++)
	{
		snprintf(name, sizeof(name), "F%02d", count);
		CHECK(ae_pf_folder_add(&store, name) > 0);
	}
	CHECK(ae_pf_folder_add(&store, "One more") == -1);
	for (count = 0; count < AE_PF_FOLDER_ENTRIES; count++)
	{
		snprintf(id, sizeof(id), "map%d", count);
		if (ae_pf_put(&store, 1, id) != 1)
			break;
	}
	CHECK(count == AE_PF_FOLDER_ENTRIES && ae_pf_put(&store, 1, "map-one-more") == -1);
	CHECK(ae_pf_write(&store, text, sizeof(text)) > 0);

	if (failures) return 1;
	printf("ae_picker_folders: ok\n");
	return 0;
}
