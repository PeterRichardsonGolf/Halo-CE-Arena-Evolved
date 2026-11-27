/*
AE_CE_ARCHIVE_RULES.H

The community map archive's pure rules (ae_ce_archive.h is the library that fetches and installs): the signed map
list's signature, its parse (bounded), the names a map may be installed under, the addresses and answers it takes,
the list's search / sort / paging, and the check of a map's header. No network, no files, no threads.

The list's format, its keys' use, its signature's check and the tab-separated lines are from the Android host's
fractum_archive.c (thelinkin3000) as Halo Content Evolved's port/linux/src/map_archive.c (Watanoge) brought them to
the desktop; both AGPL-3.0-or-later. The bounds, the name rules and the address and answer rules are this port's.

A list is a text, a line a map, fields separated by tabs:
	file name (with ".map"), header checksum (8 hex digits), size, piece length, info hash,
	blake2b:<64 hex digits>, share:<64 hex digits>, sources
signed as "fractum-halo-map-index 1\n" followed by the list's exact bytes (the signature: 128 hex digits, by one of
the keys the build pins). Plain types only: the game's units and the platform's both include it.
*/

#ifndef AE_CE_ARCHIVE_RULES_H
#define AE_CE_ARCHIVE_RULES_H

#include <stddef.h>
#include <stdint.h>

enum
{
	/* the list, and its signature's text, at most (a longer one is refused before it is read) */
	AE_CE_ARCHIVE_LIST_MAXIMUM = 8 * 1024 * 1024,
	AE_CE_ARCHIVE_SIGNATURE_MAXIMUM = 1024,
	/* the maps a list may name, its lines in all, and one line's length */
	AE_CE_ARCHIVE_ENTRIES_MAXIMUM = 20000,
	AE_CE_ARCHIVE_LINES_MAXIMUM = 40000,
	AE_CE_ARCHIVE_LINE_MAXIMUM = 2048,
	/* a map file's name: its name's letters, digits, '_' and '-' (the game's 31, and a second version's _<8 digits>),
	then ".map" */
	AE_CE_ARCHIVE_STEM_MAXIMUM = 40,
	AE_CE_ARCHIVE_FILE_SIZE = 48,
	AE_CE_ARCHIVE_HASH_BYTES = 32,
	/* the header of a map file */
	AE_CE_ARCHIVE_HEADER_BYTES = 0x800,
	AE_CE_ARCHIVE_SEARCH_SIZE = 64,
	/* a list's "# serial": at most this many digits, and no more than the maximum below */
	AE_CE_ARCHIVE_SERIAL_DIGITS = 15
};

/* the largest serial taken: a quadrillion (a serial counted in seconds or in days is far below it) */
#define AE_CE_ARCHIVE_SERIAL_MAXIMUM 999999999999999ULL

/* the largest map taken, whatever a list says (bytes) */
#define AE_CE_ARCHIVE_MAP_MAXIMUM (400ULL * 1024 * 1024)
/* the bytes a disk keeps free after a map (the same margin as map sharing's) */
#define AE_CE_ARCHIVE_DISK_MARGIN (256ULL * 1024 * 1024)

struct ae_ce_archive_entry
{
	/* the file name as the list gives it (the address's), and the one it is installed under: lowercase */
	char listed[AE_CE_ARCHIVE_FILE_SIZE];
	char file[AE_CE_ARCHIVE_FILE_SIZE];
	unsigned long long size;
	unsigned int checksum;
	uint8_t blake2b[AE_CE_ARCHIVE_HASH_BYTES];
	/* map sharing's hash: all zero when the list has none */
	uint8_t share[AE_CE_ARCHIVE_HASH_BYTES];
	unsigned char installed;
};

/* whether file is a name a map may be installed under: 1 to AE_CE_ARCHIVE_STEM_MAXIMUM of letters, digits, '_' and
'-', then ".map" (any case) and nothing else: no separator, no other dot, no space, not a name Windows reserves (CON,
PRN, AUX, NUL, COM1-9, LPT1-9). 1 with the lowercase file in installed (AE_CE_ARCHIVE_FILE_SIZE bytes, may be NULL) */
int ae_ce_archive_file_name_ok(char const *file, char *installed);

/* whether signature_text (the list's .sig: hex, spaces around it allowed) is the signature of the list's bytes by one
of keys (32 bytes each) */
int ae_ce_archive_list_signed(unsigned char const *list, size_t list_size, char const *signature_text,
	size_t signature_size, uint8_t const (*keys)[32], int key_count);

/* the list's maps into out (capacity out_size entries): their count in *count, the lines left out (a name or a
number the rules refuse, a resource map, a name given twice) in *skipped, the list's "# serial" in *serial. 1 if it
is a list of this format within the bounds above; else 0 with why in error and nothing in out. Refused whole: not
the list's format line first, a NUL in it, a line longer than AE_CE_ARCHIVE_LINE_MAXIMUM, more lines or maps than
the bounds, no map left */
int ae_ce_archive_list_parse(unsigned char const *list, size_t size, struct ae_ce_archive_entry *out, int out_size,
	int *count, int *skipped, unsigned long long *serial, char *error, int error_size);

