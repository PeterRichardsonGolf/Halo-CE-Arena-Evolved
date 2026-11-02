#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ae_map_share_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned long ip(unsigned a, unsigned b, unsigned c, unsigned d)
{
	return (unsigned long)a << 24 | (unsigned long)b << 16 | (unsigned long)c << 8 | (unsigned long)d;
}

static int exists(char const *path)
{
	struct stat info;

	return stat(path, &info) == 0;
}

static void touch(char const *path)
{
	FILE *file = fopen(path, "wb");

	if (file)
	{
		fputs("map", file);
		fclose(file);
	}
}

static int delete_real(char const *path, void *context)
{
	int *calls = context;

	(*calls)++;
	return remove(path) == 0;
}

static int delete_logged(char const *path, void *context)
{
	(void)path;
	(*(int *)context)++;
	return 1;
}

/* ---------- sharing every map but the game's own */

static struct ae_map_share_entry entry_of(char const *name, int kind, unsigned long size)
{
	struct ae_map_share_entry entry;

	memset(&entry, 0, sizeof(entry));
	strncpy(entry.name, name, sizeof(entry.name) - 1);
	entry.kind = (unsigned char)kind;
	entry.size = size;
	memset(entry.hash, 0x5A, sizeof(entry.hash));
	return entry;
}

/* the test's own list of the retail game's maps: every one must be a stock name, in any case */
static char const *const retail_maps[] =
{
	"beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout", "carousel",
	"boardingaction", "bloodgulch", "wizard", "putput", "longest",
	"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
	"ui",
	"bitmaps", "sounds", "loc",
};

static int lacks_all(struct ae_map_share_entry const *entry, void *context)
{
	(void)entry;
	(void)context;
	return 1;
}

static int lacks_but_first(struct ae_map_share_entry const *entry, void *context)
{
	return strcmp(entry->name, (char const *)context) != 0;
}

