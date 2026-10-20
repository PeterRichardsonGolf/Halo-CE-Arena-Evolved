/*
AE_MAP_SHARE_ALL.C

Arena Evolved: map sharing for every map but the retail game's own (0.2.3). The rules are ae_map_share_rules.c's
(pure, unit-tested); this is their engine side. OpenCE's map sharing (map_share.c) still carries every byte: its
requests, offers, connections, pieces and hashes. This adds what it lacks, through AE hooks (ae_hooks.txt):

- THE HOST'S CATALOGUE. A host lists every map it has that is not a stock one: community maps in the Xbox's format
  (the maps folder's, its active mod's, those it downloaded into maps_shared) and Custom Edition maps named for their
  hashes (maps_ce, custom_maps). Each is hashed once (map sharing's own hash), a little each frame. The list goes to
  each guest that runs this game (its join request's mark, ae_silent.c: ae_peer_is_ae), in frames that ride map
  sharing's offer message with a level name no map has: a machine without this code never gets one, and would drop
  it (an offer of more files than there are kinds).
- THE PLAYED MAP. A guest without the game's map asks for it as Custom Edition maps are asked for: a community map
  too, into maps_shared, where the game then finds it.
- GET ALL. In the lobby, F6 (or BACK held on a pad) fetches every listed map the guest lacks, one after
  another; the same again stops it. The game's own map goes first; a game that starts stops the rest until the
  lobby is back.
- CONSENT AND LIMITS. Nothing is asked of a host before the player said yes, once per host; a size over the
  engine's limit for the kind, the total cap (network.map_download_limit), a full disk, a name the list did not
  carry, a size or hash that is not the list's: refused before a byte is stored. Internet games share and download
  only with both switches on (network.map_share_internet, network.map_download_internet).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "bungie_net/network/transport.h"
#include "bungie_net/network/transport_endpoint_winsock.h"
#include "bungie_net/network/transport_address_constants.h"
#include "cache/cache_files.h"
#include "../src/ae_discs.h"
#include "game/game.h"
#include "interface/interface.h"
#include "interface/ui_widget.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_client_manager.h"
#include "networking/network_connection.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "rasterizer/rasterizer.h"
#include "network_distributed.h"
#include "map_share.h"
#include "ae_map_share.h"
#include "ae_map_share_rules.h"
#include "ae_layout.h"
#include "ae_layout_share_rules.h"
#include "ae_silent.h"
#include "ae_map_folders.h"
#include "halo_map_families.h"
#include "halo_custom_maps.h"
#include "../src/p2p.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

#undef malloc
#undef free

int config_boolean(const char *name);
long config_integer(const char *name);
unsigned long config_changes(void);
const char *config_string(const char *name);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);
unsigned long network_game_server_machine_address(long machine_index);
unsigned long system_milliseconds(void);
int ae_platform_map_share_key(void);
int ae_platform_map_share_pad(void);

/* ---------- constants */

enum
{
	MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES,
	LEVEL_BYTES = 64,
	PATH_BYTES = MAP_SHARE_PATH_SIZE,
	XBOX_HEADER_BYTES = 2048,
	/* an offer message's own bytes before its files (map_share.c's distributed_map_share_offer) */
	OFFER_BYTES = 68,
	OFFER_RESULT = 64,
	OFFER_FILE_COUNT = 65,
	/* the host hashes this many pieces a frame, and sends a guest this many frames of its list a frame */
	HASH_PIECES_PER_UPDATE = 4,
	FRAMES_PER_UPDATE = 2,
	/* a host's list is read again from its folders for a new guest this long after the last time */
	RESCAN_INTERVAL = 30000,
	/* a machine's requests: this many at once, one more each REQUEST_REFILL milliseconds */
	REQUEST_BURST = 24,
	REQUEST_REFILL = 2500,
	/* a guest waits this long for a host to say it runs this game, and for its list */
	HELLO_WAIT = 5000,
	CATALOGUE_WAIT = 120000,
	/* a host's lists a guest takes in one game (a list started anew again and again is dropped) */
	CATALOGUE_RESTARTS = 8,
	/* the question to get all of a host's maps is shown this long */
	PROMPT_TIME = 20000,
	/* a map of the queue is tried this often (the game's own map takes its place meanwhile) */
	QUEUE_ATTEMPTS = 3,
	REMEMBERED_HOSTS = 8,
	/* listed files that are not the game's sent at once (one a machine); with the game's own 4 and the 3 resource
	maps they stay under map sharing's 16 hashed files */
	FLIGHTS = 6,
	FLIGHT_TIMEOUT = 45000,
	RATES = 64,
	/* the question must have been shown this long before a yes is taken, and shown this lately */
	QUESTION_MINIMUM_TIME = 1000,
	QUESTION_SHOWN_LATELY = 500,
	/* GET ALL waits this long after a host said it does not share (asked too often) */
	QUEUE_PAUSE = 10000,
	/* a pad's BACK is map sharing's F6 once held this long, in the lobby (a tap is the menus') */
	PAD_HOLD_TIME = 1000,
	_text_centered = 2,
};

/* the frames' level name: no map's (it starts with a character no name has) */
static char const frame_level_name[] = "\x7F" "AE maps";
#define FRAME_RESULT 0xAE
#define FRAME_FILE_COUNT 0xFF

#define XBOX_DOWNLOAD_FOLDER "d:\\maps_shared\\"
#define CE_DOWNLOAD_FOLDER "d:\\maps_ce\\"

typedef char xbox_limit_assert[AE_MAP_SHARE_XBOX_MAXIMUM_BYTES == HALO_PORT_MULTIPLAYER_CACHE_SIZE ? 1 : -1];
typedef char hash_bytes_assert[AE_MAP_SHARE_HASH_BYTES == MAP_SHARE_HASH_BYTES ? 1 : -1];

/* ---------- structures */

struct host_entry
{
	struct ae_map_share_entry entry;
	char path[PATH_BYTES];
	unsigned long modified;
	boolean hashed;
	/* its pieces' hashes, kept from the list's hashing: handed to map sharing when the file is asked for, so that
	it is not read and hashed a second time (NULL: a resource map, which map sharing hashes itself) */
	byte *pieces;
	long piece_count;
};

/* a listed file that is not the game's, being sent to a machine: map sharing's catalogue of hashed files (16
places, replaced in turn) keeps its place while it is (map_share_files.c's hook, ae_map_share_file_kept) */
struct host_flight
{
	char path[PATH_BYTES];
	unsigned long address;
	long machine_index;
	unsigned long asked_at;
	boolean custom_edition;
};

/* a machine's requests counted by its address, which outlives its place in the game (leaving and joining again
does not start the count over) */
struct host_rate
{
	unsigned long address;
	struct ae_map_share_rate rate;
};

struct host_peer
{
	unsigned long address;
	boolean hello_sent;
	boolean shared;
	long next_entry;
	boolean listed;
	/* which list it was sent (the host's lists are numbered) */
	long list_serial;
	char requested[LEVEL_BYTES];
	/* the layouts it was sent (ae_layout_share_rules.h): the game's map's and the last other map's it asked for, by
	their texts' serials; the one being sent, and how far */
	char layout_game[LEVEL_BYTES];
	unsigned int layout_game_serial;
	char layout_extra[LEVEL_BYTES];
	unsigned int layout_extra_serial;
	boolean layout_busy;
	boolean layout_busy_game;
	char layout_level[LEVEL_BYTES];
	unsigned int layout_serial;
	long layout_offset;
};

/* a layout this host sends (ae_layout_share_rules.h): a level's, read from its layout file now and then (has: one this
host applies), its text's serial new for each new text */
struct host_layout
{
	char level[LEVEL_BYTES];
	boolean has;
	long length;
	unsigned int serial;
	unsigned long read_at;
	char text[AE_LAYOUT_MAXIMUM_FILE_SIZE];
};

struct remembered_host
{
	unsigned long address;
	unsigned long long taken;
};

struct share_globals
{
	/* the host's list, and the one before (its hashes kept for the files that did not change) */
	struct host_entry entries[AE_MAP_SHARE_CATALOGUE_MAXIMUM];
	struct host_entry previous[AE_MAP_SHARE_CATALOGUE_MAXIMUM];
	long entry_count;
	long previous_count;
	boolean scanned;
	boolean ready;
	long serial;
	unsigned long scanned_at;
	/* the entry being hashed */
	long hashing;
	FILE *hash_stream;
	byte *hash_pieces;
	long hash_piece;
	long hash_piece_count;
	struct host_peer peers[MACHINES];
	struct host_flight flights[FLIGHTS];
	struct host_rate rates[RATES];
	long rates_next;
	boolean choosing_place;

	/* the guest's: its host, what the host said, and the player's answer */
	unsigned long host_address;
	unsigned long joined_at;
	boolean host_is_ae;
	boolean host_shares;
	boolean catalogue_begun;
	long catalogue_restarts;
	struct ae_map_share_catalogue catalogue;
	int consent;
	unsigned long long taken;
	struct ae_map_share_charges charges;
	/* the yes: the bytes it was given for and what had been taken when it was; the offer being taken's files' sum
	(the disk must hold them together) */
	unsigned long long granted;
	unsigned long long grant_base;
	unsigned long long offer_bytes;
	/* the question: the game's map's size if the list has it; when it was first and last shown this time (0: not
	shown), and whether the key was seen up since */
	unsigned long long played_bytes;
	unsigned long question_first_shown;
	unsigned long question_last_shown;
	boolean question_key_clear;
	unsigned long queue_pause_until;
	struct remembered_host remembered[REMEMBERED_HOSTS];
	long remembered_next;
	/* the plan: what GET ALL would fetch */
	short plan[AE_MAP_SHARE_CATALOGUE_MAXIMUM];
	long plan_count;
	unsigned long long plan_bytes;
	long plan_over;
	unsigned long planned_at;
	/* GET ALL running: the maps it set out to fetch, the next, those fetched and those given up */
	boolean all_active;
	boolean all_wanted;
	unsigned long all_asked_at;
	short queue[AE_MAP_SHARE_CATALOGUE_MAXIMUM];
	long queue_count;
	long queue_next;
	long queue_done;
	long queue_failed;
	long queue_attempts;
	char active_level[LEVEL_BYTES];
	boolean active_cancel;
	/* (it was stopped, to be taken up again; or it failed, and is given up) */
	boolean active_stopped;
	boolean active_failed;
	/* the game's map waited for (its list not here yet), the level whose request went out, and whether the
	download's folder is the Xbox maps' */
	char pending_level[LEVEL_BYTES];
	unsigned long pending_at;
	char released_level[LEVEL_BYTES];
	boolean folder_xbox;
	boolean played_asking;
	char refusal[200];
	char notice[200];
	unsigned long notice_until;
	boolean key_down;
	unsigned long pad_down_at;
	boolean test_all_done;
};

static struct share_globals share;

enum
{
	HOST_LAYOUTS = 4,
	/* the game's map's layout file is read again this often (a file changed is sent again) */
	HOST_LAYOUT_READ_INTERVAL = 3000,
};
static struct host_layout host_layouts[HOST_LAYOUTS];
static unsigned int host_layout_serial;
/* the guest's: its host's layouts */
static struct ae_layout_share_inbox layout_inbox;

