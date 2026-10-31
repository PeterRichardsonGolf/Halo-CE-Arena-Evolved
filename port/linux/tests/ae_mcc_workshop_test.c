#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ae_mcc_workshop.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_mcc_workshop_item item;
static struct ae_mcc_workshop_map_info info;
static char reason[200];

static int read_item(const char *text)
{
	reason[0] = 0;
	return ae_mcc_workshop_read_item(text, strlen(text), &item, reason, sizeof(reason));
}

static int read_map(const char *text)
{
	reason[0] = 0;
	return ae_mcc_workshop_read_map_info(text, strlen(text), &info, reason, sizeof(reason));
}

/* appends count characters to text */
static void pad(char *text, char c, int count)
{
	size_t length = strlen(text);

	memset(text + length, c, (size_t)count);
	text[length + (size_t)count] = 0;
}

static void test_item_good(void)
{
	/* a Halo 1 item with two maps (the id is beyond 32 bits in real items: 3093203188 here) */
	CHECK(read_item("{\"ModIdentifier\":{\"HostedModIds\":{\"SteamWorkshopId\":1234567890}},"
		"\"Title\":{\"Neutral\":\"Test Pack\"},\"Engine\":\"Halo1\","
		"\"GameModContents\":{\"MultiplayerMaps\":[\"multiplayer/a.json\",\"multiplayer/b.json\"]}}"));
	CHECK(!strcmp(item.id, "1234567890") && !strcmp(item.title, "Test Pack"));
	CHECK(item.halo1 == 1 && item.map_count == 2 && !strcmp(item.map_json[0], "multiplayer/a.json"));
	CHECK(!strcmp(item.map_json[1], "multiplayer/b.json"));
	CHECK(read_item("{\"ModIdentifier\":{\"HostedModIds\":{\"SteamWorkshopId\":3093203188}},\"Engine\":\"Halo1\"}"));
	CHECK(!strcmp(item.id, "3093203188") && item.title[0] == 0);
	/* backslashes become '/' */
	CHECK(read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"multiplayer\\\\c.json\"]}}"));
	CHECK(item.map_count == 1 && !strcmp(item.map_json[0], "multiplayer/c.json"));
}

static void test_item_other(void)
{
	CHECK(read_item("{\"Engine\":\"Halo3\",\"Title\":{\"Neutral\":\"x\"}}") && item.halo1 == 0);
	CHECK(read_item("{\"Engine\":\"Halo1\",\"GameModContents\":{\"HasCampaign\":true}}") && item.halo1 == 1 &&
		item.map_count == 0);
	CHECK(read_item("{\"Engine\":\"Halo1\",\"Title\":{\"Neutral\":\"x\"}}") && !strcmp(item.id, ""));
	CHECK(read_item("{}") && item.halo1 == 0 && item.map_count == 0);
}

