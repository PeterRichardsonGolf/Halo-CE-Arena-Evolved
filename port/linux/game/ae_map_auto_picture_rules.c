/*
AE_MAP_AUTO_PICTURE_RULES.C

The automatic map picture's rules (ae_map_auto_picture_rules.h).
*/

#include <stdlib.h>
#include <string.h>

#include "ae_map_auto_picture_rules.h"

int ae_map_auto_picture_wanted(char const *map_name, int candidate, int stock_art, int local)
{
	return map_name && map_name[0] && candidate && !stock_art && local && !strstr(map_name, "@ws");
}

int ae_map_auto_picture_stock_frame(int frame_index)
{
	return frame_index >= AE_MAP_AUTO_PICTURE_STOCK_BASE && frame_index < AE_MAP_AUTO_PICTURE_STOCK_END &&
		frame_index != AE_MAP_AUTO_PICTURE_STOCK_BASE + AE_MAP_AUTO_PICTURE_UNKNOWN_LEVEL;
}

int ae_map_auto_picture_shot_due(long ticks_alive)
{
	return ticks_alive >= AE_MAP_AUTO_PICTURE_SHOT_TICKS && ticks_alive <= AE_MAP_AUTO_PICTURE_LAST_TICKS;
}

int ae_map_auto_picture_crop(int width, int height, int *x, int *y, int *crop_width, int *crop_height)
{
	long wide;

	if (width < AE_MAP_AUTO_PICTURE_WIDTH / 8 || height < AE_MAP_AUTO_PICTURE_HEIGHT / 8 || width > 16384 || height > 16384)
		return 0;
	/* (wider than 16:9: columns cut off both sides; taller: rows cut off) */
	wide = (long)width * AE_MAP_AUTO_PICTURE_HEIGHT;
	if (wide > (long)height * AE_MAP_AUTO_PICTURE_WIDTH)
	{
		*crop_height = height;
		*crop_width = (int)((long)height * AE_MAP_AUTO_PICTURE_WIDTH / AE_MAP_AUTO_PICTURE_HEIGHT);
	}
	else
	{
		*crop_width = width;
		*crop_height = (int)((long)width * AE_MAP_AUTO_PICTURE_HEIGHT / AE_MAP_AUTO_PICTURE_WIDTH);
	}
	*x = (width - *crop_width) / 2;
	*y = (height - *crop_height) / 2;
	return *crop_width > 0 && *crop_height > 0;
}

int ae_map_auto_picture_scale(unsigned char const *bgra, int width, int height, unsigned char *rgba)
{
	int x0, y0, crop_width, crop_height, ox, oy;

	if (!bgra || !rgba || !ae_map_auto_picture_crop(width, height, &x0, &y0, &crop_width, &crop_height))
		return 0;
	for (oy = 0; oy < AE_MAP_AUTO_PICTURE_HEIGHT; oy++)
	{
		int sy0 = y0 + (int)((long)oy * crop_height / AE_MAP_AUTO_PICTURE_HEIGHT);
		int sy1 = y0 + (int)((long)(oy + 1) * crop_height / AE_MAP_AUTO_PICTURE_HEIGHT);

		if (sy1 <= sy0)
			sy1 = sy0 + 1;
		for (ox = 0; ox < AE_MAP_AUTO_PICTURE_WIDTH; ox++)
		{
			int sx0 = x0 + (int)((long)ox * crop_width / AE_MAP_AUTO_PICTURE_WIDTH);
			int sx1 = x0 + (int)((long)(ox + 1) * crop_width / AE_MAP_AUTO_PICTURE_WIDTH);
			unsigned long sum[3] = { 0, 0, 0 }, count;
			int sx, sy;
			unsigned char *out = rgba + ((long)oy * AE_MAP_AUTO_PICTURE_WIDTH + ox) * 4;

			if (sx1 <= sx0)
				sx1 = sx0 + 1;
			for (sy = sy0; sy < sy1; sy++)
			{
				for (sx = sx0; sx < sx1; sx++)
				{
					unsigned char const *in = bgra + ((long)sy * width + sx) * 4;

					sum[0] += in[2];
					sum[1] += in[1];
					sum[2] += in[0];
				}
			}
			count = (unsigned long)(sy1 - sy0) * (unsigned long)(sx1 - sx0);
			out[0] = (unsigned char)((sum[0] + count / 2) / count);
			out[1] = (unsigned char)((sum[1] + count / 2) / count);
			out[2] = (unsigned char)((sum[2] + count / 2) / count);
			out[3] = 0xff;
		}
	}
	return 1;
}