/* ---------- small things */

static boolean elapsed(
	unsigned long since,
	unsigned long now,
	unsigned long time)
{
	return (long)(now - since) >= (long)time;
}

static boolean file_exists(
	char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	CloseHandle(file);
	return TRUE;
}

static boolean file_stamp(
	char const *path,
	unsigned long *size,
	unsigned long *modified)
{
	WIN32_FILE_ATTRIBUTE_DATA data;

	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data) || data.nFileSizeHigh ||
		(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return FALSE;
	}
	*size = data.nFileSizeLow;
	*modified = data.ftLastWriteTime.dwLowDateTime ^ data.ftLastWriteTime.dwHighDateTime;
	return TRUE;
}

static boolean is_host(
	void)
{
	return global_network_game_server_get() != NULL;
}

static struct network_game *client_game(
	void)
{
	struct network_game_client *client = global_network_game_client_get();

	return client ? network_game_client_get_game(client) : NULL;
}

/* the host's address as this guest's connection has it (host byte order; 0 for none, or a host) */
static unsigned long guest_host_address(
	void)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_connection *connection = client ? network_game_client_get_connection(client) : NULL;
	struct transport_address reliable;
	struct transport_address unreliable;

	if (!connection || is_host())
		return 0;
	csmemset(&reliable, 0, sizeof(reliable));
	network_connection_get_address(connection, &reliable, &unreliable);
	return reliable.address_length == IPV4_ADDRESS_LENGTH ? reliable.address.long_words[0] : 0;
}

/* a level's level_family and its file's name; whether it is an Xbox map's that a list may carry (never a stock one) */
static boolean xbox_level_file(
	char const *level_name,
	char *file,
	long size)
{
	if (!level_name || !level_name[0] || strlen(level_name) >= LEVEL_BYTES ||
		map_family_parse(level_name, file, size) != _map_family_xbox)
	{
		return FALSE;
	}
	return ae_map_share_name_allowed(file) && strlen(file) <= AE_MAP_SHARE_XBOX_NAME_LENGTH &&
		!ae_map_share_stock_name(file) && map_share_name_safe(file);
}

/* whether an Xbox map's file is no longer than its header's length allows. The header's length is the map's
UNPACKED size (the file is its header and then the rest compressed: Blood Gulch is 22.5 MB of file for a header
that says 44.3 MB), so the two are not equal; but a file cannot be longer than what it unpacks to, but for the
compression's own few bytes and the padding to a disc sector: a file with megabytes of anything after a small
map's data is refused */
static boolean xbox_length_fits(
	unsigned char const *header,
	unsigned long size)
{
	unsigned long length = (unsigned long)header[8] | (unsigned long)header[9] << 8 | (unsigned long)header[10] << 16 |
		(unsigned long)header[11] << 24;

	return size <= length || size - length <= 0x10000;
}

static boolean same_level(
	char const *a,
	char const *b)
{
	return a[0] && !_stricmp(a, b);
}

static long host_find(
	char const *name,
	boolean custom_edition)
{
	long index;

	for (index = 0; index < share.entry_count; index++)
	{
		struct ae_map_share_entry const *entry = &share.entries[index].entry;

		if ((entry->kind == AE_MAP_SHARE_KIND_CE) == (custom_edition != FALSE) &&
			entry->kind != AE_MAP_SHARE_KIND_RESOURCE && !_stricmp(entry->name, name))
		{
			return index;
		}
	}
	return NONE;
}

static void notice_set(
	char const *text)
{
	csstrncpy(share.notice, text, sizeof(share.notice) - 1);
	share.notice[sizeof(share.notice) - 1] = 0;
	share.notice_until = system_milliseconds() + 8000;
	error(_error_silent, "map sharing: %s", text);
}

static long megabytes(
	unsigned long long bytes)
{
	return (long)((bytes + 0xFFFFF) >> 20);
}

/* ---------- settings */

static boolean test_mode(
	char const *mode)
{
	char const *setting = config_string("debug.map_share_test");
	char const *network_test = config_string("debug.network_test");

	/* (only in an automated network test, debug.network_test: never a way to play with no question) */
	return setting && network_test && network_test[0] && !strcmp(setting, mode);
}

static boolean host_policy(
	unsigned long address)
{
	/* (debug.map_share_test "internet": every machine taken for an internet one, for the tests) */
	return ae_map_share_policy_allows(ae_map_share_address_local(address) && !test_mode("internet"),
		config_boolean("network.map_share") != 0, config_boolean("network.map_share_internet") != 0) != 0;
}

static boolean guest_policy(
	void)
{
	unsigned long address = guest_host_address();

	/* (debug.map_share_test "internet": this host taken for an internet one, for the tests) */
	return ae_map_share_policy_allows(ae_map_share_address_local(address) && !test_mode("internet"),
		config_boolean("network.map_download") != 0, config_boolean("network.map_download_internet") != 0) != 0;
}

static long guest_cap(
	void)
{
	return ae_map_share_cap_megabytes(config_integer("network.map_download_limit"));
}

/* ---------- the host's list */

static void host_hash_end(
	void)
{
	if (share.hash_stream)
		fclose(share.hash_stream);
	share.hash_stream = NULL;
	free(share.hash_pieces);
	share.hash_pieces = NULL;
	share.hashing = NONE;
}

/* a listed file's hashes handed to map sharing's catalogue as they are, if the file is still the one hashed: it is
not read again when a machine asks for it */
static void host_seed(
	struct host_entry const *entry)
{
	unsigned long size;
	unsigned long modified;

	/* (never from inside the catalogue's own choice of a place: ae_map_share_file_kept) */
	if (share.choosing_place || !entry->hashed || !entry->pieces || !file_stamp(entry->path, &size, &modified) ||
		size != entry->entry.size || modified != entry->modified)
	{
		return;
	}
	if (map_share_file_find(entry->entry.hash) != NONE)
		return;
	map_share_file_hashed(entry->path, _map_share_file_map, entry->entry.size, entry->pieces, entry->piece_count,
		entry->entry.hash);
}

/* a Custom Edition map's entry named for its hash, as the list sends it (network version 29 keeps maps under their own
names: the list still names each <name>_<hash>, which the client finds by its hash whatever its file is called,
map_family_find): a name of its hash already, checked; FALSE if it is another's or the name does not fit */
static boolean host_name_for_hash(
	struct host_entry *entry)
{
	char digits[MAP_SHARE_NAME_DIGITS + 1];
	size_t length = strlen(entry->entry.name);

	map_share_hash_digits(entry->entry.hash, digits);
	if (map_share_name_hashed(entry->entry.name))
		return csstrcmp(entry->entry.name + length - MAP_SHARE_NAME_DIGITS, digits) == 0;
	if (length + 1 + MAP_SHARE_NAME_DIGITS >= sizeof(entry->entry.name))
		return FALSE;
	snprintf(entry->entry.name + length, sizeof(entry->entry.name) - length, "_%s", digits);
	return ae_map_share_entry_valid(&entry->entry) && map_share_name_safe(entry->entry.name);
}

static void host_add(
	char const *path,
	char const *name,
	int kind)
{
	struct host_entry *entry;
	unsigned long size;
	unsigned long modified;
	long index;

	if (share.entry_count >= AE_MAP_SHARE_CATALOGUE_MAXIMUM)
	{
		error(_error_silent, "map sharing: the list is full (%d maps): %s is not shared",
			AE_MAP_SHARE_CATALOGUE_MAXIMUM, name);
		return;
	}
	if (strlen(path) >= PATH_BYTES || strlen(name) >= AE_MAP_SHARE_NAME_BYTES || !file_stamp(path, &size, &modified))
		return;
	entry = &share.entries[share.entry_count];
	csmemset(entry, 0, sizeof(*entry));
	csstrncpy(entry->entry.name, name, sizeof(entry->entry.name) - 1);
	entry->entry.kind = (unsigned char)kind;
	entry->entry.size = size;
	csstrncpy(entry->path, path, sizeof(entry->path) - 1);
	entry->modified = modified;
	/* (a Custom Edition map by its own name is checked once it is named for its hash: host_name_for_hash) */
	if ((!ae_map_share_entry_valid(&entry->entry) && !(kind == AE_MAP_SHARE_KIND_CE && !map_share_name_hashed(name))) ||
		(kind != AE_MAP_SHARE_KIND_RESOURCE && !map_share_name_safe(name)))
	{
		return;
	}
	/* (one name once: the first folder's is the one the game loads) */
	for (index = 0; index < share.entry_count; index++)
	{
		if ((share.entries[index].entry.kind == AE_MAP_SHARE_KIND_RESOURCE) == (kind == AE_MAP_SHARE_KIND_RESOURCE) &&
			(share.entries[index].entry.kind == AE_MAP_SHARE_KIND_CE) == (kind == AE_MAP_SHARE_KIND_CE) &&
			!_stricmp(share.entries[index].entry.name, name))
		{
			return;
		}
	}
	/* (hashed before, the file as it was) */
	for (index = 0; index < share.previous_count; index++)
	{
		struct host_entry const *before = &share.previous[index];

		if (before->hashed && before->pieces && before->entry.size == size && before->modified == modified &&
			!_stricmp(before->path, path))
		{
			csmemcpy(entry->entry.hash, before->entry.hash, sizeof(entry->entry.hash));
			entry->pieces = before->pieces;
			entry->piece_count = before->piece_count;
			share.previous[index].pieces = NULL;
			entry->hashed = TRUE;
			if (kind == AE_MAP_SHARE_KIND_CE && !host_name_for_hash(entry))
			{
				free(entry->pieces);
				return;
			}
			break;
		}
	}
	/* (a resource map is listed for its size alone: the engine hashes it when a map that needs it is asked for) */
	if (kind == AE_MAP_SHARE_KIND_RESOURCE)
		entry->hashed = TRUE;
	share.entry_count++;
}

/* a folder's maps in the Xbox's format that the game would list (custom_maps.c's own test), stock ones left out */
static void host_scan_xbox(
	char const *folder,
	int kind)
{
	WIN32_FIND_DATAA data;
	char pattern[PATH_BYTES];
	HANDLE find;

	if (snprintf(pattern, sizeof(pattern), "%s*.map", folder) >= (int)sizeof(pattern))
		return;
	find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return;
	do
	{
		unsigned char header[XBOX_HEADER_BYTES];
		char path[PATH_BYTES];
		char name[HALO_CUSTOM_MAP_NAME_SIZE + 1];
		unsigned long read = 0;
		HANDLE file;

		if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
			snprintf(path, sizeof(path), "%s%s", folder, data.cFileName) >= (int)sizeof(path))
		{
			continue;
		}
		file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (file == INVALID_HANDLE_VALUE)
			continue;
		if (!ReadFile(file, header, sizeof(header), &read, NULL))
			read = 0;
		CloseHandle(file);
		if (read != sizeof(header))
			continue;
		/* (debug.map_share_test "lie-map": a host that lists a file under its file's name whatever its header says,
		for the tests of what a guest does with one) */
		if (test_mode("lie-map") && strlen(data.cFileName) > 4 && strlen(data.cFileName) - 4 <= HALO_CUSTOM_MAP_NAME_SIZE)
		{
			snprintf(name, sizeof(name), "%.*s", (int)(strlen(data.cFileName) - 4), data.cFileName);
			if (!ae_map_share_stock_name(name))
				host_add(path, name, kind);
			continue;
		}
		/* (version 5, a build the game plays multiplayer with, its header's name its file's, and no longer than
		its header says: what a guest checks of what it downloaded) */
		if (!native_map_header_valid(header, data.cFileName) || data.nFileSizeHigh ||
			!xbox_length_fits(header, data.nFileSizeLow))
		{
			continue;
		}
		csmemcpy(name, header + 0x20, HALO_CUSTOM_MAP_NAME_SIZE);
		name[HALO_CUSTOM_MAP_NAME_SIZE] = 0;
		if (ae_map_share_stock_name(name))
			continue;
		host_add(path, name, kind);
	}
	while (FindNextFileA(find, &data));
	CloseHandle(find);
}