/* ---------- addresses and answers */

struct ae_ce_archive_url
{
	/* 1 for https */
	int secure;
	char host[128];
	int port;
	char path[512];
};

/* http://host[:port]/path or https://...: the host a name of letters, digits, '.' and '-' (no user name, no IPv6 form),
a port 1-65535, a path that is empty (then "/") or starts with '/' and holds no space, control character or '#'. 1 or 0 */
int ae_ce_archive_url_parse(char const *url, struct ae_ce_archive_url *out);
/* whether host is this machine's own, 127.0.0.1 or localhost (the only host a test's server may be) */
int ae_ce_archive_host_is_loopback(char const *host);
/* base's path followed by name, each byte outside letters, digits and "-_.~" as %XX: 1 if it fits */
int ae_ce_archive_map_path(char const *base_path, char const *name, char *path, size_t path_size);

struct ae_ce_archive_head
{
	int status;
	/* -1 when the answer names none, -2 when it names two that differ or one that is no number */
	long long content_length;
	int chunked;
	/* a Content-Encoding: the bytes are not the file's */
	int encoded;
	int has_location;
};

/* an answer's head (the status line and the headers, size bytes, up to before the empty line): 1 if it reads */
int ae_ce_archive_head_parse(char const *head, size_t size, struct ae_ce_archive_head *out);

/* whether an answer's head lets a file of size bytes be read: status 200, no redirect, no encoding, no chunks, a
Content-Length that is the size exactly; else 0 with why in error */
int ae_ce_archive_head_allows(struct ae_ce_archive_head const *head, unsigned long long size, char *error,
	int error_size);

/* ---------- disk and header */

/* whether a disk with free bytes takes size more and keeps AE_CE_ARCHIVE_DISK_MARGIN */
int ae_ce_archive_disk_allows(unsigned long long free_bytes, unsigned long long size);

/* whether the first AE_CE_ARCHIVE_HEADER_BYTES bytes of a map file of file_size bytes are a Custom Edition cache
file's header (the 'head' and 'foot' words, cache version 609, its length and tag data within the file, a name that
ends); else 0 with why in error. Whether it can be played the game's own checks decide before it is */
int ae_ce_archive_header_ok(unsigned char const *header, unsigned long long file_size, char *error, int error_size);

/* ---------- the highest serial seen

The serial and the BLAKE2b of the highest list ever taken are kept apart from the kept list (a file of their own), so
a list cleared from the cache is not forgotten: no list with a lower serial is taken, and none with the same serial and
other bytes. A text "serial <digits> <64 hex digits>" and a line feed. */

/* the days a list's serial may be ahead of the client's clock (a serial is Unix seconds) */
#define AE_CE_ARCHIVE_SERIAL_DAYS_AHEAD 30
/* whether a serial is no more than AE_CE_ARCHIVE_SERIAL_DAYS_AHEAD days after now (Unix seconds); serial 0 (none) always */
int ae_ce_archive_serial_in_time(unsigned long long serial, unsigned long long now);

/* the record's text read: 1 with its serial and hash (32 bytes) */
int ae_ce_archive_record_parse(char const *text, unsigned long long *serial, uint8_t *hash);
void ae_ce_archive_record_format(unsigned long long serial, uint8_t const *hash, char *text, size_t size);
/* whether a list of this serial and hash may be taken after the record (have_record 0: no record yet); else 0 with why */
int ae_ce_archive_serial_allows(int have_record, unsigned long long record_serial, uint8_t const *record_hash,
	unsigned long long serial, uint8_t const *hash, char *error, int error_size);

/* whether an IPv4 address (4 bytes, first octet first) can be the archive's: not this machine's, a private, link-local,
shared or multicast one */
int ae_ce_archive_address_public(unsigned char const *octets);

/* ---------- search, sort, paging */

enum { AE_CE_ARCHIVE_SORT_NAME, AE_CE_ARCHIVE_SORT_SIZE, AE_CE_ARCHIVE_SORT_COUNT };
enum { AE_CE_ARCHIVE_SHOW_ALL, AE_CE_ARCHIVE_SHOW_INSTALLED, AE_CE_ARCHIVE_SHOW_MISSING, AE_CE_ARCHIVE_SHOW_COUNT };

struct ae_ce_archive_query
{
	/* part of a name, any case; "" for every map */
	char text[AE_CE_ARCHIVE_SEARCH_SIZE];
	int sort;
	int descending;
	int show;
};

/* the entries that match the query, sorted, as their indexes into view (capacity count): how many. Names sort
without regard to case, then by size; sizes sort by size, then name */
int ae_ce_archive_view_build(struct ae_ce_archive_entry const *entries, int count, struct ae_ce_archive_query const *query,
	int *view);

/* the page of per_page rows of a view of view_count rows: the first row's place, and how many pages there are
(at least 1); page is clamped into the range */
int ae_ce_archive_page_first(int view_count, int per_page, int *page, int *pages);

/* the hex digits of text (length chars) into bytes; 1 if it is exactly count bytes of hex */
int ae_ce_archive_hex(char const *text, size_t length, uint8_t *bytes, size_t count);

#endif