/* ---------- the PNG */

static unsigned long crc_table[256];
static int crc_ready;

static unsigned long crc_update(unsigned long crc, unsigned char const *data, unsigned long size)
{
	unsigned long index;

	if (!crc_ready)
	{
		unsigned long n, k;

		for (n = 0; n < 256; n++)
		{
			unsigned long c = n;

			for (k = 0; k < 8; k++)
				c = (c & 1) ? 0xedb88320ul ^ (c >> 1) : c >> 1;
			crc_table[n] = c;
		}
		crc_ready = 1;
	}
	for (index = 0; index < size; index++)
		crc = crc_table[(crc ^ data[index]) & 0xff] ^ (crc >> 8);
	return crc;
}

static void put32(unsigned char *out, unsigned long value)
{
	out[0] = (unsigned char)(value >> 24);
	out[1] = (unsigned char)(value >> 16);
	out[2] = (unsigned char)(value >> 8);
	out[3] = (unsigned char)value;
}

/* a chunk's bytes from out + at: length, type, data (already in place at +8), crc; its size */
static unsigned int finish_chunk(unsigned char *chunk, char const *type, unsigned long data_size)
{
	put32(chunk, data_size);
	memcpy(chunk + 4, type, 4);
	put32(chunk + 8 + data_size, crc_update(0xfffffffful, chunk + 4, 4 + data_size) ^ 0xfffffffful);
	return (unsigned int)(12 + data_size);
}

/* the zlib stream (RFC 1950) of the filtered rows (RFC 2083 filters): one dynamic Huffman block (RFC 1951) over hash chain
matches. Deterministic: no clock, no pointers' values, ties broken by the lowest symbol, so the same pixels give the
same bytes on every platform. If a block would not be smaller than stored blocks (noise), stored blocks are written,
which is also the bound ae_map_auto_picture_png_size gives. */
static unsigned long raw_size(int width, int height)
{
	return (unsigned long)height * (1 + (unsigned long)width * 4);
}

static unsigned long stream_size(int width, int height)
{
	unsigned long raw = raw_size(width, height);

	return 2 + raw + 5 * ((raw + 65534) / 65535) + 4;
}

unsigned int ae_map_auto_picture_png_size(int width, int height)
{
	if (width < 1 || height < 1 || width > 2048 || height > 2048)
		return 0;
	return (unsigned int)(8 + 25 + (12 + stream_size(width, height)) + 12);
}

/* ---- the filters: each row takes the filter (0 none, 1 sub, 2 up, 3 average, 4 Paeth) of the smallest sum of
residuals as signed bytes, the lowest filter on a tie */