static void host_scan_custom_edition(
	char const *folder)
{
	WIN32_FIND_DATAA data;
	char pattern[PATH_BYTES];
	HANDLE find;

	if (snprintf(pattern, sizeof(pattern), "%s*.map", folder) >= (int)sizeof(pattern))
		return;
	find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return;
	do
	{
		char path[PATH_BYTES];
		char name[PATH_BYTES];
		size_t length = strlen(data.cFileName);

		if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || length <= 4 || length - 4 >= AE_MAP_SHARE_NAME_BYTES ||
			snprintf(path, sizeof(path), "%s%s", folder, data.cFileName) >= (int)sizeof(path))
		{
			continue;
		}
		snprintf(name, sizeof(name), "%.*s", (int)(length - 4), data.cFileName);
		/* (by its own name or one of its hash: listed named for its hash once hashed, host_name_for_hash) */
		host_add(path, name, AE_MAP_SHARE_KIND_CE);
	}
	while (FindNextFileA(find, &data));
	CloseHandle(find);
}

/* the folders of the maps folder, as the game finds their maps (ae_map_folders.c) */
static void host_scan_xbox_folder(
	char const *folder)
{
	host_scan_xbox(folder, AE_MAP_SHARE_KIND_XBOX);
}

static void host_scan(
	void)
{
	char const *mod = cache_files_mod_started();
	boolean custom_edition = FALSE;
	long index;

	host_hash_end();
	for (index = 0; index < share.previous_count; index++)
		free(share.previous[index].pieces);
	csmemcpy(share.previous, share.entries, sizeof(share.previous));
	share.previous_count = share.entry_count;
	share.entry_count = 0;
	share.ready = FALSE;
	/* (the game's own order: the mod played, the maps folder, its folders A-Z, then the downloads) */
	if (mod && mod[0] && !strchr(mod, '\\') && !strchr(mod, '/') && !strstr(mod, ".."))
	{
		char folder[PATH_BYTES];

		if (snprintf(folder, sizeof(folder), "d:\\%s\\%s\\maps\\", ae_disc_folder(mod), mod) < (int)sizeof(folder))
			host_scan_xbox(folder, AE_MAP_SHARE_KIND_MOD);
	}
	host_scan_xbox(cache_files_map_directory(), AE_MAP_SHARE_KIND_XBOX);
	ae_map_folders_scan(host_scan_xbox_folder);
	host_scan_xbox(XBOX_DOWNLOAD_FOLDER, AE_MAP_SHARE_KIND_XBOX);
	if (halo_custom_edition_tag_cache())
	{
		host_scan_custom_edition("d:\\maps_ce\\");
		host_scan_custom_edition("d:\\custom_maps\\");
		ae_map_folders_ce_scan(host_scan_custom_edition);
	}
	for (index = 0; index < share.entry_count; index++)
		custom_edition |= share.entries[index].entry.kind == AE_MAP_SHARE_KIND_CE;
	if (custom_edition)
	{
		short type;

		for (type = _resource_map_bitmaps; type < NUMBER_OF_RESOURCE_MAP_TYPES; type++)
		{
			char path[PATH_BYTES];

			if (custom_edition_resource_map_file_path(type, path))
				host_add(path, resource_map_type_describe((enum resource_map_type)type), AE_MAP_SHARE_KIND_RESOURCE);
		}
	}
	share.scanned = TRUE;
	share.scanned_at = system_milliseconds();
	share.serial++;
	error(_error_silent, "map sharing: this host lists %ld maps for the players who run this game", share.entry_count);
}

/* the list's hashing, a few pieces a frame; an entry whose file cannot be read, changed, or (a Custom Edition
map's) is not named for its hash leaves the list */
static void host_hash_update(
	void)
{
	long budget = HASH_PIECES_PER_UPDATE;

	while (budget > 0)
	{
		struct host_entry *entry;
		unsigned long size;
		unsigned long modified;
		boolean failed = FALSE;

		if (share.hashing == NONE)
		{
			long index;

			for (index = 0; index < share.entry_count && share.entries[index].hashed; index++)
				;
			if (index == share.entry_count)
			{
				if (!share.ready)
				{
					share.ready = TRUE;
					error(_error_silent, "map sharing: the list is hashed (%ld maps)", share.entry_count);
				}
				return;
			}
			entry = &share.entries[index];
			share.hash_piece_count = map_share_piece_count(entry->entry.size);
			share.hash_piece = 0;
			share.hash_stream = fopen(entry->path, "rb");
			share.hash_pieces = malloc((size_t)MAX(share.hash_piece_count, 1) * MAP_SHARE_HASH_BYTES);
			share.hashing = index;
			if (!share.hash_stream || !share.hash_pieces)
				failed = TRUE;
		}
		entry = &share.entries[share.hashing];
		while (!failed && budget > 0 && share.hash_piece < share.hash_piece_count)
		{
			static byte piece[MAP_SHARE_PIECE_BYTES];
			unsigned long offset = (unsigned long)share.hash_piece * MAP_SHARE_PIECE_BYTES;
			unsigned long bytes = MIN((unsigned long)MAP_SHARE_PIECE_BYTES, entry->entry.size - offset);

			if (fread(piece, 1, bytes, share.hash_stream) != bytes)
			{
				failed = TRUE;
				break;
			}
			p2p_blake2b_256(piece, bytes, share.hash_pieces + share.hash_piece * MAP_SHARE_HASH_BYTES);
			share.hash_piece++;
			budget--;
		}
		if (!failed && share.hash_piece < share.hash_piece_count)
			return;
		if (!failed && (!file_stamp(entry->path, &size, &modified) || size != entry->entry.size ||
			modified != entry->modified))
		{
			failed = TRUE;
		}
		if (!failed)
		{
			map_share_file_hash_of_pieces(entry->entry.size, share.hash_pieces, share.hash_piece_count,
				entry->entry.hash);
			if (entry->entry.kind == AE_MAP_SHARE_KIND_CE)
				failed = !host_name_for_hash(entry);
		}
		if (failed)
		{
			long index = share.hashing;

			error(_error_silent, "map sharing: %s could not be hashed (or is not named for its hash): not listed",
				entry->path);
			host_hash_end();
			memmove(&share.entries[index], &share.entries[index + 1],
				(size_t)(share.entry_count - index - 1) * sizeof(share.entries[0]));
			share.entry_count--;
		}
		else
		{
			entry->hashed = TRUE;
			entry->pieces = share.hash_pieces;
			entry->piece_count = share.hash_piece_count;
			share.hash_pieces = NULL;
			host_hash_end();
		}
		budget--;
	}
}

enum
{
	FRAME_BODY_BYTES = AE_MAP_SHARE_WIRE_FRAME_BYTES + AE_MAP_SHARE_FRAME_ENTRIES * AE_MAP_SHARE_WIRE_ENTRY_BYTES,
};
typedef char layout_frame_assert[AE_LAYOUT_SHARE_FRAME_MAXIMUM_BYTES <= FRAME_BODY_BYTES ? 1 : -1];

/* a frame's body (a list's, or a layout's) sent to a machine in map sharing's offer message */
static boolean host_send_body(
	long machine_index,
	byte const *bytes,
	long body)
{
	struct
	{
		struct distributed_message_header header;
		byte offer[OFFER_BYTES];
		byte body[FRAME_BODY_BYTES];
	} message;
	word size = (word)(sizeof(message.header) + sizeof(message.offer) + body);

	if (body <= 0 || body > (long)sizeof(message.body))
		return FALSE;
	csmemcpy(message.body, bytes, (size_t)body);
	csmemset(&message.header, 0, sizeof(message.header));
	csmemset(message.offer, 0, sizeof(message.offer));
	build_message_header(&message.header.header, size, 2, 0);
	message.header.type = _distributed_message_map_share_offer;
	message.header.count = 1;
	message.header.game_time = game_in_progress() ? game_time_get() : 0;
	csmemcpy(message.offer, frame_level_name, sizeof(frame_level_name));
	message.offer[OFFER_RESULT] = FRAME_RESULT;
	message.offer[OFFER_FILE_COUNT] = FRAME_FILE_COUNT;
	return network_distributed_server_send_to_machine_reliably(machine_index, &message, size);
}

static boolean host_send_frame(
	long machine_index,
	struct ae_map_share_frame const *frame)
{
	byte body[FRAME_BODY_BYTES];
	long length = ae_map_share_frame_encode(frame, body, sizeof(body));

	return length ? host_send_body(machine_index, body, length) : FALSE;
}

/* ---------- the host's layouts (layouts travel with maps, ae_layout_share_rules.h) */

static struct host_layout *host_layout_find(
	char const *level_name)
{
	long index;

	for (index = 0; index < HOST_LAYOUTS; index++)
	{
		if (same_level(host_layouts[index].level, level_name))
			return &host_layouts[index];
	}
	return NULL;
}

/* a level's layout as this host applies it, read from its file if it was not, or (fresh) not lately */
static struct host_layout *host_layout_get(
	char const *level_name,
	boolean fresh)
{
	static char text[AE_LAYOUT_MAXIMUM_FILE_SIZE];
	unsigned long now = system_milliseconds();
	struct host_layout *entry;
	long length = 0;
	boolean has;

	if (!level_name || !level_name[0] || strlen(level_name) >= LEVEL_BYTES)
		return NULL;
	entry = host_layout_find(level_name);
	if (entry && (!fresh || !elapsed(entry->read_at, now, HOST_LAYOUT_READ_INTERVAL)))
		return entry;
	has = ae_layout_host_text(level_name, text, sizeof(text), &length);
	if (entry && entry->has == has && (!has || (entry->length == length && !memcmp(entry->text, text, (size_t)length))))
	{
		entry->read_at = now;
		return entry;
	}
	if (!entry)
	{
		long index;

		/* (in place of the one read longest ago) */
		entry = &host_layouts[0];
		for (index = 0; index < HOST_LAYOUTS; index++)
		{
			if (!host_layouts[index].level[0])
			{
				entry = &host_layouts[index];
				break;
			}
			if ((long)(host_layouts[index].read_at - entry->read_at) < 0)
				entry = &host_layouts[index];
		}
	}
	csstrncpy(entry->level, level_name, sizeof(entry->level) - 1);
	entry->level[sizeof(entry->level) - 1] = 0;
	entry->has = has;
	entry->length = has ? length : 0;
	if (has)
		csmemcpy(entry->text, text, (size_t)length);
	entry->serial = ++host_layout_serial & 0xFFFF;
	entry->read_at = now;
	if (has)
		error(_error_silent, "map sharing: this host's layout for %s (%ld bytes) goes to its guests that run this game",
			level_name, length);
	else
		error(_error_silent, "map sharing: this host has no layout for %s: its guests that run this game are told so",
			level_name);
	return entry;
}