static void test_item_bad(void)
{
	char *big;
	char text[257 * 24 + 200];
	int index;

	CHECK(!read_item("{\"Engine\":\"Halo1\",\"Title\":") && reason[0]);
	CHECK(!read_item("") && reason[0]);
	CHECK(!read_item("[1,2]") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":5}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[7]}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"../x.json\"]}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"a/../x.json\"]}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"/etc/x.json\"]}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"\\\\x.json\"]}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"C:/x.json\"]}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"a\\u0001b.json\"]}}") && reason[0]);
	CHECK(!read_item("{\"GameModContents\":{\"MultiplayerMaps\":[\"\"]}}") && reason[0]);
	/* a path that does not fit */
	big = malloc(300);
	memset(big, 'a', 200);
	strcpy(big + 200, ".json");
	snprintf(text, sizeof(text), "{\"GameModContents\":{\"MultiplayerMaps\":[\"%s\"]}}", big);
	CHECK(!read_item(text) && reason[0]);
	free(big);
	/* the id: negative, fractional */
	CHECK(!read_item("{\"ModIdentifier\":{\"HostedModIds\":{\"SteamWorkshopId\":-5}}}") && reason[0]);
	CHECK(!read_item("{\"ModIdentifier\":{\"HostedModIds\":{\"SteamWorkshopId\":1.5}}}") && reason[0]);
	CHECK(!read_item("{\"ModIdentifier\":{\"HostedModIds\":{\"SteamWorkshopId\":\"12\"}}}") && reason[0]);
	/* the 75 maps in CE Digsite Multiplayer fit */
	strcpy(text, "{\"GameModContents\":{\"MultiplayerMaps\":[");
	for (index = 0; index < 75; index++)
		sprintf(text + strlen(text), "%s\"m/%d.json\"", index ? "," : "", index);
	strcat(text, "]}}");
	CHECK(read_item(text) && item.map_count == 75);
	/* 256 maps fit, 257 do not */
	strcpy(text, "{\"GameModContents\":{\"MultiplayerMaps\":[");
	for (index = 0; index < 257; index++)
		sprintf(text + strlen(text), "%s\"m/%d.json\"", index ? "," : "", index);
	strcat(text, "]}}");
	CHECK(!read_item(text) && reason[0]);
	strcpy(text, "{\"GameModContents\":{\"MultiplayerMaps\":[");
	for (index = 0; index < 256; index++)
		sprintf(text + strlen(text), "%s\"m/%d.json\"", index ? "," : "", index);
	strcat(text, "]}}");
	CHECK(read_item(text) && item.map_count == 256);
	/* 300 KB: refused */
	big = malloc(300 * 1024 + 1);
	memset(big, ' ', 300 * 1024);
	big[300 * 1024] = 0;
	memcpy(big, "{}", 2);
	CHECK(!read_item(big) && reason[0]);
	free(big);
	/* nesting past 16 */
	CHECK(!read_item("{\"a\":[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]}") && reason[0]);
}

static void test_title(void)
{
	char text[600];

	/* a long title is cut, at a whole UTF-8 character (a 2-byte one straddles the 127-byte limit) */
	strcpy(text, "{\"Title\":{\"Neutral\":\"");
	pad(text, 'a', 126);
	strcpy(text + strlen(text), "\xC3\xA9\xC3\xA9\"}}");
	CHECK(read_item(text) && strlen(item.title) == 126);
	strcpy(text, "{\"Title\":{\"Neutral\":\"");
	pad(text, 'b', 300);
	strcpy(text + strlen(text), "\"}}");
	CHECK(read_item(text) && strlen(item.title) == 127);
}

