/*
AE_MAP_SHARE_RULES.H

Arena Evolved: map sharing's safety rules (from DamnationCE v0.3.41, CC0), as
pure functions with no engine calls (port/linux/tests/ae_map_share_rules_test.c
runs them):

- A source the host names as a LAN player is reached at its address only when
  that address is a local network's (private 10/8, 172.16/12, 192.168/16,
  link-local 169.254/16, loopback 127/8). A host can so never have its players
  open connections to anywhere on the internet. Internet play's sources are
  reached by their keys, never at an address the host gave, and are not
  affected.
- A download that finished but failed its check (the host's version, a broken
  map, a resource map it needs) is removed from the downloads' folder, never
  left there under the map's name, where it would stand in for the real one.
  Only a file directly in that folder, named <something>.map, is removed: a
  path with another folder in it (a separator, "..") is refused.
*/

#ifndef __AE_MAP_SHARE_RULES_H
#define __AE_MAP_SHARE_RULES_H

/* whether an IPv4 address (host byte order) is a local network's: private,
link-local or loopback. 0.0.0.0, broadcast and anything above 32 bits are not */
int ae_map_share_address_local(
	unsigned long address);

/* whether path is a file directly in folder (folder ends in its separator, as
"d:\\maps_ce\\"), named <name>.map: nothing after the folder but a plain name */
int ae_map_share_path_in_folder(
	char const *path,
	char const *folder);

/* the failed download's files removed: each of the count paths that is in
folder (ae_map_share_path_in_folder) handed to delete_file; the others left
alone. Returns how many delete_file removed (it returns nonzero for one it did) */
int ae_map_share_discard(
	char const *const *paths,
	int count,
	char const *folder,
	int (*delete_file)(char const *path, void *context),
	void *context);

/* ---------- sharing every map but the game's own (0.2.3; ae_map_share_all.c is the engine's side)

A host lists (its catalogue) every map it has that is not one of the retail game's: community maps in the Xbox's
format (its maps folder's, its active mod's, those it downloaded) and Custom Edition maps named for their hashes. A
guest that runs this game is sent the list, and may fetch a map of it it lacks: the one the game is on, or all of
them. These are the rules of both ends, with no engine calls. */

enum
{
	AE_MAP_SHARE_KIND_NONE,
	/* a community map in the Xbox's format, of the maps folder (or one downloaded) */
	AE_MAP_SHARE_KIND_XBOX,
	/* ... of the host's active mod (mods/<mod>/maps) */
	AE_MAP_SHARE_KIND_MOD,
	/* a Custom Edition map, named for its hash (<name>_<8 digits>) */
	AE_MAP_SHARE_KIND_CE,
	/* Custom Edition's bitmaps, sounds or loc, which the engine sends with a map that needs them: listed for
	their sizes alone */
	AE_MAP_SHARE_KIND_RESOURCE,
	AE_MAP_SHARE_KINDS
};

enum
{
	/* a map's name with its end (a Custom Edition level's longest file name is 51) */
	AE_MAP_SHARE_NAME_BYTES = 52,
	/* an Xbox map's name is its header's: 31 characters at most */
	AE_MAP_SHARE_XBOX_NAME_LENGTH = 31,
	AE_MAP_SHARE_HASH_BYTES = 32,
	/* the most maps a host lists, and a guest takes of a list */
	AE_MAP_SHARE_CATALOGUE_MAXIMUM = 512,
	/* the entries one frame carries */
	AE_MAP_SHARE_FRAME_ENTRIES = 10,
	/* an entry in a frame: name, kind, 3 unused, size (little-endian), hash */
	AE_MAP_SHARE_WIRE_ENTRY_BYTES = AE_MAP_SHARE_NAME_BYTES + 4 + 4 + AE_MAP_SHARE_HASH_BYTES,
	/* a frame's own bytes before its entries: type, flags, expected, first (little-endian words), count, unused */
	AE_MAP_SHARE_WIRE_FRAME_BYTES = 8,
	/* megabytes a guest takes of one host: the default, and the setting's most */
	AE_MAP_SHARE_CAP_DEFAULT_MEGABYTES = 4096,
	AE_MAP_SHARE_CAP_MAXIMUM_MEGABYTES = 65536,
};

/* the engine's own limits of a map's size, by kind: the multiplayer map cache's for an Xbox map
(HALO_PORT_MULTIPLAYER_CACHE_SIZE), map sharing's for a Custom Edition map and a resource map (map_share.c) */
#define AE_MAP_SHARE_XBOX_MAXIMUM_BYTES 0x08000000UL
#define AE_MAP_SHARE_CE_MAXIMUM_BYTES 0x30000000UL
#define AE_MAP_SHARE_RESOURCE_MAXIMUM_BYTES 0x20000000UL
/* the disk's free bytes that must be left after a download */
#define AE_MAP_SHARE_DISK_MARGIN_BYTES 0x10000000UL