/* a guest that runs this game sent the layout of the game's map (or word that there is none), else of the other map
it asked for last; a few frames each frame, the rest next time (a queue that is full: the same frame again) */
static void host_layout_update(
	long machine_index,
	struct host_peer *peer)
{
	struct network_game *game = client_game();
	char const *game_level = game && game->map.name[0] && strlen(game->map.name) < LEVEL_BYTES ? game->map.name : NULL;
	struct host_layout *entry = NULL;
	long frames;

	if (peer->layout_busy)
	{
		entry = host_layout_find(peer->layout_level);
		/* (its text changed, or it was given up for another level's: begun again) */
		if (!entry || entry->serial != peer->layout_serial ||
			(peer->layout_busy_game && (!game_level || !same_level(game_level, peer->layout_level))))
		{
			peer->layout_busy = FALSE;
			entry = NULL;
		}
	}
	if (!peer->layout_busy)
	{
		boolean is_game = FALSE;

		if (game_level)
		{
			entry = host_layout_get(game_level, TRUE);
			is_game = entry != NULL;
			if (entry && same_level(peer->layout_game, entry->level) && peer->layout_game_serial == entry->serial)
				entry = NULL, is_game = FALSE;
		}
		if (!entry && peer->shared && peer->requested[0] && (!game_level || !same_level(game_level, peer->requested)))
		{
			entry = host_layout_get(peer->requested, FALSE);
			if (entry && same_level(peer->layout_extra, entry->level) && peer->layout_extra_serial == entry->serial)
				entry = NULL;
		}
		if (!entry)
			return;
		peer->layout_busy = TRUE;
		peer->layout_busy_game = is_game;
		csmemcpy(peer->layout_level, entry->level, sizeof(peer->layout_level));
		peer->layout_serial = entry->serial;
		peer->layout_offset = 0;
	}
	for (frames = 0; frames < FRAMES_PER_UPDATE; frames++)
	{
		struct ae_layout_share_frame frame;
		byte body[AE_LAYOUT_SHARE_FRAME_MAXIMUM_BYTES];
		long length;

		if (!ae_layout_share_frame_make(entry->level, entry->serial, entry->has ? entry->text : NULL, entry->length,
			peer->layout_offset, &frame))
		{
			peer->layout_busy = FALSE;
			return;
		}
		length = ae_layout_share_frame_encode(&frame, body, sizeof(body));
		if (!length || !host_send_body(machine_index, body, length))
			return;
		peer->layout_offset += frame.length;
		if (!entry->has || peer->layout_offset >= entry->length)
		{
			if (peer->layout_busy_game)
			{
				csmemcpy(peer->layout_game, entry->level, sizeof(peer->layout_game));
				peer->layout_game_serial = entry->serial;
			}
			else
			{
				csmemcpy(peer->layout_extra, entry->level, sizeof(peer->layout_extra));
				peer->layout_extra_serial = entry->serial;
			}
			peer->layout_busy = FALSE;
			error(_error_silent, "map sharing: machine #%ld was sent %s for %s", machine_index,
				entry->has ? "this host's layout" : "word of no layout", entry->level);
			return;
		}
	}
}

static void host_update(
	void)
{
	unsigned long now = system_milliseconds();
	short local_machine = network_game_client_get_local_machine_index();
	boolean wanted = FALSE;
	long index;

	/* (not before this host's own machine is known: it is never sent its own list; map sharing itself sends
	nothing until then, map_share_sending) */
	if (local_machine == NONE)
		return;
	for (index = 0; index < MACHINES; index++)
	{
		struct host_peer *peer = &share.peers[index];
		unsigned long address = network_game_server_machine_address(index);
		long frames;

		if (!address || address != peer->address || index == local_machine)
		{
			csmemset(peer, 0, sizeof(*peer));
			peer->address = index == local_machine ? 0 : address;
			if (!peer->address)
				continue;
		}
		if (!ae_peer_is_ae(index))
			continue;
		if (!peer->hello_sent)
		{
			struct ae_map_share_frame hello;

			csmemset(&hello, 0, sizeof(hello));
			peer->shared = host_policy(address);
			hello.type = AE_MAP_SHARE_FRAME_HELLO;
			hello.flags = peer->shared ? AE_MAP_SHARE_FLAG_SHARED : 0;
			/* (not sent: tried again next frame) */
			if (!host_send_frame(index, &hello))
				continue;
			peer->hello_sent = TRUE;
			error(_error_silent, "map sharing: machine #%ld runs this game: %s", index, peer->shared ?
				"it gets this host's list of maps" : "not shared with (network.map_share, network.map_share_internet)");
		}
		/* (the game's map's layout to every machine that runs this game, whatever the sharing settings: it plays
		the same level as this host; another map's to a machine this host shares with) */
		host_layout_update(index, peer);
		if (!peer->shared || peer->listed)
			continue;
		wanted = TRUE;
		if (!share.ready)
			continue;
		if (peer->list_serial != share.serial)
		{
			peer->list_serial = share.serial;
			peer->next_entry = 0;
		}
		for (frames = 0; frames < FRAMES_PER_UPDATE && !peer->listed; frames++)
		{
			struct ae_map_share_frame frame;
			long first = peer->next_entry;

			csmemset(&frame, 0, sizeof(frame));
			frame.type = AE_MAP_SHARE_FRAME_CATALOGUE;
			frame.flags = AE_MAP_SHARE_FLAG_SHARED;
			frame.expected = (int)share.entry_count;
			frame.first = (int)peer->next_entry;
			while (frame.count < AE_MAP_SHARE_FRAME_ENTRIES && peer->next_entry < share.entry_count)
			{
				frame.entries[frame.count] = share.entries[peer->next_entry++].entry;
				/* (debug.map_share_test "lie-size", "lie-hash": a host whose list is not what it then offers) */
				if (test_mode("lie-size"))
					frame.entries[frame.count].size++;
				if (test_mode("lie-hash"))
					frame.entries[frame.count].hash[AE_MAP_SHARE_HASH_BYTES - 1] ^= 1;
				frame.count++;
			}
			/* (not sent, its connection's queue full: the same entries again next frame, so the list stays whole) */
			if (!host_send_frame(index, &frame))
			{
				peer->next_entry = first;
				break;
			}
			if (peer->next_entry >= share.entry_count)
			{
				peer->listed = TRUE;
				error(_error_silent, "map sharing: machine #%ld was sent the list (%ld maps)", index,
					share.entry_count);
			}
		}
	}
	/* (the folders read when a guest first wants the list, and again for a later guest now and then; never while
	one is being sent it) */
	if (wanted && (!share.scanned || (share.ready && elapsed(share.scanned_at, now, RESCAN_INTERVAL))))
	{
		boolean sending = FALSE;

		for (index = 0; index < MACHINES; index++)
			sending |= share.peers[index].shared && !share.peers[index].listed && share.peers[index].next_entry > 0;
		if (!sending)
			host_scan();
	}
	if (share.scanned && !share.ready)
		host_hash_update();
}

/* ---------- the host's hooks */

static struct ae_map_share_rate *host_rate_of(
	unsigned long address)
{
	long index;

	for (index = 0; index < RATES; index++)
	{
		if (share.rates[index].address == address)
			return &share.rates[index].rate;
	}
	index = share.rates_next++ % RATES;
	csmemset(&share.rates[index], 0, sizeof(share.rates[index]));
	share.rates[index].address = address;
	return &share.rates[index].rate;
}

/* whether a flight is still one: its machine is the one that asked, and it asked lately or is still downloading */
static boolean flight_live(
	struct host_flight const *flight,
	unsigned long now)
{
	return flight->path[0] && flight->address &&
		network_game_server_machine_address(flight->machine_index) == flight->address &&
		(!elapsed(flight->asked_at, now, FLIGHT_TIMEOUT) || map_share_machine_downloading(flight->machine_index));
}

/* a machine asks for a listed file that is not the game's: its one flight (the one before it is let go); FALSE if
as many other machines' are in flight as there is room for */
static boolean flight_take(
	long machine_index,
	struct host_entry const *entry)
{
	unsigned long now = system_milliseconds();
	unsigned long address = network_game_server_machine_address(machine_index);
	struct host_flight *mine = NULL;
	struct host_flight *spare = NULL;
	long index;

	for (index = 0; index < FLIGHTS; index++)
	{
		struct host_flight *flight = &share.flights[index];

		if (flight->path[0] && flight->machine_index == machine_index && flight->address == address)
			mine = flight;
		else if (!flight_live(flight, now))
		{
			csmemset(flight, 0, sizeof(*flight));
			spare = flight;
		}
	}
	if (!mine)
		mine = spare;
	if (!mine || !address)
		return FALSE;
	csmemset(mine, 0, sizeof(*mine));
	csstrncpy(mine->path, entry->path, sizeof(mine->path) - 1);
	mine->address = address;
	mine->machine_index = machine_index;
	mine->asked_at = now;
	mine->custom_edition = entry->entry.kind == AE_MAP_SHARE_KIND_CE;
	return TRUE;
}

boolean ae_map_share_file_kept(
	char const *path)
{
	struct network_game *game = client_game();
	unsigned long now = system_milliseconds();
	boolean custom_edition = game && map_family_parse(game->map.name, NULL, 0) == _map_family_custom_edition;
	char other[PATH_BYTES];
	long index;
	short type;

	boolean kept = FALSE;

	if (!path || !path[0] || !is_host() || share.choosing_place)
		return FALSE;
	share.choosing_place = TRUE;
	for (index = 0; index < FLIGHTS; index++)
	{
		if (!flight_live(&share.flights[index], now))
			continue;
		kept |= !_stricmp(share.flights[index].path, path);
		custom_edition |= share.flights[index].custom_edition;
	}
	/* (the game's own map, and the resource maps while a Custom Edition map is played or sent) */
	if (!kept && game && game->map.name[0] && custom_edition_level_name(game->map.name) &&
		custom_edition_map_file_path(game->map.name, other) && !_stricmp(other, path))
	{
		kept = TRUE;
	}
	for (type = _resource_map_bitmaps; !kept && custom_edition && type < NUMBER_OF_RESOURCE_MAP_TYPES; type++)
		kept = custom_edition_resource_map_file_path(type, other) && !_stricmp(other, path);
	share.choosing_place = FALSE;
	return kept;
}