static void test_map_info(void)
{
	CHECK(read_map("{\"ScenarioFile\":\"levels/test/pat_x/pat_x\",\"Title\":{\"Neutral\":\"Pat X\"}}"));
	CHECK(!strcmp(info.name, "pat_x") && !strcmp(info.title, "Pat X"));
	CHECK(read_map("{\"ScenarioFile\":\"levels\\\\test\\\\y\\\\y\"}") && !strcmp(info.name, "y") && info.title[0] == 0);
	CHECK(read_map("{\"ScenarioFile\":\"solo\"}") && !strcmp(info.name, "solo"));
	CHECK(!read_map("{\"ScenarioFile\":\"\"}") && reason[0]);
	CHECK(!read_map("{\"Title\":{\"Neutral\":\"x\"}}") && reason[0]);
	CHECK(!read_map("{\"ScenarioFile\":\"levels/test/dir/\"}") && reason[0]);
	CHECK(!read_map("{\"ScenarioFile\":7}") && reason[0]);
	CHECK(!read_map("{\"ScenarioFile\":\"levels/a") && reason[0]);
	CHECK(!read_map("") && reason[0]);
	CHECK(!read_map("{\"ScenarioFile\":\"levels/a/b\\u0001c\"}") && reason[0]);
	/* its pictures: item-relative, '/' separators; a path that is not plain is "" (the map still reads) */
	CHECK(read_map("{\"ScenarioFile\":\"levels/p/p\",\"Images\":{\"Large\":\"images\\\\p_large.png\","
		"\"Thumbnail\":\"images\\\\p_small.png\"}}") && !strcmp(info.thumbnail, "images/p_small.png") &&
		!strcmp(info.large, "images/p_large.png"));
	CHECK(read_map("{\"ScenarioFile\":\"levels/p/p\",\"Images\":{\"Large\":\"../x.png\",\"Thumbnail\":\"/abs.png\"}}") &&
		info.thumbnail[0] == 0 && info.large[0] == 0);
	CHECK(read_map("{\"ScenarioFile\":\"levels/p/p\",\"Images\":{\"Thumbnail\":\"C:\\\\x.png\",\"Large\":7}}") &&
		info.thumbnail[0] == 0 && info.large[0] == 0);
	CHECK(read_map("{\"ScenarioFile\":\"levels/p/p\"}") && info.thumbnail[0] == 0 && info.large[0] == 0);
	{
		char text[300];
		strcpy(text, "{\"ScenarioFile\":\"levels/");
		pad(text, 'n', 64);
		strcpy(text + strlen(text), "\"}");
		CHECK(!read_map(text) && reason[0]);
		strcpy(text, "{\"ScenarioFile\":\"levels/");
		pad(text, 'n', 63);
		strcpy(text + strlen(text), "\"}");
		CHECK(read_map(text) && strlen(info.name) == 63);
	}
}


/* ---------- the scan: fake Steam trees under a mkdtemp folder */

static char root[256];

static void make_folder(const char *relative)
{
	char path[600], *c;

	snprintf(path, sizeof(path), "%s/%s", root, relative);
	for (c = path + strlen(root) + 1; *c; c++)
	{
		if (*c == '/')
		{
			*c = 0;
			mkdir(path, 0700);
			*c = '/';
		}
	}
	mkdir(path, 0700);
}

static void put_file(const char *relative, const void *data, size_t size)
{
	char path[600];
	FILE *file;

	snprintf(path, sizeof(path), "%s/%s", root, relative);
	file = fopen(path, "wb");
	if (file)
	{
		fwrite(data, 1, size, file);
		fclose(file);
	}
	else
		CHECK(!"put_file");
}

static void put_text(const char *relative, const char *text)
{
	put_file(relative, text, strlen(text));
}

/* a synthetic v13 map: header size field, the file length, toof at 0x7FC */
static void put_map(const char *relative, unsigned long header_size, size_t file_size, int version)
{
	unsigned char *bytes = calloc(1, file_size);

	memcpy(bytes, "daeh", 4);
	bytes[4] = (unsigned char)version;
	bytes[8] = (unsigned char)header_size;
	bytes[9] = (unsigned char)(header_size >> 8);
	bytes[10] = (unsigned char)(header_size >> 16);
	bytes[11] = (unsigned char)(header_size >> 24);
	if (file_size >= 0x800)
		memcpy(bytes + 0x7FC, "toof", 4);
	put_file(relative, bytes, file_size);
	free(bytes);
}

static void full_path(char *out, size_t size, const char *relative)
{
	snprintf(out, size, "%s/%s", root, relative);
}

