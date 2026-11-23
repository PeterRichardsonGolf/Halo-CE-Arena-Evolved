/* ae_ce_archive_rules.c: the community map archive's pure rules. The list's signature (made here with a key made
here), its parse and its bounds, the names a map may be installed under, the addresses and answers taken, the disk
and header rules, and the search, sort and paging. Nothing here reaches a network. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_ce_archive_rules.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void hex(uint8_t const *bytes, int count, char *text)
{
	int i;

	for (i = 0; i < count; i++)
		sprintf(text + 2 * i, "%02x", bytes[i]);
}

/* a list line: name, checksum, size, the hash digits (c) in blake2b and share */
static int line(char *out, size_t size, char const *name, char const *checksum, char const *length, char digit)
{
	char hash[65];

	memset(hash, digit, 64);
	hash[64] = 0;
	return snprintf(out, size, "%s\t%s\t%s\t262144\tinfohash\tblake2b:%s\tshare:%s\tsources\n", name, checksum, length,
		hash, hash);
}

static char const FORMAT[] = "# halo map torrents v1\n# serial 7\n";

int main(void)
{
	static struct ae_ce_archive_entry entries[AE_CE_ARCHIVE_ENTRIES_MAXIMUM];
	uint8_t seed[32], secret[64], key[2][32], other[32], signature[64];
	char signature_text[130], text[256], error[160], installed[AE_CE_ARCHIVE_FILE_SIZE];
	char list[4096];
	size_t used;
	int count, skipped, i;
	unsigned long long serial;

	/* ---------- names */
	CHECK(ae_ce_archive_file_name_ok("hugeass.map", installed) && !strcmp(installed, "hugeass.map"));
	CHECK(ae_ce_archive_file_name_ok("Cyber_Arena-2.MAP", installed) && !strcmp(installed, "cyber_arena-2.map"));
	CHECK(ae_ce_archive_file_name_ok("a.map", NULL));
	/* a name shaped <base>_<8 hex digits> is the form a version's name takes: refused; other shapes are not */
	CHECK(!ae_ce_archive_file_name_ok("map_0a1b2c3d.map", NULL) && !ae_ce_archive_file_name_ok("trap_1A2B3C4D.map", NULL) &&
		ae_ce_archive_file_name_ok("map_0a1b2c3.map", NULL) && ae_ce_archive_file_name_ok("map_0a1b2c3dd.map", NULL) &&
		ae_ce_archive_file_name_ok("map_0a1b2c3g.map", NULL));
	/* an 8.3 short name's "~" is no character of a name */
	CHECK(!ae_ce_archive_file_name_ok("LONGNA~1.map", NULL) && !ae_ce_archive_file_name_ok("a~.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("../x.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("..\\x.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("a/b.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("/etc/passwd.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("c:evil.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x.map.exe", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x.map.", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x.map ", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x .map", NULL));
	CHECK(!ae_ce_archive_file_name_ok(" x.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x..map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x.y.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok(".map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x.exe", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x\n.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("x%2e.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("", NULL) && !ae_ce_archive_file_name_ok(NULL, NULL));
	CHECK(!ae_ce_archive_file_name_ok("CON.map", NULL) && !ae_ce_archive_file_name_ok("con.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("PRN.map", NULL) && !ae_ce_archive_file_name_ok("aux.MAP", NULL) &&
		!ae_ce_archive_file_name_ok("Nul.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("COM1.map", NULL) && !ae_ce_archive_file_name_ok("lpt9.map", NULL));
	CHECK(!ae_ce_archive_file_name_ok("com0.map", NULL) && !ae_ce_archive_file_name_ok("LPT0.map", NULL) &&
		ae_ce_archive_file_name_ok("console.map", NULL) && ae_ce_archive_file_name_ok("com10.map", NULL));
	{
		char longest[80];

		memset(longest, 'a', AE_CE_ARCHIVE_STEM_MAXIMUM);
		strcpy(longest + AE_CE_ARCHIVE_STEM_MAXIMUM, ".map");
		CHECK(ae_ce_archive_file_name_ok(longest, NULL));
		memset(longest, 'a', AE_CE_ARCHIVE_STEM_MAXIMUM + 1);
		strcpy(longest + AE_CE_ARCHIVE_STEM_MAXIMUM + 1, ".map");
		CHECK(!ae_ce_archive_file_name_ok(longest, NULL));
	}

	/* ---------- the signature: a key made here */
	memset(seed, 7, sizeof(seed));
	crypto_ed25519_key_pair(secret, key[0], seed);
	{
		uint8_t secret_other[64];

		memset(seed, 9, sizeof(seed));
		crypto_ed25519_key_pair(secret_other, other, seed);
	}
	memset(key[1], 0x55, 32);
	used = (size_t)snprintf(list, sizeof(list), "%s", FORMAT);
	used += (size_t)line(list + used, sizeof(list) - used, "alpha.map", "0badf00d", "1000", 'a');
	used += (size_t)line(list + used, sizeof(list) - used, "Beta.map", "00c0ffee", "5000", 'b');
	{
		uint8_t message[sizeof(list) + 32];
		size_t context = strlen("fractum-halo-map-index 1\n");

		memcpy(message, "fractum-halo-map-index 1\n", context);
		memcpy(message + context, list, used);
		crypto_ed25519_sign(signature, secret, message, context + used);
	}
	hex(signature, 64, signature_text);
	CHECK(ae_ce_archive_list_signed((unsigned char const *)list, used, signature_text, 128, (uint8_t const (*)[32])key, 2));
	/* the second key of the two signs as well; spaces and a line feed around the signature are fine */
	snprintf(text, sizeof(text), "  %s\r\n", signature_text);
	CHECK(ae_ce_archive_list_signed((unsigned char const *)list, used, text, strlen(text), (uint8_t const (*)[32])key, 2));
	/* a list with a byte changed, a signature with one, a short one, a long one, no hex, another key, no key */
	list[used - 3] ^= 1;
	CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, used, signature_text, 128, (uint8_t const (*)[32])key, 2));
	list[used - 3] ^= 1;
	signature_text[10] = signature_text[10] == '0' ? '1' : '0';
	CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, used, signature_text, 128, (uint8_t const (*)[32])key, 2));
	signature_text[10] = signature_text[10] == '0' ? '1' : '0';
	CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, used, signature_text, 126, (uint8_t const (*)[32])key, 2));
	CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, used, "", 0, (uint8_t const (*)[32])key, 2));
	CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, used, "zz", 2, (uint8_t const (*)[32])key, 2));
	{
		uint8_t const (*wrong)[32] = (uint8_t const (*)[32])other;

		CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, used, signature_text, 128, wrong, 1));
	}
	CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, used, signature_text, 128, (uint8_t const (*)[32])key, 0));
	CHECK(!ae_ce_archive_list_signed((unsigned char const *)list, AE_CE_ARCHIVE_LIST_MAXIMUM + 1, signature_text, 128,
		(uint8_t const (*)[32])key, 2));

	/* ---------- parse: a good list */
	CHECK(ae_ce_archive_list_parse((unsigned char const *)list, used, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
		&skipped, &serial, error, sizeof(error)));
	CHECK(count == 2 && skipped == 0 && serial == 7);
	CHECK(!strcmp(entries[0].file, "alpha.map") && entries[0].size == 1000 && entries[0].checksum == 0x0badf00dUL);
	CHECK(!strcmp(entries[1].listed, "Beta.map") && !strcmp(entries[1].file, "beta.map") && entries[1].size == 5000);
	CHECK(entries[0].blake2b[0] == 0xaa && entries[0].share[31] == 0xaa);

	/* lines the rules leave out: a path, a reserved name, a double extension, a bad size, a bad hash, a resource map,
	a name given twice; the rest is kept */
	{
		static char big[16384];
		size_t at = (size_t)snprintf(big, sizeof(big), "%s", FORMAT);

		at += (size_t)line(big + at, sizeof(big) - at, "../evil.map", "0badf00d", "100", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "a/b.map", "0badf00d", "100", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "CON.map", "0badf00d", "100", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "x.map.exe", "0badf00d", "100", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "trail.map.", "0badf00d", "100", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "space.map ", "0badf00d", "100", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "zero.map", "0badf00d", "0", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "huge.map", "0badf00d", "999999999999", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "huge2.map", "0badf00d", "419430401", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "neg.map", "0badf00d", "-5", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "hexsize.map", "0badf00d", "0x10", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "badsum.map", "0badf0zz", "100", 'a');
		at += (size_t)line(big + at, sizeof(big) - at, "resource.map", "00000000", "100", 'a');
		at += (size_t)snprintf(big + at, sizeof(big) - at, "nohash.map\t0badf00d\t100\t1\tx\tblake2b:abcd\n");
		at += (size_t)line(big + at, sizeof(big) - at, "good.map", "0badf00d", "419430400", 'c');
		at += (size_t)line(big + at, sizeof(big) - at, "GOOD.map", "0badf00d", "100", 'd');
		CHECK(ae_ce_archive_list_parse((unsigned char const *)big, at, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
			&skipped, &serial, error, sizeof(error)));
		CHECK(count == 1 && !strcmp(entries[0].file, "good.map") && entries[0].size == AE_CE_ARCHIVE_MAP_MAXIMUM);
		CHECK(skipped == 15);
	}

	/* a list with no map left, not the format, a NUL, a line too long, the wrong first line */
	CHECK(!ae_ce_archive_list_parse((unsigned char const *)FORMAT, strlen(FORMAT), entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM,
		&count, &skipped, &serial, error, sizeof(error)) && count == 0 && error[0]);
	CHECK(!ae_ce_archive_list_parse((unsigned char const *)"hello\n", 6, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
		&skipped, &serial, error, sizeof(error)));
	CHECK(!ae_ce_archive_list_parse((unsigned char const *)"", 0, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count, &skipped,
		&serial, error, sizeof(error)));
	CHECK(!ae_ce_archive_list_parse((unsigned char const *)"# halo map torrents v10\n", 24, entries,
		AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count, &skipped, &serial, error, sizeof(error)));
	list[used - 5] = 0;
	CHECK(!ae_ce_archive_list_parse((unsigned char const *)list, used, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
		&skipped, &serial, error, sizeof(error)) && strstr(error, "NUL"));
	list[used - 5] = 'a';

	/* a list cut short in the middle of a line (the signature would not hold; the parse is safe too) */
	CHECK(ae_ce_archive_list_parse((unsigned char const *)list, used - 100, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
		&skipped, &serial, error, sizeof(error)) && count == 1 && skipped == 1);

	/* oversized: more maps than the bound, more lines, a line of 2 MB (a huge string), a list over the size limit */
	{
		size_t cap = 6 * 1024 * 1024;
		char *huge = (char *)malloc(cap);
		size_t at = (size_t)snprintf(huge, cap, "%s", FORMAT);

		for (i = 0; i < AE_CE_ARCHIVE_ENTRIES_MAXIMUM + 1; i++)
		{
			char name[32];

			snprintf(name, sizeof(name), "m%d.map", i);
			at += (size_t)line(huge + at, cap - at, name, "0badf00d", "100", 'a');
		}
		CHECK(!ae_ce_archive_list_parse((unsigned char const *)huge, at, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
			&skipped, &serial, error, sizeof(error)) && count == 0);
		/* exactly the bound is taken */
		at = (size_t)snprintf(huge, cap, "%s", FORMAT);
		for (i = 0; i < AE_CE_ARCHIVE_ENTRIES_MAXIMUM; i++)
		{
			char name[32];

			snprintf(name, sizeof(name), "m%d.map", i);
			at += (size_t)line(huge + at, cap - at, name, "0badf00d", "100", 'a');
		}
		CHECK(ae_ce_archive_list_parse((unsigned char const *)huge, at, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
			&skipped, &serial, error, sizeof(error)) && count == AE_CE_ARCHIVE_ENTRIES_MAXIMUM);
		/* too many comment lines */
		at = (size_t)snprintf(huge, cap, "%s", FORMAT);
		for (i = 0; i < AE_CE_ARCHIVE_LINES_MAXIMUM + 5; i++)
			at += (size_t)snprintf(huge + at, cap - at, "#\n");
		at += (size_t)line(huge + at, cap - at, "x.map", "0badf00d", "100", 'a');
		CHECK(!ae_ce_archive_list_parse((unsigned char const *)huge, at, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
			&skipped, &serial, error, sizeof(error)));
		/* a line of 2 MB (no tab, no end) */
		at = (size_t)snprintf(huge, cap, "%s", FORMAT);
		memset(huge + at, 'x', 2 * 1024 * 1024);
		at += 2 * 1024 * 1024;
		CHECK(!ae_ce_archive_list_parse((unsigned char const *)huge, at, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count,
			&skipped, &serial, error, sizeof(error)) && strstr(error, "longer"));
		/* a list past the size limit is refused before it is looked at */
		CHECK(!ae_ce_archive_list_parse((unsigned char const *)huge, (size_t)AE_CE_ARCHIVE_LIST_MAXIMUM + 1, entries,
			AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count, &skipped, &serial, error, sizeof(error)) && strstr(error, "larger"));
		free(huge);
	}

	/* ---------- addresses */
	{
		struct ae_ce_archive_url url;
		char path[64];

		CHECK(ae_ce_archive_url_parse("https://raw.example.org/a/b.txt", &url) && url.secure && url.port == 443 &&
			!strcmp(url.host, "raw.example.org") && !strcmp(url.path, "/a/b.txt"));
		CHECK(ae_ce_archive_url_parse("http://127.0.0.1:8080/maps/", &url) && !url.secure && url.port == 8080 &&
			ae_ce_archive_host_is_loopback(url.host) && !strcmp(url.path, "/maps/"));
		CHECK(ae_ce_archive_url_parse("http://maps.example.org/maps/", &url) && url.port == 80 &&
			!ae_ce_archive_host_is_loopback(url.host));
		CHECK(ae_ce_archive_url_parse("http://a", &url) && !strcmp(url.path, "/") && url.port == 80);
		CHECK(!ae_ce_archive_url_parse("ftp://x/y", &url) && !ae_ce_archive_url_parse("http://", &url) &&
			!ae_ce_archive_url_parse("http://a:0/x", &url) &&
			!ae_ce_archive_url_parse("http://a:65536/x", &url) && !ae_ce_archive_url_parse("http://a:80x/x", &url) &&
			!ae_ce_archive_url_parse("http://user@a/x", &url) && !ae_ce_archive_url_parse("http://[::1]/x", &url) &&
			!ae_ce_archive_url_parse("http://a b/x", &url) && !ae_ce_archive_url_parse("http://a/x y", &url) &&
			!ae_ce_archive_url_parse("http://a/x#y", &url) && !ae_ce_archive_url_parse("http://a/x\r\nHost: b", &url) &&
			!ae_ce_archive_url_parse("http://a?x/y", &url));
		CHECK(!ae_ce_archive_host_is_loopback("127.0.0.2") && !ae_ce_archive_host_is_loopback("localhost.evil.org") &&
			ae_ce_archive_host_is_loopback("localhost"));
		CHECK(ae_ce_archive_map_path("/maps/", "alpha.map", path, sizeof(path)) && !strcmp(path, "/maps/alpha.map"));
		CHECK(ae_ce_archive_map_path("/maps/", "a b/c?.map", path, sizeof(path)) &&
			!strcmp(path, "/maps/a%20b%2Fc%3F.map"));
		CHECK(!ae_ce_archive_map_path("/maps/", "alpha.map", path, 12));
	}

	/* ---------- answers */
	{
		struct ae_ce_archive_head head;
		char const *good = "HTTP/1.1 200 OK\r\nServer: x\r\nContent-Length: 1000\r\nConnection: close";

		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.status == 200 && head.content_length == 1000 &&
			!head.chunked && !head.encoded && !head.has_location);
		CHECK(ae_ce_archive_head_allows(&head, 1000, error, sizeof(error)));
		CHECK(!ae_ce_archive_head_allows(&head, 999, error, sizeof(error)) && strstr(error, "size"));
		CHECK(!ae_ce_archive_head_allows(&head, 1001, error, sizeof(error)));
		good = "HTTP/1.1 302 Found\r\nLocation: http://other.example/x\r\nContent-Length: 0";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.status == 302 && head.has_location);
		CHECK(!ae_ce_archive_head_allows(&head, 0, error, sizeof(error)) && strstr(error, "another address"));
		good = "HTTP/1.1 404 Not Found\r\nContent-Length: 9";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && !ae_ce_archive_head_allows(&head, 9, error,
			sizeof(error)) && strstr(error, "does not have"));
		good = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.chunked &&
			!ae_ce_archive_head_allows(&head, 5, error, sizeof(error)));
		good = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Encoding: gzip";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.encoded &&
			!ae_ce_archive_head_allows(&head, 5, error, sizeof(error)));
		good = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Encoding: identity";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && !head.encoded);
		good = "HTTP/1.1 200 OK\r\nServer: x";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.content_length == -1 &&
			!ae_ce_archive_head_allows(&head, 5, error, sizeof(error)));
		/* two lengths that differ, a length that is no number, a negative one */
		good = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 6";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.content_length == -2);
		good = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.content_length == 5);
		good = "HTTP/1.1 200 OK\r\nContent-Length: 5x";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.content_length == -2);
		good = "HTTP/1.1 200 OK\r\nContent-Length: -5";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.content_length == -2);
		good = "HTTP/1.1 200 OK\r\nCONTENT-LENGTH: 7";
		CHECK(ae_ce_archive_head_parse(good, strlen(good), &head) && head.content_length == 7);
		CHECK(!ae_ce_archive_head_parse("garbage", 7, &head) && !ae_ce_archive_head_parse("", 0, &head) &&
			!ae_ce_archive_head_parse("HTTP/2 200", 10, &head) && !ae_ce_archive_head_parse("HTTP/1.1 99 x", 13, &head));
	}

	/* ---------- disk and header */
	CHECK(ae_ce_archive_disk_allows(AE_CE_ARCHIVE_DISK_MARGIN + 100, 100));
	CHECK(!ae_ce_archive_disk_allows(AE_CE_ARCHIVE_DISK_MARGIN + 99, 100));
	CHECK(!ae_ce_archive_disk_allows(10, 1) && !ae_ce_archive_disk_allows(0, 0));
	{
		unsigned char header[AE_CE_ARCHIVE_HEADER_BYTES];

		memset(header, 0, sizeof(header));
		memcpy(header, "daeh", 4);
		memcpy(header + 0x7fc, "toof", 4);
		header[4] = 0x61;
		header[5] = 0x02;
		header[0x10 + 1] = 0x08; /* tag data at 0x800 */
		header[0x14 + 1] = 0x01; /* 0x100 bytes of it */
		memcpy(header + 0x20, "alpha", 6);
		CHECK(ae_ce_archive_header_ok(header, 0x4000, error, sizeof(error)));
		CHECK(!ae_ce_archive_header_ok(header, 0x7ff, error, sizeof(error)));
		CHECK(!ae_ce_archive_header_ok(header, 0x800, error, sizeof(error)) && strstr(error, "tag data"));
		header[8] = 0xff;
		header[9] = 0xff;
		CHECK(!ae_ce_archive_header_ok(header, 0x4000, error, sizeof(error)) && strstr(error, "says"));
		header[8] = header[9] = 0;
		header[4] = 7;
		CHECK(!ae_ce_archive_header_ok(header, 0x4000, error, sizeof(error)) && strstr(error, "version"));
		header[4] = 0x61;
		header[0] = 'x';
		CHECK(!ae_ce_archive_header_ok(header, 0x4000, error, sizeof(error)) && strstr(error, "not a cache file"));
		header[0] = 'd';
		memset(header + 0x20, 'z', 0x20);
		CHECK(!ae_ce_archive_header_ok(header, 0x4000, error, sizeof(error)) && strstr(error, "name"));
		CHECK(!ae_ce_archive_header_ok(header, AE_CE_ARCHIVE_MAP_MAXIMUM + 1, error, sizeof(error)));
	}

	/* ---------- the serial, the record of the highest, addresses */
	{
		static char big[512];
		uint8_t hash[32], other_hash[32], parsed[32];
		unsigned char octets[4];
		unsigned long long record;
		size_t at;
		int have;

		/* a serial of 15 digits is taken, 16 or a letter or none is refused (none is 0) */
		for (have = 0; have < 4; have++)
		{
			char const *serials[4] = { "999999999999999", "1000000000000000", "99999999999999999999999", "12x" };

			at = (size_t)snprintf(big, sizeof(big), "# halo map torrents v1\n# serial %s\n", serials[have]);
			at += (size_t)line(big + at, sizeof(big) - at, "a.map", "0badf00d", "100", 'a');
			CHECK(ae_ce_archive_list_parse((unsigned char const *)big, at, entries, 10, &count, &skipped, &serial, error,
				sizeof(error)) == (have == 0 || have == 3 ? 1 : 0));
		}
		CHECK(serial == 12 || serial == 0);
		memset(hash, 1, 32);
		memset(other_hash, 2, 32);
		ae_ce_archive_record_format(77, hash, text, sizeof(text));
		CHECK(!strncmp(text, "serial 77 0101", 14) && text[strlen(text) - 1] == '\n' && strlen(text) == 7 + 3 + 64 + 1);
		CHECK(ae_ce_archive_record_parse(text, &record, parsed) && record == 77 && !memcmp(parsed, hash, 32));
		CHECK(!ae_ce_archive_record_parse("serial 77", &record, parsed) && !ae_ce_archive_record_parse("", &record, parsed) &&
			!ae_ce_archive_record_parse("serial x 0101", &record, parsed) &&
			!ae_ce_archive_record_parse("serial 1000000000000000 0101010101010101010101010101010101010101010101010101010101010101",
			&record, parsed));
		CHECK(ae_ce_archive_serial_allows(0, 0, NULL, 1, hash, error, sizeof(error)));
		CHECK(ae_ce_archive_serial_allows(1, 77, hash, 78, other_hash, error, sizeof(error)));
		CHECK(ae_ce_archive_serial_allows(1, 77, hash, 77, hash, error, sizeof(error)));
		CHECK(!ae_ce_archive_serial_allows(1, 77, hash, 77, other_hash, error, sizeof(error)) && strstr(error, "other bytes"));
		CHECK(!ae_ce_archive_serial_allows(1, 77, hash, 76, hash, error, sizeof(error)) && strstr(error, "older"));
		CHECK(!ae_ce_archive_serial_allows(1, 77, hash, 0, hash, error, sizeof(error)));
		/* a record cut short is refused without a read past its end (a heap copy of exactly its length, under the sanitizers) */
		{
			char const *cut[] = { "serial 1 ab", "serial 1 ", "serial 1 0101010101010101010101010101010101010101010101010101010101010", "serial 1" };
			char *copy;
			int each;

			for (each = 0; each < 4; each++)
			{
				copy = (char *)malloc(strlen(cut[each]) + 1);
				memcpy(copy, cut[each], strlen(cut[each]) + 1);
				CHECK(!ae_ce_archive_record_parse(copy, &record, parsed));
				free(copy);
			}
			copy = (char *)malloc(7 + 2 + 64 + 1);
			memcpy(copy, "serial 1 0101010101010101010101010101010101010101010101010101010101010101", 7 + 2 + 64 + 1);
			CHECK(ae_ce_archive_record_parse(copy, &record, parsed) && record == 1);
			free(copy);
		}
		/* a serial is Unix seconds: up to 30 days after the clock is in time, a second more is not */
		CHECK(ae_ce_archive_serial_in_time(1793481173ULL, 1793481173ULL) && ae_ce_archive_serial_in_time(0, 5));
		/* (a clock before 2020 is not set: no serial is in time) */
		CHECK(!ae_ce_archive_serial_in_time(5, 1000) && !ae_ce_archive_serial_in_time(1793481173ULL, 0));
		CHECK(ae_ce_archive_serial_in_time(1700000000ULL + 30 * 86400ULL, 1700000000ULL) && !ae_ce_archive_serial_in_time(1700000001ULL + 30 * 86400ULL, 1700000000ULL));
		CHECK(!ae_ce_archive_serial_in_time(999999999999999ULL, 1793481173ULL));
		/* addresses: the archive's cannot be this machine's or a private one */
		octets[0] = 8; octets[1] = 8; octets[2] = 8; octets[3] = 8;
		CHECK(ae_ce_archive_address_public(octets));
		octets[0] = 185; octets[1] = 199; CHECK(ae_ce_archive_address_public(octets));
		octets[0] = 172; octets[1] = 15; CHECK(ae_ce_archive_address_public(octets));
		octets[1] = 32; CHECK(ae_ce_archive_address_public(octets));
		octets[1] = 16; CHECK(!ae_ce_archive_address_public(octets));
		octets[1] = 31; CHECK(!ae_ce_archive_address_public(octets));
		octets[0] = 127; octets[1] = 0; CHECK(!ae_ce_archive_address_public(octets));
		octets[0] = 10; CHECK(!ae_ce_archive_address_public(octets));
		octets[0] = 192; octets[1] = 168; CHECK(!ae_ce_archive_address_public(octets));
		octets[0] = 169; octets[1] = 254; CHECK(!ae_ce_archive_address_public(octets));
		octets[0] = 0; octets[1] = 0; CHECK(!ae_ce_archive_address_public(octets));
		octets[0] = 100; octets[1] = 64; CHECK(!ae_ce_archive_address_public(octets));
		octets[0] = 224; CHECK(!ae_ce_archive_address_public(octets));
	}

	/* ---------- search, sort, paging */
	{
		struct ae_ce_archive_entry sample[5];
		struct ae_ce_archive_query query;
		int view[5], rows, page, pages, first;

		memset(sample, 0, sizeof(sample));
		strcpy(sample[0].file, "Zeta.map"); sample[0].size = 300;
		strcpy(sample[1].file, "alpha.map"); sample[1].size = 100; sample[1].installed = 1;
		strcpy(sample[2].file, "beta.map"); sample[2].size = 500;
		strcpy(sample[3].file, "gamma_alpha.map"); sample[3].size = 100;
		strcpy(sample[4].file, "delta.map"); sample[4].size = 200; sample[4].installed = 1;
		memset(&query, 0, sizeof(query));
		rows = ae_ce_archive_view_build(sample, 5, &query, view);
		CHECK(rows == 5 && view[0] == 1 && view[1] == 2 && view[2] == 4 && view[3] == 3 && view[4] == 0);
		query.descending = 1;
		rows = ae_ce_archive_view_build(sample, 5, &query, view);
		CHECK(rows == 5 && view[0] == 0 && view[4] == 1);
		query.descending = 0;
		query.sort = AE_CE_ARCHIVE_SORT_SIZE;
		rows = ae_ce_archive_view_build(sample, 5, &query, view);
		CHECK(rows == 5 && view[0] == 1 && view[1] == 3 && view[2] == 4 && view[3] == 0 && view[4] == 2);
		query.sort = AE_CE_ARCHIVE_SORT_NAME;
		strcpy(query.text, "ALPHA");
		rows = ae_ce_archive_view_build(sample, 5, &query, view);
		CHECK(rows == 2 && view[0] == 1 && view[1] == 3);
		query.show = AE_CE_ARCHIVE_SHOW_INSTALLED;
		rows = ae_ce_archive_view_build(sample, 5, &query, view);
		CHECK(rows == 1 && view[0] == 1);
		query.show = AE_CE_ARCHIVE_SHOW_MISSING;
		rows = ae_ce_archive_view_build(sample, 5, &query, view);
		CHECK(rows == 1 && view[0] == 3);
		strcpy(query.text, "nothing");
		CHECK(ae_ce_archive_view_build(sample, 5, &query, view) == 0);
		page = 0;
		first = ae_ce_archive_page_first(45, 20, &page, &pages);
		CHECK(first == 0 && pages == 3);
		page = 2;
		first = ae_ce_archive_page_first(45, 20, &page, &pages);
		CHECK(first == 40 && pages == 3);
		page = 9;
		first = ae_ce_archive_page_first(45, 20, &page, &pages);
		CHECK(first == 40 && page == 2);
		page = -4;
		first = ae_ce_archive_page_first(0, 20, &page, &pages);
		CHECK(first == 0 && pages == 1 && page == 0);
	}

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