static int paeth(int a, int b, int c)
{
	int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;

	return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

static unsigned char filter_byte(int type, unsigned char const *row, unsigned char const *above, unsigned long i)
{
	int a = i >= 4 ? row[i - 4] : 0, b = above ? above[i] : 0, c = (above && i >= 4) ? above[i - 4] : 0, x = row[i], p;

	switch (type)
	{
		case 1: p = a; break;
		case 2: p = b; break;
		case 3: p = (a + b) / 2; break;
		case 4: p = paeth(a, b, c); break;
		default: p = 0; break;
	}
	return (unsigned char)(x - p);
}

static void filter_rows(unsigned char const *rgba, int width, int height, unsigned char *raw)
{
	unsigned long stride = (unsigned long)width * 4, i;
	int row, type;

	for (row = 0; row < height; row++)
	{
		unsigned char const *line = rgba + (unsigned long)row * stride, *above = row ? line - stride : NULL;
		unsigned char *out = raw + (unsigned long)row * (stride + 1);
		unsigned long best_sum = 0;
		int best = 0;

		for (type = 0; type < 5; type++)
		{
			unsigned long sum = 0;

			for (i = 0; i < stride; i++)
			{
				int v = (signed char)filter_byte(type, line, above, i);

				sum += (unsigned long)(v < 0 ? -v : v);
			}
			if (type == 0 || sum < best_sum)
			{
				best_sum = sum;
				best = type;
			}
		}
		out[0] = (unsigned char)best;
		for (i = 0; i < stride; i++)
			out[1 + i] = filter_byte(best, line, above, i);
	}
}

/* ---- the bits (low bit first); a full buffer sets over and writes nothing more */

struct bits
{
	unsigned char *out;
	unsigned long size, at;
	unsigned long buffer;
	int count, over;
};

static void put_bits(struct bits *bits, unsigned long value, int count)
{
	bits->buffer |= value << bits->count;
	bits->count += count;
	while (bits->count >= 8)
	{
		if (bits->at < bits->size)
			bits->out[bits->at++] = (unsigned char)bits->buffer;
		else
			bits->over = 1;
		bits->buffer >>= 8;
		bits->count -= 8;
	}
}

/* a Huffman code goes out most significant bit first */
static void put_code(struct bits *bits, unsigned int code, int length)
{
	unsigned int reversed = 0;
	int n;

	for (n = 0; n < length; n++)
		reversed |= ((code >> n) & 1u) << (length - 1 - n);
	put_bits(bits, reversed, length);
}

/* ---- the codes */

enum
{
	LITERALS = 286,
	DISTANCES = 30,
	MAXIMUM_BITS = 15,
	WINDOW = 32768,
	HASH_SIZE = 1 << 15,
	CHAIN = 24
};

static unsigned short const length_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
	67, 83, 99, 115, 131, 163, 195, 227, 258 };
static unsigned char const length_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
	5, 5, 5, 5, 0 };
static unsigned short const distance_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
	513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static unsigned char const distance_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
	10, 11, 11, 12, 12, 13, 13 };

static int length_symbol(int length)
{
	int n = 28;

	while (length_base[n] > length)
		n--;
	return n;
}

static int distance_symbol(int distance)
{
	int n = 29;

	while (distance_base[n] > distance)
		n--;
	return n;
}

/* code lengths (at most limit) of a count's symbols from their frequencies: the Huffman tree by two lowest weights
(the lowest index first on a tie), halving the weights again until the depth fits. At least two symbols get a code. */
static void code_lengths(unsigned long const *frequency, int symbols, int limit, unsigned char *lengths)
{
	unsigned long weight[2 * LITERALS], scaled[LITERALS];
	int parent[2 * LITERALS], alive[2 * LITERALS];
	int n, nodes, used, again;

	for (n = 0; n < symbols; n++)
		scaled[n] = frequency[n];
	for (used = 0, n = 0; n < symbols; n++)
		used += scaled[n] != 0;
	for (n = 0; used < 2 && n < symbols; n++)
		if (!scaled[n])
		{
			scaled[n] = 1;
			used++;
		}
	do
	{
		int depth_over = 0;

		for (n = 0; n < symbols; n++)
		{
			weight[n] = scaled[n];
			alive[n] = scaled[n] != 0;
			parent[n] = -1;
			lengths[n] = 0;
		}
		nodes = symbols;
		for (;;)
		{
			int first = -1, second = -1;

			for (n = 0; n < nodes; n++)
			{
				if (!alive[n])
					continue;
				if (first < 0 || weight[n] < weight[first])
				{
					second = first;
					first = n;
				}
				else if (second < 0 || weight[n] < weight[second])
					second = n;
			}
			if (second < 0)
				break;
			weight[nodes] = weight[first] + weight[second];
			alive[nodes] = 1;
			parent[nodes] = -1;
			alive[first] = alive[second] = 0;
			parent[first] = parent[second] = nodes;
			nodes++;
		}
		for (n = 0; n < symbols; n++)
		{
			int depth = 0, at = n;

			if (!scaled[n])
				continue;
			while (parent[at] >= 0)
			{
				at = parent[at];
				depth++;
			}
			lengths[n] = (unsigned char)depth;
			if (depth > limit)
				depth_over = 1;
		}
		again = depth_over;
		if (again)
			for (n = 0; n < symbols; n++)
				if (scaled[n])
					scaled[n] = (scaled[n] + 1) / 2;
	}
	while (again);
}