static void test_map_file_valid(void)
{
	char path[600], text[128];

	put_map("whole.map", 2048, 2048, 13);
	full_path(path, sizeof(path), "whole.map");
	CHECK(ae_mcc_workshop_map_file_valid(path, text, sizeof(text)) == 1);
	put_map("short.map", 4096, 2048, 13);
	full_path(path, sizeof(path), "short.map");
	CHECK(ae_mcc_workshop_map_file_valid(path, text, sizeof(text)) == 0 && !strcmp(text, "being updated by Steam"));
	put_map("xbox.map", 2048, 2048, 7);
	full_path(path, sizeof(path), "xbox.map");
	CHECK(ae_mcc_workshop_map_file_valid(path, text, sizeof(text)) == 0 && !strcmp(text, "not an MCC Halo CE map"));
	{
		unsigned char bad = 'x';
		char full[600];

		full_path(full, sizeof(full), "fourcc.map");
		put_file("fourcc.map", &bad, 1);
		CHECK(ae_mcc_workshop_map_file_valid(full, text, sizeof(text)) == 0 && !strcmp(text, "not an MCC Halo CE map"));
	}
	/* no toof */
	{
		unsigned char *bytes = calloc(1, 2048);

		memcpy(bytes, "daeh", 4);
		bytes[4] = 13;
		bytes[9] = 8;
		put_file("notoof.map", bytes, 2048);
		free(bytes);
	}
	full_path(path, sizeof(path), "notoof.map");
	CHECK(ae_mcc_workshop_map_file_valid(path, text, sizeof(text)) == 0 && !strcmp(text, "not an MCC Halo CE map"));
	/* longer than the header says */
	put_map("long.map", 2048, 4096, 13);
	full_path(path, sizeof(path), "long.map");
	CHECK(ae_mcc_workshop_map_file_valid(path, text, sizeof(text)) == 0);
	full_path(path, sizeof(path), "nothere.map");
	CHECK(ae_mcc_workshop_map_file_valid(path, text, sizeof(text)) == 0 && text[0]);
}

static void put_item(const char *library, const char *id, const char *mod_info)
{
	char path[300];

	snprintf(path, sizeof(path), "%s/steamapps/workshop/content/976730/%s", library, id);
	make_folder(path);
	snprintf(path, sizeof(path), "%s/steamapps/workshop/content/976730/%s/ModInfo.json", library, id);
	put_text(path, mod_info);
}

