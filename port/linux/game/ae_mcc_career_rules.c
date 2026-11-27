/* The MCC career insignia's rules (see the header). Pure code. */
#include <string.h>

#include "ae_mcc_career_rules.h"

enum { NAMES_MOST = 1024, CHAIN_MOST = 64, STRIDE_MOST = 512 };

static unsigned int read32(unsigned char const *at)
{
	return (unsigned int)at[0] | ((unsigned int)at[1] << 8) | ((unsigned int)at[2] << 16) | ((unsigned int)at[3] << 24);
}

/* the package's name table: the name of each index, as a pointer into the package and its length; the count, 0 when it does not fit */
static int names_read(unsigned char const *ua, size_t size, unsigned char const **name, int *length)
{
	size_t at = 8;
	long custom, count, offset, each;

	/* (tag, legacy version -7 or lower, then the UE3, UE4 and licensee versions and the custom versions) */
	if (size < 40 || read32(ua) != 0x9E2A83C1u || (int)read32(ua + 4) > -7)
		return 0;
	at += 12;
	custom = (long)(int)read32(ua + at);
	if (custom < 0 || custom > 256)
		return 0;
	at += 4 + (size_t)custom * 20;
	/* (the header's total size, the folder name, the package flags, then the names' count and place) */
	at += 4;
	if (at + 4 > size)
		return 0;
	{
		long folder = (long)(int)read32(ua + at);

		if (folder < 0 || folder > 1024 || at + 4 + (size_t)folder + 12 > size)
			return 0;
		at += 4 + (size_t)folder + 4;
	}
	count = (long)(int)read32(ua + at);
	offset = (long)(int)read32(ua + at + 4);
	if (count < 1 || count > NAMES_MOST || offset < 0 || (size_t)offset >= size)
		return 0;
	at = (size_t)offset;
	for (each = 0; each < count; each++)
	{
		long n;

		if (at + 4 > size)
			return 0;
		n = (long)(int)read32(ua + at);
		at += 4;
		if (n < 1 || n > 512 || at + (size_t)n + 4 > size)
			return 0;
		name[each] = ua + at;
		length[each] = (int)n - 1;
		at += (size_t)n + 4;
	}
	return (int)count;
}

/* whether a name is an object path of a picture ("/Game/.../T_Im_X.T_Im_X": the folder holds "/T_Im_" and the name repeats after a
   dot); 1 with the pak path of its export file in path */
static int picture_path(unsigned char const *name, int length, char *path)
{
	static char const game[] = "/Game/";
	static char const root[] = "MCC/Content/";
	char text[AE_MCC_CAREER_PATH];
	char *dot, *slash;
	size_t base;

	if (length < 8 || length >= (int)sizeof(text) - 8)
		return 0;
	memcpy(text, name, (size_t)length);
	text[length] = 0;
	if (strncmp(text, game, sizeof(game) - 1) != 0)
		return 0;
	slash = strrchr(text, '/');
	dot = strrchr(text, '.');
	if (!slash || !dot || dot < slash || strncmp(slash + 1, "T_Im_", 5) != 0)
		return 0;
	/* (the same name before and after the dot) */
	if ((size_t)(dot - (slash + 1)) != strlen(dot + 1) || memcmp(slash + 1, dot + 1, strlen(dot + 1)) != 0)
		return 0;
	*dot = 0;
	base = strlen(text + sizeof(game) - 1);
	if (sizeof(root) - 1 + base + 6 >= AE_MCC_CAREER_PATH)
		return 0;
	memcpy(path, root, sizeof(root) - 1);
	memcpy(path + sizeof(root) - 1, text + sizeof(game) - 1, base);
	memcpy(path + sizeof(root) - 1 + base, ".uexp", 6);
	return 1;
}

/* the places of the export data where a picture's object path is referenced (a name index and a zero number): -1 where none */
struct chain
{
	int length, stride, start;
	int name[CHAIN_MOST];
};

static void chain_best(short const *at, size_t size, struct chain *best, int const *excluded, int excluded_count)
{
	size_t p;
	int stride;

	memset(best, 0, sizeof(*best));
	for (p = 0; p < size; p++)
	{
		if (at[p] < 0)
			continue;
		for (stride = 16; stride <= STRIDE_MOST; stride++)
		{
			struct chain now;
			size_t q = p;
			int each, clash = 0;

			/* (a chain starts where the place a stride before is empty) */
			if (p >= (size_t)stride && at[p - (size_t)stride] >= 0)
				continue;
			now.length = 0;
			now.stride = stride;
			now.start = (int)p;
			while (q < size && at[q] >= 0 && now.length < CHAIN_MOST)
			{
				for (each = 0; each < now.length; each++)
					if (now.name[each] == at[q])
						clash = 1;
				for (each = 0; each < excluded_count; each++)
					if (excluded[each] == at[q])
						clash = 1;
				if (clash)
					break;
				now.name[now.length++] = at[q];
				q += (size_t)stride;
			}
			if (now.length > best->length)
				*best = now;
		}
	}
}