static void share_all_tests(void)
{
	static struct ae_map_share_catalogue catalogue;
	static struct ae_map_share_frame frame;
	static unsigned char bytes[AE_MAP_SHARE_WIRE_FRAME_BYTES + AE_MAP_SHARE_FRAME_ENTRIES * AE_MAP_SHARE_WIRE_ENTRY_BYTES + 8];
	struct ae_map_share_entry entry;
	struct ae_map_share_rate rate;
	static short indices[AE_MAP_SHARE_CATALOGUE_MAXIMUM];
	unsigned long long total = 0;
	unsigned char hash[AE_MAP_SHARE_HASH_BYTES];
	char name[80];
	int over = 0;
	int count = 0;
	size_t index;
	long size;
	char text[200];

	/* the stock maps: each of the retail game's is one, in any case, as an Xbox map, a mod's and a Custom Edition
	map; the engine's list is exactly these */
	for (index = 0; index < sizeof(retail_maps) / sizeof(retail_maps[0]); index++)
	{
		size_t letter;

		CHECK(ae_map_share_stock_name(retail_maps[index]));
		snprintf(name, sizeof(name), "%s", retail_maps[index]);
		for (letter = 0; name[letter]; letter++)
			name[letter] = (char)(name[letter] >= 'a' && name[letter] <= 'z' ? name[letter] - 32 : name[letter]);
		CHECK(ae_map_share_stock_name(name));
		entry = entry_of(retail_maps[index], AE_MAP_SHARE_KIND_XBOX, 1000);
		CHECK(!ae_map_share_entry_valid(&entry));
		entry.kind = AE_MAP_SHARE_KIND_MOD;
		CHECK(!ae_map_share_entry_valid(&entry));
		entry.kind = AE_MAP_SHARE_KIND_CE;
		CHECK(!ae_map_share_entry_valid(&entry));
	}
	for (count = 0; ae_map_share_stock_names()[count]; count++)
		;
	CHECK(count == (int)(sizeof(retail_maps) / sizeof(retail_maps[0])));
	CHECK(!ae_map_share_stock_name("badcreek") && !ae_map_share_stock_name("bloodgulch2") &&
		!ae_map_share_stock_name("") && !ae_map_share_stock_name(NULL) && !ae_map_share_stock_name("a1"));

	/* names: one folder's file names of an allowed character set */
	CHECK(ae_map_share_name_allowed("badcreek") && ae_map_share_name_allowed("Beaver Creek V2") &&
		ae_map_share_name_allowed("homobox_1a2b3c4d") && ae_map_share_name_allowed("map-2(b)+1.5"));
	CHECK(!ae_map_share_name_allowed("") && !ae_map_share_name_allowed(NULL) && !ae_map_share_name_allowed(".") &&
		!ae_map_share_name_allowed("..") && !ae_map_share_name_allowed("a..b") && !ae_map_share_name_allowed(".a") &&
		!ae_map_share_name_allowed("a.") && !ae_map_share_name_allowed(" a") && !ae_map_share_name_allowed("a ") &&
		!ae_map_share_name_allowed("a/b") && !ae_map_share_name_allowed("a\\b") && !ae_map_share_name_allowed("c:a") &&
		!ae_map_share_name_allowed("a\nb") && !ae_map_share_name_allowed("a*") && !ae_map_share_name_allowed("a?") &&
		!ae_map_share_name_allowed("a\"b") && !ae_map_share_name_allowed("a<b") && !ae_map_share_name_allowed("a|b") &&
		!ae_map_share_name_allowed("a@ce") && !ae_map_share_name_allowed("caf\xC3\xA9") &&
		!ae_map_share_name_allowed("..\\..\\maps\\bloodgulch"));
	memset(name, 'a', sizeof(name));
	name[AE_MAP_SHARE_NAME_BYTES - 1] = 0;
	CHECK(ae_map_share_name_allowed(name));
	name[AE_MAP_SHARE_NAME_BYTES - 1] = 'a';
	name[AE_MAP_SHARE_NAME_BYTES] = 0;
	CHECK(!ae_map_share_name_allowed(name));
	CHECK(ae_map_share_name_hashed("a_1a2b3c4d") && !ae_map_share_name_hashed("_1a2b3c4d") &&
		!ae_map_share_name_hashed("a_1A2B3C4D") && !ae_map_share_name_hashed("a_1a2b3c4") &&
		!ae_map_share_name_hashed("a1a2b3c4d5") && !ae_map_share_name_hashed(NULL));

	/* sizes: the engine's limit of each kind, never an empty file, never an unknown kind */
	CHECK(ae_map_share_size_allowed(AE_MAP_SHARE_KIND_XBOX, AE_MAP_SHARE_XBOX_MAXIMUM_BYTES) &&
		!ae_map_share_size_allowed(AE_MAP_SHARE_KIND_XBOX, AE_MAP_SHARE_XBOX_MAXIMUM_BYTES + 1ULL) &&
		!ae_map_share_size_allowed(AE_MAP_SHARE_KIND_MOD, AE_MAP_SHARE_XBOX_MAXIMUM_BYTES + 1ULL) &&
		ae_map_share_size_allowed(AE_MAP_SHARE_KIND_CE, AE_MAP_SHARE_CE_MAXIMUM_BYTES) &&
		!ae_map_share_size_allowed(AE_MAP_SHARE_KIND_CE, AE_MAP_SHARE_CE_MAXIMUM_BYTES + 1ULL) &&
		ae_map_share_size_allowed(AE_MAP_SHARE_KIND_RESOURCE, AE_MAP_SHARE_RESOURCE_MAXIMUM_BYTES) &&
		!ae_map_share_size_allowed(AE_MAP_SHARE_KIND_RESOURCE, AE_MAP_SHARE_RESOURCE_MAXIMUM_BYTES + 1ULL) &&
		!ae_map_share_size_allowed(AE_MAP_SHARE_KIND_XBOX, 0) && !ae_map_share_size_allowed(AE_MAP_SHARE_KIND_NONE, 5) &&
		!ae_map_share_size_allowed(AE_MAP_SHARE_KINDS, 5) && !ae_map_share_size_allowed(AE_MAP_SHARE_KIND_CE, 1ULL << 40));

	/* entries */
	entry = entry_of("badcreek", AE_MAP_SHARE_KIND_XBOX, 22956032);
	CHECK(ae_map_share_entry_valid(&entry));
	entry = entry_of("a name of more than thirty-one chars", AE_MAP_SHARE_KIND_XBOX, 5);
	CHECK(!ae_map_share_entry_valid(&entry));
	entry = entry_of("homobox_1a2b3c4d", AE_MAP_SHARE_KIND_CE, 5);
	CHECK(ae_map_share_entry_valid(&entry));
	entry = entry_of("homobox", AE_MAP_SHARE_KIND_CE, 5);
	CHECK(!ae_map_share_entry_valid(&entry));
	entry = entry_of("bitmaps", AE_MAP_SHARE_KIND_RESOURCE, 5);
	CHECK(ae_map_share_entry_valid(&entry));
	entry = entry_of("badcreek", AE_MAP_SHARE_KIND_RESOURCE, 5);
	CHECK(!ae_map_share_entry_valid(&entry));
	entry = entry_of("badcreek", 9, 5);
	CHECK(!ae_map_share_entry_valid(&entry));
	entry = entry_of("badcreek", AE_MAP_SHARE_KIND_XBOX, 5);
	memset(entry.name, 'a', sizeof(entry.name));
	CHECK(!ae_map_share_entry_valid(&entry));
	CHECK(!ae_map_share_entry_valid(NULL));

	/* the list's bounds: no more than the maximum expected or taken, frames in order only, no name twice */
	CHECK(!ae_map_share_catalogue_begin(&catalogue, AE_MAP_SHARE_CATALOGUE_MAXIMUM + 1));
	CHECK(!ae_map_share_catalogue_begin(&catalogue, -1));
	CHECK(ae_map_share_catalogue_begin(&catalogue, 0) && ae_map_share_catalogue_complete(&catalogue));
	CHECK(ae_map_share_catalogue_begin(&catalogue, AE_MAP_SHARE_CATALOGUE_MAXIMUM));
	for (count = 0; count < AE_MAP_SHARE_CATALOGUE_MAXIMUM; count += AE_MAP_SHARE_FRAME_ENTRIES)
	{
		memset(&frame, 0, sizeof(frame));
		frame.type = AE_MAP_SHARE_FRAME_CATALOGUE;
		frame.expected = AE_MAP_SHARE_CATALOGUE_MAXIMUM;
		frame.first = count;
		for (frame.count = 0; frame.count < AE_MAP_SHARE_FRAME_ENTRIES && count + frame.count < AE_MAP_SHARE_CATALOGUE_MAXIMUM;
			frame.count++)
		{
			snprintf(name, sizeof(name), "map%d", count + frame.count);
			frame.entries[frame.count] = entry_of(name, AE_MAP_SHARE_KIND_XBOX, 1000);
		}
		CHECK(!ae_map_share_catalogue_complete(&catalogue));
		CHECK(ae_map_share_catalogue_take(&catalogue, &frame));
		/* (the same frame again: not the next) */
		CHECK(!ae_map_share_catalogue_take(&catalogue, &frame));
	}
	CHECK(ae_map_share_catalogue_complete(&catalogue) && catalogue.count == AE_MAP_SHARE_CATALOGUE_MAXIMUM);
	frame.first = AE_MAP_SHARE_CATALOGUE_MAXIMUM;
	frame.count = 1;
	CHECK(!ae_map_share_catalogue_take(&catalogue, &frame) && catalogue.count == AE_MAP_SHARE_CATALOGUE_MAXIMUM);
	CHECK(ae_map_share_catalogue_find(&catalogue, "MAP7", 0) == 7 && ae_map_share_catalogue_find(&catalogue, "map7", 1) < 0 &&
		ae_map_share_catalogue_find(&catalogue, "nomap", 0) < 0);
	/* (bad entries and names said twice are left out; a frame for another list, or with too many, is refused) */
	CHECK(ae_map_share_catalogue_begin(&catalogue, 4));
	memset(&frame, 0, sizeof(frame));
	frame.type = AE_MAP_SHARE_FRAME_CATALOGUE;
	frame.expected = 4;
	frame.count = 4;
	frame.entries[0] = entry_of("good", AE_MAP_SHARE_KIND_XBOX, 10);
	frame.entries[1] = entry_of("GOOD", AE_MAP_SHARE_KIND_MOD, 10);
	frame.entries[2] = entry_of("bloodgulch", AE_MAP_SHARE_KIND_XBOX, 10);
	frame.entries[3] = entry_of("..\\evil", AE_MAP_SHARE_KIND_XBOX, 10);
	CHECK(ae_map_share_catalogue_take(&catalogue, &frame));
	CHECK(ae_map_share_catalogue_complete(&catalogue) && catalogue.count == 1);
	CHECK(ae_map_share_catalogue_begin(&catalogue, 4));
	frame.expected = 5;
	CHECK(!ae_map_share_catalogue_take(&catalogue, &frame));
	frame.expected = 4;
	frame.first = 1;
	CHECK(!ae_map_share_catalogue_take(&catalogue, &frame));
	frame.first = 0;
	frame.count = AE_MAP_SHARE_FRAME_ENTRIES + 1;
	CHECK(!ae_map_share_catalogue_take(&catalogue, &frame));
	frame.count = 5;
	CHECK(!ae_map_share_catalogue_take(&catalogue, &frame));
	frame.type = AE_MAP_SHARE_FRAME_HELLO;
	frame.count = 4;
	CHECK(!ae_map_share_catalogue_take(&catalogue, &frame) && catalogue.count == 0);

	/* frames: what is written is read back; any other size, type or count is refused, and a name not ended */
	memset(&frame, 0, sizeof(frame));
	frame.type = AE_MAP_SHARE_FRAME_CATALOGUE;
	frame.flags = AE_MAP_SHARE_FLAG_SHARED;
	frame.expected = 300;
	frame.first = 290;
	frame.count = 2;
	frame.entries[0] = entry_of("badcreek", AE_MAP_SHARE_KIND_XBOX, 0x12345678);
	frame.entries[1] = entry_of("homobox_1a2b3c4d", AE_MAP_SHARE_KIND_CE, 99);
	size = ae_map_share_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(size == AE_MAP_SHARE_WIRE_FRAME_BYTES + 2 * AE_MAP_SHARE_WIRE_ENTRY_BYTES);
	{
		static struct ae_map_share_frame read;

		CHECK(ae_map_share_frame_decode(bytes, size, &read));
		CHECK(read.type == frame.type && read.flags == frame.flags && read.expected == 300 && read.first == 290 &&
			read.count == 2 && !strcmp(read.entries[0].name, "badcreek") && read.entries[0].size == 0x12345678 &&
			read.entries[1].kind == AE_MAP_SHARE_KIND_CE && !memcmp(read.entries[1].hash, frame.entries[1].hash, 32));
		CHECK(!ae_map_share_frame_decode(bytes, size - 1, &read) && !ae_map_share_frame_decode(bytes, size + 1, &read) &&
			!ae_map_share_frame_decode(bytes, 3, &read) && !ae_map_share_frame_decode(NULL, size, &read));
		bytes[0] = 9;
		CHECK(!ae_map_share_frame_decode(bytes, size, &read));
		bytes[0] = AE_MAP_SHARE_FRAME_CATALOGUE;
		bytes[6] = AE_MAP_SHARE_FRAME_ENTRIES + 1;
		CHECK(!ae_map_share_frame_decode(bytes, AE_MAP_SHARE_WIRE_FRAME_BYTES +
			(AE_MAP_SHARE_FRAME_ENTRIES + 1) * AE_MAP_SHARE_WIRE_ENTRY_BYTES, &read));
		bytes[6] = 2;
		bytes[2] = 0xFF;
		bytes[3] = 0xFF;
		CHECK(!ae_map_share_frame_decode(bytes, size, &read));
		bytes[2] = 2;
		bytes[3] = 0;
		memset(bytes + AE_MAP_SHARE_WIRE_FRAME_BYTES, 'x', AE_MAP_SHARE_NAME_BYTES);
		CHECK(!ae_map_share_frame_decode(bytes, size, &read));
		/* (a hello carries no entries) */
		memset(bytes, 0, sizeof(bytes));
		bytes[0] = AE_MAP_SHARE_FRAME_HELLO;
		bytes[1] = AE_MAP_SHARE_FLAG_SHARED;
		CHECK(ae_map_share_frame_decode(bytes, AE_MAP_SHARE_WIRE_FRAME_BYTES, &read) && read.flags == 1);
		bytes[6] = 1;
		CHECK(!ae_map_share_frame_decode(bytes, AE_MAP_SHARE_WIRE_FRAME_BYTES + AE_MAP_SHARE_WIRE_ENTRY_BYTES, &read));
	}
	CHECK(!ae_map_share_frame_encode(&frame, bytes, 10));
	frame.count = AE_MAP_SHARE_FRAME_ENTRIES + 1;
	CHECK(!ae_map_share_frame_encode(&frame, bytes, sizeof(bytes)));

	/* an offer is the list's map only with the list's size and whole hash (a lying size, a lying hash) */
	entry = entry_of("badcreek", AE_MAP_SHARE_KIND_XBOX, 1000);
	memcpy(hash, entry.hash, sizeof(hash));
	CHECK(ae_map_share_offer_matches(&entry, 1000, hash));
	CHECK(!ae_map_share_offer_matches(&entry, 1001, hash) && !ae_map_share_offer_matches(&entry, 999, hash));
	hash[31] ^= 1;
	CHECK(!ae_map_share_offer_matches(&entry, 1000, hash));
	hash[31] ^= 1;
	hash[0] ^= 0x80;
	CHECK(!ae_map_share_offer_matches(&entry, 1000, hash) && !ae_map_share_offer_matches(NULL, 1000, hash) &&
		!ae_map_share_offer_matches(&entry, 1000, NULL));

	/* the total cap, the disk */
	CHECK(ae_map_share_cap_megabytes(-5) == 0 && ae_map_share_cap_megabytes(4096) == 4096 &&
		ae_map_share_cap_megabytes(1 << 30) == AE_MAP_SHARE_CAP_MAXIMUM_MEGABYTES);
	CHECK(ae_map_share_cap_allows(0, 4096ULL << 20, 4096) && !ae_map_share_cap_allows(1, 4096ULL << 20, 4096) &&
		!ae_map_share_cap_allows(0, (4096ULL << 20) + 1, 4096) && ae_map_share_cap_allows(4095ULL << 20, 1 << 20, 4096) &&
		!ae_map_share_cap_allows(0, 1, 0) && !ae_map_share_cap_allows(~0ULL, 1, 4096) &&
		!ae_map_share_cap_allows(5, ~0ULL, 4096) && !ae_map_share_cap_allows(0, 1, -1));
	CHECK(ae_map_share_disk_allows(AE_MAP_SHARE_DISK_MARGIN_BYTES + 100ULL, 100) &&
		!ae_map_share_disk_allows(AE_MAP_SHARE_DISK_MARGIN_BYTES + 100ULL, 101) && !ae_map_share_disk_allows(5, 1) &&
		!ae_map_share_disk_allows(0, 0) && !ae_map_share_disk_allows(~0ULL, ~0ULL));

	/* the LAN test is item 77's (ae_map_share_address_local): local machines by the setting alone, the others only
	with the internet's switch too; off is off for both */
	CHECK(ae_map_share_policy_allows(ae_map_share_address_local(ip(192, 168, 1, 7)), 1, 0));
	CHECK(!ae_map_share_policy_allows(ae_map_share_address_local(ip(100, 64, 0, 3)), 1, 0));
	CHECK(!ae_map_share_policy_allows(ae_map_share_address_local(ip(8, 8, 8, 8)), 1, 0));
	CHECK(ae_map_share_policy_allows(ae_map_share_address_local(ip(8, 8, 8, 8)), 1, 1));
	CHECK(!ae_map_share_policy_allows(ae_map_share_address_local(ip(10, 0, 0, 1)), 0, 1));
	CHECK(!ae_map_share_policy_allows(0, 0, 1));

	/* no question (the OpenCE way): the setting is the player's yes, and the policy above is all there is. The names,
	amounts and reasons the card and the dialog show */
	ae_map_share_display_name("h2_coagulation_9fc847de", text, sizeof(text));
	CHECK(!strcmp(text, "H2 Coagulation"));
	ae_map_share_display_name("h2_zanzibar_D9913A5C", text, sizeof(text));
	CHECK(!strcmp(text, "H2 Zanzibar"));
	ae_map_share_display_name("h2_coagulation_9fc847de@ce", text, sizeof(text));
	CHECK(!strcmp(text, "H2 Coagulation"));
	ae_map_share_display_name("sharecreek", text, sizeof(text));
	CHECK(!strcmp(text, "Sharecreek"));
	ae_map_share_display_name("d:\\maps_ce\\cmt_snow_grove.MAP", text, sizeof(text));
	CHECK(!strcmp(text, "Cmt Snow Grove"));
	/* (a name that only looks like a hash is kept: 7 digits, a non-hex letter) */
	ae_map_share_display_name("ridge_9fc847d", text, sizeof(text));
	CHECK(!strcmp(text, "Ridge 9fc847d"));
	ae_map_share_display_name("ridge_9fc847dG", text, sizeof(text));
	CHECK(!strcmp(text, "Ridge 9fc847dG"));
	ae_map_share_display_name(NULL, text, sizeof(text));
	CHECK(!text[0]);
	ae_map_share_display_name("abcdefghij", text, 5);
	CHECK(!strcmp(text, "Abcd"));
	ae_map_share_amount_text(7ULL << 20, 22ULL << 20, text, sizeof(text));
	CHECK(!strcmp(text, "7 of 22 MB"));
	ae_map_share_amount_text(400000, 10300000, text, sizeof(text));
	CHECK(!strcmp(text, "0.3 of 9.8 MB"));
	ae_map_share_amount_text(0, 10ULL << 20, text, sizeof(text));
	CHECK(!strcmp(text, "0 of 10 MB"));
	ae_map_share_megabytes_text(210ULL << 20, text, sizeof(text));
	CHECK(!strcmp(text, "210 MB"));
	ae_map_share_megabytes_text(5ULL << 20, text, sizeof(text));
	CHECK(!strcmp(text, "5.0 MB"));
	CHECK(ae_map_share_why("The host does not share its maps.") == AE_MAP_SHARE_WHY_NOT_SHARED);
	CHECK(ae_map_share_why("The host does not share this map.") == AE_MAP_SHARE_WHY_NOT_SHARED);
	CHECK(ae_map_share_why("The host cannot share this map.") == AE_MAP_SHARE_WHY_CANNOT_SEND);
	CHECK(ae_map_share_why("The host's offer was not of this map.") == AE_MAP_SHARE_WHY_WRONG_FILE);
	CHECK(ae_map_share_why("There is not enough memory to download the map.") == AE_MAP_SHARE_WHY_NO_MEMORY);
	CHECK(ae_map_share_why("The map could not be written (is the disk full?).") == AE_MAP_SHARE_WHY_DISK_FULL);
	CHECK(ae_map_share_why("There is not enough free disk space to download the map (9 MB).") == AE_MAP_SHARE_WHY_DISK_FULL);
	CHECK(ae_map_share_why("The downloaded map could not be put in maps_ce (a file of its name is there).") ==
		AE_MAP_SHARE_WHY_NAME_TAKEN);
	CHECK(ae_map_share_why("The map was downloaded, but cannot be played.") == AE_MAP_SHARE_WHY_WONT_LOAD);
	CHECK(ae_map_share_why("The host did not answer the request for the map.") == AE_MAP_SHARE_WHY_NO_ANSWER);
	CHECK(ae_map_share_why("The download stopped: no player sent any of the map for a minute.") == AE_MAP_SHARE_WHY_STALLED);
	CHECK(ae_map_share_why("The map is 90 MB: over the 80 MB you take from one host (network.map_download_limit).") ==
		AE_MAP_SHARE_WHY_OVER_LIMIT);
	CHECK(ae_map_share_why("DOWNLOAD MAPS is OFF.") == AE_MAP_SHARE_WHY_SETTING_OFF);
	CHECK(ae_map_share_why("Something new") == AE_MAP_SHARE_WHY_OTHER && ae_map_share_why(NULL) == AE_MAP_SHARE_WHY_OTHER);
	ae_map_share_why_line(AE_MAP_SHARE_WHY_OVER_LIMIT, 4096, text, sizeof(text));
	CHECK(!strcmp(text, "Over your 4096 MB host limit."));
	ae_map_share_why_dialog(AE_MAP_SHARE_WHY_OTHER, 0, "Something new", text, sizeof(text));
	CHECK(!strcmp(text, "Something new"));
	ae_map_share_why_dialog(AE_MAP_SHARE_WHY_SETTING_OFF, 0, NULL, text, sizeof(text));
	CHECK(!strncmp(text, "DOWNLOAD MAPS is OFF.", 21));
	CHECK(!strcmp(ae_map_share_why_tag(AE_MAP_SHARE_WHY_DISK_FULL), "disk full"));

	/* requests: a burst, then one each refill; a clock that wraps */
	memset(&rate, 0, sizeof(rate));
	for (count = 0; count < 5; count++)
		CHECK(ae_map_share_rate_allow(&rate, 1000, 5, 100));
	CHECK(!ae_map_share_rate_allow(&rate, 1000, 5, 100) && !ae_map_share_rate_allow(&rate, 1099, 5, 100));
	CHECK(ae_map_share_rate_allow(&rate, 1100, 5, 100) && !ae_map_share_rate_allow(&rate, 1150, 5, 100));
	CHECK(ae_map_share_rate_allow(&rate, 1350, 5, 100) && ae_map_share_rate_allow(&rate, 1350, 5, 100) &&
		!ae_map_share_rate_allow(&rate, 1350, 5, 100));
	for (count = 0; count < 5; count++)
		CHECK(ae_map_share_rate_allow(&rate, 900000, 5, 100));
	CHECK(!ae_map_share_rate_allow(&rate, 900000, 5, 100));
	memset(&rate, 0, sizeof(rate));
	CHECK(ae_map_share_rate_allow(&rate, 0xFFFFFFF0UL, 1, 100) && !ae_map_share_rate_allow(&rate, 0xFFFFFFF8UL, 1, 100) &&
		ae_map_share_rate_allow(&rate, 0x60UL, 1, 100));
	CHECK(!ae_map_share_rate_allow(NULL, 0, 1, 1) && !ae_map_share_rate_allow(&rate, 0, 0, 1) &&
		!ae_map_share_rate_allow(&rate, 0, 1, 0));

	/* the plan: the maps lacked, in order, under the cap; the resource maps counted once with the first Custom
	Edition map; a map that does not fit is passed over, a smaller one after it still taken */
	CHECK(ae_map_share_catalogue_begin(&catalogue, 6));
	memset(&frame, 0, sizeof(frame));
	frame.type = AE_MAP_SHARE_FRAME_CATALOGUE;
	frame.expected = 6;
	frame.count = 6;
	frame.entries[0] = entry_of("one", AE_MAP_SHARE_KIND_XBOX, 10 << 20);
	frame.entries[1] = entry_of("big", AE_MAP_SHARE_KIND_MOD, 100 << 20);
	frame.entries[2] = entry_of("cemap_1a2b3c4d", AE_MAP_SHARE_KIND_CE, 20 << 20);
	frame.entries[3] = entry_of("cetwo_1a2b3c4d", AE_MAP_SHARE_KIND_CE, 20 << 20);
	frame.entries[4] = entry_of("bitmaps", AE_MAP_SHARE_KIND_RESOURCE, 30 << 20);
	frame.entries[5] = entry_of("sounds", AE_MAP_SHARE_KIND_RESOURCE, 5 << 20);
	CHECK(ae_map_share_catalogue_take(&catalogue, &frame) && catalogue.count == 6);
	count = ae_map_share_plan(&catalogue, lacks_all, NULL, 4096, 0, indices, 512, &total, &over);
	CHECK(count == 4 && over == 0 && total == (185ULL << 20) && indices[0] == 0 && indices[3] == 3);
	count = ae_map_share_plan(&catalogue, lacks_all, NULL, 90, 0, indices, 512, &total, &over);
	CHECK(count == 3 && over == 1 && total == (85ULL << 20) && indices[0] == 0 && indices[1] == 2 && indices[2] == 3);
	count = ae_map_share_plan(&catalogue, lacks_all, NULL, 90, 80ULL << 20, indices, 512, &total, &over);
	CHECK(count == 1 && over == 3 && total == (10ULL << 20));
	count = ae_map_share_plan(&catalogue, lacks_but_first, "one", 4096, 0, indices, 512, &total, &over);
	CHECK(count == 3 && indices[0] == 1);
	count = ae_map_share_plan(&catalogue, lacks_all, NULL, 0, 0, indices, 512, &total, &over);
	CHECK(count == 0 && over == 4 && total == 0);
	count = ae_map_share_plan(&catalogue, lacks_all, NULL, 4096, 0, indices, 2, &total, &over);
	CHECK(count == 2 && over == 2);
	CHECK(ae_map_share_plan(NULL, lacks_all, NULL, 4096, 0, indices, 2, &total, &over) == 0);

	/* M1: a file counted before is charged its growth when it is offered again larger (a host that lists a map
	small, then large), never nothing; the same size or a smaller one adds nothing; a key the table lost is charged
	whole again; the cap then refuses what the growth would pass */
	{
		static struct ae_map_share_charges charges;
		unsigned long long taken = 0;
		unsigned long long growth;
		char key[32];

		memset(&charges, 0, sizeof(charges));
		growth = ae_map_share_charge_growth(&charges, "0:foo", 4096);
		CHECK(growth == 4096 && ae_map_share_cap_allows(taken, growth, 100));
		ae_map_share_charge_set(&charges, "0:foo", 4096);
		taken += growth;
		CHECK(ae_map_share_charge_growth(&charges, "0:foo", 4096) == 0);
		CHECK(ae_map_share_charge_growth(&charges, "0:FOO", 4096) == 0);
		CHECK(ae_map_share_charge_growth(&charges, "0:foo", 100) == 0);
		CHECK(ae_map_share_charge_growth(&charges, "1:foo", 4096) == 4096);
		/* (the same level listed again at 128 MiB: all of the growth counts, and a 100 MB cap refuses it) */
		growth = ae_map_share_charge_growth(&charges, "0:foo", 128ULL << 20);
		CHECK(growth == (128ULL << 20) - 4096);
		CHECK(!ae_map_share_cap_allows(taken, growth, 100));
		CHECK(ae_map_share_cap_allows(taken, growth, 4096));
		ae_map_share_charge_set(&charges, "0:foo", 128ULL << 20);
		taken += growth;
		CHECK(taken == (128ULL << 20));
		/* (set smaller: the largest is kept) */
		ae_map_share_charge_set(&charges, "0:foo", 5);
		CHECK(ae_map_share_charge_growth(&charges, "0:foo", 128ULL << 20) == 0);
		/* (pushed out of the table by 96 others: whole again) */
		for (count = 0; count < AE_MAP_SHARE_CHARGES; count++)
		{
			snprintf(key, sizeof(key), "0:map%d", count);
			ae_map_share_charge_set(&charges, key, 10);
		}
		CHECK(ae_map_share_charge_growth(&charges, "0:foo", 128ULL << 20) == (128ULL << 20));
		CHECK(ae_map_share_charge_growth(&charges, "0:map95", 10) == 0);
		CHECK(ae_map_share_charge_growth(NULL, "0:foo", 7) == 7 && ae_map_share_charge_growth(&charges, NULL, 7) == 7);
	}
	/* M1: an offer's files are checked against the disk together (768 + 3 x 512 MiB do not fit a disk with 1 GiB
	free, though each alone would), and a disk whose free space is not known (0) takes nothing */
	{
		unsigned long long free_bytes = 1ULL << 30;
		unsigned long long sum = 0;
		unsigned long long sizes[4] = { 768ULL << 20, 512ULL << 20, 512ULL << 20, 512ULL << 20 };
		int taken_files = 0;

		for (count = 0; count < 4; count++)
		{
			CHECK(ae_map_share_disk_allows(free_bytes, sizes[count]) == (count == 0 ? 1 : 1));
			if (ae_map_share_disk_allows(free_bytes, sum + sizes[count]))
			{
				sum += sizes[count];
				taken_files++;
			}
		}
		CHECK(taken_files == 1);
		CHECK(!ae_map_share_disk_allows(0, 1));
	}
	/* overwrite refusal's path rule is the downloads' folder's own (above): a download's file is a plain name of
	maps_shared, nothing else */
	CHECK(ae_map_share_path_in_folder("d:\\maps_shared\\badcreek_1a2b3c4d.map", "d:\\maps_shared\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps\\badcreek.map", "d:\\maps_shared\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_shared\\..\\maps\\bloodgulch.map", "d:\\maps_shared\\"));
}