/* the canonical codes (RFC 1951 3.2.2) of the lengths */
static void canonical_codes(unsigned char const *lengths, int symbols, unsigned int *codes)
{
	unsigned int count[MAXIMUM_BITS + 1] = { 0 }, next[MAXIMUM_BITS + 2] = { 0 }, code = 0;
	int n, bits;

	for (n = 0; n < symbols; n++)
		count[lengths[n]]++;
	count[0] = 0;
	for (bits = 1; bits <= MAXIMUM_BITS; bits++)
	{
		code = (code + count[bits - 1]) << 1;
		next[bits] = code;
	}
	for (n = 0; n < symbols; n++)
		codes[n] = lengths[n] ? next[lengths[n]]++ : 0;
}

/* the tokens: a literal is its byte, a match is 0x80000000 | length << 16 | distance */
static unsigned long find_tokens(unsigned char const *data, unsigned long size, unsigned long *token)
{
	int *head = malloc(sizeof(int) * HASH_SIZE), *previous = malloc(sizeof(int) * (size ? size : 1));
	unsigned long at = 0, count = 0;

	if (!head || !previous)
	{
		free(head);
		free(previous);
		return 0;
	}
	memset(head, 0xff, sizeof(int) * HASH_SIZE);
	while (at < size)
	{
		unsigned long best = 0, best_distance = 0;

		if (at + 3 <= size)
		{
			unsigned int h = (((unsigned int)data[at] << 10) ^ ((unsigned int)data[at + 1] << 5) ^ data[at + 2]) &
				(HASH_SIZE - 1);
			int candidate = head[h], tries = CHAIN;
			unsigned long limit = size - at > 258 ? 258 : size - at;

			while (candidate >= 0 && tries-- && at - (unsigned long)candidate <= WINDOW)
			{
				unsigned long length = 0;

				while (length < limit && data[(unsigned long)candidate + length] == data[at + length])
					length++;
				if (length > best)
				{
					best = length;
					best_distance = at - (unsigned long)candidate;
					if (length == limit)
						break;
				}
				candidate = previous[candidate];
			}
			previous[at] = head[h];
			head[h] = (int)at;
		}
		if (best >= 4)
		{
			unsigned long k;

			token[count++] = 0x80000000ul | (best << 16) | best_distance;
			for (k = 1; k < best; k++)
				if (at + k + 3 <= size)
				{
					unsigned long p = at + k;
					unsigned int h = (((unsigned int)data[p] << 10) ^ ((unsigned int)data[p + 1] << 5) ^ data[p + 2]) &
						(HASH_SIZE - 1);

					previous[p] = head[h];
					head[h] = (int)p;
				}
			at += best;
		}
		else
			token[count++] = data[at++];
	}
	free(head);
	free(previous);
	return count;
}

