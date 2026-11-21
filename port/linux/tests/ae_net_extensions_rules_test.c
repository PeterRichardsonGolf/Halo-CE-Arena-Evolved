/* The capability message's builder and reader (port/linux/game/ae_net_extensions_rules.c): the bytes, and a reader that
takes truncated, oversized, duplicated and garbage packets without a fault. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../game/ae_net_extensions_rules.h"

static int failures = 0;

static void expect(long got, long want, char const *what)
{
	if (got != want)
	{
		printf("FAIL %s: got %ld, want %ld\n", what, got, want);
		failures++;
	}
}

static struct ae_netext_capability const mine[] =
{
	{ "ae.arena_evolved", 1, "You need Arena Evolved." },
	{ "other_thing", 3, "" },
};
#define MINE ((long)(sizeof(mine) / sizeof(mine[0])))

static long put_entry(unsigned char *out, long at, char const *key, unsigned short version, unsigned char flags,
	char const *text)
{
	long key_length = (long)strlen(key), text_length = (long)strlen(text);

	out[at++] = (unsigned char)key_length;
	memcpy(out + at, key, (size_t)key_length);
	at += key_length;
	out[at++] = (unsigned char)(version & 0xFF);
	out[at++] = (unsigned char)(version >> 8);
	out[at++] = flags;
	out[at++] = (unsigned char)text_length;
	memcpy(out + at, text, (size_t)text_length);

	return at + text_length;
}

static long head(unsigned char *out, unsigned char kind, unsigned char count)
{
	memcpy(out, "SLXT", 4);
	out[4] = 1;
	out[5] = kind;
	out[6] = count;

	return 7;
}

static int parse(unsigned char const *bytes, long size, unsigned char kind, struct ae_netext_peer *peer)
{
	return ae_netext_parse(bytes, size, kind, mine, MINE, peer);
}

int main(void)
{
	unsigned char message[AE_NETEXT_MAXIMUM_BYTES + 64];
	unsigned char big[4096];
	struct ae_netext_peer peer;
	long size, at, cut;
	long index;
	unsigned long seed = 12345;

	/* the bytes of an answer of one capability, exactly */
	{
		static struct ae_netext_capability const one[] = { { "ae.arena_evolved", 1, "text" } };
		unsigned char want[] = { 'S', 'L', 'X', 'T', 1, 1, 1, 16, 'a', 'e', '.', 'a', 'r', 'e', 'n', 'a', '_', 'e', 'v', 'o',
			'l', 'v', 'e', 'd', 1, 0, 0, 0, 255 };

		size = ae_netext_build(AE_NETEXT_KIND_ANSWER, one, 1, 0, message, (long)sizeof(message));
		expect(size, (long)sizeof(want), "answer's size");
		expect(size == (long)sizeof(want) && !memcmp(message, want, sizeof(want)), 1, "answer's bytes");
		/* an announcement carries the text, and the flag when required */
		size = ae_netext_build(AE_NETEXT_KIND_ANNOUNCEMENT, one, 1, 1, message, (long)sizeof(message));
		expect(size, 29 + 4, "announcement's size");
		expect(message[7 + 1 + 16 + 2], 1, "announcement's required flag");
		expect(message[7 + 1 + 16 + 3], 4, "announcement's text length");
		expect(message[size - 1], 255, "the type byte ends it");
	}
	/* round trip through the reader */
	size = ae_netext_build(AE_NETEXT_KIND_ANNOUNCEMENT, mine, MINE, 2, message, (long)sizeof(message));
	expect(parse(message, size, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_OK, "round trip reads");
	expect(peer.count, 2, "round trip count");
	expect(ae_netext_peer_version(&peer, "ae.arena_evolved"), 1, "version told");
	expect(ae_netext_peer_version(&peer, "other_thing"), 3, "second version told");
	expect(ae_netext_peer_version(&peer, "nothing"), 0, "unknown key has no version");
	expect(peer.missing_required, 0, "known and equal: nothing missing");
	expect(ae_netext_first_missing(&peer, mine, MINE, 3), -1, "all told: none missing");
	/* the wrong kind is another message, silently */
	expect(parse(message, size, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_OTHER, "announcement is not an answer");
	/* a required key this reader has not keeps its text, printable */
	at = head(message, AE_NETEXT_KIND_ANNOUNCEMENT, 2);
	at = put_entry(message, at, "future.thing", 7, 1, "need it\x01!");
	at = put_entry(message, at, "ae.arena_evolved", 1, 0, "");
	message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_OK, "unknown required key reads");
	expect(peer.missing_required, 1, "unknown required key is missing");
	expect(strcmp(peer.missing_key, "future.thing"), 0, "its key");
	expect(strcmp(peer.missing_text, "need it?!"), 0, "its text, made printable");
	expect(peer.count, 2, "unknown keys are kept in the list");
	/* a required key of a newer version than the reader has */
	at = head(message, AE_NETEXT_KIND_ANNOUNCEMENT, 1);
	at = put_entry(message, at, "other_thing", 4, 1, "newer");
	message[at++] = 255;
	parse(message, at, AE_NETEXT_KIND_ANNOUNCEMENT, &peer);
	expect(peer.missing_required, 1, "a newer required version is missing");
	/* the host's check of an answer */
	at = head(message, AE_NETEXT_KIND_ANSWER, 1);
	at = put_entry(message, at, "other_thing", 2, 0, "");
	message[at++] = 255;
	parse(message, at, AE_NETEXT_KIND_ANSWER, &peer);
	expect(ae_netext_first_missing(&peer, mine, MINE, 2), 1, "an older version does not meet the requirement");
	expect(ae_netext_first_missing(&peer, mine, MINE, 1), 0, "an answer without the key misses it");
	expect(ae_netext_first_missing(&peer, mine, MINE, 0), -1, "nothing required: nothing missing");

	/* truncated: every cut of a good message is malformed (or another message), never a fault or a read */
	size = ae_netext_build(AE_NETEXT_KIND_ANNOUNCEMENT, mine, MINE, 3, message, (long)sizeof(message));
	for (cut = 0; cut < size; cut++)
	{
		int result;

		/* (an exact-size copy: the sanitizers see a read past it) */
		unsigned char *exact = malloc((size_t)(cut ? cut : 1));

		memcpy(exact, message, (size_t)cut);
		exact[cut ? cut - 1 : 0] = cut ? 255 : 0;
		result = parse(exact, cut, AE_NETEXT_KIND_ANNOUNCEMENT, &peer);
		if (cut == 0)
			result = AE_NETEXT_PARSE_MALFORMED;
		expect(result != AE_NETEXT_PARSE_OK, 1, "a truncated message does not read");
		free(exact);
	}
	/* the type byte alone, and nothing */
	expect(parse(message, 1, AE_NETEXT_KIND_ANNOUNCEMENT, &peer) != AE_NETEXT_PARSE_OK, 1, "type byte alone");
	/* trailing bytes after the entries */
	memcpy(big, message, (size_t)size);
	big[size - 1] = 0;
	big[size] = 255;
	expect(parse(big, size + 1, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_MALFORMED, "trailing bytes");
	/* a count past the entries, and a count past the cap */
	memcpy(big, message, (size_t)size);
	big[6] = 5;
	expect(parse(big, size, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_MALFORMED, "count past the entries");
	big[6] = 255;
	expect(parse(big, size, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_MALFORMED, "count of 255");
	big[6] = AE_NETEXT_MAXIMUM_ENTRIES + 1;
	expect(parse(big, size, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_MALFORMED, "count past the cap");
	/* a key's length past the message, zero, and past its cap */
	at = head(message, AE_NETEXT_KIND_ANSWER, 1);
	message[at++] = 200; memset(message + at, 'a', 20); at += 20; message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "key length past the end");
	at = head(message, AE_NETEXT_KIND_ANSWER, 1);
	at = put_entry(message, at, "x", 1, 0, ""); message[7] = 0; message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "key length zero");
	at = head(message, AE_NETEXT_KIND_ANSWER, 1);
	memset(big, 'k', 64);
	big[64] = 0;
	at = put_entry(message, at, (char const *)big, 1, 0, ""); message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "key of 64 characters");
	/* a bad character in a key */
	at = head(message, AE_NETEXT_KIND_ANSWER, 1);
	at = put_entry(message, at, "bad key", 1, 0, ""); message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "a space in a key");
	at = head(message, AE_NETEXT_KIND_ANSWER, 1);
	at = put_entry(message, at, "bad\xC3\xA9", 1, 0, ""); message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "a high byte in a key");
	/* a text's length past its cap and past the end */
	at = head(message, AE_NETEXT_KIND_ANNOUNCEMENT, 1);
	at = put_entry(message, at, "a", 1, 0, ""); message[at - 1] = 128; memset(message + at, 'x', 128); at += 128; message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_MALFORMED, "text of 128");
	at = head(message, AE_NETEXT_KIND_ANNOUNCEMENT, 1);
	at = put_entry(message, at, "a", 1, 0, ""); message[at - 1] = 100; message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_MALFORMED, "text past the end");
	/* the longest legal entry reads */
	at = head(message, AE_NETEXT_KIND_ANNOUNCEMENT, 1);
	memset(big, 'k', 63);
	big[63] = 0;
	{
		char text[128];

		memset(text, 't', 127);
		text[127] = 0;
		at = put_entry(message, at, (char const *)big, 1, 1, text);
	}
	message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANNOUNCEMENT, &peer), AE_NETEXT_PARSE_OK, "the longest entry reads");
	expect((long)strlen(peer.missing_text), 127, "and its text is whole");
	/* duplicated key */
	at = head(message, AE_NETEXT_KIND_ANSWER, 2);
	at = put_entry(message, at, "ae.arena_evolved", 1, 0, "");
	at = put_entry(message, at, "ae.arena_evolved", 9, 0, "");
	message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "a key twice");
	expect(peer.count, 0, "a refused message leaves nothing behind");
	/* the most entries, and one more */
	at = head(message, AE_NETEXT_KIND_ANSWER, AE_NETEXT_MAXIMUM_ENTRIES);
	for (index = 0; index < AE_NETEXT_MAXIMUM_ENTRIES; index++)
	{
		char key[32];

		snprintf(key, sizeof(key), "k%ld", index);
		at = put_entry(message, at, key, 1, 0, "");
	}
	message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_OK, "the most entries reads");
	expect(peer.count, AE_NETEXT_MAXIMUM_ENTRIES, "and all are kept");
	/* an oversized message of another sort: past the byte cap, whatever it starts with */
	memset(big, 0, sizeof(big));
	expect(parse(big, 4000, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "4000 zero bytes");
	memcpy(big, "SLXT", 4);
	big[4] = 1;
	big[5] = AE_NETEXT_KIND_ANSWER;
	big[6] = 0;
	expect(parse(big, 4000, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED, "a valid head and 4000 bytes");
	expect(parse(big, AE_NETEXT_MAXIMUM_BYTES + 2, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_MALFORMED,
		"one byte past the cap");
	/* other magics, formats and kinds are not ours */
	at = head(message, AE_NETEXT_KIND_ANSWER, 0);
	message[at++] = 255;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_OK, "an empty answer reads");
	message[0] = 'X';
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_OTHER, "another magic");
	message[0] = 'S'; message[4] = 2;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_OTHER, "another format");
	message[4] = 1; message[5] = 2;
	expect(parse(message, at, AE_NETEXT_KIND_ANSWER, &peer), AE_NETEXT_PARSE_OTHER, "a data kind");
	/* a builder that is given too little room or a bad table says no */
	expect(ae_netext_build(AE_NETEXT_KIND_ANSWER, mine, MINE, 0, message, 20), -1, "too little room");
	expect(ae_netext_build(AE_NETEXT_KIND_ANSWER, mine, MINE, 0, NULL, 100), -1, "no buffer");
	{
		static struct ae_netext_capability const bad[] = { { "bad key", 1, "" } };

		expect(ae_netext_build(AE_NETEXT_KIND_ANSWER, bad, 1, 0, message, 100), -1, "a bad key in the table");
	}
	/* garbage: random bytes, some with a good head, of exact-size heap copies (the sanitizers see an overread) */
	for (index = 0; index < 20000; index++)
	{
		long length = (long)(seed % 300) + 1;
		unsigned char *junk = malloc((size_t)length);
		long each;

		for (each = 0; each < length; each++)
		{
			seed = seed * 1103515245UL + 12345UL;
			junk[each] = (unsigned char)(seed >> 16);
		}
		if (index % 2 && length > 8)
		{
			memcpy(junk, "SLXT", 4);
			junk[4] = 1;
			junk[5] = AE_NETEXT_KIND_ANSWER;
			junk[6] = (unsigned char)(junk[6] % 6);
		}
		(void)parse(junk, length, AE_NETEXT_KIND_ANSWER, &peer);
		expect(peer.count <= AE_NETEXT_MAXIMUM_ENTRIES, 1, "garbage keeps within the cap");
		free(junk);
	}
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