boolean ae_map_share_host_allowed(
	long machine_index,
	char const *level_name)
{
	struct host_peer *peer;

	if (machine_index < 0 || machine_index >= MACHINES)
		return FALSE;
	peer = &share.peers[machine_index];
	csstrncpy(peer->requested, level_name ? level_name : "", sizeof(peer->requested) - 1);
	peer->requested[sizeof(peer->requested) - 1] = 0;
	if (!ae_map_share_rate_allow(host_rate_of(network_game_server_machine_address(machine_index)),
		system_milliseconds(), REQUEST_BURST, REQUEST_REFILL))
	{
		error(_error_silent, "map sharing: machine #%ld asks too often: not answered with a map", machine_index);
		return FALSE;
	}
	if (!host_policy(network_game_server_machine_address(machine_index)))
	{
		error(_error_silent, "map sharing: machine #%ld is not on the local network, and internet sharing is off "
			"(network.map_share_internet): not shared with", machine_index);
		return FALSE;
	}
	return TRUE;
}

boolean ae_map_share_host_lists(
	long machine_index,
	char const *level_name)
{
	char file[LEVEL_BYTES];
	short level_family;
	long index;

	if (machine_index < 0 || machine_index >= MACHINES || !is_host() || !share.ready ||
		!share.peers[machine_index].listed || !ae_peer_is_ae(machine_index) || !level_name ||
		strlen(level_name) >= LEVEL_BYTES)
	{
		return FALSE;
	}
	level_family = map_family_parse(level_name, file, sizeof(file));
	if (level_family == _map_family_custom_edition)
		index = host_find(file, TRUE);
	else
		index = xbox_level_file(level_name, file, sizeof(file)) ? host_find(file, FALSE) : NONE;
	if (index == NONE)
		return FALSE;
	/* (one listed file in flight a machine, and only so many in all: the catalogue keeps their places) */
	if (!flight_take(machine_index, &share.entries[index]))
	{
		error(_error_silent, "map sharing: machine #%ld is not sent %s yet: too many of this host's maps are being "
			"sent at once", machine_index, level_name);
		return FALSE;
	}
	host_seed(&share.entries[index]);
	return TRUE;
}

boolean ae_map_share_host_serving(
	void)
{
	long index;

	if (!is_host())
		return FALSE;
	for (index = 0; index < MACHINES; index++)
	{
		if (share.peers[index].listed)
			return TRUE;
	}
	return FALSE;
}

short ae_map_share_host_file(
	short found,
	byte const *hash,
	unsigned long address)
{
	short catalog_file;
	struct map_share_file *file;
	boolean listed = FALSE;
	long index;

	/* (map sharing's own answer, the game's file, stands; else a file of the list, for a machine this host
	shares with and sent its list to, by its connection's address) */
	if (found != NONE || !ae_map_share_host_serving() || !share.ready || !address || !host_policy(address))
		return found;
	for (index = 0; index < MACHINES; index++)
		listed |= share.peers[index].listed && share.peers[index].address == address;
	if (!listed)
		return NONE;
	catalog_file = map_share_file_find(hash);
	file = map_share_file_get(catalog_file);
	if (!file)
		return NONE;
	/* (a file of this host's list alone, whatever else was hashed) */
	for (index = 0; index < share.entry_count; index++)
	{
		if (!_stricmp(share.entries[index].path, file->path))
			return catalog_file;
	}
	return NONE;
}

boolean ae_map_share_host_extra(
	long machine_index)
{
	struct network_game *game = client_game();

	return machine_index >= 0 && machine_index < MACHINES && game && share.peers[machine_index].requested[0] &&
		_stricmp(share.peers[machine_index].requested, game->map.name) != 0;
}

/* ---------- the adapters' Xbox maps (ae_map_share.c) */

boolean ae_map_share_xbox_level(
	char const *level_name)
{
	char file[LEVEL_BYTES];
	int index;

	if (!xbox_level_file(level_name, file, sizeof(file)))
		return FALSE;
	if (is_host())
		return share.ready && host_find(file, FALSE) != NONE;
	if (same_level(share.pending_level, level_name) || same_level(share.active_level, level_name))
		return TRUE;
	index = share.host_is_ae ? ae_map_share_catalogue_find(&share.catalogue, file, FALSE) : -1;
	return index >= 0 && share.catalogue.entries[index].kind != AE_MAP_SHARE_KIND_CE;
}

boolean ae_map_share_xbox_file_path(
	char const *level_name,
	char *path)
{
	char file[LEVEL_BYTES];
	long index;

	if (!is_host() || !share.ready || !xbox_level_file(level_name, file, sizeof(file)))
		return FALSE;
	index = host_find(file, FALSE);
	if (index == NONE)
		return FALSE;
	csstrncpy(path, share.entries[index].path, PATH_BYTES - 1);
	path[PATH_BYTES - 1] = 0;
	/* (its hashes are known: map sharing does not read the file again to answer for it) */
	host_seed(&share.entries[index]);
	return TRUE;
}

boolean ae_map_share_xbox_level_file(
	char const *level_name,
	char *name,
	long size)
{
	char file[LEVEL_BYTES];
	char digits[MAP_SHARE_NAME_DIGITS + 1] = "00000000";

	if (!xbox_level_file(level_name, file, sizeof(file)))
		return FALSE;
	if (is_host())
	{
		long index = share.ready ? host_find(file, FALSE) : NONE;

		if (index == NONE)
			return FALSE;
		map_share_hash_digits(share.entries[index].entry.hash, digits);
	}
	else
	{
		int index = ae_map_share_catalogue_find(&share.catalogue, file, FALSE);

		if (index >= 0 && share.catalogue.entries[index].kind != AE_MAP_SHARE_KIND_CE)
			map_share_hash_digits(share.catalogue.entries[index].hash, digits);
		else if (!same_level(share.pending_level, level_name))
			return FALSE;
	}
	snprintf(name, (size_t)size, "%s_%s", file, digits);
	return TRUE;
}

char const *ae_map_share_download_folder(
	void)
{
	return share.folder_xbox ? XBOX_DOWNLOAD_FOLDER : CE_DOWNLOAD_FOLDER;
}

char const *ae_map_share_xbox_folder(
	void)
{
	return XBOX_DOWNLOAD_FOLDER;
}