/* the dynamic block's bits for the tokens; the bytes written, or 0 if the buffer was too small */
static unsigned long write_dynamic(struct bits *bits, unsigned long const *token, unsigned long count)
{
	static unsigned char const order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
	unsigned long literal_frequency[LITERALS] = { 0 }, distance_frequency[DISTANCES] = { 0 }, clen_frequency[19] = { 0 }, t;
	unsigned char literal_lengths[LITERALS], distance_lengths[DISTANCES], clen_lengths[19], all[LITERALS + DISTANCES];
	unsigned char clen_symbol[LITERALS + DISTANCES], clen_value[LITERALS + DISTANCES];
	unsigned int literal_codes[LITERALS], distance_codes[DISTANCES], clen_codes[19];
	int literal_count, distance_count, total, n, clen_count, hclen;

	for (t = 0; t < count; t++)
	{
		if (token[t] & 0x80000000ul)
		{
			literal_frequency[257 + length_symbol((int)((token[t] >> 16) & 0x1ff))]++;
			distance_frequency[distance_symbol((int)(token[t] & 0xffff))]++;
		}
		else
			literal_frequency[token[t]]++;
	}
	literal_frequency[256] = 1;
	code_lengths(literal_frequency, LITERALS, MAXIMUM_BITS, literal_lengths);
	code_lengths(distance_frequency, DISTANCES, MAXIMUM_BITS, distance_lengths);
	canonical_codes(literal_lengths, LITERALS, literal_codes);
	canonical_codes(distance_lengths, DISTANCES, distance_codes);
	for (literal_count = LITERALS; literal_count > 257 && !literal_lengths[literal_count - 1]; literal_count--)
		;
	for (distance_count = DISTANCES; distance_count > 1 && !distance_lengths[distance_count - 1]; distance_count--)
		;
	total = literal_count + distance_count;
	memcpy(all, literal_lengths, (size_t)literal_count);
	memcpy(all + literal_count, distance_lengths, (size_t)distance_count);
	/* the lengths, run length coded with 16 (repeat the last 3 to 6), 17 (3 to 10 zeros), 18 (11 to 138 zeros) */
	clen_count = 0;
	for (n = 0; n < total;)
	{
		int run = 1;

		while (n + run < total && all[n + run] == all[n])
			run++;
		if (all[n] == 0 && run >= 3)
		{
			int use = run > 138 ? 138 : run;

			clen_symbol[clen_count] = use >= 11 ? 18 : 17;
			clen_value[clen_count++] = (unsigned char)(use >= 11 ? use - 11 : use - 3);
			n += use;
		}
		else if (all[n] != 0 && run >= 4)
		{
			int use = run - 1 > 6 ? 6 : run - 1;

			clen_symbol[clen_count] = all[n];
			clen_value[clen_count++] = 0;
			clen_symbol[clen_count] = 16;
			clen_value[clen_count++] = (unsigned char)(use - 3);
			n += 1 + use;
		}
		else
		{
			clen_symbol[clen_count] = all[n];
			clen_value[clen_count++] = 0;
			n++;
		}
	}
	for (n = 0; n < clen_count; n++)
		clen_frequency[clen_symbol[n]]++;
	code_lengths(clen_frequency, 19, 7, clen_lengths);
	canonical_codes(clen_lengths, 19, clen_codes);
	for (hclen = 19; hclen > 4 && !clen_lengths[order[hclen - 1]]; hclen--)
		;
	put_bits(bits, 1, 1);  /* the last block */
	put_bits(bits, 2, 2);  /* dynamic codes */
	put_bits(bits, (unsigned long)(literal_count - 257), 5);
	put_bits(bits, (unsigned long)(distance_count - 1), 5);
	put_bits(bits, (unsigned long)(hclen - 4), 4);
	for (n = 0; n < hclen; n++)
		put_bits(bits, clen_lengths[order[n]], 3);
	for (n = 0; n < clen_count; n++)
	{
		put_code(bits, clen_codes[clen_symbol[n]], clen_lengths[clen_symbol[n]]);
		if (clen_symbol[n] == 16)
			put_bits(bits, clen_value[n], 2);
		else if (clen_symbol[n] == 17)
			put_bits(bits, clen_value[n], 3);
		else if (clen_symbol[n] == 18)
			put_bits(bits, clen_value[n], 7);
	}
	for (t = 0; t < count; t++)
	{
		if (token[t] & 0x80000000ul)
		{
			int length = (int)((token[t] >> 16) & 0x1ff), distance = (int)(token[t] & 0xffff);
			int ls = length_symbol(length), ds = distance_symbol(distance);

			put_code(bits, literal_codes[257 + ls], literal_lengths[257 + ls]);
			put_bits(bits, (unsigned long)(length - length_base[ls]), length_extra[ls]);
			put_code(bits, distance_codes[ds], distance_lengths[ds]);
			put_bits(bits, (unsigned long)(distance - distance_base[ds]), distance_extra[ds]);
		}
		else
			put_code(bits, literal_codes[token[t]], literal_lengths[token[t]]);
	}
	put_code(bits, literal_codes[256], literal_lengths[256]);
	if (bits->count)
		put_bits(bits, 0, 8 - bits->count);
	return bits->over ? 0 : bits->at;
}