static void test_scan(void)
{
	struct ae_mcc_workshop_map maps[16];
	const char *libraries[2];
	char l1[300], l2[300];
	char path[600];
	int hidden = -1, count, index;

	make_folder("lib1");
	make_folder("lib2");
	snprintf(l1, sizeof(l1), "%s/lib1", root);
	snprintf(l2, sizeof(l2), "%s/lib2", root);
	libraries[0] = l1;
	libraries[1] = l2;

	/* library 1: item 111, one map under maps/, one under Maps/ named PAT_B.map */
	put_item("lib1", "111", "{\"ModIdentifier\":{\"HostedModIds\":{\"SteamWorkshopId\":111}},\"Title\":{\"Neutral\":"
		"\"Pack\"},\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"multiplayer/a.json\","
		"\"multiplayer/b.json\"]}}");
	make_folder("lib1/steamapps/workshop/content/976730/111/multiplayer");
	put_text("lib1/steamapps/workshop/content/976730/111/multiplayer/a.json",
		"{\"ScenarioFile\":\"levels/test/pat_a/pat_a\",\"Title\":{\"Neutral\":\"Pat A\"}}");
	put_text("lib1/steamapps/workshop/content/976730/111/multiplayer/b.json",
		"{\"ScenarioFile\":\"levels/test/pat_b/pat_b\"}");
	make_folder("lib1/steamapps/workshop/content/976730/111/maps");
	put_map("lib1/steamapps/workshop/content/976730/111/maps/pat_a.map", 2048, 2048, 13);
	/* library 2: item 222 Halo3, item 333 Halo1 campaign only */
	put_item("lib2", "222", "{\"Engine\":\"Halo3\",\"GameModContents\":{\"MultiplayerMaps\":[\"x.json\"]}}");
	put_item("lib2", "333", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"HasCampaign\":true}}");

	/* (item 111's b map has no file yet) */
	count = ae_mcc_workshop_scan(libraries, 2, maps, 16, &hidden);
	CHECK(count == 2 && hidden == 1);
	/* now give b its file in a Maps/ folder of its own item */
	put_item("lib1", "112", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"multiplayer/b.json\"]}}");
	make_folder("lib1/steamapps/workshop/content/976730/112/multiplayer");
	put_text("lib1/steamapps/workshop/content/976730/112/multiplayer/b.json",
		"{\"ScenarioFile\":\"levels/test/pat_b/pat_b\"}");
	make_folder("lib1/steamapps/workshop/content/976730/112/Maps");
	put_map("lib1/steamapps/workshop/content/976730/112/Maps/PAT_B.map", 2048, 2048, 13);
	count = ae_mcc_workshop_scan(libraries, 2, maps, 16, &hidden);
	CHECK(count == 3 && hidden == 1);
	{
		int playable = 0, saw_b = 0;

		for (index = 0; index < count; index++)
		{
			if (maps[index].status == AE_MCC_WORKSHOP_PLAYABLE)
			{
				playable++;
				CHECK(access(maps[index].path, R_OK) == 0);
				if (!strcmp(maps[index].name, "pat_b"))
				{
					saw_b = 1;
					CHECK(strstr(maps[index].path, "/Maps/PAT_B.map") && !strcmp(maps[index].item_id, "112"));
					CHECK(!strcmp(maps[index].title, "pat_b"));
				}
				if (!strcmp(maps[index].name, "pat_a"))
					CHECK(!strcmp(maps[index].title, "Pat A") && !strcmp(maps[index].item_title, "Pack"));
			}
			else
				CHECK(!strcmp(maps[index].reason, "map file missing") && !strcmp(maps[index].name, "pat_b"));
		}
		CHECK(playable == 2 && saw_b);
	}
	/* b of item 111 completes: its file name differs in case from the ScenarioFile's */
	put_map("lib1/steamapps/workshop/content/976730/111/maps/Pat_B.MAP", 2048, 2048, 13);
	count = ae_mcc_workshop_scan(libraries, 2, maps, 16, &hidden);
	CHECK(count == 3 && hidden == 0);

	/* maximum: 3 maps, room for 2 */
	memset(maps, 0x55, sizeof(maps));
	count = ae_mcc_workshop_scan(libraries, 2, maps, 2, &hidden);
	CHECK(count == 2 && maps[2].item_id[0] == 0x55);
	(void)path;
}