/* a downloaded Xbox map, whole and checked piece by piece, in maps_shared under its name and hash: taken only if
it is a map the game lists (version 5, a known build, its header's name the one asked for, no stock name), then
given its own name; never over a file that is there */
boolean ae_map_share_xbox_present(
	char const *level_name,
	char *message,
	long message_size)
{
	unsigned char header[XBOX_HEADER_BYTES];
	char file[LEVEL_BYTES];
	char hashed[LEVEL_BYTES + 16];
	char hashed_path[PATH_BYTES];
	char final_name[LEVEL_BYTES + 8];
	char final_path[PATH_BYTES];
	unsigned long read = 0;
	unsigned long size;
	unsigned long modified;
	HANDLE handle;

	if (!xbox_level_file(level_name, file, sizeof(file)) ||
		!ae_map_share_xbox_level_file(level_name, hashed, sizeof(hashed)))
	{
		snprintf(message, (size_t)message_size, "The map is not one this game shares.");
		return FALSE;
	}
	snprintf(hashed_path, sizeof(hashed_path), "%s%s.map", XBOX_DOWNLOAD_FOLDER, hashed);
	snprintf(final_name, sizeof(final_name), "%s.map", file);
	snprintf(final_path, sizeof(final_path), "%s%s", XBOX_DOWNLOAD_FOLDER, final_name);
	handle = CreateFileA(hashed_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (handle != INVALID_HANDLE_VALUE)
	{
		if (!ReadFile(handle, header, sizeof(header), &read, NULL))
			read = 0;
		CloseHandle(handle);
	}
	/* (and no longer than its header says it is once unpacked) */
	if (read != sizeof(header) || !native_map_header_valid(header, final_name) ||
		ae_map_share_stock_name((char const *)header + 0x20) || !file_stamp(hashed_path, &size, &modified) ||
		!ae_map_share_size_allowed(AE_MAP_SHARE_KIND_XBOX, size) || !xbox_length_fits(header, size))
	{
		snprintf(message, (size_t)message_size, "The downloaded map %.40s is not a map this game plays.", file);
		return FALSE;
	}
	if (file_exists(final_path) || !MoveFileA(hashed_path, final_path))
	{
		snprintf(message, (size_t)message_size, "A map named %.40s is in maps_shared already: it was not replaced.",
			file);
		return FALSE;
	}
	error(_error_silent, "map sharing: %s is in maps_shared now (%lu bytes)", final_name, size);
	return TRUE;
}

/* ---------- the game's side of the downloads' folder (cache_files.c, map_families.c, custom_maps.c) */

void ae_map_share_xbox_path(
	char const *map_name,
	char *path,
	long size)
{
	char shared[PATH_BYTES];

	if (!map_name || !path || !path[0] || file_exists(path) || !ae_map_share_name_allowed(map_name) ||
		strlen(map_name) > AE_MAP_SHARE_XBOX_NAME_LENGTH || ae_map_share_stock_name(map_name))
	{
		return;
	}
	if (snprintf(shared, sizeof(shared), "%s%s.map", XBOX_DOWNLOAD_FOLDER, map_name) >= (int)sizeof(shared) ||
		(long)strlen(shared) >= size || !file_exists(shared))
	{
		return;
	}
	snprintf(path, (size_t)size, "%s", shared);
}

boolean ae_map_share_xbox_downloaded(
	char const *map_name)
{
	char file[LEVEL_BYTES];
	char path[PATH_BYTES];

	if (!map_name || strlen(map_name) >= LEVEL_BYTES || map_family_parse(map_name, file, sizeof(file)) != _map_family_xbox)
		return FALSE;
	cache_files_map_file_path(file, path, sizeof(path));
	return !_strnicmp(path, XBOX_DOWNLOAD_FOLDER, sizeof(XBOX_DOWNLOAD_FOLDER) - 1);
}

void ae_map_share_scan_downloads(
	void (*scan)(char const *directory))
{
	if (scan)
		scan(XBOX_DOWNLOAD_FOLDER);
}

/* ---------- the guest: its host */

static void guest_remember(
	void)
{
	long index;

	/* (a LAN host's answer is kept for this run; an internet host's address is not its own) */
	if (!share.host_address || !ae_map_share_address_local(share.host_address))
		return;
	for (index = 0; index < REMEMBERED_HOSTS && share.remembered[index].address != share.host_address; index++)
		;
	if (index == REMEMBERED_HOSTS)
		index = share.remembered_next++ % REMEMBERED_HOSTS;
	share.remembered[index].address = share.host_address;
	share.remembered[index].taken = share.taken;
}

static void guest_host_changed(
	unsigned long address)
{
	long index;

	guest_remember();
	share.host_address = address;
	share.joined_at = system_milliseconds();
	share.host_is_ae = FALSE;
	share.host_shares = FALSE;
	share.catalogue_begun = FALSE;
	share.catalogue_restarts = 0;
	ae_map_share_catalogue_begin(&share.catalogue, 0);
	share.consent = ae_map_share_consent_step(share.consent, AE_MAP_SHARE_CONSENT_EVENT_HOST);
	share.taken = 0;
	csmemset(&share.charges, 0, sizeof(share.charges));
	share.granted = 0;
	share.grant_base = 0;
	share.offer_bytes = 0;
	share.played_bytes = 0;
	share.question_first_shown = 0;
	share.question_last_shown = 0;
	share.queue_pause_until = 0;
	share.plan_count = 0;
	share.plan_bytes = 0;
	share.plan_over = 0;
	share.all_active = FALSE;
	share.all_wanted = FALSE;
	share.queue_count = 0;
	share.active_level[0] = 0;
	share.active_cancel = FALSE;
	share.pending_level[0] = 0;
	share.released_level[0] = 0;
	share.played_asking = FALSE;
	share.test_all_done = FALSE;
	/* (another host's layouts are not this one's) */
	ae_layout_share_clear(&layout_inbox);
	for (index = 0; address && index < REMEMBERED_HOSTS; index++)
	{
		if (share.remembered[index].address == address && ae_map_share_address_local(address))
		{
			share.taken = share.remembered[index].taken;
		}
	}
}

static int guest_lacks(
	struct ae_map_share_entry const *entry,
	void *context)
{
	char path[PATH_BYTES];

	(void)context;
	switch (entry->kind)
	{
	case AE_MAP_SHARE_KIND_XBOX:
	case AE_MAP_SHARE_KIND_MOD:
		cache_files_map_file_path(entry->name, path, sizeof(path));
		return !path[0] || !file_exists(path);
	case AE_MAP_SHARE_KIND_CE:
		return halo_custom_edition_tag_cache() &&
			!map_family_find(_map_family_custom_edition, entry->name, path, sizeof(path));
	case AE_MAP_SHARE_KIND_RESOURCE:
	{
		short type;

		for (type = _resource_map_bitmaps; type < NUMBER_OF_RESOURCE_MAP_TYPES; type++)
		{
			if (!_stricmp(entry->name, resource_map_type_describe((enum resource_map_type)type)))
				return !custom_edition_resource_map_file_path(type, path);
		}
		return 0;
	}
	}
	return 0;
}

static void guest_plan(
	void)
{
	int over = 0;

	share.plan_count = ae_map_share_catalogue_complete(&share.catalogue) && share.catalogue_begun ?
		ae_map_share_plan(&share.catalogue, guest_lacks, NULL, guest_cap(), share.taken, share.plan,
		NUMBEROF(share.plan), &share.plan_bytes, &over) : 0;
	if (!share.plan_count)
		share.plan_bytes = 0;
	share.plan_over = over;
	share.planned_at = system_milliseconds();
}

static unsigned long long disk_free(
	void)
{
	ULARGE_INTEGER available;

	available.QuadPart = 0;
	/* (not known: nothing fits) */
	if (!GetDiskFreeSpaceExA("d:\\", &available, NULL, NULL))
		return 0;
	return available.QuadPart;
}

/* a file's key in what was counted toward this host's total (ae_map_share_charges): its kind and its level */
static void charge_key(
	char *key,
	char const *level_name,
	short kind)
{
	snprintf(key, AE_MAP_SHARE_CHARGE_KEY_BYTES, "%d:%s", (int)kind, level_name);
}

/* the question's amount: what the host has that this machine lacks, the game's map at least (0: not known) */
static unsigned long long question_bytes(
	void)
{
	return MAX(share.plan_bytes, share.played_bytes);
}

/* whether the yes given covers more bytes on top of what was taken since */
static boolean grant_covers(
	unsigned long long more)
{
	return ae_map_share_consent_writes(share.consent) &&
		ae_map_share_grant_covers(share.granted, share.taken - share.grant_base, more) != 0;
}

static void refuse_printf(
	char const *format,
	...)
{
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(share.refusal, sizeof(share.refusal), format, arguments);
	va_end(arguments);
}

/* ---------- the guest's hooks (map_share.c) */

boolean ae_map_share_download_permitted(
	char const *level_name)
{
	if (is_host() || guest_policy())
		return TRUE;
	error(_error_silent, "map sharing: %s is not asked for: downloads are off for this host (network.map_download, "
		"and for a host that is not on the local network network.map_download_internet)", level_name ? level_name : "");
	return FALSE;
}

char const *ae_map_share_refusal(
	void)
{
	return share.refusal[0] ? share.refusal : "The map is not downloaded.";
}

int ae_map_share_download_gate(
	char const *level_name,
	boolean asking)
{
	unsigned long now = system_milliseconds();
	struct ae_map_share_entry const *entry = NULL;
	char file[LEVEL_BYTES];
	short level_family;
	boolean xbox_map;

	if (!level_name || strlen(level_name) >= LEVEL_BYTES)
		return 0;
	/* (a map of GET ALL: stopped at once when asked to, or when the game leaves the lobby) */
	if (same_level(share.active_level, level_name))
	{
		if (share.active_cancel || !main_menu_is_active() || !ae_map_share_consent_writes(share.consent))
		{
			refuse_printf("Getting the host's maps was stopped.");
			share.active_stopped = TRUE;
			return 3;
		}
	}
	if (!asking || same_level(share.released_level, level_name))
		return 0;
	level_family = map_family_parse(level_name, file, sizeof(file));
	xbox_map = level_family == _map_family_xbox;
	if (xbox_map)
	{
		int index;

		if (!share.host_is_ae)
		{
			if (!elapsed(share.pending_at, now, HELLO_WAIT))
				return 1;
			refuse_printf("The host does not share this map.");
			return 3;
		}
		if (!share.host_shares)
		{
			refuse_printf("The host does not share its maps.");
			return 3;
		}
		if (!share.catalogue_begun || !ae_map_share_catalogue_complete(&share.catalogue))
		{
			if (!elapsed(share.pending_at, now, CATALOGUE_WAIT))
				return 1;
			refuse_printf("The host did not send its list of maps.");
			return 3;
		}
		index = ae_map_share_catalogue_find(&share.catalogue, file, FALSE);
		if (index < 0 || share.catalogue.entries[index].kind == AE_MAP_SHARE_KIND_CE)
		{
			refuse_printf("The host's list of maps does not have this map.");
			return 3;
		}
		entry = &share.catalogue.entries[index];
	}
	else if (share.host_is_ae && share.catalogue_begun && ae_map_share_catalogue_complete(&share.catalogue))
	{
		int index = ae_map_share_catalogue_find(&share.catalogue, file, FALSE);

		if (index >= 0 && share.catalogue.entries[index].kind == AE_MAP_SHARE_KIND_CE)
			entry = &share.catalogue.entries[index];
	}
	if (entry)
	{
		char key[AE_MAP_SHARE_CHARGE_KEY_BYTES];
		unsigned long long growth;

		/* (what it adds to this host's total: all of it, or its growth over what was counted for it before) */
		charge_key(key, level_name, _map_share_file_map);
		growth = ae_map_share_charge_growth(&share.charges, key, entry->size);
		if (!same_level(share.active_level, level_name))
		{
			share.played_bytes = growth;
			/* (more than the yes was given for: asked again, with the new amount) */
			if (ae_map_share_consent_writes(share.consent) && !grant_covers(growth))
				share.consent = AE_MAP_SHARE_CONSENT_UNASKED;
		}
		if (!ae_map_share_cap_allows(share.taken, growth, guest_cap()))
		{
			refuse_printf("The map is %ld MB: over the %ld MB you take from one host (network.map_download_limit).",
				megabytes(entry->size), guest_cap());
			return 3;
		}
		if (!ae_map_share_disk_allows(disk_free(), entry->size))
		{
			refuse_printf("There is not enough free disk space to download the map (%ld MB).", megabytes(entry->size));
			return 3;
		}
	}
	share.consent = ae_map_share_consent_step(share.consent, AE_MAP_SHARE_CONSENT_EVENT_NEED);
	if (share.consent == AE_MAP_SHARE_CONSENT_ASKING)
	{
		if (!share.played_asking)
			error(_error_silent, "map sharing: the player is asked whether to download from this host");
		share.played_asking = TRUE;
		if (!share.planned_at || elapsed(share.planned_at, now, 2000))
			guest_plan();
		return 1;
	}
	if (!ae_map_share_consent_writes(share.consent))
	{
		refuse_printf("You chose not to download maps from this host.");
		return 3;
	}
	csstrncpy(share.released_level, level_name, sizeof(share.released_level) - 1);
	share.released_level[sizeof(share.released_level) - 1] = 0;
	return 2;
}

boolean ae_map_share_download_begun(
	char const *level_name)
{
	share.released_level[0] = 0;
	share.refusal[0] = 0;
	share.pending_at = system_milliseconds();
	share.folder_xbox = FALSE;
	share.offer_bytes = 0;
	share.played_bytes = 0;
	return ae_map_share_download_gate(level_name, TRUE) == 2;
}

boolean ae_map_share_download_failed(
	char const *level_name,
	char const *reason)
{
	if (!level_name || !same_level(share.active_level, level_name))
		return FALSE;
	/* (a map of GET ALL: the game is not left for it; one that failed, not merely stopped, is not tried again) */
	share.active_failed = !share.active_stopped;
	if (reason && !strncmp(reason, "The host does not share", 23))
	{
		share.active_failed = FALSE;
		share.queue_pause_until = system_milliseconds() + QUEUE_PAUSE;
	}
	error(_error_silent, "map sharing: %s of the host's maps is not fetched: %s", level_name, reason ? reason : "");
	return TRUE;
}

boolean ae_map_share_download_extra(
	char const *level_name)
{
	return level_name && same_level(share.active_level, level_name);
}

boolean ae_map_share_download_host_only(
	char const *level_name)
{
	return level_name && (ae_map_share_download_extra(level_name) || ae_map_share_xbox_level(level_name));
}

boolean ae_map_share_offer_allowed(
	char const *level_name,
	short kind,
	unsigned long size,
	byte const *hash)
{
	struct ae_map_share_entry const *entry = NULL;
	char file[LEVEL_BYTES];
	short level_family;
	int index;

	if (!level_name || strlen(level_name) >= LEVEL_BYTES)
		return FALSE;
	/* (nothing is written without the player's yes) */
	if (!ae_map_share_consent_writes(share.consent))
	{
		refuse_printf("You did not say yes to downloading from this host.");
		return FALSE;
	}
	level_family = map_family_parse(level_name, file, sizeof(file));
	index = share.host_is_ae && share.catalogue_begun && ae_map_share_catalogue_complete(&share.catalogue) ?
		ae_map_share_catalogue_find(&share.catalogue, file, FALSE) : -1;
	if (index >= 0 && (share.catalogue.entries[index].kind == AE_MAP_SHARE_KIND_CE) ==
		(level_family == _map_family_custom_edition))
	{
		entry = &share.catalogue.entries[index];
	}
	if (level_family == _map_family_xbox)
	{
		/* (an Xbox map: the map alone, and only what the list said) */
		if (kind != _map_share_file_map || !entry || !xbox_level_file(level_name, file, sizeof(file)))
		{
			refuse_printf("The host offered a file its list of maps does not have.");
			return FALSE;
		}
	}
	else if (level_family != _map_family_custom_edition)
	{
		refuse_printf("The host offered a map of a kind that is not downloaded.");
		return FALSE;
	}
	if (kind == _map_share_file_map)
	{
		if (entry && !ae_map_share_offer_matches(entry, size, hash))
		{
			refuse_printf("The host's offer is not the map of its list (another size or hash).");
			return FALSE;
		}
		if (!ae_map_share_size_allowed(level_family == _map_family_xbox ? AE_MAP_SHARE_KIND_XBOX : AE_MAP_SHARE_KIND_CE, size))
		{
			refuse_printf("The map is larger than the game takes (%ld MB).", megabytes(size));
			return FALSE;
		}
	}
	else if (!ae_map_share_size_allowed(AE_MAP_SHARE_KIND_RESOURCE, size))
	{
		refuse_printf("The map's resource map is larger than the game takes (%ld MB).", megabytes(size));
		return FALSE;
	}
	{
		char key[AE_MAP_SHARE_CHARGE_KEY_BYTES];
		unsigned long long growth;

		/* (counted toward this host's total: all of it, or its growth over what was counted for this file before,
		whatever size it had then) */
		charge_key(key, level_name, kind);
		growth = ae_map_share_charge_growth(&share.charges, key, size);
		if (!ae_map_share_cap_allows(share.taken, growth, guest_cap()))
		{
			refuse_printf("This would be over the %ld MB you take from one host (network.map_download_limit).", guest_cap());
			return FALSE;
		}
		/* (no more than the player's yes was given for: asked again next time) */
		if (!grant_covers(growth))
		{
			share.consent = AE_MAP_SHARE_CONSENT_UNASKED;
			refuse_printf("The host offers more than you said yes to.");
			return FALSE;
		}
		/* (every offer, and its files together: the disk keeps its margin after all of them) */
		if (!ae_map_share_disk_allows(disk_free(), share.offer_bytes + size))
		{
			refuse_printf("There is not enough free disk space to download the map (%ld MB).",
				megabytes(share.offer_bytes + size));
			return FALSE;
		}
		share.offer_bytes += size;
		ae_map_share_charge_set(&share.charges, key, size);
		share.taken += growth;
	}
	share.folder_xbox = level_family == _map_family_xbox;
	return TRUE;
}

boolean ae_map_share_played_map(
	char const *map_name)
{
	char file[LEVEL_BYTES];
	char message[160];

	if (is_host() || !global_network_game_client_get() || !xbox_level_file(map_name, file, sizeof(file)))
		return FALSE;
	csstrncpy(share.pending_level, map_name, sizeof(share.pending_level) - 1);
	share.pending_level[sizeof(share.pending_level) - 1] = 0;
	snprintf(message, sizeof(message), "You don't have the map %.40s.", file);
	if (map_share_download_begin(map_name, 0, message))
		return TRUE;
	share.pending_level[0] = 0;
	return FALSE;
}

/* ---------- the guest: frames */

boolean ae_map_share_handle_message(
	long machine_index,
	word const *message,
	word size,
	boolean from_stream)
{
	struct distributed_message_header header;
	byte const *body = (byte const *)message + sizeof(header);
	long length = (long)size - (long)sizeof(header);
	struct ae_map_share_frame frame;

	if (size < sizeof(header) + OFFER_BYTES)
		return FALSE;
	csmemcpy(&header, message, sizeof(header));
	if (header.type != _distributed_message_map_share_offer || csmemcmp(body, frame_level_name, sizeof(frame_level_name)))
		return FALSE;
	/* (a frame: taken from a host alone, over its connection, by a guest; any other dropped here) */
	if (machine_index != NONE || !from_stream || is_host() || !global_network_game_client_get() ||
		body[OFFER_RESULT] != FRAME_RESULT || body[OFFER_FILE_COUNT] != FRAME_FILE_COUNT)
	{
		return TRUE;
	}
	/* (a layout's frame, from a host that said it runs this game: ae_layout_share_rules.h. Versions before 0.3.5 drop
	it below, as a frame of a type they do not know) */
	if (length > OFFER_BYTES && body[OFFER_BYTES] == AE_LAYOUT_SHARE_FRAME_TYPE)
	{
		static struct ae_layout_share_frame layout_frame;
		int slot;

		if (share.host_is_ae && ae_layout_share_frame_decode(body + OFFER_BYTES, length - OFFER_BYTES, &layout_frame) &&
			ae_layout_share_take(&layout_inbox, &layout_frame, &slot) == 1)
		{
			struct ae_layout_share_slot const *taken = &layout_inbox.slots[slot];

			if (taken->state == AE_LAYOUT_SHARE_NONE)
				error(_error_silent, "map sharing: the host has no layout for %s", taken->level);
			else
				error(_error_silent, "map sharing: the host's layout for %s came (%ld bytes)", taken->level, taken->total);
		}
		return TRUE;
	}
	if (!ae_map_share_frame_decode(body + OFFER_BYTES, length - OFFER_BYTES, &frame))
		return TRUE;
	if (frame.type == AE_MAP_SHARE_FRAME_HELLO)
	{
		share.host_is_ae = TRUE;
		share.host_shares = (frame.flags & AE_MAP_SHARE_FLAG_SHARED) != 0;
		error(_error_silent, "map sharing: the host runs this game%s", share.host_shares ? "" :
			" and does not share its maps with this machine");
		return TRUE;
	}
	/* (no list is kept of a host this machine does not download from) */
	if (!share.host_is_ae || !share.host_shares || !guest_policy())
		return TRUE;
	/* (a name a file system takes for a device is no map's: left out, as any entry that is not valid) */
	{
		int entry_index;

		for (entry_index = 0; entry_index < frame.count; entry_index++)
		{
			if (!map_share_name_safe(frame.entries[entry_index].name))
				frame.entries[entry_index].kind = AE_MAP_SHARE_KIND_NONE;
		}
	}
	if (frame.first == 0)
	{
		/* (a list while one is being fetched from is not taken: its entries are what the downloads are checked
		against) */
		if (share.catalogue_restarts >= CATALOGUE_RESTARTS || share.all_active)
			return TRUE;
		share.catalogue_restarts++;
		share.catalogue_begun = ae_map_share_catalogue_begin(&share.catalogue, frame.expected) != 0;
	}
	if (!share.catalogue_begun || ae_map_share_catalogue_complete(&share.catalogue) ||
		!ae_map_share_catalogue_take(&share.catalogue, &frame))
	{
		return TRUE;
	}
	if (ae_map_share_catalogue_complete(&share.catalogue))
	{
		guest_plan();
		error(_error_silent, "map sharing: the host lists %d maps; this machine lacks %ld of them (%ld MB)%s",
			share.catalogue.count, share.plan_count, megabytes(share.plan_bytes),
			guest_policy() ? "" : "; downloads are off for this host");
	}
	return TRUE;
}

/* ---------- the guest: its host's layouts (ae_layout_share_rules.h) */

/* the inbox's slot of a map (as the game names it: its family and file, any case), or NULL */
static struct ae_layout_share_slot const *layout_slot_of(
	char const *map_name)
{
	char file[LEVEL_BYTES];
	char other[LEVEL_BYTES];
	short family;
	long index;

	if (!map_name || !map_name[0] || strlen(map_name) >= LEVEL_BYTES)
		return NULL;
	family = map_family_parse(map_name, file, sizeof(file));
	if (!file[0])
		return NULL;
	for (index = 0; index < AE_LAYOUT_SHARE_SLOTS; index++)
	{
		struct ae_layout_share_slot const *slot = &layout_inbox.slots[index];

		if (slot->state != AE_LAYOUT_SHARE_EMPTY && map_family_parse(slot->level, other, sizeof(other)) == family &&
			!_stricmp(other, file))
		{
			return slot;
		}
	}
	return NULL;
}

int ae_map_share_layout_received(
	char const *map_name,
	char const **text,
	long *length)
{
	struct ae_layout_share_slot const *slot;

	*text = NULL;
	*length = 0;
	if (is_host() || !global_network_game_client_get() || !share.host_is_ae)
		return AE_LAYOUT_SHARE_HOST_UNKNOWN;
	slot = layout_slot_of(map_name);
	if (!slot)
	{
		error(_error_silent, "map sharing: no word from the host on a layout for %s: this machine's own file is used, if "
			"it has one", map_name ? map_name : "");
		return AE_LAYOUT_SHARE_HOST_UNKNOWN;
	}
	if (slot->state == AE_LAYOUT_SHARE_NONE)
		return AE_LAYOUT_SHARE_HOST_NONE;
	if (slot->state != AE_LAYOUT_SHARE_TEXT)
	{
		error(_error_silent, "map sharing: the host's layout for %s had not all come (%ld of %ld bytes): this machine's "
			"own file is used, if it has one", slot->level, slot->received, slot->total);
		return AE_LAYOUT_SHARE_HOST_UNKNOWN;
	}
	*text = slot->text;
	*length = slot->total;
	return AE_LAYOUT_SHARE_HOST_TEXT;
}

void ae_map_share_layout_downloaded(
	char const *level_name)
{
	struct ae_layout_share_slot const *slot;

	if (is_host() || !share.host_is_ae)
		return;
	slot = layout_slot_of(level_name);
	if (slot && slot->state == AE_LAYOUT_SHARE_TEXT)
		ae_layout_keep_host_text(level_name, slot->text, slot->total);
}

/* ---------- the guest: GET ALL */

static void all_start(
	void)
{
	guest_plan();
	if (!share.plan_count)
	{
		notice_set("You have all of the host's maps.");
		return;
	}
	csmemcpy(share.queue, share.plan, sizeof(share.queue));
	share.queue_count = share.plan_count;
	share.queue_next = 0;
	share.queue_done = 0;
	share.queue_failed = 0;
	share.queue_attempts = 0;
	share.active_level[0] = 0;
	share.active_cancel = FALSE;
	share.all_active = TRUE;
	error(_error_silent, "map sharing: getting all of the host's maps this machine lacks: %ld maps, %ld MB",
		share.queue_count, megabytes(share.plan_bytes));
}

static void all_stop(
	char const *why)
{
	char text[160];

	share.all_active = FALSE;
	if (share.active_level[0])
		share.active_cancel = TRUE;
	snprintf(text, sizeof(text), "Getting the host's maps %s: %ld of %ld fetched%s.", why, share.queue_done,
		share.queue_count, share.queue_failed ? ", some could not be" : "");
	notice_set(text);
}

static void all_update(
	void)
{
	struct network_game *game = client_game();
	struct ae_map_share_entry const *entry;
	char level[LEVEL_BYTES];

	/* (the map being fetched: still coming, or over) */
	if (share.active_level[0])
	{
		if (map_share_downloading(share.active_level))
			return;
		if (share.queue_next < share.queue_count)
		{
			entry = &share.catalogue.entries[share.queue[share.queue_next]];
			if (!guest_lacks(entry, NULL))
			{
				share.queue_done++;
				share.queue_next++;
				share.queue_attempts = 0;
			}
			/* (stopped for the game's own map, or for the game: tried again; failed: given up) */
			else if (share.active_failed || ++share.queue_attempts >= QUEUE_ATTEMPTS)
			{
				share.queue_failed++;
				share.queue_next++;
				share.queue_attempts = 0;
			}
		}
		share.active_level[0] = 0;
		share.active_cancel = FALSE;
		share.active_stopped = FALSE;
		share.active_failed = FALSE;
		return;
	}
	if (!share.all_active)
		return;
	/* (only in the lobby, and never while the game's own map is coming) */
	if (!game || !main_menu_is_active() || (game->map.name[0] && map_share_downloading(game->map.name)))
		return;
	if (share.queue_pause_until && !elapsed(share.queue_pause_until, system_milliseconds(), 0))
		return;
	share.queue_pause_until = 0;
	if (share.queue_next >= share.queue_count)
	{
		all_stop("is done");
		guest_plan();
		return;
	}
	entry = &share.catalogue.entries[share.queue[share.queue_next]];
	if (!guest_lacks(entry, NULL))
	{
		share.queue_done++;
		share.queue_next++;
		return;
	}
	snprintf(level, sizeof(level), "%s%s", entry->name, entry->kind == AE_MAP_SHARE_KIND_CE ?
		map_family_suffix(_map_family_custom_edition) : "");
	csstrncpy(share.active_level, level, sizeof(share.active_level) - 1);
	share.active_level[sizeof(share.active_level) - 1] = 0;
	share.active_cancel = FALSE;
	share.active_stopped = FALSE;
	share.active_failed = FALSE;
	/* (network version 29: a Custom Edition map's version, its hash's first 32 bits, which its offer is checked
	against; an Xbox map's 0) */
	if (!map_share_download_begin(level, entry->kind == AE_MAP_SHARE_KIND_CE ? map_share_hash_version(entry->hash) : 0,
		""))
	{
		error(_error_silent, "map sharing: %s of the host's maps could not be asked for", level);
		share.active_level[0] = 0;
		share.queue_failed++;
		share.queue_next++;
	}
}

/* ---------- the guest: the question and its answers (the interface a dialog calls; the F6 key is one caller) */

boolean ae_map_share_consent_question(
	struct ae_map_share_question *question)
{
	if (is_host() || !share.host_address || share.consent != AE_MAP_SHARE_CONSENT_ASKING)
		return FALSE;
	if (question)
	{
		question->host = share.host_address;
		question->maps = share.plan_count;
		question->bytes = question_bytes();
		question->limit_megabytes = guest_cap();
		question->needed_to_play = share.played_asking;
	}
	return TRUE;
}

void ae_map_share_consent_shown(
	void)
{
	unsigned long now = system_milliseconds();

	if (share.consent != AE_MAP_SHARE_CONSENT_ASKING)
		return;
	/* (shown anew: its time starts, and the key must be let go of before it answers) */
	if (!share.question_first_shown || elapsed(share.question_last_shown, now, QUESTION_SHOWN_LATELY))
	{
		share.question_first_shown = now | 1;
		share.question_key_clear = FALSE;
	}
	share.question_last_shown = now;
}

/* whether an answer is taken now: the question is the one asked, on the screen (shown lately, and for long enough
to be read), in the lobby or the menus (never in a game) */
static boolean consent_answerable(
	unsigned long host)
{
	unsigned long now = system_milliseconds();

	return share.consent == AE_MAP_SHARE_CONSENT_ASKING && host && host == share.host_address &&
		main_menu_is_active() && share.question_first_shown &&
		!elapsed(share.question_last_shown, now, QUESTION_SHOWN_LATELY) &&
		elapsed(share.question_first_shown, now, QUESTION_MINIMUM_TIME);
}

boolean ae_map_share_consent_yes(
	unsigned long host,
	unsigned long long bytes)
{
	/* (for the host and the amount that are shown now, no other) */
	if (!consent_answerable(host) || bytes != question_bytes())
		return FALSE;
	share.consent = ae_map_share_consent_step(share.consent, AE_MAP_SHARE_CONSENT_EVENT_YES);
	/* (the yes is for that amount: more is asked for again; an amount not known is the limit's) */
	share.granted = bytes ? bytes : (unsigned long long)guest_cap() << 20;
	share.grant_base = share.taken;
	share.played_asking = FALSE;
	share.question_first_shown = 0;
	error(_error_silent, "map sharing: the player said yes to downloading from this host (%ld MB)",
		megabytes(share.granted));
	if (share.all_wanted)
	{
		share.all_wanted = FALSE;
		all_start();
	}
	return TRUE;
}

boolean ae_map_share_consent_no(
	unsigned long host)
{
	if (share.consent != AE_MAP_SHARE_CONSENT_ASKING || !host || host != share.host_address)
		return FALSE;
	/* (no to GET ALL alone is not a no to the host: it may be asked again; no to the game's map is) */
	share.consent = share.played_asking ? ae_map_share_consent_step(share.consent, AE_MAP_SHARE_CONSENT_EVENT_NO) :
		AE_MAP_SHARE_CONSENT_UNASKED;
	share.all_wanted = FALSE;
	share.played_asking = FALSE;
	share.question_first_shown = 0;
	return TRUE;
}

boolean ae_map_share_download_may_write(
	void)
{
	return !is_host() && ae_map_share_consent_writes(share.consent) != 0;
}

static void guest_update(
	void)
{
	unsigned long now = system_milliseconds();
	unsigned long address = guest_host_address();
	boolean lobby = main_menu_is_active();
	boolean key = ae_platform_map_share_key() != 0;
	boolean pressed;

	/* (a pad's BACK held in the lobby counts as the key: for a guest with no keyboard) */
	if (!lobby || !ae_platform_map_share_pad())
		share.pad_down_at = 0;
	else if (!share.pad_down_at)
		share.pad_down_at = now | 1;
	else if (elapsed(share.pad_down_at, now, PAD_HOLD_TIME))
		key = TRUE;
	pressed = key && !share.key_down;
	share.key_down = key;
	if (address != share.host_address)
		guest_host_changed(address);
	if (!address)
		return;
	if (share.consent == AE_MAP_SHARE_CONSENT_ASKING)
	{
		/* (the key answers only once it was seen up while the question was shown) */
		if (!key && share.question_first_shown)
			share.question_key_clear = TRUE;
		/* (debug.map_share_test, in a network test alone: the player's answers, through the same functions) */
		if (test_mode("yes") || test_mode("all") || test_mode("internet"))
			ae_map_share_consent_yes(address, question_bytes());
		else if (test_mode("no"))
		{
			share.played_asking = TRUE;
			ae_map_share_consent_no(address);
		}
		else if (pressed && share.question_key_clear)
			ae_map_share_consent_yes(address, question_bytes());
		/* (asked for GET ALL alone, and not answered: not asked any more, and may be asked again) */
		else if (share.all_wanted && !share.played_asking && elapsed(share.all_asked_at, now, PROMPT_TIME))
			ae_map_share_consent_no(address);
	}
	else
	{
		if ((test_mode("all") || test_mode("internet")) && !share.test_all_done && lobby && share.plan_count &&
			!share.all_active)
		{
			share.test_all_done = TRUE;
			pressed = TRUE;
		}
		if (pressed && share.all_active)
		{
			all_stop("was stopped");
		}
		else if (pressed && lobby && guest_policy() && share.host_shares)
		{
			guest_plan();
			if (!share.plan_count)
				notice_set(share.plan_over ? "The host's other maps are over what you take from one host." :
					"You have all of the host's maps.");
			/* (the yes given covers them: fetched; else asked, with their amount) */
			else if (grant_covers(share.plan_bytes))
				all_start();
			else if (share.consent != AE_MAP_SHARE_CONSENT_NO)
			{
				share.consent = ae_map_share_consent_step(AE_MAP_SHARE_CONSENT_UNASKED, AE_MAP_SHARE_CONSENT_EVENT_NEED);
				share.all_wanted = TRUE;
				share.all_asked_at = now;
				error(_error_silent, "map sharing: the player is asked whether to download from this host");
			}
		}
	}
	all_update();
}

/* ---------- every frame, and the screen's line */

void ae_map_share_all_update(
	void)
{
	if (is_host())
	{
		if (share.host_address)
			guest_host_changed(0);
		host_update();
		return;
	}
	/* (no host: its machines forgotten; the list's hashes are kept for the next game it hosts) */
	csmemset(share.peers, 0, sizeof(share.peers));
	guest_update();
}

void ae_map_share_all_render(
	void)
{
	unsigned long now = system_milliseconds();
	real_argb_color color = { 1.0f, 1.0f, 1.0f, 1.0f };
	rectangle2d bounds;
	char text[240];
	long font_tag_index;

	text[0] = 0;
	if (is_host() || !share.host_address)
		return;
	if (share.consent == AE_MAP_SHARE_CONSENT_ASKING)
	{
		if (share.plan_count)
		{
			snprintf(text, sizeof(text), "The host has %ld map%s you lack, %ld MB (you take %ld MB at most). "
				"Download from this host? F6 or hold BACK: yes%s", share.plan_count,
				share.plan_count == 1 ? "" : "s", megabytes(question_bytes()), guest_cap(),
				share.played_asking ? " - B or Esc: leave" : "");
		}
		else
		{
			snprintf(text, sizeof(text), "Download this game's map from the host? F6 or hold BACK: yes - B or Esc: leave");
		}
	}
	else if (share.all_active)
	{
		/* (the pad's BACK only in the lobby: in a game it is the scoreboard's) */
		snprintf(text, sizeof(text), "Getting the host's maps: %ld of %ld - F6%s stops",
			MIN(share.queue_next + 1, share.queue_count), share.queue_count,
			main_menu_is_active() ? " or hold BACK" : "");
	}
	else if (share.notice[0] && !elapsed(share.notice_until, now, 0))
	{
		snprintf(text, sizeof(text), "%s", share.notice);
	}
	else if (main_menu_is_active() && share.host_shares && share.plan_count && guest_policy())
	{
		snprintf(text, sizeof(text), "The host has %ld map%s you lack (%ld MB): F6 or hold BACK gets them all",
			share.plan_count, share.plan_count == 1 ? "" : "s", megabytes(share.plan_bytes));
	}
	if (!text[0])
		return;
	font_tag_index = interface_get_tag_index(_interface_font_terminal);
	if (font_tag_index == NONE)
		return;
	/* (under map sharing's own line, at the top of the screen) */
	bounds = render.camera.window_bounds;
	offset_rectangle2d(&bounds, -render.camera.viewport_bounds.x0, -render.camera.viewport_bounds.y0);
	bounds.y0 = 24;
	bounds.y1 = 42;
	draw_string_set_draw_mode(font_tag_index, NONE, _text_centered, 0, &color);
	rasterizer_draw_string(&bounds, NULL, NULL, 0, text);
	/* (the question was really drawn: only then, and only after a while, is an answer taken) */
	if (share.consent == AE_MAP_SHARE_CONSENT_ASKING)
		ae_map_share_consent_shown();
}

/* map_share.c's upload_limit (an AE hook): a host whose every other machine is on the local network sends at
network.map_share_lan_upload when that is more than network.map_share_upload (a LAN party's 100 MB map in seconds, not
minutes); one machine on the internet, and the host keeps the internet's, which leaves its upload to the game */
long ae_map_share_upload_kilobytes(long kilobytes)
{
	static unsigned long read_at = (unsigned long)-1;
	static long lan_kilobytes;
	short local_machine = network_game_client_get_local_machine_index();
	long index;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		lan_kilobytes = config_integer("network.map_share_lan_upload");
	}
	if (lan_kilobytes <= kilobytes || !global_network_game_server_get())
		return kilobytes;
	for (index = 0; index < MACHINES; index++)
	{
		unsigned long address = network_game_server_machine_address(index);

		if (index != local_machine && address && !ae_map_share_address_local(address))
			return kilobytes;
	}
	return lan_kilobytes;
}