static unsigned long adler32_of(unsigned char const *data, unsigned long size)
{
	unsigned long a = 1, b = 0, i;

	for (i = 0; i < size; i++)
	{
		a = (a + data[i]) % 65521ul;
		b = (b + a) % 65521ul;
	}
	return (b << 16) | a;
}

/* the zlib stream of raw into data (stream_size bytes at most); its length */
static unsigned long write_stream(unsigned char const *raw_bytes, unsigned long raw, unsigned char *data,
	unsigned long capacity)
{
	unsigned long *token = malloc(sizeof(unsigned long) * (raw ? raw : 1)), count, length = 0, adler = adler32_of(raw_bytes, raw);
	struct bits bits;

	if (token)
	{
		count = find_tokens(raw_bytes, raw, token);
		memset(&bits, 0, sizeof(bits));
		bits.out = data + 2;
		/* (smaller than the stored blocks' size, or they are written instead) */
		bits.size = raw + 5 * ((raw + 65534) / 65535) - 1;
		if (count && write_dynamic(&bits, token, count))
		{
			data[0] = 0x78;
			data[1] = 0x9c;
			length = 2 + bits.at;
			put32(data + length, adler);
			length += 4;
		}
		free(token);
	}
	if (!length)
	{
		unsigned long left = raw, done = 0;

		data[0] = 0x78;
		data[1] = 0x01;
		length = 2;
		do
		{
			unsigned long block = left > 65535 ? 65535 : left;

			data[length++] = left <= 65535 ? 1 : 0;
			data[length++] = (unsigned char)block;
			data[length++] = (unsigned char)(block >> 8);
			data[length++] = (unsigned char)~block;
			data[length++] = (unsigned char)(~block >> 8);
			memcpy(data + length, raw_bytes + done, block);
			length += block;
			done += block;
			left -= block;
		}
		while (left);
		put32(data + length, adler);
		length += 4;
	}
	(void)capacity;
	return length;
}

int ae_map_auto_picture_png(unsigned char const *rgba, int width, int height, unsigned char *out, unsigned int size,
	unsigned int *written)
{
	static unsigned char const signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	unsigned int total = ae_map_auto_picture_png_size(width, height);
	unsigned long raw = raw_size(width, height), length;
	unsigned char *at, *filtered, *stream;

	if (!rgba || !out || !written || !total || size < total)
		return 0;
	filtered = malloc(raw);
	stream = malloc(stream_size(width, height));
	if (!filtered || !stream)
	{
		free(filtered);
		free(stream);
		return 0;
	}
	filter_rows(rgba, width, height, filtered);
	length = write_stream(filtered, raw, stream, stream_size(width, height));
	memcpy(out, signature, 8);
	at = out + 8;
	put32(at + 8, (unsigned long)width);
	put32(at + 12, (unsigned long)height);
	at[16] = 8;  /* bits a channel */
	at[17] = 6;  /* RGBA */
	at[18] = at[19] = at[20] = 0;
	at += finish_chunk(at, "IHDR", 13);
	memcpy(at + 8, stream, length);
	at += finish_chunk(at, "IDAT", length);
	at += finish_chunk(at, "IEND", 0);
	free(filtered);
	free(stream);
	if ((unsigned int)(at - out) > total)
		return 0;
	*written = (unsigned int)(at - out);
	return 1;
}