static void test_scan_hidden(void)
{
	struct ae_mcc_workshop_map maps[16];
	const char *libraries[1];
	char lib[300];
	int hidden = -1, count, index, missing = 0, garbage = 0, unsafe = 0, updating = 0;

	make_folder("hid");
	snprintf(lib, sizeof(lib), "%s/hid", root);
	libraries[0] = lib;
	put_item("hid", "444", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"m/x.json\"]}}");
	make_folder("hid/steamapps/workshop/content/976730/444/m");
	put_text("hid/steamapps/workshop/content/976730/444/m/x.json", "{\"ScenarioFile\":\"levels/missing\"}");
	make_folder("hid/steamapps/workshop/content/976730/444/maps");
	put_item("hid", "555", "this is {{ not json");
	put_item("hid", "666", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"m/a.json\","
		"\"m/b.json\",\"m/c.json\"]}}");
	make_folder("hid/steamapps/workshop/content/976730/666/m");
	put_text("hid/steamapps/workshop/content/976730/666/m/a.json", "{\"ScenarioFile\":\"levels/a/..\"}");
	put_text("hid/steamapps/workshop/content/976730/666/m/b.json", "{\"ScenarioFile\":\"levels/a/c:d\"}");
	put_text("hid/steamapps/workshop/content/976730/666/m/c.json", "{\"ScenarioFile\":\"levels/a/part\"}");
	make_folder("hid/steamapps/workshop/content/976730/666/maps");
	put_map("hid/steamapps/workshop/content/976730/666/maps/part.map", 4096, 2048, 13);

	count = ae_mcc_workshop_scan(libraries, 1, maps, 16, &hidden);
	CHECK(count == 5 && hidden == 5);
	for (index = 0; index < count; index++)
	{
		/* (one being updated keeps its file, to be checked again: ae_mcc_workshop_recheck) */
		CHECK(maps[index].status == AE_MCC_WORKSHOP_HIDDEN &&
			(!strcmp(maps[index].reason, "being updated by Steam") ? maps[index].path[0] != 0 : maps[index].path[0] == 0));
		if (!strcmp(maps[index].item_id, "444") && !strcmp(maps[index].reason, "map file missing") &&
			!strcmp(maps[index].name, "missing"))
			missing++;
		if (!strcmp(maps[index].item_id, "555") && !strncmp(maps[index].reason, "unreadable ModInfo.json: ", 25) && strlen(maps[index].reason) > 25 &&
			maps[index].name[0] == 0)
			garbage++;
		if (!strcmp(maps[index].item_id, "666") && !strcmp(maps[index].reason, "unsafe map name") &&
			maps[index].name[0] == 0)
			unsafe++;
		if (!strcmp(maps[index].item_id, "666") && !strcmp(maps[index].reason, "being updated by Steam") &&
			!strcmp(maps[index].name, "part"))
			updating++;
	}
	CHECK(missing == 1 && garbage == 1 && unsafe == 2 && updating == 1);

	/* the item folders of the libraries, sorted, each once */
	{
		char ids[8][24];

		CHECK(ae_mcc_workshop_item_ids(libraries, 1, ids, 8) == 3 && !strcmp(ids[0], "444") && !strcmp(ids[1], "555") &&
			!strcmp(ids[2], "666"));
		CHECK(ae_mcc_workshop_item_ids(libraries, 1, ids, 2) == 2);
	}
	/* two lists of item ids: the same ids whatever lies past each one's end (a rescan's against the start's) */
	{
		char a[3][24], b[3][24];

		memset(a, 0x11, sizeof(a));
		memset(b, 0x77, sizeof(b));
		strcpy(a[0], "444"); strcpy(a[1], "555"); strcpy(a[2], "666");
		strcpy(b[0], "444"); strcpy(b[1], "555"); strcpy(b[2], "666");
		CHECK(ae_mcc_workshop_same_items(a, 3, b, 3));
		CHECK(!ae_mcc_workshop_same_items(a, 3, b, 2) && ae_mcc_workshop_same_items(a, 0, b, 0));
		strcpy(b[2], "667");
		CHECK(!ae_mcc_workshop_same_items(a, 3, b, 3));
	}
	/* Steam finishes the update: the map checked again is playable; nothing else changes */
	CHECK(ae_mcc_workshop_recheck(maps, count) == 0);
	put_map("hid/steamapps/workshop/content/976730/666/maps/part.map", 4096, 4096, 13);
	CHECK(ae_mcc_workshop_recheck(maps, count) == 1);
	for (index = 0, updating = 0; index < count; index++)
	{
		if (!strcmp(maps[index].name, "part"))
			CHECK(maps[index].status == AE_MCC_WORKSHOP_PLAYABLE && !maps[index].reason[0] && maps[index].path[0]);
		else
			CHECK(maps[index].status == AE_MCC_WORKSHOP_HIDDEN);
	}
	CHECK(ae_mcc_workshop_recheck(maps, count) == 0);
}