int main(void)
{
	/* private ranges, their edges */
	CHECK(ae_map_share_address_local(ip(10, 0, 0, 1)));
	CHECK(ae_map_share_address_local(ip(10, 255, 255, 254)));
	CHECK(ae_map_share_address_local(ip(172, 16, 0, 1)));
	CHECK(ae_map_share_address_local(ip(172, 31, 255, 254)));
	CHECK(!ae_map_share_address_local(ip(172, 15, 255, 255)));
	CHECK(!ae_map_share_address_local(ip(172, 32, 0, 0)));
	CHECK(ae_map_share_address_local(ip(192, 168, 0, 1)));
	CHECK(ae_map_share_address_local(ip(192, 168, 255, 255)));
	CHECK(!ae_map_share_address_local(ip(192, 169, 0, 1)));
	CHECK(!ae_map_share_address_local(ip(192, 167, 255, 255)));
	/* loopback, link-local */
	CHECK(ae_map_share_address_local(ip(127, 0, 0, 1)));
	CHECK(ae_map_share_address_local(ip(127, 0, 0, 200)));
	CHECK(ae_map_share_address_local(ip(169, 254, 10, 20)));
	CHECK(!ae_map_share_address_local(ip(169, 253, 10, 20)));
	/* public: never a LAN source */
	CHECK(!ae_map_share_address_local(ip(8, 8, 8, 8)));
	CHECK(!ae_map_share_address_local(ip(1, 1, 1, 1)));
	CHECK(!ae_map_share_address_local(ip(11, 0, 0, 1)));
	CHECK(!ae_map_share_address_local(ip(9, 255, 255, 255)));
	CHECK(!ae_map_share_address_local(ip(203, 0, 113, 7)));
	/* internet play's tunnel range (100.64/10) is reached by key, never as a LAN address */
	CHECK(!ae_map_share_address_local(ip(100, 64, 0, 1)));
	/* malformed: none, broadcast, multicast, more than 32 bits */
	CHECK(!ae_map_share_address_local(0));
	CHECK(!ae_map_share_address_local(0xFFFFFFFFUL));
	CHECK(!ae_map_share_address_local(ip(224, 0, 0, 1)));
	CHECK(!ae_map_share_address_local(ip(0, 0, 0, 10)));
	if (sizeof(unsigned long) > 4)
	{
		unsigned long high = ((unsigned long)1 << 16) << 16;

		CHECK(!ae_map_share_address_local(high | ip(192, 168, 0, 1)));
		CHECK(!ae_map_share_address_local(high | ip(10, 0, 0, 1)));
	}

	/* the paths a failed download may remove: a plain <name>.map in the folder */
	CHECK(ae_map_share_path_in_folder("d:\\maps_ce\\hugeass_1a2b3c4d.map", "d:\\maps_ce\\"));
	CHECK(ae_map_share_path_in_folder("d:\\maps_ce\\bitmaps.map", "d:\\maps_ce\\"));
	CHECK(ae_map_share_path_in_folder("d:\\maps_ce\\X.MAP", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\hugeass.txt", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\hugeass.map.part", "d:\\maps_ce\\"));
	/* traversal and other folders */
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\..\\maps\\bloodgulch.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\..map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\a..b.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\sub\\x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\sub/x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\c:x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps\\bloodgulch.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce_other\\x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("e:\\maps_ce\\x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\x\n.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder(NULL, "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\x.map", NULL));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\x.map", ""));

	/* discarding: only the listed paths in the folder reach the remover */
	{
		char const *paths[] = { "d:\\maps_ce\\bad_00000000.map", "d:\\maps_ce\\..\\maps\\a10.map",
			"d:\\maps\\bloodgulch.map", "d:\\maps_ce\\sounds.map" };
		int calls = 0;

		CHECK(ae_map_share_discard(paths, 4, "d:\\maps_ce\\", delete_logged, &calls) == 2);
		CHECK(calls == 2);
		calls = 0;
		CHECK(ae_map_share_discard(paths, 0, "d:\\maps_ce\\", delete_logged, &calls) == 0 && calls == 0);
		CHECK(ae_map_share_discard(NULL, 4, "d:\\maps_ce\\", delete_logged, &calls) == 0 && calls == 0);
	}

	/* on disk: the failed download's file goes; a good map beside it, and a file outside the folder named
	through "..", stay */
	{
		char const *scratch = getenv("AE_TEST_SCRATCH");
		char root[160];
		char folder[192], outside[192], bad[256], good[256], traversal[256];
		char const *paths[2];
		int calls = 0;

		snprintf(root, sizeof(root), "%s/ae_map_share_rules_XXXXXX", scratch && scratch[0] ? scratch : ".");
		if (!mkdtemp(root))
		{
			CHECK(!"mkdtemp");
			return 1;
		}
		snprintf(folder, sizeof(folder), "%s/maps_ce/", root);
		CHECK(mkdir(folder, 0700) == 0);
		snprintf(outside, sizeof(outside), "%s/keep.map", root);
		snprintf(bad, sizeof(bad), "%sbad_00000000.map", folder);
		snprintf(good, sizeof(good), "%sgood_11111111.map", folder);
		snprintf(traversal, sizeof(traversal), "%s../keep.map", folder);
		touch(outside);
		touch(bad);
		touch(good);
		paths[0] = bad;
		paths[1] = traversal;
		CHECK(ae_map_share_discard(paths, 2, folder, delete_real, &calls) == 1);
		CHECK(calls == 1);
		CHECK(!exists(bad));
		CHECK(exists(good));
		CHECK(exists(outside));
		remove(good);
		remove(outside);
		rmdir(folder);
		rmdir(root);
	}

	share_all_tests();

	if (failures)
		return 1;
	printf("ae_map_share_rules_test: all passed\n");
	return 0;
}