/* frames' types */
enum
{
	/* the host runs this game: flags say whether it shares with this guest */
	AE_MAP_SHARE_FRAME_HELLO = 1,
	/* some of the catalogue's entries (first 0 starts it anew) */
	AE_MAP_SHARE_FRAME_CATALOGUE = 2,
};
#define AE_MAP_SHARE_FLAG_SHARED 0x01

struct ae_map_share_entry
{
	char name[AE_MAP_SHARE_NAME_BYTES];
	unsigned char kind;
	unsigned long size;
	unsigned char hash[AE_MAP_SHARE_HASH_BYTES];
};

struct ae_map_share_catalogue
{
	/* how many entries the host said it has, how many came whole frames of, and how many were taken */
	int expected;
	int received;
	int count;
	struct ae_map_share_entry entries[AE_MAP_SHARE_CATALOGUE_MAXIMUM];
};

struct ae_map_share_frame
{
	int type;
	int flags;
	int expected;
	int first;
	int count;
	struct ae_map_share_entry entries[AE_MAP_SHARE_FRAME_ENTRIES];
};

/* whether name (a map's file name without .map, any case) is one of the retail game's: its 13 multiplayer maps, its
10 campaign maps, ui, and Custom Edition's resource maps' names (bitmaps, sounds, loc). ae_map_share_stock_names
lists them (ended by NULL) */
int ae_map_share_stock_name(char const *name);
char const *const *ae_map_share_stock_names(void);

/* whether name is one a list may carry: 1 to 51 of letters, digits, _ - + ( ) . and space, no dot or space at
either end, no two dots together: a file name of one folder, on any system */
int ae_map_share_name_allowed(char const *name);
/* whether name ends in _ and 8 lowercase hexadecimal digits after at least one character */
int ae_map_share_name_hashed(char const *name);
/* whether a file of size bytes is one the engine takes of the kind (never an empty one) */
int ae_map_share_size_allowed(int kind, unsigned long long size);
/* whether an entry is one a list may carry: its kind known, its name allowed for the kind (a map's never a stock
name; an Xbox map's 31 characters at most; a Custom Edition map's hashed; a resource map's bitmaps, sounds or loc),
its size allowed */
int ae_map_share_entry_valid(struct ae_map_share_entry const *entry);

/* a list started anew for expected entries: 0 (and an empty list) if more than the maximum */
int ae_map_share_catalogue_begin(struct ae_map_share_catalogue *catalogue, int expected);
/* a frame's entries taken: 0 if the frame is not the next of the list (first is not what came so far, or it holds
more than was expected); the entries that are not valid, or named as one before (any case), are left out */
int ae_map_share_catalogue_take(struct ae_map_share_catalogue *catalogue, struct ae_map_share_frame const *frame);
int ae_map_share_catalogue_complete(struct ae_map_share_catalogue const *catalogue);
/* the entry named so (any case) of a map kind (resource: 0 a map's, 1 a resource map's), or -1 */
int ae_map_share_catalogue_find(struct ae_map_share_catalogue const *catalogue, char const *name, int resource);

/* a frame from a message's bytes and into them (the bytes written, 0 if they do not fit); decoding refuses a frame
of another size than its count says, an unknown type, more entries than a frame's, a name not ended */
int ae_map_share_frame_decode(unsigned char const *bytes, long size, struct ae_map_share_frame *frame);
long ae_map_share_frame_encode(struct ae_map_share_frame const *frame, unsigned char *bytes, long size);

/* whether what a host offers for an entry is what its list said: the size and the whole hash */
int ae_map_share_offer_matches(struct ae_map_share_entry const *entry, unsigned long size,
	unsigned char const *hash);

/* the megabytes setting as a guest uses it (0 to the maximum), and whether size more bytes fit under it after
taken bytes */
long ae_map_share_cap_megabytes(long setting);
int ae_map_share_cap_allows(unsigned long long taken, unsigned long long size, long cap_megabytes);
/* whether a disk with free bytes takes size more and keeps the margin */
int ae_map_share_disk_allows(unsigned long long free_bytes, unsigned long long size);