/* a map's pictures, found case-blind in its item */
static void test_scan_pictures(void)
{
	struct ae_mcc_workshop_map maps[4];
	const char *libraries[1];
	char lib[300];
	int hidden = -1;

	make_folder("pic");
	snprintf(lib, sizeof(lib), "%s/pic", root);
	libraries[0] = lib;
	put_item("pic", "777", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"m/p.json\"]}}");
	make_folder("pic/steamapps/workshop/content/976730/777/m");
	put_text("pic/steamapps/workshop/content/976730/777/m/p.json", "{\"ScenarioFile\":\"levels/p/p\",\"Images\":"
		"{\"Thumbnail\":\"images\\\\p_small.png\",\"Large\":\"images\\\\p_large.png\"}}");
	make_folder("pic/steamapps/workshop/content/976730/777/maps");
	put_map("pic/steamapps/workshop/content/976730/777/maps/p.map", 2048, 2048, 13);
	make_folder("pic/steamapps/workshop/content/976730/777/Images");
	put_text("pic/steamapps/workshop/content/976730/777/Images/P_Small.PNG", "png");
	CHECK(ae_mcc_workshop_scan(libraries, 1, maps, 4, &hidden) == 1 && hidden == 0);
	CHECK(maps[0].status == AE_MCC_WORKSHOP_PLAYABLE && strstr(maps[0].thumbnail, "/Images/P_Small.PNG") &&
		access(maps[0].thumbnail, R_OK) == 0 && maps[0].large[0] == 0);
}

/* MCC's stock maps in an install: playable, missing, or being updated */
static void test_stock_state(void)
{
	char install[300], why[128];

	make_folder("mcc/halo1/maps");
	snprintf(install, sizeof(install), "%s/mcc", root);
	put_map("mcc/halo1/maps/bloodgulch.map", 2048, 2048, 13);
	put_map("mcc/halo1/maps/wizard.map", 4096, 2048, 13);
	CHECK(ae_mcc_stock_map_state(install, "bloodgulch", why, sizeof(why)) == AE_MCC_STOCK_PLAYABLE);
	CHECK(ae_mcc_stock_map_state(install, "BloodGulch", why, sizeof(why)) == AE_MCC_STOCK_PLAYABLE);
	CHECK(ae_mcc_stock_map_state(install, "chillout", why, sizeof(why)) == AE_MCC_STOCK_MISSING);
	CHECK(ae_mcc_stock_map_state(install, "wizard", why, sizeof(why)) == AE_MCC_STOCK_HIDDEN &&
		!strcmp(why, "being updated by Steam"));
	CHECK(ae_mcc_stock_map_state(install, "a10", why, sizeof(why)) == AE_MCC_STOCK_MISSING);
	CHECK(ae_mcc_stock_map_state(NULL, "bloodgulch", why, sizeof(why)) == AE_MCC_STOCK_MISSING);
}

static void test_scan_spaces(void)
{
	struct ae_mcc_workshop_map maps[4];
	const char *libraries[1];
	char lib[300];
	int hidden = -1;

	make_folder("Program Files (x86)/Steam");
	snprintf(lib, sizeof(lib), "%s/Program Files (x86)/Steam", root);
	libraries[0] = lib;
	put_item("Program Files (x86)/Steam", "777", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":"
		"[\"m/s.json\"]}}");
	make_folder("Program Files (x86)/Steam/steamapps/workshop/content/976730/777/m");
	make_folder("Program Files (x86)/Steam/steamapps/workshop/content/976730/777/maps");
	put_text("Program Files (x86)/Steam/steamapps/workshop/content/976730/777/m/s.json", "{\"ScenarioFile\":\"a/s\"}");
	put_map("Program Files (x86)/Steam/steamapps/workshop/content/976730/777/maps/s.map", 2048, 2048, 13);
	CHECK(ae_mcc_workshop_scan(libraries, 1, maps, 4, &hidden) == 1 && hidden == 0 &&
		maps[0].status == AE_MCC_WORKSHOP_PLAYABLE && strstr(maps[0].path, "Program Files (x86)/Steam/"));
	/* a library without a workshop folder, and a missing one: nothing, no crash */
	libraries[0] = "/nonexistent/steam";
	CHECK(ae_mcc_workshop_scan(libraries, 1, maps, 4, &hidden) == 0 && hidden == 0);
}

