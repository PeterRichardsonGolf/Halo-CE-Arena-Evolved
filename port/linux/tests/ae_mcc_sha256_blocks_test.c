/*
AE_MCC_SHA256_BLOCKS_TEST.C

mcc_sha256.c with ae_mcc_sha256_blocks.c: FIPS 180-4's example digests; and the same bytes hashed whole (whole blocks
through the CPU's SHA instructions when it has them) and a byte at a time (every block through the portable rounds),
at many sizes and in pieces that start on and off a block, give the same digest. Prints whether the instructions ran,
and a megabyte-scale timing of each way.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../game/mcc_sha256.h"
#include "../game/ae_mcc_sha256_blocks.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void hex(unsigned char const digest[32], char out[65])
{
	int index;

	for (index = 0; index < 32; index++)
		sprintf(out + index * 2, "%02x", digest[index]);
}

static void whole(void const *data, int size, unsigned char digest[32])
{
	struct mcc_sha256 context;

	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, data, size);
	mcc_sha256_end(&context, digest);
}

/* (a byte at a time: the block is always filled from the context's own buffer, so the portable rounds do it all) */
static void byte_by_byte(unsigned char const *data, int size, unsigned char digest[32])
{
	struct mcc_sha256 context;
	int index;

	mcc_sha256_begin(&context);
	for (index = 0; index < size; index++)
		mcc_sha256_add(&context, data + index, 1);
	mcc_sha256_end(&context, digest);
}

/* (pieces of sizes that cycle: some start with the context's buffer empty, some part filled) */
static void in_pieces(unsigned char const *data, int size, unsigned char digest[32])
{
	static int const sizes[] = {1, 63, 64, 65, 128, 7, 4096, 191, 16384};
	struct mcc_sha256 context;
	int offset = 0, piece = 0;

	mcc_sha256_begin(&context);
	while (offset < size)
	{
		int count = sizes[piece++ % (int)(sizeof(sizes) / sizeof(sizes[0]))];

		if (count > size - offset)
			count = size - offset;
		mcc_sha256_add(&context, data + offset, count);
		offset += count;
	}
	mcc_sha256_end(&context, digest);
}

static void known_digests(void)
{
	static unsigned char million[1000000];
	unsigned char digest[32];
	char text[65];

	whole("", 0, digest);
	hex(digest, text);
	CHECK(!strcmp(text, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
	whole("abc", 3, digest);
	hex(digest, text);
	CHECK(!strcmp(text, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
	whole("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, digest);
	hex(digest, text);
	CHECK(!strcmp(text, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
	memset(million, 'a', sizeof(million));
	whole(million, (int)sizeof(million), digest);
	hex(digest, text);
	CHECK(!strcmp(text, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

static void both_ways_agree(void)
{
	static unsigned char data[(1 << 20) + 64];
	static int const sizes[] = {0, 1, 55, 56, 63, 64, 65, 119, 127, 128, 129, 1000, 4096, 65536, 69999, 1 << 20,
		(1 << 20) + 64};
	unsigned int seed = 12345;
	int index;

	for (index = 0; index < (int)sizeof(data); index++)
	{
		seed = seed * 1103515245u + 12345u;
		data[index] = (unsigned char)(seed >> 16);
	}
	for (index = 0; index < (int)(sizeof(sizes) / sizeof(sizes[0])); index++)
	{
		unsigned char a[32], b[32], c[32], d[32];

		whole(data, sizes[index], a);
		byte_by_byte(data, sizes[index], b);
		in_pieces(data, sizes[index], c);
		/* (a start off the buffer's alignment: the instructions' loads are unaligned ones) */
		whole(data + 3, sizes[index] > 3 ? sizes[index] - 3 : 0, d);
		CHECK(!memcmp(a, b, 32));
		CHECK(!memcmp(a, c, 32));
		byte_by_byte(data + 3, sizes[index] > 3 ? sizes[index] - 3 : 0, b);
		CHECK(!memcmp(d, b, 32));
	}
	/* (as the fingerprint goes: a 22-byte prefix and a 9-byte file header first, then 16 KB pieces; the part block is
	filled, then the CPU's instructions take the whole ones) */
	{
		struct mcc_sha256 context;
		unsigned char a[32], b[32];
		unsigned long before = ae_mcc_sha256_hardware_blocks();
		int offset;

		mcc_sha256_begin(&context);
		mcc_sha256_add(&context, "GulchCE MCC content v1", 22);
		mcc_sha256_add(&context, data, 9);
		for (offset = 0; offset < (1 << 20); offset += 16384)
			mcc_sha256_add(&context, data + offset, 16384);
		mcc_sha256_end(&context, a);
		{
			static unsigned char joined[22 + 9 + (1 << 20)];

			memcpy(joined, "GulchCE MCC content v1", 22);
			memcpy(joined + 22, data, 9);
			memcpy(joined + 31, data, 1 << 20);
			byte_by_byte(joined, (int)sizeof(joined), b);
		}
		CHECK(!memcmp(a, b, 32));
		if (ae_mcc_sha256_hardware())
			/* (each piece: one block filled from the part block, the other 255 whole) */
			CHECK(ae_mcc_sha256_hardware_blocks() - before >= (unsigned long)((1 << 20) / 16384 * 255));
	}
	/* (nothing done for no blocks, whatever the CPU) */
	{
		uint32_t state[8] = {1, 2, 3, 4, 5, 6, 7, 8};

		CHECK(ae_mcc_sha256_blocks(state, data, 0) == 0);
		CHECK(state[0] == 1 && state[7] == 8);
		CHECK(ae_mcc_sha256_blocks(state, data, 1) == (ae_mcc_sha256_hardware() ? 1 : 0));
	}
}

static double seconds(void)
{
	return (double)clock() / CLOCKS_PER_SEC;
}

static void measured(void)
{
	static unsigned char data[8 << 20];
	struct mcc_sha256 context;
	unsigned char digest[32];
	double began, hardware, portable;
	int offset;

	memset(data, 0x5a, sizeof(data));
	began = seconds();
	whole(data, (int)sizeof(data), digest);
	hardware = seconds() - began;
	/* (the portable rounds over the same bytes: 63 bytes first, so no piece starts on an empty buffer) */
	began = seconds();
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, data, 63);
	for (offset = 63; offset < (int)sizeof(data); offset += 64)
		mcc_sha256_add(&context, data + offset, (int)sizeof(data) - offset < 64 ? (int)sizeof(data) - offset : 64);
	mcc_sha256_end(&context, digest);
	portable = seconds() - began;
	printf("measured: 8 MB whole %.1f ms (%s), portable rounds %.1f ms\n", hardware * 1000,
		ae_mcc_sha256_hardware() ? "SHA instructions" : "no SHA instructions", portable * 1000);
}

int main(void)
{
	known_digests();
	both_ways_agree();
	measured();
	if (failures)
		return 1;
	printf("ae_mcc_sha256_blocks: all checks passed (%s)\n",
		ae_mcc_sha256_hardware() ? "SHA instructions used" : "this CPU has no SHA instructions: portable only");
	return 0;
}