/* what was counted toward a host's total, by file (a key: its kind and level): a file offered again (a download taken
up again, or a host that lists it again at another size) is charged its growth over what it was charged, never
nothing for more bytes. The table is small: a key pushed out by newer ones is charged whole again */
enum
{
	AE_MAP_SHARE_CHARGES = 96,
	AE_MAP_SHARE_CHARGE_KEY_BYTES = 72,
};
struct ae_map_share_charges
{
	struct
	{
		char key[AE_MAP_SHARE_CHARGE_KEY_BYTES];
		unsigned long long size;
	} items[AE_MAP_SHARE_CHARGES];
	int next;
};
/* the bytes to count for a file of size under key: size less what the key was charged (0 if no more) */
unsigned long long ae_map_share_charge_growth(struct ae_map_share_charges const *charges, char const *key,
	unsigned long long size);
/* the key charged size (its largest so far kept) */
void ae_map_share_charge_set(struct ae_map_share_charges *charges, char const *key, unsigned long long size);

/* whether maps are shared (a host's setting) or downloaded (a guest's) with a machine: the setting on, and the
machine on the local network (ae_map_share_address_local of its address) or the internet's setting on too */
int ae_map_share_policy_allows(int local, int setting, int internet_setting);

/* Map downloads are the OpenCE way: no question. network.map_download on (and network.map_download_internet for a
host that is not on the local network, ae_map_share_policy_allows) is the player's yes. What the card and the dialog
say follows. */

/* a map's name as the card and the dialog show it: the folder, the extension (.map) and a level's "@ce" dropped, the "_" and 8
hexadecimal digits a second version ends in dropped, underscores spaces, each word's first letter a capital
("h2_coagulation_9fc847de" is "H2 Coagulation"). out is size bytes; "" if name is NULL */
void ae_map_share_display_name(char const *name, char *out, int size);

/* bytes as megabytes in text: "210 MB" from 10 MB, else one decimal ("9.8 MB", "0.4 MB") */
void ae_map_share_megabytes_text(unsigned long long bytes, char *out, int size);
/* a download's amount: "31 of 73 MB" (both whole megabytes if total is 10 MB or more, else both with one decimal:
"0.4 of 9.8 MB") */
void ae_map_share_amount_text(unsigned long long done, unsigned long long total, char *out, int size);

/* why a download did not come: the reasons of map_share.c's download_fail and of this game's refusals, in groups */
enum
{
	AE_MAP_SHARE_WHY_OTHER,
	AE_MAP_SHARE_WHY_NOT_SHARED,
	AE_MAP_SHARE_WHY_CANNOT_SEND,
	AE_MAP_SHARE_WHY_WRONG_FILE,
	AE_MAP_SHARE_WHY_NO_MEMORY,
	AE_MAP_SHARE_WHY_DISK_FULL,
	AE_MAP_SHARE_WHY_NAME_TAKEN,
	AE_MAP_SHARE_WHY_WONT_LOAD,
	AE_MAP_SHARE_WHY_NO_ANSWER,
	AE_MAP_SHARE_WHY_STALLED,
	AE_MAP_SHARE_WHY_OVER_LIMIT,
	AE_MAP_SHARE_WHY_SETTING_OFF,
	AE_MAP_SHARE_WHY_INTERNET_OFF,
	AE_MAP_SHARE_WHY_STOPPED,
	AE_MAP_SHARE_WHY_COUNT,
};
/* the group a reason (the text download_fail was given) belongs to */
int ae_map_share_why(char const *reason);
/* the card's short tag beside the map's name ("disk full") */
char const *ae_map_share_why_tag(int why);
/* the card's line of what to do ("Free up space, then join again."); limit_megabytes is the cap a host may take */
void ae_map_share_why_line(int why, long limit_megabytes, char *out, int size);
/* the dialog's text after "The map <Name> did not download." and a blank line; reason is used for a group of none */
void ae_map_share_why_dialog(int why, long limit_megabytes, char const *reason, char *out, int size);

/* requests counted per machine: burst at once, one more each refill milliseconds */
struct ae_map_share_rate
{
	unsigned long at;
	int tokens;
	int started;
};
int ae_map_share_rate_allow(struct ae_map_share_rate *rate, unsigned long now, int burst, unsigned long refill);

/* what a guest fetches of a list: the maps it lacks (lacks says so of an entry), in the list's order, while they
fit under the cap after taken bytes; the resource maps it lacks counted once if a Custom Edition map is fetched.
indices gets the maps' (at most maximum), bytes their sizes' sum with the resource maps', over how many lacked maps
did not fit. Returns how many maps */
int ae_map_share_plan(struct ae_map_share_catalogue const *catalogue,
	int (*lacks)(struct ae_map_share_entry const *entry, void *context), void *context, long cap_megabytes,
	unsigned long long taken, short *indices, int maximum, unsigned long long *bytes, int *over);

#endif