static void test_scan_paths(void)
{
	struct ae_mcc_workshop_map maps[8];
	const char *libraries[1];
	char lib[300];
	int hidden = -1, count;

	make_folder("pth");
	snprintf(lib, sizeof(lib), "%s/pth", root);
	libraries[0] = lib;
	/* a wrong-case json path is found; a json that is a folder is hidden; a UTF-8 library/item folder scans */
	put_item("pth", "888", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"Multiplayer/A.JSON\"]}}");
	make_folder("pth/steamapps/workshop/content/976730/888/multiplayer");
	put_text("pth/steamapps/workshop/content/976730/888/multiplayer/a.json", "{\"ScenarioFile\":\"x/wrong_case\"}");
	make_folder("pth/steamapps/workshop/content/976730/888/maps");
	put_map("pth/steamapps/workshop/content/976730/888/maps/wrong_case.map", 2048, 2048, 13);
	count = ae_mcc_workshop_scan(libraries, 1, maps, 8, &hidden);
	CHECK(count == 1 && hidden == 0 && maps[0].status == AE_MCC_WORKSHOP_PLAYABLE && !strcmp(maps[0].name, "wrong_case"));

	put_item("pth", "889", "{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"m/dir.json\"]}}");
	make_folder("pth/steamapps/workshop/content/976730/889/m/dir.json");
	count = ae_mcc_workshop_scan(libraries, 1, maps, 8, &hidden);
	CHECK(count == 2 && hidden == 1);
	{
		int index, found = 0;

		for (index = 0; index < count; index++)
		{
			if (!strcmp(maps[index].item_id, "889"))
			{
				found = maps[index].status == AE_MCC_WORKSHOP_HIDDEN && !strcmp(maps[index].reason, "unreadable map json");
			}
		}
		CHECK(found);
	}

	/* a ModInfo.json that is a folder: hidden with a reason */
	make_folder("pth/steamapps/workshop/content/976730/890/ModInfo.json");
	count = ae_mcc_workshop_scan(libraries, 1, maps, 8, &hidden);
	CHECK(count == 3 && hidden == 2);

	/* UTF-8 names */
	make_folder("b\303\274r\303\244/steamapps/workshop/content/976730/\342\202\254999/m");
	make_folder("b\303\274r\303\244/steamapps/workshop/content/976730/\342\202\254999/maps");
	put_text("b\303\274r\303\244/steamapps/workshop/content/976730/\342\202\254999/ModInfo.json",
		"{\"Engine\":\"Halo1\",\"GameModContents\":{\"MultiplayerMaps\":[\"m/u.json\"]}}");
	put_text("b\303\274r\303\244/steamapps/workshop/content/976730/\342\202\254999/m/u.json", "{\"ScenarioFile\":\"a/u\"}");
	put_map("b\303\274r\303\244/steamapps/workshop/content/976730/\342\202\254999/maps/u.map", 2048, 2048, 13);
	snprintf(lib, sizeof(lib), "%s/b\303\274r\303\244", root);
	count = ae_mcc_workshop_scan(libraries, 1, maps, 8, &hidden);
	CHECK(count == 1 && hidden == 0 && maps[0].status == AE_MCC_WORKSHOP_PLAYABLE);
}

static void remove_tree(const char *path)
{
	char command[700];

	snprintf(command, sizeof(command), "rm -rf '%s'", path);
	if (system(command))
		printf("cleanup failed\n");
}

int main(void)
{
	strcpy(root, "/tmp/ae_mcc_workshop_XXXXXX");
	if (!mkdtemp(root))
		return 2;
	test_item_good();
	test_item_other();
	test_item_bad();
	test_title();
	test_map_info();
	test_map_file_valid();
	test_scan();
	test_scan_hidden();
	test_scan_pictures();
	test_stock_state();
	test_scan_spaces();
	test_scan_paths();
	remove_tree(root);
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_mcc_workshop ok\n");
	return 0;
}