int ae_mcc_career_order(unsigned char const *uasset, size_t uasset_size, unsigned char const *uexp, size_t uexp_size,
	char ranks[AE_MCC_CAREER_RANKS][AE_MCC_CAREER_PATH], char tours[AE_MCC_CAREER_TOURS][AE_MCC_CAREER_PATH])
{
	unsigned char const *name[NAMES_MOST];
	int length[NAMES_MOST];
	static short at[AE_MCC_CAREER_MOST_ASSET];
	static char is_picture[NAMES_MOST];
	char path[AE_MCC_CAREER_PATH];
	struct chain levels, prestige;
	int count, each;
	size_t p;

	if (!uasset || !uexp || uexp_size < 16 || uexp_size > AE_MCC_CAREER_MOST_ASSET)
		return 0;
	count = names_read(uasset, uasset_size, name, length);
	if (count < 1)
		return 0;
	for (each = 0; each < count; each++)
		is_picture[each] = (char)picture_path(name[each], length[each], path);
	for (p = 0; p < uexp_size; p++)
		at[p] = -1;
	for (p = 0; p + 8 <= uexp_size; p++)
	{
		unsigned int index = read32(uexp + p);

		if (index < (unsigned int)count && is_picture[index] && read32(uexp + p + 4) == 0)
			at[p] = (short)index;
	}
	chain_best(at, uexp_size, &levels, NULL, 0);
	if (levels.length != AE_MCC_CAREER_RANKS)
		return 0;
	chain_best(at, uexp_size, &prestige, levels.name, levels.length);
	if (prestige.length != AE_MCC_CAREER_TOURS)
		return 0;
	for (each = 0; each < AE_MCC_CAREER_RANKS; each++)
		if (!picture_path(name[levels.name[each]], length[levels.name[each]], ranks[each]))
			return 0;
	for (each = 0; each < AE_MCC_CAREER_TOURS; each++)
		if (!picture_path(name[prestige.name[each]], length[prestige.name[each]], tours[each]))
			return 0;
	return 1;
}

int ae_mcc_career_texture(unsigned char const *uexp, size_t uexp_size, unsigned int *side, size_t *pixels)
{
	static char const format[] = "PF_B8G8R8A8";
	size_t at, found = 0, limit = uexp_size < 1024 ? uexp_size : 1024, start;
	unsigned long long elements, size, root;
	unsigned int s;

	if (!uexp || uexp_size < 64)
		return 0;
	for (at = 4; at + sizeof(format) <= limit; at++)
		if (!memcmp(uexp + at, format, sizeof(format)) && read32(uexp + at - 4) == sizeof(format))
		{
			found = at + sizeof(format);
			break;
		}
	if (!found)
		return 0;
	/* (the picture's first mip and count of mips, a mip's flags, its element count and its size, and its offset 8: the pixels follow) */
	if (found + 12 + 20 > uexp_size || read32(uexp + found + 4) != 1)
		return 0;
	elements = read32(uexp + found + 12 + 4);
	size = read32(uexp + found + 12 + 8);
	start = found + 12 + 20;
	if (elements != size || size == 0 || (size & 3u) || size > (unsigned long long)AE_MCC_CAREER_SIDE_MOST * AE_MCC_CAREER_SIDE_MOST * 4 ||
		start + size > uexp_size)
		return 0;
	root = size / 4;
	for (s = 4; s <= AE_MCC_CAREER_SIDE_MOST; s++)
		if ((unsigned long long)s * s == root)
		{
			*side = s;
			*pixels = start;
			return 1;
		}
	return 0;
}

int ae_mcc_career_scale(unsigned char const *bgra, unsigned int side_in, unsigned char *rgba, unsigned int side_out)
{
	unsigned int block, x, y, bx, by;

	if (!bgra || !rgba || side_out < 1 || side_in < side_out || side_in % side_out != 0 || side_in > AE_MCC_CAREER_SIDE_MOST)
		return 0;
	block = side_in / side_out;
	for (y = 0; y < side_out; y++)
		for (x = 0; x < side_out; x++)
		{
			unsigned long red = 0, green = 0, blue = 0, alpha = 0;

			for (by = 0; by < block; by++)
				for (bx = 0; bx < block; bx++)
				{
					unsigned char const *in = bgra + ((size_t)(y * block + by) * side_in + (x * block + bx)) * 4;

					blue += (unsigned long)in[0] * in[3];
					green += (unsigned long)in[1] * in[3];
					red += (unsigned long)in[2] * in[3];
					alpha += in[3];
				}
			{
				unsigned char *out = rgba + ((size_t)y * side_out + x) * 4;

				out[0] = alpha ? (unsigned char)(red / alpha) : 0;
				out[1] = alpha ? (unsigned char)(green / alpha) : 0;
				out[2] = alpha ? (unsigned char)(blue / alpha) : 0;
				out[3] = (unsigned char)(alpha / (block * block));
			}
		}
	return 1;
}
